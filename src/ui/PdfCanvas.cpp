#include "ui/PdfCanvas.h"

#include "pdf/PdfDocument.h"
#include "ui/Theme.h"

#include <QEvent>
#include <QFont>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QTimer>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace pdfforge::ui {
namespace {

QImage bitmapToImage(const pdfforge::Bitmap& bitmap) {
    if (bitmap.empty()) {
        return {};
    }
    QImage img(bitmap.bgra.data(), bitmap.width, bitmap.height, bitmap.stride,
               QImage::Format_ARGB32);
    return img.copy();
}

}  // namespace

PdfCanvas::PdfCanvas(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(200, 200);
    setAutoFillBackground(false);
    editor_ = new QLineEdit(this);
    editor_->setObjectName(QStringLiteral("pdfInlineEditor"));
    editor_->hide();
    editor_->setFrame(true);
    editor_->setAutoFillBackground(true);
    editor_->installEventFilter(this);
    connect(editor_, &QLineEdit::returnPressed, this, [this]() { finishInlineEdit(true); });
}

void PdfCanvas::setDocument(pdfforge::PdfDocument* document) {
    cancelInlineEdit();
    document_ = document;
    pageIndex_ = 0;
    hoverSpan_ = -1;
    selectedSpan_ = -1;
    hits_.clear();
    activeHit_ = -1;
    if (document_) {
        loadSpans();
        requestRender();
    } else {
        spans_.clear();
        image_ = {};
        emit selectionCleared();
        update();
    }
}

void PdfCanvas::setPage(int pageIndex) {
    if (!document_ || pageIndex == pageIndex_) {
        return;
    }
    cancelInlineEdit();
    pageIndex_ = pageIndex;
    hoverSpan_ = -1;
    selectedSpan_ = -1;
    emit selectionCleared();
    loadSpans();
    requestRender();
}

void PdfCanvas::setZoom(float zoom) {
    cancelInlineEdit();
    zoom_ = std::clamp(zoom, 0.25f, 8.0f);
    requestRender();
}

void PdfCanvas::setRotation(int quarterTurns) {
    cancelInlineEdit();
    rotation_ = ((quarterTurns % 4) + 4) % 4;
    requestRender();
}

void PdfCanvas::setSearchHits(const std::vector<pdfforge::SearchHit>& hits, int activeIndex) {
    hits_ = hits;
    activeHit_ = activeIndex;
    update();
}

void PdfCanvas::reload() {
    cancelInlineEdit();
    const int keep = selectedSpan_;
    loadSpans();
    if (keep >= 0 && keep < static_cast<int>(spans_.size())) {
        selectedSpan_ = keep;
        emit spanSelected(spans_[static_cast<std::size_t>(keep)]);
    } else {
        selectedSpan_ = -1;
        emit selectionCleared();
    }
    requestRender();
}

void PdfCanvas::setAddTextMode(bool enabled) {
    addTextMode_ = enabled;
    setCursor(enabled ? Qt::CrossCursor : Qt::ArrowCursor);
}

void PdfCanvas::clearSelection() {
    cancelInlineEdit();
    selectedSpan_ = -1;
    emit selectionCleared();
    update();
}

std::optional<pdfforge::TextSpan> PdfCanvas::selectedSpan() const {
    if (selectedSpan_ < 0 || selectedSpan_ >= static_cast<int>(spans_.size())) {
        return std::nullopt;
    }
    return spans_[static_cast<std::size_t>(selectedSpan_)];
}

float PdfCanvas::dpi() const {
    return 96.0f * zoom_;
}

void PdfCanvas::zoomIn() {
    setZoom(zoom_ * 1.15f);
}
void PdfCanvas::zoomOut() {
    setZoom(zoom_ / 1.15f);
}
void PdfCanvas::resetZoom() {
    setZoom(1.0f);
}

void PdfCanvas::loadSpans() {
    spans_.clear();
    if (!document_) {
        return;
    }
    try {
        spans_ = document_->extractText(pageIndex_);
    } catch (const pdfforge::Error&) {
        spans_.clear();
    }
}

void PdfCanvas::requestRender() {
    if (!document_) {
        return;
    }
    try {
        pdfforge::RenderRequest req;
        req.pageIndex = pageIndex_;
        req.dpi = dpi();
        req.rotationQuarterTurns = rotation_;
        applyBitmap(document_->render(req));
        emit statusMessage(tr("Page %1 · %2%")
                               .arg(pageIndex_ + 1)
                               .arg(static_cast<int>(std::lround(zoom_ * 100.0f))));
    } catch (const pdfforge::Error& ex) {
        image_ = {};
        emit statusMessage(QString::fromStdString(ex.userMessage()));
    }
    update();
}

