// Numerical validation of the acoustic FDTD solver against closed-form
// references. docs/advanced_acoustics/ACOUSTIC_VALIDATION.md records the
// results; every tolerance here is stated there.
#include "fdtd/FdtdRun.h"
#include "dsp/Spectrum.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <cmath>

using namespace resonant;
using namespace resonant::fdtd;
using namespace resonant::dsp;

namespace {

constexpr double pi = 3.14159265358979323846;
constexpr double kC = 340.0;
constexpr double kRho = 1.2;

CellMaterial gas()
{
    return CellMaterial{"test gas", kRho, kC, 0.0, false, false};
}

// A two-dimensional grid of the test gas with the same boundary everywhere.
GridSpec grid2D(int nx, int ny, double dx, double courantFraction, BoundaryKind boundary)
{
    GridSpec g;
    g.nx = nx;
    g.ny = ny;
    g.nz = 1;
    g.dx = dx;
    g.materials = {gas()};
    g.boundary.fill(boundary);
    g.dt = courantFraction * stabilityLimit(dx, kC, 2);
    return g;
}

// A "duct": two cells wide with rigid side walls, so that a column of equal
// sources radiates plane waves along X. The X ends are given.
GridSpec duct(int nx, double dx, double courant, BoundaryKind xMin, BoundaryKind xMax)
{
    GridSpec g = grid2D(nx, 2, dx, 1.0, BoundaryKind::Rigid);
    g.boundary[XMin] = xMin;
    g.boundary[XMax] = xMax;
    g.dt = courant * dx / kC;  // `courant` is S = c dt / dx here
    return g;
}

void addPlaneSource(Problem& problem, int i, const Waveform& waveform, double strength = 1.0)
{
    for (int j = 0; j < problem.grid.ny; ++j)
        problem.sources.push_back({i, j, 0, strength, 0.0, waveform});
}

Waveform ricker(double f)
{
    Waveform w;
    w.kind = WaveformKind::Ricker;
    w.frequencyHz = f;
    return w;
}

Waveform continuous(double f)
{
    Waveform w;
    w.kind = WaveformKind::Continuous;
    w.frequencyHz = f;
    return w;
}

double peakAbs(const std::vector<double>& x, size_t from = 0, size_t to = SIZE_MAX)
{
    double best = 0.0;
    for (size_t i = from; i < std::min(to, x.size()); ++i)
        best = std::max(best, std::abs(x[i]));
    return best;
}

// Signed value of the largest excursion in a window.
double signedPeak(const std::vector<double>& x, size_t from, size_t to)
{
    double best = 0.0;
    for (size_t i = from; i < std::min(to, x.size()); ++i)
        if (std::abs(x[i]) > std::abs(best))
            best = x[i];
    return best;
}

std::complex<double> hankel2(double x)
{
    return {std::cyl_bessel_j(0.0, x), -std::cyl_neumann(0.0, x)};
}

RunOutput mustRun(const Problem& problem)
{
    RunOutput out;
    const RunEnd end = run(problem, out);
    EXPECT_EQ(end.kind, RunEnd::Kind::Completed) << end.message;
    return out;
}

// Measured phase velocity over the true one, for a plane wave along X with
// N points per wavelength and Courant number S.
double measuredPhaseVelocityRatio(double pointsPerWavelength, double courant)
{
    const double dx = 1e-3;
    const double f = kC / (pointsPerWavelength * dx);
    Problem problem;
    problem.grid = duct(static_cast<int>(12 * pointsPerWavelength), dx, courant, BoundaryKind::Absorbing, BoundaryKind::Absorbing);
    problem.grid.pmlCells = 20;
    addPlaneSource(problem, 10, continuous(f));
    const double period = 1.0 / f;
    problem.steps = std::lround(40.0 * period / problem.grid.dt);
    problem.recording.harmonicFrequencyHz = f;
    problem.recording.harmonicPeriods = 10.0;
    const RunOutput out = mustRun(problem);
    // Phase lag over a separation of several wavelengths.
    const int i0 = static_cast<int>(2 * pointsPerWavelength);
    const int separation = static_cast<int>(8 * pointsPerWavelength);
    const std::complex<double> a = out.harmonic[problem.grid.interiorIndex(i0, 0, 0)];
    const std::complex<double> b = out.harmonic[problem.grid.interiorIndex(i0 + separation, 0, 0)];
    double lag = std::arg(a / b);  // positive: b lags a
    // The true lag is close to 8 wavelengths; unwrap towards it.
    const double expected = 2.0 * pi * 8.0;
    lag += 2.0 * pi * std::round((expected - lag) / (2.0 * pi));
    return expected / lag;
}

}  // namespace

// --- waveforms --------------------------------------------------------------------

TEST(FdtdWaveform, SignalsStartAtZeroAndHaveTheStatedShape)
{
    Waveform burst;
    burst.kind = WaveformKind::ToneBurst;
    burst.frequencyHz = 1000.0;
    burst.cycles = 4.0;
    EXPECT_DOUBLE_EQ(burst.value(-1e-6), 0.0);
    EXPECT_NEAR(burst.value(0.0), 0.0, 1e-15);
    EXPECT_NEAR(burst.value(0.002), 1.0, 1e-12);  // middle of the Hann envelope, a carrier maximum
    EXPECT_DOUBLE_EQ(burst.value(0.0041), 0.0);
    EXPECT_DOUBLE_EQ(burst.length(), 0.004);
    EXPECT_DOUBLE_EQ(burst.highestFrequency(), 1500.0);

    const Waveform cw = continuous(1000.0);
    EXPECT_NEAR(cw.value(0.0), 0.0, 1e-15);
    EXPECT_NEAR(cw.value(0.010), 1.0, 1e-12);  // after the ramp: cos(2 pi 10)
    EXPECT_TRUE(std::isinf(cw.length()));

    // A Ricker wavelet has zero mean: no static pressure is left behind.
    const Waveform r = ricker(1000.0);
    double sum = 0.0;
    for (int i = 0; i < 30000; ++i)
        sum += r.value(i * 1e-7) * 1e-7;
    EXPECT_NEAR(sum, 0.0, 1e-9);
    EXPECT_NEAR(r.value(1.5e-3), 1.0, 1e-12);

    Waveform chirp;
    chirp.kind = WaveformKind::Chirp;
    chirp.frequencyHz = 1000.0;
    chirp.endFrequencyHz = 5000.0;
    chirp.duration = 0.01;
    EXPECT_DOUBLE_EQ(chirp.highestFrequency(), 5000.0);
    EXPECT_DOUBLE_EQ(chirp.value(0.011), 0.0);

    Waveform sampled;
    sampled.kind = WaveformKind::Sampled;
    sampled.samples = {0.0, 1.0, 0.0, -1.0};
    sampled.sampleRateHz = 4000.0;
    EXPECT_DOUBLE_EQ(sampled.value(0.000125), 0.5);  // linear interpolation
    EXPECT_DOUBLE_EQ(sampled.value(0.00075), -1.0);
    EXPECT_DOUBLE_EQ(sampled.value(0.001), 0.0);
    EXPECT_DOUBLE_EQ(sampled.highestFrequency(), 2000.0);

    // Repetition.
    burst.repetitionPeriod = 0.01;
    EXPECT_NEAR(burst.value(0.012), 1.0, 1e-9);
}

