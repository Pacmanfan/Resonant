#include "electromagnetics/CavityResonator.h"

#include "dsp/Spectrum.h"
#include "dsp/Waveform.h"
#include "electromagnetics/Yee.h"
#include "simulation/Job.h"

#include <algorithm>
#include <cmath>

namespace resonant::em {

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

// Per component and axis: whether the spatial factor is a cosine (true) or a sine.
struct Component {
    double amplitude = 0.0;    // |amplitude| of the component
    std::array<bool, 3> cosine{};
};

struct ModeShape {
    std::array<double, 3> k{};          // kx, ky, kz
    std::array<Component, 3> e, h;       // amplitudes for the potential normalized to 1
};

// The integral of cos^2 or sin^2 (k x) over [0, L] for k = n pi / L.
double integral(bool cosine, double k, double length)
{
    if (k == 0.0)
        return cosine ? length : 0.0;
    return 0.5 * length;
}

ModeShape shapeOf(const CavityResonatorSpec& s, ModeKind kind, int m, int n, int p, double omega)
{
    const double mu = kMu0 * s.fillPermeability, eps = kEpsilon0 * s.fillPermittivity;
    ModeShape shape;
    const double kx = m * kPi / s.a, ky = n * kPi / s.b, kz = p * kPi / s.d;
    shape.k = {kx, ky, kz};
    const double kc2 = kx * kx + ky * ky;
    if (kind == ModeKind::TE) {
        // Hz = cos(kx x) cos(ky y) sin(kz z).
        shape.e[0] = {omega * mu * ky / kc2, {true, false, false}};
        shape.e[1] = {omega * mu * kx / kc2, {false, true, false}};
        shape.e[2] = {0.0, {false, false, false}};
        shape.h[0] = {kx * kz / kc2, {false, true, true}};
        shape.h[1] = {ky * kz / kc2, {true, false, true}};
        shape.h[2] = {1.0, {true, true, false}};
    } else {
        // Ez = sin(kx x) sin(ky y) cos(kz z).
        shape.e[0] = {kx * kz / kc2, {true, false, false}};
        shape.e[1] = {ky * kz / kc2, {false, true, false}};
        shape.e[2] = {1.0, {false, false, true}};
        shape.h[0] = {omega * eps * ky / kc2, {false, true, true}};
        shape.h[1] = {omega * eps * kx / kc2, {true, false, true}};
        shape.h[2] = {0.0, {false, false, false}};
    }
    return shape;
}

// The largest |E|^2 over the box: the factors are bilinear in cos^2 of each
// axis, so the maximum is at a corner where one component is at its peak.
double peakE2(const ModeShape& shape)
{
    double best = 0.0;
    for (const Component& c : shape.e) {
        bool reachable = true;
        for (int axis = 0; axis < 3; ++axis)
            if (!c.cosine[axis] && shape.k[axis] == 0.0)
                reachable = false;  // sin(0 x) = 0 everywhere
        if (reachable)
            best = std::max(best, c.amplitude * c.amplitude);
    }
    return best;
}

double volumeIntegral(const std::array<Component, 3>& field, const ModeShape& shape, const std::array<double, 3>& size)
{
    double sum = 0.0;
    for (const Component& c : field) {
        double term = c.amplitude * c.amplitude;
        for (int axis = 0; axis < 3; ++axis)
            term *= integral(c.cosine[axis], shape.k[axis], size[axis]);
        sum += term;
    }
    return sum;
}

// The integral of |H_tangential|^2 over all six walls.
double wallIntegral(const ModeShape& shape, const std::array<double, 3>& size)
{
    double sum = 0.0;
    for (int normal = 0; normal < 3; ++normal)
        for (int c = 0; c < 3; ++c) {
            if (c == normal)
                continue;  // the normal component is not tangential
            const Component& h = shape.h[c];
            // On the walls at 0 and L the factor along the normal is cos(0) = cos(n pi) = +-1, or sin = 0.
            if (!h.cosine[normal])
                continue;
            double term = h.amplitude * h.amplitude;
            for (int axis = 0; axis < 3; ++axis)
                if (axis != normal)
                    term *= integral(h.cosine[axis], shape.k[axis], size[axis]);
            sum += 2.0 * term;  // both walls
        }
    return sum;
}

std::string labelOf(ModeKind kind, int m, int n, int p)
{
    return std::string(kind == ModeKind::TE ? "TE" : "TM") + std::to_string(m) + std::to_string(n) + std::to_string(p);
}

}  // namespace

