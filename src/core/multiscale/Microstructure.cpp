#include "multiscale/Microstructure.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace resonant::multiscale {

using nlohmann::json;

namespace {

constexpr double kPi = 3.14159265358979323846;

// Uniform in [0, 1) from the raw 64-bit output (the same on every standard library).
struct Random {
    explicit Random(std::uint64_t seed) : engine(seed) {}
    double uniform() { return static_cast<double>(engine() >> 11) * (1.0 / 9007199254740992.0); }
    double normal()
    {
        const double u = std::max(uniform(), 1e-300), v = uniform();
        return std::sqrt(-2.0 * std::log(u)) * std::cos(2 * kPi * v);
    }
    std::mt19937_64 engine;
};

std::array<double, 3> unit(std::array<double, 3> a)
{
    const double r = std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
    return {a[0] / r, a[1] / r, a[2] / r};
}

// The minimum-image distance squared in a periodic box.
double periodicDistance2(const std::array<double, 3>& a, const std::array<double, 3>& b, const std::array<double, 3>& box)
{
    double d2 = 0.0;
    for (size_t k = 0; k < 3; ++k) {
        double d = a[k] - b[k];
        d -= box[k] * std::round(d / box[k]);
        d2 += d * d;
    }
    return d2;
}

std::array<double, 3> centre(const VoxelRve& r, int i, int j, int k)
{
    return {(i + 0.5) * r.voxel(0), (j + 0.5) * r.voxel(1), (k + 0.5) * r.voxel(2)};
}

}  // namespace

std::vector<double> VoxelRve::volumeFractions() const
{
    std::vector<double> f(phases.size(), 0.0);
    for (std::uint16_t p : phase)
        if (p < f.size())
            f[p] += 1.0;
    for (double& x : f)
        x /= static_cast<double>(std::max<size_t>(1, count()));
    return f;
}

