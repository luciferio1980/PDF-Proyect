#include "ui/SignaturePad.h"

#include "ui/Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QResizeEvent>
#include <QVBoxLayout>

namespace pdfforge::ui {

SignatureInk::SignatureInk(QWidget* parent) : QWidget(parent) {
    setMinimumSize(640, 200);
    setCursor(Qt::CrossCursor);
    setAutoFillBackground(false);
}

void SignatureInk::clearInk() {
    if (!image_.isNull()) {
        image_.fill(Qt::transparent);
    }
    drawing_ = false;
    update();
}

void SignatureInk::ensureImage() {
    if (image_.width() == width() && image_.height() == height()) {
        return;
    }
    QImage next(size(), QImage::Format_ARGB32);
    next.fill(Qt::transparent);
    if (!image_.isNull()) {
        QPainter p(&next);
        p.drawImage(0, 0, image_);
    }
    image_ = next;
}

void SignatureInk::strokeTo(const QPoint& pos) {
    ensureImage();
    QPainter p(&image_);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(theme().ink, 3.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.drawLine(last_, pos);
    last_ = pos;
    update();
}

void SignatureInk::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), theme().paper);
    p.setPen(QPen(theme().copper, 1));
    p.drawRect(rect().adjusted(0, 0, -1, -1));
    if (!image_.isNull()) {
        p.drawImage(0, 0, image_);
    }
}

void SignatureInk::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        return;
    }
    ensureImage();
    drawing_ = true;
    last_ = event->pos();
    strokeTo(event->pos());
}

void SignatureInk::mouseMoveEvent(QMouseEvent* event) {
    if (drawing_) {
        strokeTo(event->pos());
    }
}

void SignatureInk::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        drawing_ = false;
    }
}

void SignatureInk::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    ensureImage();
}

SignaturePad::SignaturePad(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Dibujar firma"));
    setModal(true);
    auto* root = new QVBoxLayout(this);
    root->addWidget(new QLabel(tr("Dibuja tu firma con el ratón. El fondo queda transparente."), this));
    ink_ = new SignatureInk(this);
    ink_->setMinimumSize(720, 240);
    root->addWidget(ink_, 1);
    auto* buttons = new QHBoxLayout();
    auto* clearBtn = new QPushButton(tr("Borrar"), this);
    auto* cancelBtn = new QPushButton(tr("Cancelar"), this);
    auto* okBtn = new QPushButton(tr("Usar esta firma"), this);
    buttons->addWidget(clearBtn);
    buttons->addStretch(1);
    buttons->addWidget(cancelBtn);
    buttons->addWidget(okBtn);
    root->addLayout(buttons);
    connect(clearBtn, &QPushButton::clicked, ink_, &SignatureInk::clearInk);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    resize(780, 380);
}

QImage SignaturePad::signatureImage() const {
    return ink_ ? ink_->image() : QImage();
}

}  // namespace pdfforge::ui
