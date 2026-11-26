#include "manipulators/Analytics.h"

#include "foundation/Units.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace resonant::manipulators {

double parabolaDepth(double focalLength, double apertureDiameter)
{
    return apertureDiameter * apertureDiameter / (16.0 * focalLength);
}

double ellipseFocalDistance(double semiMajor, double semiMinor)
{
    return std::sqrt(std::max(0.0, semiMajor * semiMajor - semiMinor * semiMinor));
}

double sphericalMirrorAxisCrossing(double curvatureRadius, double rayHeight)
{
    // The ray meets the mirror where the radius makes the angle theta with
    // the axis, sin(theta) = h / R, and after reflection crosses the axis at
    // a distance R / (2 cos(theta)) from the centre of curvature.
    const double sinTheta = rayHeight / curvatureRadius;
    const double cosTheta = std::sqrt(std::max(0.0, 1.0 - sinTheta * sinTheta));
    return curvatureRadius - curvatureRadius / (2.0 * cosTheta);
}

double mirrorImageDistance(double focalLength, double sourceDistance)
{
    const double inverse = 1.0 / focalLength - 1.0 / sourceDistance;
    return std::abs(inverse) < 1e-300 ? std::numeric_limits<double>::infinity() : 1.0 / inverse;
}

double pressureReflectionCoefficient(double z1, double z2)
{
    return (z2 - z1) / (z2 + z1);
}

double intensityTransmissionCoefficient(double z1, double z2)
{
    return 4.0 * z1 * z2 / ((z1 + z2) * (z1 + z2));
}

LensEstimate estimateLens(double radius1, double radius2, double centerThickness, double mediumSoundSpeed,
                          double mediumDensity, double lensSoundSpeed, double lensDensity)
{
    LensEstimate estimate;
    const double n = mediumSoundSpeed / lensSoundSpeed;
    estimate.refractiveIndex = n;
    const double curvature1 = radius1 == 0.0 ? 0.0 : 1.0 / radius1;
    const double curvature2 = radius2 == 0.0 ? 0.0 : 1.0 / radius2;
    const double power =
        (n - 1.0) * (curvature1 - curvature2 + (n - 1.0) * centerThickness * curvature1 * curvature2 / n);
    estimate.focalLength = std::abs(power) < 1e-300 ? std::numeric_limits<double>::infinity() : 1.0 / power;
    const double z1 = mediumDensity * mediumSoundSpeed;
    const double z2 = lensDensity * lensSoundSpeed;
    estimate.surfaceTransmission1 = intensityTransmissionCoefficient(z1, z2);
    estimate.surfaceTransmission2 = intensityTransmissionCoefficient(z2, z1);
    estimate.assumptions = {
        "Paraxial thick-lens (lensmaker's) equation with the acoustic refractive index n = c_medium / c_lens.",
        "The lens is treated as a fluid: only longitudinal waves, no shear waves, no mode conversion.",
        "No absorption in the lens; the transmission values are for normal incidence on one flat interface each "
        "and ignore multiple reflections inside the lens.",
        "A first-order design estimate, not a field simulation. Aberrations, diffraction at the aperture and "
        "wavelength-scale effects are not included.",
    };
    if (n < 1.0)
        estimate.assumptions.push_back("The lens material is faster than the medium (n < 1): unlike an optical "
                                       "glass lens, a convex lens of this material diverges and a concave one "
                                       "converges.");
    return estimate;
}