TEST(FdtdWaveform, InvalidSettingsAreRefusedAndJsonRoundTrips)
{
    Waveform w;
    w.frequencyHz = -1.0;
    EXPECT_FALSE(w.validate().ok());
    w = Waveform{};
    w.kind = WaveformKind::ToneBurst;
    w.cycles = 0.5;
    EXPECT_FALSE(w.validate().ok());
    w.cycles = 5.0;
    w.repetitionPeriod = 1e-6;  // shorter than the burst
    EXPECT_FALSE(w.validate().ok());
    w = Waveform{};
    w.kind = WaveformKind::Sampled;
    EXPECT_FALSE(w.validate().ok());
    w = Waveform{};
    w.repetitionPeriod = 1.0;  // a continuous signal cannot repeat
    EXPECT_FALSE(w.validate().ok());

    Waveform pulse;
    pulse.kind = WaveformKind::GaussianPulse;
    pulse.frequencyHz = 25000.0;
    pulse.fractionalBandwidth = 0.8;
    pulse.phase = 0.3;
    const auto loaded = waveformFromJson(toJson(pulse));
    ASSERT_TRUE(loaded.ok()) << loaded.message();
    EXPECT_EQ(loaded.value(), pulse);
    EXPECT_FALSE(waveformFromJson(nlohmann::json{{"kind", "square"}}).ok());
    EXPECT_NE(pulse.describe().find("Gaussian pulse"), std::string::npos);
}

TEST(FdtdWaveform, GaussianPulseHasTheStatedBandwidth)
{
    Waveform pulse;
    pulse.kind = WaveformKind::GaussianPulse;
    pulse.frequencyHz = 40000.0;
    pulse.fractionalBandwidth = 0.5;
    const double dt = 1e-7;
    std::vector<double> samples;
    for (int i = 0; i * dt < pulse.length(); ++i)
        samples.push_back(pulse.value(i * dt));
    const Spectrum s = computeSpectrum(samples, dt, Window::Rectangular, 16);
    // -6 dB points at 40 kHz -/+ 10 kHz.
    double peak = 0.0;
    for (const auto& a : s.amplitude)
        peak = std::max(peak, std::abs(a));
    const auto at = [&](double f) { return std::abs(s.amplitude[static_cast<size_t>(std::lround(f / s.binSpacingHz))]) / peak; };
    EXPECT_NEAR(at(40000.0), 1.0, 0.01);
    EXPECT_NEAR(at(30000.0), 0.5, 0.02);
    EXPECT_NEAR(at(50000.0), 0.5, 0.02);
}

// --- spectra ----------------------------------------------------------------------

TEST(FdtdSpectrum, SinusoidReadsItsAmplitudeAndPhaseAndResolutionIsNotImprovedByPadding)
{
    const double dt = 1e-5;
    std::vector<double> x(1024);  // 10.24 ms: exactly 20 periods
    const double f = 20.0 / (1024.0 * dt);
    for (size_t i = 0; i < x.size(); ++i)
        x[i] = 3.0 * std::cos(2.0 * pi * f * static_cast<double>(i) * dt + 0.7) + 0.5;
    for (const Window window : {Window::Rectangular, Window::Hann}) {
        const Spectrum s = computeSpectrum(x, dt, window, 8);
        const auto peaks = findPeaks(s, 0.5, 100.0);
        ASSERT_EQ(peaks.size(), 1u);
        EXPECT_NEAR(peaks[0].frequencyHz, f, 2.0);
        EXPECT_NEAR(peaks[0].magnitude, 3.0, 0.01);
        const std::complex<double> a = s.amplitude[static_cast<size_t>(std::lround(f / s.binSpacingHz))];
        EXPECT_NEAR(std::arg(a), 0.7, 0.02);
        EXPECT_NEAR(std::abs(s.amplitude[0]), 0.5, 0.01);  // the mean
        EXPECT_DOUBLE_EQ(s.nyquistHz, 50000.0);
        EXPECT_DOUBLE_EQ(s.observationSeconds, 0.01024);
        // 1 / T for the rectangular window, 2 / T for Hann - whatever the padding.
        EXPECT_DOUBLE_EQ(s.resolutionHz, (window == Window::Hann ? 2.0 : 1.0) / 0.01024);
        EXPECT_LT(s.binSpacingHz, 0.2 * s.resolutionHz);
        EXPECT_NE(s.describe().find("frequency resolution"), std::string::npos);
    }
    EXPECT_DOUBLE_EQ(computeSpectrum(x, dt, Window::Rectangular, 1).resolutionHz, 1.0 / 0.01024);
}

