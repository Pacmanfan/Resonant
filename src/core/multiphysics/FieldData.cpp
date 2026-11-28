#include "multiphysics/FieldData.h"

#include <algorithm>
#include <cmath>

namespace resonant::multiphysics {

double GridSupport::cellVolume(size_t cell) const
{
    const size_t nx = count(0), ny = count(1);
    const size_t i = cell % nx, j = (cell / nx) % ny, k = cell / (nx * ny);
    return width(0, i) * width(1, j) * width(2, k);
}

Vec3 GridSupport::cellCentre(size_t cell) const
{
    const size_t nx = count(0), ny = count(1);
    const std::array<size_t, 3> ijk{cell % nx, (cell / nx) % ny, cell / (nx * ny)};
    Vec3 c;
    for (int a = 0; a < 3; ++a)
        c[a] = 0.5 * (edges[static_cast<size_t>(a)][ijk[static_cast<size_t>(a)]] + edges[static_cast<size_t>(a)][ijk[static_cast<size_t>(a)] + 1]);
    return c;
}

double GridSupport::volume() const
{
    double v = 1.0;
    for (const auto& e : edges)
        v *= e.back() - e.front();
    return v;
}

Status GridSupport::validate() const
{
    for (int a = 0; a < 3; ++a) {
        const auto& e = edges[static_cast<size_t>(a)];
        if (e.size() < 2)
            return Status::failure("a grid support needs at least one cell along every axis");
        for (size_t i = 0; i + 1 < e.size(); ++i)
            if (!(e[i + 1] > e[i]) || !std::isfinite(e[i]) || !std::isfinite(e[i + 1]))
                return Status::failure("grid edges must be finite and strictly ascending");
    }
    return Status::success();
}

bool GridSupport::sameAs(const GridSupport& other) const
{
    return frame == other.frame && edges == other.edges;
}

GridSupport GridSupport::uniform(const Vec3& origin, const Vec3& size, size_t nx, size_t ny, size_t nz)
{
    GridSupport g;
    const std::array<size_t, 3> n{nx, ny, nz};
    for (int a = 0; a < 3; ++a) {
        const size_t m = std::max<size_t>(1, n[static_cast<size_t>(a)]);
        auto& e = g.edges[static_cast<size_t>(a)];
        e.resize(m + 1);
        for (size_t i = 0; i <= m; ++i)
            e[i] = origin[a] + size[a] * static_cast<double>(i) / static_cast<double>(m);
    }
    return g;
}

nlohmann::json GridSupport::toJson() const
{
    nlohmann::json j;
    j["frame"] = frame;
    j["cells"] = {count(0), count(1), count(2)};
    j["min"] = {edges[0].front(), edges[1].front(), edges[2].front()};
    j["max"] = {edges[0].back(), edges[1].back(), edges[2].back()};
    return j;
}

std::string_view toString(TimeCharacter time)
{
    switch (time) {
    case TimeCharacter::Steady: return "steady";
    case TimeCharacter::Harmonic: return "harmonic";
    case TimeCharacter::Instant: return "instant";
    }
    return "steady";
}

double FieldData::integral(int component) const
{
    double sum = 0.0;
    for (size_t c = 0; c < support.cellCount(); ++c)
        sum += value(c, component) * support.cellVolume(c);
    return sum;
}

double FieldData::maxAbs() const
{
    double m = 0.0;
    for (double v : values)
        m = std::max(m, std::abs(v));
    return m;
}

bool FieldData::finite() const
{
    return std::all_of(values.begin(), values.end(), [](double v) { return std::isfinite(v); });
}

Status FieldData::validate() const
{
    if (Status s = support.validate(); !s)
        return s;
    if (values.size() != support.cellCount() * static_cast<size_t>(components()))
        return Status::failure(quantity.id + ": " + std::to_string(values.size()) + " values for " + std::to_string(support.cellCount()) + " cells");
    if (!finite())
        return Status::failure(quantity.id + " from " + producer + " contains values that are not finite");
    return Status::success();
}

nlohmann::json FieldData::summary() const
{
    nlohmann::json j;
    j["quantity"] = quantity.id;
    j["unit"] = quantity.unit;
    j["meaning"] = std::string(toString(quantity.meaning));
    j["time"] = std::string(toString(time));
    if (time == TimeCharacter::Harmonic)
        j["frequencyHz"] = frequencyHz;
    j["support"] = support.toJson();
    j["producer"] = producer;
    j["solver"] = solver;
    j["materialRevision"] = materialRevision;
    j["geometryRevision"] = geometryRevision;
    if (!values.empty()) {
        j["min"] = *std::min_element(values.begin(), values.end());
        j["max"] = *std::max_element(values.begin(), values.end());
        if (quantity.conservative)
            j["integral"] = integral();
    }
    return j;
}

}  // namespace resonant::multiphysics
