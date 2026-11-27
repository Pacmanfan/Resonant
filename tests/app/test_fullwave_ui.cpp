// The full-wave solver driven through the real main window.
#include "AcousticController.h"
#include "AcousticsPanel.h"
#include "DocumentController.h"
#include "FdtdController.h"
#include "FdtdPanel.h"
#include "FieldAnalysisPanel.h"
#include "MainWindow.h"
#include "PlotWidget.h"
#include "TimeSignalPanel.h"
#include "ViewportWidget.h"

#include <QAction>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QPushButton>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTest>

#include <gtest/gtest.h>

using namespace resonant;
using namespace resonant::acoustics;
using namespace resonant::fdtd;

namespace {

class FullWaveUiTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        window_ = std::make_unique<MainWindow>();
        window_->unsavedChangesPrompt = [] { return QMessageBox::Discard; };
        window_->resize(1400, 900);
        window_->show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(window_.get()));
        QTest::qWait(30);
    }
    void TearDown() override
    {
        window_->close();
        window_.reset();
    }
    void trigger(const char* name)
    {
        QAction* action = window_->action(QString::fromLatin1(name));
        ASSERT_NE(action, nullptr) << name;
        action->trigger();
        QCoreApplication::processEvents();
    }
    DocumentController& doc() { return window_->controller(); }
    FdtdController& solver() { return window_->fullWave(); }
    FdtdPanel& panel() { return window_->fullWavePanel(); }
    template <typename T>
    T* child(QWidget& parent, const char* name)
    {
        T* widget = parent.findChild<T*>(QString::fromLatin1(name));
        EXPECT_NE(widget, nullptr) << name;
        return widget;
    }
    // An emitter at the origin, a 60 mm plane around it and a probe.
    void smallScene()
    {
        trigger("actionAddEmitter");
        ASSERT_TRUE(doc().setTransform(doc().primarySelection(), Transform{}, QStringLiteral("Move")).ok());
        trigger("actionAddFieldPlane");
        region_ = doc().primarySelection();
        ASSERT_TRUE(doc().setTransform(region_, Transform{}, QStringLiteral("Move")).ok());
        RegionSettings small;
        small.kind = RegionKind::PlaneXZ;
        small.size = Vec3(0.06, 0.1, 0.06);
        ASSERT_TRUE(window_->acoustics().setComponent(region_, std::make_shared<RegionComponent>(small), QStringLiteral("Edit")).ok());
        trigger("actionAddProbe");
        probe_ = doc().primarySelection();
        ASSERT_TRUE(doc().setTransform(probe_, Transform{Vec3(0.02, 0, 0)}, QStringLiteral("Move")).ok());
        QCoreApplication::processEvents();
        panel().selectRegion(region_);
    }
    bool waitForResults(size_t count, int timeoutMs = 120000)
    {
        return QTest::qWaitFor([&] { return solver().results().size() == count && !solver().isRunning(); }, timeoutMs);
    }

    std::unique_ptr<MainWindow> window_;
    Id region_, probe_;
};

}  // namespace

