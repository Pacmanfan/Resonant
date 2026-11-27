// The experiment framework (REQ-CHAT-20261126-008): experiments, the
// hardware abstraction with simulated devices, acquisition and its
// integrity checks, the measurement store, calibration records, safety and
// output authorization, execution, persistence, spatial data, comparison,
// archives and reproducibility.
#include "dsp/SpectralEstimation.h"
#include "experiments/Acquisition.h"
#include "experiments/Calibration.h"
#include "experiments/Comparison.h"
#include "experiments/Experiment.h"
#include "experiments/ExperimentArchive.h"
#include "experiments/ExperimentProject.h"
#include "experiments/ExperimentReport.h"
#include "experiments/ExperimentRunner.h"
#include "experiments/MeasurementStore.h"
#include "experiments/Processing.h"
#include "experiments/Safety.h"
#include "experiments/SimulatedHardware.h"
#include "experiments/Spatial.h"
#include "experiments/Uncertainty.h"
#include "project/ProjectSerializer.h"

#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <thread>

#include <unistd.h>

using namespace resonant;
using namespace resonant::experiments;
namespace fs = std::filesystem;

namespace {

fs::path scratch(const std::string& name)
{
    fs::path p = fs::path(RESONANT_SOURCE_DIR) / "tmp" / "test-experiments" / (name + "-" + std::to_string(::getpid()));
    std::error_code ec;
    fs::permissions(p, fs::perms::owner_all, fs::perm_options::add, ec);
    for (auto it = fs::recursive_directory_iterator(p, ec); it != fs::recursive_directory_iterator(); ++it)
        fs::permissions(it->path(), fs::perms::owner_all, fs::perm_options::add, ec);
    fs::remove_all(p, ec);
    fs::create_directories(p);
    return p;
}

std::shared_ptr<DeviceRegistry> simulatedRegistry()
{
    auto r = std::make_shared<DeviceRegistry>();
    r->registerDriver(std::make_shared<SimulatedDriver>());
    r->discover();
    return r;
}

// A minimal vibration experiment on the simulated DAQ.
Experiment daqExperiment(bool withOutput)
{
    Experiment e;
    e.id = "exp-test";
    e.name = "DAQ test";
    e.domain = PhysicsDomain::Mechanical;
    e.acquisition.sampleRateHz = 8192.0;
    e.acquisition.durationSeconds = 1.0;
    e.acquisition.blockFrames = 512;
    Sensor force;
    force.id = "f";
    force.name = "load cell";
    force.category = SensorCategory::LoadCell;
    force.domain = PhysicsDomain::Mechanical;
    force.measures = Quantity::Force;
    Sensor acc = force;
    acc.id = "a";
    acc.name = "accelerometer";
    acc.category = SensorCategory::Accelerometer;
    acc.measures = Quantity::Acceleration;
    e.sensors = {force, acc};
    ChannelBinding bf{"ch-f", "sim:daq-4x1", "ai0", ChannelDirection::Input, "f", "", 1.0, "", true};
    ChannelBinding ba{"ch-a", "sim:daq-4x1", "ai1", ChannelDirection::Input, "a", "", 1.0, "", false};
    e.channels = {bf, ba};
    if (withOutput) {
        Actuator shaker;
        shaker.id = "shaker";
        shaker.name = "shaker";
        shaker.category = ActuatorCategory::Shaker;
        shaker.domain = PhysicsDomain::Mechanical;
        shaker.outputQuantity = Quantity::Force;
        shaker.maxCommandAmplitude = 5.0;
        e.actuators = {shaker};
        e.channels.push_back({"ch-s", "sim:daq-4x1", "ao0", ChannelDirection::Output, "", "shaker", 1.0, "", false});
        e.stimulus.kind = StimulusKind::Sine;
        e.stimulus.amplitude = 1.0;
        e.stimulus.startHz = e.stimulus.endHz = 120.0;
        e.stimulus.durationSeconds = 1.0;
        e.stimulus.outputBindings = {"ch-s"};
        e.safety.maxOutputAmplitude = 2.0;
        e.safety.allowedOutputChannels = {"ch-s"};
        e.safety.minFrequencyHz = 10.0;
        e.safety.maxFrequencyHz = 1000.0;
    }
    return e;
}

OutputAuthorization authorize(const Experiment& e)
{
    auto a = OutputAuthority::instance().grant(outputRequestFor(e), e.safety, "test operator", "I confirm the setup is safe");
    EXPECT_TRUE(a.ok()) << a.message();
    return a.value();
}

}  // namespace

