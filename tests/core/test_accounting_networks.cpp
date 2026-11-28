// Accounting of lumped networks (RES-ACC-004, -007, -008, -011, -012, -014): a closed
// two-body collision conserving momentum while kinetic and elastic energy exchange; a damped
// driven oscillator whose work is stored or dissipated; coupled resonators accounted whole
// and as subsystems; the residual's order in the time step; Q = w E / P and the buildup and
// decay of a resonator; forces with their origins and pairs; a rotating shaft's angular
// momentum and torques.
#include "accounting/Conservation.h"
#include "coupling/AccountingAdapters.h"
#include "oscillators/LumpedNetwork.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace resonant;
using namespace resonant::accounting;
using resonant::oscillators::Network;
using resonant::oscillators::TransientOptions;

namespace {

constexpr double kPi = 3.14159265358979323846;

Check check(const Report& r, const std::string& model)
{
    for (const Check& c : r.checks)
        if (c.model == model)
            return c;
    ADD_FAILURE() << "no check " << model;
    return {};
}

Ledger run(const Network& net, TransientOptions o, coupling::NetworkAccounting a = {})
{
    o.elementFlows = true;
    auto rec = oscillators::integrate(net, o);
    EXPECT_TRUE(rec) << rec.message();
    auto l = coupling::networkLedger(net, rec.value(), a);
    EXPECT_TRUE(l) << l.message();
    return l.take();
}

Network gliders()
{
    Network n;
    n.names = {"a", "b"};
    n.masses = {1.0, 2.0};
    n.springs = {{"bumper", 0, 1, 100.0}};
    return n;
}

Network coupled()
{
    Network n;
    n.names = {"r1", "r2"};
    n.masses = {1.0, 1.0};
    n.springs = {{"k1", 0, -1, 100.0}, {"k2", 1, -1, 100.0}, {"coupling", 0, 1, 5.0}};
    return n;
}

const Tolerances tight{1e-6, 1e-4, 1e-15};  // Runge-Kutta transients: about 1e-8 of the energy

}  // namespace

TEST(AccountingNetworks, ClosedSystemConservesMomentumWhileEnergyChangesForm)
{
    TransientOptions o;
    o.duration = 2.0;
    o.v0 = {1.0, 0.0};
    const Ledger l = run(gliders(), o);
    for (const Entity& e : l.entities)
        EXPECT_FALSE(e.external()) << e.id;  // nothing outside: a closed system
    const Report r = evaluate(l, wholeSystem(l), tight);
    const Check p = check(r, "momentum.linear");
    EXPECT_EQ(p.status, CheckStatus::Verified);
    EXPECT_NEAR(p.initial[0], 1.0, 1e-15);
    EXPECT_NEAR(p.final[0], 1.0, 1e-12);
    EXPECT_LT(p.residualNorm(), 1e-12);
    const Check e = check(r, "energy.total");
    EXPECT_EQ(e.status, CheckStatus::Verified) << e.attribution;
    EXPECT_NE(e.attribution.find("within the stated numerical errors"), std::string::npos) << e.attribution;
    // Kinetic energy became elastic energy in the bumper and back.
    double peakElastic = 0.0;
    for (const Holding& h : l.holdings)
        if (h.form == "elastic")
            for (double v : h.history)
                peakElastic = std::max(peakElastic, v);
    EXPECT_GT(peakElastic, 0.3 * 0.5);  // a third of the initial kinetic energy at least (1/3 at closest approach)
    EXPECT_EQ(check(r, "momentum.angular").status, CheckStatus::NotApplicable);
}

TEST(AccountingNetworks, ForcesKeepTheirOriginAndPairs)
{
    TransientOptions o;
    o.duration = 0.3;
    o.v0 = {1.0, 0.0};
    const Ledger l = run(gliders(), o);
    ASSERT_EQ(l.forces.size(), 2u);
    EXPECT_EQ(l.forces[0].pair, l.forces[1].id);
    EXPECT_EQ(l.forces[1].pair, l.forces[0].id);
    EXPECT_EQ(l.forces[0].category, "elastic");
    EXPECT_EQ(l.forces[0].source, "element:bumper");
    for (size_t i = 0; i < 3; ++i)
        EXPECT_DOUBLE_EQ(l.forces[0].vector[i], -l.forces[1].vector[i]);
    const Report whole = evaluate(l, wholeSystem(l), {});
    EXPECT_EQ(whole.forces["internal"].size(), 2u);
    EXPECT_DOUBLE_EQ(whole.forces["externalForce"][0].get<double>(), 0.0);
    // Body a alone: the bumper's force is external to it, and its momentum changes by that impulse.
    AccountingSystem a{"a", "Glider a", SystemKind::Object, {"body:a"}};
    const Report ra = evaluate(l, a, tight);
    EXPECT_DOUBLE_EQ(ra.forces["externalForce"][0].get<double>(), l.forces[0].vector[0]);
    const Check pa = check(ra, "momentum.linear");
    EXPECT_EQ(pa.status, CheckStatus::Verified);
    EXPECT_LT(pa.final[0], 1.0);  // slowed by the bumper
    EXPECT_NEAR(pa.impulse[0], pa.final[0] - 1.0, 1e-12);
}

