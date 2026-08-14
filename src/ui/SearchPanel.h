#pragma once

#include "pdf/PdfSearch.h"

#include <QWidget>

#include <vector>

class QLineEdit;
class QLabel;

namespace pdfforge::ui {

class SearchPanel : public QWidget {
    Q_OBJECT
public:
    explicit SearchPanel(QWidget* parent = nullptr);

    void setHits(const std::vector<pdfforge::SearchHit>& hits);
    [[nodiscard]] QString query() const;
    [[nodiscard]] int activeIndex() const { return active_; }

public slots:
    void focusQuery();
    void nextHit();
    void previousHit();

signals:
    void querySubmitted(const QString& text);
    void activeHitChanged(int index);

private:
    void updateLabel();

    QLineEdit* edit_ = nullptr;
    QLabel* label_ = nullptr;
    std::vector<pdfforge::SearchHit> hits_;
    int active_ = -1;
};

}  // namespace pdfforge::ui
