#include "ui/MainWindow.h"

#include "core/Logger.h"
#include "core/Version.h"
#include "model/PageClassification.h"
#include "pdf/PdfSearch.h"
#include "ui/PdfCanvas.h"
#include "ui/SearchPanel.h"
#include "ui/ThumbnailPane.h"

#include <QAction>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeySequence>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QSpinBox>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>

#include <algorithm>
#include <filesystem>

namespace pdfforge::ui {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    runtime_ = pdfforge::PdfiumRuntime::acquire();
    setWindowTitle(QString::fromUtf8(pdfforge::kProductName.data(),
                                     static_cast<int>(pdfforge::kProductName.size())));
    resize(1280, 840);
    buildUi();
    buildMenus();
    statusBar()->showMessage(
        QString::fromUtf8(pdfforge::kProductTagline.data(),
                          static_cast<int>(pdfforge::kProductTagline.size())));
}

void MainWindow::buildUi() {
    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    search_ = new SearchPanel(central);
    canvas_ = new PdfCanvas(central);
    layout->addWidget(search_);
    layout->addWidget(canvas_, 1);
    setCentralWidget(central);

    thumbs_ = new ThumbnailPane(this);
    auto* dock = new QDockWidget(tr("Pages"), this);
    dock->setObjectName(QStringLiteral("pagesDock"));
    dock->setWidget(thumbs_);
    addDockWidget(Qt::LeftDockWidgetArea, dock);

    auto* toolbar = addToolBar(tr("Document"));
    toolbar->setMovable(false);
    toolbar->addAction(tr("Open"), this, &MainWindow::openFile);
    toolbar->addAction(tr("Save copy"), this, &MainWindow::saveCopy);
    toolbar->addSeparator();
    pageSpin_ = new QSpinBox(toolbar);
    pageSpin_->setMinimum(1);
    pageSpin_->setMaximum(1);
    pageTotal_ = new QLabel(tr("/ 0"), toolbar);
    toolbar->addWidget(pageSpin_);
    toolbar->addWidget(pageTotal_);
    toolbar->addSeparator();
    toolbar->addAction(tr("Zoom +"), canvas_, &PdfCanvas::zoomIn);
    toolbar->addAction(tr("Zoom −"), canvas_, &PdfCanvas::zoomOut);
    toolbar->addAction(tr("100%"), canvas_, &PdfCanvas::resetZoom);
    toolbar->addSeparator();
    toolbar->addAction(tr("Rotate CW"), this, &MainWindow::rotateClockwise);
    toolbar->addAction(tr("Rotate CCW"), this, &MainWindow::rotateCounterClockwise);
    classLabel_ = new QLabel(toolbar);
    toolbar->addSeparator();
    toolbar->addWidget(classLabel_);

    connect(pageSpin_, qOverload<int>(&QSpinBox::valueChanged), this, &MainWindow::goToPage);
    connect(thumbs_, &ThumbnailPane::pageActivated, this, [this](int page) {
        pageSpin_->setValue(page + 1);
    });
    connect(search_, &SearchPanel::querySubmitted, this, &MainWindow::runSearch);
    connect(search_, &SearchPanel::activeHitChanged, this, &MainWindow::jumpToHit);
    connect(canvas_, &PdfCanvas::statusMessage, this, [this](const QString& m) {
        statusBar()->showMessage(m, 4000);
    });
    connect(canvas_, &PdfCanvas::hoverSpanChanged, this, [this](const QString& t) {
        if (!t.isEmpty()) {
            statusBar()->showMessage(t, 2000);
        }
    });
}

void MainWindow::buildMenus() {
    auto* file = menuBar()->addMenu(tr("&File"));
    file->addAction(tr("&Open…"), QKeySequence::Open, this, &MainWindow::openFile);
    file->addAction(tr("Save &copy…"), this, &MainWindow::saveCopy);
    file->addSeparator();
    file->addAction(tr("E&xit"), QKeySequence::Quit, this, &QWidget::close);

    auto* view = menuBar()->addMenu(tr("&View"));
    view->addAction(tr("Zoom in"), QKeySequence::ZoomIn, canvas_, &PdfCanvas::zoomIn);
    view->addAction(tr("Zoom out"), QKeySequence::ZoomOut, canvas_, &PdfCanvas::zoomOut);
    view->addAction(tr("Reset zoom"), QKeySequence(Qt::CTRL | Qt::Key_0), canvas_,
                    &PdfCanvas::resetZoom);
    view->addSeparator();
    view->addAction(tr("Previous page"), QKeySequence::MoveToPreviousPage, this, [this]() {
        pageSpin_->setValue(pageSpin_->value() - 1);
    });
    view->addAction(tr("Next page"), QKeySequence::MoveToNextPage, this, [this]() {
        pageSpin_->setValue(pageSpin_->value() + 1);
    });

    auto* edit = menuBar()->addMenu(tr("&Edit"));
    edit->addAction(tr("Find"), QKeySequence::Find, search_, &SearchPanel::focusQuery);

    auto* help = menuBar()->addMenu(tr("&Help"));
    help->addAction(tr("About PDFForge"), this, [this]() {
        QMessageBox::about(this, tr("About PDFForge"),
                           tr("PDFForge %1\nIndependent professional PDF editor.\n"
                              "Local processing. Telemetry off. JavaScript disabled.")
                               .arg(QString::fromUtf8(pdfforge::kVersionString.data(),
                                                      static_cast<int>(pdfforge::kVersionString.size()))));
    });
}

