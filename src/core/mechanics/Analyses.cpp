#include "mechanics/Analyses.h"

#include <Eigen/SparseCholesky>
#include <Eigen/SparseLU>

#include <chrono>
#include <cmath>
#include <cstdio>

namespace resonant::mechanics {
namespace {

constexpr double twoPi = 6.283185307179586476925;

Eigen::VectorXd totalLoad(const StructuralModel& model, const std::vector<ForceInput>& forces, double scaleOf(const ForceInput&))
{
    Eigen::VectorXd f = Eigen::VectorXd::Zero(static_cast<Eigen::Index>(model.dofCount()));
    for (const ForceInput& force : forces)
        f += scaleOf(force) * force.pattern;
    return f;
}

std::string number(double v)
{
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%.4g", v);
    return buffer;
}

}  // namespace

std::string_view toString(DampingKind kind)
{
    switch (kind) {
    case DampingKind::None: return "none";
    case DampingKind::Rayleigh: return "rayleigh";
    case DampingKind::LossFactor: return "lossFactor";
    case DampingKind::ModalRatio: return "modalRatio";
    }
    return "none";
}

double Damping::ratioAt(double omega) const
{
    switch (kind) {
    case DampingKind::None: return 0.0;
    case DampingKind::Rayleigh: return omega > 0.0 ? alpha / (2.0 * omega) + beta * omega / 2.0 : 0.0;
    case DampingKind::LossFactor: return eta / 2.0;
    case DampingKind::ModalRatio: return zeta;
    }
    return 0.0;
}

std::string Damping::describe() const
{
    switch (kind) {
    case DampingKind::None: return "undamped";
    case DampingKind::Rayleigh: return "proportional (Rayleigh) damping C = " + number(alpha) + " M + " + number(beta) + " K";
    case DampingKind::LossFactor:
        return std::string("structural loss factor: complex stiffness K (1 + j eta)") +
               (materialLossFactors ? ", eta of each material where it states one, otherwise " : ", eta = ") + number(eta);
    case DampingKind::ModalRatio: return "modal damping ratio " + number(zeta) + " in every mode";
    }
    return "";
}

Status Damping::validate() const
{
    if (!(std::isfinite(alpha) && alpha >= 0.0 && std::isfinite(beta) && beta >= 0.0))
        return Status::failure("Rayleigh coefficients must be zero or positive");
    if (!(std::isfinite(eta) && eta >= 0.0 && eta < 2.0))
        return Status::failure("the loss factor must be between 0 and 2");
    if (!(std::isfinite(zeta) && zeta >= 0.0 && zeta < 1.0))
        return Status::failure("the modal damping ratio must be between 0 and 1");
    return Status::success();
}

size_t nearestNode(const StructuralModel& model, const Vec3& point, double* distance)
{
    size_t best = 0;
    double bestDistance = std::numeric_limits<double>::infinity();
    for (size_t n = 0; n < model.nodes.size(); ++n) {
        const double d = (model.nodes[n] - point).norm();
        if (d < bestDistance) {
            bestDistance = d;
            best = n;
        }
    }
    if (distance)
        *distance = bestDistance;
    return best;
}

ResponsePoint responseAt(const StructuralModel& model, size_t node, const Vec3& direction, std::string label)
{
    ResponsePoint p;
    p.label = std::move(label);
    p.node = node;
    p.position = model.nodes[node];
    p.direction = direction.normalized();
    p.selector = Eigen::VectorXd::Zero(static_cast<Eigen::Index>(model.dofCount()));
    for (int d = 0; d < std::min(3, model.dofsPerNode()); ++d)
        p.selector(static_cast<Eigen::Index>(model.dof(node, d))) = p.direction.dot(model.dofDirection(d));
    return p;
}

Eigen::VectorXd forcePattern(const StructuralModel& model, size_t node, const Vec3& direction, double* lostFraction)
{
    Eigen::VectorXd f = Eigen::VectorXd::Zero(static_cast<Eigen::Index>(model.dofCount()));
    const Vec3 d = direction.normalized();
    double carried = 0.0;
    for (int local = 0; local < std::min(3, model.dofsPerNode()); ++local) {
        const double component = d.dot(model.dofDirection(local));
        f(static_cast<Eigen::Index>(model.dof(node, local))) = component;
        carried += component * component;
    }
    if (lostFraction)
        *lostFraction = 1.0 - std::sqrt(std::min(1.0, carried));
    return f;
}

Eigen::VectorXd distributedPattern(const StructuralModel& model, const Vec3& point, double radius, const Vec3& direction,
                                   double* lostFraction, size_t* nodeCount)
{
    std::vector<size_t> nodes;
    for (size_t n = 0; n < model.nodes.size(); ++n)
        if ((model.nodes[n] - point).norm() <= radius)
            nodes.push_back(n);
    if (nodes.empty())
        nodes.push_back(nearestNode(model, point));
    Eigen::VectorXd f = Eigen::VectorXd::Zero(static_cast<Eigen::Index>(model.dofCount()));
    for (const size_t n : nodes)
        f += forcePattern(model, n, direction, lostFraction) / static_cast<double>(nodes.size());
    if (nodeCount)
        *nodeCount = nodes.size();
    return f;
}

// --- static ------------------------------------------------------------------------------

Result<StaticSolution> solveStatic(const StructuralModel& model, const SystemMatrices& system, const std::vector<ForceInput>& forces)
{
    using Failure = Result<StaticSolution>;
    if (system.map.freeCount() == 0)
        return Failure::failure("every degree of freedom is fixed");
    const Eigen::VectorXd f = totalLoad(model, forces, [](const ForceInput& force) { return force.amplitude * std::cos(force.phase); });
    // A structure that is not held can move as a rigid body: K is singular,
    // which shows as a failed factorization or a pivot at rounding level.
    const char* const notHeld = "the structure is not held: it can move as a rigid body. Add supports before a static analysis";
    Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> ldlt(system.stiffness);
    if (ldlt.info() != Eigen::Success)
        return Failure::failure(notHeld);
    const Eigen::VectorXd d = ldlt.vectorD();
    if (d.minCoeff() <= 1e-9 * d.cwiseAbs().maxCoeff())
        return Failure::failure(notHeld);
    StaticSolution out;
    out.displacement = system.map.expand(Eigen::VectorXd(ldlt.solve(system.map.reduce(f))));
    out.compliance = f.dot(out.displacement);
    for (size_t n = 0; n < model.nodes.size(); ++n)
        out.maxTranslation = std::max(out.maxTranslation, model.translation(out.displacement, n).norm());
    return out;
}

// --- modal -------------------------------------------------------------------------------

Result<ModalSolution> solveModal(const StructuralModel& model, const SystemMatrices& system, int count, JobContext* context, bool forceIterative)
{
    // The shift: below every eigenvalue, scaled to the problem (a small
    // fraction of the mean diagonal ratio of K and M).
    const double ratio = system.stiffness.diagonal().cwiseAbs().mean() / std::max(1e-300, system.mass.diagonal().cwiseAbs().mean());
    const double shift = -1e-4 * ratio;
    auto solved = lowestEigenpairs(system.stiffness, system.mass, count, shift, 1e-8, context, forceIterative);
    if (!solved)
        return Result<ModalSolution>::failure(solved.message());
    const EigenSolution& e = solved.value();
    ModalSolution out;
    out.method = e.method;
    // Influence vectors of rigid translation along X, Y, Z.
    std::array<Eigen::VectorXd, 3> influence;
    for (int axis = 0; axis < 3; ++axis) {
        Eigen::VectorXd r = Eigen::VectorXd::Zero(static_cast<Eigen::Index>(model.dofCount()));
        for (size_t n = 0; n < model.nodes.size(); ++n)
            for (int d = 0; d < std::min(3, model.dofsPerNode()); ++d)
                r(static_cast<Eigen::Index>(model.dof(n, d))) = model.dofDirection(d)(axis);
        influence[static_cast<size_t>(axis)] = system.map.reduce(r);
        out.totalMass(axis) = influence[static_cast<size_t>(axis)].dot(system.mass * influence[static_cast<size_t>(axis)]);
    }
    for (Eigen::Index r = 0; r < e.values.size(); ++r) {
        Mode mode;
        mode.omega = std::sqrt(std::max(0.0, e.values(r)));
        mode.frequencyHz = mode.omega / twoPi;
        mode.shape = system.map.expand(Eigen::VectorXd(e.vectors.col(r)));
        mode.residual = e.residuals[static_cast<size_t>(r)];
        const Eigen::VectorXd mphi = system.mass * e.vectors.col(r);
        for (int axis = 0; axis < 3; ++axis)
            if (out.totalMass(axis) > 0.0) {
                const double gamma = mphi.dot(influence[static_cast<size_t>(axis)]);
                mode.effectiveMassFraction(axis) = gamma * gamma / out.totalMass(axis);
            }
        out.cumulativeMassFraction += mode.effectiveMassFraction;
        out.modes.push_back(std::move(mode));
    }
    return out;
}

// --- harmonic ----------------------------------------------------------------------------

Eigen::SparseMatrix<Complex> dynamicStiffness(const SystemMatrices& system, double omega, const Damping& damping)
{
    const Complex j(0.0, 1.0);
    Eigen::SparseMatrix<Complex> d = system.stiffness.cast<Complex>() - (omega * omega) * system.mass.cast<Complex>();
    if (damping.kind == DampingKind::Rayleigh)
        d += (j * omega) * (damping.alpha * system.mass + damping.beta * system.stiffness).cast<Complex>();
    if (damping.kind == DampingKind::LossFactor) {
        if (damping.materialLossFactors && system.lossStiffness.nonZeros() > 0) {
            // Elements whose material states a loss factor use it; the rest use eta.
            // K_eta = sum eta_e K_e + eta (K - sum_{stated} K_e) is approximated by
            // the stated part plus eta K when some elements state none.
            d += j * system.lossStiffness.cast<Complex>();
            if (damping.eta > 0.0)
                d += (j * damping.eta) * system.stiffness.cast<Complex>();
        } else {
            d += (j * damping.eta) * system.stiffness.cast<Complex>();
        }
    }
    return d;
}

Result<HarmonicSolution> solveHarmonic(const StructuralModel& model, const SystemMatrices& system, const std::vector<ForceInput>& forces,
                                       const std::vector<ResponsePoint>& points, const HarmonicSettings& settings, JobContext* context)
{
    using Failure = Result<HarmonicSolution>;
    if (Status status = settings.damping.validate(); !status)
        return Failure::failure(status.message());
    if (settings.frequenciesHz.empty())
        return Failure::failure("no frequencies to calculate");
    for (const double f : settings.frequenciesHz)
        if (!(std::isfinite(f) && f > 0.0))
            return Failure::failure("frequencies must be positive");
    if (forces.empty())
        return Failure::failure("there is no active actuator on the structure");
    if (settings.damping.kind == DampingKind::ModalRatio && settings.method == HarmonicMethod::Direct)
        return Failure::failure("a modal damping ratio exists only in modal superposition; choose the modal method or another damping model");

    HarmonicSolution out;
    out.frequenciesHz = settings.frequenciesHz;
    out.responses.assign(points.size(), std::vector<Complex>(settings.frequenciesHz.size()));
    const size_t fieldBytes = model.dofCount() * sizeof(Complex) * settings.frequenciesHz.size();
    const bool keep = settings.keepFields && fieldBytes <= settings.fieldMemoryLimit;
    out.fieldsNote = keep ? std::string()
                          : settings.keepFields ? "the full fields at every frequency would need " + number(fieldBytes / 1048576.0)
                                                      + " MiB, more than the limit: only the response points are kept"
                                                : "full fields not kept";

    std::optional<ModalSolution> modes;
    if (settings.method == HarmonicMethod::Modal) {
        auto solved = solveModal(model, system, settings.modeCount, context);
        if (!solved)
            return Failure::failure(solved.message());
        modes = solved.take();
        for (const Mode& mode : modes->modes)
            out.modeFrequenciesHz.push_back(mode.frequencyHz);
        out.method = "modal superposition of the lowest " + std::to_string(modes->modes.size()) + " modes (up to "
                     + number(out.modeFrequenciesHz.back()) + " Hz): the contribution of higher modes is left out";
    } else {
        out.method = "direct solution of the dynamic stiffness at every frequency (sparse LU)";
    }

    Eigen::SparseLU<Eigen::SparseMatrix<Complex>> lu;
    bool analyzed = false;
    for (size_t i = 0; i < settings.frequenciesHz.size(); ++i) {
        if (context) {
            if (context->cancelRequested())
                return Failure::failure("cancelled");
            context->reportProgress(static_cast<double>(i) / static_cast<double>(settings.frequenciesHz.size()),
                                    "frequency " + std::to_string(i + 1) + " of " + std::to_string(settings.frequenciesHz.size()));
        }
        const double omega = twoPi * settings.frequenciesHz[i];
        Eigen::VectorXcd f = Eigen::VectorXcd::Zero(static_cast<Eigen::Index>(model.dofCount()));
        for (const ForceInput& force : forces)
            f += force.phasor(omega) * force.pattern.cast<Complex>();
        Eigen::VectorXcd u;
        if (modes) {
            u = Eigen::VectorXcd::Zero(static_cast<Eigen::Index>(model.dofCount()));
            for (const Mode& mode : modes->modes) {
                const Complex modal = mode.shape.cast<Complex>().dot(f);  // phi^T f (phi real)
                Complex denominator;
                if (settings.damping.kind == DampingKind::LossFactor)
                    denominator = mode.omega * mode.omega * Complex(1.0, settings.damping.eta) - omega * omega;
                else
                    denominator = mode.omega * mode.omega - omega * omega
                                  + Complex(0.0, 2.0 * settings.damping.ratioAt(mode.omega) * mode.omega * omega);
                u += (modal / denominator) * mode.shape.cast<Complex>();
            }
        } else {
            const Eigen::SparseMatrix<Complex> d = dynamicStiffness(system, omega, settings.damping);
            if (!analyzed) {
                lu.analyzePattern(d);
                analyzed = true;
            }
            lu.factorize(d);
            if (lu.info() != Eigen::Success)
                return Failure::failure("the dynamic stiffness is singular at " + number(settings.frequenciesHz[i])
                                        + " Hz: an undamped natural frequency. Add damping or move the frequency");
            Eigen::VectorXcd reduced(static_cast<Eigen::Index>(system.map.freeCount()));
            for (size_t k = 0; k < system.map.freeCount(); ++k)
                reduced(static_cast<Eigen::Index>(k)) = f(static_cast<Eigen::Index>(system.map.model[k]));
            u = system.map.expand(Eigen::VectorXcd(lu.solve(reduced)));
        }
        for (size_t p = 0; p < points.size(); ++p)
            out.responses[p][i] = points[p].selector.cast<Complex>().dot(u);
        if (keep)
            out.fields.push_back(std::move(u));
    }
    return out;
}

// --- transient ---------------------------------------------------------------------------

double newmarkPeriodError(double omega, double dt)
{
    const double numerical = 2.0 * std::atan(0.5 * omega * dt) / dt;
    return omega / numerical - 1.0;
}

Result<TransientSolution> solveTransient(const StructuralModel& /*model*/, const SystemMatrices& system, const std::vector<ForceInput>& forces,
                                         const std::vector<ResponsePoint>& points, const TransientSettings& settings, JobContext* context,
                                         const Eigen::VectorXd* initialDisplacement)
{
    using Failure = Result<TransientSolution>;
    if (!(settings.timeStep > 0.0 && settings.duration > 0.0))
        return Failure::failure("the duration and the time step must be positive");
    if (settings.damping.kind == DampingKind::LossFactor || settings.damping.kind == DampingKind::ModalRatio)
        return Failure::failure("a " + std::string(settings.damping.kind == DampingKind::LossFactor ? "loss factor" : "modal damping ratio")
                                + " has no time-domain equivalent in this solver; use Rayleigh damping or none");
    if (Status status = settings.damping.validate(); !status)
        return Failure::failure(status.message());
    for (const ForceInput& force : forces)
        if (Status status = force.waveform.validate(); !status)
            return Failure::failure(status.message());

    const auto started = std::chrono::steady_clock::now();
    const double dt = settings.timeStep, beta = 0.25, gamma = 0.5;
    const long steps = std::max<long>(1, std::lround(std::ceil(settings.duration / dt)));
    const auto n = static_cast<Eigen::Index>(system.map.freeCount());
    const Eigen::SparseMatrix<double>& k = system.stiffness;
    const Eigen::SparseMatrix<double>& m = system.mass;
    Eigen::SparseMatrix<double> c(n, n);
    if (settings.damping.kind == DampingKind::Rayleigh)
        c = settings.damping.alpha * m + settings.damping.beta * k;

    std::vector<Eigen::VectorXd> patterns;
    for (const ForceInput& force : forces)
        patterns.push_back(system.map.reduce(force.pattern));
    const auto load = [&](double t) {
        Eigen::VectorXd f = Eigen::VectorXd::Zero(n);
        for (size_t i = 0; i < forces.size(); ++i)
            f += forces[i].amplitude * forces[i].waveform.value(t - forces[i].delay) * patterns[i];
        return f;
    };

    Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> effective(k + (gamma / (beta * dt)) * c + (1.0 / (beta * dt * dt)) * m);
    Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> massSolver(m);
    if (effective.info() != Eigen::Success || massSolver.info() != Eigen::Success)
        return Failure::failure("the system matrices could not be factorized");

    Eigen::VectorXd u = initialDisplacement ? system.map.reduce(*initialDisplacement) : Eigen::VectorXd::Zero(n);
    Eigen::VectorXd v = Eigen::VectorXd::Zero(n);
    Eigen::VectorXd f = load(0.0);
    Eigen::VectorXd a = massSolver.solve(f - k * u - c * v);

    TransientSolution out;
    out.dt = dt;
    out.steps = steps;
    out.time.reserve(static_cast<size_t>(steps + 1));
    out.displacement.assign(points.size(), {});
    out.velocity.assign(points.size(), {});
    out.acceleration.assign(points.size(), {});
    std::vector<Eigen::VectorXd> selectors;
    for (const ResponsePoint& p : points)
        selectors.push_back(system.map.reduce(p.selector));
    const long snapshotTotal = std::min<long>(settings.snapshotCount, steps);
    long snapshotsTaken = 0;
    const long energyEvery = std::max<long>(1, steps / std::max(1, settings.energySamples));
    double work = 0.0;
    const auto record = [&](long step) {
        const double t = static_cast<double>(step) * dt;
        out.time.push_back(t);
        for (size_t p = 0; p < points.size(); ++p) {
            out.displacement[p].push_back(selectors[p].dot(u));
            out.velocity[p].push_back(selectors[p].dot(v));
            out.acceleration[p].push_back(selectors[p].dot(a));
        }
        if (step % energyEvery == 0 || step == steps) {
            const double kinetic = 0.5 * v.dot(m * v), strain = 0.5 * u.dot(k * u);
            out.energyTimes.push_back(t);
            out.kinetic.push_back(kinetic);
            out.strain.push_back(strain);
            out.externalWork.push_back(work);
            out.dissipated.push_back(work - kinetic - strain);
        }
        if (snapshotTotal > 0 && snapshotsTaken < snapshotTotal
            && step >= std::lround(static_cast<double>(snapshotsTaken + 1) * static_cast<double>(steps) / static_cast<double>(snapshotTotal))) {
            ++snapshotsTaken;
            out.snapshotTimes.push_back(t);
            out.snapshots.push_back(system.map.expand(u));
        }
    };
    record(0);
    for (long step = 1; step <= steps; ++step) {
        const double t = static_cast<double>(step) * dt;
        const Eigen::VectorXd fNext = load(t);
        Eigen::VectorXd rhs = fNext + m * (u / (beta * dt * dt) + v / (beta * dt) + (0.5 / beta - 1.0) * a);
        if (settings.damping.kind == DampingKind::Rayleigh)
            rhs += c * (gamma / (beta * dt) * u + (gamma / beta - 1.0) * v + dt * (0.5 * gamma / beta - 1.0) * a);
        const Eigen::VectorXd uNext = effective.solve(rhs);
        const Eigen::VectorXd aNext = (uNext - u) / (beta * dt * dt) - v / (beta * dt) - (0.5 / beta - 1.0) * a;
        const Eigen::VectorXd vNext = v + dt * ((1.0 - gamma) * a + gamma * aNext);
        work += 0.5 * (f + fNext).dot(uNext - u);
        u = uNext;
        v = vNext;
        a = aNext;
        f = fNext;
        record(step);
        if (step % 200 == 0) {
            if (!u.allFinite())
                return Failure::failure("the solution stopped being finite at step " + std::to_string(step));
            if (context) {
                if (context->cancelRequested())
                    return Failure::failure("cancelled");
                context->reportProgress(static_cast<double>(step) / static_cast<double>(steps),
                                        "step " + std::to_string(step) + " of " + std::to_string(steps));
            }
        }
    }
    out.elapsedSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    return out;
}

}  // namespace resonant::mechanics