TEST(FdtdSpectrum, TwoComponentsCloserThanTheResolutionAreNotSeparated)
{
    const double dt = 1e-5;
    const auto record = [&](double separation, size_t n) {
        std::vector<double> x(n);
        // In phase at the middle of the record, where the window weighs most.
        for (size_t i = 0; i < n; ++i) {
            const double t = (static_cast<double>(i) - 0.5 * static_cast<double>(n)) * dt;
            x[i] = std::cos(2.0 * pi * 5000.0 * t) + std::cos(2.0 * pi * (5000.0 + separation) * t);
        }
        return x;
    };
    // 10 ms observation, Hann: resolution 200 Hz.
    EXPECT_EQ(findPeaks(computeSpectrum(record(80.0, 1000), dt), 0.3, 4000.0, 6000.0).size(), 1u);
    EXPECT_EQ(findPeaks(computeSpectrum(record(400.0, 1000), dt), 0.3, 4000.0, 6000.0).size(), 2u);
    // Observing ten times longer separates the close pair.
    EXPECT_EQ(findPeaks(computeSpectrum(record(80.0, 10000), dt), 0.3, 4000.0, 6000.0).size(), 2u);
}

// --- stability ----------------------------------------------------------------------

TEST(FdtdStability, TimeStepBeyondTheCourantLimitIsRefused)
{
    EXPECT_DOUBLE_EQ(stabilityLimit(1e-3, 340.0, 2), 1e-3 / (340.0 * std::sqrt(2.0)));
    EXPECT_DOUBLE_EQ(stabilityLimit(1e-3, 340.0, 3), 1e-3 / (340.0 * std::sqrt(3.0)));
    GridSpec g = grid2D(40, 40, 1e-3, 1.0, BoundaryKind::Rigid);
    EXPECT_TRUE(g.validate().ok());
    g.dt *= 1.001;
    const Status status = g.validate();
    ASSERT_FALSE(status.ok());
    EXPECT_NE(status.message().find("stability limit"), std::string::npos);
    EXPECT_FALSE(FdtdSolver::create(g).ok());

    // A faster material anywhere in the grid lowers the limit.
    g = grid2D(40, 40, 1e-3, 1.0, BoundaryKind::Rigid);
    g.materials.push_back(CellMaterial{"fast", 1000.0, 1500.0, 0.0, false, false});
    g.cellMaterial.assign(1600, 0);
    EXPECT_TRUE(g.validate().ok());  // listed but not used
    g.cellMaterial[820] = 1;
    EXPECT_FALSE(g.validate().ok());
    g.dt = stabilityLimit(g.dx, 1500.0, 2);
    EXPECT_TRUE(g.validate().ok());

    // Three dimensions have their own limit.
    GridSpec g3 = g;
    g3.cellMaterial.clear();
    g3.nz = 10;
    g3.dt = stabilityLimit(g3.dx, kC, 2);
    EXPECT_FALSE(g3.validate().ok());
    g3.dt = stabilityLimit(g3.dx, kC, 3);
    EXPECT_TRUE(g3.validate().ok());
}

TEST(FdtdStability, AtTheLimitTheEnergyOfAClosedRigidBoxStaysConstant)
{
    for (const double fraction : {0.5, 0.99}) {
        Problem problem;
        problem.grid = grid2D(60, 45, 1e-3, fraction, BoundaryKind::Rigid);
        problem.sources.push_back({17, 11, 0, 1.0, 0.0, ricker(20000.0)});
        problem.steps = 6000;
        problem.recording.energySamples = 600;
        const RunOutput out = mustRun(problem);
        // The source has ended after 3 / f = 150 us.
        double lowest = 1e300, highest = 0.0;
        for (size_t i = 0; i < out.energy.size(); ++i) {
            if (out.energyTimes[i] < 2e-4)
                continue;
            lowest = std::min(lowest, out.energy[i]);
            highest = std::max(highest, out.energy[i]);
        }
        EXPECT_GT(lowest, 0.0);
        EXPECT_LT(highest / lowest, 1.0 + 1e-7) << fraction;
    }
}

// --- propagation ----------------------------------------------------------------------

TEST(FdtdPropagation, PlaneWavePhaseVelocityFollowsTheDispersionRelation)
{
    // Spatial refinement at a fixed Courant number: the error falls with the
    // square of the resolution (second-order scheme).
    const double s = 0.5;
    const double e8 = 1.0 - measuredPhaseVelocityRatio(8.0, s);
    const double e16 = 1.0 - measuredPhaseVelocityRatio(16.0, s);
    const double e32 = 1.0 - measuredPhaseVelocityRatio(32.0, s);
    EXPECT_NEAR(e8, 1.0 - axialPhaseVelocityRatio(8.0, s), 2e-4);
    EXPECT_NEAR(e16, 1.0 - axialPhaseVelocityRatio(16.0, s), 1e-4);
    EXPECT_NEAR(e32, 1.0 - axialPhaseVelocityRatio(32.0, s), 1e-4);
    EXPECT_GT(e8, 0.0);  // the numerical wave is slower than the true one
    EXPECT_NEAR(e8 / e16, 4.0, 0.4);
    EXPECT_LT(e16, 0.005);  // 16 points per wavelength: within 0.5 %

    // Temporal refinement at a fixed grid: the error GROWS as the time step
    // shrinks (space and time errors cancel at the 1D limit S = 1).
    const double coarse = 1.0 - measuredPhaseVelocityRatio(16.0, 0.7);
    const double fine = 1.0 - measuredPhaseVelocityRatio(16.0, 0.35);
    EXPECT_NEAR(coarse, 1.0 - axialPhaseVelocityRatio(16.0, 0.7), 1e-4);
    EXPECT_NEAR(fine, 1.0 - axialPhaseVelocityRatio(16.0, 0.35), 1e-4);
    EXPECT_GT(fine, coarse);
    EXPECT_NEAR(axialPhaseVelocityRatio(16.0, 1.0), 1.0, 1e-12);
}

TEST(FdtdPropagation, PulseTravelsAtTheSpeedOfSound)
{
    Problem problem;
    const double dx = 1e-3, f = 4000.0;  // 85 points per wavelength at the peak frequency
    problem.grid = duct(700, dx, 0.7, BoundaryKind::Absorbing, BoundaryKind::Absorbing);
    addPlaneSource(problem, 50, ricker(f));
    problem.probes = {{Id("a"), "a", Vec3::Zero(), 150, 0, 0}, {Id("b"), "b", Vec3::Zero(), 650, 0, 0}};
    problem.steps = std::lround((0.65 / kC + 4.0 / f) / problem.grid.dt);
    const RunOutput out = mustRun(problem);
    const auto arrival = [&](const std::vector<double>& x) {
        size_t best = 0;
        for (size_t i = 0; i < x.size(); ++i)
            if (std::abs(x[i]) > std::abs(x[best]))
                best = i;
        return static_cast<double>(best) * out.dt;
    };
    const double speed = 0.5 / (arrival(out.probes[1].pressure) - arrival(out.probes[0].pressure));
    EXPECT_NEAR(speed, kC, 0.004 * kC);
    // A plane wave in a lossless duct keeps its amplitude.
    EXPECT_NEAR(peakAbs(out.probes[1].pressure) / peakAbs(out.probes[0].pressure), 1.0, 0.02);
}