namespace {

// Zeros j'_mn of the derivative of the Bessel function J_m, the first few
// in ascending order: {m, n (number of radial nodal circles + ...), value}.
// (m, n) follows the usual acoustics labelling with n counted from 0.
struct BesselZero {
    int m;
    int n;
    double value;
};
constexpr BesselZero kBesselDerivativeZeros[] = {
    {0, 0, 0.0},      {1, 0, 1.8411838}, {2, 0, 3.0542369}, {0, 1, 3.8317060}, {3, 0, 4.2011889},
    {4, 0, 5.3175531}, {1, 1, 5.3314428}, {5, 0, 6.4156164}, {2, 1, 6.7061331}, {0, 2, 7.0155867},
    {6, 0, 7.5012661}, {3, 1, 8.0152366}, {1, 2, 8.5363164}, {7, 0, 8.5778364},
};

// Zeros of the derivative of the spherical Bessel functions j_n, ascending:
// {order n, index, value}. The lowest sphere mode is n = 1.
constexpr BesselZero kSphericalDerivativeZeros[] = {
    {1, 1, 2.0815759}, {2, 1, 3.3420937}, {0, 1, 4.4934095}, {3, 1, 4.5140996},
    {4, 1, 5.6467036}, {1, 2, 5.9403700}, {5, 1, 6.7564563}, {2, 2, 7.2899322},
};

void sortAndTrim(std::vector<Mode>& modes, int count)
{
    std::sort(modes.begin(), modes.end(), [](const Mode& a, const Mode& b) { return a.frequencyHz < b.frequencyHz; });
    if (static_cast<int>(modes.size()) > count)
        modes.resize(static_cast<size_t>(std::max(0, count)));
}

}  // namespace

std::vector<Mode> rectangularDuctCutoffs(double width, double height, double soundSpeed, int count)
{
    std::vector<Mode> modes;
    const int limit = count + 2;
    for (int m = 0; m <= limit; ++m)
        for (int n = 0; n <= limit; ++n)
            if (m + n > 0)
                modes.push_back({m, n, 0, 0.5 * soundSpeed * std::hypot(m / width, n / height)});
    sortAndTrim(modes, count);
    return modes;
}

std::vector<Mode> circularDuctCutoffs(double radius, double soundSpeed, int count)
{
    std::vector<Mode> modes;
    for (const BesselZero& zero : kBesselDerivativeZeros)
        if (zero.value > 0.0)
            modes.push_back({zero.m, zero.n, 0, soundSpeed * zero.value / (2.0 * units::pi * radius)});
    sortAndTrim(modes, count);
    return modes;
}

std::vector<Mode> rectangularCavityModes(double lx, double ly, double lz, double soundSpeed, int count)
{
    std::vector<Mode> modes;
    const int limit = count + 2;
    for (int x = 0; x <= limit; ++x)
        for (int y = 0; y <= limit; ++y)
            for (int z = 0; z <= limit; ++z)
                if (x + y + z > 0)
                    modes.push_back({x, y, z,
                                     0.5 * soundSpeed * std::sqrt((x / lx) * (x / lx) + (y / ly) * (y / ly) + (z / lz) * (z / lz))});
    sortAndTrim(modes, count);
    return modes;
}

std::vector<Mode> cylindricalCavityModes(double radius, double length, double soundSpeed, int count)
{
    std::vector<Mode> modes;
    for (const BesselZero& zero : kBesselDerivativeZeros) {
        for (int q = 0; q <= count + 2; ++q) {
            if (zero.value == 0.0 && q == 0)
                continue;
            const double k = std::hypot(zero.value / radius, q * units::pi / length);
            modes.push_back({zero.m, zero.n, q, soundSpeed * k / (2.0 * units::pi)});
        }
    }
    sortAndTrim(modes, count);
    return modes;
}

std::vector<Mode> sphericalCavityModes(double radius, double soundSpeed, int count)
{
    std::vector<Mode> modes;
    for (const BesselZero& zero : kSphericalDerivativeZeros)
        modes.push_back({zero.m, zero.n, 0, soundSpeed * zero.value / (2.0 * units::pi * radius)});
    sortAndTrim(modes, count);
    return modes;
}

double helmholtzFrequency(double neckRadius, double neckLength, double cavityVolume, double soundSpeed)
{
    const double area = units::pi * neckRadius * neckRadius;
    const double effectiveLength = neckLength + 1.7 * neckRadius;
    return soundSpeed / (2.0 * units::pi) * std::sqrt(area / (cavityVolume * effectiveLength));
}

double quarterWaveFrequency(double length, double soundSpeed, int n)
{
    return (2.0 * n - 1.0) * soundSpeed / (4.0 * length);
}

}  // namespace resonant::manipulators
