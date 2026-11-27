// The example experiment projects (REQ-CHAT-20261126-008): acoustic,
// mechanical and RF experiments on the simulated devices, each of which
// validates and runs to completion with an explicit output authorization.
//
// To regenerate the example files after an intended change:
//   RESONANT_WRITE_EXAMPLES=1 resonant_core_tests --gtest_filter='ExperimentExamples.*'
#include "acoustics/AcousticExperiments.h"
#include "electromagnetics/RfExperiments.h"
#include "experiments/ExperimentProject.h"
#include "experiments/ExperimentRunner.h"
#include "experiments/SimulatedHardware.h"
#include "mechanics/MechanicalExperiments.h"
#include "project/ProjectSerializer.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>

using namespace resonant;
using namespace resonant::experiments;

namespace {

CalibrationRecord simulatedCalibration(const std::string& id, const std::string& sensor, double value, const std::string& unit)
{
    CalibrationRecord c;
    c.id = id;
    c.sensorId = sensor;
    c.type = CalibrationType::SensorSensitivity;
    c.scalar = value;
    c.unit = unit;
    c.dateUtc = "2026-11-27T00:00:00Z";
    c.procedure = "the simulated device's model sensitivity (synthetic: it calibrates the simulated chain only)";
    c.source = "simulated device model";
    c.fromSimulatedDevice = true;
    c.scalarJustification = "the simulated sensor has a flat response by construction";
    return c;
}

std::unique_ptr<Project> build(const std::string& slug, const std::string& name, ExperimentWorkspace w)
{
    auto project = Project::createNew(name);
    project->id = Id("ex-exp-" + slug);
    project->createdUtc = project->modifiedUtc = "2026-11-27T00:00:00Z";
    for (auto& e : w.experiments)
        e.projectId = project->id.str();
    storeExperiments(*project, w);
    return project;
}

std::unique_ptr<Project> checkExample(const std::string& file, const Project& built)
{
    const auto path = std::filesystem::path(RESONANT_SOURCE_DIR) / "examples" / "experiments" / file;
    const nlohmann::json expected = serializeProject(built);
    if (std::getenv("RESONANT_WRITE_EXAMPLES")) {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path) << expected.dump(2) << "\n";
    }
    auto loaded = loadProjectFile(path);
    EXPECT_TRUE(loaded.ok()) << loaded.message();
    if (!loaded.ok())
        return nullptr;
    EXPECT_TRUE(serializeProject(*loaded.value().project).dump() == nlohmann::json::parse(expected.dump()).dump())
        << file << " is stale; regenerate it with RESONANT_WRITE_EXAMPLES=1";
    return std::move(loaded.value().project);
}

// Every experiment of a project validates and completes on the simulated bench.
void runAll(const Project& project)
{
    auto w = loadExperiments(project);
    ASSERT_TRUE(w.ok()) << w.message();
    DeviceRegistry registry;
    registry.registerDriver(std::make_shared<SimulatedDriver>());
    registry.discover();
    for (const auto& e : w.value().experiments) {
        const auto report = validateExperiment(e);
        EXPECT_TRUE(report.ok()) << e.id << ": " << (report.errors.empty() ? "" : report.errors.front());
        ExperimentRunner runner(registry, w.value().calibrations, nullptr);
        RunOptions options;
        if (e.hasPhysicalOutput()) {
            auto a = OutputAuthority::instance().grant(outputRequestFor(e), e.safety, "example check", "simulated devices only");
            ASSERT_TRUE(a.ok()) << e.id << ": " << a.message();
            options.authorization = a.value();
        }
        const RunResult r = runner.run(e, options);
        EXPECT_EQ(r.state, ExperimentState::Completed) << e.id << ": " << r.record.message;
    }
}

}  // namespace

