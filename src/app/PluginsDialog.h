// The Plugins dialog (RES-PLUG-*): every plugin found, its state and why,
// enabling and disabling (persisted; loading happens only on Reload), and the
// manifest's details: what it provides, requires and declares it does.
// Interface: forms/PluginsDialog.ui.
#pragma once

#include <QDialog>

#include <functional>
#include <memory>

namespace resonant::plugins {
class PluginManager;
}
namespace Ui {
class PluginsDialog;
}

class PluginsDialog : public QDialog {
    Q_OBJECT
public:
    PluginsDialog(resonant::plugins::PluginManager& manager, std::function<void()> reload, QWidget* parent = nullptr);
    ~PluginsDialog() override;

    void refresh();
    int rowCount() const;
    QString cell(int row, int column) const;
    int rowOf(const QString& pluginId) const;
    bool setEnabled(const QString& pluginId, bool enabled);  // as the check box does
    QString detailsText() const;

signals:
    void helpRequested(const QString& topic);

private:
    void showDetails();

    std::unique_ptr<Ui::PluginsDialog> ui_;
    resonant::plugins::PluginManager& manager_;
    std::function<void()> reload_;
    bool filling_ = false;
};
