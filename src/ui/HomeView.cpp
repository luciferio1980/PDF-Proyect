#include "ui/HomeView.h"

#include "core/Version.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace pdfforge::ui {

HomeView::HomeView(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(48, 48, 48, 48);
    root->setSpacing(18);
    root->addStretch(1);

    auto* title = new QLabel(QString::fromUtf8(pdfforge::kProductName.data(),
                                               static_cast<int>(pdfforge::kProductName.size())),
                             this);
    title->setObjectName(QStringLiteral("homeTitle"));
    title->setAlignment(Qt::AlignHCenter);

    auto* subtitle = new QLabel(tr("¿Qué quieres hacer?"), this);
    subtitle->setObjectName(QStringLiteral("homeSubtitle"));
    subtitle->setAlignment(Qt::AlignHCenter);

    auto* hint = new QLabel(tr("Elige una herramienta. Luego abre el PDF con el que quieres trabajar."),
                            this);
    hint->setObjectName(QStringLiteral("homeHint"));
    hint->setAlignment(Qt::AlignHCenter);
    hint->setWordWrap(true);

    auto* editBtn = new QPushButton(tr("Editar PDF\nCambiar texto, páginas y objetos"), this);
    editBtn->setObjectName(QStringLiteral("homeCard"));
    editBtn->setCursor(Qt::PointingHandCursor);
    editBtn->setMinimumHeight(110);

    auto* signBtn = new QPushButton(tr("Firmar PDF\nSube una imagen o dibuja tu firma"), this);
    signBtn->setObjectName(QStringLiteral("homeCard"));
    signBtn->setCursor(Qt::PointingHandCursor);
    signBtn->setMinimumHeight(110);

    root->addWidget(title);
    root->addWidget(subtitle);
    root->addWidget(hint);
    root->addSpacing(12);
    root->addWidget(editBtn, 0, Qt::AlignHCenter);
    root->addWidget(signBtn, 0, Qt::AlignHCenter);
    root->addStretch(2);

    editBtn->setFixedWidth(420);
    signBtn->setFixedWidth(420);

    connect(editBtn, &QPushButton::clicked, this, &HomeView::editPdfRequested);
    connect(signBtn, &QPushButton::clicked, this, &HomeView::signPdfRequested);
}

}  // namespace pdfforge::ui
