#include "LaboratoryPanel.h"

#include "LaboratoryController.h"
#include "PlotWidget.h"

#include "ui_LaboratoryPanel.h"

#include <QCheckBox>
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

QVector<double> vec(const json& a)
{
    QVector<double> out;
    if (a.is_array())
        for (const json& x : a)
            out.push_back(x.is_number() ? x.get<double>() : std::nan(""));
    return out;
}

}  // namespace

LaboratoryPanel::LaboratoryPanel(QWidget* parent) : QWidget(parent), ui_(std::make_unique<Ui::LaboratoryPanel>())
{
    ui_->setupUi(this);
    ui_->kindCombo->addItem(tr("Frequency response"), QStringLiteral("response"));
    ui_->kindCombo->addItem(tr("Transient"), QStringLiteral("transient"));
    ui_->kindCombo->addItem(tr("Resonance"), QStringLiteral("resonance"));
    ui_->observableCombo->addItem(tr("Magnitude"), QStringLiteral("magnitude"));
    ui_->observableCombo->addItem(tr("Real and imaginary parts"), QStringLiteral("real-imag"));
    ui_->observableCombo->addItem(tr("Values"), QStringLiteral("value"));
    for (QTableWidget* t : {ui_->issuesTable, ui_->parameterTable, ui_->estimateTable}) {
        t->horizontalHeader()->setStretchLastSection(true);
        t->verticalHeader()->setVisible(false);
    }
    ui_->issuesTable->setColumnCount(5);
    ui_->issuesTable->setHorizontalHeaderLabels({tr("Severity"), tr("Code"), tr("Row"), tr("Column"), tr("Message")});
    ui_->parameterTable->setColumnCount(5);
    ui_->parameterTable->setHorizontalHeaderLabels({tr("Estimate"), tr("Parameter"), tr("Initial"), tr("Lower"), tr("Upper")});
    ui_->estimateTable->setColumnCount(5);
    ui_->estimateTable->setHorizontalHeaderLabels({tr("Parameter"), tr("Estimate"), tr("95 % interval"), tr("Unit"), tr("Identifiability")});
    setEnabled(false);
}

LaboratoryPanel::~LaboratoryPanel() = default;