Result<VoxelRve> generateRve(const json& s)
{
    using R = Result<VoxelRve>;
    try {
        VoxelRve r;
        r.generator = s.value("generator", "");
        const auto n = s.value("voxels", std::vector<int>{});
        const auto size = s.value("size", std::vector<double>{});
        if (n.size() != 3 || size.size() != 3)
            return R::failure("an RVE needs \"voxels\": [nx, ny, nz] and \"size\": [Lx, Ly, Lz]");
        for (size_t k = 0; k < 3; ++k) {
            if (n[k] < 1 || n[k] > 256)
                return R::failure("voxels per axis must lie in [1, 256]");
            if (!(size[k] > 0.0))
                return R::failure("the RVE's size must be positive");
            r.n[k] = n[k];
            r.size[k] = size[k];
        }
        r.phases = s.value("phases", std::vector<std::string>{});
        if (r.phases.empty())
            return R::failure("an RVE needs at least one phase");
        if (r.phases.size() > 64)
            return R::failure("at most 64 phases");
        r.phase.assign(r.count(), 0);
        r.grain.assign(r.count(), -1);
        Random rng(s.value("seed", 1ull));
        const auto checkPhase = [&](int p) { return p >= 0 && p < static_cast<int>(r.phases.size()); };
        if (r.generator == "homogeneous") {
            const int p = s.value("phase", 0);
            if (!checkPhase(p))
                return R::failure("unknown phase index");
            std::fill(r.phase.begin(), r.phase.end(), static_cast<std::uint16_t>(p));
        } else if (r.generator == "layered") {
            const int axis = s.value("axis", 2);
            const auto fractions = s.value("fractions", std::vector<double>{});
            const int periods = std::max(1, s.value("periods", 1));
            if (axis < 0 || axis > 2 || fractions.size() < 2 || fractions.size() > r.phases.size())
                return R::failure("a layered RVE needs an axis 0..2 and one fraction per phase");
            double total = 0.0;
            for (double f : fractions) {
                if (!(f > 0.0))
                    return R::failure("layer fractions must be positive");
                total += f;
            }
            const int m = r.n[static_cast<size_t>(axis)];
            for (int c = 0; c < m; ++c) {
                const double t = std::fmod((c + 0.5) / m * periods, 1.0) * total;
                int p = 0;
                double acc = fractions[0];
                while (t > acc && p + 1 < static_cast<int>(fractions.size()))
                    acc += fractions[static_cast<size_t>(++p)];
                for (int a = 0; a < r.n[0]; ++a)
                    for (int b = 0; b < r.n[1]; ++b)
                        for (int k = 0; k < r.n[2]; ++k) {
                            const int idx[3] = {a, b, k};
                            if (idx[axis] == c)
                                r.phase[r.index(a, b, k)] = static_cast<std::uint16_t>(p);
                        }
            }
        } else if (r.generator == "spheres") {
            const int matrix = s.value("matrix", 0), inclusion = s.value("inclusion", 1);
            const double radius = s.value("radius", 0.0), target = s.value("volumeFraction", 0.0);
            if (!checkPhase(matrix) || !checkPhase(inclusion))
                return R::failure("unknown matrix or inclusion phase");
            if (!(radius > 0.0) || radius > 0.5 * *std::min_element(r.size.begin(), r.size.end()))
                return R::failure("the sphere radius must be positive and at most half the RVE");
            if (!(target > 0.0) || target > 0.5)
                return R::failure("random non-overlapping spheres reach volume fractions up to about 0.5");
            std::fill(r.phase.begin(), r.phase.end(), static_cast<std::uint16_t>(matrix));
            std::vector<std::array<double, 3>> centres;
            const double volume = r.size[0] * r.size[1] * r.size[2];
            const double sphere = 4.0 / 3.0 * kPi * radius * radius * radius;
            const size_t wanted = static_cast<size_t>(std::max(1.0, std::round(target * volume / sphere)));
            for (int attempt = 0; attempt < 200000 && centres.size() < wanted; ++attempt) {
                const std::array<double, 3> c{rng.uniform() * r.size[0], rng.uniform() * r.size[1], rng.uniform() * r.size[2]};
                bool clear = true;
                for (const auto& o : centres)
                    clear = clear && periodicDistance2(c, o, r.size) >= 4.0 * radius * radius * 1.0000001;
                if (clear)
                    centres.push_back(c);
            }
            if (centres.size() < wanted)
                return R::failure("could not place " + std::to_string(wanted) + " non-overlapping spheres (placed " + std::to_string(centres.size()) + ")");
            for (int k = 0; k < r.n[2]; ++k)
                for (int j = 0; j < r.n[1]; ++j)
                    for (int i = 0; i < r.n[0]; ++i) {
                        const auto x = centre(r, i, j, k);
                        for (const auto& c : centres)
                            if (periodicDistance2(x, c, r.size) <= radius * radius) {
                                r.phase[r.index(i, j, k)] = static_cast<std::uint16_t>(inclusion);
                                break;
                            }
                    }
        } else if (r.generator == "voronoi") {
            const int grains = s.value("grains", 8), p = s.value("phase", 0);
            if (grains < 1 || grains > 4096 || !checkPhase(p))
                return R::failure("a Voronoi RVE needs 1..4096 grains and a phase");
            const std::string texture = s.value("texture", "random");
            const auto axis = unit(s.value("axis", std::array<double, 3>{0.0, 0.0, 1.0}));
            const double spread = s.value("spreadDeg", 0.0) * kPi / 180.0;
            std::vector<std::array<double, 3>> seeds;
            for (int g = 0; g < grains; ++g) {
                seeds.push_back({rng.uniform() * r.size[0], rng.uniform() * r.size[1], rng.uniform() * r.size[2]});
                if (texture == "random") {
                    // Uniform on the sphere.
                    const double z = 2.0 * rng.uniform() - 1.0, phi = 2 * kPi * rng.uniform(), rho = std::sqrt(std::max(0.0, 1.0 - z * z));
                    r.axes.push_back({rho * std::cos(phi), rho * std::sin(phi), z});
                } else if (texture == "aligned") {
                    // The axis tilted by a Gaussian angle of the given spread.
                    std::array<double, 3> a{axis[0] + std::tan(spread) * rng.normal(), axis[1] + std::tan(spread) * rng.normal(),
                                            axis[2] + std::tan(spread) * rng.normal()};
                    r.axes.push_back(unit(a));
                } else {
                    return R::failure("texture must be random or aligned");
                }
            }
            for (int k = 0; k < r.n[2]; ++k)
                for (int j = 0; j < r.n[1]; ++j)
                    for (int i = 0; i < r.n[0]; ++i) {
                        const auto x = centre(r, i, j, k);
                        int best = 0;
                        double d = 1e300;
                        for (int g = 0; g < grains; ++g) {
                            const double e = periodicDistance2(x, seeds[static_cast<size_t>(g)], r.size);
                            if (e < d) {
                                d = e;
                                best = g;
                            }
                        }
                        r.phase[r.index(i, j, k)] = static_cast<std::uint16_t>(p);
                        r.grain[r.index(i, j, k)] = best;
                    }
        } else if (r.generator == "voxels") {
            const auto data = s.value("data", std::vector<int>{});
            if (data.size() != r.count())
                return R::failure("imported voxel data must hold nx * ny * nz phase indices");
            for (size_t v = 0; v < data.size(); ++v) {
                if (!checkPhase(data[v]))
                    return R::failure("imported voxel " + std::to_string(v) + " names an unknown phase");
                r.phase[v] = static_cast<std::uint16_t>(data[v]);
            }
            const auto grain = s.value("grain", std::vector<int>{});
            const auto axes = s.value("axes", std::vector<std::array<double, 3>>{});
            if (!grain.empty()) {
                if (grain.size() != r.count())
                    return R::failure("imported grain data must hold one index per voxel");
                for (size_t v = 0; v < grain.size(); ++v) {
                    if (grain[v] < -1 || grain[v] >= static_cast<int>(axes.size()))
                        return R::failure("imported voxel " + std::to_string(v) + " names an unknown grain");
                    r.grain[v] = grain[v];
                }
                for (const auto& a : axes)
                    r.axes.push_back(unit(a));
            }
            if (!s.contains("periodic"))
                return R::failure("imported voxel data must say whether it is periodic (\"periodic\": true or false)");
            r.periodic = s["periodic"].get<bool>();
        } else {
            return R::failure("unknown generator '" + r.generator + "' (homogeneous, layered, spheres, voronoi, voxels)");
        }
        return r;
    } catch (const std::exception& e) {
        return R::failure(std::string("invalid RVE specification: ") + e.what());
    }
}

}  // namespace resonant::multiscale
