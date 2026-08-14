#include "ui/PdfCanvas.h"

#include "core/Error.h"
#include "pdf/PdfDocument.h"
#include "ui/Theme.h"

#include <QCursor>
#include <QEvent>
#include <QFont>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPen>
#include <QPixmap>
#include <QPoint>
#include <QPolygon>
#include <QRect>
#include <QRectF>
#include <QResizeEvent>
#include <QTimer>
#include <QWheelEvent>
#include <QWidget>

#include <algorithm>
#include <cmath>

namespace pdfforge::ui {
namespace {

constexpr QColor kSelectBlue{0x1A, 0x73, 0xE8};
constexpr int kHandlePad = 6;

QImage bitmapToImage(const pdfforge::Bitmap& bitmap) {
    if (bitmap.empty()) {
        return {};
    }
    QImage img(bitmap.bgra.data(), bitmap.width, bitmap.height, bitmap.stride,
               QImage::Format_ARGB32);
    return img.copy();
}

QCursor fourArrowCursor() {
    static const QCursor kCursor = []() {
        QPixmap pm(32, 32);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing, false);
        const QPoint c(15, 15);
        auto drawArrows = [&](const QColor& color, int width) {
            p.setPen(QPen(color, width, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
            p.drawLine(QPoint(c.x(), 3), QPoint(c.x(), 27));
            p.drawLine(QPoint(3, c.y()), QPoint(27, c.y()));
            p.setBrush(color);
            p.setPen(Qt::NoPen);
            p.drawPolygon(QPolygon() << QPoint(c.x(), 1) << QPoint(c.x() - 5, 8)
                                    << QPoint(c.x() + 5, 8));
            p.drawPolygon(QPolygon() << QPoint(c.x(), 29) << QPoint(c.x() - 5, 22)
                                    << QPoint(c.x() + 5, 22));
            p.drawPolygon(QPolygon() << QPoint(1, c.y()) << QPoint(8, c.y() - 5)
                                    << QPoint(8, c.y() + 5));
            p.drawPolygon(QPolygon() << QPoint(29, c.y()) << QPoint(22, c.y() - 5)
                                    << QPoint(22, c.y() + 5));
        };
        drawArrows(Qt::white, 5);
        drawArrows(Qt::black, 3);
        p.end();
        return QCursor(pm, 15, 15);
    }();
    return kCursor;
}

class RegionFrame final : public QWidget {
public:
    explicit RegionFrame(QWidget* parent) : QWidget(parent) {
        setMouseTracking(true);
        setAutoFillBackground(false);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setCursor(fourArrowCursor());
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, false);
        const QRect box = rect().adjusted(kHandlePad, kHandlePad, -kHandlePad, -kHandlePad);
        p.setPen(QPen(kSelectBlue, 1));
        p.setBrush(Qt::NoBrush);
        p.drawRect(box.adjusted(0, 0, -1, -1));
        const QPoint pts[] = {
            box.topLeft(),
            box.topRight() - QPoint(1, 0),
            box.bottomLeft() - QPoint(0, 1),
            box.bottomRight() - QPoint(1, 1),
            QPoint(box.center().x(), box.top()),
            QPoint(box.center().x(), box.bottom() - 1),
            QPoint(box.left(), box.center().y()),
            QPoint(box.right() - 1, box.center().y()),
        };
        p.setBrush(kSelectBlue);
        p.setPen(QPen(Qt::white, 1));
        for (const QPoint& pt : pts) {
            p.drawRect(QRect(pt.x() - 3, pt.y() - 3, 7, 7));
        }
    }
};

}  // namespace

PdfCanvas::PdfCanvas(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(200, 200);
    setAutoFillBackground(false);
    regionFrame_ = new RegionFrame(this);
    regionFrame_->hide();
    regionFrame_->installEventFilter(this);
    editor_ = new QLineEdit(this);
    editor_->setObjectName(QStringLiteral("pdfInlineEditor"));
    editor_->hide();
    editor_->setFrame(false);
    editor_->setAutoFillBackground(true);
    editor_->setMouseTracking(true);
    editor_->setCursor(Qt::IBeamCursor);
    editor_->installEventFilter(this);
    editor_->raise();
    connect(editor_, &QLineEdit::returnPressed, this, [this]() { finishInlineEdit(true); });
}

void PdfCanvas::setDocument(pdfforge::PdfDocument* document) {
    cancelInlineEdit();
    stopRegionMove();
    document_ = document;
    pageIndex_ = 0;
    hoverSpan_ = -1;
    selectedSpan_ = -1;
    selectedSignature_ = -1;
    resizingStamp_ = false;
    marqueeDrag_ = false;
    movingRegion_ = false;
    hasRegion_ = false;
    hits_.clear();
    activeHit_ = -1;
    signatures_.clear();
    if (document_) {
        loadSpans();
        loadSignatures();
        requestRender();
    } else {
        spans_.clear();
        image_ = {};
        emit selectionCleared();
        emit signatureSelectionCleared();
        update();
    }
}

void PdfCanvas::setPage(int pageIndex) {
    if (!document_ || pageIndex == pageIndex_) {
        return;
    }
    cancelInlineEdit();
    stopRegionMove();
    pageIndex_ = pageIndex;
    hoverSpan_ = -1;
    selectedSpan_ = -1;
    selectedSignature_ = -1;
    resizingStamp_ = false;
    hasRegion_ = false;
    movingRegion_ = false;
    emit selectionCleared();
    emit signatureSelectionCleared();
    loadSpans();
    loadSignatures();
    requestRender();
}

void PdfCanvas::setZoom(float zoom) {
    cancelInlineEdit();
    stopRegionMove();
    zoom_ = std::clamp(zoom, 0.25f, 8.0f);
    requestRender();
}

void PdfCanvas::setRotation(int quarterTurns) {
    cancelInlineEdit();
    stopRegionMove();
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
    stopRegionMove();
    const int keepSignIndex = selectedSignature_ >= 0 ? signatures_[static_cast<std::size_t>(selectedSignature_)].pdfObjectIndex : -1;
    loadSpans();
    loadSignatures();
    selectedSpan_ = -1;
    hasRegion_ = false;
    movingRegion_ = false;
    emit selectionCleared();
    selectedSignature_ = -1;
    if (keepSignIndex >= 0) {
        for (int i = 0; i < static_cast<int>(signatures_.size()); ++i) {
            if (signatures_[static_cast<std::size_t>(i)].pdfObjectIndex == keepSignIndex) {
                selectedSignature_ = i;
                emit signatureSelected(signatures_[static_cast<std::size_t>(i)]);
                break;
            }
        }
    }
    if (selectedSignature_ < 0) {
        emit signatureSelectionCleared();
    }
    requestRender();
}

void PdfCanvas::setAddTextMode(bool enabled) {
    addTextMode_ = enabled;
    if (enabled) {
        placeStampMode_ = false;
    }
    setCursor(enabled ? Qt::CrossCursor : Qt::ArrowCursor);
}

void PdfCanvas::setPlaceStampMode(bool enabled) {
    placeStampMode_ = enabled;
    if (enabled) {
        addTextMode_ = false;
        cancelInlineEdit();
        selectedSpan_ = -1;
        hoverSpan_ = -1;
        clearSignatureSelection();
    }
    setCursor(enabled ? Qt::CrossCursor : Qt::ArrowCursor);
    update();
}

void PdfCanvas::setSignWorkspace(bool enabled) {
    signWorkspace_ = enabled;
    if (!enabled) {
        clearSignatureSelection();
        signatures_.clear();
    } else {
        loadSignatures();
    }
    update();
}

void PdfCanvas::setStampPreview(const QImage& image, float widthPt) {
    stampPreview_ = image;
    stampWidthPt_ = std::max(8.0f, widthPt);
    update();
}

void PdfCanvas::clearSignatureSelection() {
    resizingStamp_ = false;
    activeHandle_ = StampHandle::None;
    if (selectedSignature_ >= 0) {
        selectedSignature_ = -1;
        emit signatureSelectionCleared();
        update();
    }
}

void PdfCanvas::selectSignatureAt(const pdfforge::PointF& pagePoint) {
    loadSignatures();
    int hit = -1;
    for (int i = static_cast<int>(signatures_.size()) - 1; i >= 0; --i) {
        if (signatures_[static_cast<std::size_t>(i)].bounds.contains(pagePoint.x, pagePoint.y)) {
            hit = i;
            break;
        }
    }
    if (hit < 0 && !signatures_.empty()) {
        hit = static_cast<int>(signatures_.size()) - 1;
    }
    selectedSignature_ = hit;
    resizingStamp_ = false;
    if (hit >= 0) {
        emit signatureSelected(signatures_[static_cast<std::size_t>(hit)]);
    } else {
        emit signatureSelectionCleared();
    }
    update();
}

std::optional<pdfforge::ImageObject> PdfCanvas::selectedSignature() const {
    if (selectedSignature_ < 0 || selectedSignature_ >= static_cast<int>(signatures_.size())) {
        return std::nullopt;
    }
    return signatures_[static_cast<std::size_t>(selectedSignature_)];
}

void PdfCanvas::loadSignatures() {
    signatures_.clear();
    if (!document_ || !signWorkspace_) {
        return;
    }
    try {
        for (const auto& image : document_->extractImages(pageIndex_)) {
            if (image.isSignature) {
                signatures_.push_back(image);
            }
        }
    } catch (const pdfforge::Error&) {
        signatures_.clear();
    }
}

void PdfCanvas::clearSelection() {
    cancelInlineEdit();
    stopRegionMove();
    selectedSpan_ = -1;
    hasRegion_ = false;
    movingRegion_ = false;
    marqueeDrag_ = false;
    emit selectionCleared();
    update();
}

void PdfCanvas::setRegionSelection(const pdfforge::RectF& pageRect, const QString& text) {
    regionRect_ = pageRect;
    regionText_ = text;
    hasRegion_ = !pageRect.empty();
    selectedSpan_ = -1;
    syncRegionFrame();
    update();
}

void PdfCanvas::clearRegionSelection() {
    stopRegionMove();
    hasRegion_ = false;
    regionText_.clear();
    marqueeDrag_ = false;
    syncRegionFrame();
    update();
}

std::optional<pdfforge::RectF> PdfCanvas::selectedRegion() const {
    if (!hasRegion_) {
        return std::nullopt;
    }
    return regionRect_;
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
    syncRegionFrame();
}

void PdfCanvas::applyBitmap(const pdfforge::Bitmap& bitmap) {
    image_ = bitmapToImage(bitmap);
}

QPoint PdfCanvas::imageOffset() const {
    const int x = std::max(0, (width() - image_.width()) / 2);
    const int y = std::max(0, (height() - image_.height()) / 2);
    return {x, y};
}

QRect PdfCanvas::imageRect() const {
    if (image_.isNull()) {
        return {};
    }
    return QRect(imageOffset(), image_.size());
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

bool PdfCanvas::widgetToPageClamped(const QPoint& widgetPos, pdfforge::PointF& page) const {
    const QRect img = imageRect();
    if (img.isEmpty()) {
        return false;
    }
    const QPoint clamped(std::clamp(widgetPos.x(), img.left(), img.right() - 1),
                         std::clamp(widgetPos.y(), img.top(), img.bottom() - 1));
    return widgetToPage(clamped, page);
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

int PdfCanvas::hitSignatureAt(const QPoint& widgetPos) const {
    pdfforge::PointF page;
    if (!widgetToPage(widgetPos, page)) {
        return -1;
    }
    for (int i = static_cast<int>(signatures_.size()) - 1; i >= 0; --i) {
        if (signatures_[static_cast<std::size_t>(i)].bounds.contains(page.x, page.y)) {
            return i;
        }
    }
    return -1;
}

QRectF PdfCanvas::signatureWidgetRect(const pdfforge::RectF& bounds) const {
    const QPolygonF poly = spanPolygon(bounds);
    if (poly.isEmpty()) {
        return {};
    }
    return poly.boundingRect();
}

PdfCanvas::StampHandle PdfCanvas::hitStampHandle(const QPoint& widgetPos) const {
    if (selectedSignature_ < 0 || selectedSignature_ >= static_cast<int>(signatures_.size())) {
        return StampHandle::None;
    }
    const QRectF box = signatureWidgetRect(signatures_[static_cast<std::size_t>(selectedSignature_)].bounds);
    if (box.isEmpty()) {
        return StampHandle::None;
    }
    constexpr qreal k = 10.0;
    const QRectF nw(box.left() - k * 0.5, box.top() - k * 0.5, k, k);
    const QRectF ne(box.right() - k * 0.5, box.top() - k * 0.5, k, k);
    const QRectF sw(box.left() - k * 0.5, box.bottom() - k * 0.5, k, k);
    const QRectF se(box.right() - k * 0.5, box.bottom() - k * 0.5, k, k);
    if (se.contains(widgetPos)) {
        return StampHandle::SE;
    }
    if (nw.contains(widgetPos)) {
        return StampHandle::NW;
    }
    if (ne.contains(widgetPos)) {
        return StampHandle::NE;
    }
    if (sw.contains(widgetPos)) {
        return StampHandle::SW;
    }
    return StampHandle::None;
}

pdfforge::RectF PdfCanvas::resizedSignatureRect(StampHandle handle, const pdfforge::PointF& page) const {
    if (selectedSignature_ < 0 || selectedSignature_ >= static_cast<int>(signatures_.size())) {
        return {};
    }
    const pdfforge::RectF r = signatures_[static_cast<std::size_t>(selectedSignature_)].bounds;
    const float aspect = r.height / std::max(1.0f, r.width);
    const float right = r.x + r.width;
    const float top = r.y + r.height;
    float width = r.width;
    float height = r.height;
    float x = r.x;
    float y = r.y;
    switch (handle) {
        case StampHandle::SE:
            width = std::max(24.0f, page.x - r.x);
            height = width * aspect;
            break;
        case StampHandle::NE:
            width = std::max(24.0f, page.x - r.x);
            height = width * aspect;
            y = top - height;
            break;
        case StampHandle::SW:
            width = std::max(24.0f, right - page.x);
            height = width * aspect;
            x = right - width;
            break;
        case StampHandle::NW:
            width = std::max(24.0f, right - page.x);
            height = width * aspect;
            x = right - width;
            y = top - height;
            break;
        case StampHandle::None:
            break;
    }
    width = std::clamp(width, 24.0f, 480.0f);
    height = width * aspect;
    if (handle == StampHandle::NE || handle == StampHandle::NW) {
        y = top - height;
    }
    if (handle == StampHandle::SW || handle == StampHandle::NW) {
        x = right - width;
    }
    return pdfforge::RectF{x, y, width, height};
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

QRectF PdfCanvas::regionWidgetRect() const {
    if (!hasRegion_) {
        return {};
    }
    const QPolygonF poly = spanPolygon(regionRect_);
    if (poly.isEmpty()) {
        return {};
    }
    return poly.boundingRect();
}

QRect PdfCanvas::regionEditorRect() const {
    const QRect box = regionWidgetRect().toRect();
    if (box.isEmpty()) {
        return {};
    }
    const int pad = (box.height() < 28 || box.width() < 56) ? 5 : 10;
    QRect inner = box.adjusted(pad, pad, -pad, -pad);
    if (inner.width() < 16) {
        inner.setLeft(box.left() + 2);
        inner.setWidth(std::max(16, box.width() - 4));
    }
    if (inner.height() < 12) {
        inner.setTop(box.top() + 2);
        inner.setHeight(std::max(12, box.height() - 4));
    }
    return inner;
}

void PdfCanvas::syncRegionFrame() {
    if (!regionFrame_) {
        return;
    }
    if (!hasRegion_ || signWorkspace_ || placeStampMode_) {
        regionFrame_->hide();
        return;
    }
    const QRect box = regionWidgetRect().toRect();
    if (box.isEmpty()) {
        regionFrame_->hide();
        return;
    }
    regionFrame_->setGeometry(box.adjusted(-kHandlePad, -kHandlePad, kHandlePad, kHandlePad));
    regionFrame_->setCursor(fourArrowCursor());
    regionFrame_->show();
    regionFrame_->raise();
    if (editor_->isVisible()) {
        editor_->raise();
    }
}

bool PdfCanvas::hitsRegionMoveHandle(const QPoint& widgetPos) const {
    if (!hasRegion_ || signWorkspace_ || placeStampMode_ || addTextMode_) {
        return false;
    }
    const QRectF box = regionWidgetRect();
    if (box.isEmpty()) {
        return false;
    }
    constexpr qreal k = 10.0;
    const QRectF outer = box.adjusted(-k, -k, k, k);
    if (!outer.contains(widgetPos)) {
        return false;
    }
    QRectF inner = box.adjusted(k, k, -k, -k);
    if (inner.width() < 8.0 || inner.height() < 8.0) {
        return true;
    }
    return !inner.contains(widgetPos);
}

void PdfCanvas::startRegionMove(const QPoint& widgetPos) {
    pdfforge::PointF page;
    if (!widgetToPage(widgetPos, page) && !widgetToPageClamped(widgetPos, page)) {
        page = pdfforge::PointF{regionRect_.x + regionRect_.width * 0.5f,
                                regionRect_.y + regionRect_.height * 0.5f};
    }
    movingRegion_ = true;
    moveLastPage_ = page;
    setCursor(fourArrowCursor());
    if (regionFrame_) {
        regionFrame_->setCursor(fourArrowCursor());
    }
    if (editor_->isVisible()) {
        editor_->setCursor(fourArrowCursor());
    }
    grabMouse();
}

void PdfCanvas::stopRegionMove() {
    if (!movingRegion_ && mouseGrabber() != this) {
        return;
    }
    movingRegion_ = false;
    if (mouseGrabber() == this) {
        releaseMouse();
    }
    setCursor(hasRegion_ ? fourArrowCursor() : Qt::ArrowCursor);
    if (regionFrame_) {
        regionFrame_->setCursor(fourArrowCursor());
    }
    if (editor_->isVisible()) {
        editor_->setCursor(Qt::IBeamCursor);
        editor_->setFocus();
    }
}

void PdfCanvas::clampRegionToPage() {
    if (!document_ || regionRect_.empty()) {
        return;
    }
    const pdfforge::SizeF page = document_->pageSize(pageIndex_);
    regionRect_.x = std::clamp(regionRect_.x, 0.0f, std::max(0.0f, page.width - regionRect_.width));
    regionRect_.y = std::clamp(regionRect_.y, 0.0f, std::max(0.0f, page.height - regionRect_.height));
}

void PdfCanvas::syncEditorGeometry() {
    if (!editor_->isVisible() || !hasRegion_) {
        return;
    }
    const QRect rect = regionEditorRect();
    if (rect.isEmpty()) {
        return;
    }
    int pixelSize = std::max(10, rect.height() - 4);
    QFont font = editor_->font();
    font.setPixelSize(pixelSize);
    editor_->setFont(font);
    editor_->setGeometry(rect);
    syncRegionFrame();
}

void PdfCanvas::beginInlineEdit() {
    QRect rect;
    QString text;
    int pixelSize = 14;
    if (hasRegion_) {
        const QRect rectBox = regionEditorRect();
        if (rectBox.isEmpty()) {
            return;
        }
        rect = rectBox;
        text = regionText_;
        pixelSize = std::max(10, rect.height() - 4);
        editingSpan_ = -2;
    } else if (selectedSpan_ >= 0 && selectedSpan_ < static_cast<int>(spans_.size())) {
        const auto& span = spans_[static_cast<std::size_t>(selectedSpan_)];
        const QPolygonF poly = spanPolygon(span.bounds());
        if (poly.isEmpty()) {
            return;
        }
        rect = poly.boundingRect().adjusted(-4, -4, 8, 4).toRect();
        text = QString::fromStdString(span.text);
        pixelSize = std::max(10, static_cast<int>(std::lround(span.fontSize * dpi() / 72.0f)));
        editingSpan_ = selectedSpan_;
    } else {
        return;
    }
    QFont font = editor_->font();
    font.setPixelSize(pixelSize);
    editor_->setFont(font);
    editor_->setGeometry(rect);
    editor_->setText(text);
    editor_->show();
    editor_->setFocus();
    editor_->selectAll();
    syncRegionFrame();
}

void PdfCanvas::finishInlineEdit(bool commit) {
    if (!editor_->isVisible()) {
        return;
    }
    const int index = editingSpan_;
    const QString text = editor_->text();
    editor_->hide();
    editingSpan_ = -1;
    if (!commit) {
        return;
    }
    if (index == -2 || hasRegion_) {
        regionText_ = text;
        emit regionEditCommitted(text);
        return;
    }
    if (index < 0 || index >= static_cast<int>(spans_.size())) {
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
    syncRegionFrame();
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
    if (editor_->isVisible() && hasRegion_) {
        const QPolygonF cover = spanPolygon(regionRect_);
        if (!cover.isEmpty()) {
            p.setBrush(theme().paper);
            p.setPen(Qt::NoPen);
            p.drawPolygon(cover);
        }
    } else if (editor_->isVisible() && editingSpan_ >= 0 &&
               editingSpan_ < static_cast<int>(spans_.size())) {
        const QPolygonF cover = spanPolygon(spans_[static_cast<std::size_t>(editingSpan_)].bounds());
        if (!cover.isEmpty()) {
            p.setBrush(theme().paper);
            p.setPen(Qt::NoPen);
            p.drawPolygon(cover);
        }
    }
    if (placeStampMode_ && !stampPreview_.isNull() && document_ && !image_.isNull()) {
        pdfforge::PointF page;
        if (widgetToPage(lastMouse_, page) && stampPreview_.width() > 0) {
            const float heightPt =
                stampWidthPt_ * static_cast<float>(stampPreview_.height()) /
                static_cast<float>(stampPreview_.width());
            const pdfforge::RectF bounds{page.x - stampWidthPt_ * 0.5f, page.y - heightPt * 0.5f,
                                         stampWidthPt_, heightPt};
            const QPolygonF poly = spanPolygon(bounds);
            if (!poly.isEmpty()) {
                const QRectF box = poly.boundingRect();
                p.setOpacity(0.72);
                p.drawImage(box, stampPreview_);
                p.setOpacity(1.0);
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen(theme().copper, 1, Qt::DashLine));
                p.drawRect(box);
            }
        }
    }

    if (signWorkspace_ && selectedSignature_ >= 0 &&
        selectedSignature_ < static_cast<int>(signatures_.size()) && !placeStampMode_) {
        const pdfforge::RectF bounds =
            resizingStamp_ ? liveStampRect_ : signatures_[static_cast<std::size_t>(selectedSignature_)].bounds;
        const QRectF box = signatureWidgetRect(bounds);
        if (!box.isEmpty()) {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(theme().copper, 2));
            p.drawRect(box);
            constexpr qreal k = 8.0;
            p.setBrush(theme().copper);
            p.setPen(Qt::NoPen);
            const QPointF corners[4] = {box.topLeft(), box.topRight(), box.bottomLeft(), box.bottomRight()};
            for (const auto& c : corners) {
                p.drawRect(QRectF(c.x() - k * 0.5, c.y() - k * 0.5, k, k));
            }
        }
    }

    if (marqueeDrag_ && !signWorkspace_ && !placeStampMode_) {
        const QRect box = QRect(marqueeOrigin_, lastMouse_).normalized();
        QColor fill = theme().copper;
        fill.setAlpha(28);
        p.setBrush(fill);
        p.setPen(QPen(theme().copper, 1, Qt::DashLine));
        p.drawRect(box);
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
    lastMouse_ = event->pos();
    if (placeStampMode_) {
        setCursor(Qt::CrossCursor);
        update();
        return;
    }
    if (signWorkspace_) {
        if (resizingStamp_) {
            pdfforge::PointF page;
            if (widgetToPage(event->pos(), page)) {
                liveStampRect_ = resizedSignatureRect(activeHandle_, page);
                update();
            }
            return;
        }
        const StampHandle handle = hitStampHandle(event->pos());
        if (handle == StampHandle::NW || handle == StampHandle::SE) {
            setCursor(Qt::SizeFDiagCursor);
        } else if (handle == StampHandle::NE || handle == StampHandle::SW) {
            setCursor(Qt::SizeBDiagCursor);
        } else if (hitSignatureAt(event->pos()) >= 0) {
            setCursor(Qt::PointingHandCursor);
        } else {
            setCursor(Qt::ArrowCursor);
        }
        return;
    }
    if (addTextMode_) {
        setCursor(Qt::CrossCursor);
        return;
    }
    if (movingRegion_) {
        pdfforge::PointF page;
        if (widgetToPageClamped(event->pos(), page)) {
            regionRect_.x += page.x - moveLastPage_.x;
            regionRect_.y += page.y - moveLastPage_.y;
            clampRegionToPage();
            moveLastPage_ = page;
            syncEditorGeometry();
            update();
        }
        setCursor(fourArrowCursor());
        return;
    }
    if (!signWorkspace_ && !placeStampMode_) {
        if (hitsRegionMoveHandle(event->pos())) {
            setCursor(fourArrowCursor());
        } else {
            setCursor(Qt::CrossCursor);
        }
        if (marqueeDrag_) {
            update();
        }
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
    if (!signWorkspace_ && !placeStampMode_ && !addTextMode_ && hitsRegionMoveHandle(event->pos())) {
        startRegionMove(event->pos());
        return;
    }
    finishInlineEdit(true);
    if (placeStampMode_) {
        pdfforge::PointF page;
        if (widgetToPage(event->pos(), page)) {
            emit stampPlaced(page);
        }
        return;
    }
    if (signWorkspace_) {
        const StampHandle handle = hitStampHandle(event->pos());
        if (handle != StampHandle::None) {
            resizingStamp_ = true;
            activeHandle_ = handle;
            liveStampRect_ = signatures_[static_cast<std::size_t>(selectedSignature_)].bounds;
            setFocus();
            return;
        }
        const int hit = hitSignatureAt(event->pos());
        selectedSignature_ = hit;
        resizingStamp_ = false;
        if (hit >= 0) {
            emit signatureSelected(signatures_[static_cast<std::size_t>(hit)]);
        } else {
            emit signatureSelectionCleared();
        }
        setFocus();
        update();
        return;
    }
    if (addTextMode_) {
        pdfforge::PointF page;
        if (widgetToPage(event->pos(), page)) {
            emit emptyPageClicked(page);
        }
        return;
    }
    if (!signWorkspace_ && !placeStampMode_) {
        finishInlineEdit(true);
        marqueeDrag_ = true;
        marqueeOrigin_ = event->pos();
        lastMouse_ = event->pos();
        setCursor(Qt::CrossCursor);
        setFocus();
        update();
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

void PdfCanvas::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }
    if (movingRegion_) {
        stopRegionMove();
        update();
        return;
    }
    if (marqueeDrag_ && !signWorkspace_ && !placeStampMode_) {
        marqueeDrag_ = false;
        const QRect box = QRect(marqueeOrigin_, event->pos()).normalized();
        lastMouse_ = event->pos();
        if (box.width() < 8 || box.height() < 8) {
            clearSelection();
            update();
            return;
        }
        pdfforge::PointF a;
        pdfforge::PointF b;
        if (!widgetToPageClamped(box.topLeft(), a) || !widgetToPageClamped(box.bottomRight(), b)) {
            update();
            return;
        }
        const pdfforge::RectF pageRect{std::min(a.x, b.x), std::min(a.y, b.y),
                                       std::fabs(a.x - b.x), std::fabs(a.y - b.y)};
        emit regionSelected(pageRect);
        update();
        return;
    }
    if (!resizingStamp_) {
        QWidget::mouseReleaseEvent(event);
        return;
    }
    resizingStamp_ = false;
    if (selectedSignature_ >= 0 && selectedSignature_ < static_cast<int>(signatures_.size()) &&
        liveStampRect_.width >= 1.0f && liveStampRect_.height >= 1.0f) {
        emit signatureResizeCommitted(signatures_[static_cast<std::size_t>(selectedSignature_)],
                                      liveStampRect_);
    }
    activeHandle_ = StampHandle::None;
}

void PdfCanvas::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton || placeStampMode_ || signWorkspace_) {
        return;
    }
    if (hasRegion_) {
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
        if (signWorkspace_) {
            if (placeStampMode_) {
                setPlaceStampMode(false);
            }
            clearSignatureSelection();
            event->accept();
            return;
        }
        clearSelection();
        event->accept();
        return;
    }
    if (signWorkspace_ && selectedSignature_ >= 0 &&
        (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)) {
        emit signatureDeleteRequested();
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
    if (watched == regionFrame_) {
        if (event->type() == QEvent::MouseButtonPress) {
            const auto* mouse = static_cast<QMouseEvent*>(event);
            if (mouse->button() == Qt::LeftButton) {
                startRegionMove(regionFrame_->mapToParent(mouse->pos()));
                return true;
            }
        }
        if (event->type() == QEvent::Enter || event->type() == QEvent::MouseMove) {
            regionFrame_->setCursor(fourArrowCursor());
            setCursor(fourArrowCursor());
        }
        return false;
    }
    if (watched == editor_) {
        if (event->type() == QEvent::MouseMove || event->type() == QEvent::MouseButtonPress) {
            const auto* mouse = static_cast<QMouseEvent*>(event);
            const QPoint onCanvas = editor_->mapToParent(mouse->pos());
            if (event->type() == QEvent::MouseMove && !movingRegion_) {
                editor_->setCursor(hitsRegionMoveHandle(onCanvas) ? fourArrowCursor()
                                                                 : Qt::IBeamCursor);
            }
            if (event->type() == QEvent::MouseButtonPress && mouse->button() == Qt::LeftButton &&
                hitsRegionMoveHandle(onCanvas)) {
                startRegionMove(onCanvas);
                return true;
            }
        }
        if (event->type() == QEvent::KeyPress) {
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
    }
    return QWidget::eventFilter(watched, event);
}

}  // namespace pdfforge::ui
