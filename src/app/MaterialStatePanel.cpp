#include "MaterialStatePanel.h"

#include "MaterialStateController.h"
#include "PlotWidget.h"

#include "ui_MaterialStatePanel.h"

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
    return v.is_number() ? QString::number(v.get<double>(), 'g', 6) : v.is_boolean() ? (v.get<bool>() ? QStringLiteral("yes") : QStringLiteral("no"))
                                                                                       : QStringLiteral("—");
}

}  // namespace

MaterialStatePanel::MaterialStatePanel(QWidget* parent) : QWidget(parent), ui_(std::make_unique<Ui::MaterialStatePanel>())
{
    ui_->setupUi(this);
    ui_->historyTable->horizontalHeader()->setStretchLastSection(true);
    ui_->historyTable->verticalHeader()->setVisible(false);
    ui_->snapshotSpin->setEnabled(false);
    setEnabled(false);
}

MaterialStatePanel::~MaterialStatePanel() = default;

void MaterialStatePanel::setController(MaterialStateController* controller)
{
    controller_ = controller;
    setEnabled(controller_ != nullptr);
    connect(controller_, &MaterialStateController::modelChanged, this, &MaterialStatePanel::refresh);
    connect(controller_, &MaterialStateController::resultReady, this, [this](const QString& id) {
        showResult(id);
        ui_->runStudyButton->setEnabled(ui_->studyCombo->count() > 0);
        ui_->runModelButton->setEnabled(true);
    });
    connect(controller_, &MaterialStateController::runFailed, this, [this](const QString& message) {
        ui_->statusLabel->setText(tr("The run failed: %1").arg(message));
        ui_->runStudyButton->setEnabled(ui_->studyCombo->count() > 0);
        ui_->runModelButton->setEnabled(true);
    });
    const auto started = [this](const resonant::Result<QString>& job) {
        if (!job) {
            ui_->statusLabel->setText(QString::fromStdString(job.message()));
            return;
        }
        ui_->runStudyButton->setEnabled(false);
        ui_->runModelButton->setEnabled(false);
        ui_->saveButton->setEnabled(false);
        ui_->statusLabel->setText(tr("Running…"));
    };
    connect(ui_->runStudyButton, &QPushButton::clicked, this, [this, started] { started(controller_->run(ui_->studyCombo->currentData().toString(), json())); });
    connect(ui_->runModelButton, &QPushButton::clicked, this, [this, started] {
        json study{{"model", ui_->modelCombo->currentData().toString().toStdString()}};
        for (const json& m : controller_->models())
            if (m.value("id", "") == study["model"].get<std::string>())
                for (const json& p : m.value("loadings", json::array()))
                    if (QString::fromStdString(p.value("id", "")) == ui_->presetCombo->currentData().toString())
                        study["loading"] = p["loading"];
        started(controller_->run(QString(), study));
    });
    connect(ui_->modelCombo, &QComboBox::currentIndexChanged, this, [this] {
        if (!filling_)
            showCard();
    });
    connect(ui_->curveCombo, &QComboBox::currentIndexChanged, this, &MaterialStatePanel::plotCurve);
    connect(ui_->snapshotSpin, &QSpinBox::valueChanged, this, &MaterialStatePanel::plotCurve);
    connect(ui_->saveButton, &QPushButton::clicked, this, [this] {
        auto id = controller_->saveRecord(resultId_, text(result_, "title"));
        ui_->statusLabel->setText(id ? tr("Saved the record %1 in the project. Undo removes it.").arg(id.value())
                                     : tr("Not saved: %1").arg(QString::fromStdString(id.message())));
    });
    connect(ui_->commitButton, &QPushButton::clicked, this, [this] {
        const auto status = controller_->commit(ui_->recordCombo->currentData().toString(), ui_->targetCombo->currentData().toString());
        ui_->statusLabel->setText(status ? tr("Committed to '%1' (provenance computed, with its derivation). Undo reverts it.").arg(ui_->targetCombo->currentText())
                                         : tr("Not committed: %1").arg(QString::fromStdString(status.message())));
    });
    connect(ui_->recordCombo, &QComboBox::currentIndexChanged, this, [this] {
        bool committable = false;
        for (const json& r : controller_->records())
            if (QString::fromStdString(r.value("id", "")) == ui_->recordCombo->currentData().toString())
                committable = !r.value("committable", json::array()).empty();
        ui_->commitButton->setEnabled(committable);
    });
    refresh();
}

