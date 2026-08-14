#pragma once

#include "pdf/PdfDocument.h"
#include "pdf/PdfiumRuntime.h"
#include "pdf/PdfSearch.h"

#include <QImage>
#include <QMainWindow>

#include <filesystem>
#include <memory>
#include <vector>

class QSpinBox;
class QLabel;
class QAction;
class QCloseEvent;
class QColor;
class QStackedWidget;
class QToolBar;
class QDockWidget;
class QDoubleSpinBox;

namespace pdfforge::ui {

class PdfCanvas;
class ThumbnailPane;
class SearchPanel;
class InspectorPanel;
class HomeView;
class SignatureLibrary;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

    void openPath(const QString& path);
    void openPathForEdit(const QString& path);
    void openPathForSign(const QString& path);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void openFile();
    void saveDocument();
    void saveAs();
    void saveCopy();
    void undoEdit();
    void goToPage(int pageOneBased);
    void rotatePageClockwise();
    void rotatePageCounterClockwise();
    void deleteCurrentPage();
    void insertBlankPage();
    void insertPdf();
    void toggleAddText(bool on);
    void runSearch(const QString& query);
    void jumpToHit(int hitIndex);
    void applySpanEdits(const QString& text, float fontSize, const QColor& color);
    void deleteSelectedSpan();
    void commitInlineEdit(const pdfforge::TextSpan& span, const QString& text);
    void addTextAt(const pdfforge::PointF& pagePoint);
    void runOcr();
    void showHome();
    void startEditPdf();
    void startSignPdf();
    void loadSignatureImage();
    void drawSignature();
    void placeSignature(const pdfforge::PointF& pagePoint);
    void useSavedSignature(const QImage& image);
    void saveCurrentSignature();
    void deleteSelectedSignature();
    void onSignatureSelected(const pdfforge::ImageObject& image);
    void onSignatureCleared();
    void onSignatureResized(const pdfforge::ImageObject& image, const pdfforge::RectF& pageRect);

private:
    enum class Workspace { Home, Edit, Sign };

    void buildUi();
    void buildMenus();
    void refreshPageUi();
    void refreshAfterMutation(bool rebuildThumbs);
    void showError(const pdfforge::Error& error);
    void updateTitle();
    bool confirmDiscard();
    void setWorkspace(Workspace workspace);
    void applyStampPreview();
    std::filesystem::path toFsPath(const QString& path) const;

    std::shared_ptr<pdfforge::PdfiumRuntime> runtime_;
    std::unique_ptr<pdfforge::PdfDocument> document_;
    Workspace workspace_ = Workspace::Home;
    QStackedWidget* stack_ = nullptr;
    HomeView* home_ = nullptr;
    QWidget* workPage_ = nullptr;
    PdfCanvas* canvas_ = nullptr;
    ThumbnailPane* thumbs_ = nullptr;
    SearchPanel* search_ = nullptr;
    InspectorPanel* inspector_ = nullptr;
    QDockWidget* pagesDock_ = nullptr;
    QDockWidget* inspectorDock_ = nullptr;
    QDockWidget* firmasDock_ = nullptr;
    QToolBar* navBar_ = nullptr;
    QToolBar* editBar_ = nullptr;
    QToolBar* signBar_ = nullptr;
    SignatureLibrary* library_ = nullptr;
    QSpinBox* pageSpin_ = nullptr;
    QLabel* pageTotal_ = nullptr;
    QLabel* classLabel_ = nullptr;
    QDoubleSpinBox* stampWidthSpin_ = nullptr;
    QAction* addTextAction_ = nullptr;
    QAction* undoAction_ = nullptr;
    QAction* deleteStampAction_ = nullptr;
    QImage stampImage_;
    bool placedThisVisit_ = false;
    bool ignoreStampWidth_ = false;
    std::vector<pdfforge::SearchHit> hits_;
};

}  // namespace pdfforge::ui
