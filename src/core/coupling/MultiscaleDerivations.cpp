#include "coupling/MultiscaleDerivations.h"

#include "atomistic/LatticeStatics.h"
#include "atomistic/MolecularDynamics.h"
#include "electromagnetics/EmMaterial.h"
#include "foundation/Time.h"
#include "foundation/Version.h"
#include "multiscale/Microstructure.h"
#include "multiscale/PotentialHomogenization.h"
#include "project/Project.h"
#include "simulation/Job.h"
#include "thermal/ThermalMaterial.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>

namespace resonant::coupling {

using nlohmann::json;
namespace ms = multiscale;
using R = Result<json>;

namespace {

constexpr double kElectronVolt = 1.602176634e-19;

// --- RVE phases --------------------------------------------------------------------------------

struct PhaseModel {
    bool uniaxial = false;
    std::string material, axial, transverse;  // isotropic: material; uniaxial: along / across the grain's axis
};

Result<std::vector<PhaseModel>> phaseModels(const ms::Representation& r)
{
    using PR = Result<std::vector<PhaseModel>>;
    const json rve = r.data.value("rve", json::object());
    const auto phases = rve.value("phases", std::vector<std::string>{});
    const json models = r.data.value("phaseMaterials", json::array());
    if (!models.is_array() || models.size() != phases.size())
        return PR::failure("the RVE needs one entry of \"phaseMaterials\" per phase (" + std::to_string(phases.size()) + ")");
    std::vector<PhaseModel> out;
    for (const json& m : models) {
        PhaseModel p;
        if (m.is_string()) {
            p.material = m.get<std::string>();
        } else if (m.is_object() && m.contains("axial") && m.contains("transverse")) {
            p.uniaxial = true;
            p.axial = m["axial"].get<std::string>();
            p.transverse = m["transverse"].get<std::string>();
        } else {
            return PR::failure("a phase material is a material id or {\"axial\": id, \"transverse\": id}");
        }
        for (const std::string& id : {p.material, p.axial, p.transverse})
            if (!id.empty() && std::find(r.constituents.begin(), r.constituents.end(), id) == r.constituents.end())
                return PR::failure("the phase material '" + id + "' is not among the representation's constituents");
        out.push_back(std::move(p));
    }
    return out;
}

// A constituent's scalar property at the state: the library value, then the representation's linear law.
Result<double> constituentValue(const ms::Representation& r, const Project& project, const std::string& materialId, const std::string& property,
                                const std::optional<double>& temperature, double frequencyHz)
{
    using DR = Result<double>;
    const Material* m = project.materials.find(Id(materialId));
    if (!m)
        return DR::failure("the constituent '" + materialId + "' is not in the library");
    double v = 0.0;
    if (property == "thermal.thermalConductivity" || property == "thermal.density" || property == "thermal.specificHeat") {
        auto t = thermal::thermalMaterial(*m);
        if (!t)
            return DR::failure("'" + m->name() + "': " + t.message());
        v = property == "thermal.thermalConductivity" ? t.value().conductivity : property == "thermal.density" ? t.value().density : t.value().specificHeat;
    } else if (property == "electromagnetic.conductivity" || property == "electromagnetic.relativePermittivity") {
        auto e = em::emMaterialOf(*m, frequencyHz);
        if (!e)
            return DR::failure("'" + m->name() + "': " + e.message());
        v = property == "electromagnetic.conductivity" ? e.value().conductivity : e.value().relativePermittivity;
    } else {
        return DR::failure("no constituent property '" + property + "'");
    }
    if (temperature)
        for (const json& law : r.data.value("temperatureLaws", json::array()))
            if (law.value("material", "") == materialId && law.value("property", "") == property)
                v += law.value("slope", 0.0) * (*temperature - law.value("reference", 293.15));
    return v;
}

struct RveRun {
    Eigen::Matrix3d K = Eigen::Matrix3d::Zero();
    double hillMandel = 0.0, residual = 0.0;
    int iterations = 0, voxels = 0;
    std::vector<double> fractions;
};

Result<RveRun> runRve(const ms::Representation& r, const Project& project, const json& spec, const std::string& property, ms::Boundary boundary,
                      const std::optional<double>& temperature, double frequencyHz, JobContext* ctx)
{
    using RR = Result<RveRun>;
    auto rve = ms::generateRve(spec);
    if (!rve)
        return RR::failure(rve.message());
    auto models = phaseModels(r);
    if (!models)
        return RR::failure(models.message());
    std::vector<Eigen::Matrix3d> iso(models.value().size());
    std::vector<std::pair<double, double>> uni(models.value().size());
    for (size_t p = 0; p < models.value().size(); ++p) {
        const PhaseModel& m = models.value()[p];
        if (!m.uniaxial) {
            auto k = constituentValue(r, project, m.material, property, temperature, frequencyHz);
            if (!k)
                return RR::failure(k.message());
            if (!(k.value() > 0.0))
                return RR::failure("the phase '" + rve.value().phases[p] + "' has no positive " + property + " (" + std::to_string(k.value()) + ")");
            iso[p] = Eigen::Matrix3d::Identity() * k.value();
        } else {
            auto a = constituentValue(r, project, m.axial, property, temperature, frequencyHz);
            auto t = constituentValue(r, project, m.transverse, property, temperature, frequencyHz);
            if (!a || !t)
                return RR::failure(a ? t.message() : a.message());
            if (!(a.value() > 0.0) || !(t.value() > 0.0))
                return RR::failure("the oriented phase '" + rve.value().phases[p] + "' needs positive values");
            uni[p] = {a.value(), t.value()};
        }
    }
    ms::PotentialProblem pp;
    pp.n = rve.value().n;
    pp.size = rve.value().size;
    pp.periodic = rve.value().periodic;
    pp.K.reserve(rve.value().count());
    for (size_t v = 0; v < rve.value().count(); ++v) {
        const size_t p = rve.value().phase[v];
        if (!models.value()[p].uniaxial) {
            pp.K.push_back(iso[p]);
            continue;
        }
        const int g = rve.value().grain[v];
        if (g < 0)
            return RR::failure("an oriented phase needs grains with axes (a Voronoi RVE or imported grains)");
        const auto& ax = rve.value().axes[static_cast<size_t>(g)];
        const Eigen::Vector3d c(ax[0], ax[1], ax[2]);
        pp.K.push_back(uni[p].second * (Eigen::Matrix3d::Identity() - c * c.transpose()) + uni[p].first * c * c.transpose());
    }
    auto h = ms::homogenize(pp, boundary, 1e-10, [ctx] { return ctx && ctx->cancelRequested(); });
    if (!h)
        return RR::failure(h.message());
    RveRun out;
    out.K = h.value().effective;
    out.hillMandel = h.value().hillMandel;
    out.residual = h.value().residual;
    out.iterations = h.value().iterations;
    out.voxels = static_cast<int>(rve.value().count());
    out.fractions = rve.value().volumeFractions();
    return out;
}

json matrixJson(const Eigen::MatrixXd& m)
{
    json j = json::array();
    for (int i = 0; i < m.rows(); ++i) {
        json row = json::array();
        for (int k = 0; k < m.cols(); ++k)
            row.push_back(m(i, k));
        j.push_back(row);
    }
    return j;
}

std::string unitOf(const std::string& domain, const std::string& key)
{
    const auto d = physicsDomainFromString(domain);
    const PropertyDef* def = d ? findPropertyDefinition(*d, key) : nullptr;
    return def ? def->unit : std::string();
}

json propertyJson(const std::string& domain, const std::string& key, const std::string& component, double value, std::optional<double> u = {})
{
    json p{{"domain", domain}, {"key", key}, {"component", component}, {"value", value}, {"unit", unitOf(domain, key)}};
    if (u)
        p["uncertainty"] = *u;
    return p;
}

json base(const ms::Representation& r, const Project& project, const DerivationMethod& m)
{
    json keys = json::object();
    for (const std::string& c : r.constituents)
        if (const Material* mat = project.materials.find(Id(c)))
            keys[c] = ms::materialContentKey(*mat);
    return {{"format", kDerivationFormat},
            {"version", 1},
            {"kind", "derivation"},
            {"method", m.id},
            {"modelCard", m.modelCard},
            {"representation", {{"id", r.id}, {"name", r.name}, {"scale", std::string(ms::toString(r.scale))}, {"kind", r.kind}, {"revision", r.revision()}}},
            {"sourceScale", std::string(ms::toString(m.source))},
            {"targetScale", std::string(ms::toString(m.target))},
            {"constituentKeys", keys},
            {"properties", json::array()},
            {"warnings", json::array()},
            {"reliability", json::array()},
            {"created", utcNowIso8601()},
            {"application", std::string("Resonant ") + kApplicationVersion}};
}

// --- RVE derivations ---------------------------------------------------------------------------

R deriveRve(const ms::Representation& r, const Project& project, const DerivationMethod& m, const json& s, JobContext* ctx)
{
    const std::string property = m.id == "rve.thermal-conductivity"      ? "thermal.thermalConductivity"
                                 : m.id == "rve.electrical-conductivity" ? "electromagnetic.conductivity"
                                                                         : "electromagnetic.relativePermittivity";
    const json spec = r.data.value("rve", json::object());
    const bool periodicStructure = spec.value("generator", "") != "voxels" || spec.value("periodic", false);
    const std::string boundaryName = s.value("boundary", periodicStructure ? "periodic" : "linear");
    ms::Boundary boundary = ms::Boundary::Periodic;
    if (boundaryName == "linear")
        boundary = ms::Boundary::Linear;
    else if (boundaryName == "mixed")
        boundary = ms::Boundary::Mixed;
    else if (boundaryName == "flux")
        boundary = ms::Boundary::Flux;
    else if (boundaryName != "periodic")
        return R::failure("boundary must be periodic, linear, mixed or flux");
    std::optional<double> temperature;
    if (s.contains("temperatureK") && s["temperatureK"].is_number())
        temperature = s["temperatureK"].get<double>();
    const double frequency = s.value("frequencyHz", 0.0);
    const bool random = spec.value("generator", "") == "spheres" || spec.value("generator", "") == "voronoi";
    const int realizations = random ? std::clamp(s.value("realizations", 1), 1, 50) : 1;
    std::vector<int> sizes = s.value("sizes", std::vector<int>{});
    const auto base_n = spec.value("voxels", std::vector<int>{});
    if (!sizes.empty() && spec.value("generator", "") == "voxels")
        return R::failure("a size study needs a generated RVE, not imported voxels");
    if (sizes.empty() && base_n.size() == 3)
        sizes = {base_n[0]};
    const double tolerance = s.value("tolerance", 0.02);
    json d = base(r, project, m);
    if (temperature)
        d["conditions"] = {{"temperatureK", *temperature}};
    json study = json::array();
    std::vector<Eigen::Matrix3d> last;
    RveRun lastRun;
    int step = 0;
    const int total = static_cast<int>(sizes.size()) * realizations;
    for (int n : sizes) {
        if (base_n.size() != 3 || n < 1)
            return R::failure("the RVE's voxels are not set");
        json sized = spec;
        const double scale = static_cast<double>(n) / base_n[0];
        sized["voxels"] = {n, std::max(1, static_cast<int>(std::lround(base_n[1] * scale))), std::max(1, static_cast<int>(std::lround(base_n[2] * scale)))};
        const auto size = spec.value("size", std::vector<double>{1, 1, 1});
        sized["size"] = {size[0] * scale, size[1] * scale, size[2] * scale};
        if (spec.value("generator", "") == "voronoi")
            sized["grains"] = std::max(1, static_cast<int>(std::lround(spec.value("grains", 8) * scale * scale * scale)));
        std::vector<Eigen::Matrix3d> Ks;
        for (int k = 0; k < realizations; ++k) {
            if (ctx && ctx->cancelRequested())
                return R::failure("cancelled");
            json one = sized;
            one["seed"] = spec.value("seed", 1) + k;
            auto run = runRve(r, project, one, property, boundary, temperature, frequency, ctx);
            if (!run)
                return R::failure(run.message());
            Ks.push_back(run.value().K);
            lastRun = run.value();
            if (ctx)
                ctx->reportProgress(static_cast<double>(++step) / total, "RVE " + std::to_string(n) + " voxels, realization " + std::to_string(k + 1));
        }
        Eigen::Matrix3d mean = Eigen::Matrix3d::Zero();
        for (const auto& K : Ks)
            mean += K / static_cast<double>(Ks.size());
        double var = 0.0;
        for (const auto& K : Ks)
            var += std::pow(K.trace() / 3.0 - mean.trace() / 3.0, 2);
        const double sd = Ks.size() > 1 ? std::sqrt(var / (Ks.size() - 1)) : 0.0;
        study.push_back({{"voxels", sized["voxels"]}, {"size", sized["size"]}, {"realizations", Ks.size()}, {"mean", mean.trace() / 3.0},
                         {"standardDeviation", sd}, {"tensor", matrixJson(mean)}});
        last = Ks;
    }
    Eigen::Matrix3d K = Eigen::Matrix3d::Zero();
    for (const auto& k : last)
        K += k / static_cast<double>(last.size());
    const double meanValue = K.trace() / 3.0;
    const double sd = study.back()["standardDeviation"].get<double>();
    std::optional<double> u;
    if (last.size() > 1)
        u = sd / std::sqrt(static_cast<double>(last.size()));
    bool representative = last.size() < 2 || sd / meanValue < tolerance;
    if (study.size() > 1) {
        const double before = study[study.size() - 2]["mean"].get<double>();
        representative = representative && std::abs(meanValue / before - 1.0) < tolerance;
    }
    const std::string domain = property.substr(0, property.find('.')), key = property.substr(property.find('.') + 1);
    d["properties"].push_back(propertyJson(domain, key, "mean", meanValue, u));
    for (int a = 0; a < 3; ++a)
        d["properties"].push_back(propertyJson(domain, key, std::string(1, "xyz"[a]) + std::string(1, "xyz"[a]), K(a, a), u));
    d["tensor"] = matrixJson(K);
    d["anisotropy"] = K.diagonal().maxCoeff() / K.diagonal().minCoeff();
    if (property == "thermal.thermalConductivity") {
        // Density by volume and specific heat by mass (exact averages).
        auto models = phaseModels(r);
        double rho = 0.0, rhoCp = 0.0;
        bool ok = true;
        for (size_t p = 0; p < models.value().size(); ++p) {
            const std::string id = models.value()[p].uniaxial ? models.value()[p].axial : models.value()[p].material;
            auto dr = constituentValue(r, project, id, "thermal.density", temperature, 0.0);
            auto cp = constituentValue(r, project, id, "thermal.specificHeat", temperature, 0.0);
            if (!dr || !cp) {
                ok = false;
                break;
            }
            const double f = p < lastRun.fractions.size() ? lastRun.fractions[p] : 0.0;
            rho += f * dr.value();
            rhoCp += f * dr.value() * cp.value();
        }
        if (ok && rho > 0.0) {
            d["properties"].push_back(propertyJson("thermal", "density", "isotropic", rho));
            d["properties"].push_back(propertyJson("thermal", "specificHeat", "isotropic", rhoCp / rho));
        } else {
            d["warnings"].push_back("density and specific heat need every phase's thermal data: not derived");
        }
    }
    if (property == "electromagnetic.relativePermittivity") {
        auto models = phaseModels(r);
        for (const auto& pm : models.value())
            for (const std::string& id : {pm.material, pm.axial, pm.transverse})
                if (!id.empty())
                    if (auto sigma = constituentValue(r, project, id, "electromagnetic.conductivity", temperature, frequency); sigma && sigma.value() > 0.0)
                        d["warnings"].push_back("a phase conducts: the static permittivity of a conducting composite is not a material constant");
    }
    if (s.value("bounds", false)) {
        json bounds = json::object();
        for (auto [name, b] : {std::pair{"flux", ms::Boundary::Flux}, std::pair{"linear", ms::Boundary::Linear}}) {
            json one = spec;
            one["voxels"] = study.back()["voxels"];
            one["size"] = study.back()["size"];
            auto run = runRve(r, project, one, property, b, temperature, frequency, ctx);
            if (run)
                bounds[name] = run.value().K.trace() / 3.0;
        }
        d["bounds"] = bounds;
        if (bounds.contains("flux") && bounds.contains("linear"))
            d["bounds"]["relativeGap"] = (bounds["linear"].get<double>() - bounds["flux"].get<double>()) / meanValue;
    }
    d["convergence"] = {{"sizeStudy", study}, {"representative", representative}, {"tolerance", tolerance},
                        {"hillMandel", lastRun.hillMandel}, {"solverResidual", lastRun.residual}, {"iterations", lastRun.iterations}};
    d["resolution"] = {{"voxels", lastRun.voxels}, {"elements", "trilinear hexahedra, one per voxel"}};
    d["sampling"] = {{"realizations", last.size()}, {"seeds", spec.value("seed", 1)}};
    d["boundaryConditions"] = {{"kind", ms::toString(boundary)}};
    d["inputs"] = {{"rve", spec}, {"phaseMaterials", r.data.value("phaseMaterials", json::array())}, {"property", property}};
    d["solver"] = "multiscale.potential-fe";
    d["solverVersion"] = "1";
    d["observables"] = {{"volumeFractions", lastRun.fractions}};
    if (!representative)
        d["reliability"].push_back("the RVE is not shown to be representative (spread or size change above " + std::to_string(tolerance) + ")");
    if (lastRun.hillMandel > 1e-6)
        d["reliability"].push_back("the energy consistency (Hill-Mandel) residual is " + std::to_string(lastRun.hillMandel));
    if (last.size() < 2 && random)
        d["reliability"].push_back("one realization of a random RVE: its spread, and so the uncertainty, is unknown");
    d["reliable"] = d["reliability"].empty();
    return d;
}

// --- atomistic derivations ---------------------------------------------------------------------

Result<std::pair<atomistic::Crystal, const atomistic::ForceField*>> crystalOf(const ms::Representation& r)
{
    using CR = Result<std::pair<atomistic::Crystal, const atomistic::ForceField*>>;
    auto c = atomistic::Crystal::fromJson(r.data.value("structure", json::object()));
    if (!c)
        return CR::failure(c.message());
    const atomistic::ForceField* f = atomistic::findForceField(r.data.value("forceField", ""));
    if (!f)
        return CR::failure("the representation names no known force field (\"forceField\": lj.argon, lj.reduced)");
    return std::make_pair(c.take(), f);
}

R deriveElastic(const ms::Representation& r, const Project& project, const DerivationMethod& m, const json& s, JobContext* ctx)
{
    auto cf = crystalOf(r);
    if (!cf)
        return R::failure(cf.message());
    const atomistic::ForceField& f = *cf.value().second;
    atomistic::StaticsSettings st;
    st.cutoffSigma = s.value("cutoffSigma", f.cutoffSigma);
    const double strain = s.value("strain", 1e-4);
    auto relaxed = atomistic::relaxIsotropic(cf.value().first, f, st);
    if (!relaxed)
        return R::failure(relaxed.message());
    if (ctx)
        ctx->reportProgress(0.3, "relaxed");
    auto el = atomistic::elasticConstants(relaxed.value().crystal, f, st, strain);
    if (!el)
        return R::failure(el.message());
    // The cutoff sensitivity: the same at 1.5 times the cutoff.
    atomistic::StaticsSettings wider = st;
    wider.cutoffSigma = 1.5 * st.cutoffSigma;
    auto relaxedWide = atomistic::relaxIsotropic(cf.value().first, f, wider);
    auto elWide = relaxedWide ? atomistic::elasticConstants(relaxedWide.value().crystal, f, wider, strain) : Result<atomistic::ElasticResult>::failure("");
    if (ctx)
        ctx->reportProgress(0.9, "elastic constants");
    const auto& e = el.value();
    const auto du = [&](double a, double b) { return std::optional<double>(std::abs(a - b)); };
    json d = base(r, project, m);
    d["conditions"] = {{"temperatureK", 0.0}};
    const bool wideOk = static_cast<bool>(elWide);
    const auto unc = [&](double a, double b) { return wideOk ? du(a, b) : std::optional<double>(); };
    d["properties"] = json::array({
        propertyJson("mechanical", "density", "isotropic", e.density, unc(e.density, wideOk ? elWide.value().density : 0.0)),
        propertyJson("mechanical", "youngsModulus", "isotropic (Voigt-Reuss-Hill)", e.young, unc(e.young, wideOk ? elWide.value().young : 0.0)),
        propertyJson("mechanical", "poissonsRatio", "isotropic (Voigt-Reuss-Hill)", e.poisson, unc(e.poisson, wideOk ? elWide.value().poisson : 0.0)),
        propertyJson("acoustic", "density", "isotropic", e.density, unc(e.density, wideOk ? elWide.value().density : 0.0)),
        propertyJson("acoustic", "soundSpeed", "longitudinal, isotropic", e.longitudinalSpeed,
                 unc(e.longitudinalSpeed, wideOk ? elWide.value().longitudinalSpeed : 0.0)),
    });
    d["elasticConstants"] = matrixJson(e.C);
    d["observables"] = {{"latticeParameter", relaxed.value().crystal.lattice(0, 0)},
                        {"cohesiveEnergyPerAtomJ", -relaxed.value().energyPerAtom},
                        {"cohesiveEnergyPerAtomEv", -relaxed.value().energyPerAtom / kElectronVolt},
                        {"residualPressure", relaxed.value().pressure},
                        {"bulkModulus", e.bulk},
                        {"shearModulus", e.shear},
                        {"bulkVoigt", e.bulkVoigt},
                        {"bulkReuss", e.bulkReuss},
                        {"shearVoigt", e.shearVoigt},
                        {"shearReuss", e.shearReuss},
                        {"transverseSpeed", e.transverseSpeed},
                        {"cauchyDeviation", e.cauchyDeviation},
                        {"bornStable", e.stable},
                        {"maxInternalForce", e.maxInternalForce}};
    d["convergence"] = {{"latticeRelaxation", "bisection of dE/ds to 1e-13 in the scale"},
                        {"cutoffSigma", st.cutoffSigma},
                        {"youngsModulusAtWiderCutoff", wideOk ? json(elWide.value().young) : json()},
                        {"strain", strain}};
    d["resolution"] = {{"atomsInCell", cf.value().first.atoms.size()}, {"cutoffSigma", st.cutoffSigma}};
    d["sampling"] = {{"kind", "static: no sampling"}};
    d["boundaryConditions"] = {{"kind", "periodic crystal, homogeneous strain"}};
    d["inputs"] = {{"forceField", f.toJson()}, {"structure", cf.value().first.toJson()}};
    d["solver"] = "atomistic.lj.lattice-statics";
    d["solverVersion"] = "1";
    for (const std::string& w : e.warnings)
        d["warnings"].push_back(w);
    if (!e.stable)
        d["reliability"].push_back("not mechanically stable (Born)");
    if (!e.warnings.empty())
        d["reliability"].push_back("see the warnings");
    d["warnings"].push_back("0 K, classical: zero-point motion and thermal softening are absent (" + f.validity + ")");
    d["reliable"] = d["reliability"].empty();
    return d;
}

R deriveMd(const ms::Representation& r, const Project& project, const DerivationMethod& m, const json& s, JobContext* ctx)
{
    auto cf = crystalOf(r);
    if (!cf)
        return R::failure(cf.message());
    const atomistic::ForceField& f = *cf.value().second;
    const auto cells = s.value("supercell", std::vector<int>{4, 4, 4});
    if (cells.size() != 3 || *std::min_element(cells.begin(), cells.end()) < 1 || cells[0] * cells[1] * cells[2] > 512)
        return R::failure("supercell: three repetitions, at most 512 cells");
    // Start from the relaxed lattice.
    auto relaxed = atomistic::relaxIsotropic(cf.value().first, f);
    if (!relaxed)
        return R::failure(relaxed.message());
    const atomistic::Crystal super = relaxed.value().crystal.supercell(cells[0], cells[1], cells[2]);
    atomistic::MdSettings md;
    md.temperature = s.value("temperatureK", 20.0);
    md.timeStep = s.value("timeStep", 1e-14);
    md.steps = std::clamp(s.value("steps", 2000), 10, 200000);
    md.seed = s.value("seed", 1ull);
    md.cutoffSigma = s.value("cutoffSigma", 2.5);
    md.sampleEvery = std::max(1, md.steps / 200);
    const auto cancel = [ctx] { return ctx && ctx->cancelRequested(); };
    auto run = atomistic::runNve(super, f, md, cancel);
    if (!run)
        return R::failure(run.message());
    json d = base(r, project, m);
    d["conditions"] = {{"temperatureK", run.value().meanTemperature}};
    json conv = {{"energyDrift", run.value().energyDrift}, {"timeStep", md.timeStep}, {"steps", md.steps}};
    if (s.value("checkTimeStep", true)) {
        atomistic::MdSettings half = md;
        half.timeStep = 0.5 * md.timeStep;
        half.steps = 2 * md.steps;
        half.sampleEvery = 2 * md.sampleEvery;
        auto h = atomistic::runNve(super, f, half, cancel);
        if (h) {
            conv["energyDriftHalfStep"] = h.value().energyDrift;
            conv["driftRatio"] = run.value().energyDrift / std::max(1e-300, h.value().energyDrift);
        }
    }
    const double drift = run.value().energyDrift;
    d["observables"] = {{"meanTemperatureK", run.value().meanTemperature},
                        {"atoms", run.value().atoms},
                        {"rdfRadius", run.value().rdfRadius},
                        {"rdf", run.value().rdf},
                        {"time", run.value().time},
                        {"totalEnergy", run.value().total},
                        {"msd", run.value().msd}};
    d["convergence"] = conv;
    d["resolution"] = {{"atoms", run.value().atoms}, {"supercell", cells}, {"cutoffSigma", md.cutoffSigma}};
    d["sampling"] = {{"ensemble", "NVE"}, {"samples", run.value().time.size()}, {"seed", md.seed}};
    d["boundaryConditions"] = {{"kind", "periodic orthorhombic cell, minimum image"}};
    d["inputs"] = {{"forceField", f.toJson()}, {"settings", s}};
    d["solver"] = "atomistic.lj.md-nve";
    d["solverVersion"] = "1";
    if (drift > 1e-3)
        d["reliability"].push_back("the energy drifts by " + std::to_string(drift) + " of the kinetic energy: reduce the time step");
    if (conv.contains("driftRatio") && (conv["driftRatio"].get<double>() < 2.0 || conv["driftRatio"].get<double>() > 8.0))
        d["reliability"].push_back("the drift does not scale with the time step squared: the run is not in the integrator's asymptotic regime");
    d["warnings"].push_back("one short NVE run: observables are single-run values, not converged ensemble averages; no library property is derived");
    d["reliable"] = d["reliability"].empty();
    return d;
}

}  // namespace

json DerivationMethod::toJson() const
{
    return {{"id", id},
            {"title", title},
            {"modelCard", modelCard},
            {"description", description},
            {"sourceScale", std::string(ms::toString(source))},
            {"targetScale", std::string(ms::toString(target))},
            {"kinds", kinds},
            {"properties", properties},
            {"settings", settings}};
}

const std::vector<DerivationMethod>& derivationMethods()
{
    static const json rveSettings{
        {"boundary", "periodic (default for periodic RVEs) | linear | mixed | flux"},
        {"realizations", "random RVEs: seeds seed, seed + 1, ... (default 1)"},
        {"sizes", "voxels per axis of a size study at constant voxel size (default: the RVE's)"},
        {"bounds", "also compute the flux (lower) and linear (upper) apparent values (default false)"},
        {"temperatureK", "evaluate the constituents' temperature laws at this temperature"},
        {"tolerance", "relative spread and size change below which the RVE is representative (default 0.02)"}};
    static const std::vector<DerivationMethod> methods{
        {"rve.thermal-conductivity", "Thermal conductivity of an RVE", "rve.potential-fe",
         "the effective conductivity tensor by finite elements on the voxel RVE; density by volume and specific heat by mass", ms::Scale::Microstructure,
         ms::Scale::Effective, {"rve.voxel"}, {"thermal.thermalConductivity", "thermal.density", "thermal.specificHeat"}, rveSettings},
        {"rve.electrical-conductivity", "Electrical conductivity of an RVE", "rve.potential-fe", "steady conduction: the same problem as heat",
         ms::Scale::Microstructure, ms::Scale::Effective, {"rve.voxel"}, {"electromagnetic.conductivity"}, rveSettings},
        {"rve.static-permittivity", "Static permittivity of an RVE", "rve.potential-fe",
         "electrostatics of insulating phases: the quasi-static permittivity, valid for features far below the wavelength", ms::Scale::Microstructure,
         ms::Scale::Effective, {"rve.voxel"}, {"electromagnetic.relativePermittivity"},
         [] {
             json s = rveSettings;
             s["frequencyHz"] = "the frequency the constituents' values are taken at (default 0: their nominal values)";
             return s;
         }()},
        {"atomistic.elastic-constants", "Elastic constants of a crystal", "atomistic.lj.lattice-statics",
         "Lennard-Jones lattice statics: relaxation, C_ij, Voigt-Reuss-Hill moduli, density and sound speed", ms::Scale::Atomistic, ms::Scale::Continuum,
         {"crystal", "atomic-structure"},
         {"mechanical.density", "mechanical.youngsModulus", "mechanical.poissonsRatio", "acoustic.density", "acoustic.soundSpeed"},
         {{"cutoffSigma", "the cutoff in sigma (default: the force field's)"}, {"strain", "the strain amplitude (default 1e-4)"}}},
        {"atomistic.md-nve", "NVE molecular dynamics of a crystal", "atomistic.lj.md-nve",
         "energy conservation, temperature, radial distribution and mean squared displacement; diagnostic: no library property",
         ms::Scale::Atomistic, ms::Scale::Atomistic, {"crystal", "atomic-structure"}, {},
         {{"supercell", "[n1, n2, n3] (default [4, 4, 4])"}, {"temperatureK", "initial temperature (default 20)"}, {"timeStep", "s (default 1e-14)"},
          {"steps", "default 2000"}, {"checkTimeStep", "also run at half the step (default true)"}, {"cutoffSigma", "default 2.5"}, {"seed", "default 1"}}},
    };
    return methods;
}

const DerivationMethod* findDerivationMethod(const std::string& id)
{
    for (const DerivationMethod& m : derivationMethods())
        if (m.id == id)
            return &m;
    return nullptr;
}

std::vector<std::string> applicability(const DerivationMethod& m, const ms::Representation& r, const Project& project, const json&)
{
    std::vector<std::string> issues;
    if (r.scale != m.source)
        issues.push_back("the method '" + m.id + "' derives from the " + std::string(ms::toString(m.source)) + " scale, not from the " +
                         std::string(ms::toString(r.scale)) + " scale of '" + r.name + "'");
    if (std::find(m.kinds.begin(), m.kinds.end(), r.kind) == m.kinds.end())
        issues.push_back("the method '" + m.id + "' does not accept a '" + r.kind + "' representation");
    if (!issues.empty())
        return issues;
    if (m.source == ms::Scale::Microstructure) {
        auto models = phaseModels(r);
        if (!models)
            issues.push_back(models.message());
        for (const std::string& c : r.constituents)
            if (!project.materials.find(Id(c)))
                issues.push_back("the constituent '" + c + "' is not in the library");
        if (auto rve = ms::generateRve(r.data.value("rve", json::object())); !rve)
            issues.push_back(rve.message());
    } else if (m.source == ms::Scale::Atomistic) {
        auto cf = crystalOf(r);
        if (!cf) {
            issues.push_back(cf.message());
        } else {
            if (!cf.value().first.periodic)
                issues.push_back("the structure has no cell: lattice statics and this molecular dynamics need a periodic structure");
            for (const std::string& s : atomistic::incompatibleSpecies(*cf.value().second, cf.value().first))
                issues.push_back("the force field '" + cf.value().second->id + "' is not applicable to the species '" + s + "'");
        }
    }
    return issues;
}

R derive(const ms::Representation& r, const Project& project, const std::string& methodId, const json& settings, JobContext* ctx)
{
    const DerivationMethod* m = findDerivationMethod(methodId);
    if (!m) {
        if (r.scale == ms::Scale::Electronic || r.scale == ms::Scale::Molecular)
            return R::failure("no " + std::string(ms::toString(r.scale)) + "-scale calculation is integrated in this build: an external engine and its adapter "
                              "are needed (see the external solvers)");
        return R::failure("unknown derivation method '" + methodId + "'");
    }
    const json s = settings.is_object() ? settings : json::object();
    if (const auto issues = applicability(*m, r, project, s); !issues.empty()) {
        std::string text;
        for (const auto& i : issues)
            text += (text.empty() ? "" : "; ") + i;
        return R::failure(text);
    }
    try {
        if (m->source == ms::Scale::Microstructure)
            return deriveRve(r, project, *m, s, ctx);
        if (m->id == "atomistic.elastic-constants")
            return deriveElastic(r, project, *m, s, ctx);
        return deriveMd(r, project, *m, s, ctx);
    } catch (const std::exception& e) {
        return R::failure(std::string("invalid settings: ") + e.what());
    }
}

Result<Assignment> planAssignment(const json& d, const Project& project, const ms::MultiscaleModel& model, const json& options, const std::string& calculation)
{
    using AR = Result<Assignment>;
    if (d.value("format", "") != kDerivationFormat)
        return AR::failure("not a multiscale derivation document");
    const json props = d.value("properties", json::array());
    if (props.empty())
        return AR::failure("the derivation produced no library property (the method '" + d.value("method", std::string()) + "' is diagnostic)");
    const json o = options.is_object() ? options : json::object();
    const json rep = d.value("representation", json::object());
    const ms::Representation* source = model.find(rep.value("id", ""));
    if (!source)
        return AR::failure("the derivation's representation '" + rep.value("id", std::string()) + "' is not in the project");
    if (source->revision() != rep.value("revision", ""))
        return AR::failure("the representation '" + source->name + "' changed since the calculation: derive again");
    const json keys = d.value("constituentKeys", json::object());  // items() must not outlive its object
    for (const auto& [id, key] : keys.items()) {
        const Material* c = project.materials.find(Id(id));
        if (!c || ms::materialContentKey(*c) != key.get<std::string>())
            return AR::failure("the constituent '" + id + "' changed since the calculation: derive again");
    }
    Assignment a;
    const std::string existing = o.value("materialId", "");
    if (!existing.empty()) {
        const Material* m = project.materials.find(Id(existing));
        if (!m)
            return AR::failure("there is no material '" + existing + "'");
        a.material = *m;
    } else {
        const json n = o.value("newMaterial", json::object());
        const std::string id = n.value("id", Id::generate().str());
        if (project.materials.find(Id(id)))
            return AR::failure("a material with the id '" + id + "' exists");
        a.material = Material(Id(id), n.value("name", source->name + " (derived)"));
        a.material.setDescription("Derived by " + d.value("method", std::string()) + " from the " + rep.value("scale", std::string()) + " representation '" +
                                  source->name + "'.");
        a.created = true;
    }
    std::vector<std::string> wanted = o.value("properties", std::vector<std::string>{});
    const json components = o.value("components", json::object());
    ms::Derivation drv;
    drv.id = "drv-" + Id::generate().str();
    drv.method = d.value("method", "");
    drv.modelCard = d.value("modelCard", "");
    drv.solver = d.value("solver", "");
    drv.solverVersion = d.value("solverVersion", "");
    drv.representation = source->id;
    drv.representationRevision = source->revision();
    drv.sourceScale = ms::scaleFromString(d.value("sourceScale", "microstructure")).value_or(ms::Scale::Microstructure);
    drv.targetScale = ms::scaleFromString(d.value("targetScale", "continuum")).value_or(ms::Scale::Continuum);
    drv.material = a.material.id().str();
    drv.inputs = d.value("inputs", json::object());
    drv.boundaryConditions = d.value("boundaryConditions", json::object());
    drv.convergence = d.value("convergence", json::object());
    drv.resolution = d.value("resolution", json::object());
    drv.sampling = d.value("sampling", json::object());
    const json cond = d.value("conditions", json::object());
    if (cond.contains("temperatureK"))
        drv.conditions.temperatureK = cond["temperatureK"].get<double>();
    drv.constituentKeys = d.value("constituentKeys", std::map<std::string, std::string>{});
    drv.created = d.value("created", "");
    drv.calculation = calculation;
    const bool homogenized = drv.sourceScale == ms::Scale::Microstructure;
    std::set<std::string> done;
    for (const json& p : props) {
        const std::string domain = p.value("domain", ""), key = p.value("key", ""), full = domain + "." + key;
        if (!wanted.empty() && std::find(wanted.begin(), wanted.end(), full) == wanted.end())
            continue;
        if (done.count(full))
            continue;
        const std::string want = components.value(full, std::string());
        if (!want.empty() && p.value("component", "") != want)
            continue;
        const auto dom = physicsDomainFromString(domain);
        if (!dom)
            return AR::failure("unknown domain '" + domain + "'");
        PropertyValue v;
        v.value = p.value("value", 0.0);
        v.provenance = homogenized ? Provenance::Homogenized : Provenance::Computed;
        std::string at;
        if (drv.conditions.temperatureK) {
            char t[32];
            std::snprintf(t, sizeof t, " at %.6g K", *drv.conditions.temperatureK);
            at = t;
        }
        v.source = drv.method + " of '" + source->name + "' (" + rep.value("scale", std::string()) + ", " + p.value("component", std::string()) + ")" + at +
                   "; derivation " + drv.id;
        v.derivation = drv.id;
        if (p.contains("uncertainty") && p["uncertainty"].is_number())
            v.uncertainty = p["uncertainty"].get<double>();
        if (const PropertyValue* old = a.material.property(*dom, key); old && !old->derivation.empty())
            drv.parent = old->derivation;
        if (Status st = a.material.setProperty(*dom, key, v); !st)
            return AR::failure(full + ": " + st.message());
        ms::DerivedProperty dp;
        dp.domain = domain;
        dp.key = key;
        dp.component = p.value("component", "isotropic");
        dp.unit = p.value("unit", "");
        dp.value = v.value;
        dp.uncertainty = v.uncertainty;
        drv.properties.push_back(dp);
        done.insert(full);
    }
    for (const std::string& w : wanted)
        if (!done.count(w))
            return AR::failure("the derivation has no property '" + w + "'" + (components.contains(w) ? " with the component '" + components[w].get<std::string>() + "'" : ""));
    if (drv.properties.empty())
        return AR::failure("no property selected");
    if (Status v = a.material.validate(); !v)
        return AR::failure(v.message());
    if (!d.value("reliable", false))
        a.warnings.push_back("the derivation is not marked reliable: " + d.value("reliability", json::array()).dump());
    a.derivation = drv;
    a.relationships = {{"material:" + drv.material, "representation:" + source->id, "derived-from"},
                       {"derivation:" + drv.id, "representation:" + source->id, "computed-from"}};
    return a;
}

}  // namespace resonant::coupling