void MaterialStatePanel::refresh()
{
    if (!controller_)
        return;
    filling_ = true;
    {
        const QSignalBlocker b(ui_->studyCombo);
        const QString current = ui_->studyCombo->currentData().toString();
        ui_->studyCombo->clear();
        for (const json& s : controller_->studies())
            ui_->studyCombo->addItem(text(s, "name").isEmpty() ? text(s, "id") : text(s, "name"), text(s, "id"));
        if (const int i = ui_->studyCombo->findData(current); i >= 0)
            ui_->studyCombo->setCurrentIndex(i);
        ui_->runStudyButton->setEnabled(ui_->studyCombo->count() > 0 && !controller_->busy());
    }
    {
        const QSignalBlocker b(ui_->modelCombo);
        const QString current = ui_->modelCombo->currentData().toString();
        ui_->modelCombo->clear();
        for (const json& m : controller_->models())
            if (m.value("status", "") == "implemented" && !m.value("loadings", json::array()).empty())
                ui_->modelCombo->addItem(text(m, "title"), text(m, "id"));
        if (const int i = ui_->modelCombo->findData(current); i >= 0)
            ui_->modelCombo->setCurrentIndex(i);
    }
    {
        const QSignalBlocker a(ui_->recordCombo), b(ui_->targetCombo);
        const QString record = ui_->recordCombo->currentData().toString(), target = ui_->targetCombo->currentData().toString();
        ui_->recordCombo->clear();
        for (const json& r : controller_->records())
            ui_->recordCombo->addItem(text(r, "name"), text(r, "id"));
        ui_->targetCombo->clear();
        for (const json& m : controller_->materials())
            ui_->targetCombo->addItem(text(m, "name"), text(m, "id"));
        if (const int i = ui_->recordCombo->findData(record); i >= 0)
            ui_->recordCombo->setCurrentIndex(i);
        if (const int i = ui_->targetCombo->findData(target); i >= 0)
            ui_->targetCombo->setCurrentIndex(i);
    }
    filling_ = false;
    showCard();
    emit ui_->recordCombo->currentIndexChanged(ui_->recordCombo->currentIndex());
}

void MaterialStatePanel::showCard()
{
    const QString id = ui_->modelCombo->currentData().toString();
    json card;
    QStringList notImplemented;
    for (const json& m : controller_->models()) {
        if (QString::fromStdString(m.value("id", "")) == id)
            card = m;
        if (m.value("status", "") != "implemented")
            notImplemented << text(m, "title");
    }
    {
        const QSignalBlocker b(ui_->presetCombo);
        ui_->presetCombo->clear();
        for (const json& p : card.value("loadings", json::array()))
            ui_->presetCombo->addItem(text(p, "title"), text(p, "id"));
    }
    QStringList d;
    if (card.is_object()) {
        d << tr("%1 (%2)").arg(text(card, "title"), text(card, "domain"));
        d << tr("Method: %1").arg(text(card, "method"));
        d << tr("State variables:");
        for (const json& v : card.value("variables", json::array()))
            d << QStringLiteral("  • %1 [%2] — %3").arg(text(v, "label"), text(v, "unit"), text(v, "class"));
        d << tr("Parameters:");
        for (const json& p : card.value("parameters", json::array()))
            d << QStringLiteral("  • %1 = %2 %3").arg(text(p, "label"), number(p["value"]), text(p, "unit"));
        for (const json& a : card.value("assumptions", json::array()))
            d << tr("Assumes: %1").arg(QString::fromStdString(a.get<std::string>()));
        for (const json& e : card.value("excluded", json::array()))
            d << tr("Excludes: %1").arg(QString::fromStdString(e.get<std::string>()));
        d << tr("Valid: %1").arg(text(card, "validity"));
    }
    d << tr("Not implemented: %1").arg(notImplemented.join(QStringLiteral(", ")));
    ui_->cardText->setPlainText(d.join(QLatin1Char('\n')));
}

void MaterialStatePanel::showResult(const QString& resultId)
{
    resultId_ = resultId;
    result_ = controller_->result(resultId);
    const bool reliable = result_.value("reliable", false);
    QStringList status;
    status << tr("%1: %2").arg(text(result_, "title").isEmpty() ? tr("Evolution") : text(result_, "title"),
                               reliable ? tr("reliable") : tr("not marked reliable"));
    for (const json& r : result_.value("reliability", json::array()))
        status << QString::fromStdString(r.get<std::string>());
    const json energy = result_.value("energy", json::object());
    QStringList e;
    for (const auto& [k, v] : energy.items())
        if (v.is_number())
            e << QStringLiteral("%1 %2").arg(QString::fromStdString(k), number(v));
    if (!e.isEmpty())
        status << tr("Energy (%1): %2").arg(text(energy, "unit"), e.join(QStringLiteral(", ")));
    ui_->statusLabel->setText(status.join(QStringLiteral(" — ")));
    {
        const QSignalBlocker b(ui_->curveCombo);
        ui_->curveCombo->clear();
        for (const json& c : result_.value("curves", json::array()))
            ui_->curveCombo->addItem(text(c, "title"), text(c, "id"));
        const json profiles = result_.value("profiles", json::array());
        if (!profiles.empty())
            ui_->curveCombo->addItem(tr("Profile: %1").arg(text(profiles[0], "title")), QStringLiteral("profile"));
        if (result_.contains("positions"))
            ui_->curveCombo->addItem(tr("Profile: temperature"), QStringLiteral("temperature"));
    }
    const int snapshots = static_cast<int>(result_.contains("positions") ? result_.value("history", json::array()).size()
                                                                         : result_.value("profiles", json::array()).size());
    {
        const QSignalBlocker b(ui_->snapshotSpin);
        ui_->snapshotSpin->setRange(0, std::max(0, snapshots - 1));
        ui_->snapshotSpin->setValue(std::max(0, snapshots - 1));
        ui_->snapshotSpin->setEnabled(snapshots > 0);
    }
    // History: the step, its time, and the first scalar values.
    const json history = result_.value("history", json::array());
    QStringList columns;
    if (!history.empty())
        for (const auto& [k, v] : history.front().items())
            if ((v.is_number() || v.is_boolean()) && columns.size() < 7)
                columns << QString::fromStdString(k);
    ui_->historyTable->clear();
    ui_->historyTable->setColumnCount(static_cast<int>(columns.size()));
    ui_->historyTable->setHorizontalHeaderLabels(columns);
    ui_->historyTable->setRowCount(static_cast<int>(history.size()));
    for (size_t r = 0; r < history.size(); ++r)
        for (int c = 0; c < columns.size(); ++c)
            ui_->historyTable->setItem(static_cast<int>(r), c, new QTableWidgetItem(number(history[r].value(columns[c].toStdString(), json()))));
    ui_->saveButton->setEnabled(true);
    plotCurve();
}

