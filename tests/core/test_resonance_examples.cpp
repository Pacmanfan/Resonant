// The resonance reference examples (RES-RSN-001, -005, -006, -007, -008,
// -011, -012, -014): each example's analysis against closed forms and
// independent references; natural modes and driven peaks kept apart;
// geometry-driven tuning, avoided crossings, the Q-versus-extraction tradeoff
// and the cavity's ring-down.
#include "coupling/ResonanceAnalyses.h"
#include "dsp/Resonance.h"
#include "electromagnetics/CavityResonator.h"
#include "electromagnetics/Conventions.h"
#include "functional/FunctionalStudy.h"
#include "manipulators/Helmholtz.h"
#include "mechanics/LumpedResonators.h"
#include "project/Project.h"
#include "project/ProjectSerializer.h"

#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>

using namespace resonant;
using namespace resonant::functional;
using nlohmann::json;

namespace {

constexpr double kPi = 3.14159265358979323846;

struct Example {
    std::unique_ptr<Project> project;
    FunctionalStructure structure;
};

Example load(const std::string& folder, const std::string& file)
{
    auto loaded = loadProjectFile(std::filesystem::path(RESONANT_SOURCE_DIR) / "examples" / folder / file);
    EXPECT_TRUE(loaded) << loaded.message();
    Example e{std::move(loaded.value().project), {}};
    e.structure = structuresOf(*e.project).value().at(0);
    return e;
}

json analyze(const Example& e, const std::string& model = "")
{
    auto d = analyzeDocument(e.structure, *e.project, AnalysisRegistry::instance(), model, nullptr);
    EXPECT_TRUE(d) << d.message();
    return d ? d.value() : json::object();
}

double metric(const json& d, const std::string& key)
{
    for (const json& m : d["metrics"])
        if (m["key"] == key)
            return m["value"].is_number() ? m["value"].get<double>() : std::numeric_limits<double>::infinity();
    ADD_FAILURE() << "no metric " << key;
    return 0.0;
}

}  // namespace

TEST(ResonanceExamples, HelmholtzResonatorMatchesItsClosedFormAndTheDuctModel)
{
    const Example e = load("resonance", "a-helmholtz-resonator.resonant");
    const json d = analyze(e);
    const double S = kPi * 0.005 * 0.005, V = kPi * 0.025 * 0.025 * 0.05;
    const double Leff = 0.01 + 8.0 / (3 * kPi) * 0.005 * (1.0 + (1.0 - 1.25 * 0.2));
    EXPECT_NEAR(metric(d, "helmholtz.f0"), 343.0 / (2 * kPi) * std::sqrt(S / (V * Leff)), 1e-9);
    EXPECT_NEAR(1.0 / metric(d, "helmholtz.Q"), 1.0 / metric(d, "helmholtz.qRadiation") + 1.0 / metric(d, "helmholtz.qViscous"), 1e-12);
    // The natural mode carries its loss breakdown; the pressure-gain peak is a driven peak, reported apart.
    ASSERT_GE(d["modes"].size(), 2u);
    EXPECT_EQ(d["modes"][0]["nature"], "natural");
    EXPECT_TRUE(d["modes"][0]["lossQ"].contains("radiation"));
    EXPECT_TRUE(d["modes"][0]["lossQ"].contains("viscous (neck)"));
    EXPECT_EQ(d["modes"][1]["nature"], "driven-peak");
    EXPECT_NE(d["modes"][1]["frequencyHz"].get<double>(), d["modes"][0]["frequencyHz"].get<double>());
    // The plane-wave stepped duct, a separate model: within a few percent here (k h ~ 0.4).
    const json duct = analyze(e, manipulators::kHelmholtzDuctModelId);
    EXPECT_LT(std::abs(metric(duct, "helmholtz.lumpedDeviation")), 0.08);
    // Tuning by geometry: f0 ~ V^(-1/2): d ln f / d ln (cavity length) = -1/2.
    auto s = sensitivityDocument(e.structure, *e.project, AnalysisRegistry::instance(), {"cavityLength"}, 1e-4, "", nullptr);
    ASSERT_TRUE(s) << s.message();
    for (const json& entry : s.value()["entries"])
        if (entry["metric"] == "helmholtz.f0") {
            EXPECT_NEAR(entry["normalized"].get<double>(), -0.5, 1e-6);
        }
}