TEST(ExperimentExamples, Acoustic)
{
    ExperimentWorkspace w;
    Experiment speaker = acoustics::speakerCharacterizationExperiment("ex-exp-speaker");
    speaker.sensors[0].calibrationId = "cal-sim-mic-1";
    ProcessingStep cal;
    cal.operation = "calibrate";
    cal.parameters = {{"bindings", {"ch-mic"}}};
    speaker.processing.push_back(cal);  // the H1 estimate in Pa per FS of drive
    ASSERT_TRUE(w.addExperiment(speaker).ok());
    ASSERT_TRUE(w.addExperiment(acoustics::fieldMappingExperiment("ex-exp-field-map", 2000.0)).ok());
    ASSERT_TRUE(w.addExperiment(acoustics::focusingValidationExperiment("ex-exp-focus", Vec3(0.2, 1.0, 0.0))).ok());
    ASSERT_TRUE(w.addExperiment(acoustics::ultrasonicElementExperiment("ex-exp-element-a", "out-a")).ok());
    ASSERT_TRUE(w.calibrations.add(simulatedCalibration("cal-sim-mic-1", "mic-1", simulated::kMicFsPerPa, "FS/Pa")).ok());
    auto project = checkExample("acoustic_experiments.resonant", *build("acoustic", "Acoustic experiments (simulated devices)", w));
    ASSERT_NE(project, nullptr);
    runAll(*project);
}

TEST(ExperimentExamples, Mechanical)
{
    ExperimentWorkspace w;
    Experiment modal = mechanics::modalTestExperiment("ex-exp-modal");
    Experiment vibration = mechanics::vibrationCharacterizationExperiment("ex-exp-vibration", 120.0);
    Experiment shaker = mechanics::shakerCalibrationExperiment("ex-exp-shaker");
    ASSERT_TRUE(w.calibrations.add(simulatedCalibration("cal-sim-load-cell", "load-cell", simulated::kLoadCellVoltPerNewton, "V/N")).ok());
    for (const char* a : {"acc-1", "acc-2", "acc-3"})
        ASSERT_TRUE(w.calibrations.add(simulatedCalibration(std::string("cal-sim-") + a, a, simulated::kAccelerometerVoltPerMs2, "V/m/s^2")).ok());
    for (Experiment* e : {&modal, &vibration, &shaker})
        for (auto& s : e->sensors)
            s.calibrationId = "cal-sim-" + s.id;
    ASSERT_TRUE(w.addExperiment(modal).ok());
    ASSERT_TRUE(w.addExperiment(vibration).ok());
    ASSERT_TRUE(w.addExperiment(shaker).ok());
    auto project = checkExample("mechanical_experiments.resonant", *build("mechanical", "Mechanical experiments (simulated devices)", w));
    ASSERT_NE(project, nullptr);
    runAll(*project);
}

TEST(ExperimentExamples, Electromagnetic)
{
    ExperimentWorkspace w;
    ASSERT_TRUE(w.addExperiment(em::antennaImpedanceExperiment("ex-exp-s11", 1e9)).ok());
    ASSERT_TRUE(w.addExperiment(em::arrayCalibrationExperiment("ex-exp-array-cal", 2.4e9)).ok());
    ASSERT_TRUE(w.addExperiment(em::radiationPatternExperiment("ex-exp-pattern", 2.4e9)).ok());
    auto project = checkExample("rf_experiments.resonant", *build("rf", "RF experiments (simulated devices)", w));
    ASSERT_NE(project, nullptr);
    runAll(*project);
    // The Touchstone example imports as a two-port network.
    std::ifstream in(std::filesystem::path(RESONANT_SOURCE_DIR) / "examples" / "experiments" / "rlc_antenna_and_line.s2p");
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    auto item = em::importTouchstoneMeasurement(text, "rlc_antenna_and_line.s2p", "ex-exp-s11");
    ASSERT_TRUE(item.ok()) << item.message();
    EXPECT_EQ(em::networkOf(item.value()).value().ports, 2);
}
