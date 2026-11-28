#include "AccountingPanel.h"

#include "AccountingController.h"
#include "EnergyFlowView.h"
#include "PlotWidget.h"

#include "ui_AccountingPanel.h"

#include <QFile>
#include <QFileDialog>
#include <QHeaderView>
#include <QPushButton>
#include <QSignalBlocker>

using nlohmann::json;

namespace {

QString text(const json& v, const char* key)
{
    return QString::fromStdString(v.is_object() ? v.value(key, std::string()) : std::string());
}

QString number(const json& v)
{
    if (v.is_number())
        return QString::number(v.get<double>(), 'g', 5);
    if (v.is_array()) {
        QStringList parts;
        for (const json& x : v)
            parts << number(x);
        return QStringLiteral("(%1)").arg(parts.join(QStringLiteral(", ")));
    }
    return QStringLiteral("—");
}

QString familyTitle(const std::string& id)
{
    if (id == "energy")
        return QObject::tr("Energy");
    if (id == "linear-momentum")
        return QObject::tr("Linear momentum");
    if (id == "angular-momentum")
        return QObject::tr("Angular momentum");
    if (id == "entropy")
        return QObject::tr("Entropy");
    return QString::fromStdString(id);
}

}  // namespace

AccountingPanel::AccountingPanel(QWidget* parent) : QWidget(parent), ui_(std::make_unique<Ui::AccountingPanel>())
{
    ui_->setupUi(this);
    ui_->checksTable->setColumnCount(7);
    ui_->checksTable->setHorizontalHeaderLabels({tr("Check"), tr("Status"), tr("Residual"), tr("Relative"), tr("Tolerance"), tr("Unit"), tr("Why")});
    ui_->checksTable->horizontalHeader()->setStretchLastSection(true);
    ui_->checksTable->verticalHeader()->setVisible(false);
    ui_->forcesTable->setColumnCount(6);
    ui_->forcesTable->setHorizontalHeaderLabels({tr("Force"), tr("Category"), tr("From → on"), tr("Vector"), tr("Method"), tr("Pair")});
    ui_->forcesTable->horizontalHeader()->setStretchLastSection(true);
    ui_->forcesTable->verticalHeader()->setVisible(false);
    setEnabled(false);
}

AccountingPanel::~AccountingPanel() = default;

void AccountingPanel::setController(AccountingController* controller)
{
    controller_ = controller;
    setEnabled(controller_ != nullptr);
    connect(controller_, &AccountingController::modelChanged, this, &AccountingPanel::refresh);
    connect(controller_, &AccountingController::resultReady, this, [this](const QString& id) {
        showResult(id);
        ui_->runButton->setEnabled(ui_->studyCombo->count() > 0);
    });
    connect(controller_, &AccountingController::runFailed, this, [this](const QString& message) {
        ui_->statusLabel->setText(tr("The accounting failed: %1").arg(message));
        ui_->runButton->setEnabled(ui_->studyCombo->count() > 0);
    });
    connect(ui_->runButton, &QPushButton::clicked, this, [this] {
        auto job = controller_->run(ui_->studyCombo->currentData().toString());
        if (!job) {
            ui_->statusLabel->setText(QString::fromStdString(job.message()));
            return;
        }
        ui_->runButton->setEnabled(false);
        ui_->statusLabel->setText(tr("Running…"));
    });
    connect(ui_->systemCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (filling_ || resultId_.isEmpty())
            return;
        const json reports = result_.value("reports", json::array());
        const bool custom = index >= static_cast<int>(reports.size());
        ui_->applyButton->setEnabled(custom);
        if (!custom)
            showReport(reports[static_cast<size_t>(index)]);
    });
    connect(ui_->applyButton, &QPushButton::clicked, this, [this] {
        json members = json::array();
        for (int i = 0; i < ui_->memberList->count(); ++i)
            if (ui_->memberList->item(i)->checkState() == Qt::Checked)
                members.push_back(ui_->memberList->item(i)->data(Qt::UserRole).toString().toStdString());
        auto r = controller_->report(resultId_, {{"id", "custom"}, {"name", "Custom system"}, {"kind", "subsystem"}, {"members", members}});
        if (!r) {
            ui_->statusLabel->setText(tr("Not applied: %1").arg(QString::fromStdString(r.message())));
            return;
        }
        showReport(r.value());
    });
    connect(ui_->familyCombo, &QComboBox::currentIndexChanged, this, [this] {
        if (!filling_)
            showFlows();
    });
    connect(ui_->seriesCombo, &QComboBox::currentIndexChanged, this, &AccountingPanel::plotSeries);
    connect(ui_->saveButton, &QPushButton::clicked, this, [this] {
        auto id = controller_->saveRecord(resultId_, text(result_, "title"));
        ui_->statusLabel->setText(id ? tr("Saved the record %1 in the project. Undo removes it.").arg(id.value())
                                     : tr("Not saved: %1").arg(QString::fromStdString(id.message())));
    });
    connect(ui_->exportButton, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getSaveFileName(this, tr("Export the accounting"), QString(), tr("Markdown (*.md);;CSV (*.csv);;JSON (*.json)"));
        if (!path.isEmpty() && !exportTo(path))
            ui_->statusLabel->setText(tr("Not exported to %1").arg(path));
    });
    refresh();
}

