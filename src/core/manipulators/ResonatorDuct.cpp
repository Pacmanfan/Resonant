#include "manipulators/ResonatorDuct.h"

#include <algorithm>
#include <cmath>

namespace resonant::manipulators {

namespace {
constexpr double kPi = 3.14159265358979323846;
const Complex j(0.0, 1.0);

using M2 = std::array<Complex, 4>;
M2 mul(const M2& a, const M2& b)
{
    return {a[0] * b[0] + a[1] * b[2], a[0] * b[1] + a[1] * b[3], a[2] * b[0] + a[3] * b[2], a[2] * b[1] + a[3] * b[3]};
}

M2 segment(double length, double area, double k, double rho, double c)
{
    const double zc = rho * c / area;
    return {std::cos(k * length), j * zc * std::sin(k * length), j * std::sin(k * length) / zc, std::cos(k * length)};
}

// The resonator's impedance seen from the duct: the lumped model without free-field radiation
// (the duct is its outside), with its viscous loss if enabled.
Complex branchImpedance(const ResonatorDuctSpec& s, double f)
{
    HelmholtzSpec h = s.resonator;
    h.radiationLoss = false;
    return helmholtzImpedance(h, f);
}
}  // namespace

Status ResonatorDuctSpec::validate() const
{
    if (!(ductRadius > 0.0) || !(spacing > 0.0))
        return Status::failure("the duct needs a positive radius and spacing");
    if (Status v = resonator.validate(); !v)
        return Status::failure("the resonator: " + v.message());
    if (!(resonator.neckRadius < ductRadius))
        return Status::failure("the resonator's neck must be narrower than the duct");
    return Status::success();
}

double ResonatorDuctSpec::ductArea() const
{
    return kPi * ductRadius * ductRadius;
}

std::array<Complex, 4> resonatorDuctCell(const ResonatorDuctSpec& s, double f)
{
    const double k = 2 * kPi * f / s.resonator.soundSpeed;
    const M2 half = segment(0.5 * s.spacing, s.ductArea(), k, s.resonator.density, s.resonator.soundSpeed);
    const M2 shunt{Complex(1.0), Complex(0.0), 1.0 / branchImpedance(s, f), Complex(1.0)};
    return mul(mul(half, shunt), half);
}

Complex resonatorDuctBloch(const ResonatorDuctSpec& s, double f)
{
    const M2 t = resonatorDuctCell(s, f);
    // The principal phase per cell in [0, pi]; the attenuation per cell as |Im|.
    const Complex qa = std::acos(0.5 * (t[0] + t[3]));
    return Complex(qa.real(), std::abs(qa.imag())) / s.spacing;
}

Complex resonatorDuctBulkModulus(const ResonatorDuctSpec& s, double f)
{
    const double w = 2 * kPi * f, B0 = s.resonator.density * s.resonator.soundSpeed * s.resonator.soundSpeed;
    return 1.0 / (1.0 / B0 + (1.0 / branchImpedance(s, f)) / (j * w * s.ductArea() * s.spacing));
}

double resonatorDuctTransmission(const ResonatorDuctSpec& s, double f, int cells)
{
    M2 total{Complex(1.0), Complex(0.0), Complex(0.0), Complex(1.0)};
    const M2 cell = resonatorDuctCell(s, f);
    for (int i = 0; i < cells; ++i)
        total = mul(total, cell);
    const double zc = s.resonator.density * s.resonator.soundSpeed / s.ductArea();
    // Anechoic exit: U_out = p_out / zc; incident wave at the input p+ = (p_in + zc U_in) / 2.
    const Complex pin = total[0] + total[1] / zc, uin = total[2] + total[3] / zc;
    const Complex t = 2.0 / (pin + zc * uin);
    return std::norm(t);
}

std::vector<DuctGap> resonatorDuctGaps(const ResonatorDuctSpec& s, double fmax, int samples)
{
    std::vector<DuctGap> gaps;
    const auto outside = [&](double f) {
        const M2 t = resonatorDuctCell(s, f);
        return std::abs((0.5 * (t[0] + t[3])).real()) > 1.0;
    };
    const auto edge = [&](double in, double out) {
        for (int i = 0; i < 200 && std::abs(out - in) > 1e-9 * std::abs(out); ++i) {
            const double mid = 0.5 * (in + out);
            (outside(mid) ? out : in) = mid;
        }
        return 0.5 * (in + out);
    };
    const double df = fmax / samples;
    bool was = false;
    double start = 0.0;
    for (int i = 1; i <= samples; ++i) {
        const double f = i * df;
        const bool now = outside(f);
        if (now && !was)
            start = edge(f - df, f);
        if (!now && was)
            gaps.push_back({start, edge(f, f - df)});
        was = now;
    }
    return gaps;
}

}  // namespace resonant::manipulators