TEST(FdtdPropagation, PointSourceIn3DSpreadsAsOneOverRWithTheCalibratedAmplitude)
{
    // strength = 4 pi A / rho gives the pressure amplitude A at 1 m.
    const double dx = 2e-3, f = kC / (12.0 * dx), amplitude = 2.0;
    Problem problem;
    GridSpec& g = problem.grid;
    g.nx = g.ny = g.nz = 48;
    g.dx = dx;
    g.materials = {gas()};
    g.pmlCells = 10;
    g.dt = 0.95 * stabilityLimit(dx, kC, 3);
    problem.sources.push_back({24, 24, 24, 4.0 * pi * amplitude / kRho, 0.0, continuous(f)});
    const double period = 1.0 / f;
    problem.steps = std::lround(22.0 * period / g.dt);
    problem.recording.harmonicFrequencyHz = f;
    problem.recording.harmonicPeriods = 6.0;
    const RunOutput out = mustRun(problem);
    const auto magnitude = [&](int cells) { return std::abs(out.harmonic[g.interiorIndex(24 + cells, 24, 24)]); };
    for (const int cells : {8, 12, 18}) {
        const double r = cells * dx;
        EXPECT_NEAR(magnitude(cells), amplitude / r, 0.03 * amplitude / r) << cells;
    }
    // Along a diagonal as well (the point source occupies one cubic cell and
    // numerical dispersion is anisotropic: 5 % here).
    const double rDiagonal = 10.0 * dx * std::sqrt(3.0);
    EXPECT_NEAR(std::abs(out.harmonic[g.interiorIndex(34, 34, 34)]), amplitude / rDiagonal, 0.05 * amplitude / rDiagonal);
    // Phase progression: k r between two radii.
    const double lag = std::arg(out.harmonic[g.interiorIndex(32, 24, 24)] / out.harmonic[g.interiorIndex(35, 24, 24)]);
    EXPECT_NEAR(lag, 2.0 * pi * f * 3.0 * dx / kC, 0.05);
}

TEST(FdtdPropagation, LineSourceIn2DFollowsTheHankelFunction)
{
    const double dx = 1e-3, f = kC / (14.0 * dx);
    const double k = 2.0 * pi * f / kC;
    Problem problem;
    problem.grid = grid2D(160, 160, dx, 0.95, BoundaryKind::Absorbing);
    // Volume acceleration per metre S: p = (rho S / 4) |H0(2)(k r)|.
    const double strength = 3.0;
    problem.sources.push_back({80, 80, 0, strength, 0.0, continuous(f)});
    problem.steps = std::lround(30.0 / f / problem.grid.dt);
    problem.recording.harmonicFrequencyHz = f;
    problem.recording.harmonicPeriods = 8.0;
    const RunOutput out = mustRun(problem);
    for (const int cells : {15, 30, 60}) {
        const double expected = 0.25 * kRho * strength * std::abs(hankel2(k * cells * dx));
        EXPECT_NEAR(std::abs(out.harmonic[problem.grid.interiorIndex(80 + cells, 80, 0)]), expected, 0.03 * expected) << cells;
        EXPECT_NEAR(std::abs(out.harmonic[problem.grid.interiorIndex(80, 80 - cells, 0)]), expected, 0.03 * expected) << cells;
    }
    // Relative phase between two radii.
    const std::complex<double> ratio = out.harmonic[problem.grid.interiorIndex(100, 80, 0)] / out.harmonic[problem.grid.interiorIndex(130, 80, 0)];
    const std::complex<double> expected = hankel2(k * 20 * dx) / hankel2(k * 50 * dx);
    // 30 cells at 14 points per wavelength: the numerical wave lags by 0.6 %
    // of 13.5 rad.
    EXPECT_NEAR(std::arg(ratio / expected), 0.0, 0.12);
}

TEST(FdtdPropagation, TwoSourcesInterfere)
{
    const double dx = 1e-3, f = kC / (14.0 * dx);
    const double k = 2.0 * pi * f / kC;
    Problem problem;
    problem.grid = grid2D(160, 120, dx, 0.95, BoundaryKind::Absorbing);
    // Two equal sources 3 wavelengths (42 cells) apart.
    problem.sources.push_back({59, 30, 0, 1.0, 0.0, continuous(f)});
    problem.sources.push_back({101, 30, 0, 1.0, 0.0, continuous(f)});
    problem.steps = std::lround(34.0 / f / problem.grid.dt);
    problem.recording.harmonicFrequencyHz = f;
    const RunOutput out = mustRun(problem);
    double worst = 0.0, scale = 0.0;
    for (int i = 20; i < 140; i += 3) {
        const int j = 100;
        const double r1 = dx * std::hypot(i - 59, j - 30), r2 = dx * std::hypot(i - 101, j - 30);
        const double expected = 0.25 * kRho * std::abs(hankel2(k * r1) + hankel2(k * r2));
        worst = std::max(worst, std::abs(std::abs(out.harmonic[problem.grid.interiorIndex(i, j, 0)]) - expected));
        scale = std::max(scale, expected);
    }
    EXPECT_LT(worst, 0.05 * scale);
    // On the perpendicular bisector the two arrive in phase; where the paths
    // differ by half a wavelength they cancel.
    const double onAxis = std::abs(out.harmonic[problem.grid.interiorIndex(80, 100, 0)]);
    double minimum = 1e300;
    for (int i = 80; i < 110; ++i)
        minimum = std::min(minimum, std::abs(out.harmonic[problem.grid.interiorIndex(i, 100, 0)]));
    EXPECT_LT(minimum, 0.08 * onAxis);
}