void AccountingPanel::refresh()
{
    if (!controller_)
        return;
    const QSignalBlocker b(ui_->studyCombo);
    const QString current = ui_->studyCombo->currentData().toString();
    ui_->studyCombo->clear();
    for (const json& s : controller_->studies())
        ui_->studyCombo->addItem(text(s, "name").isEmpty() ? text(s, "id") : text(s, "name"), text(s, "id"));
    if (const int i = ui_->studyCombo->findData(current); i >= 0)
        ui_->studyCombo->setCurrentIndex(i);
    ui_->runButton->setEnabled(ui_->studyCombo->count() > 0 && !controller_->busy());
}

void AccountingPanel::showResult(const QString& resultId)
{
    resultId_ = resultId;
    result_ = controller_->result(resultId);
    filling_ = true;
    {
        const QSignalBlocker b(ui_->systemCombo);
        ui_->systemCombo->clear();
        for (const json& s : result_.value("systems", json::array()))
            ui_->systemCombo->addItem(text(s, "name"), text(s, "id"));
        ui_->systemCombo->addItem(tr("Custom (the ticked members)"), QStringLiteral("custom"));
    }
    ui_->memberList->clear();
    const json ledger = result_.value("ledger", json::object());
    for (const json& e : ledger.value("entities", json::array())) {
        const std::string kind = e.value("kind", std::string());
        if (kind == "source" || kind == "sink" || kind == "environment")
            continue;
        auto* item = new QListWidgetItem(QStringLiteral("%1 (%2)").arg(text(e, "name"), QString::fromStdString(kind)), ui_->memberList);
        item->setData(Qt::UserRole, text(e, "id"));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
    }
    filling_ = false;
    ui_->saveButton->setEnabled(true);
    ui_->exportButton->setEnabled(true);
    ui_->applyButton->setEnabled(false);
    const json reports = result_.value("reports", json::array());
    if (!reports.empty())
        showReport(reports[0]);
}

