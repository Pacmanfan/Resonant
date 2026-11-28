#include "MatterPanel.h"

#include "MatterController.h"
#include "PlotWidget.h"
#include "StackView.h"

#include "ui_MatterPanel.h"

#include <QHeaderView>
#include <QPushButton>
#include <QSignalBlocker>

#include <cmath>

using nlohmann::json;

namespace {

QString text(const json& v, const char* key)
{
    return QString::fromStdString(v.is_object() ? v.value(key, std::string()) : std::string());
}

QString number(double v)
{
    return QString::number(v, 'g', 5);
}

QVector<double> column(const json& points, const char* key)
{
    QVector<double> out;
    for (const json& p : points)
        out.push_back(p.is_array() ? p[key[0] == '0' ? 0 : 1].get<double>() : p.value(key, std::nan("")));
    return out;
}

void fill(QTableWidget* t, int row, const QStringList& cells)
{
    for (int c = 0; c < cells.size(); ++c)
        t->setItem(row, c, new QTableWidgetItem(cells[c]));
}

}  // namespace

MatterPanel::MatterPanel(QWidget* parent) : QWidget(parent), ui_(std::make_unique<Ui::MatterPanel>())
{
    ui_->setupUi(this);
    for (const auto& [id, label] : std::vector<std::pair<QString, QString>>{{"laminate", tr("Lamination theory")},
                                                                           {"modes", tr("Bending modes (coated and bare)")},
                                                                           {"harvester", tr("Piezoelectric cantilever")},
                                                                           {"thermal", tr("Through-thickness conduction")},
                                                                           {"electrical", tr("Through-thickness electrical")},
                                                                           {"electromagnetic", tr("Layered plane-wave solver")},
                                                                           {"interfaces", tr("Interface quantities")},
                                                                           {"layered-solid", tr("Three-dimensional layered solid")},
                                                                           {"layered-solid-thermal", tr("Three-dimensional layered conduction")}})
        ui_->analysisCombo->addItem(label, id);
    for (const char* d : {"mechanical", "electrical", "thermal", "electromagnetic", "acoustic", "magnetic", "optical", "chemical", "fluid"}) {
        ui_->fromCombo->addItem(QString::fromLatin1(d));
        ui_->toCombo->addItem(QString::fromLatin1(d));
    }
    ui_->toCombo->setCurrentIndex(1);
    for (QTableWidget* t : {ui_->regionTable, ui_->interfaceTable, ui_->manufactureTable, ui_->compareTable}) {
        t->horizontalHeader()->setStretchLastSection(true);
        t->verticalHeader()->setVisible(false);
    }
    ui_->regionTable->setColumnCount(7);
    ui_->regionTable->setHorizontalHeaderLabels({tr("Region"), tr("Role"), tr("Object"), tr("Extent"), tr("Made of"), tr("Process"), tr("Anisotropy")});
    ui_->interfaceTable->setColumnCount(3);
    ui_->interfaceTable->setHorizontalHeaderLabels({tr("Interface"), tr("Between"), tr("Models")});
    ui_->manufactureTable->setColumnCount(3);
    ui_->manufactureTable->setHorizontalHeaderLabels({tr("Kind"), tr("Element"), tr("Message")});
    setEnabled(false);
}

MatterPanel::~MatterPanel() = default;

