#include "coupling/MatterExploration.h"

#include "accounting/AccountingLedger.h"
#include "accounting/Conservation.h"
#include "accounting/ControlVolume.h"
#include "coupling/CouplingModels.h"
#include "coupling/MatterAnalyses.h"
#include "materials/Material.h"
#include "matter/Manufacturing.h"
#include "matter/PropertyResolution.h"
#include "project/Project.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>

namespace resonant::coupling {

using nlohmann::json;
using namespace resonant::matter;

namespace {

std::string number(double v, int precision = 3)
{
    std::ostringstream s;
    s.precision(precision);
    s << v;
    return s.str();
}

std::vector<Mechanism> buildMechanisms()
{
    std::vector<Mechanism> out{
        {"piezoelectric-direct", "Direct piezoelectric effect", "mechanical", "electrical", "implemented", "harvester",
         {"a region with a piezoelectric set (d31, eps33T, poling)"}, {"piezoelectric", "ferroelectric"},
         "a lumped single-mode cantilever with a resistive load (mechanics/PiezoelectricBeam)"},
        {"piezoelectric-converse", "Converse piezoelectric effect", "electrical", "mechanical", "implemented", "harvester",
         {"a region with a piezoelectric set (d31, eps33T, poling)"}, {"piezoelectric", "ferroelectric"}, "the tip deflection of the cantilever under a drive voltage"},
        {"thermal-conduction", "Heat conduction", "thermal", "thermal", "implemented", "thermal", {"thermal.thermalConductivity"}, {},
         "transport through the thickness, with contact resistances (not a conversion)"},
        {"thermal-expansion", "Thermal expansion", "thermal", "mechanical", "implemented", "laminate",
         {"thermal.thermalExpansion", "the elastic constants"}, {}, "the static free thermal response of a laminate: curvature and mismatch stresses"},
        {"mechanical-damping", "Mechanical damping", "mechanical", "thermal", "implemented", "harvester", {"mechanical.lossFactor or a damping ratio"}, {},
         "viscous modal damping of the cantilever"},
        {"acoustic-transmission", "Acoustic transmission across an interface", "acoustic", "acoustic", "implemented", "interfaces",
         {"acoustic.density and acoustic.soundSpeed, or the elastic constants"}, {}, "normal incidence, 4 Z1 Z2 / (Z1 + Z2)^2"},
        {"em-absorption", "Electromagnetic absorption", "electromagnetic", "thermal", "implemented", "electromagnetic",
         {"electromagnetic.lossTangent or electromagnetic.conductivity"}, {}, "the layered plane-wave solver; in the coupled engine, em.absorption-to-heat"},
        {"thermoelectric", "Thermoelectric (Seebeck) effect", "thermal", "electrical", "not-implemented", "", {"Seebeck coefficient", "electrical and thermal conductivity"},
         {"thermoelectric"}, "Resonant has no thermoelectric model and the library no Seebeck coefficient"},
        {"pyroelectric", "Pyroelectric effect", "thermal", "electrical", "not-implemented", "", {"pyroelectric coefficient"}, {"ferroelectric"}, "not modeled"},
        {"magnetostrictive", "Magnetostriction", "magnetic", "mechanical", "not-implemented", "", {"magnetostrictive coefficients", "permeability"},
         {"magnetostrictive"}, "not modeled"},
        {"electromagnetic-induction", "Electromagnetic induction (magnet and coil)", "mechanical", "electrical", "not-implemented", "",
         {"remanence", "coil geometry"}, {"magnetic", "conductor"}, "not modeled"},
        {"triboelectric", "Triboelectric effect", "mechanical", "electrical", "not-implemented", "", {"surface charge density"}, {"polymer"}, "not modeled"},
        {"electrostatic-electret", "Electret transduction", "mechanical", "electrical", "not-implemented", "", {"electret surface potential"}, {"polymer"}, "not modeled"},
        {"photovoltaic", "Photovoltaic effect", "optical", "electrical", "not-implemented", "", {"band gap", "absorption spectrum", "carrier lifetimes"},
         {"semiconductor"}, "not modeled"},
        {"electroluminescence", "Electroluminescence", "electrical", "optical", "not-implemented", "", {"quantum efficiency"}, {"semiconductor"}, "not modeled"},
        {"electrochemical", "Electrochemical conversion", "chemical", "electrical", "not-implemented", "", {"electrode potentials", "kinetics"}, {}, "not modeled"},
        {"joule-heating", "Joule heating (direct current)", "electrical", "thermal", "not-implemented", "", {"electromagnetic.conductivity"}, {"conductor"},
         "direct-current heating is not modeled; alternating-field conduction losses are part of em-absorption"},
        {"flow-induced-vibration", "Flow-induced vibration", "fluid", "mechanical", "not-implemented", "", {"flow field"}, {}, "Resonant has no fluid-dynamics domain"},
    };
    // The coupled engine's catalogue, with its own statuses.
    for (const auto& d : couplingModels()) {
        Mechanism m;
        m.id = "engine:" + d.id;
        m.title = d.name;
        m.from = d.sourceDomain;
        m.to = d.targetDomain;
        m.status = d.status == "validated" ? "engine-validated" : d.status == "experimental" ? "engine-experimental" : d.status == "external" ? "engine-external" : "not-implemented";
        m.model = d.id;
        m.needs = {d.sourceQuantity};
        m.note = d.mechanism + (d.limitations.empty() ? "" : " (" + d.limitations + ")");
        out.push_back(m);
    }
    return out;
}

bool conductive(const Architecture& a, const FunctionalRegion& r, const MaterialLibrary& lib, double threshold)
{
    const ResolvedProperty s = resolve(a, r, "electromagnetic.conductivity", 0.5, lib);
    return s.ok() && s.value > threshold;
}

json finding(const std::string& statement, const std::string& basis)
{
    return {{"statement", statement}, {"basis", basis}};
}

}  // namespace

json Mechanism::toJson() const
{
    return {{"id", id}, {"title", title}, {"from", from}, {"to", to}, {"status", status}, {"model", model}, {"requires", needs}, {"materialClasses", materialClasses},
            {"note", note}, {"implemented", implemented()}};
}

const std::vector<Mechanism>& mechanisms()
{
    static const std::vector<Mechanism> list = buildMechanisms();
    return list;
}

const Mechanism* findMechanism(const std::string& id)
{
    for (const auto& m : mechanisms())
        if (m.id == id)
            return &m;
    return nullptr;
}

json mechanismsFor(const std::string& from, const std::string& to, const Architecture* a, const Project& project)
{
    auto candidates = [&](const Mechanism& m) {
        json regions = json::array(), materials = json::array();
        if (a)
            for (const auto& r : a->regions) {
                if ((m.id == "piezoelectric-direct" || m.id == "piezoelectric-converse") && r.anisotropic.count("piezoelectric"))
                    regions.push_back(r.id);
                if (m.id == "em-absorption") {
                    const ResolvedProperty t = resolve(*a, r, "electromagnetic.lossTangent", 0.5, project.materials);
                    const ResolvedProperty s = resolve(*a, r, "electromagnetic.conductivity", 0.5, project.materials);
                    if ((t.ok() && t.value > 0.0) || (s.ok() && s.value > 0.0))
                        regions.push_back(r.id);
                }
            }
        if (m.id == "em-absorption")
            for (const auto& mat : project.materials.materials()) {
                const PropertyValue* t = mat.property(PhysicsDomain::Electromagnetic, "lossTangent");
                const PropertyValue* s = mat.property(PhysicsDomain::Electromagnetic, "conductivity");
                if ((t && t->value > 0.0) || (s && s->value > 0.0))
                    materials.push_back(mat.id().str());
            }
        json j = m.toJson();
        j["regions"] = regions;
        j["libraryMaterials"] = materials;
        if ((m.id == "piezoelectric-direct" || m.id == "piezoelectric-converse") && regions.empty())
            j["gap"] = "no region states piezoelectric coefficients: the library holds none (they are stated on regions, with their source)";
        return j;
    };
    json direct = json::array(), cascades = json::array();
    for (const auto& m : mechanisms())
        if (m.from == from && m.to == to)
            direct.push_back(candidates(m));
    for (const auto& m1 : mechanisms())
        for (const auto& m2 : mechanisms())
            if (m1.from == from && m1.to != from && m1.to == m2.from && m2.to == to && m1.to != to)
                cascades.push_back({{"stages", json::array({candidates(m1), candidates(m2)})},
                                    {"implemented", m1.implemented() && m2.implemented()},
                                    {"through", m1.to}});
    std::stable_sort(cascades.begin(), cascades.end(), [](const json& x, const json& y) { return x["implemented"].get<bool>() > y["implemented"].get<bool>(); });
    const bool any = std::any_of(direct.begin(), direct.end(), [](const json& m) { return m["implemented"].get<bool>(); }) ||
                     std::any_of(cascades.begin(), cascades.end(), [](const json& c) { return c["implemented"].get<bool>(); });
    return {{"from", from},
            {"to", to},
            {"direct", direct},
            {"cascades", cascades},
            {"modeled", any},
            {"note", any ? "candidate mechanisms: each is a hypothesis until an architecture is evaluated and measured"
                         : "no implemented mechanism or two-stage cascade connects these domains in Resonant"}};
}

json explore(const Architecture& a, const Project& project)
{
    const MaterialLibrary& lib = project.materials;
    json analyses = json::array(), findings = json::array(), hypotheses = json::array(), missing = json::array(), suggestions = json::array(),
         pathways = json::array();
    std::set<std::string> missingSeen;
    for (const auto& i : validate(a, &project))
        if (i.code == "undefined-interface" || i.code == "no-interface-model")
            missing.push_back(i.message);
    for (const auto& s : a.stacks) {
        for (const auto& cap : matterCapabilities()) {
            const Compatibility c = checkStack(a, s, cap, lib);
            json refusals = json::array();
            for (const auto& i : c.issues)
                if (i.severity == "refused") {
                    refusals.push_back(i.message);
                    if (i.code == "missing-property" && missingSeen.insert(i.message).second)
                        missing.push_back(i.message);
                }
            analyses.push_back({{"stack", s.id}, {"analysis", cap.id}, {"title", cap.title}, {"applicable", c.compatible}, {"refusals", refusals}});
        }
        // Where the transduction layers sit with respect to the neutral surface.
        auto lam = analyzeStack(a, s.id, "laminate", json::object(), lib);
        if (lam) {
            const double H = s.thickness();
            const double zn = lam.value()["metrics"]["neutralSurface"].get<double>() + H / 2.0;  // from the bottom
            double z = 0.0;
            for (const auto& l : s.layers) {
                const FunctionalRegion* r = a.region(l.region);
                const double centre = z + l.thickness / 2.0;
                z += l.thickness;
                if (!r || !r->anisotropic.count("piezoelectric"))
                    continue;
                const double d = centre - zn;
                if (std::abs(d) < 0.1 * H)
                    findings.push_back(finding("the transduction layer '" + l.id + "' lies " + number(std::abs(d) * 1e6) + " um from the neutral surface (" +
                                                   number(100.0 * std::abs(d) / H) + " % of the thickness): bending strains it little, so it converts little",
                                               "lamination theory (the neutral surface of the stack)"));
                else
                    findings.push_back(finding("the transduction layer '" + l.id + "' lies " + number(d * 1e6) + " um from the neutral surface: bending strains it",
                                               "lamination theory (the neutral surface of the stack)"));
            }
        }
        // The harvester, when the stack can be one.
        auto harvester = analyzeStack(a, s.id, "harvester", json::object(), lib);
        if (harvester) {
            const json& m = harvester.value()["metrics"];
            findings.push_back(finding("as a cantilever, the stack '" + s.id + "' delivers " + number(m["power"].get<double>() * 1e6) + " uW at 1 m/s^2 and " +
                                           number(m["shortCircuitHz"].get<double>()) + " Hz into " + number(m["load"].get<double>() / 1e3) + " kOhm (efficiency " +
                                           number(100.0 * m["efficiency"].get<double>()) + " %)",
                                       "the lumped piezoelectric cantilever (a computed result)"));
            for (const auto& w : harvester.value()["warnings"])
                if (w.get<std::string>().find("opposite signs") != std::string::npos)
                    findings.push_back(finding(w.get<std::string>(), "the signs of the layers' coupling coefficients"));
        }
        // Electrodes, leakage and dielectric loss around transduction layers.
        for (size_t i = 0; i < s.layers.size(); ++i) {
            const FunctionalRegion* r = a.region(s.layers[i].region);
            if (!r)
                continue;
            const ResolvedProperty tand = resolve(a, *r, "electromagnetic.lossTangent", 0.5, lib);
            if ((r->role == "transduction" || r->role == "isolation") && tand.ok() && tand.value > 0.05)
                findings.push_back(finding("the " + r->role + " region '" + r->id + "' has a loss tangent of " + number(tand.value) +
                                               ": dielectric loss takes a share of the converted energy",
                                           "the property " + tand.source));
            if (r->role == "isolation" && conductive(a, *r, lib, 1e-6))
                findings.push_back(finding("the isolation region '" + r->id + "' conducts (" + number(resolve(a, *r, "electromagnetic.conductivity", 0.5, lib).value) +
                                               " S/m): a leakage path", "its conductivity"));
            if (!r->anisotropic.count("piezoelectric"))
                continue;
            for (int side : {-1, +1}) {
                const long k = static_cast<long>(i) + side;
                if (k < 0 || k >= static_cast<long>(s.layers.size())) {
                    findings.push_back(finding("the transduction layer '" + s.layers[i].id + "' has no layer " + (side < 0 ? "below" : "above") +
                                                   " it: without an electrode there the charge cannot be collected",
                                               "the layer order of the stack"));
                    continue;
                }
                const FunctionalRegion* n = a.region(s.layers[static_cast<size_t>(k)].region);
                if (!n || n->role == "electrode")
                    continue;
                if (conductive(a, *n, lib, 1e2))
                    hypotheses.push_back(finding("the conductive " + (n->role.empty() ? std::string("layer") : n->role) + " '" + n->id + "' next to '" + r->id +
                                                     "' acts as its electrode (if intended, give it the electrode role; if not, it shorts or loads the film)",
                                                 "its conductivity and position"));
                else
                    findings.push_back(finding("the transduction layer '" + s.layers[i].id + "' has no electrode " + (side < 0 ? "below" : "above") +
                                                   " it ('" + n->id + "' insulates): the charge cannot be collected there",
                                               "the roles and conductivities of the neighbouring layers"));
            }
        }
        // Thermal mismatch at the operating temperatures.
        double hot = 293.15, cold = 293.15;
        for (const auto& r : a.regions)
            if (r.operating.contains("temperature") && r.operating["temperature"].is_array() && r.operating["temperature"].size() == 2) {
                cold = std::min(cold, r.operating["temperature"][0].get<double>());
                hot = std::max(hot, r.operating["temperature"][1].get<double>());
            }
        const double dT = std::abs(hot - 293.15) > std::abs(cold - 293.15) ? hot - 293.15 : cold - 293.15;
        if (dT != 0.0) {
            auto t = analyzeStack(a, s.id, "laminate", {{"deltaT", dT}}, lib);
            if (t)
                for (const json& e : t.value()["result"]["thermal"]["interfaces"]) {
                    const std::string id = e.value("interface", std::string());
                    const Interface* inter = nullptr;
                    for (const auto& x : a.interfaces)
                        if (x.id == id)
                            inter = &x;
                    const double stress = e["mismatchStress"].get<double>();
                    const InterfaceModel* cz = inter ? inter->model("cohesive-zone") : nullptr;
                    if (cz && stress > cz->parameter("strength")->value)
                        findings.push_back(finding("at " + number(293.15 + dT) + " K the mismatch stress next to '" + id + "' (" + number(stress / 1e6) +
                                                       " MPa) exceeds its strength (" + number(cz->parameter("strength")->value / 1e6) + " MPa): delamination is likely",
                                                   "lamination theory and the stated cohesive strength (a screening: edge stresses are higher)"));
                    else if (!cz && stress > 1e6)
                        hypotheses.push_back(finding("at " + number(293.15 + dT) + " K the mismatch stress next to '" + id + "' reaches " + number(stress / 1e6) +
                                                         " MPa; the interface's strength is not stated, so delamination cannot be excluded",
                                                     "lamination theory at the stated operating temperatures"));
                }
        }
    }
    // Manufacturing, with the stresses built in at the process temperatures.
    std::vector<ResidualStress> residual;
    for (const json& r : processResidualStresses(a, lib))
        if (r.contains("stress"))
            residual.push_back({r["interface"].get<std::string>(), r["layer"].get<std::string>(), r["stress"].get<double>(), r["temperature"].get<double>(),
                                r["basis"].get<std::string>()});
    const ManufacturingCheck mc = checkManufacturing(a, residual);
    for (const auto& v : mc.violations)
        findings.push_back(finding("manufacturing: " + v.message, "the stated process constraints"));
    for (const auto& u : mc.unassessed)
        missing.push_back("manufacturing: " + u.message);
    // Pathways.
    for (const auto& f : a.functions)
        for (const auto& st : f.stages) {
            const Mechanism* m = findMechanism(st.mechanism);
            const std::string status = m ? m->status : "unknown";
            pathways.push_back({{"function", f.id}, {"stage", st.id}, {"from", st.from}, {"to", st.to}, {"mechanism", st.mechanism}, {"status", status}});
            if (!m || !m->implemented())
                findings.push_back(finding("the stage '" + st.id + "' of '" + f.id + "' (" + st.mechanism + ") is not modeled in Resonant: the pathway cannot be evaluated end to end",
                                           "the mechanism registry"));
            if (m && m->implemented() && m->from != st.from)
                findings.push_back(finding("the stage '" + st.id + "' names " + st.from + " -> " + st.to + " but " + st.mechanism + " converts " + m->from + " -> " + m->to,
                                           "the mechanism registry"));
        }
    for (const auto& f : a.functions)
        if (f.targets.contains("frequency") && f.targets["frequency"].is_number())
            for (const auto& s : a.stacks) {
                auto modes = analyzeStack(a, s.id, "modes", json::object(), lib);
                if (!modes)
                    continue;
                const double f1 = modes.value()["metrics"]["f1"].get<double>(), target = f.targets["frequency"].get<double>();
                findings.push_back(finding("the first mode of '" + s.id + "' is at " + number(f1) + " Hz against the target of " + number(target) + " Hz for '" + f.id +
                                               "' (" + number(100.0 * (f1 / target - 1.0)) + " %)",
                                           "the modes of the coated structure"));
            }
    if (a.representation == "idealized")
        hypotheses.push_back(finding("the architecture is an idealized model: every prediction here is a hypothesis until it is measured",
                                     "the architecture's representation"));
    for (const json& m : missing)
        suggestions.push_back("state or measure: " + m.get<std::string>());
    for (const auto& i : a.interfaces) {
        if (!i.model("cohesive-zone") && !i.adhesionEnergy)
            suggestions.push_back("measure the strength of the interface '" + i.id + "' (a peel, scratch or four-point bending test) to judge its delamination risk");
        if (const InterfaceModel* t = i.model("thermal-contact-resistance"); t && t->parameter("resistance")->provenance != "measured")
            suggestions.push_back("calibrate the contact resistance of '" + i.id +
                                  "' from a through-thickness temperature measurement (the laboratory's matter.stack-metric model), then validate it on another sample");
    }
    for (const auto& r : a.regions)
        if (const auto it = r.anisotropic.find("piezoelectric"); it != r.anisotropic.end())
            if (const SourcedValue* d = it->second.value("d31"); d && d->provenance != "measured")
                suggestions.push_back("measure the d31 of '" + r.id + "' on the deposited film: thin films differ from bulk values (" + d->provenance + " here)");
    return {{"architecture", a.id},
            {"revision", revision(a)},
            {"analyses", analyses},
            {"pathways", pathways},
            {"findings", findings},
            {"hypotheses", hypotheses},
            {"missing", missing},
            {"suggestions", suggestions},
            {"manufacturing", mc.toJson()},
            {"disclaimer", "Findings come from the records and computations named with them; hypotheses are possible explanations to test, not results. "
                           "Nothing was changed."}};
}

Result<json> evaluatePathway(const Architecture& a, const std::string& functionId, const json& options, const MaterialLibrary& lib)
{
    const Function* f = a.function(functionId);
    if (!f)
        return Result<json>::failure("the architecture has no function '" + functionId + "'");
    if (f->stages.empty())
        return Result<json>::failure("the function '" + functionId + "' declares no stages");
    json stages = json::array();
    bool complete = true, creates = false;
    double chain = 1.0;
    for (const auto& st : f->stages) {
        json settings = st.settings;
        if (options.contains(st.id) && options[st.id].is_object())
            settings.update(options[st.id]);
        const Mechanism* m = findMechanism(st.mechanism);
        json e{{"stage", st.id}, {"from", st.from}, {"to", st.to}, {"mechanism", st.mechanism}};
        if (!m || !m->implemented()) {
            e["status"] = "not modeled";
            e["reason"] = m ? m->note : "the mechanism is not in the registry";
            complete = false;
            stages.push_back(e);
            continue;
        }
        const std::string stack = settings.value("stack", a.stacks.empty() ? std::string() : a.stacks.front().id);
        auto run = [&](const std::string& analysis, const json& o) { return analyzeStack(a, stack, analysis, o, lib); };
        if (m->id == "piezoelectric-direct") {
            auto r = run("harvester", settings);
            if (!r) {
                e["status"] = "not evaluated";
                e["reason"] = r.message();
                complete = false;
            } else {
                const json& x = r.value()["metrics"];
                e["status"] = "evaluated";
                e["input"] = x["inputPower"];
                e["output"] = x["power"];
                e["stored"] = 0.0;
                e["dissipated"] = x["dissipatedPower"];
                e["efficiency"] = x["efficiency"];
                e["unit"] = "W";
                e["balance"] = r.value()["result"]["balance"];
                e["operatingPoint"] = r.value()["result"]["point"];
                chain *= x["efficiency"].get<double>();
            }
        } else if (m->id == "em-absorption") {
            json o = settings;
            if (!o.contains("frequencies") && o.contains("frequency"))
                o["frequencies"] = json::array({o["frequency"]});
            auto r = run("electromagnetic", o);
            if (!r) {
                e["status"] = "not evaluated";
                e["reason"] = r.message();
                complete = false;
            } else {
                const double intensity = settings.value("incidentIntensity", 1.0);
                const json& x = r.value()["metrics"];
                const double A = x["absorptance"].get<double>(), R = x["reflectance"].get<double>(), T = x["transmittance"].get<double>();
                // The stage's ledger per unit area: incident = absorbed + reflected + transmitted.
                accounting::Ledger l;
                l.id = "absorption-stage";
                l.basis = accounting::Basis::Rate;
                l.per = "m^2";
                l.entities = {{"wave", "Incident wave", accounting::EntityKind::Source, "electromagnetic", "", "", "", "", ""},
                              {"stack", "Stack", accounting::EntityKind::Body, "thermal", "", "", "", "", ""},
                              {"heat", "Heat in the stack", accounting::EntityKind::Sink, "thermal", "", "", "", "", ""},
                              {"field", "Reflected and transmitted waves", accounting::EntityKind::Environment, "electromagnetic", "", "", "", "", ""}};
                auto ex = [](const char* id, const char* from, const char* to, const char* cat, double amount) {
                    accounting::Exchange item;
                    item.id = id;
                    item.from = from;
                    item.to = to;
                    item.category = cat;
                    item.amount = amount;
                    return item;
                };
                l.exchanges = {ex("incident", "wave", "stack", "radiation", intensity), ex("absorbed", "stack", "heat", "transfer", A * intensity),
                               ex("not-absorbed", "stack", "field", "radiation", (R + T) * intensity)};
                accounting::AccountingSystem sys;
                sys.id = "stack";
                sys.members = {"stack"};
                const auto report = accounting::evaluate(l, sys, accounting::Tolerances{});
                e["status"] = "evaluated";
                e["input"] = intensity;
                e["output"] = A * intensity;
                e["stored"] = 0.0;
                e["dissipated"] = 0.0;
                e["notConverted"] = (R + T) * intensity;
                e["efficiency"] = A;
                e["unit"] = "W/m^2";
                e["balance"] = std::string(accounting::toString(accounting::worst(report.checks)));
                chain *= A;
            }
        } else if (m->id == "thermal-conduction") {
            auto r = run("thermal", settings);
            if (!r) {
                e["status"] = "not evaluated";
                e["reason"] = r.message();
                complete = false;
            } else {
                const json& x = r.value()["metrics"];
                e["status"] = "evaluated";
                if (x.contains("heatFlux")) {
                    const double q = x["heatFlux"].get<double>();
                    e["input"] = q;
                    e["output"] = q;
                    e["unit"] = "W/m^2";
                    if (settings.contains("temperatures")) {
                        const double th = settings["temperatures"][0].get<double>(), tc = settings["temperatures"][1].get<double>();
                        e["entropyGeneration"] = q * (1.0 / tc - 1.0 / th);  // W/(m^2 K), never negative for heat flowing downhill
                    }
                }
                e["efficiency"] = 1.0;
                e["note"] = "transport, not a conversion: the heat passes through unchanged in amount";
            }
        } else {
            e["status"] = "evaluated (static)";
            e["note"] = "the mechanism has no energy throughput model here: " + m->note;
            complete = false;
        }
        if (e.contains("efficiency") && e["efficiency"].get<double>() > 1.0 + 1e-9)
            creates = true;
        stages.push_back(e);
    }
    json out{{"function", functionId}, {"stages", stages}, {"complete", complete}};
    if (complete && !creates) {
        out["chainEfficiency"] = chain;
        out["note"] = "the product of the stage efficiencies, each at its stated operating point: the stages are evaluated one by one, not coupled";
    } else {
        out["chainEfficiency"] = nullptr;
        out["note"] = creates ? "a stage reports more output than input: the result is rejected (no stage may create energy)"
                              : "the pathway is incomplete: a chain efficiency is not given";
    }
    out["violatesConservation"] = creates;
    return out;
}

}  // namespace resonant::coupling
