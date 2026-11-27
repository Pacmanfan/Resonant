// Domain workflows of the experiment framework on simulated devices
// (REQ-CHAT-20261126-008): what each procedure recovers is compared with
// the synthetic model the simulated device was given.
#include "acoustics/AcousticExperiments.h"
#include "electromagnetics/RfExperiments.h"
#include "experiments/ExperimentRunner.h"
#include "experiments/SimulatedHardware.h"
#include "mechanics/MechanicalExperiments.h"

#include <gtest/gtest.h>

#include <cmath>
#include <numbers>

using namespace resonant;
using namespace resonant::experiments;

namespace {

struct Bench {
    DeviceRegistry registry;
    CalibrationLibrary calibrations;
    Bench()
    {
        registry.registerDriver(std::make_shared<SimulatedDriver>());
        registry.discover();
    }
    RunResult run(const Experiment& e)
    {
        ExperimentRunner runner(registry, calibrations, nullptr);
        RunOptions options;
        if (e.hasPhysicalOutput()) {
            auto a = OutputAuthority::instance().grant(outputRequestFor(e), e.safety, "test operator", "simulated bench");
            EXPECT_TRUE(a.ok()) << a.message();
            options.authorization = a.value();
        }
        return runner.run(e, options);
    }
};

const ProcessingItem* derivedFor(const RunResult& r, const std::string& binding, MeasurementKind kind)
{
    for (const auto& item : r.derived)
        if (item.record.bindingId == binding && item.record.kind == kind)
            return &item;
    return nullptr;
}

}  // namespace

TEST(ExperimentDomains, SpeakerCharacterizationMeasuresTheSimulatedChain)
{
    Bench bench;
    const Experiment e = acoustics::speakerCharacterizationExperiment("speaker");
    ASSERT_TRUE(validateExperiment(e).ok());
    RunResult r = bench.run(e);
    ASSERT_EQ(r.state, ExperimentState::Completed) << r.record.message;
    const ProcessingItem* h = derivedFor(r, "ch-mic", MeasurementKind::TransferFunction);
    ASSERT_NE(h, nullptr);
    EXPECT_EQ(h->record.quantity.unit(), "FS/FS");
    // The simulated chain: speaker 2 Pa/FS at 1 m, mic 0.0158 FS/Pa, 1 m: 0.0316 FS/FS, with
    // the speaker's 80 Hz high-pass and the 1 m delay.
    for (double f : {1000.0, 4000.0}) {
        size_t k = 0;
        while (h->data.axis[k] < f)
            ++k;
        const double expected = simulated::kSpeakerPaPerFsAt1m * simulated::kMicFsPerPa;
        EXPECT_NEAR(20 * std::log10(std::abs(h->data.complex[k]) / expected), 0.0, 0.3) << f;
        EXPECT_GT(h->data.auxiliary[k], 0.99);
    }
    // With the microphone calibrated (its model sensitivity, marked as from a simulated
    // device), the measured response compares with the free-field prediction; the
    // prediction has no loudspeaker high-pass, which shows at low frequencies.
    CalibrationRecord mic;
    mic.id = "cal-sim-mic";
    mic.sensorId = "mic-1";
    mic.scalar = simulated::kMicFsPerPa;
    mic.unit = "FS/Pa";
    mic.fromSimulatedDevice = true;
    ProcessingStep cal;
    cal.operation = "calibrate";
    cal.parameters = {{"calibrationId", "cal-sim-mic"}};
    CalibrationLibrary lib;
    ASSERT_TRUE(lib.add(mic).ok());
    ProcessingContext ctx;
    ctx.calibrations = &lib;
    ctx.sensorQuantity["mic-1"] = Quantity::Pressure;
    auto calibrated = runStep(cal, {h}, ctx);
    ASSERT_TRUE(calibrated.ok()) << calibrated.message();
    EXPECT_EQ(calibrated.value().record.quantity.unit(), "Pa/FS");
    FrequencySeries measured;
    measured.quantity = calibrated.value().record.quantity;
    for (size_t k = 0; k < calibrated.value().data.axis.size(); ++k)
        if (calibrated.value().data.axis[k] >= 100.0 && calibrated.value().data.axis[k] <= 12000.0) {
            measured.frequencyHz.push_back(calibrated.value().data.axis[k]);
            measured.values.push_back(calibrated.value().data.complex[k]);
        }
    acoustics::Medium air;
    air.soundSpeed = simulated::kSpeedOfSound;
    std::vector<double> grid;
    for (double f = 50.0; f <= 15000.0; f += 10.0)
        grid.push_back(f);
    const auto predicted = acoustics::freeFieldResponse(Vec3(0, 0, 0), Vec3(0, 1, 0), simulated::kSpeakerPaPerFsAt1m,
                                                        Quantity::DigitalFullScale, grid, air);
    auto above = measured;
    above.frequencyHz.erase(above.frequencyHz.begin(),
                            std::find_if(above.frequencyHz.begin(), above.frequencyHz.end(), [](double f) { return f >= 1000.0; }));
    above.values.erase(above.values.begin(), above.values.begin() + (measured.values.size() - above.frequencyHz.size()));
    auto c = compareFrequencySeries(above, predicted);
    ASSERT_TRUE(c.ok()) << c.message();
    EXPECT_LT(c.value().rmsMagnitudeErrorDb, 0.3);
    EXPECT_LT(c.value().rmsPhaseErrorDeg, 10.0);
    auto all = compareFrequencySeries(measured, predicted);
    ASSERT_TRUE(all.ok());
    EXPECT_GT(all.value().maxMagnitudeErrorDb, 1.2);  // the 80 Hz high-pass: -1.5 dB at 100 Hz
}

