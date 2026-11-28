#include "FunctionalPanel.h"

#include "FunctionalController.h"
#include "PlotWidget.h"

#include "ui_FunctionalPanel.h"

#include <QHeaderView>
#include <QPushButton>
#include <QSignalBlocker>

#include <cmath>

using nlohmann::json;

namespace {

QString number(const json& v)
{
    if (v.is_number()) {
        const double x = v.get<double>();
        return QString::number(x, 'g', 7);
    }
    if (v.is_string())
        return QString::fromStdString(v.get<std::string>()) == QStringLiteral("inf") ? QStringLiteral("∞") : QString::fromStdString(v.get<std::string>());
    return QStringLiteral("—");
}

QString text(const json& v, const char* key)
{
    return QString::fromStdString(v.value(key, std::string()));
}

}  // namespace

FunctionalPanel::FunctionalPanel(QWidget* parent) : QWidget(parent), ui_(std::make_unique<Ui::FunctionalPanel>())
{
    ui_->setupUi(this);
    for (QTableWidget* t : {ui_->parameterTable, ui_->metricTable, ui_->modeTable}) {
        t->horizontalHeader()->setStretchLastSection(true);
        t->verticalHeader()->setVisible(false);
    }
    setEnabled(false);
}

FunctionalPanel::~FunctionalPanel() = default;

void FunctionalPanel::setController(FunctionalController* controller)
{
    controller_ = controller;
    setEnabled(controller_ != nullptr);
    connect(controller_, &FunctionalController::structuresChanged, this, &FunctionalPanel::refresh);
    connect(controller_, &FunctionalController::resultReady, this, [this](const QString& id, const QString& kind) {
        if (kind == QStringLiteral("sweep")) {
            sweep_ = controller_->result(id);
            ui_->statusLabel->setText(tr("Sweep finished: %1 points.").arg(sweep_.value("points", json::array()).size()));
            plotCurve();
        } else {
            showResult(id);
        }
        ui_->analyzeButton->setEnabled(true);
        ui_->sweepButton->setEnabled(true);
    });
    connect(controller_, &FunctionalController::studyFailed, this, [this](const QString& message) {
        ui_->statusLabel->setText(tr("The study failed: %1").arg(message));
        ui_->analyzeButton->setEnabled(true);
        ui_->sweepButton->setEnabled(true);
    });
    connect(ui_->structureCombo, &QComboBox::currentIndexChanged, this, [this] {
        ui_->saveMaterialButton->setEnabled(false);
        analysis_ = json::object();
        sweep_ = json();
        resultId_.clear();
        showParameters();
    });
    connect(ui_->parameterTable, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        if (filling_ || item->column() != 1)
            return;
        bool ok = false;
        const double value = item->text().toDouble(&ok);
        const QString parameter = ui_->parameterTable->item(item->row(), 0)->data(Qt::UserRole).toString();
        if (!ok) {
            ui_->statusLabel->setText(tr("'%1' is not a number.").arg(item->text()));
            showParameters();
            return;
        }
        const auto status = controller_->setParameter(currentStructure(), parameter, value);
        if (!status)
            ui_->statusLabel->setText(QString::fromStdString(status.message()));
        // The document change refreshes the panel (structuresChanged).
    });
    connect(ui_->analyzeButton, &QPushButton::clicked, this, [this] {
        auto job = controller_->analyze(currentStructure(), ui_->modelCombo->currentData().toString());
        if (!job) {
            ui_->statusLabel->setText(QString::fromStdString(job.message()));
            return;
        }
        ui_->analyzeButton->setEnabled(false);
        ui_->statusLabel->setText(tr("Analyzing…"));
    });
    connect(ui_->saveMaterialButton, &QPushButton::clicked, this, [this] {
        auto saved = controller_->createEffectiveMaterial(resultId_);
        if (!saved) {
            ui_->statusLabel->setText(tr("The effective material was not saved: %1").arg(QString::fromStdString(saved.message())));
            return;
        }
        const QString name = QString::fromStdString(analysis_["details"]["effectiveMaterial"].value("name", ""));
        ui_->statusLabel->setText(tr("Saved '%1' to the materials library (provenance: homogenized). Undo removes it.").arg(name));
    });
    connect(ui_->sweepButton, &QPushButton::clicked, this, [this] {
        auto job = controller_->sweep(currentStructure(), ui_->sweepParameterCombo->currentData().toString(), ui_->sweepFrom->value(),
                                      ui_->sweepTo->value(), ui_->sweepPoints->value(), ui_->modelCombo->currentData().toString());
        if (!job) {
            ui_->statusLabel->setText(QString::fromStdString(job.message()));
            return;
        }
        ui_->sweepButton->setEnabled(false);
        ui_->statusLabel->setText(tr("Sweeping…"));
    });
    connect(ui_->sweepParameterCombo, &QComboBox::currentIndexChanged, this, [this] {
        // Suggest +-20 % around the present value.
        const QString id = ui_->sweepParameterCombo->currentData().toString();
        for (const json& p : controller_->parameters(currentStructure()))
            if (text(p, "id") == id) {
                const double v = p.value("value", 0.0);
                double lo = v * 0.8, hi = v * 1.2;
                if (p["min"].is_number())
                    lo = std::max(lo, p["min"].get<double>());
                if (p["max"].is_number())
                    hi = std::min(hi, p["max"].get<double>());
                ui_->sweepFrom->setValue(lo);
                ui_->sweepTo->setValue(hi);
            }
    });
    connect(ui_->curveCombo, &QComboBox::currentIndexChanged, this, &FunctionalPanel::plotCurve);
    connect(ui_->sweepMetricCombo, &QComboBox::currentIndexChanged, this, &FunctionalPanel::plotCurve);
    refresh();
}

