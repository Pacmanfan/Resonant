// The presenter bar (RES-DEMO-007): in presenter mode it is the only panel
// besides the viewport: the demonstration's title, the step and its
// progress, the step's instruction, the presenter notes (hidden unless asked
// for), Pause / Resume, Next, Restart and Exit. Interface: forms/PresenterOverlay.ui.
#pragma once

#include <QWidget>

#include <memory>

namespace Ui {
class PresenterOverlay;
}

class PresenterOverlay : public QWidget {
    Q_OBJECT
public:
    explicit PresenterOverlay(QWidget* parent = nullptr);
    ~PresenterOverlay() override;

    void setTitle(const QString& title);
    void setStep(int index, int count, const QString& instruction, const QString& notes);
    void setWaiting(bool waiting);   // Next is the way on
    void setPaused(bool paused);
    void setFinished(const QString& status, const QString& message);
    QString instructionText() const;
    QString stepText() const;
    bool notesVisible() const;

signals:
    void nextRequested();
    void pauseToggled(bool pause);
    void restartRequested();
    void exitRequested();

private:
    std::unique_ptr<Ui::PresenterOverlay> ui_;
    bool paused_ = false;
};
