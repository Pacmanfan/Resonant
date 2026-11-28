#include "oscillators/LumpedNetwork.h"

#include "simulation/Job.h"

#include <Eigen/Eigenvalues>

#include <algorithm>
#include <cmath>
#include <limits>

namespace resonant::oscillators {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kInf = std::numeric_limits<double>::infinity();

void addBetween(Eigen::MatrixXd& m, int i, int j, double v)
{
    m(i, i) += v;
    if (j >= 0) {
        m(j, j) += v;
        m(i, j) -= v;
        m(j, i) -= v;
    }
}
}  // namespace

int Network::size() const
{
    return massMatrix.size() > 0 ? static_cast<int>(massMatrix.rows()) : static_cast<int>(masses.size());
}

Eigen::MatrixXd Network::M() const
{
    if (massMatrix.size() > 0)
        return massMatrix;
    Eigen::MatrixXd m = Eigen::MatrixXd::Zero(size(), size());
    for (int i = 0; i < size(); ++i)
        m(i, i) = masses[static_cast<size_t>(i)];
    return m;
}

Eigen::MatrixXd Network::K() const
{
    if (stiffnessMatrix.size() > 0)
        return stiffnessMatrix;
    Eigen::MatrixXd k = Eigen::MatrixXd::Zero(size(), size());
    for (const Spring& s : springs)
        addBetween(k, s.i, s.j, s.k);
    return k;
}

std::map<std::string, Eigen::MatrixXd> Network::dampingMatrices() const
{
    std::map<std::string, Eigen::MatrixXd> out = dampingByMechanism;
    for (const Damper& d : dampers) {
        auto [it, inserted] = out.try_emplace(d.mechanism, Eigen::MatrixXd::Zero(size(), size()));
        addBetween(it->second, d.i, d.j, d.c);
    }
    return out;
}

Eigen::MatrixXd Network::C() const
{
    Eigen::MatrixXd c = Eigen::MatrixXd::Zero(size(), size());
    for (const auto& [name, m] : dampingMatrices())
        c += m;
    return c;
}

Status Network::validate() const
{
    const int n = size();
    if (n < 1)
        return Status::failure("the network has no coordinate");
    if (massMatrix.size() == 0) {
        if (static_cast<int>(names.size()) != n)
            return Status::failure("every coordinate needs a name");
        for (double m : masses)
            if (!(m > 0.0) || !std::isfinite(m))
                return Status::failure("masses must be positive");
    }
    for (const Spring& s : springs)
        if (s.i < 0 || s.i >= n || s.j >= n || s.j == s.i || !(s.k >= 0.0) || !std::isfinite(s.k))
            return Status::failure("spring '" + s.name + "' is invalid (coordinates, or a negative stiffness)");
    for (const Damper& d : dampers)
        if (d.i < 0 || d.i >= n || d.j >= n || d.j == d.i || !(d.c >= 0.0) || !std::isfinite(d.c))
            return Status::failure("a damper of mechanism '" + d.mechanism + "' is invalid (coordinates, or a negative coefficient)");
    const Eigen::MatrixXd m = M();
    Eigen::LLT<Eigen::MatrixXd> llt(m);
    if (llt.info() != Eigen::Success)
        return Status::failure("the mass matrix is not positive definite");
    for (const auto& [name, c] : dampingMatrices())
        if (c.rows() != n || (c + c.transpose()).eval().selfadjointView<Eigen::Lower>().eigenvalues().minCoeff() < -1e-12 * (1.0 + c.norm()))
            return Status::failure("the damping of mechanism '" + name + "' is not passive (negative damping)");
    return Status::success();
}