TEST(ResonanceExamples, CantileverResonatorMatchesBeamTheoryAndItsRingDown)
{
    Example e = load("resonance", "b-coupled-cantilevers.resonant");
    // The first cantilever as a resonator on its own (an analysis of the same body).
    FunctionalStructure one = e.structure;
    one.id = "fs-cantilever";
    one.bodies = {{"ex-rs-beam1", "resonator"}};
    one.parameters = {};
    one.objectives = {};
    one.analysis = {std::string(coupling::kCantileverResonatorAnalysis), "", {{"lossFactor", 2e-3}}};
    auto d = analyzeDocument(one, *e.project, AnalysisRegistry::instance(), "", nullptr);
    ASSERT_TRUE(d) << d.message();
    // Aluminium (example library): E = 69 GPa, rho = 2700 kg/m^3.
    const double E = 69e9, rho = 2700.0, L = 0.2, w = 0.02, t = 0.002;
    const double f1 = 1.87510406871196 * 1.87510406871196 / (2 * kPi) * std::sqrt(E * w * t * t * t / 12.0 / (rho * w * t * std::pow(L, 4)));
    EXPECT_NEAR(metric(d.value(), "cantilever.f1") / f1, 1.0, 1e-9);
    EXPECT_NEAR(metric(d.value(), "cantilever.f2") / metric(d.value(), "cantilever.f1"), std::pow(4.69409113297417 / 1.87510406871196, 2), 1e-9);
    EXPECT_NEAR(metric(d.value(), "cantilever.tipMass") / metric(d.value(), "cantilever.beamMass"), 0.25, 1e-6);
    EXPECT_NEAR(metric(d.value(), "cantilever.Q"), 500.0, 1e-9);
    // Ring-down of the simulated free decay, identified by complex poles: Q = 1 / eta.
    EXPECT_NEAR(metric(d.value(), "cantilever.ringDownQ") / 500.0, 1.0, 5e-3);
    EXPECT_LT(metric(d.value(), "cantilever.ledgerResidual"), 1e-5);
}

TEST(ResonanceExamples, CoupledCantileversSplitExchangeEnergyAndAvoidCrossingWhenDetuned)
{
    const Example e = load("resonance", "b-coupled-cantilevers.resonant");
    const json d = analyze(e);
    // Identical beams: in-phase at the beam's own frequency, anti-phase stiffened by 2 kc.
    mechanics::CantileverSpec beam{0.2, 0.02, 0.002, 69e9, 2700.0, 1e-4};
    const auto m = mechanics::cantileverMode(beam).value();
    const double kc = 10.0;
    EXPECT_NEAR(metric(d, "coupled.fLower"), m.frequencyHz, 1e-3);
    EXPECT_NEAR(metric(d, "coupled.fUpper"), std::sqrt((m.tipStiffness + 2 * kc) / m.tipMass) / (2 * kPi), 1e-3);
    EXPECT_EQ(d["modes"][0]["label"], "in-phase");
    EXPECT_EQ(d["modes"][1]["label"], "anti-phase");
    EXPECT_NEAR(metric(d, "coupled.cmtSplitting") / metric(d, "coupled.splitting"), 1.0, 0.03);  // coupled-mode theory, an approximation
    EXPECT_GT(metric(d, "coupled.transferFraction"), 0.9);
    EXPECT_NEAR(metric(d, "coupled.transferTime") / (0.5 * metric(d, "coupled.beatPeriod")), 1.0, 0.05);
    EXPECT_LT(metric(d, "coupled.ledgerResidual"), 1e-5);
    // Detuning by the second beam's length: the modes repel (an avoided crossing) and the transfer drops.
    std::vector<double> lengths;
    for (int i = 0; i <= 10; ++i)
        lengths.push_back(0.19 + 0.002 * i);
    auto sweep = sweepDocument(e.structure, *e.project, AnalysisRegistry::instance(), {{"length2", lengths}}, "", nullptr);
    ASSERT_TRUE(sweep) << sweep.message();
    const json& avoided = sweep.value()["tracks"]["avoidedCrossings"];
    ASSERT_EQ(avoided.size(), 1u) << sweep.value()["tracks"].dump();
    EXPECT_NEAR(avoided[0]["minimumGapHz"].get<double>(), metric(d, "coupled.splitting"), 0.02 * metric(d, "coupled.splitting"));
    EXPECT_EQ(avoided[0]["at"], 5);  // equal lengths
    const json& points = sweep.value()["points"];
    EXPECT_LT(points[0]["metrics"]["coupled.transferFraction"].get<double>(), 0.5);
    EXPECT_GT(points[5]["metrics"]["coupled.transferFraction"].get<double>(), 0.9);
}

