// The Coupled Energy-Matter Engine's infrastructure (RES-CEME-002..009): quantities,
// field data, transfers, material and geometry state, the physical system document,
// the orchestrator (with synthetic participants) and the ledger.
#include "multiphysics/Orchestrator.h"
#include "multiphysics/Transfer.h"

#include <gtest/gtest.h>

#include <cmath>
#include <random>

using namespace resonant;
using namespace resonant::multiphysics;

namespace {

FieldData density(const GridSupport& g, double value)
{
    FieldData f;
    f.quantity = *findQuantity("heat_source_density");
    f.support = g;
    f.values.assign(g.cellCount(), value);
    f.producer = "test";
    return f;
}

GridSupport column(std::vector<double> z)
{
    GridSupport g;
    g.edges[0] = {-0.05, 0.05};
    g.edges[1] = {-0.05, 0.05};
    g.edges[2] = std::move(z);
    return g;
}

}  // namespace

TEST(Multiphysics, QuantitiesAreTypedAndCompared)
{
    const QuantityDescriptor* t = findQuantity("temperature");
    ASSERT_NE(t, nullptr);
    EXPECT_EQ(t->unit, "K");
    EXPECT_FALSE(t->conservative);
    const QuantityDescriptor* q = findQuantity("absorbed_power_density");
    ASSERT_NE(q, nullptr);
    EXPECT_EQ(q->unit, "W/m^3");
    EXPECT_EQ(q->meaning, Meaning::TimeAveraged);
    EXPECT_TRUE(q->conservative);
    EXPECT_EQ(findQuantity("stress")->components(), 6);
    EXPECT_EQ(findQuantity("electric_field")->representation, Representation::Complex);
    EXPECT_EQ(findQuantity("no_such_quantity"), nullptr);
    EXPECT_TRUE(compatible(*t, *t));
    EXPECT_FALSE(compatible(*q, *findQuantity("heat_source_density")));  // a declared conversion is needed
    QuantityDescriptor other = *t;
    other.unit = "degC";
    EXPECT_FALSE(compatible(*t, other));
    for (const QuantityDescriptor& d : quantityRegistry())
        EXPECT_FALSE(d.unit.empty()) << d.id;
}

TEST(Multiphysics, FieldDataIntegratesAndRejectsInvalidValues)
{
    const GridSupport g = column({0.0, 0.01, 0.03});
    FieldData f = density(g, 1e5);
    EXPECT_TRUE(f.validate());
    EXPECT_NEAR(f.integral(), 1e5 * 0.01 * 0.03, 1e-12);
    f.values[1] = std::nan("");
    EXPECT_FALSE(f.validate());
    f.values = {1.0};
    EXPECT_FALSE(f.validate());
    EXPECT_EQ(f.summary()["quantity"], "heat_source_density");
}

TEST(Multiphysics, ConservativeTransferPreservesTheIntegral)
{
    std::mt19937 rng(7);
    std::uniform_real_distribution<double> u(0.0, 1.0);
    // Random non-matching column grids over the same 0..0.02 m.
    auto randomEdges = [&](int n) {
        std::vector<double> e{0.0};
        double sum = 0.0;
        std::vector<double> w(static_cast<size_t>(n));
        for (double& x : w)
            sum += (x = 0.2 + u(rng));
        for (double x : w)
            e.push_back(e.back() + 0.02 * x / sum);
        e.back() = 0.02;
        return e;
    };
    const GridSupport source = column(randomEdges(37));
    FieldData f = density(source, 0.0);
    for (double& v : f.values)
        v = 1e6 * u(rng);
    TransferReport report;
    auto moved = transfer(f, column(randomEdges(11)), TransferMethod::Conservative, &report);
    ASSERT_TRUE(moved) << moved.message();
    EXPECT_NEAR(moved.value().integral(), f.integral(), 1e-12 * f.integral());
    EXPECT_LT(report.relativeDifference, 1e-12);
    EXPECT_NEAR(report.coveredFraction, 1.0, 1e-12);
    // A target covering half the source keeps the half it covers, and says so.
    auto half = transfer(density(source, 2.0), column({0.0, 0.004, 0.01}), TransferMethod::Conservative, &report);
    ASSERT_TRUE(half);
    EXPECT_NEAR(report.targetIntegral, 2.0 * 0.01 * 0.01, 1e-15);
    EXPECT_NEAR(report.coveredFraction, 0.5, 1e-12);
    // Intensive quantities are not remapped conservatively.
    FieldData t = f;
    t.quantity = *findQuantity("temperature");
    EXPECT_FALSE(transfer(t, column({0.0, 0.02}), TransferMethod::Conservative));
    FieldData elsewhere = f;
    elsewhere.support.frame = "local";
    EXPECT_FALSE(transfer(elsewhere, column({0.0, 0.02}), TransferMethod::Conservative));
}

