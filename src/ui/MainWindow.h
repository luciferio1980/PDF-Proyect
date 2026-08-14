#pragma once

#include "pdf/PdfDocument.h"
#include "pdf/PdfiumRuntime.h"
#include "pdf/PdfSearch.h"

#include <QMainWindow>

#include <memory>
#include <vector>

class QSpinBox;
class QLabel;

namespace pdfforge::ui {

class PdfCanvas;
class ThumbnailPane;
class SearchPanel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

    void openPath(const QString& path);

private slots:
    void openFile();
    void saveCopy();
    void goToPage(int pageOneBased);
    void rotateClockwise();
    void rotateCounterClockwise();
    void runSearch(const QString& query);
    void jumpToHit(int hitIndex);

private:
    void buildUi();
    void buildMenus();
    void refreshPageUi();
    void showError(const pdfforge::Error& error);

    std::shared_ptr<pdfforge::PdfiumRuntime> runtime_;
    std::unique_ptr<pdfforge::PdfDocument> document_;
    PdfCanvas* canvas_ = nullptr;
    ThumbnailPane* thumbs_ = nullptr;
    SearchPanel* search_ = nullptr;
    QSpinBox* pageSpin_ = nullptr;
    QLabel* pageTotal_ = nullptr;
    QLabel* classLabel_ = nullptr;
    std::vector<pdfforge::SearchHit> hits_;
};

}  // namespace pdfforge::ui
