#include "OutputAuthorizationDialog.h"

#include "ui_OutputAuthorizationDialog.h"

#include <QPushButton>

using namespace resonant::experiments;

OutputAuthorizationDialog::OutputAuthorizationDialog(const OutputRequest& request, const SafetyCheck& check, QWidget* parent)
    : QDialog(parent), ui_(std::make_unique<Ui::OutputAuthorizationDialog>()), allowed_(check.allowed)
{
    ui_->setupUi(this);
    setWindowTitle(tr("Authorize Physical Output"));
    QStringList lines;
    lines << tr("Experiment: %1").arg(QString::fromStdString(request.experimentId));
    lines << tr("Domain: %1").arg(QString::fromUtf8(resonant::toString(request.domain).data()));
    QStringList channels;
    for (const auto& c : request.channels)
        channels << QString::fromStdString(c);
    lines << tr("Output channels: %1").arg(channels.join(QStringLiteral(", ")));
    if (request.peakAmplitude > 0.0)
        lines << tr("Peak command amplitude: %1").arg(request.peakAmplitude);
    if (request.transmitPowerDbm > -1000.0)
        lines << tr("Transmit power: %1 dBm").arg(request.transmitPowerDbm);
    lines << tr("Frequencies: %1 ... %2 Hz").arg(request.minFrequencyHz).arg(request.maxFrequencyHz);
    lines << tr("Duty cycle: %1, total output time: %2 s").arg(request.dutyCycle).arg(request.durationSeconds);
    ui_->requestText->setPlainText(lines.join(QStringLiteral("\n")));
    QStringList warnings;
    for (const auto& v : check.violations)
        warnings << tr("NOT PERMITTED: %1").arg(QString::fromStdString(v));
    for (const auto& w : check.warnings)
        warnings << QString::fromStdString(w);
    ui_->warningsText->setPlainText(warnings.isEmpty() ? tr("No warnings.") : warnings.join(QStringLiteral("\n\n")));
    connect(ui_->operatorEdit, &QLineEdit::textChanged, this, &OutputAuthorizationDialog::updateButtons);
    connect(ui_->acknowledgeCheck, &QCheckBox::toggled, this, &OutputAuthorizationDialog::updateButtons);
    updateButtons();
}

OutputAuthorizationDialog::~OutputAuthorizationDialog() = default;

QString OutputAuthorizationDialog::operatorName() const { return ui_->operatorEdit->text().trimmed(); }

QString OutputAuthorizationDialog::acknowledgement() const
{
    return ui_->acknowledgeCheck->isChecked() ? ui_->acknowledgeCheck->text() : QString();
}

void OutputAuthorizationDialog::fill(const QString& name, bool acknowledged)
{
    ui_->operatorEdit->setText(name);
    ui_->acknowledgeCheck->setChecked(acknowledged);
}

bool OutputAuthorizationDialog::canAccept() const { return allowed_ && !operatorName().isEmpty() && ui_->acknowledgeCheck->isChecked(); }

void OutputAuthorizationDialog::updateButtons()
{
    if (QPushButton* ok = ui_->buttonBox->button(QDialogButtonBox::Ok)) {
        ok->setText(tr("Authorize"));
        ok->setEnabled(canAccept());
    }
}