Result<std::vector<NetworkMode>> naturalModes(const Network& net, int* rigidModes)
{
    using R = Result<std::vector<NetworkMode>>;
    if (Status v = net.validate(); !v)
        return R::failure(v.message());
    const int n = net.size();
    const Eigen::MatrixXd M = net.M(), K = net.K(), C = net.C();
    const Eigen::MatrixXd Minv = M.inverse();
    Eigen::MatrixXd A = Eigen::MatrixXd::Zero(2 * n, 2 * n);
    A.topRightCorner(n, n) = Eigen::MatrixXd::Identity(n, n);
    A.bottomLeftCorner(n, n) = -Minv * K;
    A.bottomRightCorner(n, n) = -Minv * C;
    Eigen::EigenSolver<Eigen::MatrixXd> eig(A);
    if (eig.info() != Eigen::Success)
        return R::failure("the eigenvalue problem did not converge");
    // Undamped modes for the per-mechanism modal Q (light damping): Q_i = w m / (phi^T C_i phi).
    Eigen::GeneralizedSelfAdjointEigenSolver<Eigen::MatrixXd> undamped(K, M);
    std::vector<NetworkMode> modes;
    int rigid = 0;
    const double scale = std::sqrt(std::max(1e-300, (Minv * K).norm()));
    for (int i = 0; i < 2 * n; ++i) {
        const std::complex<double> s = eig.eigenvalues()(i);
        if (s.imag() < 0.0)
            continue;
        if (std::abs(s) < 1e-9 * scale) {
            ++rigid;
            continue;
        }
        if (s.imag() <= 1e-12 * std::abs(s))
            continue;  // overdamped: no oscillation
        NetworkMode mode;
        mode.frequencyHz = s.imag() / (2.0 * kPi);
        mode.undampedHz = std::abs(s) / (2.0 * kPi);
        mode.decayRate = -s.real();
        mode.quality = mode.decayRate > 0.0 ? std::abs(s) / (2.0 * mode.decayRate) : kInf;
        const Eigen::VectorXcd v = eig.eigenvectors().col(i).head(n);
        // Normalize the phase so that the largest component is real.
        int big = 0;
        for (int k = 0; k < n; ++k)
            if (std::abs(v(k)) > std::abs(v(big)))
                big = k;
        const Eigen::VectorXcd w = v * std::conj(v(big)) / std::abs(v(big));
        const double top = w.cwiseAbs().maxCoeff();
        double kinetic = 0.0;
        for (int k = 0; k < n; ++k) {
            mode.shape.push_back(w(k).real() / top);
            kinetic += M(k, k) * std::norm(w(k));
        }
        for (int k = 0; k < n; ++k)
            mode.participation.push_back(kinetic > 0.0 ? M(k, k) * std::norm(w(k)) / kinetic : 0.0);
        if (n == 2 && std::abs(mode.shape[0]) > 1e-6 && std::abs(mode.shape[1]) > 1e-6)
            mode.symmetry = mode.shape[0] * mode.shape[1] > 0.0 ? "symmetric" : "antisymmetric";
        // The undamped mode nearest this frequency, for the loss breakdown.
        int nearest = -1;
        for (int k = 0; k < n; ++k) {
            const double wk = std::sqrt(std::max(0.0, undamped.eigenvalues()(k)));
            if (nearest < 0 || std::abs(wk - std::abs(s)) < std::abs(std::sqrt(std::max(0.0, undamped.eigenvalues()(nearest))) - std::abs(s)))
                nearest = k;
        }
        if (nearest >= 0) {
            const Eigen::VectorXd phi = undamped.eigenvectors().col(nearest);
            const double w0 = std::sqrt(std::max(0.0, undamped.eigenvalues()(nearest)));
            const double modalMass = phi.dot(M * phi);
            for (const auto& [name, c] : net.dampingMatrices()) {
                const double cm = phi.dot(c * phi);
                mode.qualityByMechanism[name] = cm > 0.0 ? w0 * modalMass / cm : kInf;
            }
        }
        modes.push_back(std::move(mode));
    }
    std::sort(modes.begin(), modes.end(), [](const NetworkMode& a, const NetworkMode& b) { return a.frequencyHz < b.frequencyHz; });
    if (rigidModes)
        *rigidModes = rigid;
    return modes;
}

Result<std::vector<DrivenPoint>> drivenResponse(const Network& net, const std::vector<std::complex<double>>& force, const std::vector<double>& frequencies)
{
    using R = Result<std::vector<DrivenPoint>>;
    if (Status v = net.validate(); !v)
        return R::failure(v.message());
    const int n = net.size();
    if (static_cast<int>(force.size()) != n)
        return R::failure("one force per coordinate");
    const Eigen::MatrixXcd M = net.M().cast<std::complex<double>>(), K = net.K().cast<std::complex<double>>();
    const auto damping = net.dampingMatrices();
    Eigen::MatrixXcd C = net.C().cast<std::complex<double>>();
    Eigen::VectorXcd F(n);
    for (int i = 0; i < n; ++i)
        F(i) = force[static_cast<size_t>(i)];
    std::vector<DrivenPoint> out;
    const std::complex<double> j(0.0, 1.0);
    for (double f : frequencies) {
        if (!(f > 0.0))
            return R::failure("frequencies must be positive");
        const double w = 2.0 * kPi * f;
        const Eigen::MatrixXcd D = K - w * w * M + j * w * C;
        const Eigen::VectorXcd X = D.fullPivLu().solve(F);
        DrivenPoint p;
        p.frequencyHz = f;
        for (int i = 0; i < n; ++i)
            p.displacement.push_back(X(i));
        const Eigen::VectorXcd V = j * w * X;
        p.suppliedPower = 0.5 * (F.dot(V)).real();  // F^H V; F.dot(V) = sum conj(F) V
        for (const auto& [name, c] : damping)
            p.dissipated[name] = 0.5 * (V.dot(c.cast<std::complex<double>>() * V)).real();
        p.kineticEnergy = 0.25 * (V.dot(M * V)).real();
        p.potentialEnergy = 0.25 * (X.dot(K * X)).real();
        out.push_back(std::move(p));
    }
    return out;
}