void MainWindow::openFile() {
    const QString path = QFileDialog::getOpenFileName(this, tr("Open PDF"), {}, tr("PDF (*.pdf)"));
    if (!path.isEmpty()) {
        openPath(path);
    }
}

void MainWindow::openPath(const QString& path) {
    try {
        pdfforge::OpenOptions options;
        document_ = pdfforge::PdfDocument::open(runtime_, std::filesystem::path(path.toStdString()),
                                                options);
        canvas_->setDocument(document_.get());
        thumbs_->setDocument(document_.get());
        pageSpin_->blockSignals(true);
        pageSpin_->setMaximum(std::max(1, document_->pageCount()));
        pageSpin_->setValue(1);
        pageSpin_->blockSignals(false);
        pageTotal_->setText(tr("/ %1").arg(document_->pageCount()));
        setWindowTitle(QStringLiteral("PDFForge — %1").arg(QFileInfo(path).fileName()));
        refreshPageUi();
        pdfforge::Logger::instance().info("ui", "opened document");
    } catch (const pdfforge::Error& ex) {
        document_.reset();
        canvas_->setDocument(nullptr);
        thumbs_->setDocument(nullptr);
        showError(ex);
    }
}

void MainWindow::saveCopy() {
    if (!document_) {
        return;
    }
    const QString path =
        QFileDialog::getSaveFileName(this, tr("Save copy"), {}, tr("PDF (*.pdf)"));
    if (path.isEmpty()) {
        return;
    }
    try {
        document_->writeCopy(std::filesystem::path(path.toStdString()));
        statusBar()->showMessage(tr("Copy saved"), 4000);
    } catch (const pdfforge::Error& ex) {
        showError(ex);
    }
}

void MainWindow::goToPage(int pageOneBased) {
    if (!document_) {
        return;
    }
    const int index = std::clamp(pageOneBased, 1, document_->pageCount()) - 1;
    canvas_->setPage(index);
    thumbs_->setCurrentPage(index);
    refreshPageUi();
}

void MainWindow::rotateClockwise() {
    canvas_->setRotation(canvas_->rotation() + 1);
}
void MainWindow::rotateCounterClockwise() {
    canvas_->setRotation(canvas_->rotation() - 1);
}

void MainWindow::runSearch(const QString& query) {
    hits_.clear();
    if (!document_ || query.trimmed().isEmpty()) {
        search_->setHits({});
        canvas_->setSearchHits({}, -1);
        return;
    }
    pdfforge::SearchQuery q;
    q.needle = query.toStdString();
    q.caseInsensitive = true;
    for (int i = 0; i < document_->pageCount(); ++i) {
        const auto spans = document_->extractText(i);
        auto pageHits = pdfforge::searchSpans(spans, q);
        hits_.insert(hits_.end(), pageHits.begin(), pageHits.end());
    }
    search_->setHits(hits_);
    canvas_->setSearchHits(hits_, hits_.empty() ? -1 : 0);
}

void MainWindow::jumpToHit(int hitIndex) {
    if (hitIndex < 0 || hitIndex >= static_cast<int>(hits_.size())) {
        return;
    }
    const auto& hit = hits_[static_cast<std::size_t>(hitIndex)];
    pageSpin_->setValue(hit.pageIndex + 1);
    canvas_->setSearchHits(hits_, hitIndex);
}

void MainWindow::refreshPageUi() {
    if (!document_) {
        classLabel_->clear();
        return;
    }
    try {
        const auto cls = document_->classifyPage(canvas_->pageIndex());
        QString ocr = (cls.ocr == pdfforge::OcrNeed::Required) ? tr("OCR required") : tr("text layer");
        classLabel_->setText(QStringLiteral("%1 · %2")
                                 .arg(QString::fromUtf8(pdfforge::pageContentKindName(cls.kind)))
                                 .arg(ocr));
    } catch (const pdfforge::Error& ex) {
        classLabel_->setText(QString::fromStdString(ex.userMessage()));
    }
}

void MainWindow::showError(const pdfforge::Error& error) {
    pdfforge::Logger::instance().error("ui", error.technical());
    QMessageBox::warning(this, tr("PDFForge"), QString::fromStdString(error.userMessage()));
}

}  // namespace pdfforge::ui