void AccountingPanel::showReport(const json& report)
{
    report_ = report;
    // Status line: the overall status and the energy balance's attribution.
    QStringList status;
    status << tr("%1 — %2: %3").arg(text(result_, "title"), text(report["system"], "name"), text(report, "status"));
    for (const json& c : report.value("checks", json::array()))
        if (c.value("model", std::string()) == "energy.total" && !c.value("attribution", std::string()).empty())
            status << tr("energy residual %1").arg(text(c, "attribution"));
    ui_->statusLabel->setText(status.join(QStringLiteral("; ")));
    // Checks.
    const json checks = report.value("checks", json::array());
    ui_->checksTable->setRowCount(static_cast<int>(checks.size()));
    for (size_t r = 0; r < checks.size(); ++r) {
        const json& c = checks[r];
        QStringList why;
        if (!c.value("attribution", std::string()).empty())
            why << text(c, "attribution");
        for (const json& m : c.value("missing", json::array()))
            why << tr("missing: %1").arg(QString::fromStdString(m.get<std::string>()));
        for (const json& n : c.value("notes", json::array()))
            why << QString::fromStdString(n.get<std::string>());
        const QStringList cells{text(c, "title"),  text(c, "status"), number(c.value("residualNorm", json())), number(c.value("relative", json())),
                                number(c.value("tolerance", json())), text(c, "unit"), why.join(QStringLiteral("; "))};
        for (int k = 0; k < cells.size(); ++k) {
            auto* item = new QTableWidgetItem(cells[k]);
            if (k == 0)
                item->setData(Qt::UserRole, text(c, "model"));
            if (k == 1) {
                const QString s = cells[k];
                item->setForeground(s == QLatin1String("verified")   ? QColor(30, 130, 60)
                                    : s == QLatin1String("exceeded") ? QColor(190, 40, 40)
                                    : s == QLatin1String("warning")  ? QColor(180, 120, 0)
                                                                     : palette().color(QPalette::PlaceholderText));
            }
            ui_->checksTable->setItem(static_cast<int>(r), k, item);
        }
    }
    // Breakdown.
    ui_->breakdownTree->clear();
    for (const json& b : report.value("breakdown", json::array())) {
        const QString unit = text(b, "unit");
        auto* top = new QTreeWidgetItem(ui_->breakdownTree, {familyTitle(b.value("family", std::string())), QString(), text(b, "status")});
        for (const char* key : {"input", "impulse", "output", "dissipated", "internal", "generated", "stored", "residual"})
            if (b.contains(key))
                new QTreeWidgetItem(top, {QString::fromLatin1(key), number(b[key]) + QLatin1Char(' ') + unit, QString()});
        auto* contributions = new QTreeWidgetItem(top, {tr("Exchanges"), QString(), QString()});
        for (const json& x : b.value("contributions", json::array()))
            new QTreeWidgetItem(contributions, {QStringLiteral("%1 → %2 (%3)").arg(text(x, "from"), text(x, "to"), text(x, "mechanism")),
                                                number(x["amount"]) + QLatin1Char(' ') + unit, text(x, "role")});
        auto* holdings = new QTreeWidgetItem(top, {tr("Holdings"), QString(), QString()});
        for (const json& h : b.value("holdings", json::array()))
            new QTreeWidgetItem(holdings, {QStringLiteral("%1: %2").arg(text(h, "entity"), text(h, "form")), number(h["change"]) + QLatin1Char(' ') + unit,
                                           h.value("recoverable", true) ? tr("recoverable") : tr("not recoverable")});
        top->setExpanded(true);
    }
    // Forces.
    ui_->forcesTable->setRowCount(0);
    const json forces = report.value("forces", json::object());
    const json targets = forces.value("targets", json::object());  // named: items() of a temporary would dangle
    for (const auto& [target, t] : targets.items())
        for (const json& f : t.value("contributions", json::array())) {
            const int row = ui_->forcesTable->rowCount();
            ui_->forcesTable->insertRow(row);
            const QStringList cells{text(f, "id"), text(f, "category"), QStringLiteral("%1 → %2").arg(text(f, "source"), QString::fromStdString(target)),
                                    number(f["vector"]) + (text(f, "family") == QLatin1String("torque") ? tr(" N m") : tr(" N")), text(f, "method"),
                                    text(f, "pair")};
            for (int k = 0; k < cells.size(); ++k)
                ui_->forcesTable->setItem(row, k, new QTableWidgetItem(cells[k]));
        }
    // Families with flows.
    {
        filling_ = true;
        const QSignalBlocker b(ui_->familyCombo);
        const QString current = ui_->familyCombo->currentData().toString();
        ui_->familyCombo->clear();
        std::vector<std::string> seen;
        for (const json& e : report["flows"].value("edges", json::array())) {
            const std::string f = e.value("family", std::string());
            if (std::find(seen.begin(), seen.end(), f) == seen.end()) {
                seen.push_back(f);
                ui_->familyCombo->addItem(familyTitle(f), QString::fromStdString(f));
            }
        }
        if (const int i = ui_->familyCombo->findData(current); i >= 0)
            ui_->familyCombo->setCurrentIndex(i);
        filling_ = false;
    }
    showFlows();
    // Histories: the series of the balances and the residual of every check that has one.
    {
        const QSignalBlocker b(ui_->seriesCombo);
        ui_->seriesCombo->clear();
        int k = 0;
        for (const json& s : report.value("series", json::array())) {
            for (const char* key : {"stored", "input", "output", "dissipated", "internal", "residual"})
                ui_->seriesCombo->addItem(tr("%1: %2").arg(familyTitle(s.value("family", std::string())), QString::fromLatin1(key)),
                                          QStringLiteral("series:%1:%2").arg(k).arg(QString::fromLatin1(key)));
            ++k;
        }
        for (size_t c = 0; c < checks.size(); ++c)
            if (checks[c].contains("residualHistory"))
                ui_->seriesCombo->addItem(tr("Residual over time: %1").arg(text(checks[c], "title")), QStringLiteral("check:%1").arg(c));
    }
    plotSeries();
}