void FunctionalPanel::refresh()
{
    if (!controller_)
        return;
    const QString current = currentStructure();
    {
        const QSignalBlocker block(ui_->structureCombo);
        ui_->structureCombo->clear();
        for (const json& s : controller_->structures())
            ui_->structureCombo->addItem(text(s, "name"), text(s, "id"));
        const int index = ui_->structureCombo->findData(current);
        ui_->structureCombo->setCurrentIndex(index >= 0 ? index : 0);
    }
    if (currentStructure() != current) {
        analysis_ = json::object();
        sweep_ = json();
        resultId_.clear();
    }
    showParameters();
    showStaleness();
}

void FunctionalPanel::showParameters()
{
    filling_ = true;
    const QString id = currentStructure();
    const json s = id.isEmpty() ? json() : controller_->structure(id);
    const json kind = s.is_object() ? controller_->analysisKind(QString::fromStdString(s.value("analysis", json::object()).value("kind", ""))) : json::object();
    ui_->summaryLabel->setText(s.is_object() ? tr("%1 — %2 (%3)").arg(text(s, "description"), text(kind, "title"), text(kind, "category"))
                                             : tr("No functional structure in this project. Example projects: examples/functional."));
    const QString model = ui_->modelCombo->currentData().toString();
    {
        const QSignalBlocker block(ui_->modelCombo);
        ui_->modelCombo->clear();
        for (const json& m : kind.value("models", json::array()))
            ui_->modelCombo->addItem(tr("%1 (%2)").arg(text(m, "name"), text(m, "fidelity")), text(m, "id"));
        const int i = ui_->modelCombo->findData(model);
        if (i >= 0)
            ui_->modelCombo->setCurrentIndex(i);
    }
    const json parameters = id.isEmpty() ? json::array() : controller_->parameters(id);
    ui_->parameterTable->setRowCount(static_cast<int>(parameters.size()));
    const QString sweepParameter = ui_->sweepParameterCombo->currentData().toString();
    {
        const QSignalBlocker block(ui_->sweepParameterCombo);
        ui_->sweepParameterCombo->clear();
        int row = 0;
        for (const json& p : parameters) {
            auto* name = new QTableWidgetItem(text(p, "name"));
            name->setData(Qt::UserRole, text(p, "id"));
            name->setFlags(name->flags() & ~Qt::ItemIsEditable);
            name->setToolTip(text(p, "binding").isEmpty() ? text(p, "kind") : text(p, "binding"));
            auto* value = new QTableWidgetItem(number(p["value"]));
            if (!p.value("editable", false))
                value->setFlags(value->flags() & ~Qt::ItemIsEditable);
            auto* unit = new QTableWidgetItem(text(p, "unit"));
            unit->setFlags(unit->flags() & ~Qt::ItemIsEditable);
            auto* range = new QTableWidgetItem(QStringLiteral("%1 … %2").arg(number(p["min"]), number(p["max"])));
            range->setFlags(range->flags() & ~Qt::ItemIsEditable);
            auto* kindItem = new QTableWidgetItem(text(p, "kind") + (text(p, "object").isEmpty() ? QString() : QStringLiteral(" (%1)").arg(text(p, "object"))));
            kindItem->setFlags(kindItem->flags() & ~Qt::ItemIsEditable);
            ui_->parameterTable->setItem(row, 0, name);
            ui_->parameterTable->setItem(row, 1, value);
            ui_->parameterTable->setItem(row, 2, unit);
            ui_->parameterTable->setItem(row, 3, range);
            ui_->parameterTable->setItem(row, 4, kindItem);
            if (p.value("editable", false))
                ui_->sweepParameterCombo->addItem(text(p, "name"), text(p, "id"));
            ++row;
        }
        const int i = ui_->sweepParameterCombo->findData(sweepParameter);
        ui_->sweepParameterCombo->setCurrentIndex(i >= 0 ? i : 0);
    }
    {
        const QSignalBlocker block(ui_->sweepMetricCombo);
        const QString metric = ui_->sweepMetricCombo->currentData().toString();
        ui_->sweepMetricCombo->clear();
        for (const json& m : kind.value("metrics", json::array()))
            ui_->sweepMetricCombo->addItem(text(m, "label"), text(m, "key"));
        const int i = ui_->sweepMetricCombo->findData(metric);
        if (i >= 0)
            ui_->sweepMetricCombo->setCurrentIndex(i);
    }
    if (ui_->sweepFrom->value() == 0.0 && ui_->sweepTo->value() == 0.0)
        emit ui_->sweepParameterCombo->currentIndexChanged(ui_->sweepParameterCombo->currentIndex());
    ui_->analyzeButton->setEnabled(!id.isEmpty() && !controller_->busy());
    ui_->sweepButton->setEnabled(!id.isEmpty() && !controller_->busy());
    filling_ = false;
}

