#pragma once

#include "model/Objects.h"
#include "pdf/PdfSearch.h"
#include "renderer/Bitmap.h"

#include <QImage>
#include <QWidget>

#include <memory>
#include <vector>

namespace pdfforge {
class PdfDocument;
}

namespace pdfforge::ui {

class PdfCanvas : public QWidget {
    Q_OBJECT
public:
    explicit PdfCanvas(QWidget* parent = nullptr);

    void setDocument(pdfforge::PdfDocument* document);
    void setPage(int pageIndex);
    void setZoom(float zoom);
    void setRotation(int quarterTurns);
    void setSearchHits(const std::vector<pdfforge::SearchHit>& hits, int activeIndex);

    [[nodiscard]] int pageIndex() const { return pageIndex_; }
    [[nodiscard]] float zoom() const { return zoom_; }
    [[nodiscard]] int rotation() const { return rotation_; }
    [[nodiscard]] float dpi() const;

public slots:
    void zoomIn();
    void zoomOut();
    void resetZoom();

signals:
    void hoverSpanChanged(const QString& preview);
    void statusMessage(const QString& text);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void requestRender();
    void applyBitmap(const pdfforge::Bitmap& bitmap);
    QPoint imageOffset() const;
    int hitSpanAt(const QPoint& widgetPos) const;

    pdfforge::PdfDocument* document_ = nullptr;
    int pageIndex_ = 0;
    float zoom_ = 1.0f;
    int rotation_ = 0;
    QImage image_;
    std::vector<pdfforge::TextSpan> spans_;
    std::vector<pdfforge::SearchHit> hits_;
    int activeHit_ = -1;
    int hoverSpan_ = -1;
};

}  // namespace pdfforge::ui
