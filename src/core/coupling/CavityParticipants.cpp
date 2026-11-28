#include "coupling/CavityParticipants.h"

#include "electromagnetics/CavityResonator.h"
#include "electromagnetics/Conventions.h"
#include "materials/Material.h"
#include "multiphysics/Quantity.h"

#include <cmath>
#include <limits>

namespace resonant::coupling {

namespace {

using multiphysics::DomainDef;
using multiphysics::EnergyLedger;
using multiphysics::FieldData;
using multiphysics::GeometryChange;
using multiphysics::GridSupport;
using multiphysics::LedgerTerm;
using multiphysics::PhysicalSystem;
using multiphysics::RunContext;
using multiphysics::SystemState;
using multiphysics::TermKind;

const multiphysics::RegionDef* cavityRegion(const PhysicalSystem& s)
{
    for (const auto& r : s.regions)
        if (r.dimensions.count("a") && r.dimensions.count("b") && r.dimensions.count("d"))
            return &r;
    return nullptr;
}

const multiphysics::RegionDef* wallsRegion(const PhysicalSystem& s)
{
    for (const auto& r : s.regions)
        if (r.id == "walls")
            return &r;
    return nullptr;
}

double intrinsic(const Material* m, PhysicsDomain d, const char* key, double fallback)
{
    if (!m)
        return fallback;
    const PropertyValue* v = m->property(d, key);
    return v ? v->value : fallback;
}

GridSupport box(const PhysicalSystem& s)
{
    const auto* r = cavityRegion(s);
    GridSupport g;
    g.edges = {std::vector<double>{0.0, r->dimensions.at("a")}, std::vector<double>{0.0, r->dimensions.at("b")},
               std::vector<double>{0.0, r->dimensions.at("d")}};
    return g;
}

Vec3 centre(const PhysicalSystem& s)
{
    const auto* r = cavityRegion(s);
    return Vec3(0.5 * r->dimensions.at("a"), 0.5 * r->dimensions.at("b"), 0.5 * r->dimensions.at("d"));
}

class CavityModeParticipant final : public multiphysics::Participant {
public:
    CavityModeParticipant(const PhysicalSystem& s, const DomainDef& d, const MaterialLibrary& m) : system_(s), domain_(d), materials_(m) {}
    std::string id() const override { return domain_.id; }
    std::string domain() const override { return "electromagnetic"; }
    std::string solverId() const override { return kCavityModeSolver; }
    std::string solverVersion() const override { return em::kCavityModelVersion; }

    Status prepare(SystemState& state) override
    {
        const auto* region = cavityRegion(system_);
        const auto* walls = wallsRegion(system_);
        if (!region || !walls)
            return Status::failure("a rectangular-cavity system needs a region with dimensions a, b, d (the fill) and a region 'walls'");
        const Material* fill = materials_.find(Id(region->materialId));
        const Material* wall = materials_.find(Id(walls->materialId));
        if (!fill || !wall)
            return Status::failure("the cavity's fill or wall material is not in the library");
        em::CavityResonatorSpec spec;
        spec.a = state.geometry.current(region->id, "a");
        spec.b = state.geometry.current(region->id, "b");
        spec.d = state.geometry.current(region->id, "d");
        const Vec3 at = centre(system_);
        spec.fillPermittivity = std::max(1.0, state.materials.property(region->id, "electromagnetic.relativePermittivity", at,
                                                                         intrinsic(fill, PhysicsDomain::Electromagnetic, "relativePermittivity", 1.0)));
        spec.fillLossTangent = state.materials.property(region->id, "electromagnetic.lossTangent", at,
                                                        intrinsic(fill, PhysicsDomain::Electromagnetic, "lossTangent", 0.0));
        spec.fillConductivity = intrinsic(fill, PhysicsDomain::Electromagnetic, "conductivity", 0.0);
        spec.fillPermeability = std::max(1.0, intrinsic(fill, PhysicsDomain::Electromagnetic, "relativePermeability", 1.0));
        spec.wallConductivity = intrinsic(wall, PhysicsDomain::Electromagnetic, "conductivity", 0.0);
        // The port: its external Q is a property of the coupling structure, fixed at the reference state.
        if (externalQ_ <= 0.0) {
            externalQ_ = domain_.settings.value("externalQ", 0.0);
            if (externalQ_ <= 0.0) {
                const double beta = domain_.settings.value("coupling", 1.0);
                auto ref = em::analyzeRectangularCavity(spec, 1);
                if (!ref)
                    return Status::failure(ref.message());
                externalQ_ = ref.value()[0].unloadedQ / beta;
            }
        }
        spec.externalQ = externalQ_;
        auto modes = em::analyzeRectangularCavity(spec, 1);
        if (!modes)
            return Status::failure(modes.message());
        mode_ = modes.value()[0];
        spec_ = spec;
        preparedGeometry_ = state.geometry.revision();
        preparedMaterial_ = state.materials.revision();
        return Status::success();
    }

