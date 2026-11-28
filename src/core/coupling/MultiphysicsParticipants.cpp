#include "coupling/MultiphysicsParticipants.h"

#include "coupling/CavityParticipants.h"

#include "electromagnetics/LayeredMedia.h"
#include "mechanics/Analyses.h"
#include "mechanics/ElasticMaterial.h"
#include "mechanics/ThermalLoads.h"
#include "thermal/HeatConduction.h"
#include "thermal/ThermalMaterial.h"

#include <Eigen/Dense>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace resonant::coupling {
namespace {

using multiphysics::DomainDef;
using multiphysics::EnergyLedger;
using multiphysics::FieldData;
using multiphysics::GeometryChange;
using multiphysics::GridSupport;
using multiphysics::LedgerTerm;
using multiphysics::Participant;
using multiphysics::PhysicalSystem;
using multiphysics::RunContext;
using multiphysics::SystemState;
using multiphysics::TermKind;

constexpr std::uint64_t kNever = std::numeric_limits<std::uint64_t>::max();

// The column's cells along z: their region and edges, in the reference or the current geometry.
struct Column {
    std::vector<double> edges;         // z, m
    std::vector<size_t> region;        // per cell: index into system.regions
    double side = 0.0;                 // m: the square lateral side (sqrt of the area)
};

Column column(const PhysicalSystem& system, const SystemState& state, bool current)
{
    Column c;
    c.side = std::sqrt(system.lateralArea);
    c.edges.push_back(0.0);
    for (size_t r = 0; r < system.regions.size(); ++r) {
        const auto& def = system.regions[r];
        const double d = current ? state.geometry.current(def.id, "thickness") : state.geometry.reference(def.id, "thickness");
        const double start = c.edges.back();
        for (int i = 1; i <= def.cells; ++i) {
            c.edges.push_back(start + d * i / def.cells);
            c.region.push_back(r);
        }
    }
    return c;
}

GridSupport support(const Column& c)
{
    GridSupport g;
    g.edges[0] = {-c.side / 2, c.side / 2};
    g.edges[1] = {-c.side / 2, c.side / 2};
    g.edges[2] = c.edges;
    return g;
}

const Material* findMaterial(const MaterialLibrary& library, const std::string& id)
{
    return library.find(Id(id));
}

double intrinsic(const Material& m, PhysicsDomain domain, const char* key, double fallback)
{
    const PropertyValue* v = m.property(domain, key);
    return v ? v->value : fallback;
}

Result<thermal::FaceCondition> faceCondition(const nlohmann::json& j, double ambient)
{
    thermal::FaceCondition f;
    f.ambient = ambient;
    f.temperature = ambient;
    const std::string kind = j.value("kind", "adiabatic");
    if (kind == "adiabatic")
        f.kind = thermal::FaceKind::Adiabatic;
    else if (kind == "temperature") {
        f.kind = thermal::FaceKind::Temperature;
        f.temperature = j.value("temperature", ambient);
    } else if (kind == "convective") {
        f.kind = thermal::FaceKind::Convective;
        f.heatTransferCoefficient = j.value("h", 0.0);
        f.ambient = j.value("ambient", ambient);
        if (!(f.heatTransferCoefficient > 0.0))
            return Result<thermal::FaceCondition>::failure("a convective face needs a positive heat transfer coefficient h (W/(m^2 K))");
    } else
        return Result<thermal::FaceCondition>::failure("unknown face condition '" + kind + "' (adiabatic, temperature, convective)");
    return f;
}

// --- electromagnetics: layered plane wave ------------------------------------------------

class LayeredEmParticipant final : public Participant {
public:
    LayeredEmParticipant(PhysicalSystem system, DomainDef def, MaterialLibrary materials)
        : system_(std::move(system)), def_(std::move(def)), materials_(std::move(materials))
    {
    }
    std::string id() const override { return def_.id; }
    std::string domain() const override { return "electromagnetic"; }
    std::string solverId() const override { return em::kLayeredSolverId; }
    std::string solverVersion() const override { return em::kLayeredSolverVersion; }

