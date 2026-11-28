// Lumped oscillator networks and coupled resonators (RES-RSN-004, -005, -009):
// natural modes against two-oscillator closed forms, splitting, avoided
// crossings with detuning, energy exchange, the coupled-mode approximation,
// driven response, buildup and decay, losses by mechanism and an energy
// ledger without double counting.
#include "dsp/Resonance.h"
#include "oscillators/LumpedNetwork.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace resonant;
using namespace resonant::oscillators;

namespace {
constexpr double kPi = 3.14159265358979323846;

Network pair(double m1, double k1, double m2, double k2, double kc, double c1 = 0.0, double c2 = 0.0)
{
    Network n;
    n.names = {"resonator 1", "resonator 2"};
    n.masses = {m1, m2};
    n.springs = {{"own 1", 0, -1, k1}, {"own 2", 1, -1, k2}, {"coupling", 0, 1, kc}};
    if (c1 > 0.0)
        n.dampers.push_back({"damper 1", 0, -1, c1});
    if (c2 > 0.0)
        n.dampers.push_back({"damper 2", 1, -1, c2});
    return n;
}
}  // namespace

TEST(LumpedNetwork, IdenticalResonatorsSplitIntoSymmetricAndAntisymmetricModes)
{
    const double m = 0.2, k = 2000.0, kc = 100.0;
    auto modes = naturalModes(pair(m, k, m, k, kc));
    ASSERT_TRUE(modes) << modes.message();
    ASSERT_EQ(modes.value().size(), 2u);
    EXPECT_NEAR(modes.value()[0].frequencyHz, std::sqrt(k / m) / (2 * kPi), 1e-9);
    EXPECT_NEAR(modes.value()[1].frequencyHz, std::sqrt((k + 2 * kc) / m) / (2 * kPi), 1e-9);
    EXPECT_EQ(modes.value()[0].symmetry, "symmetric");
    EXPECT_EQ(modes.value()[1].symmetry, "antisymmetric");
    EXPECT_NEAR(modes.value()[0].participation[0], 0.5, 1e-9);
    EXPECT_TRUE(std::isinf(modes.value()[0].quality));
    // The coupled-mode approximation agrees to first order in kc / k (and says what it is).
    const auto cmt = coupledModeEstimate(m, k, m, k, kc);
    EXPECT_NEAR(cmt.splittingHz / (modes.value()[1].frequencyHz - modes.value()[0].frequencyHz), 1.0, 0.03);
    EXPECT_NEAR(cmt.maximumTransferFraction, 1.0, 1e-12);
    EXPECT_NE(cmt.model.find("approximation"), std::string::npos);
}

TEST(LumpedNetwork, DetuningGivesAnAvoidedCrossingAndLocalizedModes)
{
    const double m = 0.2, k = 2000.0, kc = 50.0;
    double smallest = 1e300;
    for (double k2 = 1600.0; k2 <= 2400.0; k2 += 10.0) {
        const auto modes = naturalModes(pair(m, k, m, k2, kc)).value();
        const double split = modes[1].frequencyHz - modes[0].frequencyHz;
        // Closed form of the 2 x 2 problem.
        const double a = (k + kc) / m, b = (k2 + kc) / m, c = kc / m;
        const double lo = std::sqrt(0.5 * (a + b) - std::sqrt(0.25 * (a - b) * (a - b) + c * c));
        const double hi = std::sqrt(0.5 * (a + b) + std::sqrt(0.25 * (a - b) * (a - b) + c * c));
        EXPECT_NEAR(modes[0].frequencyHz, lo / (2 * kPi), 1e-9);
        EXPECT_NEAR(modes[1].frequencyHz, hi / (2 * kPi), 1e-9);
        smallest = std::min(smallest, split);
        if (k2 == 1600.0) {
            EXPECT_GT(modes[0].participation[1], 0.9);  // far detuned: each mode lives in one resonator
            EXPECT_EQ(modes[0].symmetry, "symmetric");
        }
    }
    // The gap never closes: its minimum at zero detuning is the coupling splitting.
    EXPECT_NEAR(smallest, (std::sqrt((k + 2 * kc) / m) - std::sqrt(k / m)) / (2 * kPi), 1e-9);
}

