// Demo Studio (RES-DEMO-*): the demonstrations, each with its story (the
// problem, what is configured and calculated, what the result means, how to
// inspect it, why it matters, its assumptions), status, duration and steps;
// Run (optionally in presenter mode), Next, Pause, Stop, Restart; the steps'
// outcomes; the run manifest. Interface: forms/DemoStudioPanel.ui.
#pragma once

#include <QWidget>

#include <functional>
#include <memory>

class DemoController;
namespace Ui {
class DemoStudioPanel;
}

class DemoStudioPanel : public QWidget {
    Q_OBJECT
public:
    explicit DemoStudioPanel(QWidget* parent = nullptr);
    ~DemoStudioPanel() override;

    void setController(DemoController* controller);
    // Asked before a demonstration replaces the open project (unsaved changes).
    void setConfirmation(std::function<bool()> confirm) { confirm_ = std::move(confirm); }
    void setPresenterSwitch(std::function<void(bool)> presenter) { presenter_ = std::move(presenter); }

    void refresh();
    QStringList profileIds() const;
    bool selectProfile(const QString& id);
    QString selectedProfile() const;
    QString profileText() const;
    int stepRows() const;
    QString stepText(int row) const;
    QString statusText() const;
    bool run(bool attended, bool presenter);

private:
    void showProfile();
    void updateButtons();
    void markStep(int index, const QString& state);

    std::unique_ptr<Ui::DemoStudioPanel> ui_;
    DemoController* controller_ = nullptr;
    std::function<bool()> confirm_;
    std::function<void(bool)> presenter_;
};
