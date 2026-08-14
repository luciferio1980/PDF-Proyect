#pragma once

#include <QDialog>
#include <QImage>
#include <QPoint>
#include <QWidget>

class QMouseEvent;
class QPaintEvent;

namespace pdfforge::ui {

class SignatureInk : public QWidget {
    Q_OBJECT
public:
    explicit SignatureInk(QWidget* parent = nullptr);
    void clearInk();
    [[nodiscard]] QImage image() const { return image_; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void ensureImage();
    void strokeTo(const QPoint& pos);

    QImage image_;
    QPoint last_;
    bool drawing_ = false;
};

class SignaturePad : public QDialog {
    Q_OBJECT
public:
    explicit SignaturePad(QWidget* parent = nullptr);
    [[nodiscard]] QImage signatureImage() const;

private:
    SignatureInk* ink_ = nullptr;
};

}  // namespace pdfforge::ui
