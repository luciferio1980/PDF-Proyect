#include "ui/InspectorPanel.h"

#include "ui/Theme.h"

#include <QColorDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace pdfforge::ui {
namespace {

QString colorCss(const QColor& c) {
    return QStringLiteral("background: %1; border: 1px solid #3A414A; min-width: 36px;")
        .arg(c.name());
}

}  // namespace

InspectorPanel::InspectorPanel(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(10);

    auto* pageBox = new QGroupBox(tr("Page"), this);
    auto* pageLayout = new QVBoxLayout(pageBox);
    classLabel_ = new QLabel(tr("Open a PDF to inspect objects."), pageBox);
    classLabel_->setWordWrap(true);
    ocrBtn_ = new QPushButton(tr("OCR this page"), pageBox);
    ocrBtn_->setToolTip(tr("Run local OCR and insert an invisible searchable text layer."));
    pageLayout->addWidget(classLabel_);
    pageLayout->addWidget(ocrBtn_);

    auto* textBox = new QGroupBox(tr("Text object"), this);
    auto* form = new QFormLayout(textBox);
    fontLabel_ = new QLabel(tr("—"), textBox);
    fontLabel_->setWordWrap(true);
    textEdit_ = new QLineEdit(textBox);
    textEdit_->setPlaceholderText(tr("Select text on the page"));
    sizeSpin_ = new QDoubleSpinBox(textBox);
    sizeSpin_->setRange(1.0, 200.0);
    sizeSpin_->setDecimals(1);
    sizeSpin_->setSingleStep(0.5);
    sizeSpin_->setSuffix(QStringLiteral(" pt"));
    colorBtn_ = new QPushButton(textBox);
    colorBtn_->setFixedHeight(26);
    colorBtn_->setStyleSheet(colorCss(color_));
    applyBtn_ = new QPushButton(tr("Apply to PDF"), textBox);
    deleteBtn_ = new QPushButton(tr("Delete object"), textBox);
    hintLabel_ = new QLabel(
        tr("Edits rewrite the PDF text object. This is not a white box overlay."), textBox);
    hintLabel_->setWordWrap(true);
    hintLabel_->setStyleSheet(QStringLiteral("color: #9A9488; font-size: 11px;"));
    form->addRow(tr("Font"), fontLabel_);
    form->addRow(tr("Size"), sizeSpin_);
    form->addRow(tr("Color"), colorBtn_);
    form->addRow(tr("Text"), textEdit_);
    form->addRow(applyBtn_);
    form->addRow(deleteBtn_);
    form->addRow(hintLabel_);

    root->addWidget(pageBox);
    root->addWidget(textBox);
    root->addStretch(1);

    connect(colorBtn_, &QPushButton::clicked, this, &InspectorPanel::pickColor);
    connect(applyBtn_, &QPushButton::clicked, this, [this]() {
        if (hasSpan_) {
            emit applyRequested(textEdit_->text(), static_cast<float>(sizeSpin_->value()), color_);
        }
    });
    connect(textEdit_, &QLineEdit::returnPressed, applyBtn_, &QPushButton::click);
    connect(deleteBtn_, &QPushButton::clicked, this, [this]() {
        if (hasSpan_) {
            emit deleteRequested();
        }
    });
    connect(ocrBtn_, &QPushButton::clicked, this, &InspectorPanel::ocrRequested);

    clearSpan();
}

void InspectorPanel::setSpan(const pdfforge::TextSpan& span) {
    hasSpan_ = true;
    const auto rgb = span.color.toRgb();
    color_ = QColor::fromRgbF(rgb.c0, rgb.c1, rgb.c2, rgb.alpha);
    fontLabel_->setText(span.fontName.empty() ? tr("(unnamed)") : QString::fromStdString(span.fontName));
    sizeSpin_->setValue(span.fontSize > 0 ? span.fontSize : 12.0);
    textEdit_->setText(QString::fromStdString(span.text));
    colorBtn_->setStyleSheet(colorCss(color_));
    applyBtn_->setEnabled(true);
    deleteBtn_->setEnabled(true);
    textEdit_->setEnabled(true);
    sizeSpin_->setEnabled(true);
    colorBtn_->setEnabled(true);
}

void InspectorPanel::clearSpan() {
    hasSpan_ = false;
    fontLabel_->setText(tr("—"));
    textEdit_->clear();
    sizeSpin_->setValue(12.0);
    applyBtn_->setEnabled(false);
    deleteBtn_->setEnabled(false);
    textEdit_->setEnabled(false);
    sizeSpin_->setEnabled(false);
    colorBtn_->setEnabled(false);
}

void InspectorPanel::setClassification(const pdfforge::PageClassification& classification) {
    const char* kind = pdfforge::pageContentKindName(classification.kind);
    QString ocr = (classification.ocr == pdfforge::OcrNeed::Required) ? tr("OCR recommended")
                                                                      : tr("text layer present");
    classLabel_->setText(tr("%1 · %2\n%3 chars · %4 images")
                             .arg(QString::fromUtf8(kind))
                             .arg(ocr)
                             .arg(classification.textCharCount)
                             .arg(classification.imageCount));
}

void InspectorPanel::setOcrAvailable(bool available) {
    ocrBtn_->setEnabled(available);
    if (!available) {
        ocrBtn_->setToolTip(tr("OCR is not in this build (Windows portable ships without Tesseract)."));
    }
}

void InspectorPanel::pickColor() {
    const QColor chosen = QColorDialog::getColor(color_, this, tr("Text color"));
    if (!chosen.isValid()) {
        return;
    }
    color_ = chosen;
    colorBtn_->setStyleSheet(colorCss(color_));
    emit colorPicked(color_);
}

}  // namespace pdfforge::ui