    Status prepare(SystemState& state) override
    {
        if (problem_ && preparedGeometry_ == state.geometry.revision() && preparedMaterial_ == state.materials.revision())
            return Status::success();
        em::LayeredProblem p;
        p.frequencyHz = def_.settings.value("frequencyHz", 2.45e9);
        p.incidentIntensity = def_.settings.value("incidentIntensity", 1000.0);
        const auto halfSpace = [this](const std::string& id, em::MediumLayer& out) -> Status {
            out.name = id.empty() ? "vacuum" : id;
            if (id.empty())
                return Status::success();
            const Material* m = findMaterial(materials_, id);
            if (!m)
                return Status::failure("the material '" + id + "' is not in the library");
            out.name = m->name();
            out.permittivityReal = intrinsic(*m, PhysicsDomain::Electromagnetic, "relativePermittivity", 1.0);
            out.permeability = intrinsic(*m, PhysicsDomain::Electromagnetic, "relativePermeability", 1.0);
            return Status::success();
        };
        if (Status s = halfSpace(system_.incidenceMaterial, p.incidence); !s)
            return s;
        if (Status s = halfSpace(system_.exitMaterial, p.exit); !s)
            return s;
        reference_ = column(system_, state, false);
        current_ = column(system_, state, true);
        for (size_t c = 0; c + 1 < current_.edges.size(); ++c) {
            const auto& region = system_.regions[current_.region[c]];
            const Material* m = findMaterial(materials_, region.materialId);
            if (!m)
                return Status::failure("the material '" + region.materialId + "' is not in the library");
            if (!m->appliesTo(PhysicsDomain::Electromagnetic))
                return Status::failure("the material '" + m->name() + "' of region '" + region.id + "' has no electromagnetic properties");
            const Vec3 at(0, 0, 0.5 * (reference_.edges[c] + reference_.edges[c + 1]));  // material point (reference)
            em::MediumLayer l;
            l.name = region.id + "#" + std::to_string(c);
            l.thickness = current_.edges[c + 1] - current_.edges[c];
            l.permittivityReal = state.materials.property(region.id, "electromagnetic.relativePermittivity", at,
                                                          intrinsic(*m, PhysicsDomain::Electromagnetic, "relativePermittivity", 1.0));
            l.lossTangent = state.materials.property(region.id, "electromagnetic.lossTangent", at,
                                                     intrinsic(*m, PhysicsDomain::Electromagnetic, "lossTangent", 0.0));
            l.conductivity = state.materials.property(region.id, "electromagnetic.conductivity", at,
                                                      intrinsic(*m, PhysicsDomain::Electromagnetic, "conductivity", 0.0));
            l.permeability = intrinsic(*m, PhysicsDomain::Electromagnetic, "relativePermeability", 1.0);
            p.layers.push_back(l);
        }
        if (Status s = p.validate(); !s)
            return s;
        problem_ = p;
        preparedGeometry_ = state.geometry.revision();
        preparedMaterial_ = state.materials.revision();
        return Status::success();
    }

    Status solve(SystemState&, const RunContext&) override
    {
        auto s = em::LayeredSolution::solve(*problem_);
        if (!s)
            return Status::failure(s.message());
        solution_ = s.take();
        solvedGeometry_ = preparedGeometry_;
        solvedMaterial_ = preparedMaterial_;
        return Status::success();
    }

    std::optional<FieldData> provide(const std::string& quantity, const SystemState&) const override
    {
        if (quantity != "absorbed_power_density" || !solution_)
            return std::nullopt;
        FieldData f;
        f.quantity = *multiphysics::findQuantity("absorbed_power_density");
        f.support = support(reference_);
        f.time = multiphysics::TimeCharacter::Harmonic;
        f.frequencyHz = problem_->frequencyHz;
        f.producer = id();
        f.solver = solverId() + " " + solverVersion();
        f.geometryRevision = solvedGeometry_;
        f.materialRevision = solvedMaterial_;
        f.values.resize(reference_.region.size());
        for (size_t c = 0; c < f.values.size(); ++c) {
            // Power per lateral area absorbed in the deformed sublayer, per unit of its reference thickness.
            const double absorbed = solution_->absorbedPower(current_.edges[c], current_.edges[c + 1]);
            f.values[c] = absorbed / (reference_.edges[c + 1] - reference_.edges[c]);
        }
        return f;
    }