TEST(ExperimentDomains, CalibratorGivesTheMicrophoneSensitivity)
{
    const double fs = 48000.0;
    const double pa = 20e-6 * std::pow(10.0, 94.0 / 20.0);
    std::vector<double> recording(fs);
    for (size_t i = 0; i < recording.size(); ++i)
        recording[i] = 0.02 * pa * std::sqrt(2.0) * std::sin(2 * std::numbers::pi * 1000.0 * static_cast<double>(i) / fs);
    auto c = acoustics::calibratorSensitivity(recording, fs, 94.0, 1000.0, 0.2, Quantity::DigitalFullScale);
    ASSERT_TRUE(c.ok()) << c.message();
    EXPECT_NEAR(c.value().scalar, 0.02, 1e-5);
    EXPECT_EQ(c.value().unit, "FS/Pa");
    EXPECT_NEAR(acoustics::soundPressureLevelDb(pa), 94.0, 1e-9);
    EXPECT_NEAR(c.value().uncertainty.combinedRelative(), std::pow(10.0, 0.2 / 20.0) - 1.0, 1e-4);
}

TEST(ExperimentDomains, FieldMappingAgreesWithTheAcousticModelInShape)
{
    Bench bench;
    const Experiment e = acoustics::fieldMappingExperiment("field", 2000.0);
    RunResult r = bench.run(e);
    ASSERT_EQ(r.state, ExperimentState::Completed) << r.record.message;
    ASSERT_EQ(r.spatial.size(), 1u);
    const SpatialDataset& d = r.spatial.front();
    EXPECT_EQ(d.samples.size(), 7u);
    EXPECT_EQ(d.quantity.unit(), "FS");
    // The predicted field of a point source at the loudspeaker.
    acoustics::Medium air;
    air.soundSpeed = simulated::kSpeedOfSound;
    acoustics::PointSource source;
    source.frequencyHz = 2000.0;
    source.forward = Vec3::UnitY();
    auto evaluator = std::make_shared<const acoustics::FieldEvaluator>(std::vector<acoustics::PointSource>{source}, 2000.0, air,
                                                                       std::make_shared<const acoustics::FreeFieldModel>());
    // Uncalibrated (FS) against Pa: only in shape, and only when asked.
    EXPECT_FALSE(compareSpatial(d, acoustics::fieldSampler(evaluator), {Quantity::Pressure, std::nullopt, false}).ok());
    SpatialComparisonOptions shape;
    shape.relative = true;
    auto c = compareSpatial(d, acoustics::fieldSampler(evaluator), {Quantity::Pressure, std::nullopt, false}, shape);
    ASSERT_TRUE(c.ok()) << c.message();
    EXPECT_LT(c.value().rmsMagnitudeErrorDb, 0.2);
    EXPECT_LT(c.value().rmsPhaseErrorDeg, 3.0);
    EXPECT_TRUE(c.value().relative);
}

