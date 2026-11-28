#include "mechanics/LocallyResonantLattice.h"

#include <cmath>

namespace resonant::mechanics {

namespace {
constexpr double kPi = 3.14159265358979323846;
}

Status MassInMassCell::validate() const
{
    if (!(hostMass > 0.0) || !(hostStiffness > 0.0) || !(resonatorMass > 0.0) || !(resonatorStiffness > 0.0) || !(spacing > 0.0))
        return Status::failure("the cell needs positive masses, stiffnesses and spacing");
    if (resonatorLossFactor < 0.0 || resonatorLossFactor >= 1.0)
        return Status::failure("the resonator's loss factor must lie in [0, 1)");
    return Status::success();
}

double localResonanceHz(const MassInMassCell& c)
{
    return std::sqrt(c.resonatorStiffness / c.resonatorMass) / (2 * kPi);
}

std::complex<double> effectiveMass(const MassInMassCell& c, double f)
{
    const double w = 2 * kPi * f;
    const std::complex<double> k2(c.resonatorStiffness, c.resonatorStiffness * c.resonatorLossFactor);  // k (1 + j eta)
    return c.hostMass + c.resonatorMass * k2 / (k2 - c.resonatorMass * w * w);
}

std::array<double, 2> massInMassDispersion(const MassInMassCell& c, double reduced)
{
    const double s2 = std::pow(std::sin(0.5 * reduced * kPi), 2);
    const double w2 = c.resonatorStiffness / c.resonatorMass;
    // m1 W^2 - W [(m1 + m2) w2 + 4 k1 s^2] + 4 k1 s^2 w2 = 0, W = w^2.
    const double A = c.hostMass, B = -((c.hostMass + c.resonatorMass) * w2 + 4 * c.hostStiffness * s2), C = 4 * c.hostStiffness * s2 * w2;
    const double disc = std::sqrt(std::max(0.0, B * B - 4 * A * C));
    // -B > 0: the lower root in the form without cancellation (q -> 0).
    const double hi = (-B + disc) / (2 * A), lo = hi > 0.0 ? C / (A * hi) : 0.0;
    return {std::sqrt(std::max(0.0, lo)) / (2 * kPi), std::sqrt(std::max(0.0, hi)) / (2 * kPi)};
}

std::complex<double> massInMassBlochWavenumber(const MassInMassCell& c, double f)
{
    const double w = 2 * kPi * f;
    const std::complex<double> cosine = 1.0 - w * w * effectiveMass(c, f) / (2.0 * c.hostStiffness);
    // The principal phase per cell in [0, pi]; the attenuation per cell as |Im|.
    const std::complex<double> qa = std::acos(cosine);
    return std::complex<double>(qa.real(), std::abs(qa.imag())) / c.spacing;
}

LatticeGap massInMassGap(const MassInMassCell& c)
{
    LatticeGap g;
    g.lowerHz = massInMassDispersion(c, 1.0)[0];
    g.upperHz = massInMassDispersion(c, 0.0)[1];
    g.negativeMassLowerHz = localResonanceHz(c);
    g.negativeMassUpperHz = localResonanceHz(c) * std::sqrt(1.0 + c.resonatorMass / c.hostMass);
    return g;
}

oscillators::BlochCell massInMassBlochCell(const MassInMassCell& c)
{
    oscillators::BlochCell cell;
    cell.mass = Eigen::Matrix2d::Zero();
    cell.mass(0, 0) = c.hostMass;
    cell.mass(1, 1) = c.resonatorMass;
    cell.stiffness = Eigen::Matrix2d::Zero();
    cell.stiffness << c.resonatorStiffness, -c.resonatorStiffness, -c.resonatorStiffness, c.resonatorStiffness;
    cell.links = {{0, 0, c.hostStiffness}};
    cell.period = c.spacing;
    return cell;
}

oscillators::Network massInMassChain(const MassInMassCell& c, int cells)
{
    oscillators::Network n;
    for (int i = 0; i < cells; ++i) {
        n.names.push_back("host " + std::to_string(i));
        n.names.push_back("resonator " + std::to_string(i));
        n.masses.push_back(c.hostMass);
        n.masses.push_back(c.resonatorMass);
        n.springs.push_back({"resonator spring " + std::to_string(i), 2 * i, 2 * i + 1, c.resonatorStiffness});
        if (i + 1 < cells)
            n.springs.push_back({"host spring " + std::to_string(i), 2 * i, 2 * i + 2, c.hostStiffness});
        if (c.resonatorLossFactor > 0.0)
            n.dampers.push_back({"resonator loss", 2 * i, 2 * i + 1,
                                 c.resonatorLossFactor * c.resonatorStiffness / (2 * kPi * localResonanceHz(c))});
    }
    n.unitsNote = "displacements (m), masses (kg), springs (N/m)";
    return n;
}

}  // namespace resonant::mechanics