Status CavityResonatorSpec::validate() const
{
    if (!(a > 0.0) || !(b > 0.0) || !(d > 0.0) || !std::isfinite(a * b * d))
        return Status::failure("the cavity dimensions must be positive");
    if (!(fillPermittivity >= 1.0) || !(fillPermeability >= 1.0))
        return Status::failure("the fill's relative permittivity and permeability must be at least 1");
    if (fillLossTangent < 0.0 || fillConductivity < 0.0 || !std::isfinite(fillLossTangent + fillConductivity))
        return Status::failure("the fill's losses cannot be negative");
    if (!std::isfinite(wallConductivity) || !(wallPermeability > 0.0))
        return Status::failure("the wall conductivity must be finite (0 = perfectly conducting)");
    if (externalQ < 0.0 || !std::isfinite(externalQ))
        return Status::failure("the external Q must be positive (0 = no port)");
    return Status::success();
}

Result<std::vector<CavityResonance>> analyzeRectangularCavity(const CavityResonatorSpec& s, int count)
{
    using R = Result<std::vector<CavityResonance>>;
    if (Status v = s.validate(); !v.ok())
        return R::failure(v.message());
    if (count < 1)
        return R::failure("at least one mode must be requested");
    // The frequencies depend on the product er * mr only.
    const std::vector<CavityMode> modes = rectangularCavityModes(s.a, s.b, s.d, count, s.fillPermittivity * s.fillPermeability);
    const std::array<double, 3> size{s.a, s.b, s.d};
    std::vector<CavityResonance> out;
    for (const CavityMode& mode : modes) {
        CavityResonance r;
        r.kind = mode.kind;
        r.m = mode.m;
        r.n = mode.n;
        r.p = mode.p;
        r.label = labelOf(mode.kind, mode.m, mode.n, mode.p);
        r.frequencyHz = mode.frequencyHz;
        const double omega = 2.0 * kPi * r.frequencyHz;
        const ModeShape shape = shapeOf(s, mode.kind, mode.m, mode.n, mode.p, omega);
        const double norm = 1.0 / peakE2(shape);  // scales |field|^2 to a peak |E| of 1 V/m
        const double eps = kEpsilon0 * s.fillPermittivity, mu = kMu0 * s.fillPermeability;
        const double e2 = volumeIntegral(shape.e, shape, size) * norm;
        r.electricEnergy = 0.25 * eps * e2;
        r.magneticEnergy = 0.25 * mu * volumeIntegral(shape.h, shape, size) * norm;
        r.storedEnergy = r.electricEnergy + r.magneticEnergy;
        // Fill: eps'' = eps' tan(delta) + sigma / omega.
        const double epsImag = eps * s.fillLossTangent + s.fillConductivity / omega;
        r.fillLoss = 0.5 * omega * epsImag * e2;
        if (s.wallConductivity > 0.0) {
            const double rs = std::sqrt(omega * kMu0 * s.wallPermeability / (2.0 * s.wallConductivity));
            r.wallLoss = 0.5 * rs * wallIntegral(shape, size) * norm;
        }
        r.wallQ = r.wallLoss > 0.0 ? omega * r.storedEnergy / r.wallLoss : kInf;
        r.fillQ = r.fillLoss > 0.0 ? omega * r.storedEnergy / r.fillLoss : kInf;
        const double loss = r.wallLoss + r.fillLoss;
        r.unloadedQ = loss > 0.0 ? omega * r.storedEnergy / loss : kInf;
        if (s.externalQ > 0.0) {
            r.externalQ = s.externalQ;
            r.coupling = std::isfinite(r.unloadedQ) ? r.unloadedQ / s.externalQ : kInf;
        }
        const double inverseLoaded = (std::isfinite(r.unloadedQ) ? 1.0 / r.unloadedQ : 0.0) + (s.externalQ > 0.0 ? 1.0 / s.externalQ : 0.0);
        r.loadedQ = inverseLoaded > 0.0 ? 1.0 / inverseLoaded : kInf;
        r.bandwidthHz = std::isfinite(r.loadedQ) ? r.frequencyHz / r.loadedQ : 0.0;
        out.push_back(r);
    }
    return R(std::move(out));
}

