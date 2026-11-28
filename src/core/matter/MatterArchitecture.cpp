#include "matter/MatterArchitecture.h"

#include "laboratory/Sha256.h"
#include "materials/Material.h"
#include "project/Project.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace resonant::matter {

using nlohmann::json;

namespace {

template <typename T>
json listOf(const std::vector<T>& v)
{
    json out = json::array();
    for (const auto& x : v)
        out.push_back(x);
    return out;
}

std::vector<std::string> strings(const json& j, const char* key)
{
    std::vector<std::string> out;
    if (j.contains(key) && j[key].is_array())
        for (const json& x : j[key])
            if (x.is_string())
                out.push_back(x.get<std::string>());
    return out;
}

json objectOr(const json& j, const char* key)
{
    return j.contains(key) && j[key].is_object() ? j[key] : json::object();
}

json valuesJson(const std::map<std::string, SourcedValue>& m)
{
    json out = json::object();
    for (const auto& [k, v] : m)
        out[k] = v.toJson();
    return out;
}

Result<std::map<std::string, SourcedValue>> valuesFrom(const json& j, const std::string& where)
{
    using R = Result<std::map<std::string, SourcedValue>>;
    std::map<std::string, SourcedValue> out;
    if (j.is_null())
        return out;
    if (!j.is_object())
        return R::failure(where + ": the values are an object of key -> value");
    for (const auto& [k, v] : j.items()) {
        auto s = SourcedValue::fromJson(v);
        if (!s)
            return R::failure(where + " '" + k + "': " + s.message());
        out[k] = s.value();
    }
    return out;
}

json optionalValue(const std::optional<SourcedValue>& v)
{
    return v ? v->toJson() : json(nullptr);
}

Result<std::optional<SourcedValue>> optionalFrom(const json& j, const char* key)
{
    using R = Result<std::optional<SourcedValue>>;
    if (!j.contains(key) || j[key].is_null())
        return std::optional<SourcedValue>{};
    auto s = SourcedValue::fromJson(j[key]);
    if (!s)
        return R::failure(std::string(key) + ": " + s.message());
    return std::optional<SourcedValue>(s.value());
}

}  // namespace

const std::vector<std::string>& provenanceKinds()
{
    static const std::vector<std::string> kinds{"measured",     "manufacturer", "literature", "first-principles", "simulation", "estimated",
                                                "assumed",      "theoretical",  "illustrative", "homogenized",   "computed",   "database",
                                                "fitted",       "surrogate",    "unstated"};
    return kinds;
}

bool isProvenance(const std::string& text)
{
    return std::find(provenanceKinds().begin(), provenanceKinds().end(), text) != provenanceKinds().end();
}

json SourcedValue::toJson() const
{
    json j{{"value", value}, {"unit", unit}, {"provenance", provenance}, {"source", source}};
    j["uncertainty"] = uncertainty ? json(*uncertainty) : json(nullptr);
    return j;
}

Result<SourcedValue> SourcedValue::fromJson(const json& j)
{
    SourcedValue v;
    if (j.is_number()) {
        v.value = j.get<double>();
        return v;
    }
    if (!j.is_object() || !j.contains("value") || !j["value"].is_number())
        return Result<SourcedValue>::failure("a value is a number or {\"value\", \"unit\", \"provenance\", ...}");
    v.value = j["value"].get<double>();
    v.unit = j.value("unit", std::string());
    v.provenance = j.value("provenance", std::string("unstated"));
    v.source = j.value("source", std::string());
    if (j.contains("uncertainty") && j["uncertainty"].is_number())
        v.uncertainty = j["uncertainty"].get<double>();
    if (!std::isfinite(v.value))
        return Result<SourcedValue>::failure("a value that is not finite");
    if (!isProvenance(v.provenance))
        return Result<SourcedValue>::failure("an unknown provenance '" + v.provenance + "'");
    if (v.uncertainty && !(*v.uncertainty >= 0.0))
        return Result<SourcedValue>::failure("a negative uncertainty");
    return v;
}

// --- orientation ---------------------------------------------------------------------------------

std::array<std::array<double, 3>, 3> Orientation::rotation() const
{
    // Bunge ZXZ: R = Rz(phi1) Rx(Phi) Rz(phi2); columns are the material axes in the frame.
    const double c1 = std::cos(phi1), s1 = std::sin(phi1), c = std::cos(Phi), s = std::sin(Phi), c2 = std::cos(phi2), s2 = std::sin(phi2);
    return {{{c1 * c2 - s1 * c * s2, -c1 * s2 - s1 * c * c2, s1 * s},
             {s1 * c2 + c1 * c * s2, -s1 * s2 + c1 * c * c2, -c1 * s},
             {s * s2, s * c2, c}}};
}

bool Orientation::inPlane() const
{
    return std::abs(std::sin(Phi)) < 1e-12;
}

const SourcedValue* AnisotropicSet::value(const std::string& key) const
{
    auto it = values.find(key);
    return it == values.end() ? nullptr : &it->second;
}

const std::vector<std::string>& anisotropicKinds()
{
    static const std::vector<std::string> kinds{"elastic", "thermal", "expansion", "piezoelectric"};
    return kinds;
}

// --- interfaces ----------------------------------------------------------------------------------