    GridSupport support(const SystemState&) const override { return support(reference_); }
    Status accept(const FieldData& field) override { return Status::failure("the layered wave model takes no field inputs (" + field.quantity.id + ")"); }

    void account(EnergyLedger& ledger, const std::set<std::string>& transferred) const override
    {
        if (!solution_)
            return;
        // A steady wave: its powers enter a transient ledger multiplied by the interval.
        const double scale = ledger.powerScale();
        const double incident = problem_->incidentIntensity * system_.lateralArea * scale;
        ledger.add(LedgerTerm{id(), domain(), TermKind::Input, "incident plane wave", incident, "", "intensity x lateral area"});
        ledger.add(LedgerTerm{id(), domain(), TermKind::BoundaryOut, "reflected wave", solution_->reflectance() * incident, "", ""});
        ledger.add(LedgerTerm{id(), domain(), TermKind::BoundaryOut, "transmitted wave", solution_->transmittance() * incident, "", ""});
        if (!transferred.contains("absorbed_power_density"))
            ledger.add(LedgerTerm{id(), domain(), TermKind::Dissipated, "dielectric and conduction loss", absorbedPower() * scale, "",
                                  "not passed to another domain"});
    }

    double solverResidual() const override
    {
        if (!solution_)
            return 0.0;
        // The fluxes at the stack's faces against 1 - R - T: rounding for the exact solution.
        const double incident = problem_->incidentIntensity * system_.lateralArea;
        return (solution_->absorptance() - (solution_->poyntingFlux(0.0) - solution_->poyntingFlux(problem_->totalThickness())) /
                                               std::max(problem_->incidentIntensity, 1e-300)) *
               incident;
    }

    nlohmann::json monitor() const override
    {
        if (!solution_)
            return {};
        return {{"reflectance", solution_->reflectance()}, {"transmittance", solution_->transmittance()},
                {"absorptance", solution_->absorptance()}, {"absorbedPowerW", absorbedPower()}};
    }

    nlohmann::json summary() const override
    {
        nlohmann::json j = monitor();
        if (!problem_)
            return j;
        j["frequencyHz"] = problem_->frequencyHz;
        j["incidentIntensityWm2"] = problem_->incidentIntensity;
        j["incidentPowerW"] = problem_->incidentIntensity * system_.lateralArea;
        j["sublayers"] = problem_->layers.size();
        j["thicknessM"] = problem_->totalThickness();
        double maxE = 0.0;
        if (solution_)
            for (size_t c = 0; c + 1 < current_.edges.size(); ++c)
                maxE = std::max(maxE, std::abs(solution_->electricField(0.5 * (current_.edges[c] + current_.edges[c + 1]))));
        j["maxElectricFieldVm"] = maxE;
        j["method"] = "exact transfer-matrix solution of a layered medium, plane wave at normal incidence, e^{+j omega t} peak amplitudes";
        return j;
    }

    std::vector<std::string> resultQuantities() const override { return {"absorbed_power_density"}; }
    std::uint64_t solvedGeometryRevision() const override { return solvedGeometry_; }
    std::uint64_t solvedMaterialRevision() const override { return solvedMaterial_; }

private:
    double absorbedPower() const { return solution_ ? solution_->absorptance() * problem_->incidentIntensity * system_.lateralArea : 0.0; }
    static GridSupport support(const Column& c) { return coupling::support(c); }

    PhysicalSystem system_;
    DomainDef def_;
    MaterialLibrary materials_;
    std::optional<em::LayeredProblem> problem_;
    std::optional<em::LayeredSolution> solution_;
    Column reference_, current_;
    std::uint64_t preparedGeometry_ = kNever, preparedMaterial_ = kNever, solvedGeometry_ = kNever, solvedMaterial_ = kNever;
};

// --- thermal: conduction in a column -------------------------------------------------------

class ThermalColumnParticipant final : public Participant {
public:
    ThermalColumnParticipant(PhysicalSystem system, DomainDef def, MaterialLibrary materials)
        : system_(std::move(system)), def_(std::move(def)), materials_(std::move(materials))
    {
    }
    std::string id() const override { return def_.id; }
    std::string domain() const override { return "thermal"; }
    std::string solverId() const override { return thermal::kSolverId; }
    std::string solverVersion() const override { return thermal::kSolverVersion; }
    bool transient() const { return def_.settings.contains("transient"); }

