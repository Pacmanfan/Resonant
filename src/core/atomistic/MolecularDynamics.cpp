#include "atomistic/MolecularDynamics.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace resonant::atomistic {

namespace {
constexpr double kBoltzmann = 1.380649e-23;
constexpr double kPi = 3.14159265358979323846;

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
}  // namespace

Result<MdResult> runNve(const Crystal& c, const ForceField& f, const MdSettings& s, const std::function<bool()>& cancelled)
{
    using R = Result<MdResult>;
    if (Status v = c.validate(); !v)
        return R::failure(v.message());
    if (!c.periodic)
        return R::failure("molecular dynamics here needs a periodic cell");
    const Eigen::Matrix3d& L = c.lattice;
    if (std::abs(L(0, 1)) + std::abs(L(0, 2)) + std::abs(L(1, 0)) + std::abs(L(1, 2)) + std::abs(L(2, 0)) + std::abs(L(2, 1)) > 1e-12 * L.norm())
        return R::failure("only orthorhombic cells are supported (diagonal lattice vectors)");
    if (const auto missing = incompatibleSpecies(f, c); !missing.empty())
        return R::failure("the force field '" + f.id + "' does not cover the species '" + missing.front() + "'");
    if (!(s.timeStep > 0.0) || s.steps < 1 || s.sampleEvery < 1 || !(s.temperature >= 0.0))
        return R::failure("a positive time step, steps and sampling interval, and a non-negative temperature");
    const Eigen::Vector3d box(L(0, 0), L(1, 1), L(2, 2));
    const double rc = s.cutoffSigma * f.maxSigma();
    if (box.minCoeff() < 2.0 * rc)
        return R::failure("the cell must be at least twice the cutoff (" + std::to_string(rc) + " m) along every axis: use a larger supercell");
    const int n = static_cast<int>(c.atoms.size());
    std::vector<Eigen::Vector3d> x = c.cartesian(), v(static_cast<size_t>(n)), a(static_cast<size_t>(n));
    std::vector<double> m(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i)
        m[static_cast<size_t>(i)] = f.species.at(c.atoms[static_cast<size_t>(i)].species).mass;
    // Maxwell-Boltzmann velocities, zero total momentum, scaled to the temperature exactly.
    Random rng(s.seed);
    Eigen::Vector3d p = Eigen::Vector3d::Zero();
    double M = 0.0;
    for (int i = 0; i < n; ++i) {
        const double sd = std::sqrt(kBoltzmann * s.temperature / m[static_cast<size_t>(i)]);
        v[static_cast<size_t>(i)] = Eigen::Vector3d(rng.normal(), rng.normal(), rng.normal()) * sd;
        p += m[static_cast<size_t>(i)] * v[static_cast<size_t>(i)];
        M += m[static_cast<size_t>(i)];
    }
    double ke = 0.0;
    for (int i = 0; i < n; ++i) {
        v[static_cast<size_t>(i)] -= p / M;
        ke += 0.5 * m[static_cast<size_t>(i)] * v[static_cast<size_t>(i)].squaredNorm();
    }
    const double dof = 3.0 * n - 3.0;
    if (s.temperature > 0.0 && ke > 0.0) {
        const double scale = std::sqrt(0.5 * dof * kBoltzmann * s.temperature / ke);
        for (auto& vi : v)
            vi *= scale;
    }
    // Shifted force: phi_sf(r) = phi(r) - phi(rc) - (r - rc) phi'(rc).
    const auto forces = [&](double& potential) {
        for (auto& ai : a)
            ai.setZero();
        potential = 0.0;
        for (int i = 0; i < n; ++i)
            for (int j = i + 1; j < n; ++j) {
                Eigen::Vector3d r = x[static_cast<size_t>(j)] - x[static_cast<size_t>(i)];
                for (int k = 0; k < 3; ++k)
                    r(k) -= box(k) * std::round(r(k) / box(k));
                const double rr = r.norm();
                if (rr >= rc)
                    continue;
                const SpeciesParameters q = f.pair(c.atoms[static_cast<size_t>(i)].species, c.atoms[static_cast<size_t>(j)].species);
                const auto phi = [&](double d) {
                    const double t = std::pow(q.sigma / d, 6);
                    return 4.0 * q.epsilon * (t * t - t);
                };
                const auto dphi = [&](double d) {
                    const double t = std::pow(q.sigma / d, 6);
                    return 4.0 * q.epsilon * (-12.0 * t * t + 6.0 * t) / d;
                };
                potential += phi(rr) - phi(rc) - (rr - rc) * dphi(rc);
                const double g = dphi(rr) - dphi(rc);  // d phi_sf / dr
                const Eigen::Vector3d fij = g * r / rr;  // force on i
                a[static_cast<size_t>(i)] += fij / m[static_cast<size_t>(i)];
                a[static_cast<size_t>(j)] -= fij / m[static_cast<size_t>(j)];
            }
    };
    MdResult out;
    out.atoms = n;
    std::vector<Eigen::Vector3d> unwrapped = x, origin = x;
    double pot = 0.0;
    forces(pot);
    const auto sample = [&](int step) {
        double k = 0.0;
        for (int i = 0; i < n; ++i)
            k += 0.5 * m[static_cast<size_t>(i)] * v[static_cast<size_t>(i)].squaredNorm();
        out.time.push_back(step * s.timeStep);
        out.kinetic.push_back(k);
        out.potential.push_back(pot);
        out.total.push_back(k + pot);
        out.temperature.push_back(2.0 * k / (dof * kBoltzmann));
        double msd = 0.0;
        for (int i = 0; i < n; ++i)
            msd += (unwrapped[static_cast<size_t>(i)] - origin[static_cast<size_t>(i)]).squaredNorm();
        out.msd.push_back(msd / n);
    };
    sample(0);
    for (int step = 1; step <= s.steps; ++step) {
        if (cancelled && step % 100 == 0 && cancelled())
            return R::failure("cancelled");
        for (int i = 0; i < n; ++i) {
            v[static_cast<size_t>(i)] += 0.5 * s.timeStep * a[static_cast<size_t>(i)];
            const Eigen::Vector3d dx = s.timeStep * v[static_cast<size_t>(i)];
            x[static_cast<size_t>(i)] += dx;
            unwrapped[static_cast<size_t>(i)] += dx;
            for (int k = 0; k < 3; ++k)
                x[static_cast<size_t>(i)](k) -= box(k) * std::floor(x[static_cast<size_t>(i)](k) / box(k));
        }
        forces(pot);
        for (int i = 0; i < n; ++i)
            v[static_cast<size_t>(i)] += 0.5 * s.timeStep * a[static_cast<size_t>(i)];
        if (step % s.sampleEvery == 0)
            sample(step);
    }
    double meanK = 0.0, meanT = 0.0;
    for (size_t i = 0; i < out.kinetic.size(); ++i) {
        meanK += out.kinetic[i];
        meanT += out.temperature[i];
    }
    meanK /= static_cast<double>(out.kinetic.size());
    out.meanTemperature = meanT / static_cast<double>(out.temperature.size());
    for (double e : out.total)
        out.energyDrift = std::max(out.energyDrift, std::abs(e - out.total.front()));
    out.energyDrift /= std::max(meanK, 1e-300);
    // g(r) of the final configuration up to half the shortest box edge.
    const double rmax = 0.5 * box.minCoeff(), dr = rmax / s.rdfBins;
    std::vector<double> hist(static_cast<size_t>(s.rdfBins), 0.0);
    for (int i = 0; i < n; ++i)
        for (int j = i + 1; j < n; ++j) {
            Eigen::Vector3d r = x[static_cast<size_t>(j)] - x[static_cast<size_t>(i)];
            for (int k = 0; k < 3; ++k)
                r(k) -= box(k) * std::round(r(k) / box(k));
            const int b = static_cast<int>(r.norm() / dr);
            if (b < s.rdfBins)
                hist[static_cast<size_t>(b)] += 2.0;
        }
    const double rho = n / (box(0) * box(1) * box(2));
    for (int b = 0; b < s.rdfBins; ++b) {
        const double r0 = b * dr, r1 = r0 + dr, shell = 4.0 / 3.0 * kPi * (r1 * r1 * r1 - r0 * r0 * r0);
        out.rdfRadius.push_back(r0 + 0.5 * dr);
        out.rdf.push_back(hist[static_cast<size_t>(b)] / (n * rho * shell));
    }
    return out;
}

}  // namespace resonant::atomistic