const std::vector<InterfaceModelKind>& interfaceModelKinds()
{
    static const std::vector<InterfaceModelKind> kinds{
        {"perfect", "Perfect contact", "all", {}, "continuous displacement, temperature and potential across the interface (declared, never assumed)"},
        {"perfect-bonding", "Perfect bonding", "mechanical", {}, "no slip and no separation"},
        {"finite-stiffness", "Finite-stiffness bonding", "mechanical", {{"normalStiffness", "N/m^3"}, {"shearStiffness", "N/m^3"}},
         "a spring layer: traction proportional to the displacement jump"},
        {"cohesive-zone", "Cohesive zone", "mechanical", {{"strength", "Pa"}, {"fractureEnergy", "J/m^2"}},
         "bonded until the traction reaches the strength, then separating with the fracture energy"},
        {"frictional-contact", "Frictional contact", "mechanical", {{"frictionCoefficient", "1"}}, "Coulomb friction, no tension"},
        {"sliding", "Sliding contact", "mechanical", {{"normalStiffness", "N/m^3"}}, "frictionless contact without separation: no shear traction"},
        {"thermal-contact-resistance", "Thermal contact resistance", "thermal", {{"resistance", "m^2 K/W"}},
         "a temperature jump q R across the interface (thermal boundary or Kapitza resistance)"},
        {"electrical-contact-resistance", "Electrical contact resistance", "electrical", {{"resistance", "ohm m^2"}}, "a potential jump J R across the interface"},
        {"dielectric", "Dielectric interface", "electrical", {{"surfaceChargeDensity", "C/m^2"}}, "a jump of the normal displacement field by a surface charge"},
        {"charge-transfer", "Charge-transfer interface", "electrical", {{"barrierHeight", "V"}}, "a barrier to charge crossing the interface"},
        {"diffusive", "Diffusive interface", "chemical", {{"diffusionLength", "m"}}, "an interdiffusion zone of a stated width"},
        {"reactive", "Reactive interface", "chemical", {{"reactionLayerThickness", "m"}}, "a reaction product layer of a stated thickness"},
    };
    return kinds;
}

const InterfaceModelKind* findInterfaceModelKind(const std::string& id)
{
    for (const auto& k : interfaceModelKinds())
        if (k.id == id)
            return &k;
    return nullptr;
}

const SourcedValue* InterfaceModel::parameter(const std::string& key) const
{
    auto it = parameters.find(key);
    return it == parameters.end() ? nullptr : &it->second;
}

bool Interface::joins(const std::string& a, const std::string& b) const
{
    return (regions[0] == a && regions[1] == b) || (regions[0] == b && regions[1] == a);
}

const InterfaceModel* Interface::model(const std::string& kind) const
{
    for (const auto& m : models)
        if (m.kind == kind)
            return &m;
    return nullptr;
}

const InterfaceModel* Interface::modelFor(const std::string& physics) const
{
    for (const auto& m : models)
        if (const InterfaceModelKind* k = findInterfaceModelKind(m.kind); k && k->physics == physics)
            return &m;
    return model("perfect");
}

// --- stacks --------------------------------------------------------------------------------------

double Stack::thickness() const
{
    double t = 0.0;
    for (const auto& l : layers)
        t += l.thickness;
    return t;
}

int Stack::layerOf(const std::string& r) const
{
    for (size_t i = 0; i < layers.size(); ++i)
        if (layers[i].region == r)
            return static_cast<int>(i);
    return -1;
}

// --- the architecture ----------------------------------------------------------------------------

const FunctionalRegion* Architecture::region(const std::string& rid) const
{
    for (const auto& r : regions)
        if (r.id == rid)
            return &r;
    return nullptr;
}

FunctionalRegion* Architecture::region(const std::string& rid)
{
    for (auto& r : regions)
        if (r.id == rid)
            return &r;
    return nullptr;
}

const Stack* Architecture::stack(const std::string& sid) const
{
    for (const auto& s : stacks)
        if (s.id == sid)
            return &s;
    return nullptr;
}

Stack* Architecture::stack(const std::string& sid)
{
    for (auto& s : stacks)
        if (s.id == sid)
            return &s;
    return nullptr;
}

const Interface* Architecture::interfaceBetween(const std::string& a, const std::string& b) const
{
    for (const auto& i : interfaces)
        if (i.joins(a, b))
            return &i;
    return nullptr;
}

Interface* Architecture::interfaceById(const std::string& iid)
{
    for (auto& i : interfaces)
        if (i.id == iid)
            return &i;
    return nullptr;
}

const Process* Architecture::process(const std::string& pid) const
{
    for (const auto& p : processes)
        if (p.id == pid)
            return &p;
    return nullptr;
}

const Function* Architecture::function(const std::string& fid) const
{
    for (const auto& f : functions)
        if (f.id == fid)
            return &f;
    return nullptr;
}

const PropertyField* Architecture::field(const std::string& r, const std::string& property) const
{
    for (const auto& f : fields)
        if (f.region == r && f.property == property)
            return &f;
    return nullptr;
}