    Status prepare(SystemState& state) override
    {
        reference_ = column(system_, state, false);
        thermal::ConductionProblem p;
        p.grid.edges[0] = {-reference_.side / 2, reference_.side / 2};
        p.grid.edges[1] = {-reference_.side / 2, reference_.side / 2};
        p.grid.edges[2] = reference_.edges;
        p.referenceTemperature = system_.ambientTemperature;
        const nlohmann::json faces = def_.settings.value("faces", nlohmann::json::object());
        for (const auto& [name, index] : {std::pair{"-z", 4}, std::pair{"+z", 5}}) {
            auto f = faceCondition(faces.value(name, nlohmann::json{{"kind", "adiabatic"}}), system_.ambientTemperature);
            if (!f)
                return Status::failure(std::string(name) + " face: " + f.message());
            p.faces[static_cast<size_t>(index)] = f.value();
        }
        const size_t n = reference_.region.size();
        p.conductivity.resize(n);
        p.volumetricHeatCapacity.resize(n);
        for (size_t c = 0; c < n; ++c) {
            const auto& region = system_.regions[reference_.region[c]];
            const Material* m = findMaterial(materials_, region.materialId);
            if (!m)
                return Status::failure("the material '" + region.materialId + "' is not in the library");
            auto tm = thermal::thermalMaterial(*m);
            if (!tm)
                return Status::failure(tm.message());
            const Vec3 at(0, 0, 0.5 * (reference_.edges[c] + reference_.edges[c + 1]));
            p.conductivity[c] = state.materials.property(region.id, "thermal.thermalConductivity", at, tm.value().conductivity);
            const double rho = state.materials.property(region.id, "thermal.density", at, tm.value().density);
            const double cp = state.materials.property(region.id, "thermal.specificHeat", at, tm.value().specificHeat);
            p.volumetricHeatCapacity[c] = rho * cp;
        }
        // Thermal contact resistances between adjacent regions (REQ-CHAT-20261127-001, RES-FAM-013):
        // settings.interfaces = [{"regions": [a, b], "resistance": m^2 K/W}].
        const nlohmann::json interfaces = def_.settings.value("interfaces", nlohmann::json::array());
        if (!interfaces.empty() && n > 1) {
            std::vector<double> planes(n - 1, 0.0);
            for (const auto& in : interfaces) {
                const auto names = in.value("regions", nlohmann::json::array());
                if (!names.is_array() || names.size() != 2 || !names[0].is_string() || !names[1].is_string())
                    return Status::failure("an interface names two regions");
                const double r = in.value("resistance", -1.0);
                if (!(r >= 0.0) || !std::isfinite(r))
                    return Status::failure("an interface needs a non-negative resistance (m^2 K/W)");
                bool found = false;
                for (size_t c = 0; c + 1 < n; ++c) {
                    const std::string& a = system_.regions[reference_.region[c]].id;
                    const std::string& b = system_.regions[reference_.region[c + 1]].id;
                    if (a != b && ((a == names[0].get<std::string>() && b == names[1].get<std::string>()) ||
                                   (a == names[1].get<std::string>() && b == names[0].get<std::string>()))) {
                        planes[c] = r;
                        found = true;
                    }
                }
                if (!found)
                    return Status::failure("the interface between '" + names[0].get<std::string>() + "' and '" + names[1].get<std::string>() +
                                           "' joins regions that are not adjacent in the slab");
            }
            p.contactResistance[2] = planes;
        }
        if (source_.size() == n)
            p.source = source_;
        else {
            const double uniform = def_.settings.value("volumetricSource", 0.0);
            ownSource_ = uniform != 0.0;
            p.source.assign(n, uniform);
        }
        if (transient()) {
            // A time-stepped evolution starts each step from the previous one's temperature field
            // (RES-DMS-009); a single transient run starts at the ambient temperature.
            const nlohmann::json initial = def_.settings["transient"].value("initialTemperature", nlohmann::json::array());
            if (initial.empty())
                p.initialTemperature.assign(n, system_.ambientTemperature);
            else if (initial.size() == n)
                p.initialTemperature = initial.get<std::vector<double>>();
            else
                return Status::failure("the transient initial temperature has " + std::to_string(initial.size()) + " values for " + std::to_string(n) +
                                       " cells");
        }
        problem_ = p;
        preparedMaterial_ = state.materials.revision();
        preparedGeometry_ = state.geometry.revision();
        return Status::success();
    }

