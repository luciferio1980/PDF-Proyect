#pragma once

#include <QWidget>

class QPushButton;

namespace pdfforge::ui {

class HomeView : public QWidget {
    Q_OBJECT
public:
    explicit HomeView(QWidget* parent = nullptr);

signals:
    void editPdfRequested();
    void signPdfRequested();
};

}  // namespace pdfforge::ui