TEST_F(FullWaveUiTest, PanelShowsThePlanAndTheSolverAssessmentAndStoresSettingsUndoably)
{
    // Without a region the panel says what is missing.
    panel().check();
    EXPECT_TRUE(panel().planText().contains(QStringLiteral("Add a field plane")));
    EXPECT_FALSE(panel().run());
    smallScene();
    panel().check();
    const QString plan = panel().planText();
    EXPECT_TRUE(plan.contains(QStringLiteral("2D grid"))) << plan.toStdString();
    EXPECT_TRUE(plan.contains(QStringLiteral("12 cells per wavelength")));
    EXPECT_TRUE(plan.contains(QStringLiteral("Courant number")));
    EXPECT_TRUE(plan.contains(QStringLiteral("LINE source")));
    EXPECT_TRUE(plan.contains(QStringLiteral("Sources: 1 simulated"))) << plan.toStdString();
    // An empty scene: the analytical solver is complete and cheapest, and nothing is chosen for the user.
    const QString assessment = panel().assessmentText();
    EXPECT_TRUE(assessment.contains(QStringLiteral("Analytical free field [approximate model]: represents everything")));
    EXPECT_TRUE(assessment.contains(QStringLiteral("Full-wave time domain (FDTD) [full-wave]")));
    EXPECT_TRUE(assessment.contains(QStringLiteral("never chooses or changes the solver")));

    // Settings through the form.
    auto* resolution = child<QDoubleSpinBox>(panel(), "resolutionSpin");
    resolution->setValue(8.0);
    EXPECT_DOUBLE_EQ(solver().settings().pointsPerWavelength, 8.0);
    panel().check();
    EXPECT_TRUE(panel().planText().contains(QStringLiteral("coarse")));
    auto* waveform = child<QComboBox>(panel(), "waveformCombo");
    waveform->setCurrentIndex(1);
    emit waveform->activated(1);
    EXPECT_EQ(solver().settings().waveform.kind, WaveformKind::ToneBurst);
    EXPECT_TRUE(child<QDoubleSpinBox>(panel(), "cyclesSpin")->isEnabled());
    EXPECT_FALSE(child<QDoubleSpinBox>(panel(), "periodsSpin")->isEnabled());
    auto* boundary = child<QComboBox>(panel(), "boundaryCombo");
    boundary->setCurrentIndex(1);
    emit boundary->activated(1);
    EXPECT_EQ(solver().settings().boundary[0], BoundaryKind::Rigid);
    EXPECT_FALSE(child<QSpinBox>(panel(), "absorbingSpin")->isEnabled());
    // A solid object makes the analytical solvers incomplete.
    trigger("actionAddBox");
    ASSERT_TRUE(doc().setGeometryParameter(doc().primarySelection(), "width", 0.01).ok());
    panel().check();
    EXPECT_TRUE(panel().assessmentText().contains(QStringLiteral("Analytical free field [approximate model]: would LEAVE OUT")));
    EXPECT_TRUE(panel().planText().contains(QStringLiteral("rigid body (no material assigned)")));
    trigger("actionUndo");
    trigger("actionUndo");

    // Undo restores the settings, and they are saved with the project.
    trigger("actionUndo");
    EXPECT_EQ(solver().settings().boundary[0], BoundaryKind::Absorbing);
    EXPECT_EQ(boundary->currentIndex(), 0);
    trigger("actionUndo");
    EXPECT_EQ(solver().settings().waveform.kind, WaveformKind::Continuous);
    EXPECT_TRUE(doc().project().extensions.contains("fdtd"));
}