void FunctionalPanel::showResult(const QString& resultId)
{
    resultId_ = resultId;
    analysis_ = controller_->result(resultId);
    ui_->metricTable->setRowCount(0);
    for (const json& m : analysis_.value("metrics", json::array())) {
        const int row = ui_->metricTable->rowCount();
        ui_->metricTable->insertRow(row);
        auto* label = new QTableWidgetItem(text(m, "label"));
        label->setData(Qt::UserRole, text(m, "key"));
        label->setToolTip(text(m, "definition"));
        ui_->metricTable->setItem(row, 0, label);
        ui_->metricTable->setItem(row, 1, new QTableWidgetItem(number(m["value"])));
        ui_->metricTable->setItem(row, 2, new QTableWidgetItem(text(m, "unit")));
    }
    ui_->modeTable->setRowCount(0);
    for (const json& m : analysis_.value("modes", json::array())) {
        const int row = ui_->modeTable->rowCount();
        ui_->modeTable->insertRow(row);
        ui_->modeTable->setItem(row, 0, new QTableWidgetItem(text(m, "label").isEmpty() ? tr("mode %1").arg(row + 1) : text(m, "label")));
        ui_->modeTable->setItem(row, 1, new QTableWidgetItem(text(m, "nature")));
        ui_->modeTable->setItem(row, 2, new QTableWidgetItem(number(m["frequencyHz"])));
        ui_->modeTable->setItem(row, 3, new QTableWidgetItem(number(m["quality"])));
        QStringList losses;
        const json lossQ = m.value("lossQ", json::object());  // items() must not outlive its object
        for (const auto& [mechanism, q] : lossQ.items())
            losses << QStringLiteral("%1 %2").arg(QString::fromStdString(mechanism), number(q));
        ui_->modeTable->setItem(row, 4, new QTableWidgetItem(losses.join(QStringLiteral("; "))));
    }
    QStringList details;
    const json info = analysis_.value("analysis", json::object());
    const json model = info.value("model", json::object());
    details << tr("Model: %1 (%2, version %3)").arg(text(model, "name"), text(model, "fidelity"), text(model, "version"));
    details << tr("Behaviour established: %1").arg(text(analysis_, "behaviour"));
    details << tr("Mechanism: %1").arg(QString::fromStdString(info.value("mechanism", "")));
    details << tr("Validity: %1").arg(text(model, "validity"));
    details << tr("Assumptions:");
    for (const json& a : analysis_.value("assumptions", json::array()))
        details << QStringLiteral("  • ") + QString::fromStdString(a.get<std::string>());
    for (const json& w : analysis_.value("warnings", json::array()))
        details << tr("Warning: %1").arg(QString::fromStdString(w.get<std::string>()));
    for (const json& c : analysis_.value("constraints", json::array()))
        details << tr("Constraint %1 (%2%3): %4").arg(text(c, "id"), text(c, "category"),
                                                     text(c, "process").isEmpty() ? QString() : QStringLiteral(", ") + text(c, "process"),
                                                     c.value("satisfied", false) ? tr("satisfied") : tr("violated"));
    for (const json& o : analysis_.value("objectives", json::array()))
        details << tr("Objective %1 (%2 %3): %4").arg(text(o, "id"), text(o, "sense"), text(o, "metric"), number(o["value"]));
    const json effective = analysis_.value("details", json::object()).value("effectiveMaterial", json());
    if (effective.is_object()) {
        details << tr("Effective material: %1 (%2, by %3)").arg(text(effective, "name"), text(effective, "kind"), text(effective, "method"));
        details << tr("  valid: %1").arg(text(effective, "validity"));
        for (const json& p : effective.value("properties", json::array()))
            details << QStringLiteral("  • %1.%2 (%3) = %4%5")
                           .arg(text(p, "domain"), text(p, "key"), text(p, "component"), number(p["value"]),
                                p.value("frequencyTable", json::array()).empty() ? QString() : tr(", frequency-dependent"));
        for (const json& l : effective.value("limitations", json::array()))
            details << tr("  limitation: %1").arg(QString::fromStdString(l.get<std::string>()));
    }
    const json provenance = analysis_.value("provenance", json::object());
    details << tr("Computed %1 by %2").arg(QString::fromStdString(provenance.value("created", "")),
                                           QString::fromStdString(provenance.value("application", "")));
    ui_->detailsText->setPlainText(details.join(QLatin1Char('\n')));
    {
        const QSignalBlocker block(ui_->curveCombo);
        ui_->curveCombo->clear();
        for (const json& c : analysis_.value("curves", json::array()))
            ui_->curveCombo->addItem(text(c, "title"), text(c, "id"));
    }
    plotCurve();
    showStaleness();
}