void LaboratoryPanel::setController(LaboratoryController* controller)
{
    controller_ = controller;
    setEnabled(controller_ != nullptr);
    connect(controller_, &LaboratoryController::modelChanged, this, &LaboratoryPanel::refresh);
    connect(ui_->studyCombo, &QComboBox::currentIndexChanged, this, [this] {
        showStudy();
        fillParameters();
    });
    connect(ui_->datasetList, &QListWidget::currentRowChanged, this, &LaboratoryPanel::showDataset);
    connect(ui_->importStudyButton, &QPushButton::clicked, this, [this] {
        const json st = currentStudy();
        QStringList done;
        json lastReport;
        for (const json& f : st.value("conditions", json::object()).value("files", json::array())) {
            auto r = controller_->importFile(text(f, "path"), text(st, "id"), text(f, "role"));
            if (!r) {
                ui_->statusLabel->setText(tr("Not imported: %1").arg(QString::fromStdString(r.message())));
                return;
            }
            lastReport = r.value();
            done << tr("%1 (%2)").arg(text(f, "path"), r.value().value("accepted", false) ? tr("accepted") : tr("not accepted: see the report"));
        }
        ui_->statusLabel->setText(done.isEmpty() ? tr("The study lists no files.") : tr("Imported: %1").arg(done.join(QStringLiteral(", "))));
        const json issues = lastReport.value("issues", json::array());
        ui_->issuesTable->setRowCount(static_cast<int>(issues.size()));
        for (size_t r = 0; r < issues.size(); ++r) {
            const json& i = issues[r];
            const QStringList cells{text(i, "severity"), text(i, "code"), i.contains("row") ? QString::number(i["row"].get<long>()) : QString(), text(i, "column"),
                                    text(i, "message")};
            for (int c = 0; c < cells.size(); ++c)
                ui_->issuesTable->setItem(static_cast<int>(r), c, new QTableWidgetItem(cells[c]));
        }
    });
    connect(ui_->compareButton, &QPushButton::clicked, this, [this] {
        const json st = currentStudy();
        const json model = modelSpec();
        std::vector<std::string> measured = datasetsWithRole("calibration");
        if (measured.empty())
            measured = datasetsWithRole("raw");
        if (measured.empty()) {
            ui_->verdictLabel->setText(tr("Import the study's data first."));
            return;
        }
        const QString kind = ui_->kindCombo->currentData().toString();
        json predicted = kind == QLatin1String("resonance") ? st["conditions"].value("predicted", json::object())
                                                            : json{{"model", ui_->modelCombo->currentData().toString().toStdString()},
                                                                   {"configuration", model.value("configuration", json::object())}};
        auto r = controller_->compare({{"study", st.value("id", std::string())}, {"kind", kind.toStdString()}, {"measured", measured.front()}, {"predicted", predicted}});
        if (!r) {
            ui_->verdictLabel->setText(tr("Not compared: %1").arg(QString::fromStdString(r.message())));
            return;
        }
        lastRecord_ = text(r.value(), "id");
        ui_->verdictLabel->setText(tr("%1 — %2").arg(text(r.value(), "verdict"), QString::fromStdString(r.value().value("notes", json::array()).dump())));
        const json res = r.value().value("result", json::object());
        ui_->comparePlot->clear();
        ui_->residualPlot->clear();
        if (res.contains("measuredMagnitude")) {
            ui_->comparePlot->setCurve(vec(res["frequencyHz"]), vec(res["measuredMagnitude"]), tr("Measured and predicted"), tr("Frequency (Hz)"), tr("|response|"));
            ui_->comparePlot->addCurve(vec(res["frequencyHz"]), vec(res["predictedMagnitude"]), tr("predicted"), QColor(200, 80, 40));
            ui_->residualPlot->setCurve(vec(res["frequencyHz"]), vec(res["residual"]), tr("Residual"), tr("Frequency (Hz)"), tr("measured - predicted"));
        } else if (res.contains("times")) {
            ui_->comparePlot->setCurve(vec(res["times"]), vec(res["measured"]), tr("Measured and predicted"), tr("Time (s)"), text(res, "unit"));
            ui_->comparePlot->addCurve(vec(res["times"]), vec(res["predicted"]), tr("predicted"), QColor(200, 80, 40));
            ui_->residualPlot->setCurve(vec(res["times"]), vec(res["residual"]), tr("Residual"), tr("Time (s)"), tr("measured - predicted"));
        }
    });
    connect(ui_->calibrateButton, &QPushButton::clicked, this, [this] {
        const json st = currentStudy();
        const json model = modelSpec();
        json params = json::array();
        for (int r = 0; r < ui_->parameterTable->rowCount(); ++r) {
            auto* box = qobject_cast<QCheckBox*>(ui_->parameterTable->cellWidget(r, 0));
            if (!box || !box->isChecked())
                continue;
            params.push_back({{"id", ui_->parameterTable->item(r, 1)->data(Qt::UserRole).toString().toStdString()},
                              {"initial", ui_->parameterTable->item(r, 2)->text().toDouble()},
                              {"lower", ui_->parameterTable->item(r, 3)->text().toDouble()},
                              {"upper", ui_->parameterTable->item(r, 4)->text().toDouble()}});
        }
        const auto chosen = datasetsWithRole("calibration");
        if (chosen.empty() || params.empty()) {
            ui_->calibrationText->setPlainText(tr("A calibration needs the study's calibration data (imported) and at least one ticked parameter."));
            return;
        }
        json spec{{"study", st.value("id", std::string())}, {"model", model.value("id", std::string())}, {"configuration", model.value("configuration", json::object())},
                  {"parameters", params}, {"datasets", chosen}, {"observable", ui_->observableCombo->currentData().toString().toStdString()}};
        if (st["conditions"].contains("axisColumn")) {
            // Stress against a measured strain path: the axis dataset of the same import.
            json axes = json::array();
            for (const std::string& d : chosen) {
                std::string a = d;
                const auto pos = a.rfind("-" + st["conditions"].value("valueColumn", std::string("stress")) + "-");
                if (pos != std::string::npos)
                    a.replace(pos + 1, st["conditions"].value("valueColumn", std::string("stress")).size(), st["conditions"]["axisColumn"].get<std::string>());
                axes.push_back(a);
            }
            spec["axisDatasets"] = axes;
        }
        auto job = controller_->calibrate(spec);
        ui_->calibrationText->setPlainText(job ? tr("Calibrating…") : tr("Not started: %1").arg(QString::fromStdString(job.message())));
        ui_->calibrateButton->setEnabled(!job);
    });
    connect(controller_, &LaboratoryController::calibrationReady, this, [this](const QString& resultId, const json& r) {
        resultId_ = resultId;
        lastCalibration_ = r;
        ui_->calibrateButton->setEnabled(true);
        ui_->keepButton->setEnabled(true);
        const json params = r.value("result", json::object()).value("parameters", json::array());
        ui_->estimateTable->setRowCount(static_cast<int>(params.size()));
        for (size_t i = 0; i < params.size(); ++i) {
            const json& p = params[i];
            const QStringList cells{text(p, "id"), number(p.value("estimate", 0.0)),
                                    QStringLiteral("[%1, %2]").arg(number(p.value("lower", 0.0)), number(p.value("upper", 0.0))), text(p, "unit"),
                                    text(p, "identifiability")};
            for (int c = 0; c < cells.size(); ++c)
                ui_->estimateTable->setItem(static_cast<int>(i), c, new QTableWidgetItem(cells[c]));
        }
        QStringList lines{tr("Status: %1 (%2)").arg(text(r, "status"), text(r, "method")),
                          tr("Reduced chi-square %1, condition number %2").arg(number(r["result"].value("reducedChiSquare", 0.0)), number(r["result"].value("conditionNumber", 0.0)))};
        for (const json& n : r.value("notes", json::array()))
            lines << QString::fromStdString(n.get<std::string>());
        ui_->calibrationText->setPlainText(lines.join(QLatin1Char('\n')));
    });
    connect(controller_, &LaboratoryController::calibrationFailed, this, [this](const QString& message) {
        ui_->calibrateButton->setEnabled(true);
        ui_->calibrationText->setPlainText(tr("The calibration failed: %1").arg(message));
    });
    connect(ui_->keepButton, &QPushButton::clicked, this, [this] {
        auto r = controller_->keep(resultId_);
        if (!r) {
            ui_->statusLabel->setText(tr("Not kept: %1").arg(QString::fromStdString(r.message())));
            return;
        }
        calibrationId_ = text(r.value(), "id");
        lastRecord_ = calibrationId_;
        ui_->keepButton->setEnabled(false);
        ui_->validateButton->setEnabled(true);
        ui_->applyButton->setEnabled(true);
        ui_->statusLabel->setText(tr("Kept the calibration %1 in the project. Undo removes it.").arg(calibrationId_));
    });
    connect(ui_->validateButton, &QPushButton::clicked, this, [this] {
        const auto independent = datasetsWithRole("validation");
        json axes = json::array();
        if (lastCalibration_.contains("axisDatasets") && !lastCalibration_["axisDatasets"].empty())
            for (const std::string& d : independent) {
                const json st = currentStudy();
                std::string a = d;
                const std::string valueCol = st["conditions"].value("valueColumn", std::string("stress"));
                const auto pos = a.rfind("-" + valueCol + "-");
                if (pos != std::string::npos)
                    a.replace(pos + 1, valueCol.size(), st["conditions"].value("axisColumn", std::string("strain")));
                axes.push_back(a);
            }
        auto r = controller_->validate(calibrationId_, independent, axes);
        ui_->statusLabel->setText(r ? tr("Validation: %1").arg(text(r.value(), "status")) : tr("Not validated: %1").arg(QString::fromStdString(r.message())));
        if (r)
            lastRecord_ = text(r.value(), "id");
    });
    connect(ui_->applyButton, &QPushButton::clicked, this, [this] {
        const json st = currentStudy();
        json options = json::object();
        for (const json& m : st.value("models", json::array()))
            if (m.value("kind", std::string()) == "material-state-study")
                options["study"] = m.value("id", std::string());
        auto r = controller_->apply(calibrationId_, options);
        ui_->statusLabel->setText(r ? tr("Applied as a new revision: %1 (the original is unchanged; undo removes it)").arg(QString::fromStdString(r.value().dump()))
                                    : tr("Not applied: %1").arg(QString::fromStdString(r.message())));
    });
    connect(ui_->reportButton, &QPushButton::clicked, this, [this] {
        auto r = controller_->report(QString::fromStdString(currentStudy().value("id", std::string())), QStringLiteral("markdown"));
        if (r)
            ui_->reportView->setMarkdown(r.value());
        else
            ui_->reportView->setPlainText(QString::fromStdString(r.message()));
    });
    connect(ui_->explainButton, &QPushButton::clicked, this, [this] {
        if (lastRecord_.isEmpty()) {
            ui_->reportView->setPlainText(tr("Compare or calibrate first: the explanation is of a record."));
            return;
        }
        auto r = controller_->explain(lastRecord_);
        if (!r) {
            ui_->reportView->setPlainText(QString::fromStdString(r.message()));
            return;
        }
        QStringList lines{tr("Findings (from the records):")};
        for (const json& f : r.value()["findings"])
            lines << QStringLiteral("  • %1").arg(text(f, "statement"));
        lines << tr("Hypotheses (not findings):");
        for (const json& h : r.value()["hypotheses"])
            lines << QStringLiteral("  • %1 — %2").arg(text(h, "statement"), text(h, "basis"));
        lines << tr("Missing information:");
        for (const json& m : r.value()["missing"])
            lines << QStringLiteral("  • %1").arg(QString::fromStdString(m.get<std::string>()));
        lines << tr("Suggested measurements:");
        for (const json& s : r.value()["suggestions"])
            lines << QStringLiteral("  • %1").arg(QString::fromStdString(s.get<std::string>()));
        lines << text(r.value(), "disclaimer");
        ui_->reportView->setPlainText(lines.join(QLatin1Char('\n')));
    });
    {
        const QSignalBlocker b(ui_->modelCombo);
        for (const json& m : controller_->models())
            ui_->modelCombo->addItem(text(m, "title"), text(m, "id"));
    }
    refresh();
}