void MatterPanel::setController(MatterController* controller)
{
    controller_ = controller;
    setEnabled(controller_ != nullptr);
    connect(controller_, &MatterController::modelChanged, this, &MatterPanel::refresh);
    connect(ui_->architectureCombo, &QComboBox::currentIndexChanged, this, &MatterPanel::showArchitecture);
    connect(ui_->stackCombo, &QComboBox::currentIndexChanged, this, &MatterPanel::showStack);
    connect(ui_->runButton, &QPushButton::clicked, this, [this] {
        json options = json::object();
        if (!ui_->optionsEdit->text().trimmed().isEmpty()) {
            try {
                options = json::parse(ui_->optionsEdit->text().toStdString());
            } catch (const std::exception& e) {
                ui_->resultText->setPlainText(tr("The options are not JSON: %1").arg(QString::fromUtf8(e.what())));
                return;
            }
        }
        const QString analysis = ui_->analysisCombo->currentData().toString();
        if (analysis == QLatin1String("harvester") && !options.contains("sweep"))
            options["sweep"] = {{"points", 201}};
        auto r = controller_->analyze(currentId(), ui_->stackCombo->currentData().toString(), analysis, options);
        ui_->resultPlot->clear();
        if (!r) {
            ui_->resultText->setPlainText(tr("Not run: %1").arg(QString::fromStdString(r.message())));
            return;
        }
        const json& res = r.value();
        QStringList lines{tr("%1 - %2").arg(ui_->analysisCombo->currentText(), text(res, "representation"))};
        for (const auto& [k, v] : res["metrics"].items())
            if (v.is_number())
                lines << QStringLiteral("  %1 = %2").arg(QString::fromStdString(k), number(v.get<double>()));
        lines << tr("Assumptions:");
        for (const json& a : res.value("assumptions", json::array()))
            lines << QStringLiteral("  - %1").arg(QString::fromStdString(a.get<std::string>()));
        lines << tr("Warnings:");
        for (const json& w : res.value("warnings", json::array()))
            lines << QStringLiteral("  ! %1").arg(QString::fromStdString(w.get<std::string>()));
        ui_->resultText->setPlainText(lines.join(QLatin1Char('\n')));
        const json& result = res["result"];
        if (analysis == QLatin1String("electromagnetic") && result.contains("points")) {
            ui_->resultPlot->setCurve(column(result["points"], "frequency"), column(result["points"], "absorptance"), tr("Absorptance"), tr("Frequency (Hz)"), tr("A"));
            ui_->resultPlot->addCurve(column(result["points"], "frequency"), column(result["points"], "reflectance"), tr("reflectance"), QColor(40, 110, 200));
        } else if (analysis == QLatin1String("thermal") && !result.value("profile", json::array()).empty()) {
            ui_->resultPlot->setCurve(column(result["profile"], "0"), column(result["profile"], "1"), tr("Temperature through the stack"), tr("Height (m)"), tr("T (K)"));
        } else if (analysis == QLatin1String("harvester") && !result.value("sweep", json::array()).empty()) {
            ui_->resultPlot->setCurve(column(result["sweep"], "frequency"), column(result["sweep"], "power"), tr("Harvested power"), tr("Frequency (Hz)"), tr("P (W)"));
        } else if (analysis == QLatin1String("layered-solid") && result.contains("frequencies")) {
            QVector<double> n, f;
            for (size_t k = 0; k < result["frequencies"].size(); ++k) {
                n.push_back(static_cast<double>(k + 1));
                f.push_back(result["frequencies"][k].get<double>());
            }
            ui_->resultPlot->setCurve(n, f, tr("Modes of the three-dimensional body"), tr("Mode"), tr("f (Hz)"));
        } else if (analysis == QLatin1String("modes") && result.contains("coated")) {
            QVector<double> n, coated, bare;
            for (size_t k = 0; k < result["coated"].size(); ++k) {
                n.push_back(static_cast<double>(k + 1));
                coated.push_back(result["coated"][k].get<double>());
                bare.push_back(result["bare"][k].get<double>());
            }
            ui_->resultPlot->setCurve(n, coated, tr("Coated and bare modes"), tr("Mode"), tr("f (Hz)"));
            ui_->resultPlot->addCurve(n, bare, tr("bare"), QColor(120, 120, 120), true);
        }
    });
    connect(ui_->manufactureButton, &QPushButton::clicked, this, [this] {
        auto r = controller_->manufacturability(currentId());
        ui_->manufactureTable->setRowCount(0);
        if (!r) {
            ui_->manufactureStatus->setText(QString::fromStdString(r.message()));
            return;
        }
        const QString status = text(r.value(), "status");
        ui_->manufactureStatus->setText(status == QLatin1String("feasible") ? tr("Feasible within the stated constraints (not a validation)")
                                        : status == QLatin1String("infeasible") ? tr("Infeasible: the stated constraints are violated")
                                                                                 : tr("Not assessed: limits or processes are missing, or the architecture is idealized"));
        int row = 0;
        for (const char* group : {"violations", "unassessed", "notes"})
            for (const json& i : r.value()[group]) {
                ui_->manufactureTable->setRowCount(row + 1);
                fill(ui_->manufactureTable, row++, {QString::fromLatin1(group), text(i, "element"), text(i, "message")});
            }
    });
    connect(ui_->exploreButton, &QPushButton::clicked, this, [this] {
        auto r = controller_->explore(currentId());
        if (!r) {
            ui_->exploreText->setPlainText(QString::fromStdString(r.message()));
            return;
        }
        QStringList lines{tr("Findings (from the records and computations):")};
        for (const json& f : r.value()["findings"])
            lines << QStringLiteral("  - %1 [%2]").arg(text(f, "statement"), text(f, "basis"));
        lines << tr("Hypotheses (to test, not results):");
        for (const json& h : r.value()["hypotheses"])
            lines << QStringLiteral("  - %1").arg(text(h, "statement"));
        lines << tr("Missing:");
        for (const json& m : r.value()["missing"])
            lines << QStringLiteral("  - %1").arg(QString::fromStdString(m.get<std::string>()));
        lines << tr("Suggested next steps:");
        for (const json& s : r.value()["suggestions"])
            lines << QStringLiteral("  - %1").arg(QString::fromStdString(s.get<std::string>()));
        lines << text(r.value(), "disclaimer");
        ui_->exploreText->setPlainText(lines.join(QLatin1Char('\n')));
    });
    connect(ui_->mechanismsButton, &QPushButton::clicked, this, [this] {
        auto r = controller_->mechanisms(ui_->fromCombo->currentText(), ui_->toCombo->currentText(), currentId());
        if (!r) {
            ui_->exploreText->setPlainText(QString::fromStdString(r.message()));
            return;
        }
        QStringList lines{tr("From %1 to %2:").arg(ui_->fromCombo->currentText(), ui_->toCombo->currentText())};
        for (const json& m : r.value()["direct"])
            lines << QStringLiteral("  - %1: %2 (%3)").arg(text(m, "title"), text(m, "status"), text(m, "note"));
        lines << tr("Two-stage cascades:");
        for (const json& c : r.value()["cascades"])
            lines << QStringLiteral("  - %1 then %2 (through %3): %4")
                         .arg(text(c["stages"][0], "title"), text(c["stages"][1], "title"), text(c, "through"),
                              c["implemented"].get<bool>() ? tr("modeled") : tr("not modeled"));
        lines << text(r.value(), "note");
        ui_->exploreText->setPlainText(lines.join(QLatin1Char('\n')));
    });
    connect(ui_->duplicateButton, &QPushButton::clicked, this, [this] {
        const QString base = currentId();
        QString id = base + QStringLiteral("-alt");
        for (int k = 2; ui_->architectureCombo->findData(id) >= 0; ++k)
            id = base + QStringLiteral("-alt%1").arg(k);
        auto r = controller_->duplicate(base, id);
        ui_->statusLabel->setText(r ? tr("Created the alternative %1 (undo removes it)").arg(id) : tr("Not duplicated: %1").arg(QString::fromStdString(r.message())));
        if (r)
            selectArchitecture(id);
    });
    connect(ui_->compareButton, &QPushButton::clicked, this, [this] {
        json ids = json::array();
        for (int i = 0; i < ui_->architectureCombo->count(); ++i)
            ids.push_back(ui_->architectureCombo->itemData(i).toString().toStdString());
        auto r = controller_->compare(ids);
        ui_->compareTable->clear();
        ui_->compareTable->setRowCount(0);
        if (!r) {
            ui_->statusLabel->setText(tr("Not compared: %1").arg(QString::fromStdString(r.message())));
            return;
        }
        QStringList headers{tr("Metric")};
        for (const json& id : ids)
            headers << QString::fromStdString(id.get<std::string>());
        ui_->compareTable->setColumnCount(static_cast<int>(headers.size()));
        ui_->compareTable->setHorizontalHeaderLabels(headers);
        int row = 0;
        for (const auto& [metric, values] : r.value()["metrics"].items()) {
            ui_->compareTable->setRowCount(row + 1);
            ui_->compareTable->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(metric)));
            for (size_t c = 0; c < ids.size(); ++c) {
                const std::string id = ids[c].get<std::string>();
                if (!values.contains(id))
                    continue;
                const json& v = values[id];
                ui_->compareTable->setItem(row, static_cast<int>(c) + 1,
                                           new QTableWidgetItem(v.is_number() ? number(v.get<double>()) : QString::fromStdString(v.is_string() ? v.get<std::string>() : v.dump())));
            }
            ++row;
        }
        ui_->statusLabel->setText(text(r.value(), "note"));
    });
    connect(ui_->systemButton, &QPushButton::clicked, this, [this] {
        auto r = controller_->createPhysicalSystem(currentId(), ui_->stackCombo->currentData().toString());
        ui_->statusLabel->setText(r ? tr("Added the physical system %1 to the Coupled Energy-Matter Engine (undo removes it)").arg(text(r.value()["system"], "id"))
                                    : tr("Not created: %1").arg(QString::fromStdString(r.message())));
    });
    refresh();
}