    Status solve(SystemState&, const RunContext&) override
    {
        const double fd = domain_.settings.value("driveFrequencyHz", mode_.frequencyHz);
        incident_ = domain_.settings.value("incidentPowerW", 1.0);
        if (!(fd > 0.0) || !(incident_ >= 0.0))
            return Status::failure("the drive needs a positive frequency and a non-negative power");
        driveHz_ = fd;
        gamma2_ = std::norm(em::portReflection(mode_, fd));
        delivered_ = incident_ * (1.0 - gamma2_);
        solved_ = true;
        solvedGeometry_ = preparedGeometry_;
        solvedMaterial_ = preparedMaterial_;
        return Status::success();
    }

    std::optional<FieldData> provide(const std::string& quantity, const SystemState&) const override
    {
        if (quantity != "absorbed_power_density" || !solved_)
            return std::nullopt;
        FieldData f;
        f.quantity = *multiphysics::findQuantity("absorbed_power_density");
        f.support = box(system_);
        f.time = multiphysics::TimeCharacter::Harmonic;
        f.frequencyHz = driveHz_;
        f.producer = id();
        f.solver = solverId() + " " + solverVersion();
        f.geometryRevision = solvedGeometry_;
        f.materialRevision = solvedMaterial_;
        // All the delivered power is lost in the walls and the fill; per reference volume of the cavity.
        f.values = {delivered_ / f.support.volume()};
        return f;
    }
    GridSupport support(const SystemState&) const override { return box(system_); }
    Status accept(const FieldData& field) override { return Status::failure("the cavity mode takes no field inputs (" + field.quantity.id + ")"); }
    void account(EnergyLedger& ledger, const std::set<std::string>& transferred) const override
    {
        if (!solved_)
            return;
        const double scale = ledger.powerScale();
        ledger.add(LedgerTerm{id(), domain(), TermKind::Input, "incident power at the port", incident_ * scale, "", ""});
        ledger.add(LedgerTerm{id(), domain(), TermKind::BoundaryOut, "reflected at the port", incident_ * gamma2_ * scale, "", ""});
        if (!transferred.contains("absorbed_power_density"))
            ledger.add(LedgerTerm{id(), domain(), TermKind::Dissipated, "wall and fill loss", delivered_ * scale, "", "not passed to another domain"});
    }
    nlohmann::json monitor() const override
    {
        if (!solved_)
            return {};
        return {{"resonanceHz", mode_.frequencyHz}, {"loadedQ", mode_.loadedQ},     {"unloadedQ", mode_.unloadedQ},
                {"detuningHz", driveHz_ - mode_.frequencyHz}, {"deliveredW", delivered_}, {"reflectance", gamma2_},
                {"fillPermittivity", spec_.fillPermittivity}, {"a", spec_.a}};
    }
    nlohmann::json summary() const override
    {
        nlohmann::json j = monitor();
        j["solver"] = solverId();
        j["mode"] = mode_.label;
        j["externalQ"] = externalQ_;
        return j;
    }
    std::uint64_t solvedGeometryRevision() const override { return solvedGeometry_; }
    std::uint64_t solvedMaterialRevision() const override { return solvedMaterial_; }

private:
    const PhysicalSystem& system_;
    DomainDef domain_;
    const MaterialLibrary& materials_;
    em::CavityResonatorSpec spec_;
    em::CavityResonance mode_;
    double externalQ_ = 0.0, incident_ = 0.0, driveHz_ = 0.0, gamma2_ = 0.0, delivered_ = 0.0;
    bool solved_ = false;
    std::uint64_t preparedGeometry_ = 0, preparedMaterial_ = 0, solvedGeometry_ = 0, solvedMaterial_ = 0;
};

class LumpedThermalParticipant final : public multiphysics::Participant {
public:
    LumpedThermalParticipant(const PhysicalSystem& s, const DomainDef& d, const MaterialLibrary& m) : system_(s), domain_(d), materials_(m) {}
    std::string id() const override { return domain_.id; }
    std::string domain() const override { return "thermal"; }
    std::string solverId() const override { return kLumpedThermalSolver; }
    std::string solverVersion() const override { return "1"; }

