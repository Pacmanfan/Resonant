#include "FieldAnalysisPanel.h"

#include "AcousticController.h"
#include "DisplayUnits.h"
#include "DocumentController.h"
#include "foundation/Units.h"
#include "ui_FieldAnalysisPanel.h"

#include <QFileDialog>

using namespace resonant;
using namespace resonant::acoustics;

FieldAnalysisPanel::FieldAnalysisPanel(QWidget* parent) : QWidget(parent), ui_(std::make_unique<Ui::FieldAnalysisPanel>())
{
    ui_->setupUi(this);
    ui_->splitter->setStretchFactor(0, 2);
    ui_->splitter->setStretchFactor(1, 5);
    ui_->summaryTree->setColumnWidth(0, 190);
    connect(ui_->refreshButton, &QPushButton::clicked, this, &FieldAnalysisPanel::refresh);
    connect(ui_->includeTargetsCheck, &QCheckBox::toggled, this, &FieldAnalysisPanel::refresh);
    connect(ui_->exportButton, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Export Point Values"), QStringLiteral("points.csv"),
                                                          QStringLiteral("Comma-separated values (*.csv);;JSON (*.json)"));
        if (path.isEmpty())
            return;
        const Status status = controller_->exportPointReports(path);
        emit controller_->message(status ? QStringLiteral("Exported point values to %1").arg(path)
                                         : QString::fromStdString(status.message()));
    });
}

FieldAnalysisPanel::~FieldAnalysisPanel() = default;

void FieldAnalysisPanel::setController(AcousticController* controller, DisplayUnits* units)
{
    controller_ = controller;
    units_ = units;
    connect(controller_, &AcousticController::resultsChanged, this, &FieldAnalysisPanel::refresh);
    connect(controller_, &AcousticController::settingsChanged, this, &FieldAnalysisPanel::refresh);
    connect(&controller_->document(), &DocumentController::sceneChanged, this, &FieldAnalysisPanel::refresh);
    connect(&controller_->document(), &DocumentController::projectReplaced, this, &FieldAnalysisPanel::refresh);
    connect(units_, &DisplayUnits::changed, this, &FieldAnalysisPanel::refresh);
    refresh();
}

QStringList FieldAnalysisPanel::summaryItems() const
{
    QStringList items;
    for (int i = 0; i < ui_->summaryTree->topLevelItemCount(); ++i)
        items.append(ui_->summaryTree->topLevelItem(i)->text(0) + QLatin1Char('=') + ui_->summaryTree->topLevelItem(i)->text(1));
    return items;
}

int FieldAnalysisPanel::pointRowCount() const { return ui_->pointTable->rowCount(); }
int FieldAnalysisPanel::comparisonRowCount() const { return ui_->comparisonTable->rowCount(); }

QString FieldAnalysisPanel::comparisonCell(int row, int column) const
{
    const QTableWidgetItem* item = ui_->comparisonTable->item(row, column);
    return item ? item->text() : QString();
}

QString FieldAnalysisPanel::pointCell(int row, int column) const
{
    const QTableWidgetItem* item = ui_->pointTable->item(row, column);
    return item ? item->text() : QString();
}