QString MatterPanel::currentId() const
{
    return ui_->architectureCombo->currentData().toString();
}

void MatterPanel::refresh()
{
    if (!controller_)
        return;
    const QSignalBlocker b(ui_->architectureCombo);
    const QString current = currentId();
    ui_->architectureCombo->clear();
    for (const json& a : controller_->architectures())
        ui_->architectureCombo->addItem(QStringLiteral("%1 (%2)").arg(text(a, "name"), text(a, "representation")), text(a, "id"));
    if (const int i = ui_->architectureCombo->findData(current); i >= 0)
        ui_->architectureCombo->setCurrentIndex(i);
    showArchitecture();
}

void MatterPanel::showArchitecture()
{
    current_ = controller_ ? controller_->architecture(currentId()) : json::object();
    ui_->regionTable->setRowCount(0);
    if (current_.empty()) {
        ui_->statusLabel->setText(tr("The project has no architecture."));
        ui_->stackView->setStack(json::array(), json::array());
        return;
    }
    const QString rep = text(current_, "representation");
    int errors = 0;
    for (const json& i : current_.value("issues", json::array()))
        errors += i.value("severity", std::string()) == "error" ? 1 : 0;
    ui_->statusLabel->setText(tr("%1 - %2 architecture, revision %3%4")
                                  .arg(text(current_, "name"), rep, text(current_, "revision"),
                                       errors > 0 ? tr("; %1 validation errors").arg(errors) : QString()));
    const json regions = current_.value("regions", json::array());
    ui_->regionTable->setRowCount(static_cast<int>(regions.size()));
    for (size_t i = 0; i < regions.size(); ++i) {
        const json& r = regions[i];
        QString made = text(r, "materialId");
        const json comp = r.value("composition", json::object());
        if (!comp.value("constituents", json::array()).empty() && comp.value("constituents", json::array()).size() > 1)
            made = tr("composition (%1)").arg(comp.value("method", std::string()).empty() ? tr("no method") : QString::fromStdString(comp["method"].get<std::string>()));
        QStringList aniso;
        const json sets = r.value("anisotropic", json::object());
        for (const auto& [k, v] : sets.items())
            aniso << QString::fromStdString(k);
        fill(ui_->regionTable, static_cast<int>(i), {text(r, "id"), text(r, "role"), text(r, "object"), text(r, "extent"), made, text(r, "process"), aniso.join(QStringLiteral(", "))});
    }
    {
        const QSignalBlocker sb(ui_->stackCombo);
        ui_->stackCombo->clear();
        for (const json& s : current_.value("stacks", json::array()))
            ui_->stackCombo->addItem(text(s, "name").isEmpty() ? text(s, "id") : text(s, "name"), text(s, "id"));
    }
    showStack();
}