json Architecture::toJson() const
{
    json j{{"id", id}, {"name", name}, {"description", description}, {"representation", representation}, {"parent", parent},
           {"objects", listOf(objects)}, {"provenance", provenance}, {"note", note}};
    json rs = json::array();
    for (const auto& r : regions) {
        json cs = json::array();
        for (const auto& c : r.composition.constituents)
            cs.push_back({{"id", c.id}, {"materialId", c.materialId}, {"substance", c.substance}, {"formula", c.formula}, {"materialClass", c.materialClass},
                          {"fraction", c.fraction}, {"fractionUncertainty", c.fractionUncertainty ? json(*c.fractionUncertainty) : json(nullptr)}});
        json ps = json::array();
        for (const auto& p : r.phases)
            ps.push_back({{"id", p.id}, {"name", p.name}, {"materialId", p.materialId}, {"fraction", p.fraction}, {"distribution", p.distribution},
                          {"stability", p.stability}, {"metastable", p.metastable}, {"materialStateStudy", p.materialStateStudy}});
        json micro = nullptr;
        if (r.microstructure) {
            const Microstructure& m = *r.microstructure;
            micro = {{"crystalStructure", m.crystalStructure}, {"lattice", m.lattice}, {"grainSize", optionalValue(m.grainSize)},
                     {"grainSizeDistribution", m.grainSizeDistribution}, {"porosity", optionalValue(m.porosity)}, {"texture", m.texture},
                     {"defects", listOf(m.defects)}, {"inclusions", listOf(m.inclusions)}, {"domains", m.domains}, {"representation", m.representation},
                     {"provenance", m.provenance}, {"note", m.note}};
        }
        json aniso = json::object();
        for (const auto& [kind, set] : r.anisotropic)
            aniso[kind] = {{"values", valuesJson(set.values)}, {"poling", set.poling}};
        json orient = nullptr;
        if (r.orientation)
            orient = {{"phi1", r.orientation->phi1}, {"Phi", r.orientation->Phi}, {"phi2", r.orientation->phi2}, {"frame", r.orientation->frame}};
        rs.push_back({{"id", r.id},
                      {"name", r.name},
                      {"object", r.object},
                      {"extent", r.extent},
                      {"subvolume", r.subvolume},
                      {"role", r.role},
                      {"materialId", r.materialId},
                      {"materialClass", r.materialClass},
                      {"composition",
                       {{"kind", r.composition.kind},
                        {"basis", r.composition.basis},
                        {"constituents", cs},
                        {"method", r.composition.method},
                        {"matrix", r.composition.matrix},
                        {"note", r.composition.note}}},
                      {"phases", ps},
                      {"microstructure", micro},
                      {"orientation", orient},
                      {"anisotropic", aniso},
                      {"properties", valuesJson(r.properties)},
                      {"operating", r.operating},
                      {"limits", r.limits},
                      {"process", r.process},
                      {"functions", listOf(r.functions)},
                      {"note", r.note}});
    }
    j["regions"] = rs;
    json ss = json::array();
    for (const auto& s : stacks) {
        json ls = json::array();
        for (const auto& l : s.layers)
            ls.push_back({{"id", l.id},
                          {"region", l.region},
                          {"thickness", l.thickness},
                          {"thicknessUncertainty", l.thicknessUncertainty ? json(*l.thicknessUncertainty) : json(nullptr)},
                          {"angle", l.angle},
                          {"pattern",
                           {{"kind", l.pattern.kind},
                            {"start", l.pattern.start},
                            {"end", l.pattern.end},
                            {"segments", l.pattern.segments},
                            {"gap", l.pattern.gap},
                            {"period", l.pattern.period},
                            {"fill", l.pattern.fill},
                            {"featureSize", l.pattern.featureSize},
                            {"note", l.pattern.note}}}});
        ss.push_back({{"id", s.id}, {"name", s.name}, {"object", s.object}, {"axis", s.axis}, {"length", s.length}, {"width", s.width}, {"layers", ls}});
    }
    j["stacks"] = ss;
    json is = json::array();
    for (const auto& i : interfaces) {
        json ms = json::array();
        for (const auto& m : i.models)
            ms.push_back({{"kind", m.kind}, {"parameters", valuesJson(m.parameters)}});
        is.push_back({{"id", i.id},
                      {"name", i.name},
                      {"regions", json::array({i.regions[0], i.regions[1]})},
                      {"models", ms},
                      {"adhesionEnergy", optionalValue(i.adhesionEnergy)},
                      {"residualStress", optionalValue(i.residualStress)},
                      {"chemicalCompatibility", i.chemicalCompatibility},
                      {"defects", listOf(i.defects)},
                      {"preparation", i.preparation},
                      {"note", i.note}});
    }
    j["interfaces"] = is;
    json fs = json::array();
    for (const auto& f : fields)
        fs.push_back(f.toJson());
    j["fields"] = fs;
    if (!materialFields.empty()) {
        json mf = json::array();
        for (const auto& f : materialFields)
            mf.push_back(f.toJson());
        j["materialFields"] = mf;
    }
    json fns = json::array();
    for (const auto& f : functions) {
        json st = json::array();
        for (const auto& s : f.stages)
            st.push_back({{"id", s.id}, {"from", s.from}, {"to", s.to}, {"mechanism", s.mechanism}, {"regions", listOf(s.regions)}, {"settings", s.settings}});
        fns.push_back({{"id", f.id}, {"name", f.name}, {"kind", f.kind}, {"regions", listOf(f.regions)}, {"stages", st}, {"targets", f.targets}, {"note", f.note}});
    }
    j["functions"] = fns;
    json states = json::array();
    for (const auto& s : adaptive.states)
        states.push_back({{"id", s.id}, {"name", s.name}, {"condition", s.condition}, {"materialStateStudy", s.materialStateStudy}, {"description", s.description}});
    j["adaptive"] = {{"kind", adaptive.kind},
                     {"controlInput", adaptive.controlInput},
                     {"reversible", adaptive.reversible ? json(*adaptive.reversible) : json(nullptr)},
                     {"states", states}};
    json procs = json::array();
    for (const auto& p : processes)
        procs.push_back({{"id", p.id}, {"name", p.name}, {"kind", p.kind}, {"parameters", p.parameters}, {"constraints", p.constraints}, {"note", p.note}});
    j["processes"] = procs;
    json recs = json::array();
    for (const auto& r : records)
        recs.push_back({{"id", r.id},
                        {"date", r.date},
                        {"batch", r.batch},
                        {"process", r.process},
                        {"composition", r.composition},
                        {"parameters", r.parameters},
                        {"depositionConditions", r.depositionConditions},
                        {"heatTreatment", r.heatTreatment},
                        {"layerSequence", listOf(r.layerSequence)},
                        {"interfacePreparation", r.interfacePreparation},
                        {"postProcessing", listOf(r.postProcessing)},
                        {"inspections", r.inspections},
                        {"measuredDatasets", listOf(r.measuredDatasets)},
                        {"measuredProperties", r.measuredProperties},
                        {"note", r.note}});
    j["records"] = recs;
    return j;
}