json LaboratoryPanel::currentStudy() const
{
    const QString id = ui_->studyCombo->currentData().toString();
    for (const json& s : controller_->studies())
        if (QString::fromStdString(s.value("id", std::string())) == id)
            return s;
    return json::object();
}

json LaboratoryPanel::modelSpec() const
{
    return currentStudy().value("conditions", json::object()).value("model", json::object());
}

std::vector<std::string> LaboratoryPanel::datasetsWithRole(const std::string& role) const
{
    std::vector<std::string> out;
    const std::string column = currentStudy().value("conditions", json::object()).value("valueColumn", std::string());
    for (const json& d : currentStudy().value("datasets", json::array()))
        if (d.value("role", std::string()) == role) {
            const std::string id = d.value("measurement", std::string());
            if (column.empty() || id.find("-" + column + "-") != std::string::npos)
                out.push_back(id);
        }
    return out;
}

void LaboratoryPanel::refresh()
{
    if (!controller_)
        return;
    {
        const QSignalBlocker b(ui_->studyCombo);
        const QString current = ui_->studyCombo->currentData().toString();
        ui_->studyCombo->clear();
        for (const json& s : controller_->studies())
            ui_->studyCombo->addItem(text(s, "name").isEmpty() ? text(s, "id") : text(s, "name"), text(s, "id"));
        if (const int i = ui_->studyCombo->findData(current); i >= 0)
            ui_->studyCombo->setCurrentIndex(i);
    }
    {
        const QSignalBlocker b(ui_->datasetList);
        const int row = ui_->datasetList->currentRow();
        ui_->datasetList->clear();
        for (const json& d : controller_->datasets()) {
            auto* item = new QListWidgetItem(QStringLiteral("%1 — %2, %3 (%4)").arg(text(d, "id"), text(d, "quantity"), text(d, "category"), text(d, "provenance")),
                                             ui_->datasetList);
            item->setData(Qt::UserRole, text(d, "id"));
        }
        if (row >= 0 && row < ui_->datasetList->count())
            ui_->datasetList->setCurrentRow(row);
    }
    showStudy();
    fillParameters();
}