CavityModeField cavityModeField(const CavityResonatorSpec& s, const CavityResonance& mode, const Vec3& point)
{
    const double omega = 2.0 * kPi * mode.frequencyHz;
    const ModeShape shape = shapeOf(s, mode.kind, mode.m, mode.n, mode.p, omega);
    const double scale = 1.0 / std::sqrt(peakE2(shape));
    const auto factor = [&](const Component& c) {
        double v = c.amplitude * scale;
        for (int axis = 0; axis < 3; ++axis)
            v *= c.cosine[axis] ? std::cos(shape.k[axis] * point[axis]) : std::sin(shape.k[axis] * point[axis]);
        return v;
    };
    const Complex j(0.0, 1.0);
    CavityModeField f;
    const double kx = shape.k[0], ky = shape.k[1];
    if (mode.kind == ModeKind::TE) {
        // Signs of the potential solution: Ex = +j.., Ey = -j.., Hx = -.., Hy = -.., Hz = +..
        f.e = {j * factor(shape.e[0]), -j * factor(shape.e[1]), Complex(0.0)};
        f.h = {-factor(shape.h[0]), -factor(shape.h[1]), factor(shape.h[2])};
    } else {
        f.e = {-factor(shape.e[0]), -factor(shape.e[1]), factor(shape.e[2])};
        f.h = {j * factor(shape.h[0]), -j * factor(shape.h[1]), Complex(0.0)};
    }
    (void)kx;
    (void)ky;
    return f;
}

double te10pWallQ(const CavityResonatorSpec& s, int p, double frequencyHz)
{
    const double omega = 2.0 * kPi * frequencyHz;
    const double k = omega * std::sqrt(kMu0 * s.fillPermeability * kEpsilon0 * s.fillPermittivity);
    const double eta = std::sqrt(kMu0 * s.fillPermeability / (kEpsilon0 * s.fillPermittivity));
    const double rs = std::sqrt(omega * kMu0 * s.wallPermeability / (2.0 * s.wallConductivity));
    const double a = s.a, b = s.b, d = s.d, l = p;
    const double num = std::pow(k * a * d, 3) * b * eta;
    const double den = 2.0 * kPi * kPi * rs * (2.0 * l * l * a * a * a * b + 2.0 * b * d * d * d + l * l * a * a * a * d + a * d * d * d);
    return num / den;
}

Complex portReflection(const CavityResonance& mode, double frequencyHz)
{
    if (!(mode.coupling > 0.0) || !std::isfinite(mode.coupling))
        return Complex(1.0, 0.0);
    const double detune = frequencyHz / mode.frequencyHz - mode.frequencyHz / frequencyHz;
    const Complex y = Complex(1.0, mode.unloadedQ * detune) / mode.coupling;
    return (1.0 - y) / (1.0 + y);
}

double resonanceSensitivity(const CavityResonatorSpec& s, const CavityResonance& mode, int dimension)
{
    const double c = kSpeedOfLight / std::sqrt(s.fillPermittivity * s.fillPermeability);
    const std::array<double, 3> size{s.a, s.b, s.d};
    const std::array<int, 3> index{mode.m, mode.n, mode.p};
    const double L = size[static_cast<size_t>(dimension)], i = index[static_cast<size_t>(dimension)];
    return -(c * c / 4.0) * i * i / (L * L * L) / mode.frequencyHz;
}