TEST(Multiphysics, InterpolatingTransferIsExactForLinearFieldsAndClampsAtTheEnds)
{
    const GridSupport source = column({0.0, 0.005, 0.01, 0.015, 0.02});
    FieldData t;
    t.quantity = *findQuantity("temperature");
    t.support = source;
    for (size_t c = 0; c < source.cellCount(); ++c)
        t.values.push_back(300.0 + 1000.0 * source.cellCentre(c).z());
    auto moved = transfer(t, column({0.0, 0.001, 0.0101, 0.0199, 0.02}), TransferMethod::Interpolating);
    ASSERT_TRUE(moved);
    const GridSupport& g = moved.value().support;
    EXPECT_NEAR(moved.value().values[1], 300.0 + 1000.0 * g.cellCentre(1).z(), 1e-9);
    EXPECT_NEAR(moved.value().values[2], 300.0 + 1000.0 * g.cellCentre(2).z(), 1e-9);
    EXPECT_NEAR(moved.value().values[0], t.values[0], 1e-12);   // below the first centre: clamped
    EXPECT_NEAR(moved.value().values[3], t.values[3], 1e-12);   // beyond the last centre: clamped
}

TEST(Multiphysics, MaterialStateLawsEvaluateAndReportExtrapolation)
{
    PropertyLaw linear{"slab", "electromagnetic.lossTangent", LawKind::LinearInTemperature, 0.02, 300.0, 1e-4, {}, 250.0, 400.0, "measured", "test"};
    EXPECT_NEAR(evaluate(linear, 350.0).value, 0.025, 1e-12);
    EXPECT_FALSE(evaluate(linear, 350.0).extrapolated);
    const LawValue hot = evaluate(linear, 500.0);
    EXPECT_TRUE(hot.extrapolated);
    EXPECT_NEAR(hot.value, 0.03, 1e-12);  // clamped at 400 K
    PropertyLaw table{"slab", "thermal.thermalConductivity", LawKind::Table, 0, 0, 0, {{300.0, 1.0}, {400.0, 2.0}}, 200.0, 500.0, "measured", "test"};
    EXPECT_NEAR(evaluate(table, 325.0).value, 1.25, 1e-12);
    EXPECT_FALSE(PropertyLaw::fromJson({{"region", "slab"}, {"property", "x.y"}, {"law", "table"}, {"table", {{300, 1}}}}));
    auto round = PropertyLaw::fromJson(linear.toJson());
    ASSERT_TRUE(round) << round.message();
    EXPECT_DOUBLE_EQ(round.value().slope, 1e-4);

    MaterialState state;
    state.setReferenceTemperature(300.0);
    state.addLaw(linear);
    EXPECT_NEAR(state.property("slab", "electromagnetic.lossTangent", Vec3::Zero(), 0.5), 0.02, 1e-12);  // reference temperature
    EXPECT_NEAR(state.property("other", "electromagnetic.lossTangent", Vec3::Zero(), 0.5), 0.5, 1e-12);  // no law: intrinsic
    FieldData temperature;
    temperature.quantity = *findQuantity("temperature");
    temperature.support = column({0.0, 0.01, 0.02});
    temperature.values = {340.0, 360.0};
    auto change = state.updateTemperature(temperature, 0.5);  // relaxed from the reference
    ASSERT_TRUE(change);
    EXPECT_NEAR(change.value(), 30.0, 1e-12);
    EXPECT_EQ(state.revision(), 1u);
    EXPECT_NEAR(state.temperatureAt(Vec3(0, 0, 0.005)), 320.0, 1e-9);
    temperature.values = {340.0, 360.0};
    change = state.updateTemperature(temperature, 1.0);
    EXPECT_NEAR(change.value(), 30.0, 1e-12);
    EXPECT_EQ(state.revision(), 2u);
    EXPECT_EQ(state.history().size(), 2u);
    temperature.values = {600.0, 600.0};
    ASSERT_TRUE(state.updateTemperature(temperature));
    state.property("slab", "electromagnetic.lossTangent", Vec3(0, 0, 0.005), 0.0);
    ASSERT_EQ(state.warnings().size(), 1u);
    EXPECT_NE(state.warnings().begin()->find("outside its validity"), std::string::npos);
    FieldData wrong = temperature;
    wrong.quantity = *findQuantity("displacement");
    EXPECT_FALSE(state.updateTemperature(wrong));
}