TEST(AccountingNetworks, DampedDrivenWorkIsStoredOrDissipated)
{
    Network n;
    n.names = {"mass"};
    n.masses = {1.0};
    n.springs = {{"spring", 0, -1, 400.0}};
    n.dampers = {{"viscous", 0, -1, 0.4}};
    TransientOptions o;
    o.duration = 10.0;
    o.force = [](double t) { return std::vector<double>{std::cos(20.0 * t)}; };
    o.elementFlows = true;
    auto rec = oscillators::integrate(n, o);
    ASSERT_TRUE(rec);
    auto l = coupling::networkLedger(n, rec.value(), {});
    ASSERT_TRUE(l) << l.message();
    const Report r = evaluate(l.value(), wholeSystem(l.value()), tight);
    const Check e = check(r, "energy.total");
    EXPECT_EQ(e.status, CheckStatus::Verified) << e.attribution;
    EXPECT_GT(e.input[0], 0.0);
    // The heat is the integrator's own dissipation of that mechanism.
    EXPECT_NEAR(e.dissipated[0], rec.value().dissipatedByMechanism.at("viscous"), 1e-9 * e.dissipated[0]);
    EXPECT_NEAR(e.input[0], e.delta[0] + e.dissipated[0], 1e-6 * e.input[0]);
    // Momentum: the drive's impulse in, the spring's and damper's reactions out to the ground.
    const Check p = check(r, "momentum.linear");
    EXPECT_EQ(p.status, CheckStatus::Verified);
    EXPECT_GT(std::abs(p.output[0]), 0.0);
    ASSERT_FALSE(e.residualHistory.empty());
    EXPECT_LT(*std::max_element(e.residualHistory.begin(), e.residualHistory.end()), 1e-6 * e.input[0]);
}

TEST(AccountingNetworks, SubsystemsOfCoupledResonators)
{
    const Network net = coupled();
    TransientOptions o;
    o.duration = 4.0;
    o.x0 = {0.01, 0.0};
    const Ledger l = run(net, o);
    const Report whole = evaluate(l, wholeSystem(l), tight);
    const Check e = check(whole, "energy.total");
    EXPECT_EQ(e.status, CheckStatus::Verified) << e.attribution;
    EXPECT_NEAR(e.input[0], 0.0, 1e-18);
    EXPECT_NEAR(e.output[0], 0.0, 1e-18);   // lossless and isolated: no net loss
    EXPECT_GT(std::abs(e.internal[0]), 0.0); // energy moved inside
    AccountingSystem r1{"r1", "Resonator 1", SystemKind::Subsystem, {"body:r1", "element:k1"}};
    AccountingSystem r2{"r2", "Resonator 2", SystemKind::Subsystem, {"body:r2", "element:k2"}};
    const Check c1 = check(evaluate(l, r1, tight), "energy.total");
    const Check c2 = check(evaluate(l, r2, tight), "energy.total");
    EXPECT_EQ(c1.status, CheckStatus::Verified) << c1.attribution;
    EXPECT_EQ(c2.status, CheckStatus::Verified) << c2.attribution;
    // What left resonator 1 through the coupling is what its energy lost; resonator 2 gained energy.
    EXPECT_NEAR(c1.output[0], -c1.delta[0], 1e-6 * std::abs(c1.delta[0]));
    EXPECT_LT(c1.delta[0], 0.0);
    EXPECT_GT(c2.delta[0], 0.0);
    // The same exchange: internal to the whole, a boundary output of resonator 1.
    std::string inWhole, inR1;
    for (const auto& x : whole.classified)
        if (x["id"] == "work:r1->coupling")
            inWhole = x["role"];
    for (const auto& x : evaluate(l, r1, tight).classified)
        if (x["id"] == "work:r1->coupling")
            inR1 = x["role"];
    EXPECT_EQ(inWhole, "internal");
    EXPECT_EQ(inR1, "output");
    // The ground holds the resonators: their momentum is not conserved, but balanced by the springs' reactions.
    EXPECT_EQ(check(whole, "momentum.linear").status, CheckStatus::Verified);
}