TEST(ExperimentDomains, FocusingPutsTheMaximumAtTheTarget)
{
    Bench bench;
    const Vec3 focus(0.2, 1.0, 0.0);
    const Experiment e = acoustics::focusingValidationExperiment("focus", focus);
    ASSERT_TRUE(validateExperiment(e).ok());
    RunResult r = bench.run(e);
    ASSERT_EQ(r.state, ExperimentState::Completed) << r.record.message;
    ASSERT_EQ(r.spatial.size(), 1u);
    auto m = acoustics::focusMetrics(r.spatial.front());
    ASSERT_TRUE(m.ok()) << m.message();
    EXPECT_NEAR(m.value().peakPosition.x(), focus.x(), 0.051);  // the nearest microphone (0.1 m spacing)
    // The same two sources with the same delays, predicted.
    acoustics::Medium air;
    air.soundSpeed = simulated::kSpeedOfSound;
    std::vector<acoustics::PointSource> sources(2);
    const double w = 2 * std::numbers::pi * 4000.0;
    sources[0].position = Vec3(0, 0, 0);
    sources[1].position = Vec3(0.5, 0, 0);
    for (int i = 0; i < 2; ++i) {
        sources[i].frequencyHz = 4000.0;
        sources[i].forward = Vec3::UnitY();
        const double delay = e.stimulus.channelDelaySeconds.at(i == 0 ? "ch-spk" : "ch-spk-2");
        sources[i].amplitude = std::polar(simulated::kSpeakerPaPerFsAt1m, -w * delay);
    }
    auto evaluator = std::make_shared<const acoustics::FieldEvaluator>(sources, 4000.0, air, std::make_shared<const acoustics::FreeFieldModel>());
    auto c = acoustics::compareFocus(r.spatial.front(), acoustics::fieldSampler(evaluator));
    ASSERT_TRUE(c.ok()) << c.message();
    EXPECT_LT(c.value().locationErrorM, 1e-9);
}

TEST(ExperimentDomains, ModalTestIdentifiesTheSimulatedModes)
{
    Bench bench;
    Experiment e = mechanics::modalTestExperiment("modal");
    // The nominal data-sheet sensitivities as calibrations (from the simulated device: test only).
    auto add = [&](const std::string& sensor, double value, const std::string& unit) {
        CalibrationRecord c;
        c.id = "cal-" + sensor;
        c.sensorId = sensor;
        c.scalar = value;
        c.unit = unit;
        c.fromSimulatedDevice = true;
        ASSERT_TRUE(bench.calibrations.add(c).ok());
        for (auto& s : e.sensors)
            if (s.id == sensor)
                s.calibrationId = c.id;
    };
    add("load-cell", simulated::kLoadCellVoltPerNewton, "V/N");
    for (const char* a : {"acc-1", "acc-2", "acc-3"})
        add(a, simulated::kAccelerometerVoltPerMs2, "V/m/s^2");
    RunResult r = bench.run(e);
    ASSERT_EQ(r.state, ExperimentState::Completed) << r.record.message;
    std::vector<ProcessingItem> frfs;
    for (const char* b : {"ch-acc-1", "ch-acc-2", "ch-acc-3"}) {
        const ProcessingItem* h = derivedFor(r, b, MeasurementKind::TransferFunction);
        ASSERT_NE(h, nullptr) << b;
        EXPECT_EQ(h->record.quantity.unit(), "m/s^2/N");
        frfs.push_back(*h);
    }
    auto modes = mechanics::identifyModes(frfs[1], 0.05, 0.8, 20.0);
    ASSERT_TRUE(modes.ok()) << modes.message();
    ASSERT_GE(modes.value().size(), 2u);
    std::vector<double> found;
    for (const auto& m : modes.value())
        found.push_back(m.frequencyHz);
    std::sort(found.begin(), found.end());
    // The simulated structure: 120 Hz (zeta 0.02) and 410 Hz (zeta 0.015); resolution 2 Hz.
    const auto near = [&](double f) { return std::any_of(found.begin(), found.end(), [&](double x) { return std::abs(x - f) < 2.0; }); };
    EXPECT_TRUE(near(120.0));
    EXPECT_TRUE(near(410.0));
    for (const auto& m : modes.value()) {
        if (std::abs(m.frequencyHz - 120.0) < 2.0) {
            EXPECT_NEAR(m.dampingRatio, 0.02, 0.006);
        }
    }
    // Operating shapes at the two modes are distinct (low MAC between them).
    auto a = mechanics::operatingShape(frfs, 120.0);
    auto b = mechanics::operatingShape(frfs, 410.0);
    ASSERT_TRUE(a.ok() && b.ok());
    EXPECT_NEAR(mechanics::modalAssuranceCriterion(a.value(), a.value()), 1.0, 1e-12);
    EXPECT_LT(mechanics::modalAssuranceCriterion(a.value(), b.value()), 0.9);
    // Paired with "simulated" modes by frequency.
    auto pairs = mechanics::pairModes(modes.value(), {}, {118.0, 415.0}, {});
    ASSERT_GE(pairs.size(), 2u);
    for (const auto& p : pairs)
        EXPECT_LT(std::abs(p.frequencyErrorPercent), 3.0);
    // Accelerance -> receptance: the quantity follows.
    ProcessingStep integrate;
    integrate.operation = "integrate-frequency";
    integrate.parameters = {{"times", 2}};
    auto receptance = runStep(integrate, {&frfs[0]}, {});
    ASSERT_TRUE(receptance.ok()) << receptance.message();
    EXPECT_EQ(receptance.value().record.quantity.unit(), "m/N");
}

