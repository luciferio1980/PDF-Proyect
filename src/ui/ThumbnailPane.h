#pragma once

#include <QListWidget>

namespace pdfforge {
class PdfDocument;
}

namespace pdfforge::ui {

class ThumbnailPane : public QListWidget {
    Q_OBJECT
public:
    explicit ThumbnailPane(QWidget* parent = nullptr);
    void setDocument(pdfforge::PdfDocument* document);
    void setCurrentPage(int pageIndex);

signals:
    void pageActivated(int pageIndex);
};

}  // namespace pdfforge::ui
