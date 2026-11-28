#include "conversion/Photovoltaic.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>

namespace resonant::conversion {
namespace {

constexpr double kQ = 1.602176634e-19;   // C
constexpr double kK = 1.380649e-23;      // J/K
constexpr double kH = 6.62607015e-34;    // J s
constexpr double kC = 2.99792458e8;      // m/s

std::string num(double v)
{
    char b[64];
    std::snprintf(b, sizeof b, "%.6g", v);
    return b;
}

double bandGapAt(const PhotovoltaicCell& c, double t)
{
    const double t0 = c.referenceTemperature;
    const double eg0 = c.bandGap + c.varshniAlpha * t0 * t0 / (t0 + c.varshniBeta);  // at 0 K
    return eg0 - c.varshniAlpha * t * t / (t + c.varshniBeta);
}

double absorptionAt(const PhotovoltaicCell& c, double lambda)
{
    if (c.absorption.empty())
        return c.absorptionAboveGap;
    if (lambda <= c.absorption.front()[0])
        return c.absorption.front()[1];
    if (lambda >= c.absorption.back()[0])
        return c.absorption.back()[1];
    for (size_t i = 1; i < c.absorption.size(); ++i)
        if (lambda <= c.absorption[i][0]) {
            const double t = (lambda - c.absorption[i - 1][0]) / (c.absorption[i][0] - c.absorption[i - 1][0]);
            return c.absorption[i - 1][1] + t * (c.absorption[i][1] - c.absorption[i - 1][1]);
        }
    return c.absorption.back()[1];
}

double irradianceAt(const Spectrum& s, double lambda)
{
    const auto& p = s.points;
    if (lambda <= p.front().wavelength)
        return p.front().irradiance;
    if (lambda >= p.back().wavelength)
        return p.back().irradiance;
    const auto it = std::lower_bound(p.begin(), p.end(), lambda, [](const SpectralPoint& a, double l) { return a.wavelength < l; });
    const auto& b = *it;
    const auto& a = *(it - 1);
    const double t = (lambda - a.wavelength) / (b.wavelength - a.wavelength);
    return a.irradiance + t * (b.irradiance - a.irradiance);
}

// The trapezoidal integral of f(lambda) over the spectrum, the part below
// `cut` (above gap photon energy) or above it; f is evaluated at the points and
// at the cut, so a function that vanishes beyond the gap is integrated exactly
// to second order on each side.
double integrate(const Spectrum& s, double cut, bool belowCut, const std::function<double(double, double)>& f)
{
    double sum = 0.0;
    const auto& p = s.points;
    for (size_t i = 1; i < p.size(); ++i) {
        double a = p[i - 1].wavelength, b = p[i].wavelength;
        if (belowCut) {
            if (a >= cut)
                break;
            b = std::min(b, cut);
        } else {
            if (b <= cut)
                continue;
            a = std::max(a, cut);
        }
        if (b <= a)
            continue;
        sum += 0.5 * (b - a) * (f(a, irradianceAt(s, a)) + f(b, irradianceAt(s, b)));
    }
    return sum;
}

double junctionResidual(const PhotovoltaicCell& c, const PhotovoltaicState& st, double vj, double v)
{
    const double shunt = std::isinf(c.shuntResistance) ? 0.0 : vj / c.shuntResistance;
    return st.photocurrent - st.saturationCurrent * std::expm1(vj / st.modifiedThermalVoltage) - shunt - (vj - v) / c.seriesResistance;
}

}  // namespace

double Spectrum::total() const
{
    double sum = 0.0;
    for (size_t i = 1; i < points.size(); ++i)
        sum += 0.5 * (points[i].wavelength - points[i - 1].wavelength) * (points[i].irradiance + points[i - 1].irradiance);
    return sum;
}

Status Spectrum::validate() const
{
    if (points.size() < 2)
        return Status::failure("a spectrum needs at least two points");
    for (size_t i = 0; i < points.size(); ++i) {
        if (!(points[i].wavelength > 0.0) || points[i].irradiance < 0.0)
            return Status::failure("a spectrum has positive wavelengths and non-negative irradiances");
        if (i > 0 && !(points[i].wavelength > points[i - 1].wavelength))
            return Status::failure("a spectrum's wavelengths must increase");
    }
    return Status::success();
}

Spectrum blackBodySpectrum(double temperature, double total, double minWavelength, double maxWavelength, int count)
{
    Spectrum s;
    s.description = "a black body at " + num(temperature) + " K scaled to " + num(total) + " W/m^2 between " + num(minWavelength * 1e9) + " and " +
                    num(maxWavelength * 1e9) + " nm (a stand-in for a standard spectrum)";
    count = std::max(count, 2);
    for (int i = 0; i < count; ++i) {
        const double l = minWavelength + (maxWavelength - minWavelength) * i / (count - 1);
        const double b = 2 * kH * kC * kC / std::pow(l, 5) / std::expm1(kH * kC / (l * kK * temperature));
        s.points.push_back({l, b});
    }
    const double scale = total / s.total();
    for (auto& p : s.points)
        p.irradiance *= scale;
    return s;
}

Status PhotovoltaicCell::validate() const
{
    if (!(area > 0.0 && bandGap > 0.0 && thickness > 0.0))
        return Status::failure("a cell needs a positive area, band gap and absorber thickness");
    if (!(reflectance >= 0.0 && reflectance < 1.0) || !(collection > 0.0 && collection <= 1.0))
        return Status::failure("the reflectance lies in [0, 1) and the collection fraction in (0, 1]");
    if (!(saturationCurrent > 0.0) || !(idealityFactor > 0.0) || seriesResistance < 0.0 || !(shuntResistance > 0.0))
        return Status::failure("the circuit needs a positive saturation current, ideality factor and shunt resistance, and a non-negative series resistance");
    if (!(referenceTemperature > 0.0))
        return Status::failure("the reference temperature must be positive");
    return Status::success();
}

PhotovoltaicState cellState(const PhotovoltaicCell& c, const Spectrum& s, double t)
{
    PhotovoltaicState st;
    st.temperature = t;
    st.bandGap = bandGapAt(c, t);
    const double cut = kH * kC / (kQ * st.bandGap);
    st.photocurrent = kQ * c.area * integrate(s, cut, true, [&](double l, double e) {
                          const double eqe = (1 - c.reflectance) * -std::expm1(-absorptionAt(c, l) * c.thickness) * c.collection;
                          return eqe * e * l / (kH * kC);
                      });
    st.saturationCurrent = c.saturationCurrent * std::pow(t / c.referenceTemperature, 3) *
                           std::exp(kQ / (c.idealityFactor * kK) * (c.bandGap / c.referenceTemperature - st.bandGap / t));
    st.modifiedThermalVoltage = c.idealityFactor * kK * t / kQ;
    return st;
}

double lambertW0(double x)
{
    if (x == 0.0)
        return 0.0;
    double w = x < 1.0 ? x * (1 - x) : std::log(x) - std::log(std::max(1.0, std::log(x)));
    if (x < -0.3)
        w = -1.0 + std::sqrt(2 * (1 + std::exp(1.0) * x));
    for (int i = 0; i < 100; ++i) {  // Halley
        const double e = std::exp(w), f = w * e - x;
        const double step = f / (e * (w + 1) - (w + 2) * f / (2 * w + 2));
        w -= step;
        if (std::abs(step) <= 1e-16 * (1 + std::abs(w)))
            break;
    }
    return w;
}

double lambertW0OfExp(double x)
{
    if (x < 30.0)
        return lambertW0(std::exp(x));
    double w = x - std::log(x);  // w + ln w = x
    for (int i = 0; i < 100; ++i) {
        const double step = (w + std::log(w) - x) / (1 + 1 / w);
        w -= step;
        if (std::abs(step) <= 1e-16 * w)
            break;
    }
    return w;
}

double cellCurrent(const PhotovoltaicCell& c, const PhotovoltaicState& st, double v)
{
    if (c.seriesResistance == 0.0)
        return st.photocurrent - st.saturationCurrent * std::expm1(v / st.modifiedThermalVoltage) - (std::isinf(c.shuntResistance) ? 0.0 : v / c.shuntResistance);
    // Newton on the junction voltage, bracketed (the residual decreases monotonically).
    double lo = v, hi = v;
    double step = std::max(1e-3, st.modifiedThermalVoltage);
    while (junctionResidual(c, st, lo, v) < 0.0)
        lo -= step, step *= 2;
    step = std::max(1e-3, st.modifiedThermalVoltage);
    while (junctionResidual(c, st, hi, v) > 0.0)
        hi += step, step *= 2;
    double vj = 0.5 * (lo + hi);
    for (int i = 0; i < 200; ++i) {
        const double r = junctionResidual(c, st, vj, v);
        if (r > 0.0)
            lo = vj;
        else
            hi = vj;
        const double d = -st.saturationCurrent / st.modifiedThermalVoltage * std::exp(vj / st.modifiedThermalVoltage) -
                         (std::isinf(c.shuntResistance) ? 0.0 : 1 / c.shuntResistance) - 1 / c.seriesResistance;
        double next = vj - r / d;
        if (!(next > lo && next < hi))
            next = 0.5 * (lo + hi);  // bisection when Newton leaves the bracket
        if (std::abs(next - vj) <= 1e-15 * (1 + std::abs(vj)))
            break;
        vj = next;
    }
    return (vj - v) / c.seriesResistance;
}

double cellCurrentLambertW(const PhotovoltaicCell& c, const PhotovoltaicState& st, double v)
{
    const double a = st.modifiedThermalVoltage, i0 = st.saturationCurrent, iph = st.photocurrent, rs = c.seriesResistance;
    if (rs == 0.0)
        return cellCurrent(c, st, v);  // explicit already
    if (std::isinf(c.shuntResistance))
        return iph + i0 - a / rs * lambertW0OfExp(std::log(rs * i0 / a) + (rs * (iph + i0) + v) / a);
    const double rsh = c.shuntResistance;
    const double x = std::log(rs * i0 * rsh / (a * (rs + rsh))) + rsh * (rs * (iph + i0) + v) / (a * (rs + rsh));
    return (rsh * (iph + i0) - v) / (rs + rsh) - a / rs * lambertW0OfExp(x);
}

Result<PhotovoltaicResult> solvePhotovoltaic(const PhotovoltaicCell& c, const Spectrum& s, double temperature, int curvePoints)
{
    using R = Result<PhotovoltaicResult>;
    if (Status st = c.validate(); !st)
        return R::failure(st.message());
    if (Status st = s.validate(); !st)
        return R::failure(st.message());
    if (!(temperature > 0.0))
        return R::failure("the cell temperature must be positive");
    PhotovoltaicResult out;
    out.state = cellState(c, s, temperature);
    const PhotovoltaicState& st = out.state;
    if (!(st.photocurrent > 0.0))
        return R::failure("no light above the band gap reaches the cell: no photocurrent");
    const auto current = [&](double v) { return cellCurrent(c, st, v); };
    out.shortCircuitCurrent = current(0.0);
    // The open-circuit voltage: I(V) = 0, bracketed by the ideal value.
    double lo = 0.0, hi = st.modifiedThermalVoltage * std::log1p(st.photocurrent / st.saturationCurrent) * 1.001 + 1e-9;
    for (int i = 0; i < 200; ++i) {
        const double mid = 0.5 * (lo + hi);
        (current(mid) > 0.0 ? lo : hi) = mid;
    }
    out.openCircuitVoltage = 0.5 * (lo + hi);
    // The maximum power point: golden section on V I(V) over [0, V_oc].
    double a = 0.0, b = out.openCircuitVoltage;
    const double g = 0.5 * (std::sqrt(5.0) - 1);
    double x1 = b - g * (b - a), x2 = a + g * (b - a);
    double f1 = x1 * current(x1), f2 = x2 * current(x2);
    for (int i = 0; i < 200 && b - a > 1e-12 * out.openCircuitVoltage; ++i) {
        if (f1 > f2) {
            b = x2, x2 = x1, f2 = f1;
            x1 = b - g * (b - a), f1 = x1 * current(x1);
        } else {
            a = x1, x1 = x2, f1 = f2;
            x2 = a + g * (b - a), f2 = x2 * current(x2);
        }
    }
    const double vm = 0.5 * (a + b), im = current(vm);
    out.maximumPower = {vm, im, vm * im};
    out.fillFactor = out.maximumPower.power / (out.openCircuitVoltage * out.shortCircuitCurrent);
    for (int i = 0; i < std::max(curvePoints, 2); ++i) {
        const double v = out.openCircuitVoltage * i / (std::max(curvePoints, 2) - 1);
        const double iv = current(v);
        out.curve.push_back({v, iv, v * iv});
    }
    // The incident light, term by term.
    const double cut = kH * kC / (kQ * st.bandGap);
    const auto absorbedFraction = [&](double l) { return -std::expm1(-absorptionAt(c, l) * c.thickness); };
    out.incident = c.area * (integrate(s, cut, true, [](double, double e) { return e; }) + integrate(s, cut, false, [](double, double e) { return e; }));
    out.reflected = c.reflectance * out.incident;
    out.absorbed = c.area * integrate(s, cut, true, [&](double l, double e) { return (1 - c.reflectance) * absorbedFraction(l) * e; });
    out.transmitted = c.area * (integrate(s, cut, true, [&](double l, double e) { return (1 - c.reflectance) * (1 - absorbedFraction(l)) * e; }) +
                                integrate(s, cut, false, [&](double, double e) { return (1 - c.reflectance) * e; }));
    const double gapEnergy = kQ * st.bandGap;  // J per carrier
    const double absorbedPhotons = c.area * integrate(s, cut, true, [&](double l, double e) { return (1 - c.reflectance) * absorbedFraction(l) * e * l / (kH * kC); });
    out.thermalization = out.absorbed - absorbedPhotons * gapEnergy;
    out.uncollected = (1 - c.collection) * absorbedPhotons * gapEnergy;
    const double vj = vm + im * c.seriesResistance;
    out.voltageDeficit = st.photocurrent * (st.bandGap - vj);
    out.diodeLoss = st.saturationCurrent * std::expm1(vj / st.modifiedThermalVoltage) * vj;
    out.shuntLoss = std::isinf(c.shuntResistance) ? 0.0 : vj * vj / c.shuntResistance;
    out.seriesLoss = im * im * c.seriesResistance;
    out.circuitResidual = st.photocurrent * vj - (out.diodeLoss + out.shuntLoss + out.seriesLoss + out.maximumPower.power);
    out.efficiency = out.incident > 0.0 ? out.maximumPower.power / out.incident : 0.0;
    // The stage.
    ConversionStage& stage = out.stage;
    stage.id = "photovoltaic";
    stage.name = c.name.empty() ? "Photovoltaic cell" : c.name;
    stage.mechanism = "photovoltaic";
    stage.model = "conversion.photovoltaic.single-diode";
    stage.inputForm = EnergyForm::Optical;
    stage.outputForm = EnergyForm::Electrical;
    stage.input = out.incident;
    stage.output = out.maximumPower.power;
    stage.otherOutputs = {{"reflected light", EnergyForm::Optical, out.reflected}, {"transmitted light (below-gap included)", EnergyForm::Optical, out.transmitted}};
    stage.dissipated = out.thermalization + out.uncollected + out.voltageDeficit + out.diodeLoss + out.shuntLoss + out.seriesLoss;
    stage.coupling = {{"bandGap", st.bandGap}, {"photocurrent", st.photocurrent}, {"fillFactor", out.fillFactor}, {"efficiency", out.efficiency}};
    stage.boundaryConditions = {"illumination: " + s.description, "cell temperature " + num(temperature) + " K", "the load at the maximum power point"};
    stage.assumptions = {"the single-diode equivalent circuit (I_0, n, R_s, R_sh given, not derived)", "spectrally flat front reflectance; Beer-Lambert absorption in one pass",
                         "a fraction " + num(c.collection) + " of the generated carriers collected", "the band gap's Varshni dependence; I_0 ~ T^3 exp(-E_g / n k T)"};
    stage.validity = {"no semiconductor transport is solved (drift, diffusion, recombination kinetics)", "uniform illumination and temperature", "steady state"};
    out.assumptions = stage.assumptions;
    return R(std::move(out));
}

}  // namespace resonant::conversion
