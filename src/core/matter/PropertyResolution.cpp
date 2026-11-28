#include "matter/PropertyResolution.h"

#include "materials/Material.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace resonant::matter {

using nlohmann::json;

namespace {

struct Key {
    std::optional<PhysicsDomain> domain;
    std::string name;
};

Key split(const std::string& key)
{
    const auto dot = key.find('.');
    if (dot == std::string::npos)
        return {};
    return {physicsDomainFromString(key.substr(0, dot)), key.substr(dot + 1)};
}

std::string number(double v)
{
    std::ostringstream s;
    s.precision(4);
    s << v;
    return s.str();
}

// The methods a composition names (comma separated) that apply to a kind of property.
std::string methodFor(const Composition& c, PropertyKind kind)
{
    if (kind == PropertyKind::Density)
        return "volume-average";  // exact for any arrangement
    if (kind == PropertyKind::SpecificHeat)
        return "mass-average";
    const auto allowed = methodsFor(kind);
    std::stringstream list(c.method);
    std::string m;
    while (std::getline(list, m, ',')) {
        while (!m.empty() && m.front() == ' ')
            m.erase(m.begin());
        while (!m.empty() && m.back() == ' ')
            m.pop_back();
        if (std::find(allowed.begin(), allowed.end(), m) != allowed.end())
            return m;
    }
    return {};
}

}  // namespace

json ResolvedProperty::toJson() const
{
    json j{{"key", key}, {"value", std::isfinite(value) ? json(value) : json(nullptr)}, {"unit", unit}, {"origin", origin},
           {"provenance", provenance}, {"source", source}, {"message", message}};
    j["uncertainty"] = uncertainty ? json(*uncertainty) : json(nullptr);
    j["effective"] = effective ? effective->toJson() : json(nullptr);
    return j;
}

std::string unitOf(const std::string& key)
{
    const Key k = split(key);
    if (!k.domain)
        return {};
    const PropertyDef* d = findPropertyDefinition(*k.domain, k.name);
    return d ? d->unit : std::string();
}

ResolvedProperty constituentProperty(const Constituent& c, const std::string& key, const MaterialLibrary& library)
{
    ResolvedProperty out;
    out.key = key;
    out.unit = unitOf(key);
    if (c.substance == "void") {
        // Empty space: no conduction, no stiffness, no mass; the permittivity and permeability of vacuum.
        out.value = (key == "electromagnetic.relativePermittivity" || key == "electromagnetic.relativePermeability") ? 1.0 : 0.0;
        out.origin = "material";
        out.provenance = "theoretical";
        out.source = "void";
        return out;
    }
    const Key k = split(key);
    if (!c.materialId.empty() && k.domain) {
        if (const Material* m = library.find(Id(c.materialId)))
            if (const PropertyValue* v = m->property(*k.domain, k.name)) {
                out.value = v->value;
                out.origin = "material";
                out.provenance = std::string(toString(v->provenance));
                out.source = c.materialId + (v->source.empty() ? "" : " (" + v->source + ")");
                out.uncertainty = v->uncertainty;
                return out;
            }
    }
    out.origin = "missing";
    out.message = "the constituent '" + c.id + "' has no " + key + (c.materialId.empty() ? " (it names no library material)" : " in '" + c.materialId + "'");
    return out;
}