void MatterPanel::showStack()
{
    const QString sid = ui_->stackCombo->currentData().toString();
    json stack;
    for (const json& s : current_.value("stacks", json::array()))
        if (text(s, "id") == sid)
            stack = s;
    json layers = json::array(), interfaces = json::array();
    ui_->interfaceTable->setRowCount(0);
    if (stack.is_null()) {
        ui_->stackView->setStack(layers, interfaces);
        return;
    }
    auto roleOf = [this](const std::string& region) {
        for (const json& r : current_.value("regions", json::array()))
            if (r.value("id", std::string()) == region)
                return r.value("role", std::string());
        return std::string();
    };
    const json ls = stack.value("layers", json::array());
    for (size_t i = 0; i < ls.size(); ++i) {
        const json& l = ls[i];
        const json pattern = l.value("pattern", json::object());
        layers.push_back({{"id", l.value("id", std::string())},
                          {"region", l.value("region", std::string())},
                          {"role", roleOf(l.value("region", std::string()))},
                          {"thickness", l.value("thickness", 0.0)},
                          {"pattern", pattern.value("kind", std::string("uniform"))},
                          {"coverage", pattern.value("end", 1.0) - pattern.value("start", 0.0)}});
        if (i + 1 >= ls.size() || ls[i + 1].value("region", std::string()) == l.value("region", std::string()))
            continue;
        const std::string a = l.value("region", std::string()), b = ls[i + 1].value("region", std::string());
        json found;
        for (const json& in : current_.value("interfaces", json::array())) {
            const json rr = in.value("regions", json::array());
            if (rr.size() == 2 && ((rr[0] == a && rr[1] == b) || (rr[0] == b && rr[1] == a)))
                found = in;
        }
        QStringList models;
        std::string first;
        if (!found.is_null())
            for (const json& m : found.value("models", json::array())) {
                QString t = QString::fromStdString(m.value("kind", std::string()));
                const json parameters = m.value("parameters", json::object());
                for (const auto& [k, v] : parameters.items())
                    t += QStringLiteral(" %1 = %2 %3").arg(QString::fromStdString(k), number(v.value("value", 0.0)), QString::fromStdString(v.value("unit", std::string())));
                models << t;
                if (first.empty() || m.value("kind", std::string()) != "perfect")
                    first = m.value("kind", std::string());
            }
        interfaces.push_back({{"below", static_cast<int>(i)}, {"model", first}});
        const int row = ui_->interfaceTable->rowCount();
        ui_->interfaceTable->setRowCount(row + 1);
        fill(ui_->interfaceTable, row,
             {found.is_null() ? tr("(none declared)") : QString::fromStdString(found.value("id", std::string())), QString::fromStdString(a + " / " + b),
              models.isEmpty() ? tr("nothing is assumed about it") : models.join(QStringLiteral("; "))});
    }
    ui_->stackView->setStack(layers, interfaces);
}

