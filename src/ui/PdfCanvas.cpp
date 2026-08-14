#include "ui/PdfCanvas.h"

#include "pdf/PdfDocument.h"
#include "ui/Theme.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
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
    setMinimumSize(200, 200);
    setAutoFillBackground(false);
}

void PdfCanvas::setDocument(pdfforge::PdfDocument* document) {
    document_ = document;
    pageIndex_ = 0;
    hoverSpan_ = -1;
    hits_.clear();
    activeHit_ = -1;
    if (document_) {
        spans_ = document_->extractText(pageIndex_);
        requestRender();
    } else {
        spans_.clear();
        image_ = {};
        update();
    }
}

void PdfCanvas::setPage(int pageIndex) {
    if (!document_ || pageIndex == pageIndex_) {
        return;
    }
    pageIndex_ = pageIndex;
    hoverSpan_ = -1;
    spans_ = document_->extractText(pageIndex_);
    requestRender();
}

void PdfCanvas::setZoom(float zoom) {
    zoom_ = std::clamp(zoom, 0.25f, 8.0f);
    requestRender();
}

void PdfCanvas::setRotation(int quarterTurns) {
    rotation_ = ((quarterTurns % 4) + 4) % 4;
    requestRender();
}

void PdfCanvas::setSearchHits(const std::vector<pdfforge::SearchHit>& hits, int activeIndex) {
    hits_ = hits;
    activeHit_ = activeIndex;
    update();
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

int PdfCanvas::hitSpanAt(const QPoint& widgetPos) const {
    if (!document_ || image_.isNull()) {
        return -1;
    }
    const QPoint off = imageOffset();
    const float dx = static_cast<float>(widgetPos.x() - off.x());
    const float dy = static_cast<float>(widgetPos.y() - off.y());
    if (dx < 0 || dy < 0 || dx >= image_.width() || dy >= image_.height()) {
        return -1;
    }
    pdfforge::RenderRequest req;
    req.pageIndex = pageIndex_;
    req.dpi = dpi();
    req.rotationQuarterTurns = rotation_;
    pdfforge::PointF page;
    if (!document_->deviceToPage(pageIndex_, req, dx, dy, page)) {
        return -1;
    }
    for (int i = static_cast<int>(spans_.size()) - 1; i >= 0; --i) {
        if (spans_[static_cast<std::size_t>(i)].bounds().contains(page.x, page.y)) {
            return i;
        }
    }
    return -1;
}

void PdfCanvas::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), theme().workspace);
    if (image_.isNull()) {
        p.setPen(theme().muted);
        p.drawText(rect(), Qt::AlignCenter, tr("Open a PDF to begin"));
        return;
    }
    const QPoint off = imageOffset();
    p.fillRect(QRect(off, image_.size()).adjusted(-8, -8, 8, 8), theme().panel);
    p.drawImage(off, image_);

    const auto pageToWidget = [&](const pdfforge::RectF& r) {
        // Approximate mapping for rotation 0; device mapping is used for hit tests.
        const float s = dpi() / 72.0f;
        QRectF wr(off.x() + r.x * s, off.y() + (image_.height() - (r.y + r.height) * s), r.width * s,
                  r.height * s);
        return wr;
    };

    if (hoverSpan_ >= 0 && hoverSpan_ < static_cast<int>(spans_.size()) && rotation_ == 0) {
        QColor fill = theme().copper;
        fill.setAlpha(40);
        p.fillRect(pageToWidget(spans_[static_cast<std::size_t>(hoverSpan_)].bounds()), fill);
    }
    for (int i = 0; i < static_cast<int>(hits_.size()); ++i) {
        if (hits_[static_cast<std::size_t>(i)].pageIndex != pageIndex_ || rotation_ != 0) {
            continue;
        }
        QColor fill = (i == activeHit_) ? theme().copper : theme().copperSoft;
        if (i == activeHit_) {
            fill.setAlpha(70);
        }
        p.fillRect(pageToWidget(hits_[static_cast<std::size_t>(i)].bounds), fill);
    }
}

void PdfCanvas::mouseMoveEvent(QMouseEvent* event) {
    const int hit = hitSpanAt(event->pos());
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

}  // namespace pdfforge::ui
