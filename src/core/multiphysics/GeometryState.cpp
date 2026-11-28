#include "multiphysics/GeometryState.h"

#include <algorithm>
#include <cmath>

namespace resonant::multiphysics {

void GeometryState::setReference(const std::string& region, const std::string& dimension, double value)
{
    reference_[region][dimension] = value;
    current_[region][dimension] = value;
    ++revision_;
}

double GeometryState::reference(const std::string& region, const std::string& dimension) const
{
    const auto r = reference_.find(region);
    if (r == reference_.end())
        return 0.0;
    const auto d = r->second.find(dimension);
    return d == r->second.end() ? 0.0 : d->second;
}

double GeometryState::current(const std::string& region, const std::string& dimension) const
{
    const auto r = current_.find(region);
    if (r == current_.end())
        return 0.0;
    const auto d = r->second.find(dimension);
    return d == r->second.end() ? 0.0 : d->second;
}

Result<double> GeometryState::apply(const std::vector<GeometryChange>& changes, double relaxation)
{
    if (!(relaxation > 0.0 && relaxation <= 1.0))
        return Result<double>::failure("the relaxation factor must be in (0, 1]");
    double largest = 0.0;
    bool changed = false;
    for (const GeometryChange& c : changes) {
        const double ref = reference(c.region, c.dimension);
        if (!(ref > 0.0))
            return Result<double>::failure("the geometry has no reference " + c.dimension + " for region '" + c.region + "'");
        if (!(c.value > 0.0) || !std::isfinite(c.value))
            return Result<double>::failure("a geometry update gave region '" + c.region + "' a " + c.dimension + " that is not positive and finite");
        double& cur = current_[c.region][c.dimension];
        const double next = cur + relaxation * (c.value - cur);
        largest = std::max(largest, std::abs(next - cur) / ref);
        if (next != cur)
            changed = true;
        cur = next;
    }
    if (changed)
        ++revision_;
    return largest;
}

nlohmann::json GeometryState::toJson() const
{
    nlohmann::json j{{"revision", revision_}, {"regions", nlohmann::json::object()}};
    for (const auto& [region, dims] : reference_)
        for (const auto& [dim, value] : dims)
            j["regions"][region][dim] = {{"reference", value}, {"current", current(region, dim)}};
    return j;
}

}  // namespace resonant::multiphysics