void LaboratoryPanel::showStudy()
{
    const json s = currentStudy();
    if (s.empty()) {
        ui_->studyText->setPlainText(tr("The project has no laboratory study."));
        return;
    }
    QStringList lines{tr("%1 — status: %2").arg(text(s, "name"), text(s, "status")), tr("Objective: %1").arg(text(s, "objective")),
                      tr("Hypothesis: %1").arg(text(s, "hypothesis")), tr("Specimen: %1").arg(text(s, "specimen")), tr("Models:")};
    for (const json& m : s.value("models", json::array()))
        lines << QStringLiteral("  • %1 %2 (revision %3)").arg(text(m, "kind"), text(m, "id"), text(m, "revision").isEmpty() ? tr("not recorded") : text(m, "revision"));
    for (const json& x : s.value("stale", json::array()))
        lines << tr("  ! stale: %1").arg(QString::fromStdString(x.get<std::string>()));
    lines << tr("Measured deviations from the nominal:");
    for (const json& d : s.value("deviations", json::array()))
        lines << QStringLiteral("  • %1: %2 against %3 %4 (± %5, %6)")
                     .arg(text(d, "parameter"), number(d.value("measured", 0.0)), number(d.value("nominal", 0.0)), text(d, "unit"), number(d.value("uncertainty", 0.0)),
                          text(d, "method"));
    lines << tr("Datasets:");
    for (const json& d : s.value("datasets", json::array()))
        lines << QStringLiteral("  • %1 (%2)").arg(text(d, "measurement"), text(d, "role"));
    ui_->studyText->setPlainText(lines.join(QLatin1Char('\n')));
}