TEST(FdtdPropagation, TimeDelayShiftsTheWholeSignal)
{
    Problem problem;
    problem.grid = duct(400, 1e-3, 0.7, BoundaryKind::Absorbing, BoundaryKind::Absorbing);
    addPlaneSource(problem, 50, ricker(10000.0));
    problem.probes = {{Id("p"), "p", Vec3::Zero(), 250, 0, 0}};
    problem.steps = 900;
    const RunOutput reference = mustRun(problem);
    const long shift = 120;
    for (GridSource& s : problem.sources)
        s.delay = static_cast<double>(shift) * problem.grid.dt;
    const RunOutput delayed = mustRun(problem);
    const auto& a = reference.probes[0].pressure;
    const auto& b = delayed.probes[0].pressure;
    const double scale = peakAbs(a);
    double worst = 0.0;
    for (size_t i = static_cast<size_t>(shift); i < b.size(); ++i)
        worst = std::max(worst, std::abs(b[i] - a[i - static_cast<size_t>(shift)]));
    EXPECT_LT(worst, 1e-9 * scale);  // the same broadband signal, later: not a phase shift
}

// --- boundaries -------------------------------------------------------------------------

namespace {

// Incident and reflected plane pulse at one probe in a duct whose XMax end
// carries the boundary under test. Returns reflected / incident (signed).
double ductReflection(BoundaryKind boundary, double impedance = 0.0, int pmlCells = 12)
{
    Problem problem;
    const double f = 4000.0;  // 85 points per wavelength: the pulse keeps its shape over the path
    problem.grid = duct(600, 1e-3, 0.7, BoundaryKind::Absorbing, boundary);
    problem.grid.impedance[XMax] = impedance;
    problem.grid.pmlCells = pmlCells;
    addPlaneSource(problem, 100, ricker(f));
    problem.probes = {{Id("p"), "p", Vec3::Zero(), 300, 0, 0}};
    // The incident pulse is centred on the probe at 0.2 m / c + 1.5 / f =
    // 0.96 ms, the reflection at 0.8 m / c + 1.5 / f = 2.73 ms.
    problem.steps = std::lround(3.6e-3 / problem.grid.dt);
    const RunOutput out = mustRun(problem);
    const auto split = static_cast<size_t>(1.8e-3 / out.dt);
    const double incident = signedPeak(out.probes[0].pressure, 0, split);
    const double reflected = signedPeak(out.probes[0].pressure, split, SIZE_MAX);
    return reflected / incident;
}

}  // namespace

TEST(FdtdBoundaries, RigidWallReflectsInPhaseAndPressureReleaseInverts)
{
    EXPECT_NEAR(ductReflection(BoundaryKind::Rigid), 1.0, 0.01);
    EXPECT_NEAR(ductReflection(BoundaryKind::PressureRelease), -1.0, 0.01);
}

TEST(FdtdBoundaries, ImpedanceWallReflectsWithTheNormalIncidenceCoefficient)
{
    const double z0 = kRho * kC;
    // R = (Z - rho c) / (Z + rho c)
    EXPECT_NEAR(ductReflection(BoundaryKind::Impedance, 3.0 * z0), 0.5, 0.01);
    EXPECT_NEAR(ductReflection(BoundaryKind::Impedance, z0 / 3.0), -0.5, 0.01);
    EXPECT_NEAR(ductReflection(BoundaryKind::Impedance, z0), 0.0, 0.01);
    EXPECT_NEAR(ductReflection(BoundaryKind::Impedance, 1e9 * z0), 1.0, 0.01);
}

TEST(FdtdBoundaries, AbsorbingLayerReflectsLittleAndThickerLayersReflectLess)
{
    const double r8 = std::abs(ductReflection(BoundaryKind::Absorbing, 0.0, 8));
    const double r12 = std::abs(ductReflection(BoundaryKind::Absorbing, 0.0, 12));
    const double r24 = std::abs(ductReflection(BoundaryKind::Absorbing, 0.0, 24));
    EXPECT_LT(r12, 0.002);  // below -54 dB at normal incidence
    EXPECT_LT(r24, r12);
    EXPECT_LT(r12, r8);
    EXPECT_LT(r24, 0.0005);
}

TEST(FdtdBoundaries, AbsorbingLayersBehaveLikeOpenSpaceAtObliqueIncidence)
{
    // A small domain with absorbing layers against a domain so large that
    // nothing returns from its walls during the run.
    const double dx = 1e-3, f = 20000.0;
    const auto runIn = [&](int n, int offset, BoundaryKind boundary) {
        Problem problem;
        problem.grid = grid2D(n, n, dx, 0.9, boundary);
        problem.sources.push_back({offset + 12, offset + 20, 0, 1.0, 0.0, ricker(f)});
        // Probes close to the walls and corner of the small domain.
        problem.probes = {{Id("edge"), "edge", Vec3::Zero(), offset + 57, offset + 30, 0},
                          {Id("corner"), "corner", Vec3::Zero(), offset + 56, offset + 56, 0},
                          {Id("middle"), "middle", Vec3::Zero(), offset + 30, offset + 30, 0}};
        problem.steps = 400;  // 255 cells of travel: several crossings of the 60-cell domain
        return mustRun(problem);
    };
    const RunOutput small = runIn(60, 0, BoundaryKind::Absorbing);
    const RunOutput large = runIn(400, 170, BoundaryKind::Rigid);
    for (size_t q = 0; q < 3; ++q) {
        const double scale = peakAbs(large.probes[q].pressure);
        double worst = 0.0;
        for (size_t i = 0; i < small.probes[q].pressure.size(); ++i)
            worst = std::max(worst, std::abs(small.probes[q].pressure[i] - large.probes[q].pressure[i]));
        EXPECT_LT(worst, 0.01 * scale) << small.probes[q].label;
    }
    // The same small domain with rigid walls is nothing like open space.
    const RunOutput closed = runIn(60, 0, BoundaryKind::Rigid);
    double difference = 0.0;
    for (size_t i = 0; i < closed.probes[2].pressure.size(); ++i)
        difference = std::max(difference, std::abs(closed.probes[2].pressure[i] - large.probes[2].pressure[i]));
    EXPECT_GT(difference, 0.5 * peakAbs(large.probes[2].pressure));
    // And all the energy has left the absorbing domain.
    EXPECT_LT(small.energy.back(), 1e-6 * *std::max_element(small.energy.begin(), small.energy.end()));
}