Result<Architecture> Architecture::fromJson(const json& j)
{
    using R = Result<Architecture>;
    if (!j.is_object())
        return R::failure("an architecture is an object");
    Architecture a;
    a.id = j.value("id", std::string());
    a.name = j.value("name", a.id);
    a.description = j.value("description", std::string());
    a.representation = j.value("representation", std::string("idealized"));
    a.parent = j.value("parent", std::string());
    a.objects = strings(j, "objects");
    a.provenance = j.value("provenance", std::string("unstated"));
    a.note = j.value("note", std::string());
    const json regions = j.value("regions", json::array());
    for (const json& rj : regions) {
        FunctionalRegion r;
        r.id = rj.value("id", std::string());
        r.name = rj.value("name", r.id);
        r.object = rj.value("object", std::string());
        r.extent = rj.value("extent", std::string("object"));
        r.subvolume = objectOr(rj, "subvolume");
        r.role = rj.value("role", std::string());
        r.materialId = rj.value("materialId", std::string());
        r.materialClass = rj.value("materialClass", std::string());
        const json cj = objectOr(rj, "composition");
        r.composition.kind = cj.value("kind", std::string("homogeneous"));
        r.composition.basis = cj.value("basis", std::string("volume"));
        r.composition.method = cj.value("method", std::string());
        r.composition.matrix = cj.value("matrix", std::string());
        r.composition.note = cj.value("note", std::string());
        const json constituents = cj.value("constituents", json::array());
        for (const json& c : constituents) {
            Constituent k;
            k.id = c.value("id", std::string());
            k.materialId = c.value("materialId", std::string());
            k.substance = c.value("substance", std::string());
            k.formula = c.value("formula", std::string());
            k.materialClass = c.value("materialClass", std::string());
            k.fraction = c.value("fraction", 0.0);
            if (c.contains("fractionUncertainty") && c["fractionUncertainty"].is_number())
                k.fractionUncertainty = c["fractionUncertainty"].get<double>();
            r.composition.constituents.push_back(k);
        }
        const json phases = rj.value("phases", json::array());
        for (const json& p : phases)
            r.phases.push_back({p.value("id", std::string()), p.value("name", std::string()), p.value("materialId", std::string()), p.value("fraction", 0.0),
                                p.value("distribution", std::string("continuous")), objectOr(p, "stability"), p.value("metastable", false),
                                p.value("materialStateStudy", std::string())});
        if (rj.contains("microstructure") && rj["microstructure"].is_object()) {
            const json& m = rj["microstructure"];
            Microstructure ms;
            ms.crystalStructure = m.value("crystalStructure", std::string());
            ms.lattice = objectOr(m, "lattice");
            auto g = optionalFrom(m, "grainSize");
            auto p = optionalFrom(m, "porosity");
            if (!g || !p)
                return R::failure("region '" + r.id + "' microstructure: " + (!g ? g.message() : p.message()));
            ms.grainSize = g.value();
            ms.porosity = p.value();
            ms.grainSizeDistribution = m.value("grainSizeDistribution", std::string());
            ms.texture = m.value("texture", std::string());
            ms.defects = strings(m, "defects");
            ms.inclusions = strings(m, "inclusions");
            ms.domains = m.value("domains", std::string());
            ms.representation = m.value("representation", std::string());
            ms.provenance = m.value("provenance", std::string("unstated"));
            ms.note = m.value("note", std::string());
            r.microstructure = ms;
        }
        if (rj.contains("orientation") && rj["orientation"].is_object()) {
            const json& o = rj["orientation"];
            r.orientation = Orientation{o.value("phi1", 0.0), o.value("Phi", 0.0), o.value("phi2", 0.0), o.value("frame", std::string("stack"))};
        }
        const json aniso = objectOr(rj, "anisotropic");
        for (const auto& [kind, set] : aniso.items()) {
            auto vals = valuesFrom(set.value("values", json::object()), "region '" + r.id + "' " + kind);
            if (!vals)
                return R::failure(vals.message());
            r.anisotropic[kind] = AnisotropicSet{vals.value(), set.value("poling", std::string())};
        }
        auto props = valuesFrom(rj.value("properties", json::object()), "region '" + r.id + "' property");
        if (!props)
            return R::failure(props.message());
        r.properties = props.value();
        r.operating = objectOr(rj, "operating");
        r.limits = objectOr(rj, "limits");
        r.process = rj.value("process", std::string());
        r.functions = strings(rj, "functions");
        r.note = rj.value("note", std::string());
        a.regions.push_back(std::move(r));
    }
    const json stacks = j.value("stacks", json::array());
    for (const json& sj : stacks) {
        Stack s;
        s.id = sj.value("id", std::string());
        s.name = sj.value("name", s.id);
        s.object = sj.value("object", std::string());
        s.axis = sj.value("axis", std::string("+z"));
        s.length = sj.value("length", 0.0);
        s.width = sj.value("width", 0.0);
        const json layers = sj.value("layers", json::array());
        for (const json& lj : layers) {
            Layer l;
            l.id = lj.value("id", std::string());
            l.region = lj.value("region", std::string());
            l.thickness = lj.value("thickness", 0.0);
            if (lj.contains("thicknessUncertainty") && lj["thicknessUncertainty"].is_number())
                l.thicknessUncertainty = lj["thicknessUncertainty"].get<double>();
            l.angle = lj.value("angle", 0.0);
            const json pj = objectOr(lj, "pattern");
            l.pattern.kind = pj.value("kind", std::string("uniform"));
            l.pattern.start = pj.value("start", 0.0);
            l.pattern.end = pj.value("end", 1.0);
            l.pattern.segments = pj.value("segments", 1);
            l.pattern.gap = pj.value("gap", 0.0);
            l.pattern.period = pj.value("period", 0.0);
            l.pattern.fill = pj.value("fill", 1.0);
            l.pattern.featureSize = pj.value("featureSize", 0.0);
            l.pattern.note = pj.value("note", std::string());
            if (l.id.empty())
                l.id = s.id + "." + std::to_string(s.layers.size());
            s.layers.push_back(l);
        }
        a.stacks.push_back(std::move(s));
    }
    const json interfaces = j.value("interfaces", json::array());
    for (const json& ij : interfaces) {
        Interface i;
        i.id = ij.value("id", std::string());
        i.name = ij.value("name", i.id);
        const auto rr = strings(ij, "regions");
        if (rr.size() != 2)
            return R::failure("the interface '" + i.id + "' must join exactly two regions");
        i.regions = {rr[0], rr[1]};
        const json models = ij.value("models", json::array());
        for (const json& mj : models) {
            auto params = valuesFrom(mj.value("parameters", json::object()), "interface '" + i.id + "' parameter");
            if (!params)
                return R::failure(params.message());
            i.models.push_back({mj.value("kind", std::string()), params.value()});
        }
        auto ad = optionalFrom(ij, "adhesionEnergy");
        auto rs = optionalFrom(ij, "residualStress");
        if (!ad || !rs)
            return R::failure("interface '" + i.id + "': " + (!ad ? ad.message() : rs.message()));
        i.adhesionEnergy = ad.value();
        i.residualStress = rs.value();
        i.chemicalCompatibility = ij.value("chemicalCompatibility", std::string());
        i.defects = strings(ij, "defects");
        i.preparation = ij.value("preparation", std::string());
        i.note = ij.value("note", std::string());
        a.interfaces.push_back(std::move(i));
    }
    const json fields = j.value("fields", json::array());
    for (const json& fj : fields) {
        auto f = PropertyField::fromJson(fj);
        if (!f)
            return R::failure(f.message());
        a.fields.push_back(f.value());
    }
    for (const json& fj : j.value("materialFields", json::array())) {
        auto f = materials::MaterialField::fromJson(fj);
        if (!f)
            return R::failure(f.message());
        a.materialFields.push_back(f.value());
    }
    const json functions = j.value("functions", json::array());
    for (const json& fj : functions) {
        Function f;
        f.id = fj.value("id", std::string());
        f.name = fj.value("name", f.id);
        f.kind = fj.value("kind", std::string());
        f.regions = strings(fj, "regions");
        f.targets = objectOr(fj, "targets");
        f.note = fj.value("note", std::string());
        const json stages = fj.value("stages", json::array());
        for (const json& st : stages)
            f.stages.push_back({st.value("id", std::string()), st.value("from", std::string()), st.value("to", std::string()), st.value("mechanism", std::string()),
                                strings(st, "regions"), objectOr(st, "settings")});
        a.functions.push_back(std::move(f));
    }
    const json ad = objectOr(j, "adaptive");
    a.adaptive.kind = ad.value("kind", std::string("static"));
    a.adaptive.controlInput = ad.value("controlInput", std::string());
    if (ad.contains("reversible") && ad["reversible"].is_boolean())
        a.adaptive.reversible = ad["reversible"].get<bool>();
    const json states = ad.value("states", json::array());
    for (const json& s : states)
        a.adaptive.states.push_back({s.value("id", std::string()), s.value("name", std::string()), objectOr(s, "condition"),
                                     s.value("materialStateStudy", std::string()), s.value("description", std::string())});
    const json processes = j.value("processes", json::array());
    for (const json& p : processes)
        a.processes.push_back({p.value("id", std::string()), p.value("name", std::string()), p.value("kind", std::string()), objectOr(p, "parameters"),
                               objectOr(p, "constraints"), p.value("note", std::string())});
    const json records = j.value("records", json::array());
    for (const json& r : records) {
        ManufacturingRecord m;
        m.id = r.value("id", std::string());
        m.date = r.value("date", std::string());
        m.batch = r.value("batch", std::string());
        m.process = r.value("process", std::string());
        m.composition = objectOr(r, "composition");
        m.parameters = objectOr(r, "parameters");
        m.depositionConditions = objectOr(r, "depositionConditions");
        m.heatTreatment = objectOr(r, "heatTreatment");
        m.layerSequence = strings(r, "layerSequence");
        m.interfacePreparation = r.value("interfacePreparation", std::string());
        m.postProcessing = strings(r, "postProcessing");
        m.inspections = r.contains("inspections") && r["inspections"].is_array() ? r["inspections"] : json::array();
        m.measuredDatasets = strings(r, "measuredDatasets");
        m.measuredProperties = objectOr(r, "measuredProperties");
        m.note = r.value("note", std::string());
        a.records.push_back(std::move(m));
    }
    return a;
}

