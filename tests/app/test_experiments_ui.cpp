// The Experimental Hardware workspace driven through the real main window
// (REQ-CHAT-20261126-008): opening a project authorizes nothing, outputs need
// an explicit authorization, runs do not block the interface, data and
// markers appear, the emergency stop ends a run.
#include "DocumentController.h"
#include "ExperimentsController.h"
#include "ExperimentsPanel.h"
#include "MainWindow.h"
#include "OutputAuthorizationDialog.h"
#include "ViewportWidget.h"

#include <QAction>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <gtest/gtest.h>

#include <filesystem>

using namespace resonant;
using namespace resonant::experiments;

namespace {

class ExperimentsUiTest : public ::testing::Test {
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
    ExperimentsController& experiments() { return window_->experiments(); }
    ExperimentsPanel& panel() { return window_->experimentsPanel(); }
    // Opens an example and saves it into a temporary directory (raw data goes next to it).
    void openExample(const char* file)
    {
        const auto source = std::filesystem::path(RESONANT_SOURCE_DIR) / "examples" / "experiments" / file;
        ASSERT_TRUE(window_->openProjectFile(QString::fromStdString(source.string())).ok());
        ASSERT_TRUE(window_->saveProjectToFile(dir_.filePath(QString::fromLatin1(file))).ok());
    }
    bool waitForRun(int seconds)
    {
        QSignalSpy finished(&experiments(), &ExperimentsController::runFinished);
        return !experiments().running() || finished.wait(seconds * 1000);
    }
    std::unique_ptr<MainWindow> window_;
    QTemporaryDir dir_;
};

}  // namespace

TEST_F(ExperimentsUiTest, WorkspaceActionsAndDialogs)
{
    for (const char* name : {"actionShowExperiments", "actionDiscoverDevices", "actionEmergencyStop"})
        EXPECT_NE(window_->action(QString::fromLatin1(name)), nullptr) << name;
    // Discovery runs in the background and reports the simulated bench.
    QSignalSpy devices(&experiments(), &ExperimentsController::devicesChanged);
    window_->action(QStringLiteral("actionDiscoverDevices"))->trigger();
    ASSERT_TRUE(devices.wait(10000));
    EXPECT_GE(experiments().devices().size(), 6u);
    // The authorization dialog accepts only a named operator who ticked the acknowledgement.
    OutputRequest request;
    request.experimentId = "x";
    request.channels = {"ch"};
    SafetyCheck ok;
    ok.allowed = true;
    OutputAuthorizationDialog dialog(request, ok);
    EXPECT_FALSE(dialog.canAccept());
    dialog.fill(QStringLiteral("tester"), false);
    EXPECT_FALSE(dialog.canAccept());
    dialog.fill(QStringLiteral("tester"), true);
    EXPECT_TRUE(dialog.canAccept());
    SafetyCheck violated;
    violated.violations = {"too loud"};
    OutputAuthorizationDialog refused(request, violated);
    refused.fill(QStringLiteral("tester"), true);
    EXPECT_FALSE(refused.canAccept());
}

