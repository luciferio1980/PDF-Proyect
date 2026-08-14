#include "ui/SignatureLibrary.h"

#include <QAbstractItemView>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPixmap>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace pdfforge::ui {
namespace {

QDir signatureDir() {
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
             QStringLiteral("/signatures"));
    dir.mkpath(QStringLiteral("."));
    return dir;
}

QStringList signatureFiles() {
    QStringList files = signatureDir().entryList({QStringLiteral("*.png")}, QDir::Files, QDir::Name);
    files.sort();
    return files;
}

}  // namespace

SignatureLibrary::SignatureLibrary(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(8);

    auto* title = new QLabel(tr("Firmas guardadas"), this);
    title->setStyleSheet(QStringLiteral("font-weight: 600;"));
    auto* hint = new QLabel(
        tr("Máximo %1. Elimina una para guardar otra.\n"
           "Una firma por visita: para firmar otra vez, Inicio → Firmar PDF.")
            .arg(kMaxSaved),
        this);
    hint->setWordWrap(true);
    hint->setObjectName(QStringLiteral("homeHint"));

    list_ = new QListWidget(this);
    list_->setViewMode(QListView::ListMode);
    list_->setIconSize(QSize(132, 48));
    list_->setSpacing(6);
    list_->setSelectionMode(QAbstractItemView::SingleSelection);

    deleteBtn_ = new QPushButton(tr("Eliminar firma guardada"), this);

    root->addWidget(title);
    root->addWidget(hint);
    root->addWidget(list_, 1);
    root->addWidget(deleteBtn_);

    connect(list_, &QListWidget::itemClicked, this, &SignatureLibrary::onItemClicked);
    connect(deleteBtn_, &QPushButton::clicked, this, &SignatureLibrary::deleteSelected);
    reload();
}

int SignatureLibrary::count() const {
    return list_ ? list_->count() : 0;
}

bool SignatureLibrary::saveSignature(const QImage& image) {
    if (image.isNull()) {
        return false;
    }
    if (signatureFiles().size() >= kMaxSaved) {
        return false;
    }
    const QString name =
        QStringLiteral("firma_%1.png").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss_zzz")));
    const QString path = signatureDir().filePath(name);
    QImage png = image.convertToFormat(QImage::Format_ARGB32);
    if (!png.save(path, "PNG")) {
        return false;
    }
    reload();
    return true;
}

void SignatureLibrary::reload() {
    if (!list_) {
        return;
    }
    list_->clear();
    int n = 1;
    for (const QString& file : signatureFiles()) {
        const QString path = signatureDir().filePath(file);
        QImage img(path);
        if (img.isNull()) {
            continue;
        }
        auto* item = new QListWidgetItem(tr("Firma %1").arg(n++), list_);
        item->setIcon(QIcon(QPixmap::fromImage(img.scaled(132, 48, Qt::KeepAspectRatio,
                                                          Qt::SmoothTransformation))));
        item->setData(Qt::UserRole, path);
        item->setSizeHint(QSize(160, 56));
    }
    if (deleteBtn_) {
        deleteBtn_->setEnabled(list_->count() > 0);
    }
}

void SignatureLibrary::deleteSelected() {
    auto* item = list_ ? list_->currentItem() : nullptr;
    if (!item) {
        if (list_ && list_->count() > 0) {
            item = list_->item(list_->count() - 1);
        }
    }
    if (!item) {
        return;
    }
    const QString path = item->data(Qt::UserRole).toString();
    if (!path.isEmpty()) {
        QFile::remove(path);
    }
    reload();
}

void SignatureLibrary::onItemClicked() {
    auto* item = list_->currentItem();
    if (!item) {
        return;
    }
    const QString path = item->data(Qt::UserRole).toString();
    QImage img(path);
    if (!img.isNull()) {
        emit signatureChosen(img.convertToFormat(QImage::Format_ARGB32));
    }
}

}  // namespace pdfforge::ui