void FieldAnalysisPanel::refresh()
{
    if (!controller_)
        return;
    const QString unit = units_->lengthSymbol();
    const auto length = [this](double meters) { return QString::number(units_->lengthFromSI(meters), 'g', 6); };

    // --- summary ---
    ui_->summaryTree->clear();
    const auto row = [this](const QString& name, const QString& value) {
        auto* item = new QTreeWidgetItem(ui_->summaryTree, {name, value});
        item->setToolTip(1, value);
    };
    const AcousticSettings s = controller_->settings();
    const auto evaluator = controller_->currentEvaluator();
    row(QStringLiteral("Frequency"), QStringLiteral("%1 Hz").arg(s.frequencyHz, 0, 'g', 9));
    row(QStringLiteral("Medium"), QStringLiteral("%1: c = %2 m/s, rho = %3 kg/m^3").arg(QString::fromStdString(s.medium.name)).arg(s.medium.soundSpeed, 0, 'g', 6).arg(s.medium.density, 0, 'g', 5));
    row(QStringLiteral("Wavelength"), QStringLiteral("%1 %2").arg(length(s.medium.wavelength(s.frequencyHz)), unit));
    row(QStringLiteral("Active emitters (now)"), evaluator->excludedSourceCount() > 0
                                                     ? QStringLiteral("%1 at this frequency, %2 at other frequencies left out").arg(evaluator->sources().size()).arg(evaluator->excludedSourceCount())
                                                     : QString::number(evaluator->sources().size()));
    if (const auto result = controller_->displayedResult()) {
        const GridFrame& f = result->frame;
        row(QStringLiteral("Displayed result"), QString::fromStdString(result->name));
        row(QStringLiteral("Result frequency / medium"), QStringLiteral("%1 Hz in %2").arg(result->frequencyHz, 0, 'g', 9).arg(QString::fromStdString(result->medium.name)));
        row(QStringLiteral("Sources in result"), QString::number(result->sources.size()));
        row(QStringLiteral("Region size"), f.isVolume() ? QStringLiteral("%1 x %2 x %3 %4").arg(length(f.axisU.norm()), length(f.axisV.norm()), length(f.axisW.norm()), unit)
                                                         : QStringLiteral("%1 x %2 %3").arg(length(f.axisU.norm()), length(f.axisV.norm()), unit));
        row(QStringLiteral("Sampling"), f.isVolume() ? QStringLiteral("%1 x %2 x %3 samples").arg(f.nu).arg(f.nv).arg(f.nw) : QStringLiteral("%1 x %2 samples").arg(f.nu).arg(f.nv));
        row(QStringLiteral("Maximum |P|"), QStringLiteral("%1 Pa (peak)").arg(result->statistics.maxMagnitude, 0, 'g', 6));
        row(QStringLiteral("RMS at the maximum"), QStringLiteral("%1 Pa").arg(result->statistics.maxMagnitude / std::sqrt(2.0), 0, 'g', 6));
        row(QStringLiteral("Location of the maximum"), QStringLiteral("(%1, %2, %3) %4").arg(length(result->statistics.maxPoint.x()), length(result->statistics.maxPoint.y()), length(result->statistics.maxPoint.z()), unit));
        row(QStringLiteral("Mean |P|"), QStringLiteral("%1 Pa (peak)").arg(result->statistics.meanMagnitude, 0, 'g', 6));
        if (result->statistics.nearSourceSamples > 0)
            row(QStringLiteral("Samples inside a source"), QStringLiteral("%1 (excluded from the statistics)").arg(result->statistics.nearSourceSamples));
        row(QStringLiteral("Solver"), QStringLiteral("%1, version %2").arg(QString::fromStdString(result->metadata.solverId), QString::fromStdString(result->metadata.solverVersion)));
        row(QStringLiteral("Solver time"), QStringLiteral("%1 s").arg(result->metadata.elapsedSeconds, 0, 'f', 3));
        row(QStringLiteral("Colours show"), controller_->displayDescription());
        int index = 1;
        for (const std::string& assumption : result->assumptions)
            row(QStringLiteral("Assumption %1").arg(index++), QString::fromStdString(assumption));
    } else {
        row(QStringLiteral("Displayed result"), QStringLiteral("none - run a field simulation"));
        int index = 1;
        for (const std::string& assumption : evaluator->model().assumptions())
            row(QStringLiteral("Assumption %1").arg(index++), QString::fromStdString(assumption));
    }

    // --- points: evaluated directly from the current scene ---
    const auto reports = controller_->pointReports(ui_->includeTargetsCheck->isChecked());
    ui_->pointTable->setHorizontalHeaderLabels({QStringLiteral("Point"), QStringLiteral("Kind"), QStringLiteral("X (%1)").arg(unit), QStringLiteral("Y (%1)").arg(unit),
                                                QStringLiteral("Z (%1)").arg(unit), QStringLiteral("|P| (Pa peak)"), QStringLiteral("Phase (deg)"), QStringLiteral("RMS (Pa)"),
                                                QStringLiteral("SPL (dB)"), QStringLiteral("Active intensity (W/m^2)"), QStringLiteral("Desired |P| (Pa)"), QStringLiteral("Note")});
    ui_->pointTable->setRowCount(static_cast<int>(reports.size()));
    for (int r = 0; r < static_cast<int>(reports.size()); ++r) {
        const PointReport& p = reports[static_cast<size_t>(r)];
        const auto cell = [&](int column, const QString& text) {
            auto* item = new QTableWidgetItem(text);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            ui_->pointTable->setItem(r, column, item);
        };
        QStringList notes;
        if (p.nearSource)
            notes.append(QStringLiteral("inside a source: not valid"));
        if (!p.intensityAvailable)
            notes.append(QStringLiteral("this model provides no particle velocity"));
        else if (!p.intensityIsExact)
            notes.append(QStringLiteral("intensity approximate (directivity gradient neglected)"));
        cell(0, QString::fromStdString(p.label));
        cell(1, p.desired ? QStringLiteral("Target") : QStringLiteral("Probe"));
        cell(2, length(p.position.x()));
        cell(3, length(p.position.y()));
        cell(4, length(p.position.z()));
        cell(5, QString::number(std::abs(p.pressure), 'g', 6));
        cell(6, QString::number(units::radiansToDegrees(std::arg(p.pressure)), 'f', 2));
        cell(7, QString::number(p.rms, 'g', 6));
        cell(8, QStringLiteral("%1 re %2 uPa").arg(p.splDb, 0, 'f', 2).arg(p.referencePressure * 1e6, 0, 'g', 3));
        cell(9, p.intensityAvailable ? QString::number(p.activeIntensity.norm(), 'g', 5) : QStringLiteral("not calculated"));
        cell(10, p.desired ? QString::number(std::abs(*p.desired), 'g', 6) : QStringLiteral("-"));
        cell(11, notes.join(QStringLiteral("; ")));
    }
    ui_->pointTable->resizeColumnsToContents();
    ui_->exportButton->setEnabled(!reports.empty());

    // --- comparison of the session's results ---
    const auto intended = controller_->intendedPoint();
    ui_->comparisonTable->setRowCount(static_cast<int>(controller_->results().size()));
    for (int r = 0; r < static_cast<int>(controller_->results().size()); ++r) {
        const auto& result = controller_->results()[static_cast<size_t>(r)];
        const manipulators::FocusMetrics m = controller_->metricsFor(*result);
        const auto cell = [&](int column, const QString& text) {
            auto* item = new QTableWidgetItem(text);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            ui_->comparisonTable->setItem(r, column, item);
        };
        const auto width = [&](double w) { return w > 0.0 ? QStringLiteral("%1 %2").arg(length(w), unit) : QStringLiteral("0 (not resolved)"); };
        cell(0, QString::fromStdString(result->name));
        cell(1, QString::fromStdString(result->metadata.solverId));
        cell(2, QString::number(m.peakMagnitude, 'g', 6));
        cell(3, length(m.peakPoint.x()));
        cell(4, length(m.peakPoint.y()));
        cell(5, length(m.peakPoint.z()));
        cell(6, width(m.widthU));
        cell(7, width(m.widthV));
        cell(8, result->frame.isVolume() ? width(m.widthW) : QStringLiteral("-"));
        cell(9, m.displacement ? QStringLiteral("%1 %2").arg(length(*m.displacement), unit) : QStringLiteral("-"));
        cell(10, m.magnitudeAtIntended ? QString::number(*m.magnitudeAtIntended, 'g', 6) : QStringLiteral("-"));
        cell(11, m.sidelobeRatio ? QString::number(*m.sidelobeRatio, 'f', 3) : QStringLiteral("-"));
        cell(12, QString::number(result->metadata.elapsedSeconds, 'f', 2));
    }
    ui_->comparisonTable->resizeColumnsToContents();
    ui_->comparisonNote->setVisible(true);
    (void)intended;
}