TEST(Multiphysics, GeometryStateVersionsUpdates)
{
    GeometryState g;
    g.setReference("slab", "thickness", 0.01);
    const std::uint64_t start = g.revision();
    auto change = g.apply({{"slab", "thickness", 0.0101}}, 0.5);
    ASSERT_TRUE(change);
    EXPECT_NEAR(change.value(), 0.005, 1e-12);
    EXPECT_NEAR(g.current("slab", "thickness"), 0.01005, 1e-15);
    EXPECT_DOUBLE_EQ(g.reference("slab", "thickness"), 0.01);
    EXPECT_EQ(g.revision(), start + 1);
    EXPECT_TRUE(g.stale(start));
    ASSERT_TRUE(g.apply({{"slab", "thickness", 0.01005}}));
    EXPECT_EQ(g.revision(), start + 1);  // nothing changed
    EXPECT_FALSE(g.apply({{"slab", "thickness", -1.0}}));
    EXPECT_FALSE(g.apply({{"nowhere", "thickness", 1.0}}));
}

TEST(Multiphysics, PhysicalSystemRoundTripsAndValidates)
{
    PhysicalSystem s;
    s.id = "sys-1";
    s.name = "test";
    s.regions = {{"r1", "Layer", "mat-a", 0.01, 10, "", {}}};
    s.domains = {{"d1", "thermal", "thermal.conduction.fv", {{"faces", {{"-z", {{"kind", "temperature"}}}}}}}};
    s.couplings = {};
    PropertyLaw constant;
    constant.region = "r1";
    constant.property = "thermal.thermalConductivity";
    constant.value = 1.0;
    s.laws = {constant};
    s.scheme.kind = "iterative";
    auto back = PhysicalSystem::fromJson(s.toJson());
    ASSERT_TRUE(back) << back.message();
    EXPECT_EQ(back.value().toJson(), s.toJson());
    EXPECT_EQ(back.value().checksum(), s.checksum());
    s.name = "renamed";
    EXPECT_NE(back.value().checksum(), s.checksum());
    EXPECT_FALSE(PhysicalSystem::fromJson({{"format", "something else"}}));
    EXPECT_FALSE(PhysicalSystem::fromJson({{"format", "resonant.physical-system"}, {"version", 9}}));

    const std::vector<CouplingModelDescriptor> models = {
        {"m.ok", "ok", "thermal", "mechanical", "field-transfer", "temperature", "temperature", "interpolating", "", "", "validated", "", ""},
        {"m.plan", "plan", "thermal", "mechanical", "field-transfer", "temperature", "temperature", "interpolating", "", "", "planned", "", "not yet"},
        {"m.state", "state", "thermal", "mechanical", "state-update", "temperature", "temperature", "interpolating", "", "", "validated", "", ""}};
    const std::vector<std::pair<std::string, std::string>> solvers = {{"thermal", "thermal.conduction.fv"}, {"mechanical", "fe"}};
    const auto exists = [](const std::string& id) { return id == "mat-a"; };
    const auto errors = [&](const PhysicalSystem& sys) {
        int n = 0;
        for (const auto& i : validateSystem(sys, models, exists, solvers))
            n += i.severity == 3;
        return n;
    };
    EXPECT_EQ(errors(s), 0);
    PhysicalSystem bad = s;
    bad.regions[0].materialId = "missing";
    bad.domains.push_back({"d1", "mechanical", "fe", {}});  // duplicate id
    bad.couplings = {{"c1", "m.plan", "d1", "d1", ""}, {"c2", "m.none", "d1", "d1", ""}};
    bad.scheme.kind = "sideways";
    EXPECT_GE(errors(bad), 5);
    PhysicalSystem oneWay = s;
    oneWay.domains.push_back({"d2", "mechanical", "fe", {}});
    oneWay.couplings = {{"c1", "m.state", "d1", "d2", ""}};
    oneWay.scheme.kind = "one-way";
    EXPECT_EQ(errors(oneWay), 1);  // feedback needs sequential or iterative
}