TEST(FdtdBoundaries, RigidWallActsAsAnImageSource)
{
    const double dx = 1e-3, f = kC / (14.0 * dx);
    const double k = 2.0 * pi * f / kC;
    Problem problem;
    problem.grid = grid2D(120, 160, dx, 0.95, BoundaryKind::Absorbing);
    for (const BoundaryKind wall : {BoundaryKind::Rigid, BoundaryKind::PressureRelease}) {
        problem.grid.boundary[XMin] = wall;
        problem.sources = {{24, 80, 0, 1.0, 0.0, continuous(f)}};  // 24.5 cells from the wall (cell centres)
        problem.steps = std::lround(36.0 / f / problem.grid.dt);
        problem.recording.harmonicFrequencyHz = f;
        const RunOutput out = mustRun(problem);
        const double sign = wall == BoundaryKind::Rigid ? 1.0 : -1.0;
        double worst = 0.0, scale = 0.0;
        for (int i = 40; i < 115; i += 5)
            for (int j = 30; j < 131; j += 25) {
                const double x = (i + 0.5) * dx, y = (j - 80) * dx;
                const double direct = std::hypot(x - 24.5 * dx, y), image = std::hypot(x + 24.5 * dx, y);
                const std::complex<double> expected = 0.25 * kRho * (hankel2(k * direct) + sign * hankel2(k * image));
                worst = std::max(worst, std::abs(std::abs(out.harmonic[problem.grid.interiorIndex(i, j, 0)]) - std::abs(expected)));
                scale = std::max(scale, std::abs(expected));
            }
        EXPECT_LT(worst, 0.05 * scale) << toString(wall);
    }
}

// --- materials ----------------------------------------------------------------------------

TEST(FdtdMaterials, InterfaceReflectsAndTransmitsWithTheImpedanceCoefficients)
{
    struct Case {
        double density, speed;
    };
    // A denser and faster medium, a lighter and slower one, and one with the
    // same impedance but another speed (no reflection).
    for (const Case& second : {Case{4.8, 510.0}, Case{0.6, 250.0}, Case{0.8, 510.0}}) {
        Problem problem;
        const double f = 4000.0;
        GridSpec& g = problem.grid;
        g = duct(900, 1e-3, 1.0, BoundaryKind::Absorbing, BoundaryKind::Absorbing);
        g.materials = {gas(), CellMaterial{"second", second.density, second.speed, 0.0, false, false}};
        g.cellMaterial.assign(g.interiorCells(), 0);
        for (int i = 450; i < 900; ++i)
            for (int j = 0; j < 2; ++j)
                g.cellMaterial[g.interiorIndex(i, j, 0)] = 1;
        g.dt = 0.7 * g.dx / std::max(kC, second.speed);
        addPlaneSource(problem, 100, ricker(f));
        problem.probes = {{Id("in"), "in", Vec3::Zero(), 300, 0, 0}, {Id("out"), "out", Vec3::Zero(), 600, 0, 0}};
        problem.steps = std::lround(2.9e-3 / g.dt);
        const RunOutput out = mustRun(problem);
        // The incident pulse has passed the first probe by 0.2 / 340 + 0.75 ms
        // = 1.34 ms; the reflection starts there at 0.5 / 340 = 1.47 ms.
        const auto split = static_cast<size_t>(1.4e-3 / out.dt);
        const double incident = signedPeak(out.probes[0].pressure, 0, split);
        const double reflected = signedPeak(out.probes[0].pressure, split, SIZE_MAX);
        const double transmitted = signedPeak(out.probes[1].pressure, 0, SIZE_MAX);
        const double z1 = kRho * kC, z2 = second.density * second.speed;
        EXPECT_NEAR(reflected / incident, (z2 - z1) / (z2 + z1), 0.012) << second.density;
        EXPECT_NEAR(transmitted / incident, 2.0 * z2 / (z2 + z1), 0.012) << second.density;
        // Energy flux is conserved: R^2 + (Z1 / Z2) T^2 = 1.
        const double r = reflected / incident, t = transmitted / incident;
        EXPECT_NEAR(r * r + z1 / z2 * t * t, 1.0, 0.02);
    }
}

TEST(FdtdMaterials, WaveInTheSecondMediumTravelsAtItsSpeed)
{
    Problem problem;
    GridSpec& g = problem.grid;
    g = duct(900, 1e-3, 1.0, BoundaryKind::Absorbing, BoundaryKind::Absorbing);
    g.materials = {gas(), CellMaterial{"fast", 2.4, 680.0, 0.0, false, false}};
    g.cellMaterial.assign(g.interiorCells(), 0);
    for (int i = 300; i < 900; ++i)
        for (int j = 0; j < 2; ++j)
            g.cellMaterial[g.interiorIndex(i, j, 0)] = 1;
    g.dt = 0.7 * g.dx / 680.0;
    addPlaneSource(problem, 100, ricker(4000.0));
    problem.probes = {{Id("a"), "a", Vec3::Zero(), 400, 0, 0}, {Id("b"), "b", Vec3::Zero(), 800, 0, 0}};
    problem.steps = std::lround(2.3e-3 / g.dt);
    const RunOutput out = mustRun(problem);
    const auto arrival = [&](const std::vector<double>& x) {
        size_t best = 0;
        for (size_t i = 0; i < x.size(); ++i)
            if (std::abs(x[i]) > std::abs(x[best]))
                best = i;
        return static_cast<double>(best) * out.dt;
    };
    EXPECT_NEAR(0.4 / (arrival(out.probes[1].pressure) - arrival(out.probes[0].pressure)), 680.0, 0.005 * 680.0);
}

