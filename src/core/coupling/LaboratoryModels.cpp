#include "coupling/LaboratoryModels.h"

#include "coupling/MaterialStateModels.h"
#include "coupling/StateEvolution.h"
#include "electromagnetics/LayeredMedia.h"
#include "functional/FunctionalStructure.h"
#include "functional/FunctionalStudy.h"
#include "mechanics/Plasticity.h"
#include "multiphysics/PhysicalSystem.h"
#include "optimization/Persistence.h"
#include "oscillators/LumpedNetwork.h"
#include "project/Project.h"

#include <algorithm>
#include <cmath>

namespace resonant::coupling {

using nlohmann::json;
namespace ex = experiments;

namespace {

constexpr double kPi = 3.14159265358979323846;

double interpolate(const std::vector<double>& x, const std::vector<double>& y, double at)
{
    const auto it = std::lower_bound(x.begin(), x.end(), at);
    if (it == x.begin())
        return y.front();
    if (it == x.end())
        return y.back();
    const size_t k = static_cast<size_t>(it - x.begin());
    const double t = (at - x[k - 1]) / (x[k] - x[k - 1]);
    return y[k - 1] + t * (y[k] - y[k - 1]);
}

double param(const std::map<std::string, double>& p, const std::string& id, double fallback)
{
    const auto it = p.find(id);
    return it == p.end() ? fallback : it->second;
}

LabModelParameter mp(std::string id, std::string label, std::string unit, double value, double lower, double upper, std::string meaning)
{
    return {std::move(id), std::move(label), std::move(unit), std::move(meaning), value, lower, upper};
}

// --- resonator ------------------------------------------------------------------------------------

Result<oscillators::Network> networkFor(const json& cfg, const std::map<std::string, double>& p)
{
    using R = Result<oscillators::Network>;
    oscillators::Network n;
    if (cfg.contains("network")) {
        const json& j = cfg["network"];
        n.names = j.value("names", std::vector<std::string>{});
        n.masses = j.value("masses", std::vector<double>{});
        for (const json& s : j.value("springs", json::array()))
            n.springs.push_back({s.value("name", std::string()), s.value("i", 0), s.value("j", -1), s.value("k", 0.0)});
        for (const json& d : j.value("dampers", json::array()))
            n.dampers.push_back({d.value("mechanism", std::string("viscous")), d.value("i", 0), d.value("j", -1), d.value("c", 0.0)});
        for (size_t i = 0; i < n.masses.size(); ++i)
            n.masses[i] = param(p, "masses." + std::to_string(i), n.masses[i]);
        for (size_t i = 0; i < n.springs.size(); ++i)
            n.springs[i].k = param(p, "springs." + std::to_string(i), n.springs[i].k);
        for (size_t i = 0; i < n.dampers.size(); ++i)
            n.dampers[i].c = param(p, "dampers." + std::to_string(i), n.dampers[i].c);
    } else {
        n.names = {"resonator"};
        n.masses = {param(p, "mass", cfg.value("mass", 1.0))};
        n.springs = {{"stiffness", 0, -1, param(p, "stiffness", cfg.value("stiffness", 1.0e4))}};
        n.dampers = {{"damping", 0, -1, param(p, "damping", cfg.value("damping", 1.0))}};
    }
    if (Status v = n.validate(); !v)
        return R::failure(v.message());
    return n;
}

std::vector<LabModelParameter> resonatorParameters(const json& cfg)
{
    std::vector<LabModelParameter> out;
    if (cfg.contains("network")) {
        const json& j = cfg["network"];
        const auto masses = j.value("masses", std::vector<double>{});
        for (size_t i = 0; i < masses.size(); ++i)
            out.push_back(mp("masses." + std::to_string(i), "Mass " + std::to_string(i), "kg", masses[i], 0.0, 1e300, "a lumped mass"));
        size_t i = 0;
        for (const json& s : j.value("springs", json::array()))
            out.push_back(mp("springs." + std::to_string(i++), "Stiffness " + s.value("name", std::string()), "N/m", s.value("k", 0.0), 0.0, 1e300,
                             "a spring's stiffness"));
        i = 0;
        for (const json& d : j.value("dampers", json::array()))
            out.push_back(mp("dampers." + std::to_string(i++), "Damping " + d.value("mechanism", std::string()), "N s/m", d.value("c", 0.0), 0.0, 1e300,
                             "a damper's coefficient"));
        return out;
    }
    return {mp("mass", "Mass", "kg", cfg.value("mass", 1.0), 0.0, 1e300, "the moving mass"),
            mp("stiffness", "Stiffness", "N/m", cfg.value("stiffness", 1.0e4), 0.0, 1e300, "the restoring stiffness"),
            mp("damping", "Damping", "N s/m", cfg.value("damping", 1.0), 0.0, 1e300, "the viscous damping coefficient")};
}

Result<Prediction> predictResonator(const json& cfg, const std::map<std::string, double>& p, const std::vector<double>& f)
{
    using R = Result<Prediction>;
    auto net = networkFor(cfg, p);
    if (!net)
        return R::failure(net.message());
    const int drive = cfg.value("drive", 0), at = cfg.value("coordinate", 0);
    if (drive < 0 || drive >= net.value().size() || at < 0 || at >= net.value().size())
        return R::failure("the drive or response coordinate is not in the network");
    std::vector<std::complex<double>> force(static_cast<size_t>(net.value().size()), 0.0);
    force[static_cast<size_t>(drive)] = 1.0;
    std::vector<double> positive;
    for (double x : f)
        if (!(x > 0.0))
            return R::failure("a driven response needs positive frequencies");
    auto pts = oscillators::drivenResponse(net.value(), force, f);
    if (!pts)
        return R::failure(pts.message());
    const std::string response = cfg.value("response", std::string("receptance"));
    Prediction out;
    out.complex = true;
    out.axis = f;
    out.model = "resonator.lumped";
    out.solver = "oscillators.lumped-network (harmonic response)";
    out.quantity.per = ex::Quantity::Force;
    for (size_t k = 0; k < f.size(); ++k) {
        const std::complex<double> x = pts.value()[k].displacement[static_cast<size_t>(at)];
        const std::complex<double> jw(0.0, 2.0 * kPi * f[k]);
        out.values.push_back(response == "mobility" ? jw * x : response == "accelerance" ? jw * jw * x : x);
    }
    out.quantity.quantity = response == "mobility" ? ex::Quantity::Velocity : response == "accelerance" ? ex::Quantity::Acceleration : ex::Quantity::Displacement;
    if (response != "receptance" && response != "mobility" && response != "accelerance")
        return R::failure("the response is receptance, mobility or accelerance");
    out.unit = out.quantity.unit();
    return out;
}

// --- layered electromagnetics -------------------------------------------------------------------

std::vector<LabModelParameter> layeredParameters(const json& cfg)
{
    std::vector<LabModelParameter> out;
    for (const json& l : cfg.value("layers", json::array())) {
        const std::string n = l.value("name", std::string("layer"));
        out.push_back(mp(n + ".permittivity", n + ": relative permittivity", "1", l.value("permittivity", 1.0), 1.0, 1e4, "the real relative permittivity"));
        out.push_back(mp(n + ".lossTangent", n + ": loss tangent", "1", l.value("lossTangent", 0.0), 0.0, 10.0, "the dielectric loss tangent"));
        out.push_back(mp(n + ".thickness", n + ": thickness", "m", l.value("thickness", 0.0), 0.0, 10.0, "the layer's thickness"));
    }
    return out;
}

Result<Prediction> predictLayered(const json& cfg, const std::map<std::string, double>& p, const std::vector<double>& f)
{
    using R = Result<Prediction>;
    Prediction out;
    out.complex = true;
    out.axis = f;
    out.model = "em.layered-reflection";
    out.solver = em::kLayeredSolverId;
    out.quantity.quantity = ex::Quantity::ScatteringParameter;
    out.unit = "1";
    for (double freq : f) {
        em::LayeredProblem prob;
        prob.frequencyHz = freq;
        prob.incidentIntensity = 1.0;
        for (const json& l : cfg.value("layers", json::array())) {
            em::MediumLayer m;
            m.name = l.value("name", std::string("layer"));
            m.permittivityReal = param(p, m.name + ".permittivity", l.value("permittivity", 1.0));
            m.lossTangent = param(p, m.name + ".lossTangent", l.value("lossTangent", 0.0));
            m.thickness = param(p, m.name + ".thickness", l.value("thickness", 0.0));
            m.conductivity = l.value("conductivity", 0.0);
            m.permeability = l.value("permeability", 1.0);
            prob.layers.push_back(m);
        }
        if (cfg.value("backing", std::string("air")) == "metal") {
            em::MediumLayer metal;
            metal.name = "metal backing";
            metal.thickness = 1e-4;
            metal.conductivity = 5.8e7;
            prob.layers.push_back(metal);
        }
        auto s = em::LayeredSolution::solve(prob);
        if (!s)
            return R::failure(s.message());
        out.values.push_back(s.value().reflection());
    }
    return out;
}

// --- thermal evolution ----------------------------------------------------------------------------

Result<multiphysics::PhysicalSystem> systemOf(const Project& p, const std::string& id)
{
    for (const json& j : p.extensions.value("multiphysics", json::object()).value("systems", json::array()))
        if (j.value("id", std::string()) == id)
            return multiphysics::PhysicalSystem::fromJson(j);
    return Result<multiphysics::PhysicalSystem>::failure("there is no physical system '" + id + "'");
}

std::vector<LabModelParameter> thermalParameters(const json& cfg, const Project& p)
{
    std::vector<LabModelParameter> out;
    auto sys = systemOf(p, cfg.value("system", std::string()));
    if (!sys)
        return out;
    for (const auto& r : sys.value().regions) {
        const Material* m = p.materials.find(Id(r.materialId));
        if (!m)
            continue;
        for (const char* key : {"thermalConductivity", "density", "specificHeat"}) {
            const PropertyValue* v = m->property(PhysicsDomain::Thermal, key);
            if (v)
                out.push_back(mp(r.materialId + ".thermal." + key, r.name + ": " + key, std::string(key) == "density" ? "kg/m^3"
                                                                                       : std::string(key) == "specificHeat" ? "J/(kg K)"
                                                                                                                              : "W/(m K)",
                                 v->value, 0.0, 1e300, "a thermal property of the region's material"));
        }
    }
    return out;
}

Result<Prediction> predictThermal(const json& cfg, const std::map<std::string, double>& p, const std::vector<double>& t, const Project& project,
                                  JobContext* ctx)
{
    using R = Result<Prediction>;
    auto sys = systemOf(project, cfg.value("system", std::string()));
    if (!sys)
        return R::failure(sys.message());
    MaterialLibrary lib = project.materials;
    for (const auto& [id, value] : p) {
        const auto a = id.find('.'), b = id.find('.', a + 1);
        if (a == std::string::npos || b == std::string::npos)
            return R::failure("a thermal parameter is <material>.thermal.<key>: '" + id + "'");
        Material* m = lib.findMutable(Id(id.substr(0, a)));
        if (!m)
            return R::failure("there is no material '" + id.substr(0, a) + "'");
        const PropertyValue* old = m->property(PhysicsDomain::Thermal, id.substr(b + 1));
        PropertyValue v = old ? *old : PropertyValue{};
        v.value = value;
        if (Status s = m->setProperty(PhysicsDomain::Thermal, id.substr(b + 1), v); !s)
            return R::failure(s.message());
    }
    EvolutionSettings s;
    s.duration = cfg.value("duration", t.empty() ? 60.0 : t.back());
    s.steps = cfg.value("steps", 20);
    s.substeps = cfg.value("substeps", 4);
    auto doc = evolveSystem(sys.value(), lib, s, ctx);
    if (!doc)
        return R::failure(doc.message());
    const auto z = doc.value().value("positions", std::vector<double>{});
    const double at = cfg.value("position", z.empty() ? 0.0 : z.back());
    std::vector<double> times{0.0}, temps;
    const double ambient = sys.value().ambientTemperature;
    temps.push_back(cfg.value("initialTemperature", ambient));
    for (const json& step : doc.value()["history"]) {
        const auto T = step.value("temperature", std::vector<double>{});
        if (T.size() != z.size() || T.empty())
            return R::failure("the evolution has no temperature field");
        times.push_back(step.value("time", 0.0));
        temps.push_back(interpolate(z, T, at));
    }
    Prediction out;
    out.axis = t;
    out.model = "thermal.evolution-probe";
    out.solver = "coupling.state-evolution (coupled engine per step)";
    out.quantity.quantity = ex::Quantity::Temperature;
    out.unit = "K";
    for (double x : t)
        out.values.push_back(interpolate(times, temps, x));
    out.details = {{"position", at}, {"steps", s.steps}, {"duration", s.duration}};
    return out;
}

// --- J2 material point ----------------------------------------------------------------------------

Result<Prediction> predictJ2(const std::map<std::string, double>& p, const json& cfg, const std::vector<double>& strain)
{
    using R = Result<Prediction>;
    mechanics::J2Parameters m;
    m.youngsModulus = param(p, "youngsModulus", cfg.value("youngsModulus", 200e9));
    m.poissonsRatio = param(p, "poissonsRatio", cfg.value("poissonsRatio", 0.3));
    m.yieldStress = param(p, "yieldStress", cfg.value("yieldStress", 250e6));
    m.isotropicHardening = param(p, "isotropicHardening", cfg.value("isotropicHardening", 2e9));
    m.kinematicHardening = param(p, "kinematicHardening", cfg.value("kinematicHardening", 0.0));
    if (Status v = m.validate(); !v)
        return R::failure(v.message());
    Prediction out;
    out.axis = strain;
    out.model = "material-state.j2-uniaxial";
    out.solver = "mechanics.j2 (radial return, uniaxial stress)";
    out.quantity.quantity = ex::Quantity::Stress;
    out.unit = "Pa";
    mechanics::J2State c;
    mechanics::Vec6 guess = mechanics::Vec6::Zero();
    for (double e : strain) {
        auto u = mechanics::j2Uniaxial(m, c, e, guess);
        if (!u)
            return R::failure(u.message());
        c = u.value().response.state;
        guess = u.value().strain;
        out.values.push_back(u.value().stress);
    }
    return out;
}

// --- functional structures, materials, design revisions --------------------------------------------

Result<json> structureDocument(const Project& p, const std::string& id)
{
    for (const json& s : p.extensions.value(functional::kExtension, json::object()).value("structures", json::array()))
        if (s.value("id", std::string()) == id)
            return s;
    return Result<json>::failure("there is no functional structure '" + id + "'");
}

Result<Prediction> predictMetric(const json& cfg, const std::map<std::string, double>& p, const Project& project, JobContext* ctx)
{
    using R = Result<Prediction>;
    auto doc = structureDocument(project, cfg.value("structure", std::string()));
    if (!doc)
        return R::failure(doc.message());
    const json s = doc.value();
    auto structure = functional::FunctionalStructure::fromJson(s);
    if (!structure)
        return R::failure(structure.message());
    // Parameter values applied as a sweep would: to a copy of the project (and the structure), never the project.
    functional::Values values(p.begin(), p.end());
    std::unique_ptr<Project> changed;
    functional::FunctionalStructure applied = structure.value();
    if (!values.empty()) {
        auto w = functional::withParameters(structure.value(), project, values);
        if (!w)
            return R::failure(w.message());
        changed = std::move(w.value().first);
        applied = w.value().second;
    }
    auto r = functional::analyzeDocument(applied, changed ? *changed : project, functional::AnalysisRegistry::instance(), cfg.value("analysisModel", std::string()),
                                         ctx);
    if (!r)
        return R::failure(r.message());
    const std::string key = cfg.value("metric", std::string());
    for (const json& m : r.value().value("metrics", json::array()))
        if (m.value("key", std::string()) == key) {
            Prediction out;
            out.scalar = true;
            out.model = "functional.metric";
            out.solver = r.value().value("model", json::object()).value("id", std::string("functional analysis"));
            out.values = {m.value("value", 0.0)};
            out.unit = m.value("unit", std::string("1"));
            out.details = {{"structure", s.value("id", std::string())}, {"metric", key}, {"label", m.value("label", key)}};
            return out;
        }
    return R::failure("the analysis has no metric '" + key + "'");
}

}  // namespace

json LabModel::toJson() const
{
    json j{{"id", id}, {"title", title}, {"domain", domain}, {"observable", observable}, {"axis", axis}, {"assumptions", assumptions},
           {"complex", complex}, {"scalar", scalar}, {"configuration", configuration}};
    j["parameters"] = json::array();
    for (const auto& p : parameters)
        j["parameters"].push_back({{"id", p.id}, {"label", p.label}, {"unit", p.unit}, {"meaning", p.meaning}, {"value", p.value}, {"lower", p.lower},
                                   {"upper", p.upper}});
    return j;
}

const std::vector<LabModel>& laboratoryModels()
{
    static const std::vector<LabModel> m = {
        {"resonator.lumped", "Lumped resonator: driven response", "mechanical", "receptance, mobility or accelerance (per unit force)", "frequency (Hz)",
         "linear, viscous damping, a single input; a mass-spring-damper unless a network is configured", true, false,
         resonatorParameters(json::object()), {"response", "drive", "coordinate", "network", "mass", "stiffness", "damping"}},
        {"em.layered-reflection", "Layered stack: reflection S11", "electromagnetic", "S11 at normal incidence", "frequency (Hz)",
         "plane wave, laterally infinite layers, lossless half-spaces; optional metal backing", true, false, {}, {"layers", "backing"}},
        {"thermal.evolution-probe", "Physical system: temperature at a probe over time", "thermal", "temperature (K)", "time (s)",
         "the system's time-stepped coupled evolution; one-dimensional slab", false, false, {}, {"system", "position", "duration", "steps", "substeps"}},
        {"material-state.j2-uniaxial", "J2 material point: stress along a strain path", "mechanical", "stress (Pa)", "strain (in the path's order)",
         "small strain, rate independent, linear hardening, uniaxial stress", false, false,
         {mp("youngsModulus", "Young's modulus", "Pa", 200e9, 1e6, 1e13, "elastic stiffness"),
          mp("yieldStress", "Initial yield stress", "Pa", 250e6, 1e3, 1e10, "the onset of plastic flow"),
          mp("isotropicHardening", "Isotropic hardening", "Pa", 2e9, 1e6, 1e12, "growth of the yield surface (above zero: the uniaxial solver needs hardening)"),
          mp("kinematicHardening", "Kinematic hardening", "Pa", 0.0, 0.0, 1e12, "translation of the yield surface")},
         {"youngsModulus", "poissonsRatio", "yieldStress", "isotropicHardening", "kinematicHardening"}},
        {"functional.metric", "Functional structure: an analysis metric", "general", "one metric value", "-",
         "the structure's own analysis model", false, true, {}, {"structure", "metric", "analysisModel"}},
        {"material.property", "Library material: a property value", "general", "one property value with its provenance", "-",
         "the value as stored (measured, homogenized, computed ...)", false, true, {}, {"material", "domain", "key"}},
        {"design-revision.metric", "Design revision: the candidate's evaluated quantity", "general", "one evaluated quantity", "-",
         "the optimization's evaluation of the promoted candidate", false, true, {}, {"revision", "quantity"}},
    };
    return m;
}

const LabModel* findLaboratoryModel(std::string_view id)
{
    for (const auto& m : laboratoryModels())
        if (m.id == id)
            return &m;
    return nullptr;
}

Result<std::vector<LabModelParameter>> laboratoryModelParameters(const std::string& id, const json& cfg, const Project& project)
{
    using R = Result<std::vector<LabModelParameter>>;
    if (!findLaboratoryModel(id))
        return R::failure("unknown laboratory model '" + id + "'");
    if (id == "resonator.lumped")
        return resonatorParameters(cfg);
    if (id == "em.layered-reflection")
        return layeredParameters(cfg);
    if (id == "thermal.evolution-probe")
        return thermalParameters(cfg, project);
    if (id == "material-state.j2-uniaxial")
        return findLaboratoryModel(id)->parameters;
    if (id == "functional.metric") {
        auto doc = structureDocument(project, cfg.value("structure", std::string()));
        if (!doc)
            return R::failure(doc.message());
        std::vector<LabModelParameter> out;
        for (const json& p : doc.value().value("parameters", json::array()))
            if (p.value("kind", std::string("free")) != "derived")  // free and bound (geometry) parameters; derived ones follow from them
                out.push_back(mp(p.value("id", std::string()), p.value("name", std::string()), p.value("unit", std::string()), p.value("value", 0.0),
                                 p.value("min", -1e300), p.value("max", 1e300),
                                 p.value("kind", std::string("free")) == "bound" ? "a structure parameter bound to " + p.value("binding", std::string())
                                                                                  : "a structure parameter"));
        return out;
    }
    return std::vector<LabModelParameter>{};
}

Result<Prediction> predictLaboratoryModel(const std::string& id, const json& cfg, const std::map<std::string, double>& p, const std::vector<double>& axis,
                                          const Project& project, JobContext* ctx)
{
    using R = Result<Prediction>;
    if (id == "resonator.lumped")
        return predictResonator(cfg, p, axis);
    if (id == "em.layered-reflection")
        return predictLayered(cfg, p, axis);
    if (id == "thermal.evolution-probe")
        return predictThermal(cfg, p, axis, project, ctx);
    if (id == "material-state.j2-uniaxial")
        return predictJ2(p, cfg, axis);
    if (id == "functional.metric")
        return predictMetric(cfg, p, project, ctx);
    if (id == "material.property") {
        const Material* m = project.materials.find(Id(cfg.value("material", std::string())));
        if (!m)
            return R::failure("there is no material '" + cfg.value("material", std::string()) + "'");
        const auto d = physicsDomainFromString(cfg.value("domain", std::string()));
        if (!d)
            return R::failure("unknown domain '" + cfg.value("domain", std::string()) + "'");
        const PropertyValue* v = m->property(*d, cfg.value("key", std::string()));
        if (!v)
            return R::failure("the material has no property " + cfg.value("domain", std::string()) + "." + cfg.value("key", std::string()));
        Prediction out;
        out.scalar = true;
        out.model = "material.property";
        out.solver = "the material library";
        out.values = {v->value};
        out.standard = v->uncertainty.value_or(0.0);
        out.details = {{"material", cfg.value("material", std::string())}, {"provenance", std::string(toString(v->provenance))}, {"source", v->source},
                       {"derivation", v->derivation}};
        return out;
    }
    if (id == "design-revision.metric") {
        const auto w = optimization::workspaceOf(project);
        const std::string rid = cfg.value("revision", std::string());
        for (const auto& rev : w.revisions)
            if (rev.id == rid) {
                const auto* run = w.findRun(rev.runId);
                if (!run)
                    return R::failure("the revision's run '" + rev.runId + "' is not in the project");
                for (const auto& c : run->candidates)
                    if (c.id == rev.candidateId) {
                        const std::string q = cfg.value("quantity", std::string());
                        const auto it = c.evaluation.quantities.find(q);
                        if (it == c.evaluation.quantities.end())
                            return R::failure("the candidate's evaluation has no quantity '" + q + "'");
                        Prediction out;
                        out.scalar = true;
                        out.model = "design-revision.metric";
                        out.solver = c.evaluation.model.modelId;
                        out.values = {it->second};
                        out.details = {{"revision", rid}, {"candidate", c.id}, {"run", rev.runId}, {"quantity", q}, {"fidelity", rev.fidelity}};
                        return out;
                    }
                return R::failure("the run has no candidate '" + rev.candidateId + "'");
            }
        return R::failure("there is no design revision '" + rid + "'");
    }
    return R::failure("unknown laboratory model '" + id + "'");
}

}  // namespace resonant::coupling