// --- the orchestrator with synthetic participants -------------------------------------------

namespace {

// Produces a uniform heat source of `power` W in total, or (as a "thermal"
// participant) takes one and returns T = 300 + gain * (T_state - 300) + offset.
class Synthetic final : public Participant {
public:
    Synthetic(std::string id, std::string domain) : id_(std::move(id)), domain_(std::move(domain)) {}
    std::string id() const override { return id_; }
    std::string domain() const override { return domain_; }
    std::string solverId() const override { return "synthetic"; }
    std::string solverVersion() const override { return "1"; }
    Status prepare(SystemState& state) override
    {
        geometry_ = state.geometry.revision();
        material_ = state.materials.revision();
        return Status::success();
    }
    Status solve(SystemState& state, const RunContext&) override
    {
        ++solves;
        if (failAt > 0 && solves == failAt)
            return Status::failure("synthetic failure");
        temperature_ = 300.0 + gain * (state.materials.temperatureAt(Vec3::Zero()) - 300.0) + offset;
        return Status::success();
    }
    std::optional<FieldData> provide(const std::string& quantity, const SystemState&) const override
    {
        if (quantity == "heat_source_density") {
            FieldData f = density(support(SystemState{}), power / support(SystemState{}).volume());
            if (poison)
                f.values[0] = std::nan("");
            return f;
        }
        if (quantity == "temperature") {
            FieldData f;
            f.quantity = *findQuantity("temperature");
            f.support = support(SystemState{});
            f.values.assign(f.support.cellCount(), temperature_);
            return f;
        }
        return std::nullopt;
    }
    GridSupport support(const SystemState&) const override { return column({0.0, 0.004, 0.01}); }
    Status accept(const FieldData& field) override
    {
        received = field.integral();
        return Status::success();
    }
    void account(EnergyLedger& ledger, const std::set<std::string>& transferred) const override
    {
        if (power > 0.0) {
            ledger.add(LedgerTerm{id_, domain_, TermKind::Input, "synthetic source", power, "", ""});
            if (!transferred.contains("heat_source_density"))
                ledger.add(LedgerTerm{id_, domain_, TermKind::Dissipated, "synthetic loss", power, "", ""});
        }
        if (received > 0.0)
            ledger.add(LedgerTerm{id_, domain_, TermKind::BoundaryOut, "synthetic sink", received * leak, "", ""});
    }
    nlohmann::json monitor() const override { return {{"temperature", temperature_}}; }
    nlohmann::json summary() const override { return monitor(); }
    std::uint64_t solvedGeometryRevision() const override { return geometry_; }
    std::uint64_t solvedMaterialRevision() const override { return material_; }