TEST(Experiments, DefinitionRoundTripsAndValidatesWithoutHardware)
{
    Experiment e = daqExperiment(true);
    e.notes = "notes";
    e.annotations.push_back({"2026-11-27T00:00:00Z", "tester", "a remark"});
    e.positions.kind = ScanKind::Linear;
    e.positions.count = 3;
    e.positions.end[0] = 1.0;
    const auto back = experimentFromJson(toJson(e));
    ASSERT_TRUE(back.ok()) << back.message();
    EXPECT_EQ(toJson(back.value()), toJson(e));
    EXPECT_TRUE(validateExperiment(e).ok());

    // A sensor cannot measure what its kind does not measure; stress is never measured directly.
    Experiment bad = e;
    bad.sensors[1].measures = Quantity::Stress;
    auto report = validateExperiment(bad);
    ASSERT_FALSE(report.ok());
    EXPECT_NE(report.errors.front().find("stress is inferred"), std::string::npos);
    // A stimulus at the Nyquist frequency, an unknown output, a missing actuator.
    bad = e;
    bad.stimulus.startHz = bad.stimulus.endHz = 5000.0;
    EXPECT_FALSE(validateExperiment(bad).ok());
    bad = e;
    bad.stimulus.outputBindings = {"nope"};
    EXPECT_FALSE(validateExperiment(bad).ok());
    // Phase comparison across two devices needs a common clock or a timing calibration.
    bad = e;
    bad.synchronization.phaseSensitive = true;
    bad.channels[1].deviceId = "sim:audio-8x2";
    bad.channels[1].channelId = "in0";
    report = validateExperiment(bad);
    ASSERT_FALSE(report.ok());
    EXPECT_NE(report.errors.front().find("equal nominal sample rates"), std::string::npos);
    bad.synchronization.strategy = "shared-clock";
    EXPECT_TRUE(validateExperiment(bad).ok());
    // The running states are not persisted.
    e.state = ExperimentState::Running;
    EXPECT_EQ(toJson(e)["state"], "draft");
}

TEST(Experiments, StimulusGenerationIsDeterministicAndDelayed)
{
    Stimulus s;
    s.kind = StimulusKind::Sine;
    s.amplitude = 0.5;
    s.startHz = s.endHz = 1000.0;
    s.durationSeconds = 0.1;
    s.fadeSeconds = 0.0;
    auto x = generateStimulus(s, 48000.0);
    ASSERT_TRUE(x.ok());
    EXPECT_EQ(x.value().size(), 4800u);
    EXPECT_NEAR(*std::max_element(x.value().begin(), x.value().end()), 0.5, 1e-3);
    auto delayed = generateStimulus(s, 48000.0, 10.0 / 48000.0, 2.0);
    ASSERT_TRUE(delayed.ok());
    for (size_t i = 10; i < 4800; ++i)
        ASSERT_NEAR(delayed.value()[i], 2.0 * x.value()[i - 10], 1e-12);
    s.kind = StimulusKind::WhiteNoise;
    EXPECT_EQ(generateStimulus(s, 48000.0).value(), generateStimulus(s, 48000.0).value());
    s.kind = StimulusKind::LogSweep;
    s.endHz = 500.0;
    EXPECT_FALSE(generateStimulus(s, 48000.0).ok());  // start above end
}

TEST(Experiments, UncertaintyBudgetsCombineByRootSumSquare)
{
    UncertaintyBudget b;
    b.add({UncertaintySource::SensorCalibration, UncertaintyType::Calibration, 0.03, 0.0, "normal", 0, "cal"});
    b.add({UncertaintySource::Noise, UncertaintyType::Statistical, 0.04, 0.0, "normal", 9, "noise"});
    b.add(quantizationComponent(2.0 / 65536.0));
    EXPECT_NEAR(b.combinedRelative(), 0.05, 1e-12);
    EXPECT_NEAR(b.combinedStandard(10.0), std::sqrt(0.25 + std::pow(2.0 / 65536.0 / std::sqrt(12.0), 2)), 1e-9);
    EXPECT_NEAR(b.expanded(10.0), 2.0 * b.combinedStandard(10.0), 1e-12);
    EXPECT_NEAR(b.combinedStandard(10.0, UncertaintyType::Calibration), 0.3, 1e-12);
    const auto back = uncertaintyBudgetFromJson(toJson(b));
    ASSERT_TRUE(back.ok());
    EXPECT_NEAR(back.value().combinedStandard(10.0), b.combinedStandard(10.0), 1e-15);
}