void PdfCanvas::applyBitmap(const pdfforge::Bitmap& bitmap) {
    image_ = bitmapToImage(bitmap);
}

QPoint PdfCanvas::imageOffset() const {
    const int x = std::max(0, (width() - image_.width()) / 2);
    const int y = std::max(0, (height() - image_.height()) / 2);
    return {x, y};
}

bool PdfCanvas::widgetToPage(const QPoint& widgetPos, pdfforge::PointF& page) const {
    if (!document_ || image_.isNull()) {
        return false;
    }
    const QPoint off = imageOffset();
    const float dx = static_cast<float>(widgetPos.x() - off.x());
    const float dy = static_cast<float>(widgetPos.y() - off.y());
    if (dx < 0 || dy < 0 || dx >= image_.width() || dy >= image_.height()) {
        return false;
    }
    pdfforge::RenderRequest req;
    req.pageIndex = pageIndex_;
    req.dpi = dpi();
    req.rotationQuarterTurns = rotation_;
    return document_->deviceToPage(pageIndex_, req, dx, dy, page);
}

int PdfCanvas::hitSpanAt(const QPoint& widgetPos) const {
    pdfforge::PointF page;
    if (!widgetToPage(widgetPos, page)) {
        return -1;
    }
    for (int i = static_cast<int>(spans_.size()) - 1; i >= 0; --i) {
        if (spans_[static_cast<std::size_t>(i)].bounds().contains(page.x, page.y)) {
            return i;
        }
    }
    return -1;
}

QPolygonF PdfCanvas::spanPolygon(const pdfforge::RectF& r) const {
    QPolygonF poly;
    if (!document_ || image_.isNull()) {
        return poly;
    }
    pdfforge::RenderRequest req;
    req.pageIndex = pageIndex_;
    req.dpi = dpi();
    req.rotationQuarterTurns = rotation_;
    const QPoint off = imageOffset();
    const pdfforge::PointF corners[4] = {
        {r.x, r.y},
        {r.x + r.width, r.y},
        {r.x + r.width, r.y + r.height},
        {r.x, r.y + r.height},
    };
    for (const auto& c : corners) {
        pdfforge::PointF device;
        if (!document_->pageToDevice(pageIndex_, req, c.x, c.y, device)) {
            return {};
        }
        poly << QPointF(off.x() + device.x, off.y() + device.y);
    }
    return poly;
}

void PdfCanvas::beginInlineEdit() {
    if (selectedSpan_ < 0 || selectedSpan_ >= static_cast<int>(spans_.size())) {
        return;
    }
    const auto& span = spans_[static_cast<std::size_t>(selectedSpan_)];
    const QPolygonF poly = spanPolygon(span.bounds());
    if (poly.isEmpty()) {
        return;
    }
    const QRect rect = poly.boundingRect().adjusted(-4, -4, 8, 4).toRect();
    editingSpan_ = selectedSpan_;
    QFont font = editor_->font();
    const int pixelSize = std::max(10, static_cast<int>(std::lround(span.fontSize * dpi() / 72.0f)));
    font.setPixelSize(pixelSize);
    editor_->setFont(font);
    editor_->setGeometry(rect);
    editor_->setText(QString::fromStdString(span.text));
    editor_->show();
    editor_->setFocus();
    editor_->selectAll();
}

void PdfCanvas::finishInlineEdit(bool commit) {
    if (!editor_->isVisible()) {
        return;
    }
    const int index = editingSpan_;
    const QString text = editor_->text();
    editor_->hide();
    editingSpan_ = -1;
    if (!commit || index < 0 || index >= static_cast<int>(spans_.size())) {
        return;
    }
    const QString previous = QString::fromStdString(spans_[static_cast<std::size_t>(index)].text);
    if (text == previous) {
        return;
    }
    committing_ = true;
    emit spanEditCommitted(spans_[static_cast<std::size_t>(index)], text);
    QTimer::singleShot(0, this, [this]() { committing_ = false; });
}

void PdfCanvas::cancelInlineEdit() {
    if (editor_->isVisible()) {
        editor_->hide();
        editingSpan_ = -1;
    }
}