    Status solve(SystemState&, const RunContext& context) override
    {
        if (!source_.empty() && source_.size() == problem_->grid.cellCount())
            problem_->source = source_;
        Result<thermal::ConductionSolution> s = Result<thermal::ConductionSolution>::failure("not solved");
        if (transient()) {
            thermal::TransientSettings t;
            const auto& tj = def_.settings["transient"];
            t.duration = tj.value("duration", 1.0);
            t.steps = tj.value("steps", 100);
            t.snapshots = tj.value("snapshots", 10);
            s = thermal::solveTransient(*problem_, t, context.cancelled);
        } else
            s = thermal::solveSteady(*problem_);
        if (!s)
            return Status::failure(s.message());
        solution_ = s.take();
        solvedMaterial_ = preparedMaterial_;
        solvedGeometry_ = preparedGeometry_;
        return Status::success();
    }

    std::optional<FieldData> provide(const std::string& quantity, const SystemState&) const override
    {
        if (!solution_ || (quantity != "temperature" && quantity != "heat_source_density"))
            return std::nullopt;
        FieldData f;
        f.quantity = *multiphysics::findQuantity(quantity);
        f.support = support(SystemState{});
        f.values = quantity == "temperature" ? solution_->temperature : problem_->source;
        f.producer = id();
        f.solver = solverId() + " " + solverVersion();
        f.materialRevision = solvedMaterial_;
        f.geometryRevision = solvedGeometry_;
        if (transient()) {
            f.time = multiphysics::TimeCharacter::Instant;
            f.timeSeconds = solution_->duration;
        }
        return f;
    }

    GridSupport support(const SystemState&) const override { return coupling::support(reference_); }

    Status accept(const FieldData& field) override
    {
        if (field.quantity.id != "heat_source_density")
            return Status::failure("the conduction model takes a heat_source_density, not " + field.quantity.id);
        if (!field.support.sameAs(support(SystemState{})))
            return Status::failure("the heat source is not on the conduction grid");
        source_ = field.values;
        ownSource_ = false;
        return Status::success();
    }

    void account(EnergyLedger& ledger, const std::set<std::string>&) const override
    {
        if (!solution_)
            return;
        static const char* faceNames[] = {"-x", "+x", "-y", "+y", "-z", "+z"};
        for (size_t f = 0; f < 6; ++f) {
            const auto& cond = problem_->faces[f];
            if (cond.kind == thermal::FaceKind::Adiabatic)
                continue;
            const std::string how = cond.kind == thermal::FaceKind::Convective ? "convection" : "fixed temperature";
            ledger.add(LedgerTerm{id(), domain(), TermKind::BoundaryOut, how + " at " + faceNames[f], solution_->faceHeatOut[f], "",
                                  "heat leaving through the face (negative: entering)"});
        }
        if (ownSource_)
            ledger.add(LedgerTerm{id(), domain(), TermKind::Input, "prescribed volumetric heat source", solution_->sourcePower, "", ""});
        if (transient())
            ledger.add(LedgerTerm{id(), domain(), TermKind::Stored, "internal energy", solution_->storedEnergyChange, "", "rho c_p (T_end - T_start) V"});
    }

    double solverResidual() const override { return solution_ ? solution_->balanceResidual : 0.0; }

    nlohmann::json monitor() const override
    {
        if (!solution_)
            return {};
        double mean = 0.0;
        double volume = 0.0;
        for (size_t c = 0; c < solution_->temperature.size(); ++c) {
            const double w = reference_.edges[c + 1] - reference_.edges[c];
            mean += solution_->temperature[c] * w;
            volume += w;
        }
        return {{"maxTemperatureK", solution_->maxTemperature()}, {"minTemperatureK", solution_->minTemperature()},
                {"meanTemperatureK", volume > 0 ? mean / volume : 0.0}, {"heatOut", solution_->heatOut()}};
    }

