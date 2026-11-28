#include "multiscale/MicrostructureAnalysis.h"

#include <Eigen/Cholesky>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace resonant::multiscale {
namespace {

constexpr double kPi = 3.14159265358979323846;

// The neighbour of voxel (i, j, k) one step along axis a in direction s; false outside a non-periodic RVE.
bool neighbour(const VoxelRve& r, int i, int j, int k, int a, int s, size_t& out)
{
    int c[3] = {i, j, k};
    c[a] += s;
    if (c[a] < 0 || c[a] >= r.n[static_cast<size_t>(a)]) {
        if (!r.periodic)
            return false;
        c[a] = (c[a] + r.n[static_cast<size_t>(a)]) % r.n[static_cast<size_t>(a)];
    }
    out = r.index(c[0], c[1], c[2]);
    return true;
}

}  // namespace

nlohmann::json DefectRecord::toJson() const
{
    return {{"kind", kind}, {"where", where}, {"density", density}, {"unit", unit}, {"provenance", provenance}};
}

nlohmann::json MicrostructureStatistics::toJson() const
{
    nlohmann::json d = nlohmann::json::array();
    for (const auto& x : defects)
        d.push_back(x.toJson());
    return {{"phaseFractions", phaseFractions}, {"porosity", porosity}, {"grains", grains}, {"meanGrainDiameter", meanGrainDiameter},
            {"grainBoundaryDensity", grainBoundaryDensity}, {"phaseBoundaryDensity", phaseBoundaryDensity}, {"inclusions", inclusions},
            {"defects", d}, {"notes", notes}};
}

MicrostructureStatistics microstructureStatistics(const VoxelRve& r, const std::vector<std::string>& voidPhases, const std::vector<std::string>& inclusionPhases,
                                                  const std::vector<DefectRecord>& defects)
{
    MicrostructureStatistics s;
    s.phaseFractions = r.volumeFractions();
    const auto phaseIndex = [&](const std::string& name) {
        const auto it = std::find(r.phases.begin(), r.phases.end(), name);
        return it == r.phases.end() ? -1 : static_cast<int>(it - r.phases.begin());
    };
    for (const auto& v : voidPhases) {
        const int p = phaseIndex(v);
        if (p < 0)
            s.notes.push_back("the void phase '" + v + "' is not in the RVE");
        else
            s.porosity += s.phaseFractions[static_cast<size_t>(p)];
    }
    const double volume = r.size[0] * r.size[1] * r.size[2];
    // Boundaries: each face between two voxels counted once (the + direction).
    double gb = 0.0, pb = 0.0;
    for (int k = 0; k < r.n[2]; ++k)
        for (int j = 0; j < r.n[1]; ++j)
            for (int i = 0; i < r.n[0]; ++i) {
                const size_t v = r.index(i, j, k);
                for (int a = 0; a < 3; ++a) {
                    size_t w = 0;
                    if (!neighbour(r, i, j, k, a, +1, w))
                        continue;
                    const double area = volume / (r.n[0] * r.n[1] * r.n[2]) / r.voxel(a);
                    if (r.phase[v] != r.phase[w])
                        pb += area;
                    else if (!r.grain.empty() && r.grain[v] >= 0 && r.grain[w] >= 0 && r.grain[v] != r.grain[w])
                        gb += area;
                }
            }
    s.grainBoundaryDensity = gb / volume;
    s.phaseBoundaryDensity = pb / volume;
    // Grains and their mean size.
    if (!r.grain.empty()) {
        std::vector<int> ids;
        for (const auto g : r.grain)
            if (g >= 0)
                ids.push_back(g);
        std::sort(ids.begin(), ids.end());
        ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
        s.grains = static_cast<int>(ids.size());
        if (s.grains > 0) {
            size_t inGrains = 0;
            for (const auto g : r.grain)
                inGrains += g >= 0;
            const double meanVolume = volume * static_cast<double>(inGrains) / static_cast<double>(r.count()) / s.grains;
            s.meanGrainDiameter = std::cbrt(6 * meanVolume / kPi);
        }
    }
    // Separate inclusions: face-connected regions of the inclusion phases.
    std::vector<int> label(r.count(), -1);
    for (const auto& name : inclusionPhases) {
        const int p = phaseIndex(name);
        if (p < 0) {
            s.notes.push_back("the inclusion phase '" + name + "' is not in the RVE");
            continue;
        }
        for (size_t v0 = 0; v0 < r.count(); ++v0) {
            if (r.phase[v0] != p || label[v0] >= 0)
                continue;
            label[v0] = s.inclusions;
            std::vector<size_t> stack{v0};
            while (!stack.empty()) {
                const size_t v = stack.back();
                stack.pop_back();
                const int i = static_cast<int>(v % static_cast<size_t>(r.n[0]));
                const int j = static_cast<int>((v / static_cast<size_t>(r.n[0])) % static_cast<size_t>(r.n[1]));
                const int k = static_cast<int>(v / (static_cast<size_t>(r.n[0]) * static_cast<size_t>(r.n[1])));
                for (int a = 0; a < 3; ++a)
                    for (const int sgn : {-1, +1}) {
                        size_t w = 0;
                        if (neighbour(r, i, j, k, a, sgn, w) && r.phase[w] == p && label[w] < 0) {
                            label[w] = s.inclusions;
                            stack.push_back(w);
                        }
                    }
            }
            ++s.inclusions;
        }
    }
    s.defects = defects;
    if (!defects.empty())
        s.notes.push_back("defects are recorded densities (metadata with their provenance), not computed from the voxels");
    s.notes.push_back("boundary areas count voxel faces: a staircase estimate of curved boundaries (it overestimates their true area)");
    return s;
}

