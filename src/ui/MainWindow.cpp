#include "ui/MainWindow.h"

#include "core/Logger.h"
#include "core/Utf.h"
#include "core/Version.h"
#include "model/PageClassification.h"
#include "ocr/OcrEngine.h"
#include "pdf/PdfSearch.h"
#include "ui/InspectorPanel.h"
#include "ui/PdfCanvas.h"
#include "ui/SearchPanel.h"
#include "ui/ThumbnailPane.h"

#include <QAction>
#include <QCloseEvent>
#include <QColor>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QSpinBox>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>

#include <algorithm>

namespace pdfforge::ui {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    runtime_ = pdfforge::PdfiumRuntime::acquire();
    setWindowTitle(QString::fromUtf8(pdfforge::kProductName.data(),
                                     static_cast<int>(pdfforge::kProductName.size())));
    resize(1440, 900);
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
    auto* pagesDock = new QDockWidget(tr("Pages"), this);
    pagesDock->setObjectName(QStringLiteral("pagesDock"));
    pagesDock->setWidget(thumbs_);
    addDockWidget(Qt::LeftDockWidgetArea, pagesDock);

    inspector_ = new InspectorPanel(this);
    inspector_->setOcrAvailable(pdfforge::OcrEngine::available());
    auto* inspectDock = new QDockWidget(tr("Inspector"), this);
    inspectDock->setObjectName(QStringLiteral("inspectorDock"));
    inspectDock->setWidget(inspector_);
    addDockWidget(Qt::RightDockWidgetArea, inspectDock);

    auto* toolbar = addToolBar(tr("Document"));
    toolbar->setMovable(false);
    toolbar->addAction(tr("Open"), this, &MainWindow::openFile);
    toolbar->addAction(tr("Save"), this, &MainWindow::saveDocument);
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
    toolbar->addAction(tr("Rotate page"), this, &MainWindow::rotatePageClockwise);
    addTextAction_ = toolbar->addAction(tr("Add text"));
    addTextAction_->setCheckable(true);
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
    connect(canvas_, &PdfCanvas::spanSelected, this, [this](const pdfforge::TextSpan& span) {
        inspector_->setSpan(span);
    });
    connect(canvas_, &PdfCanvas::selectionCleared, inspector_, &InspectorPanel::clearSpan);
    connect(canvas_, &PdfCanvas::spanEditCommitted, this, &MainWindow::commitInlineEdit);
    connect(canvas_, &PdfCanvas::emptyPageClicked, this, &MainWindow::addTextAt);
    connect(addTextAction_, &QAction::toggled, this, &MainWindow::toggleAddText);
    connect(inspector_, &InspectorPanel::applyRequested, this, &MainWindow::applySpanEdits);
    connect(inspector_, &InspectorPanel::deleteRequested, this, &MainWindow::deleteSelectedSpan);
    connect(inspector_, &InspectorPanel::ocrRequested, this, &MainWindow::runOcr);
}

