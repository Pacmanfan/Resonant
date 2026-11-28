#include "coupling/MaterialStateAnalyses.h"

#include "materials/Material.h"
#include "mechanics/DamagedBeam.h"
#include "mechanics/ElasticMaterial.h"
#include "project/Project.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace resonant::coupling {

using namespace functional;
using nlohmann::json;
using R = Result<AnalysisOutcome>;

namespace {

double setting(const json& where, const char* key, const Values& v, double fallback)
{
    const auto it = where.find(key);
    if (it == where.end() || it->is_null())
        return fallback;
    if (it->is_number())
        return it->get<double>();
    if (it->is_string()) {
        const auto p = v.find(it->get<std::string>());
        if (p != v.end())
            return p->second;
        throw std::runtime_error(std::string("the setting '") + key + "' names the parameter '" + it->get<std::string>() + "', which does not exist");
    }
    throw std::runtime_error(std::string("the setting '") + key + "' must be a number or a parameter id");
}

R evaluateDamagedCantilever(const FunctionalStructure& s, const Project& p, const Values& v, const ModelInfo& mi, JobContext*)
{
    const Body* b = s.body("beam");
    const SceneObject* o = b ? p.scene.find(Id(b->objectId)) : nullptr;
    if (!o || !o->state.geometry || o->state.geometry->kindId() != "box")
        return R::failure("the body 'beam' must be a box (length along X, thickness along Z)");
    if (o->state.materialId.isNull())
        return R::failure("the beam has no material");
    const Material* m = p.materials.find(o->state.materialId);
    if (!m)
        return R::failure("the beam's material is not in the library");
    auto e = mechanics::elasticMaterialOf(*m);
    if (!e)
        return R::failure("'" + m->name() + "': " + e.message());
    const json& st = s.analysis.settings;
    mechanics::DamagedBeamSpec spec;
    spec.length = o->state.geometry->parameter("width");
    spec.width = o->state.geometry->parameter("depth");
    spec.thickness = o->state.geometry->parameter("height");
    spec.density = e.value().density;
    spec.damage.youngsModulus = e.value().youngsModulus;
    spec.damage.thresholdStrain = setting(st, "thresholdStrain", v, 1e-4);
    spec.damage.softeningStrain = setting(st, "softeningStrain", v, 1e-3);
    spec.damage.exponent = setting(st, "exponent", v, 0.6);
    spec.damage.law = st.value("law", std::string("power"));
    spec.elements = static_cast<int>(setting(st, "elements", v, 40));
    const int perHalf = static_cast<int>(setting(st, "increments", v, 40));
    std::vector<double> amplitudes;
    if (st.contains("amplitudes") && st["amplitudes"].is_array()) {
        for (const json& a : st["amplitudes"])
            amplitudes.push_back(a.is_string() ? v.at(a.get<std::string>()) : a.get<double>());
    } else {
        amplitudes.push_back(setting(st, "amplitude", v, 0.01));
    }
    for (double a : amplitudes) {
        for (int i = 1; i <= perHalf; ++i)
            spec.tipDeflections.push_back(a * i / perHalf);
        for (int i = perHalf - 1; i >= 0; --i)
            spec.tipDeflections.push_back(a * i / perHalf);
    }
    auto r = mechanics::damagedCantilever(spec);
    if (!r)
        return R::failure(r.message());
    for (const auto& step : r.value().steps)
        if (!step.converged)
            return R::failure("a load step did not converge (the damage and the shape found no fixed point)");
    const auto& x = r.value();
    AnalysisOutcome out;
    out.solver = mi.id;
    out.assumptions = mi.assumptions;
    out.behaviour = "resonant";
    Values& met = out.metrics;
    met["damage.f1Undamaged"] = x.undamagedFrequencies[0];
    met["damage.f1"] = x.frequencies[0];
    met["damage.f1Shift"] = x.frequencies[0] / x.undamagedFrequencies[0] - 1.0;
    if (x.frequencies.size() > 1)
        met["damage.f2"] = x.frequencies[1];
    met["damage.rootDamage"] = x.damage.front();
    met["damage.maxDamage"] = *std::max_element(x.damage.begin(), x.damage.end());
    met["damage.dissipated"] = x.steps.back().dissipated;
    met["damage.stiffnessRatio"] = x.damagedTipStiffness / x.undamagedTipStiffness;
    const double inf = std::numeric_limits<double>::infinity();
    for (size_t k = 0; k < x.frequencies.size(); ++k) {
        Mode mode("bending " + std::to_string(k + 1) + " (damaged)", x.frequencies[k], inf,
                  {{"undamagedHz", x.undamagedFrequencies[k]}, {"relativeShift", x.frequencies[k] / x.undamagedFrequencies[k] - 1.0}});
        mode.nature = "natural";
        out.modes.push_back(mode);
    }
    Curve load;
    load.id = "loadDeflection";
    load.title = "Tip load against deflection";
    load.xLabel = "Deflection";
    load.xUnit = "m";
    load.yLabel = "Load";
    load.yUnit = "N";
    for (const auto& step : x.steps) {
        load.x.push_back(step.deflection);
        load.y.push_back(step.load);
    }
    Curve damage;
    damage.id = "damage";
    damage.title = "Damage along the cantilever";
    damage.xLabel = "Position";
    damage.xUnit = "m";
    damage.yLabel = "Damage";
    damage.x = x.elementCentres;
    damage.y = x.damage;
    out.curves = {load, damage};
    out.details = {{"law", spec.damage.law}, {"amplitudes", amplitudes}, {"elements", spec.elements}};
    out.discretization = {{"model", "Euler-Bernoulli Hermite elements"}, {"elements", spec.elements}, {"steps", spec.tipDeflections.size()}};
    if (spec.damage.law == "exponential")
        out.warnings.push_back("the exponential law softens: its damage localizes and depends on the number of elements");
    return out;
}

}  // namespace