void AccountingPanel::showFlows()
{
    const QString family = ui_->familyCombo->currentData().toString();
    QString unit;
    for (const json& b : report_.value("breakdown", json::array()))
        if (QString::fromStdString(b.value("family", std::string())) == family)
            unit = text(b, "unit");
    if (unit.isEmpty())
        for (const json& c : report_.value("checks", json::array()))
            if (QString::fromStdString(c.value("family", std::string())) == family)
                unit = text(c, "unit");
    ui_->flowView->setFlows(report_.value("flows", json::object()), family, unit);
}

void AccountingPanel::plotSeries()
{
    const QString key = ui_->seriesCombo->currentData().toString();
    if (key.isEmpty()) {
        ui_->plot->clear();
        return;
    }
    const QStringList parts = key.split(QLatin1Char(':'));
    std::vector<double> x, y;
    QString title = ui_->seriesCombo->currentText(), yLabel;
    if (parts[0] == QLatin1String("series")) {
        const json& s = report_["series"][parts[1].toUInt()];
        x = s.value("times", std::vector<double>{});
        y = s.value(parts[2].toStdString(), std::vector<double>{});
        yLabel = text(s, "unit");
    } else {
        const json& c = report_["checks"][parts[1].toUInt()];
        x = c.value("times", std::vector<double>{});
        y = c.value("residualHistory", std::vector<double>{});
        yLabel = tr("|residual| (%1)").arg(text(c, "unit"));
    }
    ui_->plot->setLogX(false);
    ui_->plot->setCurve(QVector<double>(x.begin(), x.end()), QVector<double>(y.begin(), y.end()), title, tr("Time (s)"), yLabel);
}

bool AccountingPanel::exportTo(const QString& path)
{
    const QString format = path.endsWith(QLatin1String(".csv")) ? QStringLiteral("csv") : path.endsWith(QLatin1String(".json")) ? QStringLiteral("json")
                                                                                                                                  : QStringLiteral("markdown");
    auto textOut = controller_->exportResult(resultId_, format);
    if (!textOut)
        return false;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    f.write(textOut.value().toUtf8());
    ui_->statusLabel->setText(tr("Exported the accounting (%1).").arg(format));
    return true;
}

int AccountingPanel::studyCount() const { return ui_->studyCombo->count(); }
void AccountingPanel::selectStudy(const QString& id) { ui_->studyCombo->setCurrentIndex(ui_->studyCombo->findData(id)); }
QPushButton* AccountingPanel::runButton() const { return ui_->runButton; }
QPushButton* AccountingPanel::applyButton() const { return ui_->applyButton; }
QPushButton* AccountingPanel::saveButton() const { return ui_->saveButton; }
QStringList AccountingPanel::systemChoices() const
{
    QStringList out;
    for (int i = 0; i < ui_->systemCombo->count(); ++i)
        out << ui_->systemCombo->itemData(i).toString();
    return out;
}
void AccountingPanel::selectSystem(const QString& id) { ui_->systemCombo->setCurrentIndex(ui_->systemCombo->findData(id)); }
void AccountingPanel::tickMembers(const QStringList& ids)
{
    for (int i = 0; i < ui_->memberList->count(); ++i) {
        QListWidgetItem* item = ui_->memberList->item(i);
        item->setCheckState(ids.contains(item->data(Qt::UserRole).toString()) ? Qt::Checked : Qt::Unchecked);
    }
}
QStringList AccountingPanel::familyChoices() const
{
    QStringList out;
    for (int i = 0; i < ui_->familyCombo->count(); ++i)
        out << ui_->familyCombo->itemData(i).toString();
    return out;
}
void AccountingPanel::selectFamily(const QString& id) { ui_->familyCombo->setCurrentIndex(ui_->familyCombo->findData(id)); }
QString AccountingPanel::checkStatus(const QString& model) const
{
    for (int r = 0; r < ui_->checksTable->rowCount(); ++r)
        if (ui_->checksTable->item(r, 0)->data(Qt::UserRole).toString() == model)
            return ui_->checksTable->item(r, 1)->text();
    return {};
}
const EnergyFlowView& AccountingPanel::flowView() const { return *ui_->flowView; }
int AccountingPanel::forceRows() const { return ui_->forcesTable->rowCount(); }
QStringList AccountingPanel::seriesChoices() const
{
    QStringList out;
    for (int i = 0; i < ui_->seriesCombo->count(); ++i)
        out << ui_->seriesCombo->itemData(i).toString();
    return out;
}
void AccountingPanel::showSeries(int index) { ui_->seriesCombo->setCurrentIndex(index); }
int AccountingPanel::plottedPoints() const { return ui_->plot->pointCount(); }
QString AccountingPanel::statusText() const { return ui_->statusLabel->text(); }