TEST(FdtdMaterials, RefractionAtAnObliqueInterfaceFollowsSnellsLaw)
{
    // A point source above a horizontal interface; in the faster lower
    // medium the transmitted wavefront must arrive when Fermat's principle
    // (the least-time refracted path, i.e. Snell's law) says.
    const double dx = 1e-3, f = 15000.0, c2 = 680.0;
    Problem problem;
    GridSpec& g = problem.grid;
    g = grid2D(300, 200, dx, 1.0, BoundaryKind::Absorbing);
    g.materials = {gas(), CellMaterial{"fast", 1.2, c2, 0.0, false, false}};
    g.cellMaterial.assign(g.interiorCells(), 0);
    for (int j = 0; j < 100; ++j)  // lower half: the interface is the plane y = 100 dx
        for (int i = 0; i < 300; ++i)
            g.cellMaterial[g.interiorIndex(i, j, 0)] = 1;
    g.dt = 0.9 * stabilityLimit(dx, c2, 2);
    const int sx = 60, sy = 160;  // source 60.5 cells above the interface
    problem.sources.push_back({sx, sy, 0, 1.0, 0.0, ricker(f)});
    const int px = 240, py = 30;  // probe 69.5 cells below, 180 cells along
    problem.probes = {{Id("p"), "p", Vec3::Zero(), px, py, 0}};
    problem.steps = std::lround(1.0e-3 / g.dt);
    const RunOutput out = mustRun(problem);

    // Least travel time over the crossing point x of the interface.
    const double h1 = 60.5 * dx, h2 = 69.5 * dx, span = 180.0 * dx;
    double best = 1e300, bestX = 0.0;
    for (double x = 0.0; x <= span; x += 1e-6) {
        const double t = std::hypot(x, h1) / kC + std::hypot(span - x, h2) / c2;
        if (t < best) {
            best = t;
            bestX = x;
        }
    }
    // Snell: sin(theta1) / c1 = sin(theta2) / c2 at that point.
    EXPECT_NEAR(bestX / std::hypot(bestX, h1) / kC, (span - bestX) / std::hypot(span - bestX, h2) / c2, 1e-4);
    // The straight path would take longer; the arrival matches the refracted one.
    const double straight = std::hypot(span, h1 + h2) * (h1 / (h1 + h2) / kC + h2 / (h1 + h2) / c2);
    EXPECT_GT(straight, best * 1.05);
    // First arrival: when the signal first exceeds 5 % of its peak.
    const auto& x = out.probes[0].pressure;
    const double threshold = 0.05 * peakAbs(x);
    size_t first = 0;
    while (first < x.size() && std::abs(x[first]) < threshold)
        ++first;
    // A Ricker wavelet reaches 5 % of its peak about 0.95 / f after its start
    // (its main lobe is centred 1.5 / f after the start).
    const double onset = static_cast<double>(first) * out.dt;
    EXPECT_NEAR(onset, best + 0.95 / f, 0.25 / f);
    EXPECT_LT(onset, straight + 0.95 / f - 0.3 / f);
}

TEST(FdtdMaterials, AttenuationDecaysThePlaneWaveAsStated)
{
    const double alpha = 4.0;  // Np/m
    Problem problem;
    GridSpec& g = problem.grid;
    g = duct(700, 1e-3, 0.7, BoundaryKind::Absorbing, BoundaryKind::Absorbing);
    g.materials[0].attenuation = alpha;
    addPlaneSource(problem, 50, ricker(4000.0));
    problem.probes = {{Id("a"), "a", Vec3::Zero(), 150, 0, 0}, {Id("b"), "b", Vec3::Zero(), 650, 0, 0}};
    problem.steps = std::lround(3.0e-3 / g.dt);
    const RunOutput out = mustRun(problem);
    EXPECT_NEAR(peakAbs(out.probes[1].pressure) / peakAbs(out.probes[0].pressure), std::exp(-alpha * 0.5), 0.01);
    // Energy only decreases once the source has ended.
    for (size_t i = 1; i < out.energy.size(); ++i)
        if (out.energyTimes[i - 1] > 8e-4) {
            EXPECT_LE(out.energy[i], out.energy[i - 1] * (1.0 + 1e-3));
        }
}

TEST(FdtdMaterials, RigidObstacleCastsAShadowAndSoundDiffractsThroughASlit)
{
    const double dx = 1e-3, f = kC / (12.0 * dx);  // wavelength 12 mm
    const auto runWith = [&](bool barrier, int slitHalfWidth) {
        Problem problem;
        GridSpec& g = problem.grid;
        g = grid2D(200, 200, dx, 0.95, BoundaryKind::Absorbing);
        g.materials = {gas(), CellMaterial{"rigid", 0.0, 0.0, 0.0, true, false}};
        g.cellMaterial.assign(g.interiorCells(), 0);
        if (barrier)
            for (int i = 98; i < 102; ++i)  // a wall 4 cells thick across the domain
                for (int j = 0; j < 200; ++j)
                    if (std::abs(j - 100) >= slitHalfWidth)
                        g.cellMaterial[g.interiorIndex(i, j, 0)] = 1;
        problem.sources.push_back({40, 100, 0, 1.0, 0.0, continuous(f)});
        problem.steps = std::lround(40.0 / f / g.dt);
        problem.recording.harmonicFrequencyHz = f;
        return std::pair{mustRun(problem), g};
    };
    const auto [open, g] = runWith(false, 0);
    const auto [closed, g1] = runWith(true, 0);
    const auto [slit, g2] = runWith(true, 6);  // a slit one wavelength wide
    const auto at = [&](const RunOutput& out, int i, int j) { return std::abs(out.harmonic[g.interiorIndex(i, j, 0)]); };
    // Behind a complete rigid wall there is nothing.
    EXPECT_LT(at(closed, 160, 100), 1e-6 * at(open, 160, 100));
    // Through the slit sound reaches the far side - also far off the axis,
    // where a ray picture has none: the slit radiates like a new source.
    EXPECT_GT(at(slit, 160, 100), 0.1 * at(open, 160, 100));
    EXPECT_GT(at(slit, 130, 160), 0.1 * at(slit, 160, 100));  // 63 degrees off the axis
    // A narrow slit spreads almost evenly: on a circle around it the field
    // varies slowly with angle (no sharp beam).
    const double ahead = at(slit, 160, 100);
    const double oblique = at(slit, 142, 142);  // same radius (60 cells), 45 degrees
    EXPECT_GT(oblique, 0.35 * ahead);
    EXPECT_LT(oblique, 1.0 * ahead);
    // In front of the wall the reflection builds a standing wave: maxima
    // well above the free-field amplitude there.
    double maximum = 0.0;
    for (int i = 70; i < 96; ++i)
        maximum = std::max(maximum, at(closed, i, 100) / at(open, i, 100));
    EXPECT_GT(maximum, 1.6);
}

