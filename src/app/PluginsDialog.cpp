#include "PluginsDialog.h"

#include "plugins/PluginManager.h"
#include "ui_PluginsDialog.h"

#include <QDir>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>

using namespace resonant::plugins;

PluginsDialog::PluginsDialog(PluginManager& manager, std::function<void()> reload, QWidget* parent)
    : QDialog(parent), ui_(std::make_unique<Ui::PluginsDialog>()), manager_(manager), reload_(std::move(reload))
{
    ui_->setupUi(this);
    setProperty("helpTopic", QStringLiteral("py.plugins"));
    ui_->pluginTable->horizontalHeader()->setStretchLastSection(true);
    ui_->pluginTable->verticalHeader()->hide();
    connect(ui_->pluginTable, &QTableWidget::itemSelectionChanged, this, &PluginsDialog::showDetails);
    connect(ui_->pluginTable, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        if (filling_ || item->column() != 0)
            return;
        const QString id = item->data(Qt::UserRole).toString();
        const bool on = item->checkState() == Qt::Checked;
        if (on && QMessageBox::question(this, tr("Enable Plugin"),
                                        tr("Enable '%1'? It will run with your permissions when the plugins are reloaded.").arg(id)) != QMessageBox::Yes) {
            filling_ = true;
            item->setCheckState(Qt::Unchecked);
            filling_ = false;
            return;
        }
        setEnabled(id, on);
    });
    connect(ui_->reloadButton, &QPushButton::clicked, this, [this] {
        if (reload_)
            reload_();
        refresh();
    });
    connect(ui_->buttonBox->button(QDialogButtonBox::Help), &QPushButton::clicked, this, [this] { emit helpRequested(QStringLiteral("py.plugins")); });
    refresh();
}

PluginsDialog::~PluginsDialog() = default;

void PluginsDialog::refresh()
{
    filling_ = true;
    const auto& plugins = manager_.plugins();
    ui_->pluginTable->setRowCount(static_cast<int>(plugins.size()));
    for (int r = 0; r < static_cast<int>(plugins.size()); ++r) {
        const PluginRecord& p = plugins[static_cast<size_t>(r)];
        auto* enabled = new QTableWidgetItem;
        enabled->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        enabled->setCheckState(p.enabled ? Qt::Checked : Qt::Unchecked);
        enabled->setData(Qt::UserRole, QString::fromStdString(p.manifest.id));
        ui_->pluginTable->setItem(r, 0, enabled);
        ui_->pluginTable->setItem(r, 1, new QTableWidgetItem(QString::fromStdString(p.manifest.name)));
        ui_->pluginTable->setItem(r, 2, new QTableWidgetItem(QString::fromStdString(p.manifest.version)));
        ui_->pluginTable->setItem(r, 3, new QTableWidgetItem(QString::fromStdString(p.manifest.kind)));
        auto* state = new QTableWidgetItem(QString::fromStdString(std::string(toString(p.state))));
        if (p.state == PluginState::Failed || p.state == PluginState::Invalid || p.state == PluginState::Incompatible ||
            p.state == PluginState::MissingDependency)
            state->setForeground(QColor(0xa1, 0x2a, 0x2a));
        else if (p.state == PluginState::Loaded)
            state->setForeground(QColor(0x1b, 0x6e, 0x2e));
        ui_->pluginTable->setItem(r, 4, state);
        QString message = QString::fromStdString(p.message);
        message.replace(QDir::homePath(), QStringLiteral("~"));
        ui_->pluginTable->setItem(r, 5, new QTableWidgetItem(message));
    }
    ui_->pluginTable->resizeColumnsToContents();
    QStringList paths;
    // The home directory is shown as ~ (no private paths in screenshots).
    for (const auto& p : manager_.searchPaths()) {
        QString path = QString::fromStdString(p.string());
        if (path.startsWith(QDir::homePath()))
            path = QStringLiteral("~") + path.mid(QDir::homePath().size());
        paths.append(path);
    }
    ui_->pathsLabel->setText(tr("Searched: %1").arg(paths.join(QStringLiteral("; "))));
    filling_ = false;
    showDetails();
}