TEST(LumpedNetwork, EnergyExchangesFullyBetweenIdenticalResonatorsAndPartlyWhenDetuned)
{
    const double m = 0.2, k = 2000.0, kc = 40.0;
    const auto modes = naturalModes(pair(m, k, m, k, kc)).value();
    const double beat = 1.0 / (modes[1].frequencyHz - modes[0].frequencyHz);
    auto e = energyExchange(pair(m, k, m, k, kc), 0, 1, beat);
    ASSERT_TRUE(e) << e.message();
    EXPECT_GT(e.value().maximumTransferFraction, 0.95);
    EXPECT_NEAR(e.value().transferTime / (0.5 * beat), 1.0, 0.02);  // half a beat period
    EXPECT_LT(e.value().remainingFraction, 0.05);
    // Detuned by more than the coupling: partial transfer, as coupled-mode theory predicts.
    const double k2 = 2200.0;
    const auto cmt = coupledModeEstimate(m, k, m, k2, kc);
    auto d = energyExchange(pair(m, k, m, k2, kc), 0, 1, 2.0 * beat);
    ASSERT_TRUE(d);
    EXPECT_NEAR(d.value().maximumTransferFraction, cmt.maximumTransferFraction, 0.05);
    EXPECT_LT(d.value().maximumTransferFraction, 0.6);
}

TEST(LumpedNetwork, DrivenPeaksDifferFromNaturalFrequenciesAndPowerBalances)
{
    // A heavily damped resonator: the displacement peak lies below the natural frequency.
    Network n;
    n.names = {"r"};
    n.masses = {1.0};
    n.springs = {{"k", 0, -1, 1e4}};
    n.dampers = {{"viscous", 0, -1, 60.0}, {"radiation", 0, -1, 20.0}};
    const auto modes = naturalModes(n).value();
    std::vector<double> f;
    for (int i = 1; i <= 4000; ++i)
        f.push_back(0.01 * i);
    auto r = drivenResponse(n, {{1.0, 0.0}}, f);
    ASSERT_TRUE(r);
    size_t peak = 0;
    for (size_t i = 0; i < r.value().size(); ++i)
        if (std::abs(r.value()[i].displacement[0]) > std::abs(r.value()[peak].displacement[0]))
            peak = i;
    const double zeta = 80.0 / (2.0 * std::sqrt(1e4));
    EXPECT_NEAR(r.value()[peak].frequencyHz, modes[0].undampedHz * std::sqrt(1 - 2 * zeta * zeta), 0.01);
    EXPECT_LT(r.value()[peak].frequencyHz, modes[0].frequencyHz);
    // Supplied = dissipated (steady state), split by mechanism in proportion to the coefficients.
    for (const auto& p : r.value()) {
        double lost = 0.0;
        for (const auto& [name, w] : p.dissipated)
            lost += w;
        EXPECT_NEAR(p.suppliedPower, lost, 1e-12 * (1.0 + lost));
        EXPECT_NEAR(p.dissipated.at("viscous") / p.dissipated.at("radiation"), 3.0, 1e-9);
    }
    // Q per mechanism combines as 1/Q = sum 1/Q_i.
    const auto& q = modes[0].qualityByMechanism;
    EXPECT_NEAR(1.0 / q.at("viscous") + 1.0 / q.at("radiation"), 1.0 / modes[0].quality, 1e-9);
}