TEST(FdtdMaterials, PressureReleaseBodyHoldsThePressureAtZero)
{
    Problem problem;
    GridSpec& g = problem.grid;
    g = duct(600, 1e-3, 0.7, BoundaryKind::Absorbing, BoundaryKind::Absorbing);
    g.materials = {gas(), CellMaterial{"soft", 0.0, 0.0, 0.0, false, true}};
    g.cellMaterial.assign(g.interiorCells(), 0);
    for (int i = 450; i < 470; ++i)
        for (int j = 0; j < 2; ++j)
            g.cellMaterial[g.interiorIndex(i, j, 0)] = 1;
    addPlaneSource(problem, 100, ricker(4000.0));
    problem.probes = {{Id("in"), "in", Vec3::Zero(), 300, 0, 0}, {Id("behind"), "behind", Vec3::Zero(), 520, 0, 0}};
    problem.steps = std::lround(2.8e-3 / g.dt);
    const RunOutput out = mustRun(problem);
    const auto split = static_cast<size_t>(1.4e-3 / out.dt);
    EXPECT_NEAR(signedPeak(out.probes[0].pressure, split, SIZE_MAX) / signedPeak(out.probes[0].pressure, 0, split), -1.0, 0.02);
    EXPECT_LT(peakAbs(out.probes[1].pressure), 1e-9 * peakAbs(out.probes[0].pressure));
}

// --- resonance ------------------------------------------------------------------------------

TEST(FdtdResonance, RigidBoxRingsAtItsAnalyticalModeFrequencies)
{
    // 2D rigid box 60 x 45 mm. f_mn = (c / 2) sqrt((m / Lx)^2 + (n / Ly)^2).
    const double dx = 1e-3, lx = 0.060, ly = 0.045;
    Problem problem;
    problem.grid = grid2D(60, 45, dx, 0.95, BoundaryKind::Rigid);
    // Source and probe away from the symmetry lines so the low modes are excited and seen.
    problem.sources.push_back({7, 5, 0, 1.0, 0.0, ricker(6000.0)});
    problem.probes = {{Id("p"), "p", Vec3::Zero(), 51, 38, 0}};
    const double duration = 0.05;  // rectangular-window resolution 20 Hz
    problem.steps = std::lround(duration / problem.grid.dt);
    const RunOutput out = mustRun(problem);
    const Spectrum spectrum = computeSpectrum(out.probes[0].pressure, out.dt, Window::Hann, 4);
    EXPECT_NEAR(spectrum.resolutionHz, 40.0, 0.5);
    const std::vector<SpectralPeak> peaks = findPeaks(spectrum, 0.02, 1000.0, 8000.0);
    const auto mode = [&](int m, int n) { return 0.5 * kC * std::hypot(m / lx, n / ly); };
    for (const auto& [m, n] : {std::pair{1, 0}, {0, 1}, {1, 1}, {2, 0}, {2, 1}, {0, 2}}) {
        const double expected = mode(m, n);
        double nearest = 1e300;
        for (const SpectralPeak& p : peaks)
            if (std::abs(p.frequencyHz - expected) < std::abs(nearest - expected))
                nearest = p.frequencyHz;
        // Numerical dispersion lowers each frequency slightly (the grid has
        // 60 or more points per wavelength here: below 0.1 %).
        EXPECT_NEAR(nearest, expected, 0.5 * spectrum.resolutionHz) << m << "," << n;
    }
    EXPECT_NEAR(mode(1, 0), 2833.3, 0.1);
}

TEST(FdtdRun, RecordsSnapshotsProbesAndReportsBadInput)
{
    Problem problem;
    problem.grid = grid2D(40, 30, 1e-3, 0.9, BoundaryKind::Absorbing);
    problem.sources.push_back({20, 15, 0, 1.0, 0.0, ricker(20000.0)});
    problem.probes = {{Id("p"), "probe", Vec3(1, 2, 3), 30, 15, 0}};
    problem.steps = 200;
    problem.recording.snapshotCount = 10;
    const RunOutput out = mustRun(problem);
    EXPECT_EQ(out.snapshots.size(), 10u);
    EXPECT_EQ(out.snapshots[0].size(), 1200u);
    EXPECT_DOUBLE_EQ(out.snapshotTimes.back(), 200 * problem.grid.dt);
    EXPECT_EQ(out.probes[0].pressure.size(), 200u);
    EXPECT_EQ(out.probes[0].position, Vec3(1, 2, 3));
    EXPECT_TRUE(out.harmonic.empty());
    EXPECT_GT(out.peakPressure, 0.0);
    EXPECT_GT(out.cellUpdatesPerSecond, 0.0);
    // The last snapshot is the field at the end.
    EXPECT_FLOAT_EQ(out.snapshots.back()[problem.grid.interiorIndex(30, 15, 0)], static_cast<float>(out.probes[0].pressure.back()));

    RunOutput ignored;
    Problem bad = problem;
    bad.sources[0].i = 99;
    EXPECT_EQ(run(bad, ignored).kind, RunEnd::Kind::Invalid);
    bad = problem;
    bad.probes[0].j = -1;
    EXPECT_EQ(run(bad, ignored).kind, RunEnd::Kind::Invalid);
    bad = problem;
    bad.steps = 0;
    EXPECT_EQ(run(bad, ignored).kind, RunEnd::Kind::Invalid);
    bad = problem;
    bad.grid.dt *= 2.0;
    const RunEnd end = run(bad, ignored);
    EXPECT_EQ(end.kind, RunEnd::Kind::Invalid);
    EXPECT_NE(end.message.find("stability limit"), std::string::npos);

    // Cancellation.
    struct Cancelling final : JobContext {
        bool cancelRequested() const override { return calls > 3; }
        void reportProgress(double, std::string_view) override { ++calls; }
        int calls = 0;
    } context;
    problem.steps = 100000;
    EXPECT_EQ(run(problem, ignored, &context).kind, RunEnd::Kind::Cancelled);
    EXPECT_LT(ignored.steps, 100000);
}
