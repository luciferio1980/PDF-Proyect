#include "ui/MainWindow.h"

#include "core/Logger.h"
#include "core/Utf.h"
#include "core/Version.h"
#include "model/PageClassification.h"
#include "ocr/OcrEngine.h"
#include "pdf/PdfSearch.h"
#include "pdf/RegionRecognize.h"
#include "ui/HomeView.h"
#include "ui/InspectorPanel.h"
#include "ui/PdfCanvas.h"
#include "ui/SearchPanel.h"
#include "ui/SignatureLibrary.h"
#include "ui/SignaturePad.h"
#include "ui/ThumbnailPane.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QColor>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QRect>
#include <QRgb>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>

#include <algorithm>
#include <optional>

namespace pdfforge::ui {
namespace {

pdfforge::Bitmap qImageToBitmap(QImage image) {
    if (image.isNull()) {
        return {};
    }
    if (image.width() > 2000 || image.height() > 2000) {
        image = image.scaled(2000, 2000, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    image = image.convertToFormat(QImage::Format_ARGB32);
    pdfforge::Bitmap out;
    out.width = image.width();
    out.height = image.height();
    out.stride = image.bytesPerLine();
    const auto* bits = image.constBits();
    out.bgra.assign(bits, bits + static_cast<std::size_t>(out.stride) * static_cast<std::size_t>(out.height));
    return out;
}

QImage cropToInk(const QImage& image) {
    if (image.isNull()) {
        return {};
    }
    const QImage src = image.convertToFormat(QImage::Format_ARGB32);
    int minX = src.width();
    int minY = src.height();
    int maxX = -1;
    int maxY = -1;
    for (int y = 0; y < src.height(); ++y) {
        const auto* line = reinterpret_cast<const QRgb*>(src.constScanLine(y));
        for (int x = 0; x < src.width(); ++x) {
            if (qAlpha(line[x]) > 20) {
                minX = std::min(minX, x);
                minY = std::min(minY, y);
                maxX = std::max(maxX, x);
                maxY = std::max(maxY, y);
            }
        }
    }
    if (maxX < minX) {
        return {};
    }
    constexpr int kPad = 8;
    const int left = std::max(0, minX - kPad);
    const int top = std::max(0, minY - kPad);
    const int right = std::min(src.width() - 1, maxX + kPad);
    const int bottom = std::min(src.height() - 1, maxY + kPad);
    return src.copy(QRect(left, top, right - left + 1, bottom - top + 1));
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    runtime_ = pdfforge::PdfiumRuntime::acquire();
    setWindowTitle(QString::fromUtf8(pdfforge::kProductName.data(),
                                     static_cast<int>(pdfforge::kProductName.size())));
    resize(1440, 900);
    buildUi();
    buildMenus();
    setWorkspace(Workspace::Home);
    statusBar()->showMessage(
        QString::fromUtf8(pdfforge::kProductTagline.data(),
                          static_cast<int>(pdfforge::kProductTagline.size())));
}

void MainWindow::buildUi() {
    stack_ = new QStackedWidget(this);
    home_ = new HomeView(stack_);
    workPage_ = new QWidget(stack_);
    auto* workLayout = new QVBoxLayout(workPage_);
    workLayout->setContentsMargins(0, 0, 0, 0);
    workLayout->setSpacing(0);
    search_ = new SearchPanel(workPage_);
    canvas_ = new PdfCanvas(workPage_);
    workLayout->addWidget(search_);
    workLayout->addWidget(canvas_, 1);
    stack_->addWidget(home_);
    stack_->addWidget(workPage_);
    setCentralWidget(stack_);

    thumbs_ = new ThumbnailPane(this);
    pagesDock_ = new QDockWidget(tr("Pages"), this);
    pagesDock_->setObjectName(QStringLiteral("pagesDock"));
    pagesDock_->setWidget(thumbs_);
    addDockWidget(Qt::LeftDockWidgetArea, pagesDock_);

    inspector_ = new InspectorPanel(this);
    inspector_->setOcrAvailable(pdfforge::OcrEngine::available());
    inspectorDock_ = new QDockWidget(tr("Inspector"), this);
    inspectorDock_->setObjectName(QStringLiteral("inspectorDock"));
    inspectorDock_->setWidget(inspector_);
    addDockWidget(Qt::RightDockWidgetArea, inspectorDock_);

    library_ = new SignatureLibrary(this);
    firmasDock_ = new QDockWidget(tr("Firmas"), this);
    firmasDock_->setObjectName(QStringLiteral("firmasDock"));
    firmasDock_->setWidget(library_);
    addDockWidget(Qt::RightDockWidgetArea, firmasDock_);

    auto* nav = addToolBar(tr("Document"));
    nav->setObjectName(QStringLiteral("navBar"));
    nav->setMovable(false);
    nav->addAction(tr("Inicio"), this, &MainWindow::showHome);
    nav->addAction(tr("Open"), this, &MainWindow::openFile);
    nav->addAction(tr("Save"), this, &MainWindow::saveDocument);
    nav->addSeparator();
    pageSpin_ = new QSpinBox(nav);
    pageSpin_->setMinimum(1);
    pageSpin_->setMaximum(1);
    pageTotal_ = new QLabel(tr("/ 0"), nav);
    nav->addWidget(pageSpin_);
    nav->addWidget(pageTotal_);
    nav->addSeparator();
    nav->addAction(tr("Zoom +"), canvas_, &PdfCanvas::zoomIn);
    nav->addAction(tr("Zoom −"), canvas_, &PdfCanvas::zoomOut);
    nav->addAction(tr("100%"), canvas_, &PdfCanvas::resetZoom);
    navBar_ = nav;

    signBar_ = addToolBar(tr("Sign"));
    signBar_->setMovable(false);
    signBar_->addAction(tr("Subir imagen"), this, &MainWindow::loadSignatureImage);
    signBar_->addAction(tr("Dibujar firma"), this, &MainWindow::drawSignature);
    signBar_->addAction(tr("Guardar firma"), this, &MainWindow::saveCurrentSignature);
    signBar_->addSeparator();
    signBar_->addWidget(new QLabel(tr("Tamaño"), signBar_));
    stampWidthSpin_ = new QDoubleSpinBox(signBar_);
    stampWidthSpin_->setRange(24.0, 480.0);
    stampWidthSpin_->setValue(144.0);
    stampWidthSpin_->setSingleStep(4.0);
    stampWidthSpin_->setSuffix(QStringLiteral(" pt"));
    signBar_->addWidget(stampWidthSpin_);
    deleteStampAction_ = signBar_->addAction(tr("Borrar firma"), this, &MainWindow::deleteSelectedSignature);
    deleteStampAction_->setEnabled(false);

    editBar_ = addToolBar(tr("Edit tools"));
    editBar_->setMovable(false);
    editBar_->addAction(tr("Rotate page"), this, &MainWindow::rotatePageClockwise);
    addTextAction_ = editBar_->addAction(tr("Add text"));
    addTextAction_->setCheckable(true);
    classLabel_ = new QLabel(editBar_);
    editBar_->addSeparator();
    editBar_->addWidget(classLabel_);

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
    connect(canvas_, &PdfCanvas::selectionCleared, this, [this]() {
        region_ = {};
        inspector_->clearSpan();
    });
    connect(canvas_, &PdfCanvas::spanEditCommitted, this, &MainWindow::commitInlineEdit);
    connect(canvas_, &PdfCanvas::regionSelected, this, &MainWindow::onRegionSelected);
    connect(canvas_, &PdfCanvas::regionEditCommitted, this, &MainWindow::commitRegionEdit);
    connect(canvas_, &PdfCanvas::emptyPageClicked, this, &MainWindow::addTextAt);
    connect(canvas_, &PdfCanvas::stampPlaced, this, &MainWindow::placeSignature);
    connect(canvas_, &PdfCanvas::signatureSelected, this, &MainWindow::onSignatureSelected);
    connect(canvas_, &PdfCanvas::signatureSelectionCleared, this, &MainWindow::onSignatureCleared);
    connect(canvas_, &PdfCanvas::signatureResizeCommitted, this, &MainWindow::onSignatureResized);
    connect(canvas_, &PdfCanvas::signatureDeleteRequested, this, &MainWindow::deleteSelectedSignature);
    connect(library_, &SignatureLibrary::signatureChosen, this, &MainWindow::useSavedSignature);
    connect(addTextAction_, &QAction::toggled, this, &MainWindow::toggleAddText);
    connect(inspector_, &InspectorPanel::applyRequested, this, &MainWindow::applySpanEdits);
    connect(inspector_, &InspectorPanel::deleteRequested, this, &MainWindow::deleteSelectedSpan);
    connect(inspector_, &InspectorPanel::ocrRequested, this, &MainWindow::runOcr);
    connect(home_, &HomeView::editPdfRequested, this, &MainWindow::startEditPdf);
    connect(home_, &HomeView::signPdfRequested, this, &MainWindow::startSignPdf);
    connect(stampWidthSpin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this](double value) {
                if (ignoreStampWidth_) {
                    return;
                }
                if (auto selected = canvas_->selectedSignature()) {
                    const float width = static_cast<float>(value);
                    const float aspect =
                        selected->bounds.height / std::max(1.0f, selected->bounds.width);
                    const pdfforge::RectF next{
                        selected->bounds.x + (selected->bounds.width - width) * 0.5f,
                        selected->bounds.y + (selected->bounds.height - width * aspect) * 0.5f, width,
                        width * aspect};
                    onSignatureResized(*selected, next);
                    return;
                }
                applyStampPreview();
            });
}

void MainWindow::buildMenus() {
    auto* file = menuBar()->addMenu(tr("&File"));
    file->addAction(tr("&Inicio"), this, &MainWindow::showHome);
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
                              "Start from the home menu: Edit PDF or Sign PDF.\n"
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
    QString mode;
    if (workspace_ == Workspace::Edit) {
        mode = tr("Editar PDF");
    } else if (workspace_ == Workspace::Sign) {
        mode = tr("Firmar PDF");
    }
    if (!document_) {
        setWindowTitle(mode.isEmpty() ? QStringLiteral("PDFForge")
                                      : QStringLiteral("PDFForge — %1").arg(mode));
        return;
    }
    QString name = QFileInfo(QString::fromStdString(pdfforge::narrowPath(document_->path()))).fileName();
    if (name.isEmpty()) {
        name = tr("Untitled");
    }
    setWindowTitle(QStringLiteral("PDFForge — %1 — %2%3")
                       .arg(mode, name, document_->dirty() ? QStringLiteral("*") : QString()));
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
    if (workspace_ == Workspace::Home) {
        startEditPdf();
        return;
    }
    if (!confirmDiscard()) {
        return;
    }
    const QString path = QFileDialog::getOpenFileName(this, tr("Open PDF"), {}, tr("PDF (*.pdf)"));
    if (!path.isEmpty()) {
        openPath(path);
    }
}

void MainWindow::openPath(const QString& path) {
    if (workspace_ == Workspace::Sign) {
        openPathForSign(path);
    } else {
        openPathForEdit(path);
    }
}

void MainWindow::openPathForEdit(const QString& path) {
    try {
        pdfforge::OpenOptions options;
        document_ = pdfforge::PdfDocument::open(runtime_, toFsPath(path), options);
        canvas_->setAddTextMode(false);
        canvas_->setPlaceStampMode(false);
        if (addTextAction_) {
            addTextAction_->setChecked(false);
        }
        placedThisVisit_ = false;
        stampImage_ = {};
        canvas_->setDocument(document_.get());
        thumbs_->setDocument(document_.get());
        pageSpin_->blockSignals(true);
        pageSpin_->setMaximum(std::max(1, document_->pageCount()));
        pageSpin_->setValue(1);
        pageSpin_->blockSignals(false);
        pageTotal_->setText(tr("/ %1").arg(document_->pageCount()));
        setWorkspace(Workspace::Edit);
        refreshPageUi();
        updateTitle();
        pdfforge::Logger::instance().info("ui", "opened document for edit");
        statusBar()->showMessage(
            tr("Arrastra un recuadro sobre el texto para reconocerlo y editarlo"), 7000);
    } catch (const pdfforge::Error& ex) {
        document_.reset();
        canvas_->setDocument(nullptr);
        thumbs_->setDocument(nullptr);
        showError(ex);
    }
}

void MainWindow::openPathForSign(const QString& path) {
    try {
        pdfforge::OpenOptions options;
        document_ = pdfforge::PdfDocument::open(runtime_, toFsPath(path), options);
        canvas_->setAddTextMode(false);
        if (addTextAction_) {
            addTextAction_->setChecked(false);
        }
        placedThisVisit_ = false;
        stampImage_ = {};
        canvas_->setDocument(document_.get());
        thumbs_->setDocument(document_.get());
        pageSpin_->blockSignals(true);
        pageSpin_->setMaximum(std::max(1, document_->pageCount()));
        pageSpin_->setValue(1);
        pageSpin_->blockSignals(false);
        pageTotal_->setText(tr("/ %1").arg(document_->pageCount()));
        setWorkspace(Workspace::Sign);
        applyStampPreview();
        refreshPageUi();
        updateTitle();
        pdfforge::Logger::instance().info("ui", "opened document for sign");
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
    if (!document_) {
        return;
    }
    const pdfforge::Color next =
        pdfforge::Color::fromBytes(color.red(), color.green(), color.blue(), color.alpha());
    try {
        const pdfforge::RectF box = !region_.marquee.empty() ? region_.marquee : region_.bounds;
        const bool hasRegion = !region_.spans.empty() || !box.empty();
        if (hasRegion) {
            const int page =
                !region_.spans.empty() ? region_.spans.front().pageIndex : canvas_->pageIndex();
            document_->replaceRegion(page, box, region_.spans, text.toStdString(), fontSize, next);
        } else if (auto span = canvas_->selectedSpan()) {
            document_->editSpan(*span, text.toStdString(), fontSize, next);
        } else {
            return;
        }
        region_ = {};
        canvas_->clearRegionSelection();
        refreshAfterMutation(false);
        thumbs_->refreshPage(canvas_->pageIndex());
        statusBar()->showMessage(tr("Texto actualizado — Save para escribir el archivo"), 4000);
    } catch (const pdfforge::Error& ex) {
        showError(ex);
    }
}

void MainWindow::deleteSelectedSpan() {
    if (!document_) {
        return;
    }
    try {
        if (!region_.spans.empty() || !region_.marquee.empty() || !region_.bounds.empty()) {
            const pdfforge::RectF box = !region_.marquee.empty() ? region_.marquee : region_.bounds;
            const int page =
                !region_.spans.empty() ? region_.spans.front().pageIndex : canvas_->pageIndex();
            document_->replaceRegion(page, box, region_.spans, {}, region_.fontSize, region_.color);
        } else if (auto span = canvas_->selectedSpan()) {
            document_->deleteSpan(*span);
        } else {
            return;
        }
        region_ = {};
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

void MainWindow::onRegionSelected(const pdfforge::RectF& pageRect) {
    if (!document_) {
        return;
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    pdfforge::RegionRead read;
    try {
        read = pdfforge::recognizeRegion(*document_, canvas_->pageIndex(), pageRect);
    } catch (const pdfforge::Error& ex) {
        QApplication::restoreOverrideCursor();
        showError(ex);
        return;
    }
    QApplication::restoreOverrideCursor();
    if (read.empty()) {
        region_ = {};
        canvas_->clearRegionSelection();
        inspector_->clearSpan();
        statusBar()->showMessage(tr("No se reconoció texto en el recuadro"), 4000);
        return;
    }
    region_ = read;
    pdfforge::TextSpan view;
    if (!read.spans.empty()) {
        view = read.spans.front();
    }
    view.text = read.text;
    view.fontSize = read.fontSize;
    view.fontWeight = read.fontWeight;
    view.italic = read.italic;
    view.color = read.color;
    view.x = read.bounds.x;
    view.y = read.bounds.y;
    view.width = read.bounds.width;
    view.height = read.bounds.height;
    if (read.usedOcr) {
        view.fontName = read.matchedFamily.empty() ? "OCR" : ("OCR · " + read.matchedFamily);
    } else if (!read.matchedFamily.empty() && read.matchedFamily != read.fontName) {
        view.fontName = read.fontName + " → " + read.matchedFamily;
    } else {
        view.fontName = read.fontName.empty() ? read.matchedFamily : read.fontName;
    }
    inspector_->setSpan(view);
    canvas_->setRegionSelection(read.bounds, QString::fromStdString(read.text));
    canvas_->beginInlineEdit();
    QString source = read.usedOcr ? tr("OCR") : tr("capa de texto");
    statusBar()->showMessage(tr("Reconocido (%1): %2 · %3 pt")
                                 .arg(source, QString::fromStdString(view.fontName))
                                 .arg(read.fontSize, 0, 'f', 1),
                             6000);
}

void MainWindow::commitRegionEdit(const QString& text) {
    if (!document_) {
        return;
    }
    if (text.toStdString() == region_.text) {
        return;
    }
    const QColor color = QColor::fromRgbF(region_.color.toRgb().c0, region_.color.toRgb().c1,
                                          region_.color.toRgb().c2, region_.color.toRgb().alpha);
    applySpanEdits(text, region_.fontSize, color);
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

void MainWindow::setWorkspace(Workspace workspace) {
    workspace_ = workspace;
    const bool home = workspace == Workspace::Home;
    const bool edit = workspace == Workspace::Edit;
    const bool sign = workspace == Workspace::Sign;
    stack_->setCurrentWidget(home ? static_cast<QWidget*>(home_) : workPage_);
    if (navBar_) {
        navBar_->setVisible(!home);
    }
    if (editBar_) {
        editBar_->setVisible(edit);
    }
    if (signBar_) {
        signBar_->setVisible(sign);
    }
    if (pagesDock_) {
        pagesDock_->setVisible(!home);
    }
    if (inspectorDock_) {
        inspectorDock_->setVisible(edit);
    }
    if (firmasDock_) {
        firmasDock_->setVisible(sign);
    }
    if (search_) {
        search_->setVisible(edit);
    }
    if (canvas_) {
        canvas_->setSignWorkspace(sign);
        canvas_->setPlaceStampMode(sign && !placedThisVisit_ && !stampImage_.isNull());
        if (!sign) {
            canvas_->setPlaceStampMode(false);
            canvas_->clearSignatureSelection();
        }
        if (!edit && addTextAction_) {
            addTextAction_->setChecked(false);
            canvas_->setAddTextMode(false);
        }
    }
    updateTitle();
}

void MainWindow::showHome() {
    if (!confirmDiscard()) {
        return;
    }
    document_.reset();
    stampImage_ = {};
    placedThisVisit_ = false;
    if (canvas_) {
        canvas_->setDocument(nullptr);
        canvas_->setStampPreview({}, 144.0f);
        canvas_->setPlaceStampMode(false);
        canvas_->setSignWorkspace(false);
    }
    if (thumbs_) {
        thumbs_->setDocument(nullptr);
    }
    setWorkspace(Workspace::Home);
    statusBar()->showMessage(tr("Elige Editar PDF o Firmar PDF"), 4000);
}

void MainWindow::startEditPdf() {
    if (!confirmDiscard()) {
        return;
    }
    const QString path = QFileDialog::getOpenFileName(this, tr("Abrir PDF para editar"), {},
                                                      tr("PDF (*.pdf)"));
    if (!path.isEmpty()) {
        openPathForEdit(path);
    }
}

void MainWindow::startSignPdf() {
    if (!confirmDiscard()) {
        return;
    }
    const QString path = QFileDialog::getOpenFileName(this, tr("Abrir PDF para firmar"), {},
                                                      tr("PDF (*.pdf)"));
    if (!path.isEmpty()) {
        openPathForSign(path);
        statusBar()->showMessage(
            tr("Elige una firma guardada, súbela o dibújala. Coloca solo una; luego Inicio para firmar otra vez."),
            7000);
    }
}

void MainWindow::applyStampPreview() {
    if (!canvas_ || !stampWidthSpin_) {
        return;
    }
    canvas_->setStampPreview(stampImage_, static_cast<float>(stampWidthSpin_->value()));
    canvas_->setPlaceStampMode(workspace_ == Workspace::Sign && !placedThisVisit_ &&
                               !stampImage_.isNull());
}

void MainWindow::loadSignatureImage() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Imagen de firma"), {},
        tr("Images (*.png *.jpg *.jpeg *.webp *.bmp);;All files (*)"));
    if (path.isEmpty()) {
        return;
    }
    QImage image(path);
    if (image.isNull()) {
        QMessageBox::warning(this, tr("PDFForge"), tr("No se pudo abrir la imagen."));
        return;
    }
    QImage prepared = image.convertToFormat(QImage::Format_ARGB32);
    if (QImage cropped = cropToInk(prepared); !cropped.isNull()) {
        prepared = cropped;
    }
    useSavedSignature(prepared);
    if (library_ && !library_->saveSignature(stampImage_)) {
        if (library_->count() >= SignatureLibrary::kMaxSaved) {
            statusBar()->showMessage(
                tr("No se guardó: elimina una firma de la lista para guardar esta."), 6000);
        }
    }
}

void MainWindow::drawSignature() {
    SignaturePad pad(this);
    if (pad.exec() != QDialog::Accepted) {
        return;
    }
    const QImage drawn = cropToInk(pad.signatureImage());
    if (drawn.isNull()) {
        QMessageBox::information(this, tr("PDFForge"), tr("No hay trazo en la firma."));
        return;
    }
    useSavedSignature(drawn);
    if (library_ && !library_->saveSignature(stampImage_)) {
        if (library_->count() >= SignatureLibrary::kMaxSaved) {
            statusBar()->showMessage(
                tr("No se guardó: elimina una firma de la lista para guardar esta."), 6000);
        }
    }
}

void MainWindow::useSavedSignature(const QImage& image) {
    if (image.isNull()) {
        return;
    }
    stampImage_ = image.convertToFormat(QImage::Format_ARGB32);
    if (placedThisVisit_) {
        statusBar()->showMessage(
            tr("Para colocar otra firma, pulsa Inicio y elige Firmar PDF."), 6000);
        return;
    }
    applyStampPreview();
    statusBar()->showMessage(tr("Haz clic en la página para colocar la firma. Puedes cambiar el tamaño."), 5000);
}

void MainWindow::saveCurrentSignature() {
    if (stampImage_.isNull() || !library_) {
        return;
    }
    if (!library_->saveSignature(stampImage_)) {
        QMessageBox::information(
            this, tr("PDFForge"),
            tr("Ya hay %1 firmas guardadas. Elimina una para guardar otra.")
                .arg(SignatureLibrary::kMaxSaved));
        return;
    }
    statusBar()->showMessage(tr("Firma guardada"), 3000);
}

void MainWindow::placeSignature(const pdfforge::PointF& pagePoint) {
    if (!document_ || stampImage_.isNull() || !stampWidthSpin_ || placedThisVisit_) {
        return;
    }
    const pdfforge::Bitmap bitmap = qImageToBitmap(stampImage_);
    if (bitmap.empty()) {
        return;
    }
    const float width = static_cast<float>(stampWidthSpin_->value());
    const float height =
        width * static_cast<float>(bitmap.height) / static_cast<float>(std::max(1, bitmap.width));
    const pdfforge::RectF rect{pagePoint.x - width * 0.5f, pagePoint.y - height * 0.5f, width,
                               height};
    try {
        document_->addImage(canvas_->pageIndex(), rect, bitmap);
        placedThisVisit_ = true;
        canvas_->setPlaceStampMode(false);
        canvas_->setStampPreview({}, static_cast<float>(stampWidthSpin_->value()));
        refreshAfterMutation(false);
        thumbs_->refreshPage(canvas_->pageIndex());
        canvas_->selectSignatureAt(pagePoint);
        statusBar()->showMessage(
            tr("Firma colocada. Cambia el tamaño o Borrar. Un clic fuera quita la selección."), 7000);
    } catch (const pdfforge::Error& ex) {
        showError(ex);
    }
}

void MainWindow::onSignatureSelected(const pdfforge::ImageObject& image) {
    if (deleteStampAction_) {
        deleteStampAction_->setEnabled(true);
    }
    if (!stampWidthSpin_) {
        return;
    }
    ignoreStampWidth_ = true;
    stampWidthSpin_->setValue(static_cast<double>(std::clamp(image.bounds.width, 24.0f, 480.0f)));
    ignoreStampWidth_ = false;
}

void MainWindow::onSignatureCleared() {
    if (deleteStampAction_) {
        deleteStampAction_->setEnabled(false);
    }
}

void MainWindow::onSignatureResized(const pdfforge::ImageObject& image, const pdfforge::RectF& pageRect) {
    if (!document_) {
        return;
    }
    try {
        document_->setImageRect(image, pageRect);
        refreshAfterMutation(false);
        thumbs_->refreshPage(canvas_->pageIndex());
        canvas_->selectSignatureAt(pdfforge::PointF{pageRect.x + pageRect.width * 0.5f,
                                                    pageRect.y + pageRect.height * 0.5f});
        if (stampWidthSpin_) {
            ignoreStampWidth_ = true;
            stampWidthSpin_->setValue(static_cast<double>(pageRect.width));
            ignoreStampWidth_ = false;
        }
    } catch (const pdfforge::Error& ex) {
        showError(ex);
    }
}

void MainWindow::deleteSelectedSignature() {
    auto selected = canvas_ ? canvas_->selectedSignature() : std::nullopt;
    if (!document_ || !selected) {
        return;
    }
    try {
        document_->deleteImage(*selected);
        canvas_->clearSignatureSelection();
        refreshAfterMutation(false);
        thumbs_->refreshPage(canvas_->pageIndex());
        statusBar()->showMessage(tr("Firma eliminada del documento"), 4000);
    } catch (const pdfforge::Error& ex) {
        showError(ex);
    }
}

}  // namespace pdfforge::ui