void FunctionalPanel::showStaleness()
{
    if (resultId_.isEmpty() || !controller_)
        return;
    const json now = controller_->result(resultId_);
    ui_->saveMaterialButton->setEnabled(!now.value("stale", false) && now.value("details", json::object()).contains("effectiveMaterial"));
    if (now.value("stale", false)) {
        QStringList reasons;
        for (const json& r : now.value("currentStaleness", json::array()))
            reasons << text(r, "detail");
        ui_->statusLabel->setText(tr("The result shown is stale: %1. Analyze again.").arg(reasons.join(QStringLiteral("; "))));
    } else {
        ui_->statusLabel->setText(tr("The result is current (%1, %2).")
                                      .arg(QString::fromStdString(analysis_.value("analysis", json::object()).value("model", json::object()).value("name", "")),
                                           analysis_.value("feasible", true) ? tr("constraints satisfied") : tr("a hard constraint is violated")));
    }
}

void FunctionalPanel::plotCurve()
{
    // A finished sweep is shown for the chosen metric; otherwise the chosen curve of the analysis.
    if (sweep_.is_object() && !sweep_.value("points", json::array()).empty()) {
        const std::string parameter = sweep_["axes"][0].value("parameter", "");
        const std::string metric = ui_->sweepMetricCombo->currentData().toString().toStdString();
        QVector<double> x, y;
        for (const json& p : sweep_["points"])
            if (p.value("status", "") == "ok" && p["metrics"].contains(metric) && p["metrics"][metric].is_number()) {
                x.push_back(p["values"][parameter].get<double>());
                y.push_back(p["metrics"][metric].get<double>());
            }
        ui_->plot->setLogX(false);
        ui_->plot->setCurve(x, y, ui_->sweepMetricCombo->currentText(), QString::fromStdString(parameter), ui_->sweepMetricCombo->currentText());
        return;
    }
    const QString id = ui_->curveCombo->currentData().toString();
    for (const json& c : analysis_.value("curves", json::array()))
        if (text(c, "id") == id) {
            const auto xs = c["x"].get<std::vector<double>>();
            const auto ys = c["y"].get<std::vector<double>>();
            ui_->plot->setLogX(false);
            ui_->plot->setCurve(QVector<double>(xs.begin(), xs.end()), QVector<double>(ys.begin(), ys.end()), text(c, "title"),
                                text(c, "xLabel") + QStringLiteral(" (") + text(c, "xUnit") + QLatin1Char(')'),
                                text(c, "yLabel") + (text(c, "yUnit").isEmpty() ? QString() : QStringLiteral(" (") + text(c, "yUnit") + QLatin1Char(')')));
            return;
        }
    ui_->plot->clear();
}

