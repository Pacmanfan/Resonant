#pragma once

#include <QDialog>

#include <memory>

namespace Ui {
class AboutDialog;
}

class AboutDialog : public QDialog {
    Q_OBJECT
public:
    explicit AboutDialog(const QString& graphicsDescription, QWidget* parent = nullptr);
    ~AboutDialog() override;

private:
    std::unique_ptr<Ui::AboutDialog> ui_;
};
