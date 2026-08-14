#include "ui/SearchPanel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

namespace pdfforge::ui {

SearchPanel::SearchPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 6, 8, 6);
    edit_ = new QLineEdit(this);
    edit_->setPlaceholderText(tr("Find in document"));
    auto* prev = new QPushButton(tr("Prev"), this);
    auto* next = new QPushButton(tr("Next"), this);
    label_ = new QLabel(this);
    layout->addWidget(edit_, 1);
    layout->addWidget(prev);
    layout->addWidget(next);
    layout->addWidget(label_);
    connect(edit_, &QLineEdit::returnPressed, this, [this]() { emit querySubmitted(edit_->text()); });
    connect(next, &QPushButton::clicked, this, &SearchPanel::nextHit);
    connect(prev, &QPushButton::clicked, this, &SearchPanel::previousHit);
    updateLabel();
}

void SearchPanel::setHits(const std::vector<pdfforge::SearchHit>& hits) {
    hits_ = hits;
    active_ = hits_.empty() ? -1 : 0;
    updateLabel();
    if (active_ >= 0) {
        emit activeHitChanged(active_);
    }
}

QString SearchPanel::query() const {
    return edit_->text();
}

void SearchPanel::focusQuery() {
    edit_->setFocus();
    edit_->selectAll();
}

void SearchPanel::nextHit() {
    if (hits_.empty()) {
        return;
    }
    active_ = (active_ + 1) % static_cast<int>(hits_.size());
    updateLabel();
    emit activeHitChanged(active_);
}

void SearchPanel::previousHit() {
    if (hits_.empty()) {
        return;
    }
    active_ = (active_ - 1 + static_cast<int>(hits_.size())) % static_cast<int>(hits_.size());
    updateLabel();
    emit activeHitChanged(active_);
}

void SearchPanel::updateLabel() {
    if (hits_.empty()) {
        label_->setText(tr("0 matches"));
    } else {
        label_->setText(tr("%1 / %2").arg(active_ + 1).arg(hits_.size()));
    }
}

}  // namespace pdfforge::ui