TEST(ResonanceExamples, CoupledHelmholtzResonatorsMatchTheTwoByTwoClosedForm)
{
    const Example e = load("resonance", "c-coupled-helmholtz.resonant");
    const json d = analyze(e);
    manipulators::HelmholtzSpec open, closed;
    open.cavityRadius = closed.cavityRadius = 0.025;
    open.cavityLength = 0.05;
    closed.cavityLength = 0.06;
    open.neckRadius = closed.neckRadius = 0.005;
    open.neckLength = closed.neckLength = 0.01;
    const double rho = 1.204, c = 343.0, ac = 0.003, lc = 0.01;
    const double M1 = manipulators::analyzeHelmholtz(open).value().inertance;
    const double corr = 8.0 / (3 * kPi) * ac * (1 - 1.25 * ac / 0.025);
    const double Mc = rho * (lc + 2 * corr) / (kPi * ac * ac);
    const double C1 = open.volume() / (rho * c * c), C2 = closed.volume() / (rho * c * c);
    // M1 Mc w^4 - (M1 (1/C1 + 1/C2) + Mc / C1) w^2 + 1 / (C1 C2) = 0.
    const double A = M1 * Mc, B = -(M1 * (1 / C1 + 1 / C2) + Mc / C1), Cc = 1 / (C1 * C2);
    const double wLo = std::sqrt((-B - std::sqrt(B * B - 4 * A * Cc)) / (2 * A)), wHi = std::sqrt((-B + std::sqrt(B * B - 4 * A * Cc)) / (2 * A));
    // |s| of a damped mode equals the undamped frequency to O(1/Q^2) when the damping is not proportional.
    EXPECT_NEAR(d["modes"][0]["details"]["undampedHz"].get<double>(), wLo / (2 * kPi), 1e-4 * wLo / (2 * kPi));
    EXPECT_NEAR(d["modes"][1]["details"]["undampedHz"].get<double>(), wHi / (2 * kPi), 1e-4 * wHi / (2 * kPi));
    EXPECT_GT(metric(d, "coupled.splitting"), 0.0);
    EXPECT_TRUE(d["modes"][0]["lossQ"].contains("radiation"));
    EXPECT_EQ(d["curves"].size(), 2u);  // the two cavity pressures
}

TEST(ResonanceExamples, TheHeatedCavityShiftsItsResonanceThroughTheEngineAndTunesWithItsGeometry)
{
    const Example e = load("resonance", "d-heated-cavity.resonant");
    const json d = analyze(e);
    EXPECT_EQ(metric(d, "coupled.converged"), 1.0);
    EXPECT_EQ(metric(d, "coupled.balanced"), 1.0);
    EXPECT_GT(metric(d, "coupled.resonanceShift"), 0.0);  // the fill's permittivity falls faster than the walls grow
    EXPECT_LT(metric(d, "coupled.deliveredPower"), 1.0);   // detuned from the drive by its own heating
    EXPECT_GT(metric(d, "coupled.peakTemperature"), 293.15);
    EXPECT_TRUE(d["feasible"].get<bool>());
    // A longer cavity resonates below the fixed drive: less power, less heating (geometry-driven tuning).
    auto longer = withParameters(e.structure, *e.project, {{"d", 0.0302}});
    ASSERT_TRUE(longer) << longer.message();
    const json t = analyzeDocument(longer.value().second, *longer.value().first, AnalysisRegistry::instance(), "", nullptr).value();
    EXPECT_EQ(t["details"]["linkedRegions"][0]["dimensions"]["d"], 0.0302);
    EXPECT_NE(metric(t, "coupled.deliveredPower"), metric(d, "coupled.deliveredPower"));
}