TEST(LumpedNetwork, BuildupAndDecayFollowTheirTimeConstantsAndTheLedgerBalances)
{
    const double m = 0.5, k = 5e4, c = 0.5;  // f0 = 50.3 Hz, Q = 316
    Network n;
    n.names = {"r"};
    n.masses = {m};
    n.springs = {{"k", 0, -1, k}};
    n.dampers = {{"structural", 0, -1, c}};
    const auto mode = naturalModes(n).value()[0];
    const double w = 2 * kPi * mode.undampedHz, tau = 2.0 * m / c;  // amplitude time constant 2Q/w
    // Buildup under a resonant drive: the amplitude envelope approaches 1 - e^{-t/tau}.
    TransientOptions o;
    o.duration = 3.0 * tau;
    o.samples = 6000;
    o.force = [&](double t) { return std::vector<double>{std::sin(mode.frequencyHz * 2 * kPi * t)}; };
    auto build = integrate(n, o);
    ASSERT_TRUE(build) << build.message();
    const double steady = 1.0 / (c * mode.frequencyHz * 2 * kPi);
    const auto& x = build.value().displacement[0];
    double atTau = 0.0;
    for (size_t i = 0; i < x.size(); ++i)
        if (std::abs(build.value().time[i] - tau) < 0.05 * tau)
            atTau = std::max(atTau, std::abs(x[i]));
    EXPECT_NEAR(atTau / steady, 1.0 - std::exp(-1.0), 0.03);
    EXPECT_LT(build.value().residual, 1e-6);  // supplied = stored + dissipated, to integration accuracy
    // Free decay: the energy falls as e^{-w t / Q}; the pencil recovers Q from the record.
    TransientOptions d;
    d.duration = 2.0 * tau;
    d.x0 = {1e-3};
    d.samples = 20000;
    auto ring = integrate(n, d);
    ASSERT_TRUE(ring);
    const auto& e = ring.value().energy[0];
    const double total0 = e.front();
    const size_t half = e.size() / 2;
    double mean = 0.0;
    for (size_t i = half - 200; i < half + 200; ++i)
        mean += e[i] / 400.0;
    EXPECT_NEAR(mean / total0, std::exp(-w * ring.value().time[half] / mode.quality), 0.02);
    dsp::PencilOptions po;
    po.minHz = 10;
    po.maxHz = 200;
    auto id = dsp::identifyByMatrixPencil(ring.value().displacement[0], ring.value().time[1] - ring.value().time[0], po);
    ASSERT_TRUE(id) << id.message();
    ASSERT_FALSE(id.value().resonances.empty());
    EXPECT_NEAR(id.value().resonances[0].quality / mode.quality, 1.0, 1e-3);
    EXPECT_NEAR(ring.value().dissipatedByMechanism.at("structural") / total0, 1.0 - (e.back() / total0), 1e-4);
}

TEST(LumpedNetwork, CouplingEnergyIsCountedOnceInTheLedger)
{
    Network n = pair(0.2, 2000.0, 0.3, 2500.0, 150.0, 0.4, 0.2);
    TransientOptions o;
    o.duration = 1.0;
    o.samples = 2000;
    // A smooth (half-sine) pulse: a force with jumps is sampled inconsistently by the Runge-Kutta stages.
    o.force = [](double t) { return std::vector<double>{t < 0.01 ? 5.0 * std::sin(3.14159265358979 * t / 0.01) : 0.0, 0.0}; };
    auto r = integrate(n, o);
    ASSERT_TRUE(r) << r.message();
    // Stored = resonator energies + the coupling spring's; supplied = stored + dissipated at every sample.
    for (size_t s = 0; s < r.value().time.size(); s += 97) {
        const double stored = r.value().energy[0][s] + r.value().energy[1][s] + r.value().couplingEnergy[s];
        EXPECT_NEAR(r.value().supplied[s], stored + r.value().dissipated[s], 1e-6 * r.value().supplied.back()) << s;  // integration accuracy
    }
    EXPECT_LT(r.value().residual, 1e-6);
    EXPECT_EQ(r.value().dissipatedByMechanism.size(), 2u);
}

TEST(LumpedNetwork, InvalidNetworksAreRefused)
{
    Network n = pair(0.2, 2000.0, 0.2, 2000.0, 100.0);
    n.masses[1] = -1.0;
    EXPECT_FALSE(naturalModes(n));
    n = pair(0.2, 2000.0, 0.2, 2000.0, 100.0);
    n.dampers.push_back({"active", 0, -1, -1.0});
    EXPECT_FALSE(naturalModes(n));  // negative damping is not passive
    n = pair(0.2, 2000.0, 0.2, 2000.0, 100.0);
    n.springs.push_back({"bad", 0, 0, 10.0});
    EXPECT_FALSE(naturalModes(n));
    EXPECT_FALSE(energyExchange(pair(0.2, 2000.0, 0.2, 2000.0, 100.0), 0, 0, 1.0));
}