TEST(Experiments, CalibrationHistoryIsAppendOnlyAndFrequencyDependent)
{
    CalibrationLibrary lib;
    CalibrationRecord first;
    first.id = "cal-mic-1";
    first.sensorId = "mic";
    first.scalar = 0.05;
    first.unit = "V/Pa";
    first.validUntilUtc = "2027-01-01T00:00:00Z";
    ASSERT_TRUE(lib.add(first).ok());
    EXPECT_FALSE(lib.add(first).ok());  // same id: never replaced
    CalibrationRecord second = first;
    second.id = "cal-mic-2";
    second.supersedes = "cal-mic-1";
    second.scalar = 0.051;
    second.response = {{2000.0, {0.05, 0.01}, 0.01}, {100.0, {0.049, 0.0}, 0.01}, {1000.0, {0.05, 0.0}, 0.01}};
    ASSERT_TRUE(lib.add(second).ok());
    EXPECT_EQ(lib.find("cal-mic-2")->version, 2);
    EXPECT_EQ(lib.find("cal-mic-2")->response.front().frequencyHz, 100.0);  // sorted
    CalibrationRecord third = second;
    third.id = "cal-mic-3";
    EXPECT_FALSE(lib.add(third).ok());  // cal-mic-1 is already superseded
    ASSERT_EQ(lib.history("cal-mic-2").size(), 2u);
    EXPECT_EQ(lib.history("cal-mic-2").front()->id, "cal-mic-1");
    EXPECT_EQ(lib.latest(CalibrationType::SensorSensitivity, "mic")->id, "cal-mic-2");
    // Interpolated inside, never extrapolated.
    const CalibrationRecord& c = *lib.find("cal-mic-2");
    EXPECT_NEAR(std::abs(c.valueAt(550.0).value()), 0.0495, 1e-9);
    EXPECT_FALSE(c.valueAt(50.0).ok());
    EXPECT_FALSE(c.valueAt(5000.0).ok());
    // A frequency-dependent calibration is not applied as one scalar.
    EXPECT_FALSE(applySensitivity({1.0}, c).ok());
    EXPECT_NEAR(applySensitivity({0.1}, first).value()[0], 2.0, 1e-12);
    auto spectrum = applyResponse({100.0, 1000.0}, {{0.049, 0.0}, {0.1, 0.0}}, c);
    ASSERT_TRUE(spectrum.ok());
    EXPECT_NEAR(std::abs(spectrum.value()[1]), 2.0, 1e-12);
    EXPECT_FALSE(applyResponse({5000.0}, {{1.0, 0.0}}, c).ok());
    // Expiry.
    EXPECT_FALSE(checkCalibration(first, 100.0, 1000.0, "2028-01-01T00:00:00Z").usable);
    EXPECT_TRUE(checkCalibration(first, 100.0, 1000.0, "2026-12-01T00:00:00Z").usable);
    const auto back = CalibrationLibrary::fromJson(lib.toJson());
    ASSERT_TRUE(back.ok());
    EXPECT_EQ(back.value().toJson(), lib.toJson());
}

TEST(Experiments, CalibrationProceduresRecoverKnownValues)
{
    const double fs = 48000.0;
    // A calibrator tone of 1 Pa RMS at 1003 Hz recorded through 0.05 V/Pa.
    std::vector<double> rec(48000);
    for (size_t i = 0; i < rec.size(); ++i)
        rec[i] = 0.05 * std::sqrt(2.0) * std::sin(2 * 3.14159265358979 * 1003.0 * static_cast<double>(i) / fs) + 1e-5 * std::sin(0.37 * i);
    auto s = sensitivityFromReference(rec, fs, 1.0, 1000.0, Quantity::Pressure, Quantity::Voltage, 0.01);
    ASSERT_TRUE(s.ok()) << s.message();
    EXPECT_NEAR(s.value().scalar, 0.05, 0.05 * 1e-3);
    EXPECT_EQ(s.value().unit, "V/Pa");
    EXPECT_NEAR(s.value().referenceFrequencyHz, 1003.0, 0.5);
    EXPECT_FALSE(sensitivityFromReference(std::vector<double>(48000, 0.0), fs, 1.0, 1000.0, Quantity::Pressure, Quantity::Voltage,
                                          0.01).ok());
    // Channel-to-channel: gain 0.5, delay 3 samples on channel 2.
    std::vector<double> a(16384), b(16384);
    for (size_t i = 0; i < a.size(); ++i) {
        a[i] = std::sin(2 * 3.14159265358979 * 1000.0 * static_cast<double>(i) / fs) + 0.3 * std::sin(2 * 3.14159265358979 * 3100.0 * i / fs);
        b[i] = i >= 3 ? 0.5 * (std::sin(2 * 3.14159265358979 * 1000.0 * static_cast<double>(i - 3) / fs) +
                               0.3 * std::sin(2 * 3.14159265358979 * 3100.0 * (i - 3) / fs))
                      : 0.0;
    }
    auto c = channelToChannel({{"d", "c1", a}, {"d", "c2", b}}, 0, fs, 1000.0, 50);
    ASSERT_TRUE(c.ok()) << c.message();
    ASSERT_EQ(c.value().size(), 1u);
    EXPECT_NEAR(c.value()[0].scalar, 0.5, 1e-3);
    EXPECT_NEAR(c.value()[0].delaySeconds, 3.0 / fs, 0.05 / fs);
    EXPECT_NEAR(c.value()[0].phaseRad, -2 * 3.14159265358979 * 1000.0 * 3.0 / fs, 1e-3);
}