int FunctionalPanel::structureCount() const
{
    return ui_->structureCombo->count();
}

void FunctionalPanel::selectStructure(const QString& id)
{
    ui_->structureCombo->setCurrentIndex(ui_->structureCombo->findData(id));
}

QString FunctionalPanel::currentStructure() const
{
    return ui_->structureCombo->currentData().toString();
}

int FunctionalPanel::parameterRowCount() const
{
    return ui_->parameterTable->rowCount();
}

QString FunctionalPanel::parameterId(int row) const
{
    return ui_->parameterTable->item(row, 0)->data(Qt::UserRole).toString();
}

void FunctionalPanel::editParameter(int row, double value)
{
    ui_->parameterTable->item(row, 1)->setText(QString::number(value, 'g', 12));
}

QString FunctionalPanel::metricText(const QString& key) const
{
    for (int r = 0; r < ui_->metricTable->rowCount(); ++r)
        if (ui_->metricTable->item(r, 0)->data(Qt::UserRole).toString() == key)
            return ui_->metricTable->item(r, 1)->text() + QLatin1Char(' ') + ui_->metricTable->item(r, 2)->text();
    return {};
}

int FunctionalPanel::modeRowCount() const
{
    return ui_->modeTable->rowCount();
}

QString FunctionalPanel::modeLabel(int row) const
{
    return ui_->modeTable->item(row, 0)->text();
}

QString FunctionalPanel::modeCell(int row, int column) const
{
    return ui_->modeTable->item(row, column)->text();
}

QString FunctionalPanel::statusText() const
{
    return ui_->statusLabel->text();
}

QString FunctionalPanel::detailsText() const
{
    return ui_->detailsText->toPlainText();
}

QStringList FunctionalPanel::curveChoices() const
{
    QStringList out;
    for (int i = 0; i < ui_->curveCombo->count(); ++i)
        out << ui_->curveCombo->itemData(i).toString();
    return out;
}

void FunctionalPanel::showCurve(int index)
{
    sweep_ = json();
    ui_->curveCombo->setCurrentIndex(index);
    plotCurve();
}

int FunctionalPanel::plottedPoints() const
{
    return ui_->plot->pointCount();
}

QPushButton* FunctionalPanel::analyzeButton() const
{
    return ui_->analyzeButton;
}

QPushButton* FunctionalPanel::saveMaterialButton() const
{
    return ui_->saveMaterialButton;
}

void FunctionalPanel::startSweep(const QString& parameter, double from, double to, int points, const QString& metric)
{
    ui_->sweepParameterCombo->setCurrentIndex(ui_->sweepParameterCombo->findData(parameter));
    ui_->sweepFrom->setValue(from);
    ui_->sweepTo->setValue(to);
    ui_->sweepPoints->setValue(points);
    ui_->sweepMetricCombo->setCurrentIndex(ui_->sweepMetricCombo->findData(metric));
    ui_->sweepButton->click();
}