void MainWindow::buildMenus() {
    auto* file = menuBar()->addMenu(tr("&File"));
    file->addAction(tr("&Open…"), QKeySequence::Open, this, &MainWindow::openFile);
    file->addAction(tr("&Save"), QKeySequence::Save, this, &MainWindow::saveDocument);
    file->addAction(tr("Save &As…"), QKeySequence::SaveAs, this, &MainWindow::saveAs);
    file->addAction(tr("Save &copy…"), this, &MainWindow::saveCopy);
    file->addSeparator();
    file->addAction(tr("&Insert PDF…"), this, &MainWindow::insertPdf);
    file->addSeparator();
    file->addAction(tr("E&xit"), QKeySequence::Quit, this, &QWidget::close);

    auto* edit = menuBar()->addMenu(tr("&Edit"));
    undoAction_ = edit->addAction(tr("&Undo"), QKeySequence::Undo, this, &MainWindow::undoEdit);
    edit->addSeparator();
    edit->addAction(tr("&Edit selected text"), QKeySequence(Qt::Key_F2), canvas_,
                    &PdfCanvas::beginInlineEdit);
    edit->addAction(tr("&Delete text object"), QKeySequence::Delete, this,
                    &MainWindow::deleteSelectedSpan);
    edit->addSeparator();
    edit->addAction(tr("Find"), QKeySequence::Find, search_, &SearchPanel::focusQuery);

    auto* page = menuBar()->addMenu(tr("&Page"));
    page->addAction(tr("Rotate clockwise"), this, &MainWindow::rotatePageClockwise);
    page->addAction(tr("Rotate counter-clockwise"), this, &MainWindow::rotatePageCounterClockwise);
    page->addSeparator();
    page->addAction(tr("Insert blank page"), this, &MainWindow::insertBlankPage);
    page->addAction(tr("Delete page"), this, &MainWindow::deleteCurrentPage);

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

    auto* help = menuBar()->addMenu(tr("&Help"));
    help->addAction(tr("About PDFForge"), this, [this]() {
        QMessageBox::about(this, tr("About PDFForge"),
                           tr("PDFForge %1\nIndependent professional PDF editor.\n"
                              "Double-click text to edit it. Changes rewrite PDF objects.\n"
                              "Local processing. Telemetry off. JavaScript disabled.")
                               .arg(QString::fromUtf8(pdfforge::kVersionString.data(),
                                                      static_cast<int>(pdfforge::kVersionString.size()))));
    });
}

std::filesystem::path MainWindow::toFsPath(const QString& path) const {
#ifdef Q_OS_WIN
    return std::filesystem::path(path.toStdWString());
#else
    return std::filesystem::path(path.toStdString());
#endif
}

void MainWindow::updateTitle() {
    if (!document_) {
        setWindowTitle(QStringLiteral("PDFForge"));
        return;
    }
    QString name = QFileInfo(QString::fromStdString(pdfforge::narrowPath(document_->path()))).fileName();
    if (name.isEmpty()) {
        name = tr("Untitled");
    }
    setWindowTitle(QStringLiteral("PDFForge — %1%2")
                       .arg(name, document_->dirty() ? QStringLiteral("*") : QString()));
    if (undoAction_) {
        undoAction_->setEnabled(document_->canUndo());
    }
}

bool MainWindow::confirmDiscard() {
    if (!document_ || !document_->dirty()) {
        return true;
    }
    const auto r = QMessageBox::question(
        this, tr("PDFForge"), tr("This document has unsaved edits. Discard them?"),
        QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel);
    return r == QMessageBox::Discard;
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (confirmDiscard()) {
        event->accept();
    } else {
        event->ignore();
    }
}

void MainWindow::openFile() {
    if (!confirmDiscard()) {
        return;
    }
    const QString path = QFileDialog::getOpenFileName(this, tr("Open PDF"), {}, tr("PDF (*.pdf)"));
    if (!path.isEmpty()) {
        openPath(path);
    }
}

void MainWindow::openPath(const QString& path) {
    try {
        pdfforge::OpenOptions options;
        document_ = pdfforge::PdfDocument::open(runtime_, toFsPath(path), options);
        canvas_->setAddTextMode(false);
        if (addTextAction_) {
            addTextAction_->setChecked(false);
        }
        canvas_->setDocument(document_.get());
        thumbs_->setDocument(document_.get());
        pageSpin_->blockSignals(true);
        pageSpin_->setMaximum(std::max(1, document_->pageCount()));
        pageSpin_->setValue(1);
        pageSpin_->blockSignals(false);
        pageTotal_->setText(tr("/ %1").arg(document_->pageCount()));
        refreshPageUi();
        updateTitle();
        pdfforge::Logger::instance().info("ui", "opened document");
    } catch (const pdfforge::Error& ex) {
        document_.reset();
        canvas_->setDocument(nullptr);
        thumbs_->setDocument(nullptr);
        showError(ex);
    }
}

void MainWindow::saveDocument() {
    if (!document_) {
        return;
    }
    try {
        document_->save(document_->path());
        statusBar()->showMessage(tr("Saved"), 4000);
        updateTitle();
    } catch (const pdfforge::Error& ex) {
        showError(ex);
    }
}