    nlohmann::json summary() const override
    {
        nlohmann::json j = monitor();
        j["transient"] = transient();
        if (solution_) {
            j["method"] = solution_->method;
            j["balanceResidual"] = solution_->balanceResidual;
            if (transient())
                j["durationS"] = solution_->duration;
        }
        j["cells"] = reference_.region.size();
        return j;
    }

    std::vector<std::string> resultQuantities() const override { return {"temperature", "heat_source_density"}; }
    std::uint64_t solvedGeometryRevision() const override { return solvedGeometry_; }
    std::uint64_t solvedMaterialRevision() const override { return solvedMaterial_; }
    // The thermal solution lives in the reference configuration: it does not follow geometry updates.
    // It is re-solved every pass anyway, so its geometry revision is the latest at that time.

private:
    PhysicalSystem system_;
    DomainDef def_;
    MaterialLibrary materials_;
    Column reference_;
    std::optional<thermal::ConductionProblem> problem_;
    std::optional<thermal::ConductionSolution> solution_;
    std::vector<double> source_;
    bool ownSource_ = false;
    std::uint64_t preparedMaterial_ = kNever, preparedGeometry_ = kNever, solvedMaterial_ = kNever, solvedGeometry_ = kNever;
};

// --- mechanics: a column of hexahedra with thermal loads ------------------------------------

class SolidColumnParticipant final : public Participant {
public:
    SolidColumnParticipant(PhysicalSystem system, DomainDef def, MaterialLibrary materials)
        : system_(std::move(system)), def_(std::move(def)), materials_(std::move(materials))
    {
    }
    std::string id() const override { return def_.id; }
    std::string domain() const override { return "mechanical"; }
    std::string solverId() const override { return "mechanics.fe.solid-column"; }
    std::string solverVersion() const override { return "1.0"; }

    Status prepare(SystemState& state) override
    {
        reference_ = column(system_, state, false);
        const std::string lateral = def_.settings.value("lateral", "free");
        if (lateral != "free" && lateral != "constrained")
            return Status::failure("the lateral condition must be free or constrained");
        constrained_ = lateral == "constrained";
        mechanics::StructuralModel m;
        m.kind = mechanics::ElementKind::Solid;
        const double h = reference_.side / 2;
        const size_t levels = reference_.edges.size();
        for (size_t k = 0; k < levels; ++k) {
            const double z = reference_.edges[k];
            m.nodes.emplace_back(-h, -h, z);
            m.nodes.emplace_back(h, -h, z);
            m.nodes.emplace_back(h, h, z);
            m.nodes.emplace_back(-h, h, z);
        }
        m.fixed.assign(m.nodes.size(), 0);
        expansion_.clear();
        for (size_t e = 0; e + 1 < levels; ++e) {
            const auto& region = system_.regions[reference_.region[e]];
            const Material* mat = findMaterial(materials_, region.materialId);
            if (!mat)
                return Status::failure("the material '" + region.materialId + "' is not in the library");
            auto elastic = mechanics::elasticMaterialOf(*mat);
            if (!elastic)
                return Status::failure(elastic.message());
            const PropertyValue* alpha = mat->property(PhysicsDomain::Thermal, "thermalExpansion");
            if (!alpha)
                return Status::failure("the material '" + mat->name() + "' of region '" + region.id + "' has no thermal expansion coefficient");
            const Vec3 at(0, 0, 0.5 * (reference_.edges[e] + reference_.edges[e + 1]));
            mechanics::ElasticMaterial em = elastic.take();
            em.youngsModulus = state.materials.property(region.id, "mechanical.youngsModulus", at, em.youngsModulus);
            em.poissonsRatio = state.materials.property(region.id, "mechanical.poissonsRatio", at, em.poissonsRatio);
            m.materials.push_back(em);
            expansion_.push_back(state.materials.property(region.id, "thermal.thermalExpansion", at, alpha->value));
            mechanics::Element el;
            for (int q = 0; q < 4; ++q) {
                el.nodes[static_cast<size_t>(q)] = static_cast<int>(4 * e + static_cast<size_t>(q));
                el.nodes[static_cast<size_t>(q) + 4] = static_cast<int>(4 * (e + 1) + static_cast<size_t>(q));
            }
            el.material = static_cast<int>(e);
            m.elements.push_back(el);
        }
        for (size_t n = 0; n < m.nodes.size(); ++n) {
            const size_t q = n % 4;
            if (constrained_) {
                m.fix(n, 0);
                m.fix(n, 1);
            } else {
                if (q == 0 || q == 3)
                    m.fix(n, 0);  // the -x face: symmetry
                if (q == 0 || q == 1)
                    m.fix(n, 1);  // the -y face: symmetry
            }
            if (n < 4)
                m.fix(n, 2);  // supported at z = 0
        }
        if (Status s = m.validate(); !s)
            return s;
        model_ = std::move(m);
        preparedMaterial_ = state.materials.revision();
        if (temperature_.size() != model_->elements.size())
            temperature_.assign(model_->elements.size(), system_.ambientTemperature);
        return Status::success();
    }

