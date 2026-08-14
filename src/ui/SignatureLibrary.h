#pragma once

#include <QImage>
#include <QWidget>

class QListWidget;
class QPushButton;

namespace pdfforge::ui {

class SignatureLibrary : public QWidget {
    Q_OBJECT
public:
    static constexpr int kMaxSaved = 8;

    explicit SignatureLibrary(QWidget* parent = nullptr);

    [[nodiscard]] int count() const;
    bool saveSignature(const QImage& image);
    void reload();

signals:
    void signatureChosen(const QImage& image);

private slots:
    void deleteSelected();
    void onItemClicked();

private:
    QListWidget* list_ = nullptr;
    QPushButton* deleteBtn_ = nullptr;
};

}  // namespace pdfforge::ui