    Status prepare(SystemState& state) override
    {
        conductance_ = domain_.settings.value("conductanceWPerK", 0.0);
        if (!(conductance_ > 0.0))
            return Status::failure("the lumped thermal model needs a conductance to the ambient (settings.conductanceWPerK > 0)");
        const auto* walls = wallsRegion(system_);
        const Material* wall = walls ? materials_.find(Id(walls->materialId)) : nullptr;
        expansion_ = intrinsic(wall, PhysicsDomain::Thermal, "thermalExpansion", 0.0);
        preparedGeometry_ = state.geometry.revision();
        preparedMaterial_ = state.materials.revision();
        return Status::success();
    }
    Status solve(SystemState&, const RunContext&) override
    {
        temperature_ = system_.ambientTemperature + power_ / conductance_;
        solved_ = true;
        solvedGeometry_ = preparedGeometry_;
        solvedMaterial_ = preparedMaterial_;
        return Status::success();
    }
    std::optional<FieldData> provide(const std::string& quantity, const SystemState&) const override
    {
        if (quantity != "temperature" || !solved_)
            return std::nullopt;
        FieldData f;
        f.quantity = *multiphysics::findQuantity("temperature");
        f.support = box(system_);
        f.time = multiphysics::TimeCharacter::Steady;
        f.producer = id();
        f.solver = solverId() + " " + solverVersion();
        f.geometryRevision = solvedGeometry_;
        f.materialRevision = solvedMaterial_;
        f.values = {temperature_};
        return f;
    }
    GridSupport support(const SystemState&) const override { return box(system_); }
    Status accept(const FieldData& field) override
    {
        if (field.quantity.id != "heat_source_density")
            return Status::failure("the lumped thermal model takes heat_source_density, not " + field.quantity.id);
        power_ = field.integral();
        return Status::success();
    }
    std::vector<GeometryChange> geometryChanges(const SystemState& state) const override
    {
        std::vector<GeometryChange> out;
        const auto* region = cavityRegion(system_);
        if (!region || !solved_)
            return out;
        const double scale = 1.0 + expansion_ * (temperature_ - system_.ambientTemperature);
        for (const char* dim : {"a", "b", "d"})
            out.push_back({region->id, dim, state.geometry.reference(region->id, dim) * scale});
        return out;
    }
    void account(EnergyLedger& ledger, const std::set<std::string>&) const override
    {
        if (!solved_)
            return;
        // Steady: the heat received leaves to the ambient through the conductance.
        ledger.add(LedgerTerm{id(), domain(), TermKind::BoundaryOut, "conduction and convection to the ambient",
                              conductance_ * (temperature_ - system_.ambientTemperature) * ledger.powerScale(), "", ""});
    }
    nlohmann::json monitor() const override
    {
        if (!solved_)
            return {};
        return {{"temperatureK", temperature_}, {"heatW", power_}};
    }
    nlohmann::json summary() const override
    {
        nlohmann::json j = monitor();
        j["solver"] = solverId();
        j["conductanceWPerK"] = conductance_;
        j["wallExpansionPerK"] = expansion_;
        return j;
    }
    std::uint64_t solvedGeometryRevision() const override { return solvedGeometry_; }
    std::uint64_t solvedMaterialRevision() const override { return solvedMaterial_; }

private:
    const PhysicalSystem& system_;
    DomainDef domain_;
    const MaterialLibrary& materials_;
    double conductance_ = 0.0, expansion_ = 0.0, power_ = 0.0, temperature_ = 0.0;
    bool solved_ = false;
    std::uint64_t preparedGeometry_ = 0, preparedMaterial_ = 0, solvedGeometry_ = 0, solvedMaterial_ = 0;
};

}  // namespace

std::unique_ptr<multiphysics::Participant> makeCavityParticipant(const PhysicalSystem& system, const DomainDef& domain, const MaterialLibrary& materials)
{
    if (domain.kind == "electromagnetic" && domain.solver == kCavityModeSolver)
        return std::make_unique<CavityModeParticipant>(system, domain, materials);
    if (domain.kind == "thermal" && domain.solver == kLumpedThermalSolver)
        return std::make_unique<LumpedThermalParticipant>(system, domain, materials);
    return nullptr;
}

}  // namespace resonant::coupling