void MainWindow::saveAs() {
    if (!document_) {
        return;
    }
    const QString path =
        QFileDialog::getSaveFileName(this, tr("Save as"), {}, tr("PDF (*.pdf)"));
    if (path.isEmpty()) {
        return;
    }
    try {
        document_->save(toFsPath(path));
        statusBar()->showMessage(tr("Saved"), 4000);
        updateTitle();
    } catch (const pdfforge::Error& ex) {
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
        document_->writeCopy(toFsPath(path));
        statusBar()->showMessage(tr("Copy saved"), 4000);
    } catch (const pdfforge::Error& ex) {
        showError(ex);
    }
}

void MainWindow::undoEdit() {
    if (!document_ || !document_->canUndo()) {
        return;
    }
    try {
        document_->undo();
        refreshAfterMutation(true);
        statusBar()->showMessage(tr("Undo"), 2000);
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

void MainWindow::rotatePageClockwise() {
    if (!document_) {
        return;
    }
    try {
        const int page = canvas_->pageIndex();
        document_->setPageRotation(page, document_->pageRotation(page) + 1);
        canvas_->setRotation(0);
        refreshAfterMutation(false);
        thumbs_->refreshPage(page);
    } catch (const pdfforge::Error& ex) {
        showError(ex);
    }
}

void MainWindow::rotatePageCounterClockwise() {
    if (!document_) {
        return;
    }
    try {
        const int page = canvas_->pageIndex();
        document_->setPageRotation(page, document_->pageRotation(page) - 1);
        canvas_->setRotation(0);
        refreshAfterMutation(false);
        thumbs_->refreshPage(page);
    } catch (const pdfforge::Error& ex) {
        showError(ex);
    }
}

void MainWindow::deleteCurrentPage() {
    if (!document_) {
        return;
    }
    if (QMessageBox::question(this, tr("Delete page"),
                              tr("Delete page %1 from the PDF?").arg(canvas_->pageIndex() + 1)) !=
        QMessageBox::Yes) {
        return;
    }
    try {
        const int page = canvas_->pageIndex();
        document_->deletePage(page);
        refreshAfterMutation(true);
    } catch (const pdfforge::Error& ex) {
        showError(ex);
    }
}

void MainWindow::insertBlankPage() {
    if (!document_) {
        return;
    }
    try {
        const int at = canvas_->pageIndex() + 1;
        document_->insertBlankPage(at, document_->pageSize(canvas_->pageIndex()));
        refreshAfterMutation(true);
        pageSpin_->setValue(at + 1);
    } catch (const pdfforge::Error& ex) {
        showError(ex);
    }
}

void MainWindow::insertPdf() {
    if (!document_) {
        return;
    }
    const QString path = QFileDialog::getOpenFileName(this, tr("Insert PDF"), {}, tr("PDF (*.pdf)"));
    if (path.isEmpty()) {
        return;
    }
    try {
        document_->importPages(toFsPath(path), canvas_->pageIndex() + 1);
        refreshAfterMutation(true);
        statusBar()->showMessage(tr("Pages inserted"), 4000);
    } catch (const pdfforge::Error& ex) {
        showError(ex);
    }
}

void MainWindow::toggleAddText(bool on) {
    canvas_->setAddTextMode(on);
    if (on) {
        statusBar()->showMessage(tr("Click the page to place a new text object"), 4000);
    }
}

void MainWindow::addTextAt(const pdfforge::PointF& pagePoint) {
    if (!document_) {
        return;
    }
    bool ok = false;
    const QString text = QInputDialog::getText(this, tr("Add text"), tr("Text:"), QLineEdit::Normal,
                                               tr("Text"), &ok);
    if (!ok || text.trimmed().isEmpty()) {
        return;
    }
    try {
        document_->addText(canvas_->pageIndex(), pagePoint, text.toStdString(), 14.0f,
                           pdfforge::Color::rgb(0, 0, 0));
        if (addTextAction_) {
            addTextAction_->setChecked(false);
        }
        canvas_->setAddTextMode(false);
        refreshAfterMutation(false);
        thumbs_->refreshPage(canvas_->pageIndex());
    } catch (const pdfforge::Error& ex) {
        showError(ex);
    }
}

void MainWindow::applySpanEdits(const QString& text, float fontSize, const QColor& color) {
    auto span = canvas_->selectedSpan();
    if (!document_ || !span) {
        return;
    }
    try {
        const pdfforge::Color next =
            pdfforge::Color::fromBytes(color.red(), color.green(), color.blue(), color.alpha());
        document_->editSpan(*span, text.toStdString(), fontSize, next);
        refreshAfterMutation(false);
        thumbs_->refreshPage(canvas_->pageIndex());
    } catch (const pdfforge::Error& ex) {
        showError(ex);
    }
}

void MainWindow::deleteSelectedSpan() {
    auto span = canvas_->selectedSpan();
    if (!document_ || !span) {
        return;
    }
    try {
        document_->deleteSpan(*span);
        canvas_->clearSelection();
        refreshAfterMutation(false);
        thumbs_->refreshPage(canvas_->pageIndex());
    } catch (const pdfforge::Error& ex) {
        showError(ex);
    }
}

void MainWindow::commitInlineEdit(const pdfforge::TextSpan& span, const QString& text) {
    if (!document_) {
        return;
    }
    if (text.toStdString() == span.text) {
        return;
    }
    try {
        document_->replaceSpanText(span, text.toStdString());
        refreshAfterMutation(false);
        thumbs_->refreshPage(canvas_->pageIndex());
        statusBar()->showMessage(tr("Text updated — Save to write the file"), 4000);
    } catch (const pdfforge::Error& ex) {
        showError(ex);
    }
}

void MainWindow::runOcr() {
    if (!document_) {
        return;
    }
    if (!pdfforge::OcrEngine::available()) {
        QMessageBox::information(this, tr("OCR"),
                                 tr("OCR is not available in this build."));
        return;
    }
    try {
        pdfforge::RenderRequest req;
        req.pageIndex = canvas_->pageIndex();
        req.dpi = 200.0f;
        const auto bitmap = document_->render(req);
        pdfforge::OcrEngine engine;
        const auto result = engine.recognize(bitmap);
        document_->addInvisibleOcrLayer(canvas_->pageIndex(), result, req.dpi);
        refreshAfterMutation(false);
        statusBar()->showMessage(tr("OCR text layer added (%1 words)").arg(result.words.size()),
                                 5000);
    } catch (const pdfforge::Error& ex) {
        showError(ex);
    }
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

void MainWindow::refreshAfterMutation(bool rebuildThumbs) {
    if (!document_) {
        return;
    }
    pageSpin_->blockSignals(true);
    pageSpin_->setMaximum(std::max(1, document_->pageCount()));
    if (pageSpin_->value() > document_->pageCount()) {
        pageSpin_->setValue(document_->pageCount());
    }
    pageSpin_->blockSignals(false);
    pageTotal_->setText(tr("/ %1").arg(document_->pageCount()));
    if (rebuildThumbs) {
        const int page = std::clamp(pageSpin_->value() - 1, 0, document_->pageCount() - 1);
        thumbs_->setDocument(document_.get());
        thumbs_->setCurrentPage(page);
        canvas_->setDocument(document_.get());
        canvas_->setPage(page);
        if (page == 0) {
            canvas_->reload();
        }
    } else {
        canvas_->reload();
    }
    refreshPageUi();
    updateTitle();
}

void MainWindow::refreshPageUi() {
    if (!document_) {
        classLabel_->clear();
        inspector_->clearSpan();
        return;
    }
    try {
        const auto cls = document_->classifyPage(canvas_->pageIndex());
        QString ocr =
            (cls.ocr == pdfforge::OcrNeed::Required) ? tr("OCR recommended") : tr("text layer");
        classLabel_->setText(QStringLiteral("%1 · %2")
                                 .arg(QString::fromUtf8(pdfforge::pageContentKindName(cls.kind)))
                                 .arg(ocr));
        inspector_->setClassification(cls);
    } catch (const pdfforge::Error& ex) {
        classLabel_->setText(QString::fromStdString(ex.userMessage()));
    }
}

void MainWindow::showError(const pdfforge::Error& error) {
    pdfforge::Logger::instance().error("ui", error.technical());
    QMessageBox::warning(this, tr("PDFForge"), QString::fromStdString(error.userMessage()));
}

}  // namespace pdfforge::ui
