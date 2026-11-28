#include "electromagnetics/RadiationForce.h"

#include <cmath>

namespace resonant::em {

namespace {
constexpr double kEps0 = 8.8541878128e-12;
constexpr double kMu0 = 1.25663706212e-6;
constexpr double kC = 299792458.0;
constexpr double kPi = 3.14159265358979323846;

bool vacuum(const MediumLayer& m)
{
    return m.permittivityReal == 1.0 && m.lossTangent == 0.0 && m.conductivity == 0.0 && m.permeability == 1.0;
}
}  // namespace

Result<RadiationForce> radiationForce(const LayeredSolution& s, int points)
{
    using R = Result<RadiationForce>;
    const LayeredProblem& p = s.problem();
    if (!vacuum(p.incidence) || !vacuum(p.exit))
        return R::failure("the radiation force is computed between vacuum half-spaces only: inside matter it depends on the momentum "
                          "formulation (Abraham, Minkowski), which no solver here selects");
    for (const MediumLayer& l : p.layers)
        if (l.permeability != 1.0)
            return R::failure("the layer '" + l.name + "' is magnetic: the volume method leaves out the magnetization current");
    const double d = p.totalThickness();
    if (!(d > 0.0))
        return R::failure("the stack has no thickness");
    const double omega = 2.0 * kPi * p.frequencyHz;
    RadiationForce f;
    f.intensity = p.incidentIntensity;
    f.reflectance = s.reflectance();
    f.transmittance = s.transmittance();
    f.absorptance = s.absorptance();
    const auto stress = [&](double z) {
        const Complex E = s.electricField(z), H = s.magneticField(z);
        return 0.25 * (kEps0 * std::norm(E) + kMu0 * std::norm(H));
    };
    const double eps = 1e-9 * d;
    f.fluxIncidentSide = stress(-eps);
    f.fluxExitSide = stress(d + eps);
    f.pressureSurfaces = f.fluxIncidentSide - f.fluxExitSide;
    f.pressureFluxes = (1.0 + f.reflectance - f.transmittance) * p.incidentIntensity / kC;
    f.absorbedGlobal = s.poyntingFlux(0.0) - s.poyntingFlux(d);
    // Simpson's rule per layer (the integrands are smooth inside a layer).
    double start = 0.0;
    for (const MediumLayer& l : p.layers) {
        int n = static_cast<int>(std::lround(std::max(1.0, static_cast<double>(points)) * l.thickness / d));
        n = std::max(8, n + (n % 2));
        const double h = l.thickness / n;
        const Complex chi = l.relativePermittivity(omega) - 1.0;
        double force = 0.0, absorbed = 0.0;
        for (int k = 0; k <= n; ++k) {
            const double z = start + k * h;
            const Complex E = s.electricField(z), H = s.magneticField(z);
            const Complex J = Complex(0.0, omega * kEps0) * chi * E;
            const double fz = 0.5 * std::real(J * std::conj(kMu0 * H));
            const double q = 0.5 * omega * kEps0 * l.lossFactor(omega) * std::norm(E);
            const double w = (k == 0 || k == n) ? 1.0 : (k % 2 ? 4.0 : 2.0);
            force += w * fz;
            absorbed += w * q;
            f.z.push_back(z);
            f.forceDensity.push_back(fz);
            f.absorbedDensity.push_back(q);
        }
        f.pressureVolume += force * h / 3.0;
        f.absorbedLocal += absorbed * h / 3.0;
        f.points += n + 1;
        start += l.thickness;
    }
    return f;
}

}  // namespace resonant::em