TEST(AccountingNetworks, ResidualConvergesWithTheStep)
{
    const Network net = coupled();
    AccountingSystem r1{"r1", "", SystemKind::Subsystem, {"body:r1", "element:k1"}};
    // Below about 2 ms the residual reaches rounding (1e-10 of the energy); above, each halving gains more than RK4's 16.
    double previous = 0.0;
    for (double dt : {0.008, 0.004, 0.002}) {
        TransientOptions o;
        o.duration = 4.0;
        o.dt = dt;
        o.x0 = {0.01, 0.0};
        const Ledger l = run(net, o);
        const Check c = check(evaluate(l, r1, tight), "energy.total");
        const double r = *std::max_element(c.residualHistory.begin(), c.residualHistory.end());  // the worst over the run
        if (previous > 0.0)
            EXPECT_GT(previous / r, 12.0) << dt;
        previous = r;
    }
}

TEST(AccountingNetworks, ResonatorQualityAndBuildupDecay)
{
    Network n;
    n.names = {"resonator"};
    n.masses = {1.0};
    const double f0 = 10.0, w0 = 2 * kPi * f0;
    n.springs = {{"spring", 0, -1, w0 * w0}};
    n.dampers = {{"viscous", 0, -1, 0.2}, {"radiation", 0, -1, 0.1}};
    // Steady state: supplied = dissipated, and Q = w E / P per mechanism against the modal Q.
    auto steady = coupling::drivenNetworkLedger(n, {1.0}, f0, {});
    ASSERT_TRUE(steady) << steady.message();
    const Report rs = evaluate(steady.value(), wholeSystem(steady.value()), tight);
    EXPECT_EQ(check(rs, "energy.total").status, CheckStatus::Verified);
    EXPECT_EQ(check(rs, "energy.total").unit, "W");
    const Check q = check(rs, "resonance.quality-factor");
    EXPECT_EQ(q.status, CheckStatus::Verified) << q.details.dump();
    EXPECT_NEAR(q.details["qualityDriven"].get<double>(), w0 / 0.3, 1e-6 * w0 / 0.3);
    EXPECT_NEAR(q.details["byMechanism"][0]["driven"].get<double>(), q.details["byMechanism"][0]["modal"].get<double>(), 1e-6 * w0);
    // Driven from rest at resonance for 20 s, then free: buildup and decay at the modal rate.
    const std::vector<std::complex<double>> force{1.0};
    TransientOptions o;
    o.duration = 30.0;
    const double off = 20.0;
    o.force = [&](double t) { return std::vector<double>{t < off ? std::cos(w0 * t) : 0.0}; };
    o.samples = 30000;
    o.elementFlows = true;
    auto rec = oscillators::integrate(n, o);
    ASSERT_TRUE(rec);
    const Check bd = coupling::buildupDecayCheck(n, rec.value(), f0, force, off, 0.02);
    EXPECT_EQ(bd.status, CheckStatus::Verified) << bd.details.dump();
    EXPECT_NEAR(bd.details["energyDecayRate"].get<double>(), 2 * 0.15, 0.02 * 0.3);
    // The transient's energy account holds throughout.
    auto l = coupling::networkLedger(n, rec.value(), {});
    ASSERT_TRUE(l);
    EXPECT_EQ(check(evaluate(l.value(), wholeSystem(l.value()), tight), "energy.total").status, CheckStatus::Verified);
}

TEST(AccountingNetworks, RotatingShaftAngularMomentumAndTorques)
{
    Network n;
    n.names = {"flywheel", "rotor"};
    n.masses = {0.5, 1.5};  // moments of inertia, kg m^2
    n.springs = {{"shaft", 0, 1, 200.0}};  // N m/rad
    TransientOptions o;
    o.duration = 2.0;
    o.force = [](double t) { return std::vector<double>{t < 0.5 ? 2.0 : 0.0, 0.0}; };
    coupling::NetworkAccounting a;
    a.rotational = true;
    a.axis = {0.0, 0.0, 1.0};
    const Ledger l = run(n, o, a);
    const Report r = evaluate(l, wholeSystem(l), tight);
    const Check L = check(r, "momentum.angular");
    EXPECT_EQ(L.status, CheckStatus::Verified);
    EXPECT_NEAR(L.impulse[2], 1.0, 0.01);       // 2 N m for about 0.5 s
    EXPECT_NEAR(L.final[2], L.impulse[2], 1e-12);  // all of it in the two wheels
    EXPECT_EQ(check(r, "momentum.linear").status, CheckStatus::NotApplicable);
    const Check e = check(r, "energy.total");
    EXPECT_EQ(e.status, CheckStatus::Verified) << e.attribution;
    for (const ForceRecord& f : l.forces)
        EXPECT_EQ(f.family, Family::Torque);  // torques, never energies
    bool rotational = false;
    for (const Holding& h : l.holdings)
        rotational = rotational || h.form == "rotational kinetic";
    EXPECT_TRUE(rotational);
}
