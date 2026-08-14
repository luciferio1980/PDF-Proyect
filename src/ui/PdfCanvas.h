#pragma once

#include "model/Objects.h"
#include "pdf/PdfSearch.h"
#include "renderer/Bitmap.h"

#include <QImage>
#include <QWidget>

#include <optional>
#include <vector>

class QLineEdit;

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
    void reload();
    void setAddTextMode(bool enabled);
    void beginInlineEdit();
    void clearSelection();

    [[nodiscard]] int pageIndex() const { return pageIndex_; }
    [[nodiscard]] float zoom() const { return zoom_; }
    [[nodiscard]] int rotation() const { return rotation_; }
    [[nodiscard]] float dpi() const;
    [[nodiscard]] std::optional<pdfforge::TextSpan> selectedSpan() const;
    [[nodiscard]] bool addTextMode() const { return addTextMode_; }

public slots:
    void zoomIn();
    void zoomOut();
    void resetZoom();

signals:
    void hoverSpanChanged(const QString& preview);
    void statusMessage(const QString& text);
    void spanSelected(const pdfforge::TextSpan& span);
    void selectionCleared();
    void spanEditCommitted(const pdfforge::TextSpan& span, const QString& text);
    void emptyPageClicked(const pdfforge::PointF& pagePoint);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void requestRender();
    void applyBitmap(const pdfforge::Bitmap& bitmap);
    QPoint imageOffset() const;
    int hitSpanAt(const QPoint& widgetPos) const;
    bool widgetToPage(const QPoint& widgetPos, pdfforge::PointF& page) const;
    QPolygonF spanPolygon(const pdfforge::RectF& bounds) const;
    void finishInlineEdit(bool commit);
    void cancelInlineEdit();
    void loadSpans();

    pdfforge::PdfDocument* document_ = nullptr;
    int pageIndex_ = 0;
    float zoom_ = 1.0f;
    int rotation_ = 0;
    QImage image_;
    std::vector<pdfforge::TextSpan> spans_;
    std::vector<pdfforge::SearchHit> hits_;
    int activeHit_ = -1;
    int hoverSpan_ = -1;
    int selectedSpan_ = -1;
    bool addTextMode_ = false;
    QLineEdit* editor_ = nullptr;
    int editingSpan_ = -1;
    bool committing_ = false;
};

}  // namespace pdfforge::ui