int MatterPanel::architectureCount() const { return ui_->architectureCombo->count(); }
void MatterPanel::selectArchitecture(const QString& id) { ui_->architectureCombo->setCurrentIndex(ui_->architectureCombo->findData(id)); }
void MatterPanel::selectAnalysis(const QString& id) { ui_->analysisCombo->setCurrentIndex(ui_->analysisCombo->findData(id)); }
void MatterPanel::setOptions(const QString& options) { ui_->optionsEdit->setText(options); }
int MatterPanel::regionRows() const { return ui_->regionTable->rowCount(); }
int MatterPanel::interfaceRows() const { return ui_->interfaceTable->rowCount(); }
StackView* MatterPanel::stackView() const { return ui_->stackView; }
QPushButton* MatterPanel::runButton() const { return ui_->runButton; }
QPushButton* MatterPanel::manufactureButton() const { return ui_->manufactureButton; }
QPushButton* MatterPanel::exploreButton() const { return ui_->exploreButton; }
QPushButton* MatterPanel::mechanismsButton() const { return ui_->mechanismsButton; }
QPushButton* MatterPanel::duplicateButton() const { return ui_->duplicateButton; }
QPushButton* MatterPanel::compareButton() const { return ui_->compareButton; }
QPushButton* MatterPanel::systemButton() const { return ui_->systemButton; }
QString MatterPanel::resultText() const { return ui_->resultText->toPlainText(); }
int MatterPanel::plottedPoints() const { return ui_->resultPlot->pointCount(); }
QString MatterPanel::manufactureStatus() const { return ui_->manufactureStatus->text(); }
int MatterPanel::manufactureRows() const { return ui_->manufactureTable->rowCount(); }
QString MatterPanel::exploreText() const { return ui_->exploreText->toPlainText(); }
int MatterPanel::compareRows() const { return ui_->compareTable->rowCount(); }
int MatterPanel::compareColumns() const { return ui_->compareTable->columnCount(); }
QString MatterPanel::statusText() const { return ui_->statusLabel->text(); }
void MatterPanel::setMechanismDomains(const QString& from, const QString& to)
{
    ui_->fromCombo->setCurrentText(from);
    ui_->toCombo->setCurrentText(to);
}