TEST(ResonanceExamples, QAgainstExtractionIsATradeoffNotAnOptimum)
{
    Example e = load("functional", "a-microwave-cavity.resonant");
    FunctionalStructure s = e.structure;
    s.parameters.push_back({});
    Parameter& beta = s.parameters.back();
    beta.id = "beta";
    beta.name = "Port coupling";
    beta.kind = ParameterKind::Free;
    beta.value = 1.0;
    beta.minimum = 0.1;
    beta.maximum = 10.0;
    for (Boundary& b : s.boundaries)
        if (b.role == "port")
            b.settings = {{"coupling", "beta"}};
    auto sweep = sweepDocument(s, *e.project, AnalysisRegistry::instance(), {{"beta", {0.25, 0.5, 1.0, 2.0, 4.0}}}, "", nullptr);
    ASSERT_TRUE(sweep) << sweep.message();
    double previousQ = 1e300, previousExtraction = -1.0;
    for (const json& p : sweep.value()["points"]) {
        const double q = p["metrics"]["cavity.loadedQ"].get<double>(), x = p["metrics"]["cavity.extractionEfficiency"].get<double>();
        EXPECT_LT(q, previousQ);           // stronger coupling lowers the loaded Q ...
        EXPECT_GT(x, previousExtraction);  // ... while a larger share of the energy leaves through the port
        previousQ = q;
        previousExtraction = x;
    }
    EXPECT_NEAR(sweep.value()["points"][2]["metrics"]["cavity.extractionEfficiency"].get<double>(), 0.5, 1e-12);
}

TEST(ResonanceExamples, CavityRingDownIdentifiesTheModesAndTheFillQ)
{
    // A lossy fill (Q = 100) in a box the grid represents exactly; poles of the Yee probe record.
    em::CavityResonatorSpec s;
    s.a = 0.030;
    s.b = 0.018;
    s.d = 0.042;
    s.fillPermittivity = 2.0;
    const double f1 = em::analyzeRectangularCavity(s, 1).value()[0].frequencyHz;
    s.fillConductivity = 2.0 * kPi * f1 * em::kEpsilon0 * 2.0 / 100.0;
    em::CavityYeeSettings ys;
    ys.cellsAcrossSmallest = 12;
    ys.durationPeriods = 60;
    auto run = em::simulateRectangularCavity(s, ys);
    ASSERT_TRUE(run) << run.message();
    dsp::PencilOptions po;
    po.minHz = 0.5 * f1;
    po.maxHz = 1.5 * f1;
    po.skipSamples = run.value().pulseSteps;
    po.maxModes = 40;  // the pulse excites some twenty modes; each needs its pair of poles
    auto id = dsp::identifyByMatrixPencil(run.value().probeSignal, run.value().dt, po);
    ASSERT_TRUE(id) << id.message();
    const dsp::IdentifiedResonance* lowest = nullptr;
    for (const auto& r : id.value().resonances)
        if (!lowest || std::abs(r.frequencyHz - f1) < std::abs(lowest->frequencyHz - f1))
            lowest = &r;
    ASSERT_NE(lowest, nullptr);
    EXPECT_NEAR(lowest->frequencyHz / f1, 1.0, 0.005);   // Yee dispersion at 12 cells
    EXPECT_NEAR(lowest->quality / 100.0, 1.0, 0.03);     // Q of a conducting fill: w eps / sigma
}