    Status solve(SystemState& state, const RunContext&) override
    {
        mechanics::ThermalLoadInput input;
        input.elementTemperature = temperature_;
        input.referenceTemperature = system_.ambientTemperature;
        input.expansion = expansion_;
        auto load = mechanics::thermalLoadVector(*model_, input);
        if (!load)
            return Status::failure(load.message());
        const mechanics::SystemMatrices system = mechanics::assemble(*model_);
        mechanics::ForceInput force;
        force.label = "thermal load";
        force.pattern = load.value();
        force.amplitude = 1.0;
        auto solved = mechanics::solveStatic(*model_, system, {force});
        if (!solved)
            return Status::failure(solved.message());
        displacement_ = solved.value().displacement;
        auto stress = mechanics::thermalStress(*model_, input, displacement_);
        if (!stress)
            return Status::failure(stress.message());
        stress_ = stress.take();
        strainEnergy_ = 0.0;
        for (size_t e = 0; e < model_->elements.size(); ++e) {
            Eigen::Matrix<double, 6, 1> s;
            for (int c = 0; c < 6; ++c)
                s(c) = stress_.stress[e][static_cast<size_t>(c)];
            const Eigen::Matrix<double, 6, 6> compliance = mechanics::solidElasticity(model_->materials[e]).inverse();
            const double volume = reference_.side * reference_.side * (reference_.edges[e + 1] - reference_.edges[e]);
            strainEnergy_ += 0.5 * s.dot(compliance * s) * volume;
        }
        solvedMaterial_ = preparedMaterial_;
        solvedGeometry_ = state.geometry.revision();
        return Status::success();
    }

    double levelDisplacementZ(size_t level) const
    {
        double sum = 0.0;
        for (size_t q = 0; q < 4; ++q)
            sum += displacement_(static_cast<Eigen::Index>(model_->dof(4 * level + q, 2)));
        return sum / 4.0;
    }

    std::optional<FieldData> provide(const std::string& quantity, const SystemState&) const override
    {
        if (displacement_.size() == 0 || (quantity != "displacement" && quantity != "stress"))
            return std::nullopt;
        FieldData f;
        f.quantity = *multiphysics::findQuantity(quantity);
        f.support = coupling::support(reference_);
        f.producer = id();
        f.solver = solverId() + " " + solverVersion();
        f.materialRevision = solvedMaterial_;
        f.geometryRevision = solvedGeometry_;
        const size_t ne = model_->elements.size();
        if (quantity == "displacement") {
            f.values.resize(ne * 3);
            for (size_t e = 0; e < ne; ++e)
                for (int d = 0; d < 3; ++d) {
                    double sum = 0.0;
                    for (size_t q = 0; q < 8; ++q)
                        sum += displacement_(static_cast<Eigen::Index>(model_->dof(static_cast<size_t>(model_->elements[e].nodes[q]), d)));
                    f.values[e * 3 + static_cast<size_t>(d)] = sum / 8.0;
                }
        } else {
            f.values.resize(ne * 6);
            for (size_t e = 0; e < ne; ++e)
                for (int c = 0; c < 6; ++c)
                    f.values[e * 6 + static_cast<size_t>(c)] = stress_.stress[e][static_cast<size_t>(c)];
        }
        return f;
    }

