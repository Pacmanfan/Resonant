#include "PresenterOverlay.h"

#include "ui_PresenterOverlay.h"

#include <QShortcut>

PresenterOverlay::PresenterOverlay(QWidget* parent) : QWidget(parent), ui_(std::make_unique<Ui::PresenterOverlay>())
{
    ui_->setupUi(this);
    QFont title = ui_->titleLabel->font();
    title.setBold(true);
    title.setPointSizeF(title.pointSizeF() * 1.3);
    ui_->titleLabel->setFont(title);
    QFont instruction = ui_->instructionLabel->font();
    instruction.setPointSizeF(instruction.pointSizeF() * 1.25);
    ui_->instructionLabel->setFont(instruction);
    ui_->notesLabel->hide();
    connect(ui_->nextButton, &QPushButton::clicked, this, &PresenterOverlay::nextRequested);
    connect(ui_->restartButton, &QPushButton::clicked, this, &PresenterOverlay::restartRequested);
    connect(ui_->exitButton, &QPushButton::clicked, this, &PresenterOverlay::exitRequested);
    connect(ui_->pauseButton, &QPushButton::clicked, this, [this] { emit pauseToggled(!paused_); });
    connect(ui_->notesButton, &QPushButton::toggled, ui_->notesLabel, &QLabel::setVisible);
    for (const QKeySequence& key : {QKeySequence(Qt::Key_PageDown), QKeySequence(Qt::Key_Space)}) {
        auto* s = new QShortcut(key, this, [this] { emit nextRequested(); });
        s->setContext(Qt::WindowShortcut);
    }
    auto* escape = new QShortcut(QKeySequence(Qt::Key_Escape), this, [this] { emit exitRequested(); });
    escape->setContext(Qt::WindowShortcut);
}

PresenterOverlay::~PresenterOverlay() = default;

void PresenterOverlay::setTitle(const QString& title)
{
    ui_->titleLabel->setText(title);
}

void PresenterOverlay::setStep(int index, int count, const QString& instruction, const QString& notes)
{
    ui_->stepLabel->setText(tr("Step %1 of %2").arg(index + 1).arg(count));
    ui_->progressBar->setRange(0, std::max(1, count));
    ui_->progressBar->setValue(index + 1);
    ui_->instructionLabel->setText(instruction);
    ui_->notesLabel->setText(notes.isEmpty() ? tr("(no presenter notes for this step)") : tr("Notes: %1").arg(notes));
    setWaiting(false);
}

void PresenterOverlay::setWaiting(bool waiting)
{
    ui_->nextButton->setDefault(waiting);
    ui_->nextButton->setText(waiting ? tr("Next ▶") : tr("Next"));
}

void PresenterOverlay::setPaused(bool paused)
{
    paused_ = paused;
    ui_->pauseButton->setText(paused ? tr("Resume") : tr("Pause"));
}

void PresenterOverlay::setFinished(const QString& status, const QString& message)
{
    ui_->stepLabel->setText(tr("Finished: %1").arg(status));
    ui_->instructionLabel->setText(message.isEmpty() ? tr("The demonstration is complete.") : message);
    ui_->progressBar->setValue(ui_->progressBar->maximum());
    setWaiting(false);
}

QString PresenterOverlay::instructionText() const
{
    return ui_->instructionLabel->text();
}

QString PresenterOverlay::stepText() const
{
    return ui_->stepLabel->text();
}

bool PresenterOverlay::notesVisible() const
{
    return ui_->notesLabel->isVisible();
}