ResolvedProperty resolve(const Architecture& a, const FunctionalRegion& r, const std::string& key, double s, const MaterialLibrary& library)
{
    ResolvedProperty out;
    out.key = key;
    out.unit = unitOf(key);
    // 1. A field on the property.
    if (const PropertyField* f = a.field(r.id, key)) {
        out.value = f->at(s);
        out.origin = "field";
        out.provenance = f->provenance;
        out.source = f->id;
        out.message = std::string(f->realizable ? "a realizable " : "an idealized ") + f->kind + " field from " + number(f->from) + " to " + number(f->to);
        return out;
    }
    // 2. A value stated on the region.
    if (auto it = r.properties.find(key); it != r.properties.end()) {
        out.value = it->second.value;
        out.origin = "region";
        out.provenance = it->second.provenance;
        out.source = it->second.source;
        out.uncertainty = it->second.uncertainty;
        return out;
    }
    // 3. The composition's effective value by its method.
    const PropertyKind kind = propertyKindOf(key);
    if (!r.composition.empty() && r.composition.constituents.size() > 1) {
        const std::string method = methodFor(r.composition, kind);
        if (!method.empty() && (r.composition.basis == "volume" || kind == PropertyKind::Density)) {
            // Fractions: graded ones at s, the others sharing the remainder as stated.
            std::vector<double> fractions;
            double graded = 0.0, rest = 0.0;
            std::vector<bool> isGraded;
            for (const auto& c : r.composition.constituents) {
                const PropertyField* g = a.field(r.id, "fraction:" + c.id);
                isGraded.push_back(g != nullptr);
                fractions.push_back(g ? g->at(s) : c.fraction);
                (g ? graded : rest) += fractions.back();
            }
            const bool anyGraded = std::any_of(isGraded.begin(), isGraded.end(), [](bool g) { return g; });
            if (anyGraded)
                for (size_t i = 0; i < fractions.size(); ++i)
                    if (!isGraded[i])
                        fractions[i] = rest > 0.0 ? fractions[i] / rest * (1.0 - graded) : (1.0 - graded) / static_cast<double>(fractions.size());
            std::vector<PhaseProperty> phases;
            int matrix = -1;
            for (size_t i = 0; i < r.composition.constituents.size(); ++i) {
                const Constituent& c = r.composition.constituents[i];
                PhaseProperty p;
                p.id = c.id;
                p.fraction = fractions[i];
                const std::string valueKey = kind == PropertyKind::Elastic ? "mechanical.youngsModulus" : key;
                const ResolvedProperty v = constituentProperty(c, valueKey, library);
                if (!v.ok()) {
                    out.origin = "missing";
                    out.message = v.message + ": the composition cannot be combined (" + method + ")";
                    return out;
                }
                p.value = v.value;
                if (kind == PropertyKind::Elastic || kind == PropertyKind::Expansion) {
                    const ResolvedProperty nu = constituentProperty(c, "mechanical.poissonsRatio", library);
                    const ResolvedProperty e = kind == PropertyKind::Expansion ? constituentProperty(c, "mechanical.youngsModulus", library) : v;
                    if (!nu.ok() || !e.ok()) {
                        out.origin = "missing";
                        out.message = (!nu.ok() ? nu.message : e.message) + ": the composition cannot be combined (" + method + ")";
                        return out;
                    }
                    p.poisson = nu.value;
                    if (kind == PropertyKind::Expansion)
                        p.bulkModulus = elasticFrom(e.value, nu.value).bulk;
                }
                if (kind == PropertyKind::SpecificHeat) {
                    const ResolvedProperty rho = constituentProperty(c, "thermal.density", library);
                    if (!rho.ok()) {
                        out.origin = "missing";
                        out.message = rho.message + ": the specific heat is mass-weighted";
                        return out;
                    }
                    p.density = rho.value;
                }
                if (c.id == r.composition.matrix)
                    matrix = static_cast<int>(i);
                phases.push_back(p);
            }
            auto e = combine(kind == PropertyKind::Elastic ? "mechanical.youngsModulus" : key, phases, method, matrix);
            if (!e) {
                out.origin = "missing";
                out.message = e.message();
                return out;
            }
            out.value = key == "mechanical.poissonsRatio" ? e.value().poisson : e.value().value;
            out.origin = "composition";
            out.provenance = "homogenized";
            out.source = method;
            out.effective = e.value();
            if (!e.value().valid) {
                out.origin = "missing";
                out.message = "the " + method + " estimate does not apply here: " + e.value().validity;
            }
            return out;
        }
        if (r.materialId.empty()) {
            out.origin = "missing";
            out.message = "the composition of '" + r.id + "' names no method for " + std::string(toString(kind)) + " properties";
            // The bounds, so that the missing method is not a missing range.
            for (const auto& m : methodsFor(kind)) {
                std::vector<PhaseProperty> phases;
                bool complete = true;
                for (const auto& c : r.composition.constituents) {
                    const ResolvedProperty v = constituentProperty(c, kind == PropertyKind::Elastic ? "mechanical.youngsModulus" : key, library);
                    const ResolvedProperty nu = constituentProperty(c, "mechanical.poissonsRatio", library);
                    complete = complete && v.ok() && (kind != PropertyKind::Elastic || nu.ok());
                    phases.push_back({c.id, c.fraction, v.value, nu.ok() ? nu.value : 0.0, 0.0, 0.0});
                }
                if (!complete || kind == PropertyKind::Other || kind == PropertyKind::Expansion)
                    break;
                if (auto e = combine(kind == PropertyKind::Elastic ? "mechanical.youngsModulus" : key, phases, m, -1)) {
                    out.message += "; the " + e.value().bounds + " bounds are [" + number(e.value().lower) + ", " + number(e.value().upper) + "] " + out.unit;
                    break;
                }
            }
            return out;
        }
    }
    // 4. The library material (a one-constituent composition resolves through its constituent: a
    // material, or void).
    std::string material = r.materialId;
    if (material.empty() && r.composition.constituents.size() == 1) {
        const ResolvedProperty c = constituentProperty(r.composition.constituents.front(), key, library);
        if (c.ok())
            return c;
        material = r.composition.constituents.front().materialId;
    }
    const Key k = split(key);
    if (!material.empty() && k.domain)
        if (const Material* m = library.find(Id(material)))
            if (const PropertyValue* v = m->property(*k.domain, k.name)) {
                out.value = v->value;
                out.origin = "material";
                out.provenance = std::string(toString(v->provenance));
                out.source = material + (v->source.empty() ? "" : " (" + v->source + ")");
                out.uncertainty = v->uncertainty;
                return out;
            }
    out.origin = "missing";
    out.message = "the region '" + r.id + "' has no " + key + (material.empty() ? "" : " (not in '" + material + "')") + ": it is not substituted";
    return out;
}

}  // namespace resonant::matter