int PluginsDialog::rowCount() const
{
    return ui_->pluginTable->rowCount();
}

QString PluginsDialog::cell(int row, int column) const
{
    const auto* item = ui_->pluginTable->item(row, column);
    if (!item)
        return {};
    if (column == 0)
        return item->checkState() == Qt::Checked ? QStringLiteral("yes") : QStringLiteral("no");
    return item->text();
}

int PluginsDialog::rowOf(const QString& id) const
{
    for (int r = 0; r < rowCount(); ++r)
        if (ui_->pluginTable->item(r, 0)->data(Qt::UserRole).toString() == id)
            return r;
    return -1;
}

bool PluginsDialog::setEnabled(const QString& id, bool enabled)
{
    const auto status = manager_.setEnabled(id.toStdString(), enabled);
    if (!status) {
        QMessageBox::warning(this, tr("Plugins"), QString::fromStdString(status.message()));
        return false;
    }
    const int row = rowOf(id);
    if (row >= 0) {
        filling_ = true;
        ui_->pluginTable->item(row, 0)->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
        ui_->pluginTable->item(row, 5)->setText(enabled ? tr("enabled: loads when the plugins are reloaded") : tr("disabled: unloads at the next start"));
        filling_ = false;
    }
    return true;
}

QString PluginsDialog::detailsText() const
{
    return ui_->detailsView->toPlainText();
}

void PluginsDialog::showDetails()
{
    const int row = ui_->pluginTable->currentRow();
    if (row < 0 || row >= static_cast<int>(manager_.plugins().size())) {
        ui_->detailsView->setHtml(tr("<p>Select a plugin to see what it provides, what it needs and what it declares it does.</p>"));
        return;
    }
    const PluginRecord& p = manager_.plugins()[static_cast<size_t>(row)];
    const auto list = [](const std::vector<std::string>& items) {
        QStringList out;
        for (const auto& s : items)
            out.append(QString::fromStdString(s).toHtmlEscaped());
        return out.isEmpty() ? QStringLiteral("none") : out.join(QStringLiteral(", "));
    };
    QStringList deps;
    for (const auto& d : p.manifest.dependencies)
        deps.append(QString::fromStdString(d.id + " " + d.minVersion + "+").toHtmlEscaped());
    ui_->detailsView->setHtml(
        QStringLiteral("<b>%1</b> (%2) %3<br/>%4<br/><b>Author:</b> %5 &nbsp; <b>License:</b> %6<br/><b>Provides:</b> %7<br/>"
                       "<b>Requires:</b> %8 &nbsp; <b>Depends on:</b> %9<br/><b>Declares it:</b> %10<br/><b>Supports Resonant:</b> %11 to %12, "
                       "plugin API %13<br/><b>Registered:</b> %14<br/><b>State:</b> %15: %16")
            .arg(QString::fromStdString(p.manifest.name).toHtmlEscaped(), QString::fromStdString(p.manifest.id).toHtmlEscaped(),
                 QString::fromStdString(p.manifest.version).toHtmlEscaped(), QString::fromStdString(p.manifest.description).toHtmlEscaped(),
                 QString::fromStdString(p.manifest.author).toHtmlEscaped(), QString::fromStdString(p.manifest.license).toHtmlEscaped(),
                 list(p.manifest.provides), list(p.manifest.requires_), deps.isEmpty() ? QStringLiteral("none") : deps.join(QStringLiteral(", ")))
            .arg(list(p.manifest.permissions), QString::fromStdString(p.manifest.resonantMin).toHtmlEscaped(),
                 QString::fromStdString(p.manifest.resonantMax).toHtmlEscaped(), QString::number(p.manifest.apiVersion), list(p.extensions),
                 QString::fromStdString(std::string(toString(p.state))), QString::fromStdString(p.message).toHtmlEscaped()));
}