Result<CavityYeeResult> simulateRectangularCavity(const CavityResonatorSpec& s, const CavityYeeSettings& settings, JobContext* context)
{
    using R = Result<CavityYeeResult>;
    if (Status v = s.validate(); !v.ok())
        return R::failure(v.message());
    if (settings.cellsAcrossSmallest < 4 || settings.cellsAcrossSmallest > 80)
        return R::failure("between 4 and 80 cells across the smallest dimension");
    if (!(settings.durationPeriods >= 20.0))
        return R::failure("the run must last at least 20 periods of the lowest resonance");
    auto analytical = analyzeRectangularCavity(s, 1);
    if (!analytical)
        return R::failure(analytical.message());
    const double f1 = analytical.value()[0].frequencyHz;
    CavityYeeResult out;
    const double smallest = std::min({s.a, s.b, s.d});
    out.cell = smallest / settings.cellsAcrossSmallest;
    const std::array<double, 3> size{s.a, s.b, s.d};
    for (int axis = 0; axis < 3; ++axis) {
        out.cells[static_cast<size_t>(axis)] = std::max(2, static_cast<int>(std::lround(size[static_cast<size_t>(axis)] / out.cell)));
        out.simulatedSize[static_cast<size_t>(axis)] = out.cells[static_cast<size_t>(axis)] * out.cell;
    }
    const long total = static_cast<long>(out.cells[0]) * out.cells[1] * out.cells[2];
    if (total > 4'000'000)
        return R::failure("the grid would have " + std::to_string(total) + " cells (at most 4 000 000)");
    YeeSpec spec;
    spec.cell = out.cell;
    spec.nx = out.cells[0];
    spec.ny = out.cells[1];
    spec.nz = out.cells[2];
    spec.boundary.fill(YeeBoundary::PEC);
    YeeMaterial fill;
    fill.name = "fill";
    fill.relativePermittivity = s.fillPermittivity;
    fill.relativePermeability = s.fillPermeability;
    out.fillConductivityUsed = s.fillConductivity + 2.0 * kPi * f1 * kEpsilon0 * s.fillPermittivity * s.fillLossTangent;
    fill.conductivity = out.fillConductivityUsed;
    spec.materials = {fill};
    spec.dt = 0.95 * yeeStabilityLimit(spec.cell, kSpeedOfLight / std::sqrt(s.fillPermittivity * s.fillPermeability), 3);
    const double fmax = settings.maxFrequencyHz > 0.0 ? settings.maxFrequencyHz : 2.5 * f1;
    dsp::Waveform pulse;
    pulse.kind = dsp::WaveformKind::Ricker;
    pulse.frequencyHz = 0.5 * (f1 + fmax);
    // Sources and probes at points of no symmetry so that TE and TM modes are excited and seen.
    const auto at = [&](double fx, double fy, double fz) {
        return std::array<int, 3>{std::clamp(static_cast<int>(fx * spec.nx), 1, spec.nx - 1), std::clamp(static_cast<int>(fy * spec.ny), 1, spec.ny - 1),
                                  std::clamp(static_cast<int>(fz * spec.nz), 1, spec.nz - 1)};
    };
    const auto src = at(0.23, 0.31, 0.27), probe = at(0.71, 0.63, 0.77);
    std::vector<YeeSource> sources;
    for (int c = 0; c < 3; ++c)
        sources.push_back({src[0], src[1], src[2], c, 1.0, 0.0, pulse});
    std::vector<YeeProbe> probes;
    for (int c = 0; c < 3; ++c)
        probes.push_back({std::string("E") + "xyz"[c], probe[0], probe[1], probe[2], c});
    out.steps = std::lround(settings.durationPeriods / f1 / spec.dt);
    YeeRecording rec;
    rec.energySamples = 400;
    YeeOutput y;
    const YeeRunEnd end = runYee(spec, sources, probes, out.steps, rec, y, context);
    if (end.kind == YeeRunEnd::Kind::Cancelled)
        return R::failure("cancelled");
    if (end.kind != YeeRunEnd::Kind::Completed)
        return R::failure("the full-wave run failed: " + end.message);
    out.dt = y.dt;
    out.seconds = y.elapsedSeconds;
    // The sum of the three components: every mode (TE101 has only E_y) appears in it.
    if (!y.probes.empty()) {
        out.probeSignal.assign(y.probes[0].size(), 0.0);
        for (const auto& component : y.probes)
            for (size_t i = 0; i < component.size() && i < out.probeSignal.size(); ++i)
                out.probeSignal[i] += component[i];
    }
    out.pulseSteps = static_cast<size_t>(std::ceil(3.0 / pulse.frequencyHz / y.dt));  // the Ricker pulse lasts about 2.5 periods
    out.backend = y.backend;
    // Peaks of the summed power spectra of the three components.
    dsp::Spectrum sum;
    for (size_t c = 0; c < y.probes.size(); ++c) {
        dsp::Spectrum sp = dsp::computeSpectrum(y.probes[c], y.dt, dsp::Window::Hann, 8);
        if (c == 0) {
            sum = sp;
            for (auto& v : sum.amplitude)
                v = std::norm(v);
        } else {
            for (size_t i = 0; i < sum.amplitude.size() && i < sp.amplitude.size(); ++i)
                sum.amplitude[i] += std::norm(sp.amplitude[i]);
        }
    }
    for (auto& v : sum.amplitude)
        v = std::sqrt(v.real());
    for (const auto& peak : dsp::findPeaks(sum, 0.02, 0.3 * f1, fmax))
        out.resonancesHz.push_back(peak.frequencyHz);
    std::sort(out.resonancesHz.begin(), out.resonancesHz.end());
    // Energy decay: a uniform conducting fill takes energy at the rate sigma / eps from every mode.
    if (out.fillConductivityUsed > 0.0 && y.energy.size() >= 40) {
        const size_t n = y.energy.size();
        // Means over windows (the record ripples with the field), then a least-squares line of log energy.
        const size_t windows = 10, from = n / 3, width = (n - from) / windows;
        double sx = 0, sy = 0, sxx = 0, sxy = 0;
        int used = 0;
        for (size_t w = 0; w < windows; ++w) {
            double e = 0, t = 0;
            for (size_t i = from + w * width; i < from + (w + 1) * width; ++i) {
                e += y.energy[i];
                t += y.energyTimes[i];
            }
            e /= static_cast<double>(width);
            t /= static_cast<double>(width);
            if (!(e > 0.0))
                continue;
            sx += t;
            sy += std::log(e);
            sxx += t * t;
            sxy += t * std::log(e);
            ++used;
        }
        if (used >= 3)
            out.energyDecayRate = -(used * sxy - sx * sy) / (used * sxx - sx * sx);
    }
    out.notes.push_back("perfectly conducting walls: wall losses are not part of this model");
    if (s.fillLossTangent > 0.0)
        out.notes.push_back("the loss tangent is represented by the conductivity it equals at " + std::to_string(f1 / 1e9).substr(0, 6) + " GHz");
    for (int axis = 0; axis < 3; ++axis)
        if (std::abs(out.simulatedSize[static_cast<size_t>(axis)] - size[static_cast<size_t>(axis)]) > 1e-9 * size[static_cast<size_t>(axis)])
            out.notes.push_back(std::string("dimension ") + "abd"[axis] + " rounded to whole cells: " +
                                std::to_string(out.simulatedSize[static_cast<size_t>(axis)] * 1e3).substr(0, 8) + " mm");
    return R(std::move(out));
}

}  // namespace resonant::em
