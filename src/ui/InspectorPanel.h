#pragma once

#include "model/Objects.h"
#include "model/PageClassification.h"

#include <QWidget>

class QLabel;
class QLineEdit;
class QDoubleSpinBox;
class QPushButton;

namespace pdfforge::ui {

class InspectorPanel : public QWidget {
    Q_OBJECT
public:
    explicit InspectorPanel(QWidget* parent = nullptr);

    void setSpan(const pdfforge::TextSpan& span);
    void clearSpan();
    void setClassification(const pdfforge::PageClassification& classification);
    void setOcrAvailable(bool available);

signals:
    void applyRequested(const QString& text, float fontSize, QColor color);
    void deleteRequested();
    void ocrRequested();
    void colorPicked(const QColor& color);

private:
    void pickColor();

    QLabel* fontLabel_ = nullptr;
    QLabel* classLabel_ = nullptr;
    QLabel* hintLabel_ = nullptr;
    QLineEdit* textEdit_ = nullptr;
    QDoubleSpinBox* sizeSpin_ = nullptr;
    QPushButton* colorBtn_ = nullptr;
    QPushButton* applyBtn_ = nullptr;
    QPushButton* deleteBtn_ = nullptr;
    QPushButton* ocrBtn_ = nullptr;
    QColor color_{0, 0, 0};
    bool hasSpan_ = false;
};

}  // namespace pdfforge::ui
