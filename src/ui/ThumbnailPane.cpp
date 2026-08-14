#include "ui/ThumbnailPane.h"

#include "pdf/PdfDocument.h"

#include <QIcon>
#include <QImage>
#include <QListWidgetItem>
#include <QPixmap>

namespace pdfforge::ui {
namespace {

QImage toThumb(const pdfforge::Bitmap& bitmap, int maxWidth) {
    if (bitmap.empty()) {
        return {};
    }
    QImage img(bitmap.bgra.data(), bitmap.width, bitmap.height, bitmap.stride,
               QImage::Format_ARGB32);
    img = img.copy();
    return img.scaledToWidth(maxWidth, Qt::SmoothTransformation);
}

}  // namespace

ThumbnailPane::ThumbnailPane(QWidget* parent) : QListWidget(parent) {
    setViewMode(QListView::IconMode);
    setIconSize(QSize(96, 128));
    setResizeMode(QListView::Adjust);
    setMovement(QListView::Static);
    setSpacing(8);
    setUniformItemSizes(true);
    setWordWrap(true);
    connect(this, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0) {
            emit pageActivated(row);
        }
    });
}

void ThumbnailPane::setDocument(pdfforge::PdfDocument* document) {
    document_ = document;
    clear();
    if (!document) {
        return;
    }
    for (int i = 0; i < document->pageCount(); ++i) {
        auto* item = new QListWidgetItem(tr("Page %1").arg(i + 1), this);
        try {
            pdfforge::RenderRequest req;
            req.pageIndex = i;
            req.dpi = 36.0f;
            const auto bmp = document->render(req);
            item->setIcon(QIcon(QPixmap::fromImage(toThumb(bmp, 96))));
        } catch (const pdfforge::Error&) {
            item->setIcon(QIcon());
        }
        item->setSizeHint(QSize(112, 160));
    }
    if (count() > 0) {
        setCurrentRow(0);
    }
}

void ThumbnailPane::setCurrentPage(int pageIndex) {
    if (pageIndex >= 0 && pageIndex < count()) {
        blockSignals(true);
        setCurrentRow(pageIndex);
        blockSignals(false);
    }
}

void ThumbnailPane::refreshPage(int pageIndex) {
    if (!document_ || pageIndex < 0 || pageIndex >= count()) {
        return;
    }
    auto* item = this->item(pageIndex);
    if (!item) {
        return;
    }
    try {
        pdfforge::RenderRequest req;
        req.pageIndex = pageIndex;
        req.dpi = 36.0f;
        const auto bmp = document_->render(req);
        item->setIcon(QIcon(QPixmap::fromImage(toThumb(bmp, 96))));
    } catch (const pdfforge::Error&) {
        item->setIcon(QIcon());
    }
}

}  // namespace pdfforge::ui