TEST_F(FullWaveUiTest, ContinuousRunProducesASteadyFieldThatJoinsTheResultsAndSignalsCanBeInspected)
{
    smallScene();
    trigger("actionRunFullWave");
    EXPECT_TRUE(panel().statusText().startsWith(QStringLiteral("Running")));
    EXPECT_TRUE(solver().isRunning());
    EXPECT_FALSE(child<QPushButton>(panel(), "runButton")->isEnabled());
    ASSERT_TRUE(waitForResults(1));
    QCoreApplication::processEvents();
    EXPECT_TRUE(child<QPushButton>(panel(), "runButton")->isEnabled());

    const auto result = solver().results().front();
    ASSERT_NE(result->harmonicField, nullptr);
    // The steady-state field is an ordinary result: displayed, compared, recorded.
    AcousticController& acoustics = window_->acoustics();
    ASSERT_EQ(acoustics.results().size(), 1u);
    EXPECT_EQ(acoustics.displayedResult(), result->harmonicField);
    EXPECT_EQ(window_->fieldAnalysis().comparisonCell(0, 1), QStringLiteral("acoustic.fdtd.staggered"));
    const auto& record = doc().project().extensions["acousticRuns"][0];
    EXPECT_EQ(record["metadata"]["solver"], "acoustic.fdtd.staggered");
    EXPECT_EQ(record["metadata"]["configuration"]["grid"]["dimensions"], 2);
    EXPECT_TRUE(record["metadata"]["configuration"]["grid"].contains("courantNumber"));
    // The viewport shows it.
    QCoreApplication::processEvents();
    EXPECT_GT(acoustics.displayGrid().nu * acoustics.displayGrid().nv, 100);
    EXPECT_NE(acoustics.displayGrid().samples, nullptr);

    // Time recorded.
    TimeSignalPanel& recorded = window_->timeSignals();
    EXPECT_EQ(recorded.resultCount(), 1);
    ASSERT_EQ(recorded.probeCount(), 1);
    EXPECT_EQ(recorded.plot().pointCount(), static_cast<int>(result->output.steps));
    EXPECT_TRUE(recorded.infoText().contains(QStringLiteral("Instantaneous pressure")));
    recorded.setView(TimeSignalPanel::SpectrumMagnitude);
    EXPECT_TRUE(recorded.infoText().contains(QStringLiteral("frequency resolution")));
    EXPECT_TRUE(recorded.infoText().contains(QStringLiteral("NOT calculated eigenmodes")));
    // The strongest response peak is the driving frequency.
    ASSERT_FALSE(recorded.plot().markers().isEmpty());
    EXPECT_NEAR(recorded.plot().markers().front(), 40.0, 1.5);
    recorded.setView(TimeSignalPanel::EnergyOverTime);
    EXPECT_TRUE(recorded.plot().title().contains(QStringLiteral("energy"), Qt::CaseInsensitive));
    EXPECT_GT(recorded.plot().pointCount(), 100);

    // Recorded instants replace the displayed field without joining the results...
    ASSERT_EQ(recorded.snapshotCount(), 40);
    ASSERT_TRUE(recorded.showSnapshot(30));
    EXPECT_NE(acoustics.displayedResult(), result->harmonicField);
    EXPECT_NE(acoustics.displayedResult()->assumptions[0].find("INSTANTANEOUS pressure"), std::string::npos);
    EXPECT_EQ(acoustics.display().quantity, render::FieldQuantity::Instantaneous);
    EXPECT_FALSE(acoustics.isAnimating());
    EXPECT_EQ(acoustics.results().size(), 1u);
    // ...and the steady state comes back on request.
    child<QPushButton>(recorded, "steadyButton")->click();
    EXPECT_EQ(acoustics.displayedResult(), result->harmonicField);

    // Export of the probe recorded.
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("recorded.csv"));
    ASSERT_TRUE(solver().exportProbes(0, path).ok());
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    const QList<QByteArray> lines = file.readAll().split('\n');
    EXPECT_TRUE(lines[0].startsWith("# "));
    EXPECT_TRUE(lines[1].startsWith("time_s,\"Probe"));
    EXPECT_EQ(lines.size(), static_cast<qsizetype>(result->output.steps) + 3);
}

TEST_F(FullWaveUiTest, PulseRunShowsAnInstantAndARigidBodyChangesTheField)
{
    smallScene();
    FdtdSettings s = solver().settings();
    s.waveform.kind = WaveformKind::GaussianPulse;
    ASSERT_TRUE(solver().setSettings(s).ok());
    ASSERT_TRUE(panel().run());
    ASSERT_TRUE(waitForResults(1));
    QCoreApplication::processEvents();
    const auto open = solver().results()[0];
    EXPECT_EQ(open->harmonicField, nullptr);
    // No steady state exists: an instant from the run is shown instead.
    ASSERT_NE(window_->acoustics().displayedResult(), nullptr);
    EXPECT_NE(window_->acoustics().displayedResult()->assumptions[0].find("INSTANTANEOUS"), std::string::npos);
    EXPECT_TRUE(window_->acoustics().results().empty());
    EXPECT_FALSE(child<QPushButton>(window_->timeSignals(), "steadyButton")->isEnabled());
    EXPECT_EQ(doc().project().extensions["acousticRuns"].size(), 1u);

    // A rigid box between emitter and probe: the direct pulse is blocked.
    trigger("actionAddBox");
    const Id box = doc().primarySelection();
    ASSERT_TRUE(doc().setGeometryParameter(box, "width", 0.004).ok());
    ASSERT_TRUE(doc().setGeometryParameter(box, "depth", 0.1).ok());
    ASSERT_TRUE(doc().setGeometryParameter(box, "height", 0.03).ok());
    ASSERT_TRUE(doc().setTransform(box, Transform{Vec3(0.01, 0, 0)}, QStringLiteral("Move")).ok());
    QCoreApplication::processEvents();
    ASSERT_TRUE(panel().run());
    ASSERT_TRUE(waitForResults(2));
    const auto blocked = solver().results()[1];
    EXPECT_GT(blocked->plan.rigidCells, 0u);
    const auto peak = [](const FdtdResult& r) {
        double best = 0.0;
        for (const double v : r.output.probes.front().pressure)
            best = std::max(best, std::abs(v));
        return best;
    };
    EXPECT_LT(peak(*blocked), 0.6 * peak(*open));  // only what diffracts around the box arrives
    EXPECT_GT(peak(*blocked), 0.0);
}