void MaterialStatePanel::plotCurve()
{
    const QString id = ui_->curveCombo->currentData().toString();
    if (id.isEmpty()) {
        ui_->plot->clear();
        return;
    }
    const int k = ui_->snapshotSpin->value();
    if (id == QStringLiteral("profile")) {
        const json profiles = result_.value("profiles", json::array());
        if (k < static_cast<int>(profiles.size())) {
            const json& p = profiles[static_cast<size_t>(k)];
            const auto x = p.value("x", std::vector<double>{}), y = p.value("y", std::vector<double>{});
            ui_->plot->setLogX(false);
            ui_->plot->setCurve(QVector<double>(x.begin(), x.end()), QVector<double>(y.begin(), y.end()),
                                tr("%1 at %2").arg(text(p, "title"), number(p["time"])), text(p, "xLabel"), text(p, "yLabel"));
        }
        return;
    }
    if (id == QStringLiteral("temperature")) {
        const json history = result_.value("history", json::array());
        if (k < static_cast<int>(history.size())) {
            const auto x = result_.value("positions", std::vector<double>{});
            const auto y = history[static_cast<size_t>(k)].value("temperature", std::vector<double>{});
            ui_->plot->setLogX(false);
            ui_->plot->setCurve(QVector<double>(x.begin(), x.end()), QVector<double>(y.begin(), y.end()),
                                tr("Temperature at %1 s").arg(number(history[static_cast<size_t>(k)]["time"])), tr("Position (m)"), tr("Temperature (K)"));
        }
        return;
    }
    for (const json& c : result_.value("curves", json::array()))
        if (QString::fromStdString(c.value("id", "")) == id) {
            const auto x = c.value("x", std::vector<double>{}), y = c.value("y", std::vector<double>{});
            ui_->plot->setLogX(false);
            ui_->plot->setCurve(QVector<double>(x.begin(), x.end()), QVector<double>(y.begin(), y.end()), text(c, "title"),
                                text(c, "xLabel") + (text(c, "xUnit").isEmpty() ? QString() : QStringLiteral(" (%1)").arg(text(c, "xUnit"))),
                                text(c, "yLabel") + (text(c, "yUnit").isEmpty() ? QString() : QStringLiteral(" (%1)").arg(text(c, "yUnit"))));
        }
}

int MaterialStatePanel::studyCount() const { return ui_->studyCombo->count(); }
void MaterialStatePanel::selectStudy(const QString& id) { ui_->studyCombo->setCurrentIndex(ui_->studyCombo->findData(id)); }
void MaterialStatePanel::selectModel(const QString& id) { ui_->modelCombo->setCurrentIndex(ui_->modelCombo->findData(id)); }
QString MaterialStatePanel::cardText() const { return ui_->cardText->toPlainText(); }
QPushButton* MaterialStatePanel::runStudyButton() const { return ui_->runStudyButton; }
QPushButton* MaterialStatePanel::runModelButton() const { return ui_->runModelButton; }
QPushButton* MaterialStatePanel::saveButton() const { return ui_->saveButton; }
QPushButton* MaterialStatePanel::commitButton() const { return ui_->commitButton; }
QStringList MaterialStatePanel::curveChoices() const
{
    QStringList out;
    for (int i = 0; i < ui_->curveCombo->count(); ++i)
        out << ui_->curveCombo->itemData(i).toString();
    return out;
}
void MaterialStatePanel::showCurve(int index) { ui_->curveCombo->setCurrentIndex(index); }
int MaterialStatePanel::plottedPoints() const { return ui_->plot->pointCount(); }
int MaterialStatePanel::historyRowCount() const { return ui_->historyTable->rowCount(); }
int MaterialStatePanel::snapshotCount() const { return ui_->snapshotSpin->maximum() + 1; }
void MaterialStatePanel::chooseTarget(const QString& materialId) { ui_->targetCombo->setCurrentIndex(ui_->targetCombo->findData(materialId)); }
int MaterialStatePanel::recordCount() const { return ui_->recordCombo->count(); }
QString MaterialStatePanel::statusText() const { return ui_->statusLabel->text(); }