TEST(ExperimentDomains, StressIsInferredWithAStatedModel)
{
    ProcessingItem strain;
    strain.record.id = "m-strain";
    strain.record.quantity = {Quantity::Strain, std::nullopt, false};
    strain.record.timeBase.sampleRateHz = 1000.0;
    strain.data.real = {1e-4, 2e-4};
    auto stress = runStep(mechanics::inferStressStep(200e9, "steel"), {&strain}, {});
    ASSERT_TRUE(stress.ok()) << stress.message();
    EXPECT_EQ(stress.value().record.quantity.quantity, Quantity::Stress);
    EXPECT_NEAR(stress.value().data.real[1], 4e7, 1e-3);
    EXPECT_NE(stress.value().record.history.back().description.find("not a measured one"), std::string::npos);
}

TEST(ExperimentDomains, TouchstoneFilesOfAnyPortCount)
{
    // Two ports: Touchstone 1 writes S11 S21 S12 S22.
    auto two = em::parseTouchstone("# HZ S RI R 50\n1e9 0.1 0 0.9 0 0.2 0 0.3 0\n2e9 0.1 0 0.8 0 0.2 0 0.3 0\n", 2);
    ASSERT_TRUE(two.ok()) << two.message();
    EXPECT_NEAR(two.value().at(0, 2, 1).real(), 0.9, 1e-12);
    EXPECT_NEAR(two.value().at(0, 1, 2).real(), 0.2, 1e-12);
    EXPECT_EQ(two.value().sourceFormat, "RI");
    // Version 2 with the other two-port order and per-port references.
    auto v2 = em::parseTouchstone("[Version] 2.0\n# GHz S MA R 50\n[Number of Ports] 2\n[Two-Port Data Order] 12_21\n"
                                  "[Number of Frequencies] 1\n[Reference] 50 75\n[Network Data]\n1 0.1 0 0.2 0 0.9 0 0.3 0\n[End]\n");
    ASSERT_TRUE(v2.ok()) << v2.message();
    EXPECT_NEAR(v2.value().at(0, 2, 1).real(), 0.9, 1e-12);
    EXPECT_EQ(v2.value().referenceImpedance, (std::vector<double>{50.0, 75.0}));
    // Three ports over several lines, row by row.
    std::string three = "# MHZ S DB R 50\n100 -20 0 -3 90 -40 0\n -3 90 -20 0 -40 0\n -40 0 -40 0 -15 45\n";
    EXPECT_EQ(em::portsFromFileName("divider.S3P"), 3);
    auto s3 = em::parseTouchstone(three, em::portsFromFileName("divider.s3p"));
    ASSERT_TRUE(s3.ok()) << s3.message();
    EXPECT_NEAR(std::abs(s3.value().at(0, 1, 2)), std::pow(10.0, -3.0 / 20.0), 1e-12);
    EXPECT_NEAR(std::arg(s3.value().at(0, 3, 3)) * 180 / std::numbers::pi, 45.0, 1e-9);
    EXPECT_FALSE(em::parseTouchstone(three).ok());  // three ports need the count
    // Written and read back.
    auto back = em::parseTouchstone(em::writeTouchstone(s3.value()), 3);
    ASSERT_TRUE(back.ok()) << back.message();
    for (int i = 1; i <= 3; ++i)
        for (int j = 1; j <= 3; ++j)
            EXPECT_NEAR(std::abs(back.value().at(0, i, j) - s3.value().at(0, i, j)), 0.0, 1e-11);
    auto backV2 = em::parseTouchstone(em::writeTouchstone(v2.value()), 2);
    ASSERT_TRUE(backV2.ok()) << backV2.message();
    EXPECT_EQ(backV2.value().referenceImpedance, v2.value().referenceImpedance);
    EXPECT_NEAR(backV2.value().at(0, 2, 1).real(), 0.9, 1e-12);
    // Refused: other parameters, incomplete points, mixed-mode data.
    EXPECT_FALSE(em::parseTouchstone("# HZ Z RI R 50\n1 1 0\n", 1).ok());
    EXPECT_FALSE(em::parseTouchstone("# HZ S RI R 50\n1 0.1 0 0.2\n", 2).ok());
    EXPECT_FALSE(em::parseTouchstone("[Version] 2.0\n[Mixed-Mode Order] D2,1\n", 0).ok());
    // As an imported (raw) measurement.
    auto item = em::importTouchstoneMeasurement(three, "divider.s3p", "exp");
    ASSERT_TRUE(item.ok());
    EXPECT_EQ(item.value().record.category, DataCategory::Raw);
    EXPECT_NE(item.value().record.note.find("not acquired by Resonant"), std::string::npos);
    auto network = em::networkOf(item.value());
    ASSERT_TRUE(network.ok());
    EXPECT_EQ(network.value().ports, 3);
}

