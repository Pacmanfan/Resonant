#include "mechanics/PiezoelectricBeam.h"

#include <cmath>
#include <numbers>

namespace resonant::mechanics {

namespace {

constexpr double kEpsilon0 = 8.8541878128e-12;

// phi'(x) of the tip-normalized static deflection shape.
double slope(double x, double length)
{
    return (6.0 * x * length - 3.0 * x * x) / (2.0 * length * length * length);
}

}  // namespace

Result<HarvesterModel> harvesterModel(const HarvesterSpec& spec)
{
    using R = Result<HarvesterModel>;
    if (!(spec.width > 0.0) || !(spec.length > 0.0))
        return R::failure("the beam needs a positive width and length");
    if (spec.plies.empty())
        return R::failure("the beam needs at least one layer");
    for (const auto& p : spec.plies)
        if (Status s = p.validate(); !s)
            return R::failure(s.message());
    if (spec.piezo.empty())
        return R::failure("the beam has no piezoelectric layer");
    if (spec.piezo.size() > 1 && spec.connection == "single")
        return R::failure("several piezoelectric layers need a series or parallel connection");
    if (spec.connection != "single" && spec.connection != "series" && spec.connection != "parallel")
        return R::failure("the connection is single, series or parallel");
    if (!(spec.tipMass >= 0.0))
        return R::failure("a negative tip mass");
    const BeamSection section = beamSection(spec.plies);
    HarvesterModel m;
    m.neutralAxis = section.neutralAxis;
    const double L = spec.length, b = spec.width;
    const double ei = section.bendingStiffness * b;
    const double perLength = section.massPerArea * b;
    m.stiffness = 3.0 * ei / (L * L * L);
    m.mass = 33.0 / 140.0 * perLength * L + spec.tipMass;
    m.forcingMass = 3.0 / 8.0 * perLength * L + spec.tipMass;
    if (spec.dampingRatio >= 0.0) {
        m.dampingRatio = spec.dampingRatio;
        m.dampingSource = "stated damping ratio";
    } else {
        m.dampingRatio = section.lossFactor / 2.0;
        m.dampingSource = "the layers' loss factors weighted by their bending strain energy (zeta = eta / 2)";
    }
    if (!(m.dampingRatio > 0.0))
        return R::failure("the beam has no damping: state a damping ratio (no layer has a loss factor, and an undamped resonance has no finite response)");
    m.damping = 2.0 * m.dampingRatio * std::sqrt(m.stiffness * m.mass);
    double invC = 0.0, thetaOverC = 0.0, thetaSum = 0.0, cSum = 0.0;
    for (const auto& pl : spec.piezo) {
        if (pl.ply >= spec.plies.size())
            return R::failure("a piezoelectric layer names a ply that does not exist");
        if (!(pl.start >= 0.0 && pl.end <= 1.0 && pl.start < pl.end))
            return R::failure("the electrode span must satisfy 0 <= start < end <= 1");
        if (pl.poling != 1 && pl.poling != -1)
            return R::failure("the poling is +1 or -1");
        const Ply& p = spec.plies[pl.ply];
        const double ep = section.axialModulus[pl.ply];
        const double e31 = pl.d31 * ep;
        const double epsS = pl.eps33T * kEpsilon0 - pl.d31 * pl.d31 * ep;
        if (!(epsS > 0.0))
            return R::failure("the clamped permittivity of the ply '" + p.name + "' is not positive: check d31 and eps33T");
        const double distance = section.centroid[pl.ply] - section.neutralAxis;
        const double theta = e31 * pl.poling * b * distance * (slope(pl.end * L, L) - slope(pl.start * L, L));
        const double c = epsS * b * (pl.end - pl.start) * L / p.thickness;
        m.layerTheta.push_back(theta);
        m.layerCapacitance.push_back(c);
        invC += 1.0 / c;
        thetaOverC += theta / c;
        thetaSum += theta;
        cSum += c;
    }
    if (spec.connection == "series") {
        m.capacitance = 1.0 / invC;
        m.theta = thetaOverC / invC;
    } else {
        m.capacitance = cSum;
        m.theta = thetaSum;
    }
    m.shortCircuitHz = std::sqrt(m.stiffness / m.mass) / (2.0 * std::numbers::pi);
    m.openCircuitHz = std::sqrt((m.stiffness + m.theta * m.theta / m.capacitance) / m.mass) / (2.0 * std::numbers::pi);
    m.couplingSquared = m.theta * m.theta / (m.stiffness * m.capacitance);
    m.eulerBernoulliHz = cantileverFrequencies(ei, perLength, L, 1).front();
    m.assumptions = {"the first bending mode only (Rayleigh, the static tip-load deflection shape)",
                     "Euler-Bernoulli bending of a narrow beam; perfect bonding of every layer",
                     "linear piezoelectricity; equipotential electrodes; a purely resistive load",
                     "viscous modal damping: " + m.dampingSource,
                     "the single-mode estimate is about 1.5 % above the exact frequency of a uniform beam without a tip mass"};
    return m;
}

HarvesterPoint harvesterResponse(const HarvesterModel& m, double frequencyHz, double load, double acceleration)
{
    using C = std::complex<double>;
    const double w = 2.0 * std::numbers::pi * frequencyHz;
    const C i(0.0, 1.0);
    const C admittance = i * w * m.capacitance + (load > 0.0 && std::isfinite(load) ? 1.0 / load : 0.0);
    const C force = -m.forcingMass * acceleration;
    C dynamic = m.stiffness - w * w * m.mass + i * w * m.damping;
    if (std::abs(admittance) > 0.0)
        dynamic += i * w * m.theta * m.theta / admittance;
    HarvesterPoint p;
    p.frequency = frequencyHz;
    p.load = load;
    if (!(load > 0.0)) {
        // A short circuit: no voltage, no electrical power.
        p.displacement = force / (C(m.stiffness - w * w * m.mass) + i * w * m.damping);
        p.voltage = 0.0;
    } else {
        p.displacement = force / dynamic;
        p.voltage = i * w * m.theta * p.displacement / admittance;
    }
    const C velocity = i * w * p.displacement;
    p.electricalPower = load > 0.0 && std::isfinite(load) ? std::norm(p.voltage) / (2.0 * load) : 0.0;
    p.dissipatedPower = 0.5 * m.damping * std::norm(velocity);
    p.inputPower = 0.5 * std::real(force * std::conj(velocity));
    p.efficiency = p.inputPower > 0.0 ? p.electricalPower / p.inputPower : 0.0;
    return p;
}

double optimalLoad(const HarvesterModel& m, double frequencyHz)
{
    // A coarse scan in log R, then a golden-section refinement around the best point.
    double bestLog = 0.0, bestPower = -1.0;
    for (int k = 0; k <= 240; ++k) {
        const double lg = k * 0.05;  // 1 ohm to 1e12 ohm
        const double p = harvesterResponse(m, frequencyHz, std::pow(10.0, lg), 1.0).electricalPower;
        if (p > bestPower) {
            bestPower = p;
            bestLog = lg;
        }
    }
    double lo = bestLog - 0.05, hi = bestLog + 0.05;
    const double g = (std::sqrt(5.0) - 1.0) / 2.0;
    for (int it = 0; it < 80; ++it) {
        const double a = hi - g * (hi - lo), b = lo + g * (hi - lo);
        if (harvesterResponse(m, frequencyHz, std::pow(10.0, a), 1.0).electricalPower > harvesterResponse(m, frequencyHz, std::pow(10.0, b), 1.0).electricalPower)
            hi = b;
        else
            lo = a;
    }
    return std::pow(10.0, 0.5 * (lo + hi));
}

std::complex<double> actuatorDisplacement(const HarvesterModel& m, double frequencyHz, double voltage)
{
    // The voltage is imposed: -theta V drives the mechanical equation.
    const double w = 2.0 * std::numbers::pi * frequencyHz;
    const std::complex<double> dynamic(m.stiffness - w * w * m.mass, w * m.damping);
    return -m.theta * voltage / dynamic;
}

}  // namespace resonant::mechanics