Result<TransientRecord> integrate(const Network& net, const TransientOptions& o, JobContext* context)
{
    using R = Result<TransientRecord>;
    if (Status v = net.validate(); !v)
        return R::failure(v.message());
    const int n = net.size();
    if (!(o.duration > 0.0))
        return R::failure("the duration must be positive");
    const Eigen::MatrixXd M = net.M(), K = net.K(), C = net.C(), Minv = M.inverse();
    const auto damping = net.dampingMatrices();
    double dt = o.dt;
    if (!(dt > 0.0)) {
        Eigen::GeneralizedSelfAdjointEigenSolver<Eigen::MatrixXd> e(K, M);
        const double wmax = std::sqrt(std::max(1e-300, e.eigenvalues().maxCoeff()));
        dt = 2.0 * kPi / wmax / 200.0;
    }
    const long steps = static_cast<long>(std::ceil(o.duration / dt));
    if (steps > 50'000'000)
        return R::failure("the run would take more than 50 million steps; shorten it or raise dt");
    dt = o.duration / static_cast<double>(steps);
    Eigen::VectorXd x = Eigen::VectorXd::Zero(n), v = Eigen::VectorXd::Zero(n);
    if (!o.x0.empty())
        for (int i = 0; i < n; ++i)
            x(i) = o.x0.at(static_cast<size_t>(i));
    if (!o.v0.empty())
        for (int i = 0; i < n; ++i)
            v(i) = o.v0.at(static_cast<size_t>(i));
    const auto forceAt = [&](double t) {
        Eigen::VectorXd f = Eigen::VectorXd::Zero(n);
        if (o.force) {
            const std::vector<double> g = o.force(t);
            for (int i = 0; i < n && i < static_cast<int>(g.size()); ++i)
                f(i) = g[static_cast<size_t>(i)];
        }
        return f;
    };
    // Grounded spring of each coordinate (its own stiffness) and the coupling springs.
    std::vector<double> own(static_cast<size_t>(n), 0.0);
    std::vector<Spring> couplings;
    for (const Spring& s : net.springs) {
        if (s.j < 0)
            own[static_cast<size_t>(s.i)] += s.k;
        else
            couplings.push_back(s);
    }
    const bool matrices = net.stiffnessMatrix.size() > 0;
    const auto stored = [&](const Eigen::VectorXd& xx, const Eigen::VectorXd& vv) { return 0.5 * vv.dot(M * vv) + 0.5 * xx.dot(K * xx); };
    TransientRecord rec;
    rec.dt = dt;
    rec.steps = steps;
    rec.displacement.assign(static_cast<size_t>(n), {});
    rec.energy.assign(static_cast<size_t>(n), {});
    const long every = std::max<long>(1, steps / std::max(1, o.samples));
    double supplied = 0.0;
    std::map<std::string, double> lost;
    for (const auto& [name, c] : damping)
        lost[name] = 0.0;
    const double e0 = stored(x, v);
    double peak = std::max(e0, 1e-300), worst = 0.0;
    const auto record = [&](double t) {
        rec.time.push_back(t);
        double coupling = 0.0;
        for (const Spring& s : couplings)
            coupling += 0.5 * s.k * std::pow(x(s.i) - x(s.j), 2);
        for (int i = 0; i < n; ++i) {
            rec.displacement[static_cast<size_t>(i)].push_back(x(i));
            // Matrices without element structure: each coordinate's kinetic energy and its diagonal stiffness share.
            const double pot = matrices ? 0.5 * K(i, i) * x(i) * x(i) : 0.5 * own[static_cast<size_t>(i)] * x(i) * x(i);
            rec.energy[static_cast<size_t>(i)].push_back(0.5 * M(i, i) * v(i) * v(i) + pot);
        }
        rec.couplingEnergy.push_back(coupling);
        double d = 0.0;
        for (const auto& [name, value] : lost)
            d += value;
        rec.supplied.push_back(supplied);
        rec.dissipated.push_back(d);
    };
    record(0.0);
    // State derivative and the power densities (supply, dissipation per mechanism) at a state.
    const auto rhs = [&](double t, const Eigen::VectorXd& xx, const Eigen::VectorXd& vv, Eigen::VectorXd& dx, Eigen::VectorXd& dv, double& p,
                         std::map<std::string, double>& d) {
        const Eigen::VectorXd f = forceAt(t);
        dx = vv;
        dv = Minv * (f - C * vv - K * xx);
        p = f.dot(vv);
        for (const auto& [name, c] : damping)
            d[name] = vv.dot(c * vv);
    };
    Eigen::VectorXd k1x, k1v, k2x, k2v, k3x, k3v, k4x, k4v;
    std::map<std::string, double> d1, d2, d3, d4;
    for (long s = 0; s < steps; ++s) {
        if (context && (s & 4095) == 0) {
            if (context->cancelRequested())
                return R::failure("cancelled");
            context->reportProgress(static_cast<double>(s) / static_cast<double>(steps), "integrating");
        }
        const double t = static_cast<double>(s) * dt;
        double p1, p2, p3, p4;
        rhs(t, x, v, k1x, k1v, p1, d1);
        rhs(t + 0.5 * dt, x + 0.5 * dt * k1x, v + 0.5 * dt * k1v, k2x, k2v, p2, d2);
        rhs(t + 0.5 * dt, x + 0.5 * dt * k2x, v + 0.5 * dt * k2v, k3x, k3v, p3, d3);
        rhs(t + dt, x + dt * k3x, v + dt * k3v, k4x, k4v, p4, d4);
        x += dt / 6.0 * (k1x + 2.0 * k2x + 2.0 * k3x + k4x);
        v += dt / 6.0 * (k1v + 2.0 * k2v + 2.0 * k3v + k4v);
        supplied += dt / 6.0 * (p1 + 2.0 * p2 + 2.0 * p3 + p4);
        for (auto& [name, value] : lost)
            value += dt / 6.0 * (d1[name] + 2.0 * d2[name] + 2.0 * d3[name] + d4[name]);
        double total = 0.0;
        for (const auto& [name, value] : lost)
            total += value;
        const double now = stored(x, v);
        peak = std::max(peak, now);
        worst = std::max(worst, std::abs(supplied - (now - e0) - total));
        if ((s + 1) % every == 0 || s + 1 == steps)
            record(static_cast<double>(s + 1) * dt);
    }
    rec.dissipatedByMechanism = lost;
    rec.residual = worst / std::max(peak, std::abs(supplied));
    return rec;
}