Result<PotentialProblem> conductionProblem(const VoxelRve& r, const std::vector<Eigen::Matrix3d>& k, double resistance)
{
    using R = Result<PotentialProblem>;
    if (k.size() != r.phases.size())
        return R::failure("one conductivity tensor per phase is needed (" + std::to_string(r.phases.size()) + ")");
    for (size_t p = 0; p < k.size(); ++p)
        if ((k[p] - k[p].transpose()).cwiseAbs().maxCoeff() > 1e-12 * k[p].cwiseAbs().maxCoeff() || Eigen::LLT<Eigen::Matrix3d>(k[p]).info() != Eigen::Success)
            return R::failure("the conductivity of the phase '" + r.phases[p] + "' is not symmetric positive definite (refused, not repaired)");
    if (resistance < 0.0)
        return R::failure("a grain-boundary resistance cannot be negative");
    if (resistance > 0.0 && r.grain.empty())
        return R::failure("a grain-boundary resistance needs the RVE's grains");
    PotentialProblem pr;
    pr.n = r.n;
    pr.size = r.size;
    pr.periodic = r.periodic;
    pr.K.resize(r.count());
    for (int kk = 0; kk < r.n[2]; ++kk)
        for (int j = 0; j < r.n[1]; ++j)
            for (int i = 0; i < r.n[0]; ++i) {
                const size_t v = r.index(i, j, kk);
                Eigen::Matrix3d kv = k[r.phase[v]];
                if (resistance > 0.0 && r.grain[v] >= 0) {
                    Eigen::Vector3d scale = Eigen::Vector3d::Ones();
                    for (int a = 0; a < 3; ++a) {
                        int faces = 0;
                        for (const int sgn : {-1, +1}) {
                            size_t w = 0;
                            if (neighbour(r, i, j, kk, a, sgn, w) && r.grain[w] >= 0 && r.grain[w] != r.grain[v] && r.phase[w] == r.phase[v])
                                ++faces;
                        }
                        if (faces > 0) {
                            const double d = r.voxel(a), kaa = kv(a, a);
                            scale(a) = (d / kaa) / (d / kaa + faces * resistance / 2.0);  // k' / k in series along a
                        }
                    }
                    const Eigen::DiagonalMatrix<double, 3> s(scale.cwiseSqrt());
                    kv = s * kv * s;
                }
                pr.K[v] = kv;
            }
    return pr;
}

}  // namespace resonant::multiscale