    GridSupport support(const SystemState&) const override { return coupling::support(reference_); }

    Status accept(const FieldData& field) override
    {
        if (field.quantity.id != "temperature")
            return Status::failure("the column model takes a temperature, not " + field.quantity.id);
        if (field.values.size() != model_->elements.size())
            return Status::failure("the temperature is not on the column's elements");
        temperature_ = field.values;
        return Status::success();
    }

    std::vector<GeometryChange> geometryChanges(const SystemState& state) const override
    {
        std::vector<GeometryChange> out;
        size_t level = 0;
        for (const auto& r : system_.regions) {
            const double bottom = levelDisplacementZ(level);
            level += static_cast<size_t>(r.cells);
            const double top = levelDisplacementZ(level);
            out.push_back({r.id, "thickness", state.geometry.reference(r.id, "thickness") + top - bottom});
        }
        return out;
    }

    void account(EnergyLedger& ledger, const std::set<std::string>&) const override
    {
        ledger.add(LedgerTerm{id(), domain(), TermKind::Informational, "elastic strain energy", strainEnergy_, "",
                              "quasi-static linear thermoelasticity: J, not drawn from the modeled power flow"});
    }

    nlohmann::json monitor() const override
    {
        if (displacement_.size() == 0)
            return {};
        return {{"topDisplacementZM", levelDisplacementZ(reference_.edges.size() - 1)}, {"maxVonMisesPa", stress_.maxVonMises}};
    }

    nlohmann::json summary() const override
    {
        nlohmann::json j = monitor();
        j["lateral"] = constrained_ ? "constrained (uniaxial strain)" : "free (symmetry constraints, uniaxial stress)";
        j["elements"] = model_ ? model_->elements.size() : 0;
        j["strainEnergyJ"] = strainEnergy_;
        if (displacement_.size() > 0 && !stress_.stress.empty()) {
            const size_t mid = stress_.stress.size() / 2;
            j["centreStressPa"] = stress_.stress[mid];
        }
        j["method"] = "trilinear hexahedra with thermal loads f = int B^T D alpha dT dV, stress D (B u - eps_th), reference configuration";
        return j;
    }

    std::vector<std::string> resultQuantities() const override { return {"displacement", "stress"}; }
    std::uint64_t solvedGeometryRevision() const override { return solvedGeometry_; }
    std::uint64_t solvedMaterialRevision() const override { return solvedMaterial_; }

private:
    PhysicalSystem system_;
    DomainDef def_;
    MaterialLibrary materials_;
    Column reference_;
    std::optional<mechanics::StructuralModel> model_;
    std::vector<double> expansion_, temperature_;
    Eigen::VectorXd displacement_;
    mechanics::ThermalStressResult stress_;
    double strainEnergy_ = 0.0;
    bool constrained_ = false;
    std::uint64_t preparedMaterial_ = kNever, solvedMaterial_ = kNever, solvedGeometry_ = kNever;
};

}  // namespace

Result<std::unique_ptr<Participant>> makeParticipant(const PhysicalSystem& system, const DomainDef& domain, const MaterialLibrary& materials)
{
    using R = Result<std::unique_ptr<Participant>>;
    if (domain.kind == "electromagnetic" && domain.solver == em::kLayeredSolverId)
        return R(std::make_unique<LayeredEmParticipant>(system, domain, materials));
    if (domain.kind == "thermal" && domain.solver == thermal::kSolverId)
        return R(std::make_unique<ThermalColumnParticipant>(system, domain, materials));
    if (domain.kind == "mechanical" && domain.solver == "mechanics.fe.solid-column")
        return R(std::make_unique<SolidColumnParticipant>(system, domain, materials));
    if (auto cavity = makeCavityParticipant(system, domain, materials))
        return R(std::move(cavity));
    return R::failure("no participant for domain kind '" + domain.kind + "' with solver '" + domain.solver + "'");
}

}  // namespace resonant::coupling
