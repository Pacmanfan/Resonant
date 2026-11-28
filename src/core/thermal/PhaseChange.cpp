#include "thermal/PhaseChange.h"

#include <algorithm>
#include <cmath>

namespace resonant::thermal {

namespace {
constexpr double kPi = 3.14159265358979323846;
}

Status PhaseMaterial::validate() const
{
    if (!(density > 0.0) || !(solidSpecificHeat > 0.0) || !(liquidSpecificHeat > 0.0) || !(solidConductivity > 0.0) ||
        !(liquidConductivity > 0.0))
        return Status::failure("the phase-change material needs positive density, specific heats and conductivities");
    if (!(meltingTemperature > 0.0) || !(latentHeat > 0.0))
        return Status::failure("the phase-change material needs a melting temperature and a latent heat");
    return Status::success();
}

Status PhaseChangeProblem::validate() const
{
    if (Status v = material.validate(); !v)
        return v;
    if (!(length > 0.0) || cells < 4 || cells > 20000)
        return Status::failure("the slab needs a positive length and 4..20000 cells");
    if (!(initialTemperature > 0.0))
        return Status::failure("the initial temperature must be positive (K)");
    return Status::success();
}

Result<PhaseChangeResult> solvePhaseChange(const PhaseChangeProblem& p, const PhaseChangeSettings& s, const std::function<bool()>& cancelled)
{
    using R = Result<PhaseChangeResult>;
    if (Status v = p.validate(); !v)
        return R::failure(v.message());
    if (!(s.duration > 0.0))
        return R::failure("the duration must be positive");
    const PhaseMaterial& m = p.material;
    const int n = p.cells;
    const double dx = p.length / n, rho = m.density, rhoL = rho * m.latentHeat, Tm = m.meltingTemperature;
    const auto enthalpy = [&](double T) {
        return T < Tm ? rho * m.solidSpecificHeat * (T - Tm) : T > Tm ? rhoL + rho * m.liquidSpecificHeat * (T - Tm) : 0.0;
    };
    const auto state = [&](double H, double& T, double& f) {
        if (H < 0.0) {
            T = Tm + H / (rho * m.solidSpecificHeat);
            f = 0.0;
        } else if (H <= rhoL) {
            T = Tm;
            f = H / rhoL;
        } else {
            T = Tm + (H - rhoL) / (rho * m.liquidSpecificHeat);
            f = 1.0;
        }
    };
    PhaseChangeResult out;
    const double kMax = std::max(m.solidConductivity, m.liquidConductivity);
    const double cMin = std::min(m.solidSpecificHeat, m.liquidSpecificHeat);
    out.stabilityLimit = rho * cMin * dx * dx / (4.0 * kMax);
    const double dt = s.timeStep > 0.0 ? s.timeStep : 0.9 * out.stabilityLimit;
    if (dt > out.stabilityLimit)
        return R::failure("the time step " + std::to_string(dt) + " s exceeds the explicit stability limit " + std::to_string(out.stabilityLimit) +
                          " s: reduce it or use fewer cells");
    out.timeStep = dt;
    std::vector<double> H(static_cast<size_t>(n), enthalpy(p.initialTemperature)), T(static_cast<size_t>(n)), f(static_cast<size_t>(n)),
        k(static_cast<size_t>(n));
    const std::vector<double> H0 = H;
    for (int i = 0; i < n; ++i)
        out.centres.push_back((i + 0.5) * dx);
    const auto update = [&] {
        for (int i = 0; i < n; ++i) {
            state(H[static_cast<size_t>(i)], T[static_cast<size_t>(i)], f[static_cast<size_t>(i)]);
            k[static_cast<size_t>(i)] = (1.0 - f[static_cast<size_t>(i)]) * m.solidConductivity + f[static_cast<size_t>(i)] * m.liquidConductivity;
        }
    };
    const auto faceFlux = [&](const SlabFace& face, int cell) {  // into the slab
        switch (face.kind) {
        case SlabFace::Kind::Adiabatic: return 0.0;
        case SlabFace::Kind::Flux: return face.value;
        case SlabFace::Kind::Temperature: return 2.0 * k[static_cast<size_t>(cell)] / dx * (face.value - T[static_cast<size_t>(cell)]);
        }
        return 0.0;
    };
    const auto snapshot = [&](double t, double qLeft) {
        out.times.push_back(t);
        out.temperature.push_back(T);
        out.liquidFraction.push_back(f);
        double melted = 0.0;
        for (double x : f)
            melted += x * dx;
        out.front.push_back(melted);
        out.leftFlux.push_back(qLeft);
    };
    update();
    const int steps = static_cast<int>(std::ceil(s.duration / dt - 1e-9));
    const int every = std::max(1, steps / std::max(1, s.snapshots));
    snapshot(0.0, faceFlux(p.left, 0));
    std::vector<double> q(static_cast<size_t>(n + 1));
    double t = 0.0;
    for (int step = 1; step <= steps; ++step) {
        if (cancelled && step % 1000 == 0 && cancelled())
            return R::failure("cancelled");
        const double h = std::min(dt, s.duration - t);
        q[0] = faceFlux(p.left, 0);
        q[static_cast<size_t>(n)] = -faceFlux(p.right, n - 1);
        for (int i = 1; i < n; ++i) {
            const double G = 1.0 / (0.5 * dx / k[static_cast<size_t>(i - 1)] + 0.5 * dx / k[static_cast<size_t>(i)]);
            q[static_cast<size_t>(i)] = G * (T[static_cast<size_t>(i - 1)] - T[static_cast<size_t>(i)]);
        }
        for (int i = 0; i < n; ++i)
            H[static_cast<size_t>(i)] += h / dx * (q[static_cast<size_t>(i)] - q[static_cast<size_t>(i + 1)]);
        out.heatIn += h * (q[0] - q[static_cast<size_t>(n)]);
        t += h;
        update();
        if (step % every == 0 || step == steps)
            snapshot(t, q[0]);
    }
    out.steps = steps;
    double total = 0.0, latent = 0.0;
    const double f0 = H0[0] < 0.0 ? 0.0 : std::min(1.0, H0[0] / rhoL);
    for (int i = 0; i < n; ++i) {
        total += (H[static_cast<size_t>(i)] - H0[static_cast<size_t>(i)]) * dx;
        latent += rhoL * (f[static_cast<size_t>(i)] - f0) * dx;
    }
    out.latentChange = latent;
    out.sensibleChange = total - latent;
    out.balanceResidual = out.heatIn - total;
    return out;
}

double stefanLambda(double st)
{
    const auto g = [&](double l) { return l * std::exp(l * l) * std::erf(l) - st / std::sqrt(kPi); };
    double lo = 0.0, hi = 3.0;
    for (int i = 0; i < 200; ++i) {
        const double mid = 0.5 * (lo + hi);
        (g(mid) < 0.0 ? lo : hi) = mid;
    }
    return 0.5 * (lo + hi);
}

}  // namespace resonant::thermal
