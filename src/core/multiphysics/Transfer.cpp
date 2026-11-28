#include "multiphysics/Transfer.h"

#include <algorithm>
#include <cmath>

namespace resonant::multiphysics {
namespace {

// Overlap lengths between target cells and source cells along one axis: weights[t] = {(s, length)}.
std::vector<std::vector<std::pair<size_t, double>>> overlaps(const std::vector<double>& target, const std::vector<double>& source)
{
    std::vector<std::vector<std::pair<size_t, double>>> out(target.size() - 1);
    size_t s = 0;
    for (size_t t = 0; t + 1 < target.size(); ++t) {
        while (s + 1 < source.size() && source[s + 1] <= target[t])
            ++s;
        for (size_t q = s; q + 1 < source.size() && source[q] < target[t + 1]; ++q) {
            const double length = std::min(target[t + 1], source[q + 1]) - std::max(target[t], source[q]);
            if (length > 0.0)
                out[t].emplace_back(q, length);
        }
    }
    return out;
}

// Linear interpolation weights at x between source cell centres: {(cell, weight)}.
std::vector<std::pair<size_t, double>> linearWeights(const std::vector<double>& edges, double x)
{
    const size_t n = edges.size() - 1;
    if (n == 1)
        return {{0, 1.0}};
    const auto centre = [&edges](size_t i) { return 0.5 * (edges[i] + edges[i + 1]); };
    if (x <= centre(0))
        return {{0, 1.0}};
    if (x >= centre(n - 1))
        return {{n - 1, 1.0}};
    size_t lo = 0, hi = n - 1;
    while (hi - lo > 1) {
        const size_t mid = (lo + hi) / 2;
        (centre(mid) <= x ? lo : hi) = mid;
    }
    const double t = (x - centre(lo)) / (centre(hi) - centre(lo));
    return {{lo, 1.0 - t}, {hi, t}};
}

}  // namespace

std::string_view toString(TransferMethod method)
{
    return method == TransferMethod::Conservative ? "conservative" : "interpolating";
}

std::optional<TransferMethod> transferMethodFromString(std::string_view text)
{
    if (text == "conservative")
        return TransferMethod::Conservative;
    if (text == "interpolating")
        return TransferMethod::Interpolating;
    return std::nullopt;
}

Result<FieldData> transfer(const FieldData& source, const GridSupport& target, TransferMethod method, TransferReport* report)
{
    if (Status s = source.validate(); !s)
        return Result<FieldData>::failure(s.message());
    if (Status s = target.validate(); !s)
        return Result<FieldData>::failure(s.message());
    if (source.support.frame != target.frame)
        return Result<FieldData>::failure("the grids are in different frames (" + source.support.frame + ", " + target.frame + ")");
    if (method == TransferMethod::Conservative && !source.quantity.conservative)
        return Result<FieldData>::failure(source.quantity.id + " is not a conserved density: transfer it by interpolation");
    FieldData out = source;
    out.support = target;
    const int nc = source.components();
    out.values.assign(target.cellCount() * static_cast<size_t>(nc), 0.0);
    const size_t tnx = target.count(0), tny = target.count(1), tnz = target.count(2);
    const size_t snx = source.support.count(0), sny = source.support.count(1);
    if (method == TransferMethod::Conservative) {
        std::array<std::vector<std::vector<std::pair<size_t, double>>>, 3> w;
        for (int a = 0; a < 3; ++a)
            w[static_cast<size_t>(a)] = overlaps(target.edges[static_cast<size_t>(a)], source.support.edges[static_cast<size_t>(a)]);
        for (size_t k = 0; k < tnz; ++k)
            for (size_t j = 0; j < tny; ++j)
                for (size_t i = 0; i < tnx; ++i) {
                    const size_t t = target.index(i, j, k);
                    const double volume = target.cellVolume(t);
                    for (const auto& [sk, lz] : w[2][k])
                        for (const auto& [sj, ly] : w[1][j])
                            for (const auto& [si, lx] : w[0][i]) {
                                const size_t s = si + snx * (sj + sny * sk);
                                const double share = lx * ly * lz / volume;
                                for (int c = 0; c < nc; ++c)
                                    out.values[t * static_cast<size_t>(nc) + static_cast<size_t>(c)] += source.value(s, c) * share;
                            }
                }
    } else {
        for (size_t k = 0; k < tnz; ++k)
            for (size_t j = 0; j < tny; ++j)
                for (size_t i = 0; i < tnx; ++i) {
                    const size_t t = target.index(i, j, k);
                    const Vec3 p = target.cellCentre(t);
                    const auto wx = linearWeights(source.support.edges[0], p.x());
                    const auto wy = linearWeights(source.support.edges[1], p.y());
                    const auto wz = linearWeights(source.support.edges[2], p.z());
                    for (const auto& [sk, fz] : wz)
                        for (const auto& [sj, fy] : wy)
                            for (const auto& [si, fx] : wx) {
                                const size_t s = si + snx * (sj + sny * sk);
                                for (int c = 0; c < nc; ++c)
                                    out.values[t * static_cast<size_t>(nc) + static_cast<size_t>(c)] += source.value(s, c) * fx * fy * fz;
                            }
                }
    }
    if (report) {
        report->method = method;
        report->sourceIntegral = source.integral();
        report->targetIntegral = out.integral();
        report->relativeDifference =
            report->sourceIntegral != 0.0 ? std::abs(report->targetIntegral - report->sourceIntegral) / std::abs(report->sourceIntegral) : 0.0;
        double covered = 1.0;
        for (int a = 0; a < 3; ++a) {
            const auto& se = source.support.edges[static_cast<size_t>(a)];
            const auto& te = target.edges[static_cast<size_t>(a)];
            const double overlap = std::max(0.0, std::min(se.back(), te.back()) - std::max(se.front(), te.front()));
            covered *= overlap / (se.back() - se.front());
        }
        report->coveredFraction = covered;
        report->description = method == TransferMethod::Conservative
                                  ? "overlap-weighted remapping of a density: the integral over the overlap is preserved"
                                  : "linear interpolation between cell centres, clamped at the ends: not conservative";
    }
    return out;
}

}  // namespace resonant::multiphysics
