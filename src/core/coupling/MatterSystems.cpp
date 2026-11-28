#include "coupling/MatterSystems.h"

#include "coupling/MatterAnalyses.h"
#include "materials/Material.h"
#include "matter/PropertyResolution.h"

#include <algorithm>
#include <cmath>

namespace resonant::coupling {

using nlohmann::json;
using namespace resonant::matter;

namespace {

std::string lawProvenance(const ResolvedProperty& p)
{
    if (p.origin == "composition")
        return "homogenized";
    return p.provenance.empty() || p.provenance == "unstated" ? "illustrative" : p.provenance;
}

bool graded(const Architecture& a, const std::string& region)
{
    return std::any_of(a.fields.begin(), a.fields.end(), [&](const PropertyField& f) { return f.region == region; });
}

}  // namespace

Result<MatterSystem> stackToPhysicalSystem(const Architecture& a, const std::string& stackId, const json& options, const MaterialLibrary& lib)
{
    using R = Result<MatterSystem>;
    const Stack* s = a.stack(stackId);
    if (!s)
        return R::failure("the architecture has no stack '" + stackId + "'");
    auto compat = checkMatterStack(a, stackId, "coupled-engine", lib);
    if (!compat)
        return R::failure(compat.message());
    if (!compat.value().compatible)
        return R::failure("refused by the Coupled Energy-Matter Engine:\n" + compat.value().summary());
    const json o = options.is_object() ? options : json::object();
    MatterSystem out;
    multiphysics::PhysicalSystem& sys = out.system;
    sys.id = o.value("id", "sys-" + a.id + "-" + s->id);
    sys.name = o.value("name", a.name + " / " + (s->name.empty() ? s->id : s->name));
    sys.description = "Generated from the architecture '" + a.id + "' (revision " + revision(a) + "), stack '" + s->id + "' (RES-FAM-013).";
    sys.geometryKind = "layered-slab";
    sys.lateralArea = o.value("lateralArea", s->length > 0.0 && s->width > 0.0 ? s->length * s->width : 0.01);
    sys.ambientTemperature = o.value("ambientTemperature", 293.15);
    const int cells = std::max(1, o.value("cellsPerLayer", 10));
    const int sublayers = std::max(1, o.value("sublayers", 10));
    // The domains: as given, or a thermal domain with convective faces (stated).
    json domains = o.value("domains", json::array());
    if (domains.empty()) {
        domains = json::array({{{"id", "heat"},
                                {"kind", "thermal"},
                                {"solver", "thermal.conduction.fv"},
                                {"settings", {{"faces", {{"-z", {{"kind", "convective"}, {"h", 10.0}}}, {"+z", {{"kind", "convective"}, {"h", 10.0}}}}}}}}});
        out.notes.push_back("a default thermal domain: both faces convective, h = 10 W/(m^2 K) to the ambient - edit the system for the real conditions");
    }
    bool em = false;
    for (const json& d : domains) {
        multiphysics::DomainDef def{d.value("id", std::string()), d.value("kind", std::string()), d.value("solver", std::string()),
                                    d.value("settings", json::object())};
        em = em || def.kind == "electromagnetic";
        if (def.kind != "thermal" && def.kind != "electromagnetic")
            return R::failure("the engine's layered slab takes thermal and electromagnetic domains from an architecture, not '" + def.kind + "'");
        sys.domains.push_back(def);
    }
    std::vector<std::string> keys{"thermal.thermalConductivity", "thermal.density", "thermal.specificHeat"};
    if (em)
        for (const char* k : {"electromagnetic.relativePermittivity", "electromagnetic.lossTangent", "electromagnetic.conductivity", "electromagnetic.relativePermeability"})
            keys.push_back(k);
    std::vector<std::pair<std::string, std::string>> boundaries;  // (top sub-region of layer i, bottom sub-region of layer i + 1)
    std::string previousTop;
    for (size_t i = 0; i < s->layers.size(); ++i) {
        const Layer& l = s->layers[i];
        const FunctionalRegion* r = a.region(l.region);
        if (!r)
            return R::failure("the layer '" + l.id + "' names no region");
        std::string base = r->materialId;
        if (base.empty() && !r->composition.empty()) {
            for (const auto& c : r->composition.constituents)
                if (!c.materialId.empty() && (base.empty() || c.id == r->composition.matrix))
                    base = c.materialId;
        }
        const Material* m = base.empty() ? nullptr : lib.find(Id(base));
        if (!m)
            return R::failure("the engine needs a library material for the region '" + r->id + "' (its laws refine it)");
        const int n = graded(a, r->id) ? sublayers : 1;
        for (int k = 0; k < n; ++k) {
            const double sPos = (k + 0.5) / n;
            multiphysics::RegionDef def;
            def.id = n == 1 ? l.id : l.id + "." + std::to_string(k);
            def.name = (r->name.empty() ? r->id : r->name) + (n == 1 ? "" : " (" + std::to_string(k + 1) + "/" + std::to_string(n) + ")");
            def.materialId = base;
            def.thickness = l.thickness / n;
            def.cells = std::max(1, cells / n);
            def.objectId = s->object;
            sys.regions.push_back(def);
            if (k == 0 && !previousTop.empty() && s->layers[i - 1].region != r->id)
                boundaries.emplace_back(previousTop, def.id);
            for (const auto& key : keys) {
                double value = 0.0;
                std::string provenance, source;
                if (key == "thermal.thermalConductivity" && r->anisotropic.count("thermal")) {
                    // The through-thickness component of the rotated principal conductivities.
                    auto props = analyzeStack(a, stackId, "thermal", {{"sublayers", 1}}, lib);
                    if (!props)
                        return R::failure(props.message());
                    value = props.value()["result"]["layers"][i]["conductivity"].get<double>();
                    provenance = "computed";
                    source = "the principal conductivities of '" + r->id + "' rotated into the stack frame";
                } else {
                    const ResolvedProperty p = resolve(a, *r, key, sPos, lib);
                    if (!p.ok())
                        return R::failure("the layer '" + l.id + "' lacks " + key + ": " + p.message);
                    if (p.origin == "material" && base == r->materialId)
                        continue;  // the library value: no law needed
                    value = p.value;
                    provenance = lawProvenance(p);
                    source = p.origin + (p.source.empty() ? "" : ": " + p.source);
                }
                multiphysics::PropertyLaw law;
                law.region = def.id;
                law.property = key;
                law.kind = multiphysics::LawKind::Constant;
                law.value = value;
                law.provenance = provenance;
                law.source = source;
                sys.laws.push_back(law);
            }
            previousTop = def.id;
        }
    }
    // Interfaces: thermal contact resistances to every thermal domain; electromagnetic
    // interfaces must be perfect.
    json interfaces = json::array();
    size_t bi = 0;
    for (size_t i = 0; i + 1 < s->layers.size(); ++i) {
        if (s->layers[i].region == s->layers[i + 1].region)
            continue;
        const Interface* in = a.interfaceBetween(s->layers[i].region, s->layers[i + 1].region);
        if (bi >= boundaries.size())
            break;
        const auto [below, above] = boundaries[bi++];
        if (!in)
            continue;
        if (em)
            if (const InterfaceModel* e = in->modelFor("electrical"); e && e->kind != "perfect")
                return R::failure("the " + e->kind + " model of '" + in->id + "' is not represented by the engine's electromagnetic participant");
        if (const InterfaceModel* t = in->modelFor("thermal"); t && t->kind == "thermal-contact-resistance")
            interfaces.push_back({{"regions", json::array({below, above})}, {"resistance", t->parameter("resistance")->value}, {"interface", in->id}});
    }
    if (!interfaces.empty())
        for (auto& d : sys.domains)
            if (d.kind == "thermal")
                d.settings["interfaces"] = interfaces;
    if (o.contains("couplings") && o["couplings"].is_array())
        for (const json& c : o["couplings"])
            sys.couplings.push_back({c.value("id", std::string()), c.value("model", std::string()), c.value("from", std::string()), c.value("to", std::string()),
                                     c.value("transfer", std::string())});
    if (o.contains("scheme") && o["scheme"].is_object()) {
        sys.scheme.kind = o["scheme"].value("kind", std::string("one-way"));
        sys.scheme.maxIterations = o["scheme"].value("maxIterations", 30);
        sys.scheme.temperatureTolerance = o["scheme"].value("temperatureTolerance", 1e-3);
    }
    out.notes.push_back(std::to_string(sys.laws.size()) + " constant laws carry the architecture's graded, effective and region values");
    if (!interfaces.empty())
        out.notes.push_back(std::to_string(interfaces.size()) + " thermal contact resistances on the thermal participant's interfaces");
    for (const auto& i : compat.value().issues)
        if (i.severity == "warning")
            out.notes.push_back(i.message);
    return out;
}

}  // namespace resonant::coupling
