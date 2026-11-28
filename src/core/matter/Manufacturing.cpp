#include "matter/Manufacturing.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>

namespace resonant::matter {

using nlohmann::json;

namespace {

std::string number(double v)
{
    std::ostringstream s;
    s.precision(4);
    s << v;
    return s.str();
}

std::string micro(double metres)
{
    return number(metres * 1e6) + " um";
}

std::optional<double> constraint(const Process& p, const char* key)
{
    if (p.constraints.contains(key) && p.constraints[key].is_number())
        return p.constraints[key].get<double>();
    return std::nullopt;
}

std::optional<double> limit(const FunctionalRegion& r, const char* key)
{
    if (r.limits.contains(key) && r.limits[key].is_number())
        return r.limits[key].get<double>();
    return std::nullopt;
}

}  // namespace

const std::vector<ProcessKind>& processKinds()
{
    static const std::vector<std::string> deposition{"minLayerThickness", "maxLayerThickness", "temperature", "materialClasses", "maxResidualStress",
                                                     "roughness", "repeatability"};
    static const std::vector<std::string> additive{"minLayerThickness", "minFeatureSize", "maxDimension", "temperature", "materialClasses", "roughness",
                                                   "maxResidualStress", "repeatability"};
    static const std::vector<std::string> patterning{"minFeatureSize", "maxDimension", "materialClasses", "repeatability"};
    static const std::vector<std::string> joining{"minLayerThickness", "maxLayerThickness", "temperature", "materialClasses", "maxResidualStress"};
    static const std::vector<std::string> thermal{"temperature", "maxDimension"};
    static const std::vector<std::string> subtractive{"minFeatureSize", "maxDimension", "roughness", "repeatability"};
    static const std::vector<ProcessKind> kinds{
        {"metal-additive", "Metal additive manufacturing", "additive", additive, "layer-wise fusion of metal feedstock"},
        {"polymer-additive", "Polymer additive manufacturing", "additive", additive, "extrusion, photopolymerization or sintering of polymers"},
        {"ceramic-additive", "Ceramic additive manufacturing", "additive", additive, "shaping of ceramic slurries or powders, then debinding and sintering"},
        {"multi-material-printing", "Multi-material printing", "additive", additive, "several materials deposited in one build"},
        {"directed-energy-deposition", "Directed-energy deposition", "additive", additive, "feedstock melted by a focused energy source as it is deposited"},
        {"powder-bed-fusion", "Powder-bed fusion", "additive", additive, "a laser or electron beam fusing a powder bed layer by layer"},
        {"sputter-deposition", "Sputter deposition", "deposition", deposition, "physical vapour deposition by sputtering a target"},
        {"atomic-layer-deposition", "Atomic layer deposition", "deposition", deposition, "self-limiting surface reactions, one monolayer per cycle"},
        {"chemical-vapor-deposition", "Chemical vapour deposition", "deposition", deposition, "a film grown from gaseous precursors"},
        {"physical-vapor-deposition", "Physical vapour deposition", "deposition", deposition, "evaporation or sputtering in vacuum"},
        {"thin-film-processing", "Thin-film processing", "deposition", deposition, "spin coating, sol-gel and similar solution processes, with their anneal"},
        {"thermal-spray", "Thermal spraying", "deposition", deposition, "plasma or high-velocity oxy-fuel spraying of molten particles (coatings)"},
        {"lithography", "Lithography", "patterning", patterning, "patterning through a mask or a direct write"},
        {"patterned-electrode-fabrication", "Patterned electrode fabrication", "patterning", patterning, "electrodes deposited through a mask or etched"},
        {"lamination", "Lamination", "joining", joining, "layers bonded under pressure, with an adhesive or a cure"},
        {"diffusion-bonding", "Diffusion bonding", "joining", joining, "solid-state joining under pressure at a high temperature"},
        {"sintering", "Sintering", "thermal", thermal, "densification of a powder compact at a high temperature"},
        {"heat-treatment", "Heat treatment", "thermal", thermal, "a temperature history that changes the microstructure"},
        {"machining", "Machining and finishing", "subtractive", subtractive, "material removed to shape and to the surface finish"},
        {"composite-fabrication", "Composite fabrication", "composite", joining, "lay-up, infusion and cure of fibre composites"},
    };
    return kinds;
}

const ProcessKind* findProcessKind(const std::string& id)
{
    for (const auto& k : processKinds())
        if (k.id == id)
            return &k;
    return nullptr;
}

json ManufacturingCheck::toJson() const
{
    auto list = [](const std::vector<Issue>& v) {
        json out = json::array();
        for (const auto& i : v)
            out.push_back(i.toJson());
        return out;
    };
    return {{"status", status}, {"violations", list(violations)}, {"unassessed", list(unassessed)}, {"notes", list(notes)}};
}

ManufacturingCheck checkManufacturing(const Architecture& a, const std::vector<ResidualStress>& residual)
{
    ManufacturingCheck out;
    auto violation = [&](const std::string& code, const std::string& element, const std::string& message) {
        out.violations.push_back({"error", code, element, message});
    };
    auto unassessed = [&](const std::string& code, const std::string& element, const std::string& message) {
        out.unassessed.push_back({"warning", code, element, message});
    };
    auto note = [&](const std::string& code, const std::string& element, const std::string& message) { out.notes.push_back({"info", code, element, message}); };
    for (const auto& p : a.processes)
        if (!findProcessKind(p.kind))
            violation("unknown-process", p.id, "the process '" + p.id + "' has an unknown kind '" + p.kind + "'");
    std::set<std::string> noProcess, noLimit;
    for (const auto& r : a.regions) {
        if (r.process.empty()) {
            noProcess.insert(r.id);
            continue;
        }
        const Process* p = a.process(r.process);
        if (!p)
            continue;
        if (p->constraints.contains("materialClasses") && p->constraints["materialClasses"].is_array()) {
            const json& classes = p->constraints["materialClasses"];
            if (r.materialClass.empty())
                unassessed("no-material-class", r.id, "the material class of '" + r.id + "' is not stated: its compatibility with " + p->name + " is not assessed");
            else if (std::none_of(classes.begin(), classes.end(), [&](const json& c) { return c.is_string() && c.get<std::string>() == r.materialClass; }))
                violation("incompatible-material", r.id, p->name + " does not process " + r.materialClass + " (the region '" + r.id + "')");
        }
        if (r.extent == "subvolume")
            if (const auto f = constraint(*p, "minFeatureSize")) {
                const json mn = r.subvolume.value("min", json::array()), mx = r.subvolume.value("max", json::array());
                if (mn.size() == 3 && mx.size() == 3) {
                    double smallest = 1e300;
                    for (size_t k = 0; k < 3; ++k)
                        smallest = std::min(smallest, mx[k].get<double>() - mn[k].get<double>());
                    if (smallest < *f)
                        violation("feature-too-small", r.id,
                                  "the region '" + r.id + "' is " + micro(smallest) + " across, below the minimum feature of " + p->name + " (" + micro(*f) + ")");
                }
            }
    }
    for (const auto& s : a.stacks) {
        for (size_t i = 0; i < s.layers.size(); ++i) {
            const Layer& l = s.layers[i];
            const FunctionalRegion* r = a.region(l.region);
            if (!r || r->process.empty())
                continue;
            const Process* p = a.process(r->process);
            if (!p)
                continue;
            if (const auto mn = constraint(*p, "minLayerThickness"); mn && l.thickness < *mn)
                violation("layer-too-thin", l.id, "the layer '" + l.id + "' is " + micro(l.thickness) + " thick, below the minimum of " + p->name + " (" + micro(*mn) + ")");
            if (const auto mx = constraint(*p, "maxLayerThickness"); mx && l.thickness > *mx)
                violation("layer-too-thick", l.id, "the layer '" + l.id + "' is " + micro(l.thickness) + " thick, above the maximum of " + p->name + " (" + micro(*mx) + ")");
            if (const auto f = constraint(*p, "minFeatureSize")) {
                if (l.pattern.featureSize > 0.0 && l.pattern.featureSize < *f)
                    violation("feature-too-small", l.id,
                              "the pattern of '" + l.id + "' has " + micro(l.pattern.featureSize) + " features, below the minimum of " + p->name + " (" + micro(*f) + ")");
                if (l.pattern.gap > 0.0 && l.pattern.gap < *f)
                    violation("feature-too-small", l.id,
                              "the gaps of '" + l.id + "' are " + micro(l.pattern.gap) + ", below the minimum feature of " + p->name + " (" + micro(*f) + ")");
            }
            if (const auto d = constraint(*p, "maxDimension"); d && std::max(s.length, s.width) > *d)
                violation("too-large", l.id, "the stack '" + s.id + "' is larger than " + p->name + " can make (" + number(*d) + " m)");
            if (const auto rep = constraint(*p, "repeatability"); rep && l.thicknessUncertainty && *l.thicknessUncertainty < *rep * l.thickness)
                note("tolerance-tighter-than-process", l.id,
                     "the stated thickness tolerance of '" + l.id + "' is tighter than the repeatability of " + p->name + " (" + number(*rep * 100.0) + " %)");
            if (const auto t = constraint(*p, "temperature")) {
                // The process temperature against the region it makes and every layer made before it.
                for (size_t k = 0; k <= i; ++k) {
                    const FunctionalRegion* earlier = a.region(s.layers[k].region);
                    if (!earlier)
                        continue;
                    if (const auto lim = limit(*earlier, "maxTemperature")) {
                        if (*t > *lim)
                            violation("process-too-hot", s.layers[k].id,
                                      p->name + " at " + number(*t) + " K exceeds the limit of '" + earlier->id + "' (" + number(*lim) + " K)" +
                                          (k < i ? ", which is made before it" : ""));
                    } else if (noLimit.insert(earlier->id).second) {
                        unassessed("no-temperature-limit", earlier->id,
                                   "the region '" + earlier->id + "' states no temperature limit: " + p->name + " at " + number(*t) + " K is not checked against it");
                    }
                }
            }
            if (i > 0)
                if (const FunctionalRegion* below = a.region(s.layers[i - 1].region); below && !below->process.empty())
                    if (const Process* bp = a.process(below->process))
                        if (const auto rough = constraint(*bp, "roughness"); rough && *rough > 0.1 * l.thickness)
                            note("rough-substrate", l.id,
                                 "the layer '" + l.id + "' (" + micro(l.thickness) + ") is less than ten times the roughness of the surface below it (" + micro(*rough) +
                                     "): its continuity is at risk (a screening rule)");
        }
    }
    for (const auto& rs : residual) {
        const Interface* in = nullptr;
        for (const auto& i : a.interfaces)
            if (i.id == rs.interface)
                in = &i;
        if (!in)
            continue;
        const FunctionalRegion* made = nullptr;
        for (const auto& s : a.stacks)
            for (const auto& l : s.layers)
                if (l.id == rs.layer)
                    made = a.region(l.region);
        if (made && !made->process.empty())
            if (const Process* p = a.process(made->process))
                if (const auto lim = constraint(*p, "maxResidualStress"); lim && std::abs(rs.stress) > *lim)
                    violation("residual-stress", in->id,
                              "the mismatch stress next to '" + in->id + "' after " + p->name + " (" + number(rs.stress / 1e6) + " MPa) exceeds its limit of " +
                                  number(*lim / 1e6) + " MPa");
        if (const InterfaceModel* m = in->model("cohesive-zone"))
            if (const SourcedValue* st = m->parameter("strength"); st && std::abs(rs.stress) > st->value)
                violation("delamination-risk", in->id,
                          "the mismatch stress next to '" + in->id + "' (" + number(rs.stress / 1e6) + " MPa) exceeds the interface strength (" + number(st->value / 1e6) +
                              " MPa): a delamination risk (" + rs.basis + ")");
    }
    for (const auto& r : noProcess)
        unassessed("no-process", r, "the region '" + r + "' names no process: how it is made is not assessed");
    for (const auto& f : a.fields)
        if (!f.realizable)
            unassessed("idealized-field", f.id, "the field '" + f.id + "' is an idealized distribution: no process is shown to produce it");
    if (a.representation == "idealized")
        note("idealized", a.id, "the architecture is an idealized model; feasibility here means only that the stated constraints hold");
    out.status = !out.violations.empty() ? "infeasible" : !out.unassessed.empty() ? "not-assessed" : "feasible";
    return out;
}

}  // namespace resonant::matter