TEST_F(ExperimentsUiTest, OpeningAProjectAuthorizesNothingAndOutputsNeedAuthorization)
{
    openExample("acoustic_experiments.resonant");
    const auto all = experiments().workspace().experiments;
    ASSERT_EQ(all.size(), 4u);
    for (const auto& e : all) {
        EXPECT_TRUE(e.hasPhysicalOutput());
        EXPECT_FALSE(experiments().isAuthorized(e.id)) << e.id;
    }
    panel().selectExperiment("ex-exp-field-map");
    auto* run = panel().findChild<QPushButton*>(QStringLiteral("runButton"));
    ASSERT_NE(run, nullptr);
    EXPECT_FALSE(run->isEnabled());
    EXPECT_FALSE(panel().runSelected());
    EXPECT_NE(panel().statusText().indexOf(QStringLiteral("authorize the output first")), -1) << panel().statusText().toStdString();
    EXPECT_EQ(experiments().lastResult(), nullptr);  // nothing ran
    // Authorized, it runs as a background job; the window stays responsive.
    ASSERT_TRUE(panel().authorizeSelected(QStringLiteral("tester")));
    EXPECT_TRUE(run->isEnabled());
    ASSERT_TRUE(panel().runSelected());
    QCoreApplication::processEvents();
    ASSERT_TRUE(waitForRun(60));
    ASSERT_NE(experiments().lastResult(), nullptr);
    EXPECT_EQ(experiments().lastResult()->state, ExperimentState::Completed) << experiments().lastResult()->record.message;
    EXPECT_FALSE(experiments().isAuthorized("ex-exp-field-map"));  // spent
    EXPECT_GT(panel().measurementCount(), 7);
    // The raw data is next to the project, read-only, and loads back.
    const auto records = experiments().measurements("ex-exp-field-map");
    ASSERT_FALSE(records.empty());
    auto data = experiments().data(records.front().id);
    ASSERT_TRUE(data.ok()) << data.message();
    EXPECT_GT(data.value().size(), 0u);
    EXPECT_TRUE(std::filesystem::exists(measurementRootFor(dir_.filePath(QStringLiteral("acoustic_experiments.resonant")).toStdString())));
    // Sensors, the speaker and the measured points are drawn.
    QCoreApplication::processEvents();
    EXPECT_GE(window_->viewport().extraItemCount(), 7u + 1u + 7u);
    // A comparison with the predicted field (in shape: the data is uncalibrated).
    auto comparison = experiments().compareWithSimulation("ex-exp-field-map", "");
    ASSERT_TRUE(comparison.ok()) << comparison.message();
    EXPECT_EQ(comparison.value().kind, "spatial");
    EXPECT_LT(comparison.value().metrics.value("rmsMagnitudeErrorDb", 99.0), 0.5);
    // The report names the simulated devices.
    EXPECT_NE(experiments().reportHtml("ex-exp-field-map").indexOf(QStringLiteral("Simulated devices")), -1);
    // Saved and reopened, the run's records are there and no authorization is.
    ASSERT_TRUE(window_->saveProjectToFile(dir_.filePath(QStringLiteral("acoustic_experiments.resonant"))).ok());
    ASSERT_TRUE(window_->openProjectFile(dir_.filePath(QStringLiteral("acoustic_experiments.resonant"))).ok());
    EXPECT_EQ(experiments().measurements("ex-exp-field-map").size(), records.size());
    EXPECT_FALSE(experiments().isAuthorized("ex-exp-field-map"));
}

TEST_F(ExperimentsUiTest, EmergencyStopEndsARunningExperiment)
{
    openExample("mechanical_experiments.resonant");
    auto w = experiments().workspace();
    Experiment* modal = w.experiment("ex-exp-modal");
    ASSERT_NE(modal, nullptr);
    Experiment slow = *modal;
    slow.devices["sim:daq-4x1"].options = {{"realTime", true}};
    ASSERT_TRUE(experiments().setExperiment(slow, QStringLiteral("Run in real time")).ok());
    panel().selectExperiment("ex-exp-modal");
    ASSERT_TRUE(panel().authorizeSelected(QStringLiteral("tester")));
    ASSERT_TRUE(panel().runSelected());
    QTest::qWait(600);
    ASSERT_TRUE(experiments().running());
    window_->action(QStringLiteral("actionEmergencyStop"))->trigger();
    ASSERT_TRUE(waitForRun(30));
    ASSERT_NE(experiments().lastResult(), nullptr);
    EXPECT_EQ(experiments().lastResult()->state, ExperimentState::Cancelled);
    EXPECT_TRUE(experiments().emergencyStopActive());
    // Latched: no authorization until it is reset.
    EXPECT_FALSE(panel().authorizeSelected(QStringLiteral("tester")));
    experiments().resetEmergencyStop(QStringLiteral("tester"));
    EXPECT_FALSE(experiments().emergencyStopActive());
    EXPECT_TRUE(panel().authorizeSelected(QStringLiteral("tester")));
}

TEST_F(ExperimentsUiTest, TemplatesAndTouchstoneImport)
{
    ASSERT_TRUE(window_->saveProjectToFile(dir_.filePath(QStringLiteral("new.resonant"))).ok());
    auto id = experiments().addFromTemplate(QStringLiteral("rf.antenna-impedance"));
    ASSERT_TRUE(id.ok()) << id.message();
    EXPECT_TRUE(experiments().validate(id.value()).ok());
    panel().selectExperiment(id.value());
    EXPECT_NE(panel().definitionText().indexOf(QStringLiteral("Safety limits")), -1);
    window_->controller().undo();
    EXPECT_TRUE(experiments().workspace().experiments.empty());
    window_->controller().redo();
    ASSERT_EQ(experiments().workspace().experiments.size(), 1u);
    const QString s2p = QString::fromStdString((std::filesystem::path(RESONANT_SOURCE_DIR) / "examples" / "experiments" / "rlc_antenna_and_line.s2p").string());
    ASSERT_TRUE(experiments().importTouchstone(s2p, id.value()).ok());
    const auto records = experiments().measurements(id.value());
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records.front().category, DataCategory::Raw);
    EXPECT_EQ(records.front().kind, MeasurementKind::NetworkParameters);
    // No antenna in this project: the comparison says so instead of inventing one.
    EXPECT_FALSE(experiments().compareWithSimulation(id.value(), records.front().id).ok());
}
