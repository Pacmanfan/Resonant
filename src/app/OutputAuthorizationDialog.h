#pragma once

// The explicit operator act that authorizes one run's physical output:
// shows what the experiment would output and the safety warnings, and
// accepts only with the operator's name and the acknowledgement ticked.
// Laid out in forms/OutputAuthorizationDialog.ui.
#include "experiments/Safety.h"

#include <QDialog>

#include <memory>

namespace Ui {
class OutputAuthorizationDialog;
}

class OutputAuthorizationDialog : public QDialog {
    Q_OBJECT
public:
    OutputAuthorizationDialog(const resonant::experiments::OutputRequest& request, const resonant::experiments::SafetyCheck& check,
                              QWidget* parent = nullptr);
    ~OutputAuthorizationDialog() override;

    QString operatorName() const;
    QString acknowledgement() const;
    // For tests: fill in and accept.
    void fill(const QString& operatorName, bool acknowledged);
    bool canAccept() const;

private:
    void updateButtons();
    std::unique_ptr<Ui::OutputAuthorizationDialog> ui_;
    bool allowed_ = false;
};