TEST(ExperimentDomains, AntennaImpedanceMeasurementOnTheSimulatedVna)
{
    Bench bench;
    const Experiment e = em::antennaImpedanceExperiment("vna", 1e9);
    ASSERT_TRUE(validateExperiment(e).ok());
    // Without authorization the VNA does not sweep.
    ExperimentRunner runner(bench.registry, bench.calibrations, nullptr);
    EXPECT_EQ(runner.run(e, {}).state, ExperimentState::Failed);
    RunResult r = bench.run(e);
    ASSERT_EQ(r.state, ExperimentState::Completed) << r.record.message;
    ASSERT_EQ(r.raw.size(), 1u);
    auto n = em::networkOf(r.raw.front());
    ASSERT_TRUE(n.ok()) << n.message();
    std::vector<em::Complex> s11;
    for (size_t f = 0; f < n.value().frequencyHz.size(); ++f)
        s11.push_back(n.value().at(f, 1, 1));
    const auto m = em::reflectionMetrics(n.value().frequencyHz, s11, 50.0);
    // The simulated DUT: series RLC, 73 ohm at 1 GHz: |Gamma| = 23/123, return loss 14.6 dB.
    EXPECT_NEAR(m.bestFrequencyHz, 1e9, 3e6);
    EXPECT_NEAR(m.bestReturnLossDb, -20 * std::log10(23.0 / 123.0), 0.2);
    EXPECT_NEAR(m.vswr[static_cast<size_t>(std::find(m.returnLossDb.begin(), m.returnLossDb.end(), m.bestReturnLossDb) -
                                         m.returnLossDb.begin())],
                73.0 / 50.0, 0.02);
    EXPECT_GT(m.bandwidth10dBHz, 0.0);
    // Compared with the impedance the same RLC model predicts.
    std::vector<std::pair<double, em::AntennaImpedance>> sim;
    const double L = 10.0 * 73.0 / (2 * std::numbers::pi * 1e9), C = 1.0 / std::pow(2 * std::numbers::pi * 1e9, 2) / L;
    for (double f = 0.8e9; f <= 1.2e9; f += 1e6) {
        const double w = 2 * std::numbers::pi * f;
        em::AntennaImpedance z;
        z.input = z.terminal = em::Complex(73.0, w * L - 1.0 / (w * C));
        sim.emplace_back(f, z);
    }
    auto c = em::compareReflection(n.value(), sim, "series RLC");
    ASSERT_TRUE(c.ok()) << c.message();
    EXPECT_LT(c.value().rmsMagnitudeErrorDb, 0.1);
}

