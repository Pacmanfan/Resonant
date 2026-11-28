#include "manipulators/Helmholtz.h"

#include "geometry/Geometry.h"
#include "manipulators/Horn.h"

#include <algorithm>
#include <cmath>

namespace resonant::manipulators {

namespace {
constexpr double kPi = 3.14159265358979323846;
const Complex j(0.0, 1.0);

double outerCorrectionOf(const HelmholtzSpec& s)
{
    return (s.flanged ? 8.0 / (3.0 * kPi) : 0.6133) * s.neckRadius;
}

double innerCorrectionOf(const HelmholtzSpec& s)
{
    return s.innerCorrection ? 8.0 / (3.0 * kPi) * s.neckRadius * std::max(0.0, 1.0 - 1.25 * s.neckRadius / s.cavityRadius) : 0.0;
}

double effectiveLength(const HelmholtzSpec& s)
{
    return s.neckLength + outerCorrectionOf(s) + innerCorrectionOf(s);
}

double radiationResistance(const HelmholtzSpec& s, double f)
{
    if (!s.radiationLoss)
        return 0.0;
    const double k = 2 * kPi * f / s.soundSpeed;
    return s.density * s.soundSpeed * k * k / ((s.flanged ? 2.0 : 4.0) * kPi);
}

double viscousResistance(const HelmholtzSpec& s, double f)
{
    if (!s.viscousLoss)
        return 0.0;
    const double w = 2 * kPi * f, a = s.neckRadius;
    return 2.0 * effectiveLength(s) / (kPi * a * a * a) * std::sqrt(w * s.density * s.viscosity / 2.0);
}

// Transfer matrix of a uniform duct: [p, U] at the input from [p, U] at the output.
void duct(double length, double area, double k, double rho, double c, Complex m[2][2])
{
    const double zc = rho * c / area;
    m[0][0] = std::cos(k * length);
    m[0][1] = j * zc * std::sin(k * length);
    m[1][0] = j * std::sin(k * length) / zc;
    m[1][1] = std::cos(k * length);
}
}  // namespace

Status HelmholtzSpec::validate() const
{
    if (!(cavityRadius > 0.0) || !(cavityLength > 0.0) || !(neckRadius > 0.0) || !(neckLength > 0.0))
        return Status::failure("the cavity and the neck need positive dimensions");
    if (!(neckRadius < cavityRadius))
        return Status::failure("the neck must be narrower than the cavity");
    if (!(density > 0.0) || !(soundSpeed > 0.0) || viscosity < 0.0)
        return Status::failure("the medium needs a positive density and sound speed and a non-negative viscosity");
    return Status::success();
}

double HelmholtzSpec::volume() const
{
    return kPi * cavityRadius * cavityRadius * cavityLength;
}

double HelmholtzSpec::neckArea() const
{
    return kPi * neckRadius * neckRadius;
}

HelmholtzSpec helmholtzSpecOf(const Geometry& g)
{
    HelmholtzSpec s;
    s.cavityRadius = g.parameter("cavityRadius");
    s.cavityLength = g.parameter("cavityLength");
    s.neckRadius = g.parameter("neckRadius");
    s.neckLength = g.parameter("neckLength");
    return s;
}

Complex helmholtzImpedance(const HelmholtzSpec& s, double f)
{
    const double w = 2 * kPi * f;
    const double M = s.density * effectiveLength(s) / s.neckArea(), C = s.volume() / (s.density * s.soundSpeed * s.soundSpeed);
    return radiationResistance(s, f) + viscousResistance(s, f) + j * (w * M - 1.0 / (w * C));
}

double helmholtzPressureGain(const HelmholtzSpec& s, double f)
{
    const double w = 2 * kPi * f, C = s.volume() / (s.density * s.soundSpeed * s.soundSpeed);
    const double drive = s.flanged ? 2.0 : 1.0;  // blocked pressure at a baffled mouth: twice the incident
    return drive / (std::abs(helmholtzImpedance(s, f)) * w * C);
}

Result<HelmholtzResult> analyzeHelmholtz(const HelmholtzSpec& s)
{
    using R = Result<HelmholtzResult>;
    if (Status v = s.validate(); !v)
        return R::failure(v.message());
    HelmholtzResult r;
    r.volume = s.volume();
    r.neckArea = s.neckArea();
    r.outerCorrection = outerCorrectionOf(s);
    r.innerCorrection = innerCorrectionOf(s);
    r.effectiveLength = effectiveLength(s);
    r.inertance = s.density * r.effectiveLength / r.neckArea;
    r.compliance = r.volume / (s.density * s.soundSpeed * s.soundSpeed);
    r.frequencyHz = 1.0 / (2 * kPi * std::sqrt(r.inertance * r.compliance));
    const double w0 = 2 * kPi * r.frequencyHz;
    r.radiationResistance = radiationResistance(s, r.frequencyHz);
    r.viscousResistance = viscousResistance(s, r.frequencyHz);
    r.qRadiation = r.radiationResistance > 0.0 ? w0 * r.inertance / r.radiationResistance : std::numeric_limits<double>::infinity();
    r.qViscous = r.viscousResistance > 0.0 ? w0 * r.inertance / r.viscousResistance : std::numeric_limits<double>::infinity();
    const double total = r.radiationResistance + r.viscousResistance;
    r.quality = total > 0.0 ? w0 * r.inertance / total : std::numeric_limits<double>::infinity();
    r.bandwidthHz = std::isfinite(r.quality) ? r.frequencyHz / r.quality : 0.0;
    const double k = w0 / s.soundSpeed;
    r.neckKa = k * s.neckRadius;
    r.neckKL = k * r.effectiveLength;
    r.cavityKSize = k * std::max(2.0 * s.cavityRadius, s.cavityLength);
    r.pressureGainAtResonance = helmholtzPressureGain(s, r.frequencyHz);
    // Stored (time-averaged) energy at resonance for a unit incident pressure: 1/4 C |p_c|^2 + 1/4 M |U|^2 = 1/2 C |p_c|^2.
    r.storedEnergyPerPa2 = 0.5 * r.compliance * r.pressureGainAtResonance * r.pressureGainAtResonance;
    if (r.cavityKSize > 0.5)
        r.warnings.push_back("k times the cavity's size is " + std::to_string(r.cavityKSize).substr(0, 5) +
                             ": the cavity is not small against the wavelength and the lumped model is approximate");
    if (r.neckKL > 0.5)
        r.warnings.push_back("k L_eff = " + std::to_string(r.neckKL).substr(0, 5) + ": the neck is not short against the wavelength");
    if (s.neckRadius / s.cavityRadius > 0.4)
        r.warnings.push_back("the neck is wide against the cavity: the inner end correction (Ingard) is outside its range");
    return r;
}

Complex steppedDuctImpedance(const HelmholtzSpec& s, double f)
{
    const double k = 2 * kPi * f / s.soundSpeed;
    Complex neck[2][2], cavity[2][2];
    duct(s.neckLength, s.neckArea(), k, s.density, s.soundSpeed, neck);
    duct(s.cavityLength, kPi * s.cavityRadius * s.cavityRadius, k, s.density, s.soundSpeed, cavity);
    // Rigid back: U = 0, p = 1 at the back of the cavity; propagate to the mouth.
    const Complex pc = cavity[0][0], uc = cavity[1][0];
    const Complex p = neck[0][0] * pc + neck[0][1] * uc, u = neck[1][0] * pc + neck[1][1] * uc;
    // x runs from the mouth into the resonator, so u is the volume velocity entering it.
    return p / u;
}

Result<DuctResonance> steppedDuctResonance(const HelmholtzSpec& s)
{
    using R = Result<DuctResonance>;
    auto lumped = analyzeHelmholtz(s);
    if (!lumped)
        return R::failure(lumped.message());
    const auto load = [&](double f) { return bafflePistonImpedance(s.neckRadius, f, s.density, s.soundSpeed); };
    const auto reactance = [&](double f) { return (steppedDuctImpedance(s, f) + load(f)).imag(); };
    double lo = 0.5 * lumped.value().frequencyHz, hi = 1.5 * lumped.value().frequencyHz;
    if (reactance(lo) * reactance(hi) > 0.0)
        return R::failure("no series resonance of the stepped duct near the lumped estimate");
    for (int i = 0; i < 200; ++i) {
        const double mid = 0.5 * (lo + hi);
        (reactance(mid) * reactance(lo) > 0.0 ? lo : hi) = mid;
    }
    DuctResonance d;
    d.frequencyHz = 0.5 * (lo + hi);
    // The volume velocity for a unit driving pressure: |U|^2 = 1 / |Z_in + Z_rad|^2; its half-power width.
    const auto u2 = [&](double f) { return 1.0 / std::norm(steppedDuctImpedance(s, f) + load(f)); };
    const double peak = u2(d.frequencyHz);
    const auto half = [&](double sign) {
        double a = d.frequencyHz, b = d.frequencyHz * (1.0 + sign * 0.5);
        for (int i = 0; i < 200; ++i) {
            const double mid = 0.5 * (a + b);
            (u2(mid) > 0.5 * peak ? a : b) = mid;
        }
        return 0.5 * (a + b);
    };
    d.quality = d.frequencyHz / (half(1.0) - half(-1.0));
    return d;
}

Result<oscillators::Network> coupledHelmholtz(const HelmholtzSpec& open, const HelmholtzSpec& closed, double ac, double lc)
{
    using R = Result<oscillators::Network>;
    if (Status v = open.validate(); !v)
        return R::failure("resonator 1: " + v.message());
    if (Status v = closed.validate(); !v)
        return R::failure("resonator 2: " + v.message());
    if (!(ac > 0.0) || !(lc > 0.0))
        return R::failure("the connector needs a positive radius and length");
    if (!(ac < std::min(open.cavityRadius, closed.cavityRadius)))
        return R::failure("the connector must be narrower than both cavities");
    auto one = analyzeHelmholtz(open);
    if (!one)
        return R::failure(one.message());
    const double rho = open.density, c = open.soundSpeed;
    const double C1 = open.volume() / (rho * c * c), C2 = closed.volume() / (closed.density * closed.soundSpeed * closed.soundSpeed);
    const double corr = 8.0 / (3.0 * kPi) * ac;
    const double lcEff = lc + corr * std::max(0.0, 1.0 - 1.25 * ac / open.cavityRadius) + corr * std::max(0.0, 1.0 - 1.25 * ac / closed.cavityRadius);
    const double Sc = kPi * ac * ac;
    oscillators::Network n;
    n.names = {"neck of resonator 1", "connector"};
    n.massMatrix = Eigen::Matrix2d::Zero();
    n.massMatrix(0, 0) = one.value().inertance;
    n.massMatrix(1, 1) = rho * lcEff / Sc;
    n.stiffnessMatrix = Eigen::Matrix2d::Zero();
    n.stiffnessMatrix << 1.0 / C1, 1.0 / C1, 1.0 / C1, 1.0 / C1 + 1.0 / C2;
    const double f = one.value().frequencyHz, w = 2 * kPi * f;
    const auto diag = [](double v0, double v1) {
        Eigen::MatrixXd m = Eigen::MatrixXd::Zero(2, 2);
        m(0, 0) = v0;
        m(1, 1) = v1;
        return m;
    };
    if (open.radiationLoss)
        n.dampingByMechanism["radiation"] = diag(one.value().radiationResistance, 0.0);
    if (open.viscousLoss) {
        n.dampingByMechanism["viscous (neck 1)"] = diag(one.value().viscousResistance, 0.0);
        n.dampingByMechanism["viscous (connector)"] = diag(0.0, 2.0 * lcEff / (kPi * ac * ac * ac) * std::sqrt(w * rho * open.viscosity / 2.0));
    }
    n.unitsNote = "volume displacement (m^3), inertance (kg/m^4), compliance (m^3/Pa), resistance (Pa s/m^3)";
    return n;
}

}  // namespace resonant::manipulators