json Issue::toJson() const
{
    return {{"severity", severity}, {"code", code}, {"element", element}, {"message", message}};
}

bool hasErrors(const std::vector<Issue>& issues)
{
    return std::any_of(issues.begin(), issues.end(), [](const Issue& i) { return i.severity == "error"; });
}

namespace {

void checkValue(const SourcedValue& v, const std::string& element, const std::string& what, std::vector<Issue>& out)
{
    if (v.provenance == "unstated")
        out.push_back({"info", "unstated-provenance", element, what + " states no provenance"});
}

}  // namespace

std::vector<Issue> validate(const Architecture& a, const Project* project)
{
    std::vector<Issue> out;
    auto error = [&](const std::string& code, const std::string& element, const std::string& message) { out.push_back({"error", code, element, message}); };
    auto warning = [&](const std::string& code, const std::string& element, const std::string& message) { out.push_back({"warning", code, element, message}); };
    if (a.id.empty())
        error("missing-id", "", "the architecture needs an id");
    static const std::set<std::string> representations{"idealized", "as-designed", "as-built"};
    if (!representations.count(a.representation))
        error("unknown-representation", a.id, "the representation is idealized, as-designed or as-built, not '" + a.representation + "'");
    std::set<std::string> ids;
    auto unique = [&](const std::string& id, const std::string& what) {
        if (id.empty())
            error("missing-id", what, "a " + what + " needs an id");
        else if (!ids.insert(id).second)
            error("duplicate-id", id, "the id '" + id + "' is used twice");
    };
    for (const auto& o : a.objects)
        if (project && !project->scene.find(Id(o)))
            error("unknown-object", o, "the scene has no object '" + o + "'");
    for (const auto& r : a.regions) {
        unique(r.id, "region");
        if (r.object.empty())
            error("unbound-region", r.id, "the region '" + r.id + "' is bound to no scene object");
        else if (std::find(a.objects.begin(), a.objects.end(), r.object) == a.objects.end())
            error("unknown-object", r.id, "the region '" + r.id + "' names the object '" + r.object + "', which the architecture does not list");
        static const std::set<std::string> extents{"object", "layer", "subvolume"};
        if (!extents.count(r.extent))
            error("unknown-extent", r.id, "the extent is object, layer or subvolume, not '" + r.extent + "'");
        if (r.extent == "subvolume") {
            const json mn = r.subvolume.value("min", json::array()), mx = r.subvolume.value("max", json::array());
            bool ok = mn.is_array() && mx.is_array() && mn.size() == 3 && mx.size() == 3;
            for (size_t k = 0; ok && k < 3; ++k)
                ok = mn[k].is_number() && mx[k].is_number() && mn[k].get<double>() < mx[k].get<double>();
            if (!ok)
                error("bad-subvolume", r.id, "a subvolume region needs min and max corners [x, y, z] with min < max");
        }
        if (!r.materialId.empty() && project && !project->materials.find(Id(r.materialId)))
            error("unknown-material", r.id, "the library has no material '" + r.materialId + "'");
        if (r.materialId.empty() && r.composition.empty())
            error("no-material", r.id, "the region '" + r.id + "' has neither a material nor a composition");
        if (!r.composition.empty()) {
            double sum = 0.0;
            std::set<std::string> cids;
            for (const auto& c : r.composition.constituents) {
                if (c.materialId.empty() && c.substance.empty())
                    error("empty-constituent", r.id, "a constituent of '" + r.id + "' names no material and no substance");
                if (!c.materialId.empty() && project && !project->materials.find(Id(c.materialId)))
                    error("unknown-material", r.id, "the library has no material '" + c.materialId + "'");
                if (!(c.fraction >= 0.0 && c.fraction <= 1.0))
                    error("bad-fraction", r.id, "the fraction of '" + c.id + "' lies outside [0, 1]");
                if (!cids.insert(c.id).second)
                    error("duplicate-id", r.id, "the constituent id '" + c.id + "' is used twice in '" + r.id + "'");
                sum += c.fraction;
            }
            const bool graded = std::any_of(a.fields.begin(), a.fields.end(),
                                            [&](const PropertyField& f) { return f.region == r.id && f.property.rfind("fraction:", 0) == 0; });
            if (!graded && std::abs(sum - 1.0) > 1e-6)
                error("fractions-do-not-sum", r.id, "the fractions of '" + r.id + "' sum to " + std::to_string(sum) + ", not 1");
            if (!r.composition.matrix.empty() && !cids.count(r.composition.matrix))
                error("unknown-matrix", r.id, "the matrix '" + r.composition.matrix + "' is not a constituent of '" + r.id + "'");
        }
        double phaseSum = 0.0;
        for (const auto& p : r.phases) {
            if (!(p.fraction >= 0.0 && p.fraction <= 1.0))
                error("bad-fraction", r.id, "the phase fraction of '" + p.id + "' lies outside [0, 1]");
            phaseSum += p.fraction;
            if (!p.materialId.empty() && project && !project->materials.find(Id(p.materialId)))
                error("unknown-material", r.id, "the library has no material '" + p.materialId + "'");
        }
        if (!r.phases.empty() && std::abs(phaseSum - 1.0) > 1e-6)
            error("fractions-do-not-sum", r.id, "the phase fractions of '" + r.id + "' sum to " + std::to_string(phaseSum) + ", not 1");
        for (const auto& [kind, set] : r.anisotropic) {
            if (std::find(anisotropicKinds().begin(), anisotropicKinds().end(), kind) == anisotropicKinds().end())
                error("unknown-anisotropy", r.id, "the anisotropic set '" + kind + "' is not one of elastic, thermal, expansion, piezoelectric");
            for (const auto& [k, v] : set.values)
                checkValue(v, r.id, kind + "." + k, out);
            if (kind == "piezoelectric" && set.poling != "+3" && set.poling != "-3")
                error("no-poling", r.id, "the piezoelectric set of '" + r.id + "' needs its poling direction (+3 or -3)");
        }
        for (const auto& [k, v] : r.properties)
            checkValue(v, r.id, k, out);
        if (!r.process.empty() && !a.process(r.process))
            error("unknown-process", r.id, "the region '" + r.id + "' names the process '" + r.process + "', which the architecture does not define");
    }
    for (const auto& s : a.stacks) {
        unique(s.id, "stack");
        if (std::find(a.objects.begin(), a.objects.end(), s.object) == a.objects.end())
            error("unknown-object", s.id, "the stack '" + s.id + "' lies on the object '" + s.object + "', which the architecture does not list");
        static const std::set<std::string> axes{"+x", "-x", "+y", "-y", "+z", "-z"};
        if (!axes.count(s.axis))
            error("bad-axis", s.id, "the stack axis is one of +x, -x, +y, -y, +z, -z");
        if (s.layers.empty())
            error("empty-stack", s.id, "the stack '" + s.id + "' has no layer");
        for (size_t i = 0; i < s.layers.size(); ++i) {
            const Layer& l = s.layers[i];
            const FunctionalRegion* r = a.region(l.region);
            if (!r)
                error("unknown-region", l.id, "the layer '" + l.id + "' names no region of the architecture");
            if (!(l.thickness > 0.0) || !std::isfinite(l.thickness))
                error("bad-thickness", l.id, "the layer '" + l.id + "' needs a positive thickness");
            static const std::set<std::string> patterns{"uniform", "segmented", "periodic", "inclusions"};
            if (!patterns.count(l.pattern.kind))
                error("unknown-pattern", l.id, "the pattern is uniform, segmented, periodic or inclusions");
            if (!(l.pattern.start >= 0.0 && l.pattern.end <= 1.0 && l.pattern.start < l.pattern.end))
                error("bad-pattern", l.id, "the covered span of '" + l.id + "' must satisfy 0 <= start < end <= 1");
            if (!(l.pattern.fill > 0.0 && l.pattern.fill <= 1.0))
                error("bad-pattern", l.id, "the fill fraction of '" + l.id + "' lies in (0, 1]");
            if (i > 0) {
                const std::string& below = s.layers[i - 1].region;
                if (below == l.region)
                    continue;
                if (!a.interfaceBetween(below, l.region))
                    warning("undefined-interface", l.id,
                            "the layers '" + s.layers[i - 1].id + "' and '" + l.id + "' meet without an interface: nothing is assumed about it (declare 'perfect' or a model)");
            }
        }
    }
    for (const auto& i : a.interfaces) {
        unique(i.id, "interface");
        for (const auto& r : i.regions)
            if (!a.region(r))
                error("unknown-region", i.id, "the interface '" + i.id + "' names the region '" + r + "', which does not exist");
        if (i.regions[0] == i.regions[1])
            error("self-interface", i.id, "the interface '" + i.id + "' joins a region to itself");
        // The regions must touch: consecutive layers of a stack, or regions of one object.
        bool touches = false;
        for (const auto& s : a.stacks) {
            const int x = s.layerOf(i.regions[0]), y = s.layerOf(i.regions[1]);
            if (x >= 0 && y >= 0 && std::abs(x - y) == 1)
                touches = true;
        }
        const FunctionalRegion* r0 = a.region(i.regions[0]);
        const FunctionalRegion* r1 = a.region(i.regions[1]);
        if (!touches && r0 && r1 && (r0->extent == "subvolume" || r1->extent == "subvolume") && r0->object == r1->object)
            touches = true;  // an embedded region and its host
        if (!touches && r0 && r1 && r0->extent == "object" && r1->extent == "object" && r0->object != r1->object)
            touches = true;  // two objects in contact (the scene's geometry decides; not checked here)
        if (!touches && r0 && r1)
            error("not-adjacent", i.id, "the interface '" + i.id + "' joins regions that do not touch");
        if (i.models.empty())
            warning("no-interface-model", i.id, "the interface '" + i.id + "' has no model: every analysis that needs one will refuse it");
        for (const auto& m : i.models) {
            const InterfaceModelKind* k = findInterfaceModelKind(m.kind);
            if (!k) {
                error("unknown-interface-model", i.id, "the interface model '" + m.kind + "' is not known");
                continue;
            }
            for (const auto& [key, unit] : k->parameters)
                if (!m.parameter(key))
                    error("missing-parameter", i.id, "the " + k->title + " model of '" + i.id + "' needs '" + key + "' (" + unit + ")");
                else
                    checkValue(*m.parameter(key), i.id, m.kind + "." + key, out);
        }
    }
    for (const auto& f : a.materialFields) {
        unique(f.id, "field");
        if (!a.region(f.region))
            error("unknown-region", f.id, "the material field '" + f.id + "' names the region '" + f.region + "', which does not exist");
        if (Status st = f.validate(); !st)
            error("invalid-material-field", f.id, st.message());
    }
    for (const auto& f : a.fields) {
        unique(f.id, "field");
        if (!a.region(f.region))
            error("unknown-region", f.id, "the field '" + f.id + "' names the region '" + f.region + "', which does not exist");
        if (f.property.rfind("fraction:", 0) == 0) {
            const FunctionalRegion* r = a.region(f.region);
            const std::string c = f.property.substr(9);
            if (r && std::none_of(r->composition.constituents.begin(), r->composition.constituents.end(), [&](const Constituent& k) { return k.id == c; }))
                error("unknown-constituent", f.id, "the field '" + f.id + "' grades the fraction of '" + c + "', which is not a constituent");
            if (!(f.minimum() >= 0.0 && f.maximum() <= 1.0))
                error("bad-fraction", f.id, "the graded fraction of '" + f.id + "' leaves [0, 1]");
        }
        if (!f.realizable)
            out.push_back({"info", "idealized-field", f.id, "the field '" + f.id + "' is an idealized distribution, not a manufacturing specification"});
    }
    for (const auto& fn : a.functions) {
        unique(fn.id, "function");
        for (const auto& r : fn.regions)
            if (!a.region(r))
                error("unknown-region", fn.id, "the function '" + fn.id + "' names the region '" + r + "', which does not exist");
        for (const auto& st : fn.stages)
            for (const auto& r : st.regions)
                if (!a.region(r))
                    error("unknown-region", fn.id, "the stage '" + st.id + "' names the region '" + r + "', which does not exist");
    }
    static const std::set<std::string> adaptive{"static", "passive-adaptive", "externally-controlled", "reconfigurable"};
    if (!adaptive.count(a.adaptive.kind))
        error("unknown-adaptive-class", a.id, "the adaptive class is static, passive-adaptive, externally-controlled or reconfigurable");
    if (a.adaptive.kind == "externally-controlled" && a.adaptive.controlInput.empty())
        error("no-control-input", a.id, "an externally controlled architecture names its control input");
    if (a.adaptive.kind != "static" && a.adaptive.states.empty())
        warning("no-states", a.id, "an adaptive architecture names the states it changes between");
    if (a.adaptive.kind != "static" && !a.adaptive.reversible)
        out.push_back({"info", "reversibility-not-declared", a.id, "whether the changes are reversible is not declared: it is not assumed"});
    for (const auto& p : a.processes)
        unique(p.id, "process");
    for (const auto& rec : a.records) {
        unique(rec.id, "record");
        if (!rec.process.empty() && !a.process(rec.process))
            error("unknown-process", rec.id, "the record '" + rec.id + "' names the process '" + rec.process + "', which the architecture does not define");
    }
    return out;
}