Result<EnergyExchange> energyExchange(const Network& net, int from, int to, double duration, JobContext* context)
{
    using R = Result<EnergyExchange>;
    if (from < 0 || to < 0 || from >= net.size() || to >= net.size() || from == to)
        return R::failure("two different coordinates of the network");
    auto modes = naturalModes(net);
    if (!modes)
        return R::failure(modes.message());
    TransientOptions o;
    o.duration = duration;
    o.samples = 4000;
    o.x0.assign(static_cast<size_t>(net.size()), 0.0);
    o.x0[static_cast<size_t>(from)] = 1.0;
    auto rec = integrate(net, o, context);
    if (!rec)
        return R::failure(rec.message());
    const auto& r = rec.value();
    double e0 = r.energy[static_cast<size_t>(from)][0] + r.couplingEnergy[0];
    for (size_t i = 0; i < r.energy.size(); ++i)
        if (static_cast<int>(i) != from)
            e0 += r.energy[i][0];
    EnergyExchange out;
    const auto& target = r.energy[static_cast<size_t>(to)];
    size_t best = 0;
    for (size_t k = 1; k < target.size(); ++k) {
        if (target[k] > target[best])
            best = k;
        // The first maximum: stop once the energy has fallen well below it.
        if (target[best] > 0.05 * e0 && target[k] < 0.5 * target[best])
            break;
    }
    out.maximumTransferFraction = target[best] / e0;
    out.transferTime = r.time[best];
    out.remainingFraction = r.energy[static_cast<size_t>(from)][best] / e0;
    if (modes.value().size() >= 2)
        out.beatPeriod = 1.0 / std::abs(modes.value()[1].frequencyHz - modes.value()[0].frequencyHz);
    return out;
}

CoupledModeEstimate coupledModeEstimate(double m1, double k1, double m2, double k2, double kc)
{
    CoupledModeEstimate e;
    // Each resonator loaded by the coupling spring.
    const double w1 = std::sqrt((k1 + kc) / m1), w2 = std::sqrt((k2 + kc) / m2);
    e.couplingRate = kc / (2.0 * std::sqrt(m1 * m2) * std::sqrt(w1 * w2));
    const double mean = 0.5 * (w1 + w2), half = 0.5 * (w1 - w2);
    const double split = std::sqrt(half * half + e.couplingRate * e.couplingRate);
    e.lowerHz = (mean - split) / (2.0 * kPi);
    e.upperHz = (mean + split) / (2.0 * kPi);
    e.splittingHz = e.upperHz - e.lowerHz;
    e.maximumTransferFraction = 4.0 * e.couplingRate * e.couplingRate / (4.0 * e.couplingRate * e.couplingRate + (w1 - w2) * (w1 - w2));
    return e;
}

}  // namespace resonant::oscillators
