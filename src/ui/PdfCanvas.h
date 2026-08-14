#pragma once

#include "model/Objects.h"
#include "pdf/PdfSearch.h"
#include "renderer/Bitmap.h"

#include <QImage>
#include <QWidget>

#include <optional>
#include <vector>

class QLineEdit;
class QWidget;

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
    void setSignWorkspace(bool enabled);
    void setPlaceStampMode(bool enabled);
    void setStampPreview(const QImage& image, float widthPt);
    void beginInlineEdit();
    void clearSelection();
    void clearSignatureSelection();
    void selectSignatureAt(const pdfforge::PointF& pagePoint);
    void setRegionSelection(const pdfforge::RectF& pageRect, const QString& text,
                            float fontSizePt = 12.0f);
    void clearRegionSelection();

    [[nodiscard]] int pageIndex() const { return pageIndex_; }
    [[nodiscard]] float zoom() const { return zoom_; }
    [[nodiscard]] int rotation() const { return rotation_; }
    [[nodiscard]] float dpi() const;
    [[nodiscard]] std::optional<pdfforge::TextSpan> selectedSpan() const;
    [[nodiscard]] std::optional<pdfforge::ImageObject> selectedSignature() const;
    [[nodiscard]] std::optional<pdfforge::RectF> selectedRegion() const;
    [[nodiscard]] bool addTextMode() const { return addTextMode_; }
    [[nodiscard]] bool placeStampMode() const { return placeStampMode_; }

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
    void regionSelected(const pdfforge::RectF& pageRect);
    void regionEditCommitted(const QString& text);
    void stampPlaced(const pdfforge::PointF& pagePoint);
    void signatureSelected(const pdfforge::ImageObject& image);
    void signatureSelectionCleared();
    void signatureResizeCommitted(const pdfforge::ImageObject& image, const pdfforge::RectF& pageRect);
    void signatureDeleteRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    enum class StampHandle { None, NW, NE, SW, SE };

    void requestRender();
    void applyBitmap(const pdfforge::Bitmap& bitmap);
    QPoint imageOffset() const;
    int hitSpanAt(const QPoint& widgetPos) const;
    int hitSignatureAt(const QPoint& widgetPos) const;
    StampHandle hitStampHandle(const QPoint& widgetPos) const;
    bool widgetToPage(const QPoint& widgetPos, pdfforge::PointF& page) const;
    bool widgetToPageClamped(const QPoint& widgetPos, pdfforge::PointF& page) const;
    QRect imageRect() const;
    QPolygonF spanPolygon(const pdfforge::RectF& bounds) const;
    QRectF signatureWidgetRect(const pdfforge::RectF& bounds) const;
    QRectF regionWidgetRect() const;
    bool hitsRegionMoveHandle(const QPoint& widgetPos) const;
    QRect regionEditorRect() const;
    int editorFontPixelSize() const;
    void fitRegionToFont();
    void startRegionMove(const QPoint& widgetPos);
    void stopRegionMove();
    void syncEditorGeometry();
    void clampRegionToPage();
    void syncRegionFrame();
    pdfforge::RectF resizedSignatureRect(StampHandle handle, const pdfforge::PointF& page) const;
    void finishInlineEdit(bool commit);
    void cancelInlineEdit();
    void loadSpans();
    void loadSignatures();

    pdfforge::PdfDocument* document_ = nullptr;
    int pageIndex_ = 0;
    float zoom_ = 1.0f;
    int rotation_ = 0;
    QImage image_;
    std::vector<pdfforge::TextSpan> spans_;
    std::vector<pdfforge::SearchHit> hits_;
    std::vector<pdfforge::ImageObject> signatures_;
    int activeHit_ = -1;
    int hoverSpan_ = -1;
    int selectedSpan_ = -1;
    int selectedSignature_ = -1;
    bool addTextMode_ = false;
    bool placeStampMode_ = false;
    bool signWorkspace_ = false;
    bool resizingStamp_ = false;
    StampHandle activeHandle_ = StampHandle::None;
    pdfforge::RectF liveStampRect_{};
    QImage stampPreview_;
    float stampWidthPt_ = 144.0f;
    QPoint lastMouse_;
    QPoint marqueeOrigin_;
    bool marqueeDrag_ = false;
    bool movingRegion_ = false;
    bool hasRegion_ = false;
    pdfforge::RectF regionRect_{};
    pdfforge::PointF moveLastPage_{};
    QString regionText_;
    float regionFontSize_ = 12.0f;
    QWidget* regionFrame_ = nullptr;
    QLineEdit* editor_ = nullptr;
    int editingSpan_ = -1;
    bool committing_ = false;
};

}  // namespace pdfforge::ui
