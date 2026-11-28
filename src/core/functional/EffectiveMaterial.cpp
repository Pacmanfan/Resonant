#include "functional/EffectiveMaterial.h"

#include <algorithm>
#include <cmath>

namespace resonant::functional {

using nlohmann::json;

Status EffectiveMaterial::validate() const
{
    if (name.empty())
        return Status::failure("the effective material needs a name");
    if (kind != "homogenized" && kind != "retrieved" && kind != "equivalent-model")
        return Status::failure("the kind must be homogenized, retrieved or equivalent-model");
    if (properties.empty())
        return Status::failure("the effective material has no properties");
    if (validFromHz < 0.0 || validToHz < validFromHz)
        return Status::failure("the validity range is inverted");
    for (const EffectiveProperty& p : properties) {
        if (p.domain.empty() || p.key.empty())
            return Status::failure("every property needs a domain and a key");
        if (!std::isfinite(p.value))
            return Status::failure("the property '" + p.key + "' is not finite");
        double previous = -1.0;
        for (const auto& [f, v] : p.frequencyTable) {
            if (!(f > previous) || !std::isfinite(v))
                return Status::failure("the frequency table of '" + p.key + "' must ascend and be finite");
            previous = f;
        }
    }
    return Status::success();
}

json EffectiveMaterial::toJson() const
{
    json props = json::array();
    for (const EffectiveProperty& p : properties) {
        json table = json::array();
        for (const auto& [f, v] : p.frequencyTable)
            table.push_back({f, v});
        props.push_back({{"domain", p.domain}, {"key", p.key}, {"component", p.component}, {"value", p.value}, {"frequencyTable", table}, {"note", p.note}});
    }
    return {{"format", "resonant.effective-material"}, {"version", 1},          {"name", name},
            {"kind", kind},                            {"method", method},      {"properties", props},
            {"validFromHz", validFromHz},              {"validToHz", validToHz}, {"scaleRatio", scaleRatio},
            {"validity", validity},                    {"derivation", derivation}, {"limitations", limitations}};
}

Result<EffectiveMaterial> EffectiveMaterial::fromJson(const json& d)
{
    using R = Result<EffectiveMaterial>;
    try {
        if (d.value("format", "") != "resonant.effective-material")
            return R::failure("not an effective-material record");
        EffectiveMaterial m;
        m.name = d.value("name", "");
        m.kind = d.value("kind", "homogenized");
        m.method = d.value("method", "");
        m.validFromHz = d.value("validFromHz", 0.0);
        m.validToHz = d.value("validToHz", 0.0);
        m.scaleRatio = d.value("scaleRatio", 0.0);
        m.validity = d.value("validity", "");
        m.derivation = d.value("derivation", json::object());
        m.limitations = d.value("limitations", std::vector<std::string>{});
        for (const json& p : d.value("properties", json::array())) {
            EffectiveProperty e;
            e.domain = p.value("domain", "");
            e.key = p.value("key", "");
            e.component = p.value("component", "isotropic");
            e.value = p.value("value", 0.0);
            e.note = p.value("note", "");
            for (const json& row : p.value("frequencyTable", json::array()))
                e.frequencyTable.emplace_back(row.at(0).get<double>(), row.at(1).get<double>());
            m.properties.push_back(std::move(e));
        }
        if (Status v = m.validate(); !v)
            return R::failure(v.message());
        return m;
    } catch (const std::exception& e) {
        return R::failure(std::string("invalid effective-material record: ") + e.what());
    }
}

Result<Material> toLibraryMaterial(const EffectiveMaterial& e, const Id& id, const std::string& name, const json& components,
                                   const std::string& reference)
{
    using R = Result<Material>;
    if (Status v = e.validate(); !v)
        return R::failure(v.message());
    if (!components.is_null() && !components.is_object())
        return R::failure("components must map \"domain.key\" to a component");
    Material m(id, name.empty() ? e.name : name);
    std::string description = "Effective material (" + e.kind + ") derived by " + e.method;
    if (e.derivation.contains("structure"))
        description += " from the functional structure '" + e.derivation["structure"].value("name", std::string()) + "'";
    description += ". Valid: " + e.validity + ".";
    for (const std::string& l : e.limitations)
        description += " Limitation: " + l + ".";
    m.setDescription(description);
    std::vector<std::string> done;
    for (const EffectiveProperty& p : e.properties) {
        const std::string full = p.domain + "." + p.key;
        if (std::find(done.begin(), done.end(), full) != done.end())
            continue;
        const std::string wanted = components.is_object() ? components.value(full, std::string()) : std::string();
        if (!wanted.empty() && wanted != p.component) {
            const bool exists = std::any_of(e.properties.begin(), e.properties.end(),
                                            [&](const EffectiveProperty& q) { return q.domain == p.domain && q.key == p.key && q.component == wanted; });
            if (!exists)
                return R::failure("the effective material has no component '" + wanted + "' of " + full);
            continue;  // the wanted component comes later
        }
        done.push_back(full);
        const auto domain = physicsDomainFromString(p.domain);
        if (!domain)
            return R::failure("unknown domain '" + p.domain + "'");
        PropertyValue v;
        v.value = p.value;
        v.provenance = Provenance::Homogenized;
        v.source = e.kind + " by " + e.method + " (" + p.component + (p.note.empty() ? "" : ": " + p.note) + ")";
        if (e.derivation.contains("structure"))
            v.source += "; structure " + e.derivation["structure"].value("id", std::string());
        if (!reference.empty())
            v.source += "; " + reference;
        if (e.validToHz > 0.0) {
            v.validity.minFrequencyHz = e.validFromHz;
            v.validity.maxFrequencyHz = e.validToHz;
        }
        for (const auto& [f, x] : p.frequencyTable)
            v.frequencyTable.push_back({f, x});
        if (Status st = m.setProperty(*domain, p.key, std::move(v)); !st)
            return R::failure(full + " (" + p.component + "): " + st.message());
    }
    if (Status v = m.validate(); !v)
        return R::failure(v.message());
    return m;
}

}  // namespace resonant::functional