    double power = 0.0, received = 0.0, leak = 1.0, gain = 0.0, offset = 0.0;
    int solves = 0, failAt = 0;
    bool poison = false;

private:
    std::string id_, domain_;
    double temperature_ = 300.0;
    std::uint64_t geometry_ = 0, material_ = 0;
};

const std::vector<CouplingModelDescriptor>& syntheticModels()
{
    static const std::vector<CouplingModelDescriptor> m = {
        {"heat", "heat", "source", "thermal", "field-transfer", "heat_source_density", "heat_source_density", "conservative", "synthetic heating", "",
         "validated", "", ""},
        {"feedback", "feedback", "thermal", "source", "state-update", "temperature", "temperature", "interpolating", "", "", "validated", "", ""}};
    return m;
}

PhysicalSystem twoDomains(const std::string& scheme)
{
    PhysicalSystem s;
    s.id = "synthetic";
    s.ambientTemperature = 300.0;
    s.regions = {{"r", "r", "m", 0.01, 2, "", {}}};
    s.domains = {{"src", "source", "synthetic", {}}, {"sink", "thermal", "synthetic", {}}};
    s.couplings = {{"c-heat", "heat", "src", "sink", ""}};
    if (scheme != "one-way")
        s.couplings.push_back({"c-back", "feedback", "sink", "src", ""});
    s.scheme.kind = scheme;
    s.scheme.maxIterations = 100;
    s.scheme.temperatureTolerance = 1e-9;
    return s;
}

}  // namespace

TEST(Multiphysics, OrchestratorRunsOneWayAndBalancesTheLedger)
{
    auto src = std::make_unique<Synthetic>("src", "source");
    auto sink = std::make_unique<Synthetic>("sink", "thermal");
    src->power = 12.0;
    Synthetic* sinkPtr = sink.get();
    std::vector<std::unique_ptr<Participant>> ps;
    ps.push_back(std::move(sink));  // listed after its consumer: the order comes from the graph
    ps.push_back(std::move(src));
    Orchestrator o(twoDomains("one-way"), std::move(ps), syntheticModels());
    auto order = o.executionOrder();
    ASSERT_TRUE(order);
    EXPECT_EQ(order.value(), (std::vector<std::string>{"src", "sink"}));
    const CoupledResult r = o.run();
    ASSERT_EQ(r.status, RunStatus::Completed) << r.message;
    EXPECT_NEAR(sinkPtr->received, 12.0, 1e-12);
    ASSERT_EQ(r.ledger.transfers().size(), 1u);
    EXPECT_TRUE(r.ledger.transfers()[0].energy);
    EXPECT_TRUE(r.audit.balanced) << r.audit.toJson().dump();
    EXPECT_NEAR(r.audit.externalInput, 12.0, 1e-12);
    EXPECT_NEAR(r.audit.boundaryOut, 12.0, 1e-12);
    EXPECT_EQ(r.toJson()["format"], "resonant.coupled-result");
}

TEST(Multiphysics, LedgerAttributesAnImbalance)
{
    auto src = std::make_unique<Synthetic>("src", "source");
    auto sink = std::make_unique<Synthetic>("sink", "thermal");
    src->power = 10.0;
    sink->leak = 0.9;  // 10 % unaccounted
    std::vector<std::unique_ptr<Participant>> ps;
    ps.push_back(std::move(src));
    ps.push_back(std::move(sink));
    Orchestrator o(twoDomains("one-way"), std::move(ps), syntheticModels());
    const CoupledResult r = o.run();
    EXPECT_FALSE(r.audit.balanced);
    EXPECT_NEAR(r.audit.systemResidual, 1.0, 1e-12);
    EXPECT_NEAR(r.audit.participants[1].residual, 1.0, 1e-12);
}

