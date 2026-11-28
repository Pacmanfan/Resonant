#include "multiphysics/MaterialState.h"

#include "multiphysics/Transfer.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace resonant::multiphysics {

std::string_view toString(LawKind kind)
{
    switch (kind) {
    case LawKind::Constant: return "constant";
    case LawKind::LinearInTemperature: return "linear";
    case LawKind::Table: return "table";
    }
    return "constant";
}

Status PropertyLaw::validate() const
{
    if (region.empty() || property.find('.') == std::string::npos)
        return Status::failure("a material law needs a region and a property '<domain>.<key>'");
    if (!(validMax > validMin))
        return Status::failure("the law for " + property + " needs a validity range with max > min");
    if (kind == LawKind::Table) {
        if (table.size() < 2)
            return Status::failure("a table law needs at least two points");
        for (size_t i = 1; i < table.size(); ++i)
            if (!(table[i].first > table[i - 1].first))
                return Status::failure("table temperatures must ascend");
    }
    return Status::success();
}

nlohmann::json PropertyLaw::toJson() const
{
    nlohmann::json j{{"region", region}, {"property", property}, {"law", std::string(toString(kind))}, {"validity", {validMin, validMax}},
                     {"provenance", provenance}, {"source", source}};
    if (kind != LawKind::Table)
        j["value"] = value;
    if (kind == LawKind::LinearInTemperature) {
        j["reference"] = reference;
        j["slope"] = slope;
    }
    if (kind == LawKind::Table) {
        j["table"] = nlohmann::json::array();
        for (const auto& [t, v] : table)
            j["table"].push_back({t, v});
    }
    return j;
}

Result<PropertyLaw> PropertyLaw::fromJson(const nlohmann::json& j)
{
    if (!j.is_object())
        return Result<PropertyLaw>::failure("a material law must be an object");
    PropertyLaw law;
    law.region = j.value("region", "");
    law.property = j.value("property", "");
    const std::string kind = j.value("law", "constant");
    if (kind == "constant")
        law.kind = LawKind::Constant;
    else if (kind == "linear")
        law.kind = LawKind::LinearInTemperature;
    else if (kind == "table")
        law.kind = LawKind::Table;
    else
        return Result<PropertyLaw>::failure("unknown material law '" + kind + "' (constant, linear, table)");
    law.value = j.value("value", 0.0);
    law.reference = j.value("reference", 293.15);
    law.slope = j.value("slope", 0.0);
    if (j.contains("table") && j["table"].is_array())
        for (const auto& p : j["table"])
            if (p.is_array() && p.size() == 2)
                law.table.emplace_back(p[0].get<double>(), p[1].get<double>());
    if (j.contains("validity") && j["validity"].is_array() && j["validity"].size() == 2) {
        law.validMin = j["validity"][0].get<double>();
        law.validMax = j["validity"][1].get<double>();
    }
    law.provenance = j.value("provenance", "illustrative");
    law.source = j.value("source", "");
    if (Status s = law.validate(); !s)
        return Result<PropertyLaw>::failure(s.message());
    return law;
}

LawValue evaluate(const PropertyLaw& law, double temperatureK)
{
    LawValue out;
    double t = temperatureK;
    if (t < law.validMin || t > law.validMax) {
        out.extrapolated = true;
        t = std::clamp(t, law.validMin, law.validMax);
    }
    switch (law.kind) {
    case LawKind::Constant: out.value = law.value; break;
    case LawKind::LinearInTemperature: out.value = law.value + law.slope * (t - law.reference); break;
    case LawKind::Table: {
        const auto& tb = law.table;
        if (t <= tb.front().first)
            out.value = tb.front().second;
        else if (t >= tb.back().first)
            out.value = tb.back().second;
        else {
            const auto hi = std::upper_bound(tb.begin(), tb.end(), t, [](double x, const auto& p) { return x < p.first; });
            const auto lo = hi - 1;
            out.value = lo->second + (hi->second - lo->second) * (t - lo->first) / (hi->first - lo->first);
        }
        break;
    }
    }
    return out;
}

const PropertyLaw* MaterialState::law(const std::string& region, const std::string& property) const
{
    const auto it = std::find_if(laws_.begin(), laws_.end(), [&](const PropertyLaw& l) { return l.region == region && l.property == property; });
    return it == laws_.end() ? nullptr : &*it;
}

double MaterialState::temperatureAt(const Vec3& point) const
{
    if (!temperature_)
        return reference_;
    // One-point interpolation through the transfer operator's weights.
    GridSupport probe;
    for (int a = 0; a < 3; ++a)
        probe.edges[static_cast<size_t>(a)] = {point[a] - 1e-12, point[a] + 1e-12};
    probe.frame = temperature_->support.frame;
    auto sampled = transfer(*temperature_, probe, TransferMethod::Interpolating);
    return sampled ? sampled.value().values.front() : reference_;
}

Result<double> MaterialState::updateTemperature(const FieldData& field, double relaxation)
{
    if (field.quantity.id != "temperature")
        return Result<double>::failure("the material state takes a temperature field, not " + field.quantity.id);
    if (Status s = field.validate(); !s)
        return Result<double>::failure(s.message());
    if (!(relaxation > 0.0 && relaxation <= 1.0))
        return Result<double>::failure("the relaxation factor must be in (0, 1]");
    double change = 0.0;
    if (temperature_ && temperature_->support.sameAs(field.support)) {
        FieldData next = field;
        for (size_t i = 0; i < next.values.size(); ++i) {
            const double old = temperature_->values[i];
            next.values[i] = old + relaxation * (field.values[i] - old);
            change = std::max(change, std::abs(next.values[i] - old));
        }
        temperature_ = std::move(next);
    } else {
        for (double v : field.values)
            change = std::max(change, std::abs((relaxation < 1.0 ? reference_ + relaxation * (v - reference_) : v) - reference_));
        FieldData next = field;
        if (relaxation < 1.0)
            for (double& v : next.values)
                v = reference_ + relaxation * (v - reference_);
        temperature_ = std::move(next);
    }
    ++revision_;
    temperature_->materialRevision = revision_;
    history_.push_back(*temperature_);
    if (history_.size() > historyLimit_)
        history_.erase(history_.begin());
    return change;
}

double MaterialState::property(const std::string& region, const std::string& property, const Vec3& point, double intrinsic)
{
    const PropertyLaw* l = law(region, property);
    if (!l)
        return intrinsic;
    const double t = temperatureAt(point);
    const LawValue v = evaluate(*l, t);
    if (v.extrapolated) {
        std::ostringstream s;
        s.precision(5);
        s << property << " of region '" << region << "' evaluated at " << t << " K, outside its validity [" << l->validMin << ", " << l->validMax
          << "] K: the value at the nearest valid temperature was used";
        warnings_.insert(s.str());
    }
    return v.value;
}

}  // namespace resonant::multiphysics