TEST(Experiments, OutputNeedsAnExplicitAuthorizationWithinTheLimits)
{
    auto& authority = OutputAuthority::instance();
    Experiment e = daqExperiment(true);
    OutputRequest r = outputRequestFor(e);
    EXPECT_TRUE(checkOutput(e.safety, r).allowed);
    // Limits.
    SafetyLimits tight = e.safety;
    tight.maxOutputAmplitude = 0.5;
    EXPECT_FALSE(checkOutput(tight, r).allowed);
    tight = e.safety;
    tight.allowedOutputChannels.clear();
    EXPECT_FALSE(checkOutput(tight, r).allowed);
    EXPECT_FALSE(checkOutput(SafetyLimits{}, r).allowed);  // the default permits nothing
    // No operator, no acknowledgement: refused.
    EXPECT_FALSE(authority.grant(r, e.safety, "", "ok").ok());
    EXPECT_FALSE(authority.grant(r, e.safety, "op", "").ok());
    // Granted, consumed once, spent after release.
    auto a = authority.grant(r, e.safety, "op", "checked");
    ASSERT_TRUE(a.ok());
    EXPECT_TRUE(authority.isValid(a.value()));
    ASSERT_TRUE(authority.consume(a.value()).ok());
    EXPECT_TRUE(authority.isValid(a.value()));
    EXPECT_FALSE(authority.consume(a.value()).ok());
    authority.release(a.value());
    EXPECT_FALSE(authority.isValid(a.value()));
    // Expiry.
    auto shortLived = authority.grant(r, e.safety, "op", "checked", std::chrono::seconds(0));
    ASSERT_TRUE(shortLived.ok());
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    EXPECT_FALSE(authority.isValid(shortLived.value()));
    // The emergency stop latches and invalidates everything.
    auto b = authority.grant(r, e.safety, "op", "checked");
    ASSERT_TRUE(b.ok());
    authority.emergencyStop("test");
    EXPECT_FALSE(authority.isValid(b.value()));
    EXPECT_FALSE(authority.grant(r, e.safety, "op", "checked").ok());
    authority.resetEmergencyStop("op");
    EXPECT_TRUE(authority.grant(r, e.safety, "op", "checked").ok());
    // RF transmission needs a power limit and always carries a regulatory warning.
    OutputRequest rf;
    rf.domain = PhysicsDomain::Electromagnetic;
    rf.channels = {"tx"};
    rf.transmitPowerDbm = -20.0;
    SafetyLimits rfLimits;
    rfLimits.allowedOutputChannels = {"tx"};
    EXPECT_FALSE(checkOutput(rfLimits, rf).allowed);
    rfLimits.maxTransmitPowerDbm = -10.0;
    const auto check = checkOutput(rfLimits, rf);
    EXPECT_TRUE(check.allowed);
    ASSERT_FALSE(check.warnings.empty());
    EXPECT_NE(check.warnings.front().find("does not establish regulatory compliance"), std::string::npos);
}