TEST(Multiphysics, OrchestratorIteratesToTheFixedPoint)
{
    // T = 300 + 0.5 (T - 300) + 10  ->  T* = 320.
    auto src = std::make_unique<Synthetic>("src", "source");
    auto sink = std::make_unique<Synthetic>("sink", "thermal");
    src->power = 1.0;
    sink->gain = 0.5;
    sink->offset = 10.0;
    std::vector<std::unique_ptr<Participant>> ps;
    ps.push_back(std::move(src));
    ps.push_back(std::move(sink));
    Orchestrator o(twoDomains("iterative"), std::move(ps), syntheticModels());
    const CoupledResult r = o.run();
    ASSERT_EQ(r.status, RunStatus::Converged) << r.message;
    EXPECT_NEAR(r.participants[1]["temperature"].get<double>(), 320.0, 1e-8);
    EXPECT_LT(r.passes, 60);
    EXPECT_NEAR(o.state().materials.temperatureAt(Vec3::Zero()), 320.0, 1e-8);

    PhysicalSystem slow = twoDomains("iterative");
    slow.scheme.maxIterations = 3;
    auto a = std::make_unique<Synthetic>("src", "source");
    auto b = std::make_unique<Synthetic>("sink", "thermal");
    b->gain = 0.5;
    b->offset = 10.0;
    std::vector<std::unique_ptr<Participant>> qs;
    qs.push_back(std::move(a));
    qs.push_back(std::move(b));
    const CoupledResult n = Orchestrator(slow, std::move(qs), syntheticModels()).run();
    EXPECT_EQ(n.status, RunStatus::NotConverged);
    EXPECT_EQ(n.passes, 3);
    EXPECT_NE(n.message.find("not converged"), std::string::npos);
}

TEST(Multiphysics, OrchestratorStopsOnFailureInvalidValuesAndCancellation)
{
    const auto make = [](int failAt, bool poison) {
        auto src = std::make_unique<Synthetic>("src", "source");
        auto sink = std::make_unique<Synthetic>("sink", "thermal");
        src->power = 1.0;
        src->poison = poison;
        sink->failAt = failAt;
        sink->gain = 0.5;
        std::vector<std::unique_ptr<Participant>> ps;
        ps.push_back(std::move(src));
        ps.push_back(std::move(sink));
        return ps;
    };
    CoupledResult failed = Orchestrator(twoDomains("iterative"), make(2, false), syntheticModels()).run();
    EXPECT_EQ(failed.status, RunStatus::Failed);
    EXPECT_EQ(failed.failedStage, "solve sink (pass 2)");
    EXPECT_EQ(failed.history.size(), 2u);  // the first pass is kept

    CoupledResult poisoned = Orchestrator(twoDomains("one-way"), make(0, true), syntheticModels()).run();
    EXPECT_EQ(poisoned.status, RunStatus::Failed);
    EXPECT_NE(poisoned.message.find("not passed on"), std::string::npos);

    int calls = 0;
    RunContext cancel;
    cancel.cancelled = [&calls] { return ++calls > 3; };
    CoupledResult cancelled = Orchestrator(twoDomains("iterative"), make(0, false), syntheticModels()).run(cancel);
    EXPECT_EQ(cancelled.status, RunStatus::Cancelled);
    EXPECT_FALSE(cancelled.ok());

    PhysicalSystem loop = twoDomains("one-way");
    loop.couplings.push_back({"c-loop", "heat", "sink", "src", ""});
    EXPECT_FALSE(Orchestrator(loop, make(0, false), syntheticModels()).executionOrder());
}
