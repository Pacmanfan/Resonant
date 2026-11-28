#include "atomistic/LatticeStatics.h"

#include <Eigen/Eigenvalues>

#include <algorithm>
#include <cmath>

namespace resonant::atomistic {

namespace {

constexpr double kPi = 3.14159265358979323846;

Status check(const Crystal& c, const ForceField& f)
{
    if (Status v = c.validate(); !v)
        return v;
    if (!c.periodic)
        return Status::failure("lattice statics needs a periodic structure (a cell)");
    if (const auto missing = incompatibleSpecies(f, c); !missing.empty())
        return Status::failure("the force field '" + f.id + "' does not cover the species '" + missing.front() + "'");
    return Status::success();
}

double cutoffOf(const ForceField& f, const StaticsSettings& s)
{
    return (s.cutoffSigma > 0.0 ? s.cutoffSigma : f.cutoffSigma) * f.maxSigma();
}

// Image range so that every atom within rc of the cell is visited.
std::array<int, 3> images(const Eigen::Matrix3d& L, double rc)
{
    const Eigen::Matrix3d inv = L.inverse();
    std::array<int, 3> n{};
    for (int k = 0; k < 3; ++k) {
        const double height = 1.0 / inv.row(k).norm();  // the distance between the lattice planes normal to a_k*
        n[static_cast<size_t>(k)] = static_cast<int>(std::ceil(rc / height)) + 1;
    }
    return n;
}

template <typename Visit>
void forPairs(const Crystal& c, const Eigen::Matrix3d& F, double rc, Visit visit)
{
    const Eigen::Matrix3d L = F * c.lattice;
    std::vector<Eigen::Vector3d> x;
    for (const Atom& a : c.atoms)
        x.push_back(L * a.position);
    const auto n = images(L, rc);
    for (size_t i = 0; i < c.atoms.size(); ++i)
        for (size_t j = 0; j < c.atoms.size(); ++j)
            for (int a = -n[0]; a <= n[0]; ++a)
                for (int b = -n[1]; b <= n[1]; ++b)
                    for (int d = -n[2]; d <= n[2]; ++d) {
                        if (i == j && a == 0 && b == 0 && d == 0)
                            continue;
                        const Eigen::Vector3d r = x[j] + L * Eigen::Vector3d(a, b, d) - x[i];
                        const double rr = r.norm();
                        if (rr < rc)
                            visit(i, j, r, rr);
                    }
}

}  // namespace

Result<double> energyPerAtom(const Crystal& c, const ForceField& f, const Eigen::Matrix3d& F, const StaticsSettings& s)
{
    if (Status v = check(c, f); !v)
        return Result<double>::failure(v.message());
    const double rc = cutoffOf(f, s);
    double e = 0.0;
    forPairs(c, F, rc, [&](size_t i, size_t j, const Eigen::Vector3d&, double r) {
        const SpeciesParameters p = f.pair(c.atoms[i].species, c.atoms[j].species);
        const double q = std::pow(p.sigma / r, 6);
        e += 0.5 * 4.0 * p.epsilon * (q * q - q);
    });
    const double N = static_cast<double>(c.atoms.size());
    e /= N;
    if (s.tailCorrection) {
        const double rho = N / std::abs((F * c.lattice).determinant());
        const auto comp = c.composition();
        double tail = 0.0;
        for (const auto& [a, na] : comp)
            for (const auto& [b, nb] : comp) {
                const SpeciesParameters p = f.pair(a, b);
                tail += (na / N) * (nb / N) * 4.0 * p.epsilon * (std::pow(p.sigma, 12) / (9.0 * std::pow(rc, 9)) - std::pow(p.sigma, 6) / (3.0 * std::pow(rc, 3)));
            }
        e += 2.0 * kPi * rho * tail;
    }
    return e;
}

Result<double> maxForce(const Crystal& c, const ForceField& f, const Eigen::Matrix3d& F, const StaticsSettings& s)
{
    if (Status v = check(c, f); !v)
        return Result<double>::failure(v.message());
    std::vector<Eigen::Vector3d> force(c.atoms.size(), Eigen::Vector3d::Zero());
    forPairs(c, F, cutoffOf(f, s), [&](size_t i, size_t j, const Eigen::Vector3d& r, double rr) {
        const SpeciesParameters p = f.pair(c.atoms[i].species, c.atoms[j].species);
        const double q = std::pow(p.sigma / rr, 6);
        const double dphi = 4.0 * p.epsilon * (-12.0 * q * q + 6.0 * q) / rr;  // d phi / dr
        force[i] += dphi * r / rr;  // on i, towards j when attractive
    });
    double m = 0.0;
    for (const auto& v : force)
        m = std::max(m, v.norm());
    return m;
}

Result<Relaxed> relaxIsotropic(const Crystal& c, const ForceField& f, const StaticsSettings& s)
{
    using R = Result<Relaxed>;
    if (Status v = check(c, f); !v)
        return R::failure(v.message());
    Relaxed out;
    const auto E = [&](double scale) {
        ++out.evaluations;
        return energyPerAtom(c, f, Eigen::Matrix3d::Identity() * scale, s).value();
    };
    // dE/ds = 0 by bisection on a central difference (more precise than searching the minimum itself).
    const auto slope = [&](double x) { return E(x * (1.0 + 1e-6)) - E(x * (1.0 - 1e-6)); };
    double lo = 0.7, hi = 1.5;
    if (!(slope(lo) < 0.0 && slope(hi) > 0.0))
        return R::failure("no energy minimum within 0.7 to 1.5 of the given lattice: start closer to equilibrium");
    for (int i = 0; i < 80 && hi - lo > 1e-13; ++i) {
        const double mid = 0.5 * (lo + hi);
        (slope(mid) < 0.0 ? lo : hi) = mid;
    }
    out.scale = 0.5 * (lo + hi);
    out.crystal = c;
    out.crystal.lattice *= out.scale;
    out.energyPerAtom = E(out.scale);
    // p = -dE/dV per atom, by a central difference.
    const double h = 1e-6, V = c.volume() * std::pow(out.scale, 3) / static_cast<double>(c.atoms.size());
    const double dE = E(out.scale * (1 + h)) - E(out.scale * (1 - h));
    const double dV = V * (std::pow(1 + h, 3) - std::pow(1 - h, 3));
    out.pressure = -dE / dV;
    return out;
}

Result<ElasticResult> elasticConstants(const Crystal& c, const ForceField& f, const StaticsSettings& s, double h)
{
    using R = Result<ElasticResult>;
    if (Status v = check(c, f); !v)
        return R::failure(v.message());
    if (!(h > 0.0) || h > 1e-2)
        return R::failure("the strain amplitude must lie in (0, 0.01]");
    const double N = static_cast<double>(c.atoms.size()), V = c.volume();
    const auto F = [](const Eigen::Matrix<double, 6, 1>& e) {
        Eigen::Matrix3d m;
        m << e(0), 0.5 * e(5), 0.5 * e(4), 0.5 * e(5), e(1), 0.5 * e(3), 0.5 * e(4), 0.5 * e(3), e(2);
        return Eigen::Matrix3d(Eigen::Matrix3d::Identity() + m);
    };
    const auto W = [&](const Eigen::Matrix<double, 6, 1>& e) { return energyPerAtom(c, f, F(e), s).value() * N / V; };
    const Eigen::Matrix<double, 6, 1> zero = Eigen::Matrix<double, 6, 1>::Zero();
    const double w0 = W(zero);
    ElasticResult out;
    for (int i = 0; i < 6; ++i)
        for (int j = i; j < 6; ++j) {
            Eigen::Matrix<double, 6, 1> a = zero, b = zero;
            a(i) = h;
            b(j) = h;
            double cij;
            if (i == j)
                cij = (W(a) - 2.0 * w0 + W(-a)) / (h * h);
            else
                cij = (W(a + b) - W(a - b) - W(-a + b) + W(-a - b)) / (4.0 * h * h);
            out.C(i, j) = out.C(j, i) = cij;
        }
    // Residual stress at the reference (should vanish for a relaxed structure).
    double stress = 0.0;
    for (int i = 0; i < 3; ++i) {
        Eigen::Matrix<double, 6, 1> a = zero;
        a(i) = h;
        stress = std::max(stress, std::abs((W(a) - W(-a)) / (2.0 * h)));
    }
    if (stress > 1e-6 * out.C.diagonal().maxCoeff())
        out.warnings.push_back("the structure is not stress-free (" + std::to_string(stress) + " Pa): relax it first; the constants refer to a stressed state");
    const Eigen::SelfAdjointEigenSolver<Eigen::Matrix<double, 6, 6>> eig(out.C);
    out.stable = eig.eigenvalues().minCoeff() > 0.0;
    if (!out.stable)
        out.warnings.push_back("not mechanically stable (Born criteria): the elastic tensor is not positive definite");
    out.cauchyDeviation = std::abs(out.C(0, 1) - out.C(3, 3)) / std::max(1e-300, std::abs(out.C(3, 3)));
    // Voigt-Reuss-Hill.
    const auto& C = out.C;
    out.bulkVoigt = ((C(0, 0) + C(1, 1) + C(2, 2)) + 2.0 * (C(0, 1) + C(0, 2) + C(1, 2))) / 9.0;
    out.shearVoigt = ((C(0, 0) + C(1, 1) + C(2, 2)) - (C(0, 1) + C(0, 2) + C(1, 2)) + 3.0 * (C(3, 3) + C(4, 4) + C(5, 5))) / 15.0;
    if (out.stable) {
        const Eigen::Matrix<double, 6, 6> S = C.inverse();
        out.bulkReuss = 1.0 / ((S(0, 0) + S(1, 1) + S(2, 2)) + 2.0 * (S(0, 1) + S(0, 2) + S(1, 2)));
        out.shearReuss = 15.0 / (4.0 * (S(0, 0) + S(1, 1) + S(2, 2)) - 4.0 * (S(0, 1) + S(0, 2) + S(1, 2)) + 3.0 * (S(3, 3) + S(4, 4) + S(5, 5)));
    }
    out.bulk = 0.5 * (out.bulkVoigt + out.bulkReuss);
    out.shear = 0.5 * (out.shearVoigt + out.shearReuss);
    out.young = 9.0 * out.bulk * out.shear / (3.0 * out.bulk + out.shear);
    out.poisson = (3.0 * out.bulk - 2.0 * out.shear) / (2.0 * (3.0 * out.bulk + out.shear));
    double mass = 0.0;
    for (const Atom& a : c.atoms)
        mass += f.species.at(a.species).mass;
    out.density = mass / V;
    if (out.stable) {
        out.longitudinalSpeed = std::sqrt((out.bulk + 4.0 * out.shear / 3.0) / out.density);
        out.transverseSpeed = std::sqrt(out.shear / out.density);
    }
    // Would the basis relax under shear? (Not for Bravais lattices.)
    Eigen::Matrix<double, 6, 1> shear = zero;
    shear(3) = 10.0 * h;
    out.maxInternalForce = maxForce(c, f, F(shear), s).value();
    double scaleForce = 0.0;
    for (const auto& [name, p] : f.species)
        scaleForce = std::max(scaleForce, p.epsilon / p.sigma);
    if (out.maxInternalForce > 1e-6 * scaleForce)
        out.warnings.push_back("the basis atoms feel forces under strain: internal relaxation is not performed, so the constants are unrelaxed "
                               "(upper estimates)");
    return out;
}

}  // namespace resonant::atomistic