std::string revision(const Architecture& a)
{
    return laboratory::sha256Hex(a.toJson().dump()).substr(0, 16);
}

const Architecture* Workspace::find(const std::string& id) const
{
    for (const auto& a : architectures)
        if (a.id == id)
            return &a;
    return nullptr;
}

Architecture* Workspace::find(const std::string& id)
{
    for (auto& a : architectures)
        if (a.id == id)
            return &a;
    return nullptr;
}

Result<Workspace> workspaceOf(const Project& project)
{
    Workspace w;
    if (!project.extensions.is_object() || !project.extensions.contains(kExtension))
        return w;
    const json& doc = project.extensions.at(kExtension);
    if (!doc.is_object() || doc.value("format", std::string()) != kFormat)
        return Result<Workspace>::failure(std::string("the project extension '") + kExtension + "' is not in the format " + kFormat);
    const json archs = doc.value("architectures", json::array());
    for (const json& a : archs) {
        auto parsed = Architecture::fromJson(a);
        if (!parsed)
            return Result<Workspace>::failure(parsed.message());
        w.architectures.push_back(parsed.value());
    }
    if (doc.contains("comparisons") && doc["comparisons"].is_array())
        w.comparisons = doc["comparisons"];
    return w;
}

void storeWorkspace(Project& project, const Workspace& w)
{
    if (w.architectures.empty() && w.comparisons.empty()) {
        project.extensions.erase(kExtension);
        return;
    }
    json archs = json::array();
    for (const auto& a : w.architectures)
        archs.push_back(a.toJson());
    project.extensions[kExtension] = {{"format", kFormat}, {"version", 1}, {"architectures", archs}, {"comparisons", w.comparisons}};
}

}  // namespace resonant::matter