void registerMaterialStateAnalyses(AnalysisRegistry& registry)
{
    AnalysisKind k;
    k.id = kDamagedCantileverAnalysis;
    k.title = "Damaged cantilever";
    k.description = "A cantilever cycled by its tip: damage accumulates where the bending strain is largest and the modes shift accordingly";
    k.domains = {"mechanical"};
    k.category = Category::Confinement;
    k.mechanism = "bending strain beyond the damage threshold softens the root of the beam; the lower bending stiffness there lowers every bending "
                  "frequency, the first most";
    k.models = {{"mechanics.damaged-cantilever.euler-bernoulli", "Euler-Bernoulli elements with scalar damage", "1", "reduced",
                 {"damage per element from its extreme-fibre strain, uniform over the section", "displacement-controlled tip cycles",
                  "modes from the damaged stiffness and the consistent mass"},
                 "slender beams; the power law for mesh-objective results",
                 0.05}};
    k.metrics = {{"damage.f1Undamaged", "First frequency, undamaged", "Hz", ""},
                 {"damage.f1", "First frequency, damaged", "Hz", "from the damaged stiffness"},
                 {"damage.f1Shift", "Relative shift of f1", "", "f1 / f1,undamaged - 1"},
                 {"damage.f2", "Second frequency, damaged", "Hz", ""},
                 {"damage.rootDamage", "Damage at the root", "", ""},
                 {"damage.maxDamage", "Largest damage", "", ""},
                 {"damage.dissipated", "Energy dissipated by damage", "J", ""},
                 {"damage.stiffnessRatio", "Tip stiffness ratio", "", "damaged / undamaged"}};
    k.roles = {{"beam", "body", "a box: length along X, thickness along Z, with an elastic material", true}};
    k.evaluate = evaluateDamagedCantilever;
    registry.add(std::move(k));
}

}  // namespace resonant::coupling