void PdfCanvas::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), theme().workspace);
    if (image_.isNull()) {
        p.setPen(theme().muted);
        p.drawText(rect(), Qt::AlignCenter, tr("Open a PDF to begin"));
        return;
    }
    const QPoint off = imageOffset();
    p.fillRect(QRect(off, image_.size()).adjusted(-8, -8, 8, 8), theme().panel);
    p.drawImage(off, image_);
    if (editor_->isVisible() && editingSpan_ >= 0 &&
        editingSpan_ < static_cast<int>(spans_.size())) {
        const QPolygonF cover = spanPolygon(spans_[static_cast<std::size_t>(editingSpan_)].bounds());
        if (!cover.isEmpty()) {
            p.setBrush(theme().paper);
            p.setPen(Qt::NoPen);
            p.drawPolygon(cover);
        }
    }

    const auto drawPoly = [&](const pdfforge::RectF& bounds, const QColor& fill, const QColor& stroke) {
        const QPolygonF poly = spanPolygon(bounds);
        if (poly.isEmpty()) {
            return;
        }
        p.setBrush(fill);
        p.setPen(QPen(stroke, 1));
        p.drawPolygon(poly);
    };

    if (hoverSpan_ >= 0 && hoverSpan_ < static_cast<int>(spans_.size()) &&
        !(editor_->isVisible() && hoverSpan_ == editingSpan_)) {
        QColor fill = theme().copper;
        fill.setAlpha(36);
        QColor stroke = theme().copper;
        stroke.setAlpha(140);
        drawPoly(spans_[static_cast<std::size_t>(hoverSpan_)].bounds(), fill, stroke);
    }
    if (selectedSpan_ >= 0 && selectedSpan_ < static_cast<int>(spans_.size()) &&
        !(editor_->isVisible() && selectedSpan_ == editingSpan_)) {
        QColor fill = theme().copper;
        fill.setAlpha(70);
        drawPoly(spans_[static_cast<std::size_t>(selectedSpan_)].bounds(), fill, theme().copper);
    }
    for (int i = 0; i < static_cast<int>(hits_.size()); ++i) {
        if (hits_[static_cast<std::size_t>(i)].pageIndex != pageIndex_) {
            continue;
        }
        QColor fill = (i == activeHit_) ? theme().copper : theme().copperSoft;
        if (i == activeHit_) {
            fill.setAlpha(80);
        } else {
            fill.setAlpha(40);
        }
        QColor stroke = theme().copper;
        stroke.setAlpha(i == activeHit_ ? 200 : 80);
        drawPoly(hits_[static_cast<std::size_t>(i)].bounds, fill, stroke);
    }
}

void PdfCanvas::mouseMoveEvent(QMouseEvent* event) {
    if (addTextMode_) {
        setCursor(Qt::CrossCursor);
        return;
    }
    const int hit = hitSpanAt(event->pos());
    setCursor(hit >= 0 ? Qt::IBeamCursor : Qt::ArrowCursor);
    if (hit != hoverSpan_) {
        hoverSpan_ = hit;
        if (hit >= 0) {
            emit hoverSpanChanged(QString::fromStdString(spans_[static_cast<std::size_t>(hit)].text));
        } else {
            emit hoverSpanChanged({});
        }
        update();
    }
}

void PdfCanvas::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        return;
    }
    finishInlineEdit(true);
    if (addTextMode_) {
        pdfforge::PointF page;
        if (widgetToPage(event->pos(), page)) {
            emit emptyPageClicked(page);
        }
        return;
    }
    const int hit = hitSpanAt(event->pos());
    selectedSpan_ = hit;
    if (hit >= 0) {
        emit spanSelected(spans_[static_cast<std::size_t>(hit)]);
    } else {
        emit selectionCleared();
    }
    update();
}

void PdfCanvas::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        return;
    }
    const int hit = hitSpanAt(event->pos());
    if (hit >= 0) {
        selectedSpan_ = hit;
        emit spanSelected(spans_[static_cast<std::size_t>(hit)]);
        beginInlineEdit();
    }
}

void PdfCanvas::leaveEvent(QEvent*) {
    hoverSpan_ = -1;
    emit hoverSpanChanged({});
    update();
}

void PdfCanvas::wheelEvent(QWheelEvent* event) {
    if (event->modifiers() & Qt::ControlModifier) {
        if (event->angleDelta().y() > 0) {
            zoomIn();
        } else {
            zoomOut();
        }
        event->accept();
        return;
    }
    QWidget::wheelEvent(event);
}

void PdfCanvas::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    update();
}

void PdfCanvas::keyPressEvent(QKeyEvent* event) {
    if (committing_ || editor_->isVisible()) {
        if (event->key() == Qt::Key_Escape || event->key() == Qt::Key_Return ||
            event->key() == Qt::Key_Enter) {
            event->accept();
            return;
        }
    }
    if (event->key() == Qt::Key_Escape) {
        if (addTextMode_) {
            setAddTextMode(false);
            event->accept();
            return;
        }
        clearSelection();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_F2) {
        beginInlineEdit();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

bool PdfCanvas::eventFilter(QObject* watched, QEvent* event) {
    if (watched == editor_ && event->type() == QEvent::KeyPress) {
        const auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            finishInlineEdit(true);
            return true;
        }
        if (key->key() == Qt::Key_Escape) {
            cancelInlineEdit();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

}  // namespace pdfforge::ui