TEST(Experiments, SimulatedDevicesBehaveLikeInstruments)
{
    auto registry = simulatedRegistry();
    EXPECT_EQ(registry->knownDevices().size(), 6u);
    auto daq = registry->connect("sim:daq-4x1");
    ASSERT_TRUE(daq.ok());
    EXPECT_EQ(daq.value(), registry->connect("sim:daq-4x1").value());  // one connection per device
    const auto caps = daq.value()->capabilities();
    EXPECT_EQ(caps.inputs.size(), 4u);
    EXPECT_TRUE(caps.hardwareTimestamps);
    EXPECT_FALSE(registry->connect("sim:nothing").ok());
    // Not configured: no acquisition. An unsupported rate or channel: refused.
    EXPECT_FALSE(daq.value()->startAcquisition(100).ok());
    DeviceConfiguration c;
    c.sampleRateHz = 500.0;
    c.inputChannels = {"ai0"};
    EXPECT_FALSE(daq.value()->configure(c).ok());
    c.sampleRateHz = 8192.0;
    c.inputChannels = {"ai9"};
    EXPECT_FALSE(daq.value()->configure(c).ok());
    c.inputChannels = {"ai0", "ai1"};
    c.outputChannels = {"ao0"};
    ASSERT_TRUE(daq.value()->configure(c).ok());
    EXPECT_EQ(daq.value()->status(), DeviceStatus::Configured);
    // Output without an authorization is refused.
    OutputSignal signal;
    signal.channels = {std::vector<double>(100, 0.1)};
    OutputAuthorization fake;
    fake.token = "forged";
    EXPECT_FALSE(daq.value()->startOutput(signal, fake).ok());
    // Discovery runs on its own thread.
    DeviceRegistry async;
    async.registerDriver(std::make_shared<SimulatedDriver>());
    std::atomic<bool> done{false};
    size_t found = 0;
    async.discoverAsync([&](const DiscoveryReport& r) {
        found = r.devices.size();
        done = true;
    });
    for (int i = 0; i < 200 && !done; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    EXPECT_TRUE(done.load());
    EXPECT_EQ(found, 6u);
}

TEST(Experiments, AcquisitionRecordsEverythingAndReportsFaults)
{
    const auto dir = scratch("acquisition");
    auto store = openMeasurementStore(dir);
    ASSERT_TRUE(store.ok());
    EXPECT_EQ(store.value()->format(), hdf5Available() ? StoreFormat::Hdf5 : StoreFormat::Binary);
    auto registry = simulatedRegistry();
    auto daq = registry->connect("sim:daq-4x1").value();
    auto plan = [&](const std::string& suffix) {
        AcquisitionPlan p;
        p.frames = 8192;
        p.queueBlocks = 4;
        for (const char* ch : {"ai0", "ai1"}) {
            ChannelPlan cp;
            cp.rangeMin = -10;
            cp.rangeMax = 10;
            cp.record.id = std::string("m-") + ch + suffix;
            cp.record.experimentId = "exp";
            cp.record.channelId = ch;
            cp.record.timeBase.sampleRateHz = 8192.0;
            p.channels.push_back(cp);
        }
        return p;
    };
    DeviceConfiguration c;
    c.sampleRateHz = 8192.0;
    c.inputChannels = {"ai0", "ai1"};
    c.blockFrames = 512;
    ASSERT_TRUE(daq->configure(c).ok());
    {
        AcquisitionEngine engine(daq, plan("-clean"), store.value().get());
        ASSERT_TRUE(engine.start().ok());
        ASSERT_TRUE(engine.wait(std::chrono::seconds(20)));
        auto out = engine.takeOutput();
        EXPECT_FALSE(out.failed) << out.failure;
        EXPECT_TRUE(out.quality.clean()) << out.quality.summary();
        ASSERT_EQ(out.records.size(), 2u);
        EXPECT_EQ(out.real[0].size(), 8192u);
        EXPECT_LE(out.stats.maxQueueOccupancy, 4u);
        // The stored raw data equals what was recorded, and cannot be written again.
        auto data = store.value()->read(out.records[0].dataRef);
        ASSERT_TRUE(data.ok()) << data.message();
        EXPECT_EQ(data.value().real, out.real[0]);
        auto meta = store.value()->readRecord(out.records[0].dataRef);
        ASSERT_TRUE(meta.ok()) << meta.message();
        EXPECT_TRUE(meta.value().quality.complete);
        EXPECT_FALSE(store.value()->createStream(out.records[0], false).ok());
        EXPECT_FALSE(store.value()->write(out.records[0], data.value()).ok());
    }
    // Dropped frames and a timestamp jump are detected; the gap is stored as NaN.
    c.options = {{"dropAfterBlocks", 3}, {"dropFrames", 300}, {"timestampGlitchAfterBlocks", 6}};
    ASSERT_TRUE(daq->configure(c).ok());
    {
        AcquisitionEngine engine(daq, plan("-faulty"), store.value().get());
        ASSERT_TRUE(engine.start().ok());
        ASSERT_TRUE(engine.wait(std::chrono::seconds(20)));
        auto out = engine.takeOutput();
        EXPECT_EQ(out.quality.droppedFrames, 300u);
        EXPECT_EQ(out.quality.gaps, 1u);
        EXPECT_GE(out.quality.timestampDiscontinuities, 1u);
        EXPECT_FALSE(out.quality.clean());
        EXPECT_TRUE(std::isnan(out.real[0][512 * 3 + 10]));
        EXPECT_FALSE(std::isnan(out.real[0][512 * 3 + 300 + 10]));
    }
    // A disconnection ends the acquisition with an error; what was recorded stays readable, marked incomplete.
    c.options = {{"disconnectAfterBlocks", 4}};
    ASSERT_TRUE(daq->configure(c).ok());
    {
        AcquisitionEngine engine(daq, plan("-lost"), store.value().get());
        ASSERT_TRUE(engine.start().ok());
        ASSERT_TRUE(engine.wait(std::chrono::seconds(20)));
        auto out = engine.takeOutput();
        EXPECT_TRUE(out.failed);
        EXPECT_NE(out.failure.find("disconnected"), std::string::npos);
        EXPECT_FALSE(out.quality.complete);
        auto meta = store.value()->readRecord(out.records[0].dataRef);
        ASSERT_TRUE(meta.ok());
        EXPECT_FALSE(meta.value().quality.complete);
        EXPECT_EQ(store.value()->read(out.records[0].dataRef).value().real.size(), 4u * 512u);
        EXPECT_EQ(daq->status(), DeviceStatus::Error);
    }
}

TEST(Experiments, BinaryStoreKeepsTheSameGuarantees)
{
    const auto dir = scratch("binary-store");
    auto store = openMeasurementStore(dir, StoreFormat::Binary);
    ASSERT_TRUE(store.ok());
    MeasurementRecord r;
    r.id = "m-1";
    r.experimentId = "e";
    r.kind = MeasurementKind::Spectrum;
    MeasurementData d;
    d.axis = {1.0, 2.0};
    d.complex = {{1.0, 2.0}, {3.0, 4.0}};
    ASSERT_TRUE(store.value()->write(r, d).ok());
    EXPECT_FALSE(store.value()->write(r, d).ok());
    auto back = store.value()->read(store.value()->dataRefFor(r));
    ASSERT_TRUE(back.ok());
    EXPECT_EQ(back.value().complex, d.complex);
    EXPECT_EQ(back.value().axis, d.axis);
    EXPECT_FALSE(store.value()->read("../outside").ok());
}

TEST(Experiments, RunnerRefusesOutputWithoutAuthorizationAndRunsWithIt)
{
    const auto dir = scratch("runner");
    auto store = openMeasurementStore(dir).take();
    auto registry = simulatedRegistry();
    CalibrationLibrary calibrations;
    ExperimentRunner runner(*registry, calibrations, store.get());
    Experiment e = daqExperiment(true);
    RunResult refused = runner.run(e, {});
    EXPECT_EQ(refused.state, ExperimentState::Failed);
    EXPECT_NE(refused.record.message.find("explicit output authorization"), std::string::npos);
    EXPECT_TRUE(refused.raw.empty());
    ASSERT_GE(refused.record.steps.size(), 6u);
    EXPECT_EQ(refused.record.steps.back().step, 6);

    RunOptions options;
    options.authorization = authorize(e);
    options.operatorName = "test operator";
    std::vector<ExperimentState> states;
    options.progress = [&](const RunProgress& p) {
        if (states.empty() || states.back() != p.state)
            states.push_back(p.state);
    };
    RunResult run = runner.run(e, options);
    ASSERT_EQ(run.state, ExperimentState::Completed) << run.record.message;
    EXPECT_TRUE(run.record.outputAuthorized);
    EXPECT_EQ(states.front(), ExperimentState::Validating);
    EXPECT_NE(std::find(states.begin(), states.end(), ExperimentState::Armed), states.end());
    EXPECT_NE(std::find(states.begin(), states.end(), ExperimentState::Running), states.end());
    EXPECT_EQ(states.back(), ExperimentState::Completed);
    ASSERT_EQ(run.raw.size(), 3u);  // the command and two inputs
    EXPECT_EQ(run.raw[0].record.category, DataCategory::Command);
    EXPECT_TRUE(run.raw[1].record.simulatedDevice);
    // The shaker moved the structure: the load cell saw the 120 Hz drive.
    EXPECT_GT(std::abs(dsp::toneAmplitude(run.raw[1].data.real, 8192.0, 120.0)), 1e-3);
    EXPECT_FALSE(run.reportHtml.empty());
    EXPECT_NE(run.reportHtml.find("Simulated devices"), std::string::npos);
    EXPECT_TRUE(fs::exists(store->root() / e.id / ("report-" + run.record.runId + ".html")));
    // The authorization was spent by that run.
    RunResult again = runner.run(e, options);
    EXPECT_EQ(again.state, ExperimentState::Failed);
    // A changed experiment needs a new authorization.
    RunOptions changed;
    changed.authorization = authorize(e);
    e.stimulus.amplitude = 1.5;
    EXPECT_EQ(runner.run(e, changed).state, ExperimentState::Failed);
}

TEST(Experiments, CancellationAndEmergencyStopEndRunsCleanly)
{
    const auto dir = scratch("cancel");
    auto store = openMeasurementStore(dir).take();
    auto registry = simulatedRegistry();
    CalibrationLibrary calibrations;
    auto runner = std::make_shared<ExperimentRunner>(*registry, calibrations, store.get());
    Experiment e = daqExperiment(true);
    e.acquisition.durationSeconds = 20.0;
    e.stimulus.durationSeconds = 20.0;
    e.devices["sim:daq-4x1"].options = {{"realTime", true}};
    for (bool emergency : {false, true}) {
        RunOptions options;
        options.authorization = authorize(e);
        RunResult result;
        std::thread t([&] { result = runner->run(e, options); });
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        if (emergency)
            OutputAuthority::instance().emergencyStop("test");
        else
            runner->cancel();
        t.join();
        EXPECT_EQ(result.state, ExperimentState::Cancelled) << result.record.message;
        ASSERT_FALSE(result.raw.empty());
        const auto& input = result.raw.back();
        EXPECT_FALSE(input.record.quality.complete);
        EXPECT_LT(input.data.real.size(), 20u * 8192u);
        auto meta = store->readRecord(input.record.dataRef);
        ASSERT_TRUE(meta.ok());
        EXPECT_FALSE(meta.value().quality.complete);
        auto device = registry->connected("sim:daq-4x1");
        EXPECT_NE(device->status(), DeviceStatus::Transmitting);
        OutputAuthority::instance().resetEmergencyStop("test");
    }
}

TEST(Experiments, ProjectsKeepExperimentsButNeverAuthorizations)
{
    auto project = Project::createNew("experiments");
    ExperimentWorkspace w;
    ASSERT_TRUE(w.addExperiment(daqExperiment(true)).ok());
    EXPECT_FALSE(w.addExperiment(daqExperiment(true)).ok());
    CalibrationRecord c;
    c.id = "cal-1";
    c.scalar = 0.01;
    c.unit = "V/(m/s^2)";
    ASSERT_TRUE(w.calibrations.add(c).ok());
    MeasurementRecord m;
    m.id = "m-1";
    m.experimentId = "exp-test";
    ASSERT_TRUE(w.addMeasurement(m).ok());
    EXPECT_FALSE(w.addMeasurement(m).ok());
    storeExperiments(*project, w);
    auto loaded = deserializeProject(serializeProject(*project));
    ASSERT_TRUE(loaded.ok()) << loaded.message();
    auto back = loadExperiments(*loaded.value().project);
    ASSERT_TRUE(back.ok()) << back.message();
    EXPECT_EQ(toJson(back.value()), toJson(w));
    EXPECT_EQ(serializeProject(*loaded.value().project).dump().find("token"), std::string::npos);
    // A project without experiments has no key, and loads an empty workspace.
    auto empty = Project::createNew("plain");
    storeExperiments(*empty, ExperimentWorkspace{});
    EXPECT_FALSE(empty->extensions.contains(kExperimentsKey));
    EXPECT_TRUE(loadExperiments(*empty).value().experiments.empty());
}

TEST(Experiments, ScansAndReconstructionMarkWhatWasMeasured)
{
    ScanDefinition planar;
    planar.kind = ScanKind::Planar;
    planar.nu = 4;
    planar.nv = 3;
    planar.axisU[0] = 0.1;
    planar.axisV[1] = 0.1;
    EXPECT_EQ(scanPositions(planar).value().size(), 12u);
    ScanDefinition angular;
    angular.kind = ScanKind::Angular;
    angular.stepDeg = 30.0;
    std::vector<double> angles;
    auto ring = scanPositions(angular, &angles);
    ASSERT_TRUE(ring.ok());
    EXPECT_EQ(ring.value().size(), 12u);  // 360 is 0 again
    EXPECT_NEAR(ring.value()[3].y, 1.0, 1e-12);
    EXPECT_NEAR(ring.value()[3].dy, -1.0, 1e-12);  // pointing at the centre
    auto csv = importPositionsCsv("x,y,z\n0,0,0\n0.1,0,0 # comment\n", "pos.csv");
    ASSERT_FALSE(csv.ok());  // a trailing comment on a data line is not a number
    csv = importPositionsCsv("# positions\nx,y,z\n0,0,0\n0.1,0,0\n0.2,0,0,0,0,2\n", "pos.csv");
    ASSERT_TRUE(csv.ok()) << csv.message();
    EXPECT_EQ(csv.value().points.size(), 3u);
    EXPECT_NEAR(csv.value().points[2].dz, 1.0, 1e-12);

    SpatialDataset d;
    d.quantity = {Quantity::Pressure, std::nullopt, false};
    for (int i = 0; i < 3; ++i) {
        SpatialSample s;
        s.pose.x = 0.1 * i;
        s.value = {1.0 + i, 0.0};
        d.samples.push_back(s);
    }
    const double origin[3] = {0, 0, 0}, u[3] = {0.05, 0, 0}, v[3] = {0, 0.05, 0};
    auto r = reconstructPlane(d, origin, u, v, 9, 3, 0.06, 0.001, 1e-6);
    ASSERT_TRUE(r.ok()) << r.message();
    EXPECT_EQ(r.value().coverage[0], NodeCoverage::Measured);
    EXPECT_EQ(r.value().coverage[1], NodeCoverage::Interpolated);  // x = 0.05, between two samples
    EXPECT_NEAR(r.value().values[1].real(), 1.5, 1e-12);
    EXPECT_EQ(r.value().coverage[8], NodeCoverage::Empty);  // x = 0.4: beyond every sample's radius
    EXPECT_EQ(r.value().values[8], std::complex<double>(0.0, 0.0));
    EXPECT_EQ(r.value().measured, 3u);
    EXPECT_EQ(r.value().measured + r.value().interpolated + r.value().empty, 27u);
    d.samples[0].pose.frame = "probe";
    EXPECT_FALSE(reconstructPlane(d, origin, u, v, 9, 3, 0.06, 0.001, 1e-6).ok());
}

TEST(Experiments, ComparisonRefusesIncompatibleQuantitiesAndComparesOnlyTheOverlap)
{
    FrequencySeries m, s;
    m.quantity = {Quantity::Pressure, Quantity::Voltage, false};
    s.quantity = m.quantity;
    for (int i = 0; i <= 10; ++i) {
        m.frequencyHz.push_back(100.0 * i + 100.0);
        m.values.push_back({1.0, 0.0});
    }
    for (int i = 0; i <= 5; ++i) {
        s.frequencyHz.push_back(200.0 * i);
        s.values.push_back(std::polar(std::pow(10.0, 1.0 / 20.0), 0.1));
    }
    auto c = compareFrequencySeries(m, s);
    ASSERT_TRUE(c.ok()) << c.message();
    EXPECT_EQ(c.value().points, 10u);  // 1100 Hz is outside the simulation
    EXPECT_NEAR(c.value().rmsMagnitudeErrorDb, 1.0, 1e-9);
    EXPECT_NEAR(c.value().rmsPhaseErrorDeg, 0.1 * 180.0 / 3.14159265358979, 1e-6);
    EXPECT_EQ(c.value().fractionWithinUncertainty, -1.0);
    s.quantity = {Quantity::Pressure, std::nullopt, false};
    EXPECT_FALSE(compareFrequencySeries(m, s).ok());
    // An uncalibrated reading only in shape, and only when asked.
    m.quantity = {Quantity::DigitalFullScale, Quantity::Voltage, false};
    s.quantity = {Quantity::Pressure, Quantity::Voltage, false};
    EXPECT_FALSE(compareFrequencySeries(m, s).ok());
    ComparisonOptions relative;
    relative.relative = true;
    auto r = compareFrequencySeries(m, s, relative);
    ASSERT_TRUE(r.ok()) << r.message();
    EXPECT_NEAR(r.value().rmsMagnitudeErrorDb, 0.0, 1e-9);
    EXPECT_TRUE(r.value().relative);
}

TEST(Experiments, ProcessingIsReproducibleAndArchivesReproduceIt)
{
    const auto dir = scratch("archive");
    auto store = openMeasurementStore(dir / "project").take();
    auto registry = simulatedRegistry();
    ExperimentWorkspace w;
    Experiment e = daqExperiment(true);
    ProcessingStep tf;
    tf.operation = "transfer-function";
    tf.parameters = {{"reference", "ch-f"}, {"segmentLength", 1024}, {"bindings", {"ch-a"}}};
    ProcessingStep tone;
    tone.operation = "tone";
    tone.parameters = {{"frequencyHz", 120.0}, {"bindings", {"ch-f"}}};
    e.processing = {tf, tone};
    ASSERT_TRUE(w.addExperiment(e).ok());
    ExperimentRunner runner(*registry, w.calibrations, store.get());
    RunOptions options;
    options.authorization = authorize(e);
    RunResult run = runner.run(e, options);
    ASSERT_EQ(run.state, ExperimentState::Completed) << run.record.message;
    ASSERT_EQ(run.derived.size(), 2u);
    EXPECT_EQ(run.derived[0].record.kind, MeasurementKind::TransferFunction);
    EXPECT_EQ(run.derived[0].record.quantity.unit(), "V/V");
    EXPECT_EQ(run.derived[0].record.sourceIds.size(), 2u);
    EXPECT_EQ(run.derived[0].record.history.back().operation, "transfer-function");
    EXPECT_FALSE(run.derived[0].record.history.back().description.empty());
    // Deterministic ids: the same sources and step give the same id.
    EXPECT_EQ(run.derived[0].record.id, derivedId(run.derived[0].record.sourceIds, tf));
    for (const auto& item : run.raw)
        ASSERT_TRUE(w.addMeasurement(item.record).ok());
    for (const auto& item : run.derived)
        ASSERT_TRUE(w.addMeasurement(item.record).ok());
    auto reproduced = reproduceProcessing(w, e.id, *store);
    ASSERT_TRUE(reproduced.ok());
    EXPECT_TRUE(reproduced.value().ok()) << (reproduced.value().problems.empty() ? "" : reproduced.value().problems.front());
    EXPECT_EQ(reproduced.value().compared, 2u);
    // Export, import elsewhere, reproduce again without the original store or hardware.
    auto exported = exportExperiment(w, e.id, *store, dir / "archive");
    ASSERT_TRUE(exported.ok()) << exported.message();
    EXPECT_EQ(exported.value().dataSets, 5u);
    EXPECT_FALSE(exportExperiment(w, e.id, *store, dir / "archive").ok());  // never overwritten
    auto imported = importExperiment(dir / "archive");
    ASSERT_TRUE(imported.ok()) << imported.message();
    auto again = reproduceProcessing(imported.value().workspace, e.id, *imported.value().store);
    ASSERT_TRUE(again.ok());
    EXPECT_TRUE(again.value().ok());
    // Merged into another project's store.
    ExperimentWorkspace other;
    auto otherStore = openMeasurementStore(dir / "other").take();
    ASSERT_TRUE(mergeArchive(other, *otherStore, imported.value()).ok());
    EXPECT_EQ(other.measurements.size(), 5u);
    EXPECT_FALSE(mergeArchive(other, *otherStore, imported.value()).ok());
}