TEST(ExperimentDomains, ArrayChannelCalibrationRecoversTheReceiverErrors)
{
    Bench bench;
    const Experiment e = em::arrayCalibrationExperiment("array", 2.4e9);
    ASSERT_TRUE(validateExperiment(e).ok());
    RunResult r = bench.run(e);
    ASSERT_EQ(r.state, ExperimentState::Completed) << r.record.message;
    std::vector<em::Complex> phasors;
    std::vector<std::string> channels;
    for (int i = 1; i <= 4; ++i) {
        const ProcessingItem* t = derivedFor(r, "ch-rx-" + std::to_string(i), MeasurementKind::Spectrum);
        ASSERT_NE(t, nullptr);
        phasors.push_back(t->data.complex.front());
        channels.push_back("rx" + std::to_string(i - 1));
    }
    auto cal = em::arrayChannelCalibration(phasors, channels, "sim:rf-rx-4", 0, {}, 2.4e9);
    ASSERT_TRUE(cal.ok()) << cal.message();
    // The simulated receiver's errors: 0, 37, -52, 101 degrees; 0, -0.4, 0.3, -0.8 dB.
    const double phases[4] = {0.0, 37.0, -52.0, 101.0}, gains[4] = {0.0, -0.4, 0.3, -0.8};
    for (int i = 0; i < 4; ++i) {
        EXPECT_NEAR(cal.value()[static_cast<size_t>(i)].phaseRad * 180 / std::numbers::pi, phases[i], 0.5) << i;
        EXPECT_NEAR(20 * std::log10(cal.value()[static_cast<size_t>(i)].scalar), gains[i], 0.05) << i;
    }
    // Applied to an array's excitation: pre-compensation.
    em::EmArraySettings array;
    array.countX = 4;
    std::vector<const CalibrationRecord*> per;
    for (const auto& c : cal.value())
        per.push_back(&c);
    auto corrected = em::applyArrayCalibration(array, per);
    ASSERT_TRUE(corrected.ok()) << corrected.message();
    EXPECT_NEAR(*corrected.value().overrides.at(1).phase, -37.0 * std::numbers::pi / 180, 0.01);
    EXPECT_FALSE(em::applyArrayCalibration(array, {per[0]}).ok());
}

TEST(ExperimentDomains, RadiationPatternFromAnAngularScan)
{
    Bench bench;
    const Experiment e = em::radiationPatternExperiment("pattern", 2.4e9);
    ASSERT_TRUE(validateExperiment(e).ok()) << validateExperiment(e).errors.front();
    RunResult r = bench.run(e);
    ASSERT_EQ(r.state, ExperimentState::Completed) << r.record.message;
    ASSERT_EQ(r.spatial.size(), 1u);
    EXPECT_EQ(r.spatial.front().samples.size(), 72u);
    auto p = em::measuredPattern(r.spatial.front());
    ASSERT_TRUE(p.ok()) << p.message();
    // Within one scan step of boresight: at 5 deg the pattern is only 0.05 dB down, about the noise of the estimate.
    EXPECT_LE(std::abs(p.value().peakAngleDeg > 180 ? p.value().peakAngleDeg - 360 : p.value().peakAngleDeg), 5.0);
    // The simulated antenna: cos^1.5 in azimuth, half power at acos(0.5^(1/3)) = 37.47 deg.
    EXPECT_NEAR(p.value().halfPowerBeamwidthDeg, 2 * 37.47, 2.0);
    // Absolute levels need a calibrated chain.
    em::RfMeasurementChain chain;
    EXPECT_FALSE(em::receivedPowerDbm(-20.0, chain).ok());
    chain.receiverDbmAtFullScale = -10.0;
    chain.cableLossDb = 1.5;
    EXPECT_NEAR(em::receivedPowerDbm(-20.0, chain).value(), -28.5, 1e-12);
    EXPECT_FALSE(em::fieldStrengthDbuvPerM(-20.0, chain).ok());
    chain.antennaFactorDbPerM = 30.0;
    EXPECT_TRUE(em::fieldStrengthDbuvPerM(-20.0, chain).ok());
}