void LaboratoryPanel::fillParameters()
{
    const json model = modelSpec();
    ui_->parameterTable->setRowCount(0);
    if (model.empty())
        return;
    if (const int i = ui_->modelCombo->findData(text(model, "id")); i >= 0)
        ui_->modelCombo->setCurrentIndex(i);
    const json params = controller_->modelParameters(QString::fromStdString(model.value("id", std::string())), model.value("configuration", json::object()));
    ui_->parameterTable->setRowCount(static_cast<int>(params.size()));
    for (size_t r = 0; r < params.size(); ++r) {
        const json& p = params[r];
        auto* box = new QCheckBox(ui_->parameterTable);
        ui_->parameterTable->setCellWidget(static_cast<int>(r), 0, box);
        auto* name = new QTableWidgetItem(QStringLiteral("%1 [%2]").arg(text(p, "label"), text(p, "unit")));
        name->setData(Qt::UserRole, text(p, "id"));
        name->setToolTip(text(p, "meaning"));
        ui_->parameterTable->setItem(static_cast<int>(r), 1, name);
        ui_->parameterTable->setItem(static_cast<int>(r), 2, new QTableWidgetItem(number(p.value("value", 0.0))));
        ui_->parameterTable->setItem(static_cast<int>(r), 3, new QTableWidgetItem(number(p.value("lower", 0.0))));
        ui_->parameterTable->setItem(static_cast<int>(r), 4, new QTableWidgetItem(number(p.value("upper", 0.0))));
    }
}

