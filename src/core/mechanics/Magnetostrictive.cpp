#include "mechanics/Magnetostrictive.h"

#include <cmath>
#include <cstdio>

namespace resonant::mechanics {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kMu0 = 1.25663706212e-6;  // H/m

std::string num(double v)
{
    char b[64];
    std::snprintf(b, sizeof b, "%.6g", v);
    return b;
}

}  // namespace

double PiezomagneticConstants::couplingSquared() const
{
    return d33 * d33 / (complianceH * permeabilityT);
}

double PiezomagneticConstants::permeabilityS() const
{
    return permeabilityT * (1 - couplingSquared());
}

double PiezomagneticConstants::complianceB() const
{
    return complianceH * (1 - couplingSquared());
}

Status PiezomagneticConstants::validate() const
{
    if (!(complianceH > 0.0 && permeabilityT > 0.0 && density > 0.0))
        return Status::failure("the magnetostrictive material needs a positive compliance, permeability and density");
    const double k2 = couplingSquared();
    if (!(k2 >= 0.0 && k2 < 1.0))
        return Status::failure("the coupling k33^2 = d33^2 / (s^H mu^T) = " + num(k2) + " is not below 1: the constants are not consistent (refused)");
    if (!(biasField > 0.0 && linearFieldLimit > 0.0))
        return Status::failure("a linear magnetostrictive model needs its bias field and the dynamic-field limit of its validity");
    return Status::success();
}

PiezomagneticConstants PiezomagneticConstants::terfenolD()
{
    PiezomagneticConstants c;
    c.name = "Terfenol-D (illustrative)";
    c.provenance = "illustrative literature-order values for a prestressed, biased rod (E^H ~ 25 GPa, d33 ~ 15 nm/A, mu_r^T ~ 9); not a datasheet";
    c.complianceH = 1.0 / 25e9;
    c.d33 = 1.5e-8;
    c.permeabilityT = 9.0 * kMu0;
    c.density = 9250.0;
    c.biasField = 40e3;
    c.linearFieldLimit = 10e3;
    return c;
}

Status MagnetostrictiveRod::validate() const
{
    if (Status s = material.validate(); !s)
        return s;
    if (!(length > 0.0 && area > 0.0) || turns < 1 || coilResistance < 0.0 || tipMass < 0.0 || !(dampingRatio >= 0.0))
        return Status::failure("the rod needs a positive length, area and number of turns, and non-negative coil resistance, tip mass and damping");
    return Status::success();
}

Result<MagnetostrictiveModel> magnetostrictiveModel(const MagnetostrictiveRod& r)
{
    using R = Result<MagnetostrictiveModel>;
    if (Status s = r.validate(); !s)
        return R::failure(s.message());
    MagnetostrictiveModel m;
    const auto& c = r.material;
    const double rodMass = c.density * r.area * r.length;
    m.mass = r.tipMass + rodMass / 3.0;
    m.forcingMass = r.tipMass + rodMass / 2.0;
    m.stiffnessH = r.area / (c.complianceH * r.length);
    m.theta = r.area * c.d33 * r.turns / (c.complianceH * r.length);
    m.inductance = c.permeabilityS() * r.turns * r.turns * r.area / r.length;
    m.couplingSquared = c.couplingSquared();
    m.openCoilHz = std::sqrt(m.stiffnessH / m.mass) / (2 * kPi);
    m.shortedIdealHz = std::sqrt((m.stiffnessH + m.theta * m.theta / m.inductance) / m.mass) / (2 * kPi);
    m.damping = 2 * r.dampingRatio * std::sqrt(m.stiffnessH * m.mass);
    m.assumptions = {"linear piezomagnetic constants about the bias field " + num(c.biasField) + " A/m (" + c.provenance + ")",
                     "one degree of freedom: the rod as a spring (k^H = A / s^H L) with a third of its mass and the tip mass (Rayleigh)",
                     "a long coil over the rod (H = N i / L), no leakage, no eddy currents", "viscous damping ratio " + num(r.dampingRatio),
                     "not modelled: hysteresis, saturation, the Delta-E effect, temperature"};
    return m;
}

ActuatorResponse magnetostrictiveActuator(const MagnetostrictiveRod& r, double current)
{
    ActuatorResponse a;
    a.field = r.turns * current / r.length;
    a.freeElongation = r.material.d33 * a.field * r.length;  // S = d33 H, free
    a.blockedForce = r.material.d33 * a.field * r.area / r.material.complianceH;  // T = -d33 H / s^H to hold S = 0
    a.withinLinearRange = std::abs(a.field) <= r.material.linearFieldLimit;
    return a;
}

MagnetostrictiveHarvesterPoint magnetostrictiveHarvester(const MagnetostrictiveRod& r, const MagnetostrictiveModel& m, double frequencyHz, double load, double acceleration)
{
    using C = std::complex<double>;
    MagnetostrictiveHarvesterPoint p;
    p.frequency = frequencyHz;
    p.load = load;
    const double w = 2 * kPi * frequencyHz;
    const C j(0.0, 1.0);
    const C force = -m.forcingMass * acceleration;  // the base excitation in the base frame
    const C circuit = r.coilResistance + load + j * w * m.inductance;
    // i = -j w Theta x / (R_c + R_L + j w L_c); (k - w^2 m + j w c) x - Theta i = F.
    const C mechanical = m.stiffnessH - w * w * m.mass + j * w * m.damping + j * w * m.theta * m.theta / circuit;
    p.elongation = force / mechanical;
    p.current = -j * w * m.theta * p.elongation / circuit;
    const C velocity = j * w * p.elongation;
    p.inputPower = 0.5 * std::real(force * std::conj(velocity));
    p.loadPower = 0.5 * load * std::norm(p.current);
    p.coilLoss = 0.5 * r.coilResistance * std::norm(p.current);
    p.dampingLoss = 0.5 * m.damping * std::norm(velocity);
    p.dynamicField = std::abs(p.current) * r.turns / r.length;
    p.withinLinearRange = p.dynamicField <= r.material.linearFieldLimit;
    return p;
}

}  // namespace resonant::mechanics
