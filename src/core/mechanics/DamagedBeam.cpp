#include "mechanics/DamagedBeam.h"

#include <Eigen/Dense>
#include <Eigen/Eigenvalues>

#include <algorithm>
#include <cmath>

namespace resonant::mechanics {

namespace {
constexpr double kPi = 3.14159265358979323846;

Eigen::Matrix4d elementStiffness(double EI, double h)
{
    Eigen::Matrix4d k;
    k << 12, 6 * h, -12, 6 * h, 6 * h, 4 * h * h, -6 * h, 2 * h * h, -12, -6 * h, 12, -6 * h, 6 * h, 2 * h * h, -6 * h, 4 * h * h;
    return k * EI / (h * h * h);
}

Eigen::Matrix4d elementMass(double rhoA, double h)
{
    Eigen::Matrix4d m;
    m << 156, 22 * h, 54, -13 * h, 22 * h, 4 * h * h, 13 * h, -3 * h * h, 54, 13 * h, 156, -22 * h, -13 * h, -3 * h * h, -22 * h, 4 * h * h;
    return m * rhoA * h / 420.0;
}

// Global matrices (all 2 (N + 1) DOFs: w0, t0, w1, t1, ...).
Eigen::MatrixXd assemble(int n, double h, const std::vector<double>& EI, bool mass, double rhoA)
{
    Eigen::MatrixXd K = Eigen::MatrixXd::Zero(2 * (n + 1), 2 * (n + 1));
    for (int e = 0; e < n; ++e)
        K.block<4, 4>(2 * e, 2 * e) += mass ? elementMass(rhoA, h) : elementStiffness(EI[static_cast<size_t>(e)], h);
    return K;
}

// The largest |w''| over an element (at its ends: w'' is linear in a Hermite element).
double maxCurvature(const Eigen::VectorXd& u, int e, double h)
{
    const double w1 = u(2 * e), t1 = u(2 * e + 1), w2 = u(2 * e + 2), t2 = u(2 * e + 3);
    const double a = (-6 * w1 - 4 * h * t1 + 6 * w2 - 2 * h * t2) / (h * h);
    const double b = (6 * w1 + 2 * h * t1 - 6 * w2 + 4 * h * t2) / (h * h);
    return std::max(std::abs(a), std::abs(b));
}

// Deflection with w = t = 0 at the root and w = delta at the tip; returns u and the tip reaction.
std::pair<Eigen::VectorXd, double> deflect(const Eigen::MatrixXd& K, double delta)
{
    const int N = static_cast<int>(K.rows());
    const int tip = N - 2;
    // Free DOFs: all but 0, 1 (root) and tip (prescribed).
    std::vector<int> free;
    for (int i = 2; i < N; ++i)
        if (i != tip)
            free.push_back(i);
    Eigen::MatrixXd Kff(free.size(), free.size());
    Eigen::VectorXd rhs(free.size());
    for (size_t i = 0; i < free.size(); ++i) {
        for (size_t j = 0; j < free.size(); ++j)
            Kff(static_cast<int>(i), static_cast<int>(j)) = K(free[i], free[j]);
        rhs(static_cast<int>(i)) = -K(free[i], tip) * delta;
    }
    const Eigen::VectorXd x = Kff.ldlt().solve(rhs);
    Eigen::VectorXd u = Eigen::VectorXd::Zero(N);
    for (size_t i = 0; i < free.size(); ++i)
        u(free[i]) = x(static_cast<int>(i));
    u(tip) = delta;
    return {u, (K * u)(tip)};
}

std::vector<double> frequencies(const Eigen::MatrixXd& K, const Eigen::MatrixXd& M, int count)
{
    const int N = static_cast<int>(K.rows());
    const Eigen::MatrixXd Kr = K.bottomRightCorner(N - 2, N - 2), Mr = M.bottomRightCorner(N - 2, N - 2);
    Eigen::GeneralizedSelfAdjointEigenSolver<Eigen::MatrixXd> eig(Kr, Mr);
    std::vector<double> f;
    for (int i = 0; i < std::min<int>(count, static_cast<int>(eig.eigenvalues().size())); ++i)
        f.push_back(std::sqrt(std::max(0.0, eig.eigenvalues()(i))) / (2 * kPi));
    return f;
}

}  // namespace

Status DamagedBeamSpec::validate() const
{
    if (!(length > 0.0) || !(width > 0.0) || !(thickness > 0.0) || !(density > 0.0))
        return Status::failure("the cantilever needs a positive length, width, thickness and density");
    if (Status v = damage.validate(); !v)
        return v;
    if (elements < 4 || elements > 400)
        return Status::failure("between 4 and 400 elements");
    if (modes < 1 || modes > 10)
        return Status::failure("between 1 and 10 modes");
    return Status::success();
}

Result<DamagedBeamResult> damagedCantilever(const DamagedBeamSpec& s)
{
    using R = Result<DamagedBeamResult>;
    if (Status v = s.validate(); !v)
        return R::failure(v.message());
    const int n = s.elements;
    const double h = s.length / n, I = s.width * std::pow(s.thickness, 3) / 12.0, A = s.width * s.thickness;
    const double EI0 = s.damage.youngsModulus * I;
    std::vector<DamageState> committed(static_cast<size_t>(n));
    std::vector<double> EI(static_cast<size_t>(n), EI0);
    const Eigen::MatrixXd M = assemble(n, h, EI, true, s.density * A);
    const Eigen::MatrixXd K0 = assemble(n, h, EI, false, 0.0);
    DamagedBeamResult out;
    out.undamagedFrequencies = frequencies(K0, M, s.modes);
    out.undamagedTipStiffness = deflect(K0, 1.0).second;
    double work = 0.0, previousLoad = 0.0, previousDelta = 0.0;
    std::vector<double> previousU0(static_cast<size_t>(n), 0.0);
    for (double delta : s.tipDeflections) {
        BeamStep step;
        step.deflection = delta;
        std::vector<DamageState> trial = committed;
        Eigen::VectorXd u;
        double load = 0.0;
        bool converged = false;
        for (int it = 0; it < 200; ++it) {
            for (int e = 0; e < n; ++e)
                EI[static_cast<size_t>(e)] = EI0 * (1.0 - trial[static_cast<size_t>(e)].damage);
            std::tie(u, load) = deflect(assemble(n, h, EI, false, 0.0), delta);
            double change = 0.0;
            for (int e = 0; e < n; ++e) {
                const double eps = maxCurvature(u, e, h) * 0.5 * s.thickness;
                const DamageResponse r = damageUpdate(s.damage, committed[static_cast<size_t>(e)], eps);
                change = std::max(change, std::abs(r.state.damage - trial[static_cast<size_t>(e)].damage));
                trial[static_cast<size_t>(e)] = r.state;
            }
            step.iterations = it + 1;
            step.residual = change;
            if (change <= s.tolerance) {
                converged = true;
                break;
            }
        }
        step.converged = converged;
        if (!converged) {
            out.steps.push_back(step);
            break;
        }
        // Dissipation: the undamaged element energy times dD, averaged over the step (trapezoid, like the work).
        double dissipated = 0.0;
        std::vector<double> U0(static_cast<size_t>(n));
        for (int e = 0; e < n; ++e) {
            const Eigen::Vector4d ue = u.segment<4>(2 * e);
            U0[static_cast<size_t>(e)] = 0.5 * ue.dot(elementStiffness(EI0, h) * ue);
            const double dD = trial[static_cast<size_t>(e)].damage - committed[static_cast<size_t>(e)].damage;
            trial[static_cast<size_t>(e)].dissipated =
                committed[static_cast<size_t>(e)].dissipated + 0.5 * (U0[static_cast<size_t>(e)] + previousU0[static_cast<size_t>(e)]) * dD;
            dissipated += trial[static_cast<size_t>(e)].dissipated;
        }
        previousU0 = U0;
        committed = trial;
        for (int e = 0; e < n; ++e)
            EI[static_cast<size_t>(e)] = EI0 * (1.0 - committed[static_cast<size_t>(e)].damage);
        const Eigen::MatrixXd K = assemble(n, h, EI, false, 0.0);
        work += 0.5 * (load + previousLoad) * (delta - previousDelta);
        previousLoad = load;
        previousDelta = delta;
        step.load = load;
        step.externalWork = work;
        step.storedEnergy = 0.5 * u.dot(K * u);
        step.dissipated = dissipated;
        for (const auto& d : committed)
            step.maxDamage = std::max(step.maxDamage, d.damage);
        step.rootDamage = committed.front().damage;
        out.steps.push_back(step);
    }
    for (int e = 0; e < n; ++e) {
        out.damage.push_back(committed[static_cast<size_t>(e)].damage);
        out.elementCentres.push_back((e + 0.5) * h);
        EI[static_cast<size_t>(e)] = EI0 * (1.0 - committed[static_cast<size_t>(e)].damage);
    }
    const Eigen::MatrixXd K = assemble(n, h, EI, false, 0.0);
    out.frequencies = frequencies(K, M, s.modes);
    out.damagedTipStiffness = deflect(K, 1.0).second;
    return out;
}

}  // namespace resonant::mechanics
