#pragma once

#include "pdf/PdfDocument.h"
#include "pdf/PdfiumRuntime.h"
#include "pdf/PdfSearch.h"

#include <QMainWindow>

#include <filesystem>
#include <memory>
#include <vector>

class QSpinBox;
class QLabel;
class QAction;
class QCloseEvent;
class QColor;

namespace pdfforge::ui {

class PdfCanvas;
class ThumbnailPane;
class SearchPanel;
class InspectorPanel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

    void openPath(const QString& path);

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

private:
    void buildUi();
    void buildMenus();
    void refreshPageUi();
    void refreshAfterMutation(bool rebuildThumbs);
    void showError(const pdfforge::Error& error);
    void updateTitle();
    bool confirmDiscard();
    std::filesystem::path toFsPath(const QString& path) const;

    std::shared_ptr<pdfforge::PdfiumRuntime> runtime_;
    std::unique_ptr<pdfforge::PdfDocument> document_;
    PdfCanvas* canvas_ = nullptr;
    ThumbnailPane* thumbs_ = nullptr;
    SearchPanel* search_ = nullptr;
    InspectorPanel* inspector_ = nullptr;
    QSpinBox* pageSpin_ = nullptr;
    QLabel* pageTotal_ = nullptr;
    QLabel* classLabel_ = nullptr;
    QAction* addTextAction_ = nullptr;
    QAction* undoAction_ = nullptr;
    std::vector<pdfforge::SearchHit> hits_;
};

}  // namespace pdfforge::ui