TEST_F(FullWaveUiTest, RefusedAndCancelledRunsAreReported)
{
    trigger("actionAddEmitter");
    ASSERT_TRUE(doc().setTransform(doc().primarySelection(), Transform{}, QStringLiteral("Move")).ok());
    trigger("actionAddFieldVolume");
    region_ = doc().primarySelection();
    ASSERT_TRUE(doc().setTransform(region_, Transform{}, QStringLiteral("Move")).ok());
    RegionSettings large;
    large.kind = RegionKind::Volume;
    large.size = Vec3(0.6, 0.6, 0.6);
    ASSERT_TRUE(window_->acoustics().setComponent(region_, std::make_shared<RegionComponent>(large), QStringLiteral("Edit")).ok());
    panel().selectRegion(region_);
    EXPECT_FALSE(panel().run());
    EXPECT_TRUE(panel().statusText().contains(QStringLiteral("memory limit")));
    EXPECT_TRUE(panel().planText().startsWith(QStringLiteral("CANNOT RUN")));
    EXPECT_TRUE(panel().planText().contains(QStringLiteral("not lowered automatically")));
    EXPECT_FALSE(solver().isRunning());

    // A long 2D run, cancelled.
    RegionSettings plane;
    plane.kind = RegionKind::PlaneXZ;
    plane.size = Vec3(0.3, 0.1, 0.3);
    ASSERT_TRUE(window_->acoustics().setComponent(region_, std::make_shared<RegionComponent>(plane), QStringLiteral("Edit")).ok());
    FdtdSettings s = solver().settings();
    s.duration = 0.2;
    ASSERT_TRUE(solver().setSettings(s).ok());
    ASSERT_TRUE(panel().run());
    EXPECT_TRUE(child<QPushButton>(panel(), "cancelButton")->isEnabled());
    QTest::qWait(150);
    child<QPushButton>(panel(), "cancelButton")->click();
    ASSERT_TRUE(QTest::qWaitFor([&] { return !solver().isRunning(); }, 30000));
    EXPECT_TRUE(solver().results().empty());
    EXPECT_TRUE(panel().statusText().contains(QStringLiteral("cancelled"), Qt::CaseInsensitive));
}

TEST_F(FullWaveUiTest, ExampleProjectsOpenWithTheirSolverSettingsAndRun)
{
    for (const char* name : {"b-reflection", "e-resonant-cavity"}) {
        const QString path = QStringLiteral(RESONANT_SOURCE_DIR) + QStringLiteral("/examples/advanced_acoustics/") + QString::fromLatin1(name)
                             + QStringLiteral(".resonant");
        ASSERT_TRUE(window_->openProjectFile(path).ok()) << name;
        ASSERT_TRUE(panel().run()) << name << ": " << panel().statusText().toStdString();
        ASSERT_TRUE(waitForResults(1, 240000)) << name;
        QCoreApplication::processEvents();
    }
    // The cavity example was a Ricker pulse in a rigid domain.
    EXPECT_EQ(solver().settings().waveform.kind, WaveformKind::Ricker);
    EXPECT_EQ(child<QComboBox>(panel(), "boundaryCombo")->currentIndex(), 1);
    TimeSignalPanel& recorded = window_->timeSignals();
    recorded.setView(TimeSignalPanel::SpectrumMagnitude);
    // Its lowest resonance, c / (2 * 40 mm) = 4290 Hz, is among the marked peaks.
    bool found = false;
    for (const double marker : recorded.plot().markers())
        found = found || std::abs(marker - 4.29) < 0.15;
    EXPECT_TRUE(found);
}