void LaboratoryPanel::showDataset()
{
    auto* item = ui_->datasetList->currentItem();
    if (!item) {
        ui_->dataPlot->clear();
        return;
    }
    const json d = controller_->dataset(item->data(Qt::UserRole).toString(), 2000);
    const json& values = d["data"];
    QVector<double> x = vec(values["axis"]), y;
    if (values.contains("complexReal")) {
        const auto re = vec(values["complexReal"]), im = vec(values["complexImag"]);
        for (int k = 0; k < re.size(); ++k)
            y.push_back(std::hypot(re[k], im[k]));
    } else {
        y = vec(values["real"]);
    }
    if (x.size() != y.size()) {
        x.clear();
        for (int k = 0; k < y.size(); ++k)
            x.push_back(k);
    }
    const json rec = d["record"];
    ui_->dataPlot->setLogX(false);
    ui_->dataPlot->setCurve(x, y, text(rec, "id"), tr("Axis"), QString::fromStdString(rec.value("quantity", json::object()).dump()));
    QStringList lines{tr("Category: %1; provenance: %2").arg(text(rec, "category"), QString::fromStdString(rec.value("acquisition", json::object()).value("provenance", std::string("not stated")))),
                      tr("Instrument: %1; calibration: %2").arg(text(rec, "deviceId").isEmpty() ? tr("not identified") : text(rec, "deviceId"),
                                                                text(rec, "calibrationId").isEmpty() ? tr("not stated") : text(rec, "calibrationId"))};
    for (const json& s : rec.value("acquisition", json::object()).value("importSteps", json::array()))
        lines << tr("Import: %1").arg(text(s, "description"));
    if (!text(rec, "note").isEmpty())
        lines << text(rec, "note");
    ui_->datasetText->setPlainText(lines.join(QLatin1Char('\n')));
}

int LaboratoryPanel::studyCount() const { return ui_->studyCombo->count(); }
void LaboratoryPanel::selectStudy(const QString& id) { ui_->studyCombo->setCurrentIndex(ui_->studyCombo->findData(id)); }
QPushButton* LaboratoryPanel::importStudyButton() const { return ui_->importStudyButton; }
QPushButton* LaboratoryPanel::compareButton() const { return ui_->compareButton; }
QPushButton* LaboratoryPanel::calibrateButton() const { return ui_->calibrateButton; }
QPushButton* LaboratoryPanel::keepButton() const { return ui_->keepButton; }
QPushButton* LaboratoryPanel::validateButton() const { return ui_->validateButton; }
QPushButton* LaboratoryPanel::reportButton() const { return ui_->reportButton; }
QPushButton* LaboratoryPanel::explainButton() const { return ui_->explainButton; }
int LaboratoryPanel::datasetCount() const { return ui_->datasetList->count(); }
void LaboratoryPanel::selectDataset(int row) { ui_->datasetList->setCurrentRow(row); }
int LaboratoryPanel::plottedPoints() const { return ui_->dataPlot->pointCount(); }
int LaboratoryPanel::issueRows() const { return ui_->issuesTable->rowCount(); }
QString LaboratoryPanel::studyText() const { return ui_->studyText->toPlainText(); }
QString LaboratoryPanel::verdictText() const { return ui_->verdictLabel->text(); }
int LaboratoryPanel::comparisonCurves() const { return ui_->comparePlot->curveCount(); }
int LaboratoryPanel::residualPoints() const { return ui_->residualPlot->pointCount(); }
void LaboratoryPanel::setParameterEnabled(const QString& id, bool enabled)
{
    for (int r = 0; r < ui_->parameterTable->rowCount(); ++r)
        if (ui_->parameterTable->item(r, 1)->data(Qt::UserRole).toString() == id)
            if (auto* box = qobject_cast<QCheckBox*>(ui_->parameterTable->cellWidget(r, 0)))
                box->setChecked(enabled);
}
int LaboratoryPanel::estimateRows() const { return ui_->estimateTable->rowCount(); }
QString LaboratoryPanel::estimate(const QString& id) const
{
    for (int r = 0; r < ui_->estimateTable->rowCount(); ++r)
        if (ui_->estimateTable->item(r, 0)->text() == id)
            return QStringLiteral("%1 %2 %3").arg(ui_->estimateTable->item(r, 1)->text(), ui_->estimateTable->item(r, 2)->text(), ui_->estimateTable->item(r, 4)->text());
    return {};
}
QString LaboratoryPanel::calibrationText() const { return ui_->calibrationText->toPlainText(); }
QString LaboratoryPanel::reportText() const { return ui_->reportView->toPlainText(); }
QString LaboratoryPanel::statusText() const { return ui_->statusLabel->text(); }
