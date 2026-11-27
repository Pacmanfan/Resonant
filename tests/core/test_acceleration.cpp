// Accelerated solver paths against the CPU reference.
//
// Threads: every cell is computed exactly as by one thread, so the threaded
// solvers must give bit-identical fields, probes, snapshots and steady-state
// results, and (with the per-row reduction order) identical energies.
//
// CUDA: the kernels do the same double-precision arithmetic without fused
// multiply-add, so they are compared with the CPU reference to a few units
// in the last place. Without a usable CUDA device these cases are skipped
// and say why (they are not counted as validation).
#include "compute/Compute.h"
#include "compute/VectorKernels.h"
#include "electromagnetics/Yee.h"
#include "fdtd/FdtdRun.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>

using namespace resonant;

namespace {

// --- problems ------------------------------------------------------------------------------

struct YeeCase {
    em::YeeSpec spec;
    std::vector<em::YeeSource> sources;
    std::vector<em::YeeProbe> probes;
    long steps = 0;
    em::YeeRecording recording;
};

dsp::Waveform tone(double f)
{
    dsp::Waveform w;
    w.kind = dsp::WaveformKind::Continuous;
    w.frequencyHz = f;
    return w;
}

// 3D: absorbing faces on four sides and periodic along z, a dielectric and a
// lossy block, a conducting block, thin wires, two sources (one at a node a
// probe also reads), snapshots and a steady-state window.
YeeCase yee3D(int n = 24)
{
    YeeCase c;
    em::YeeSpec& s = c.spec;
    s.nx = n;
    s.ny = n - 4;
    s.nz = n / 2;
    s.cell = 0.01;
    s.pmlCells = 6;
    s.boundary = {em::YeeBoundary::Absorbing, em::YeeBoundary::Absorbing, em::YeeBoundary::Absorbing, em::YeeBoundary::Absorbing,
                  em::YeeBoundary::Periodic, em::YeeBoundary::Periodic};
    s.materials.push_back(em::YeeMaterial{"dielectric", 4.0, 1.0, 0.0, false});
    s.materials.push_back(em::YeeMaterial{"lossy", 2.0, 1.5, 0.05, false});
    s.materials.push_back(em::YeeMaterial{"pec", 1.0, 1.0, 0.0, true});
    s.cellMaterial.assign(static_cast<size_t>(s.nx) * s.ny * s.nz, 0);
    for (int k = 0; k < s.nz; ++k)
        for (int j = 0; j < s.ny; ++j)
            for (int i = 0; i < s.nx; ++i) {
                std::uint8_t m = 0;
                if (i > n / 2 && i < n / 2 + 5 && j > 3 && j < 9)
                    m = 1;
                else if (i > 3 && i < 7 && j > n / 2)
                    m = 2;
                else if (i == n - 5 && j > 2 && j < 6 && k > 1 && k < 4)
                    m = 3;
                s.cellMaterial[s.cellIndex(i, j, k)] = m;
            }
    s.pecEdges = {{2, 5, 5, 2}, {2, 5, 5, 3}, {0, 8, 10, 4}};
    s.dt = 0.9 * em::yeeStabilityLimit(s.cell, em::kSpeedOfLight, 3);
    const double f = em::kSpeedOfLight / (10.0 * s.cell);
    c.sources = {{n / 3, n / 3, s.nz / 2, 2, 1.0, 0.0, tone(f)}, {n / 2, 6, 2, 0, 0.5, 1e-10, tone(f)}};
    c.probes = {{"a", n / 3, n / 3, s.nz / 2, 2}, {"b", n - 6, n / 2, 1, 1}, {"c", 2, 2, 0, 0}};
    c.steps = 120;
    c.recording.snapshotCount = 3;
    c.recording.snapshotComponent = 2;
    c.recording.harmonicFrequencyHz = f;
    c.recording.harmonicPeriods = 2.0;
    c.recording.energySamples = 30;
    return c;
}

// 2D (TM): absorbing on x, PEC on y, a dielectric cylinder.
YeeCase yee2D()
{
    YeeCase c;
    em::YeeSpec& s = c.spec;
    s.nx = 70;
    s.ny = 50;
    s.nz = 1;
    s.cell = 0.005;
    s.boundary = {em::YeeBoundary::Absorbing, em::YeeBoundary::Absorbing, em::YeeBoundary::PEC, em::YeeBoundary::PEC,
                  em::YeeBoundary::Absorbing, em::YeeBoundary::Absorbing};
    s.materials.push_back(em::YeeMaterial{"dielectric", 6.0, 1.0, 0.01, false});
    s.cellMaterial.assign(static_cast<size_t>(s.nx) * s.ny, 0);
    for (int j = 0; j < s.ny; ++j)
        for (int i = 0; i < s.nx; ++i)
            if (std::hypot(i - 45, j - 25) < 8)
                s.cellMaterial[s.cellIndex(i, j, 0)] = 1;
    s.dt = 0.95 * em::yeeStabilityLimit(s.cell, em::kSpeedOfLight, 2);
    const double f = em::kSpeedOfLight / (20.0 * s.cell);
    c.sources = {{15, 25, 0, 2, 1.0, 0.0, tone(f)}};
    c.probes = {{"p", 60, 20, 0, 2}};
    c.steps = 400;
    c.recording.snapshotCount = 2;
    c.recording.harmonicFrequencyHz = f;
    c.recording.harmonicPeriods = 3.0;
    return c;
}

em::YeeOutput runYee(YeeCase c, compute::ComputeOptions options)
{
    c.spec.compute = options;
    em::YeeOutput out;
    const em::YeeRunEnd end = em::runYee(c.spec, c.sources, c.probes, c.steps, c.recording, out);
    EXPECT_EQ(end.kind, em::YeeRunEnd::Kind::Completed) << end.message;
    return out;
}

struct FdtdCase {
    fdtd::Problem problem;
};

fdtd::CellMaterial fluid(const char* name, double density, double speed, double attenuation = 0.0)
{
    fdtd::CellMaterial m;
    m.name = name;
    m.density = density;
    m.soundSpeed = speed;
    m.attenuation = attenuation;
    return m;
}

// Every boundary kind, a second fluid, a rigid body, a pressure-release body
// and an attenuating fluid; 2D or 3D.
fdtd::Problem acoustic(bool threeD)
{
    fdtd::Problem p;
    fdtd::GridSpec& g = p.grid;
    g.nx = threeD ? 26 : 80;
    g.ny = threeD ? 22 : 60;
    g.nz = threeD ? 18 : 1;
    g.dx = 1e-3;
    g.pmlCells = 6;
    g.boundary = {fdtd::BoundaryKind::Absorbing, fdtd::BoundaryKind::Impedance, fdtd::BoundaryKind::Rigid, fdtd::BoundaryKind::PressureRelease,
                  fdtd::BoundaryKind::Absorbing, fdtd::BoundaryKind::Rigid};
    g.impedance[fdtd::XMax] = 800.0;
    g.materials = {fluid("air", 1.2, 343.0), fluid("water", 1000.0, 1480.0, 0.5), fluid("gel", 1100.0, 1600.0, 2.0)};
    fdtd::CellMaterial rigid = fluid("steel", 7800.0, 5900.0);
    rigid.rigid = true;
    fdtd::CellMaterial release = fluid("vacuum", 1.0, 1.0);
    release.pressureRelease = true;
    g.materials.push_back(rigid);
    g.materials.push_back(release);
    g.cellMaterial.assign(g.interiorCells(), 0);
    for (int k = 0; k < g.nz; ++k)
        for (int j = 0; j < g.ny; ++j)
            for (int i = 0; i < g.nx; ++i) {
                std::uint8_t m = 0;
                if (i > g.nx / 2)
                    m = 1;
                if (i > g.nx / 2 + 4 && j < 6)
                    m = 2;
                if (i > 3 && i < 8 && j > g.ny / 2)
                    m = 3;
                if (i > 10 && i < 13 && j < 4)
                    m = 4;
                g.cellMaterial[g.interiorIndex(i, j, k)] = m;
            }
    g.dt = 0.9 * fdtd::stabilityLimit(g.dx, g.maxSoundSpeed(), g.dimensions());
    dsp::Waveform burst;
    burst.kind = dsp::WaveformKind::ToneBurst;
    burst.frequencyHz = 40000.0;
    burst.cycles = 3;
    p.sources = {{g.nx / 4, g.ny / 2, g.nz / 2, 1e-3, 0.0, burst}, {g.nx / 4, g.ny / 2, g.nz / 2, 5e-4, 2e-6, tone(40000.0)}};
    p.probes = {{Id("a"), "a", Vec3::Zero(), g.nx / 4, g.ny / 2, g.nz / 2}, {Id("b"), "b", Vec3::Zero(), g.nx - 3, 2, 0}};
    p.steps = threeD ? 150 : 600;
    p.recording.snapshotCount = 3;
    p.recording.harmonicFrequencyHz = 40000.0;
    p.recording.harmonicPeriods = 2.0;
    p.recording.energySamples = 40;
    return p;
}

fdtd::RunOutput runAcoustic(fdtd::Problem p, compute::ComputeOptions options)
{
    p.grid.compute = options;
    fdtd::RunOutput out;
    const fdtd::RunEnd end = fdtd::run(p, out);
    EXPECT_EQ(end.kind, fdtd::RunEnd::Kind::Completed) << end.message;
    return out;
}

compute::ComputeOptions cpu(int threads)
{
    compute::ComputeOptions o;
    o.threads = threads;
    return o;
}

compute::ComputeOptions cuda()
{
    compute::ComputeOptions o;
    o.backend = compute::ComputeBackend::Cuda;
    return o;
}

// --- comparisons ---------------------------------------------------------------------------

// Largest difference relative to the largest magnitude of the reference.
template <typename T>
double relativeDifference(const std::vector<T>& a, const std::vector<T>& b)
{
    EXPECT_EQ(a.size(), b.size());
    double scale = 0.0, difference = 0.0;
    for (size_t i = 0; i < std::min(a.size(), b.size()); ++i) {
        scale = std::max(scale, static_cast<double>(std::abs(b[i])));
        difference = std::max(difference, static_cast<double>(std::abs(a[i] - b[i])));
    }
    return scale > 0.0 ? difference / scale : difference;
}

void expectSameYee(const em::YeeOutput& a, const em::YeeOutput& b, double tolerance)
{
    ASSERT_EQ(a.probes.size(), b.probes.size());
    for (size_t q = 0; q < a.probes.size(); ++q)
        EXPECT_LE(relativeDifference(a.probes[q], b.probes[q]), tolerance) << "probe " << q;
    ASSERT_EQ(a.snapshots.size(), b.snapshots.size());
    for (size_t s = 0; s < a.snapshots.size(); ++s)
        EXPECT_LE(relativeDifference(a.snapshots[s], b.snapshots[s]), tolerance == 0.0 ? 0.0 : 1e-6) << "snapshot " << s;
    for (int c = 0; c < 3; ++c)
        EXPECT_LE(relativeDifference(a.harmonic[static_cast<size_t>(c)], b.harmonic[static_cast<size_t>(c)]), tolerance) << "harmonic " << c;
    EXPECT_LE(relativeDifference(a.energy, b.energy), tolerance);
    EXPECT_EQ(a.steps, b.steps);
}

void expectSameAcoustic(const fdtd::RunOutput& a, const fdtd::RunOutput& b, double tolerance)
{
    ASSERT_EQ(a.probes.size(), b.probes.size());
    for (size_t q = 0; q < a.probes.size(); ++q)
        EXPECT_LE(relativeDifference(a.probes[q].pressure, b.probes[q].pressure), tolerance) << "probe " << q;
    ASSERT_EQ(a.snapshots.size(), b.snapshots.size());
    for (size_t s = 0; s < a.snapshots.size(); ++s)
        EXPECT_LE(relativeDifference(a.snapshots[s], b.snapshots[s]), tolerance == 0.0 ? 0.0 : 1e-6) << "snapshot " << s;
    EXPECT_LE(relativeDifference(a.harmonic, b.harmonic), tolerance);
    EXPECT_LE(relativeDifference(a.energy, b.energy), tolerance);
    if (tolerance == 0.0)
        EXPECT_EQ(a.peakPressure, b.peakPressure);
    else
        EXPECT_NEAR(a.peakPressure, b.peakPressure, tolerance * b.peakPressure);
}

// Why the CUDA cases cannot run here, or empty when they can.
std::string cudaUnavailable(size_t bytes)
{
    const Status status = compute::checkBackend(cuda(), bytes);
    return status.ok() ? std::string() : status.message();
}

}  // namespace

TEST(AcceleratedYee, ThreadsGiveBitIdenticalResultsIn3DAnd2D)
{
    for (const YeeCase& c : {yee3D(), yee2D()}) {
        const em::YeeOutput one = runYee(c, cpu(1));
        EXPECT_EQ(one.backend, "CPU, double precision, 1 thread");
        for (const int threads : {2, 3, 7}) {
            const em::YeeOutput many = runYee(c, cpu(threads));
            expectSameYee(many, one, 0.0);
            EXPECT_NE(many.backend.find(std::to_string(std::min(threads, compute::hardwareThreads())) + " thread"), std::string::npos) << many.backend;
        }
    }
}

TEST(AcceleratedAcoustics, ThreadsGiveBitIdenticalResultsIn2DAnd3D)
{
    for (const bool threeD : {false, true}) {
        const fdtd::Problem p = acoustic(threeD);
        const fdtd::RunOutput one = runAcoustic(p, cpu(1));
        EXPECT_GT(one.peakPressure, 0.0);
        for (const int threads : {2, 5})
            expectSameAcoustic(runAcoustic(p, cpu(threads)), one, 0.0);
    }
}

TEST(AcceleratedBackends, CudaIsRefusedWithTheReasonWhenItCannotRun)
{
    const YeeCase c = yee2D();
    const std::string why = cudaUnavailable(c.spec.stateBytes());
    if (!why.empty()) {
        em::YeeSpec s = c.spec;
        s.compute = cuda();
        em::YeeOutput out;
        const em::YeeRunEnd end = em::runYee(s, c.sources, c.probes, c.steps, c.recording, out);
        EXPECT_EQ(end.kind, em::YeeRunEnd::Kind::Invalid);
        EXPECT_EQ(end.message, why);  // never a silent fall-back to the CPU
        fdtd::Problem p = acoustic(false);
        p.grid.compute = cuda();
        fdtd::RunOutput acousticOut;
        EXPECT_EQ(fdtd::run(p, acousticOut).kind, fdtd::RunEnd::Kind::Invalid);
    }
}

TEST(AcceleratedYee, CudaMatchesTheCpuReference)
{
    const YeeCase c3 = yee3D();
    if (const std::string why = cudaUnavailable(c3.spec.stateBytes()); !why.empty())
        GTEST_SKIP() << "CUDA validation not run: " << why;
    for (const YeeCase& c : {c3, yee2D()}) {
        const em::YeeOutput reference = runYee(c, cpu(0));
        const em::YeeOutput gpu = runYee(c, cuda());
        EXPECT_NE(gpu.backend.find("CUDA on "), std::string::npos) << gpu.backend;
        expectSameYee(gpu, reference, 1e-12);
    }
}

TEST(AcceleratedAcoustics, CudaMatchesTheCpuReference)
{
    const fdtd::Problem p3 = acoustic(true);
    if (const std::string why = cudaUnavailable(p3.grid.stateBytes()); !why.empty())
        GTEST_SKIP() << "CUDA validation not run: " << why;
    for (const bool threeD : {false, true}) {
        const fdtd::Problem p = acoustic(threeD);
        const fdtd::RunOutput reference = runAcoustic(p, cpu(0));
        const fdtd::RunOutput gpu = runAcoustic(p, cuda());
        EXPECT_NE(gpu.backend.find("CUDA on "), std::string::npos) << gpu.backend;
        expectSameAcoustic(gpu, reference, 1e-12);
    }
}

TEST(AcceleratedYee, PeriodicAxisLastNodeIsTheFirstNode)
{
    // Regression: along a periodic axis node n is node 0; the steady-state
    // field there was read past the end of the arrays.
    const YeeCase c = yee3D(16);
    const em::YeeOutput out = runYee(c, cpu(0));
    const em::YeeSpec& s = c.spec;
    const auto node = [&](int i, int j, int k) { return static_cast<size_t>(i) + static_cast<size_t>(s.nx + 1) * (static_cast<size_t>(j) + static_cast<size_t>(s.ny + 1) * k); };
    double largest = 0.0;
    for (int comp = 0; comp < 3; ++comp)
        for (int j = 0; j <= s.ny; ++j)
            for (int i = 0; i <= s.nx; ++i) {
                const auto& h = out.harmonic[static_cast<size_t>(comp)];
                ASSERT_EQ(h[node(i, j, s.nz)], h[node(i, j, 0)]) << comp << " " << i << " " << j;
                largest = std::max(largest, std::abs(h[node(i, j, 0)]));
            }
    EXPECT_TRUE(std::isfinite(largest));
    EXPECT_GT(largest, 0.0);
}

// --- benchmarks (the perf lane: --gtest_also_run_disabled_tests) ---------------------------

namespace {
template <typename F>
double seconds(F&& f)
{
    const auto start = std::chrono::steady_clock::now();
    f();
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}
}  // namespace

TEST(AcceleratedBenchmark, DISABLED_BenchmarkSolversAndKernels)
{
    std::printf("[ benchmark ] %s\n", compute::detectCompute().summary().c_str());
    // Yee 3D: 64 x 64 x 64 cells + 8-cell absorbing layers, 60 steps.
    YeeCase y = yee3D(64);
    y.spec.nx = y.spec.ny = y.spec.nz = 64;
    y.spec.pmlCells = 8;
    y.spec.boundary.fill(em::YeeBoundary::Absorbing);
    y.spec.cellMaterial.clear();
    y.spec.pecEdges.clear();
    y.sources = {{32, 32, 32, 2, 1.0, 0.0, tone(em::kSpeedOfLight / (10.0 * y.spec.cell))}};
    y.probes = {{"p", 40, 32, 32, 2}};
    y.steps = 60;
    y.recording = em::YeeRecording{};
    y.recording.energySamples = 3;
    const double cellsYee = static_cast<double>(y.spec.stateBytes()) / (27.0 * sizeof(double) + 1.0);
    // The step loop only (setup excluded), as the runs measure it.
    const double yeeOne = runYee(y, cpu(1)).elapsedSeconds;
    const double yeeAll = runYee(y, cpu(0)).elapsedSeconds;
    std::printf("[ benchmark ] Yee 3D %.3g cells x %ld steps: 1 thread %.2f s (%.3g cell updates/s), %d threads %.2f s (%.3g/s), speedup %.2f\n",
                cellsYee, y.steps, yeeOne, cellsYee * y.steps / yeeOne, compute::hardwareThreads(), yeeAll, cellsYee * y.steps / yeeAll,
                yeeOne / yeeAll);
    // Acoustic 3D: 100^3 cells + 8-cell layers, 100 steps.
    fdtd::Problem a = acoustic(true);
    a.grid.nx = a.grid.ny = a.grid.nz = 100;
    a.grid.pmlCells = 8;
    a.grid.boundary.fill(fdtd::BoundaryKind::Absorbing);
    a.grid.cellMaterial.clear();
    a.sources.resize(1);
    a.sources[0].i = a.sources[0].j = a.sources[0].k = 50;
    a.probes.clear();
    a.steps = 100;
    a.recording = fdtd::Recording{};
    a.recording.energySamples = 3;
    const std::array<int, 3> total = a.grid.totalCells();
    const double cellsA = static_cast<double>(total[0]) * total[1] * total[2];
    const double aOne = runAcoustic(a, cpu(1)).elapsedSeconds;
    const double aAll = runAcoustic(a, cpu(0)).elapsedSeconds;
    std::printf("[ benchmark ] acoustic 3D %.3g cells x %ld steps: 1 thread %.2f s (%.3g/s), %d threads %.2f s (%.3g/s), speedup %.2f\n", cellsA,
                a.steps, aOne, cellsA * a.steps / aOne, compute::hardwareThreads(), aAll, cellsA * a.steps / aAll, aOne / aAll);
    if (compute::hardwareThreads() >= 4) {
        EXPECT_GT(yeeOne / yeeAll, 1.5);
        EXPECT_GT(aOne / aAll, 1.5);
    }
    if (cudaUnavailable(y.spec.stateBytes()).empty()) {
        const double yeeGpu = runYee(y, cuda()).elapsedSeconds;
        const double aGpu = runAcoustic(a, cuda()).elapsedSeconds;
        std::printf("[ benchmark ] CUDA: Yee %.2f s (%.2fx the threaded CPU), acoustic %.2f s (%.2fx)\n", yeeGpu, yeeAll / yeeGpu, aGpu, aAll / aGpu);
    } else {
        std::printf("[ benchmark ] CUDA not measured: %s\n", cudaUnavailable(y.spec.stateBytes()).c_str());
    }
    // VOLK against the plain loops: |z| of 16 M complex samples, 20 times.
    const size_t n = size_t(1) << 24;
    std::vector<std::complex<float>> z(n, {0.6f, -0.8f});
    std::vector<float> out(n);
    const double volk = seconds([&] {
        for (int r = 0; r < 20; ++r)
            compute::magnitude(z.data(), out.data(), n);
    });
    const double plain = seconds([&] {
        for (int r = 0; r < 20; ++r)
            compute::scalar::magnitude(z.data(), out.data(), n);
    });
    std::printf("[ benchmark ] |z| of %zu complex floats x 20: %s %.3f s, plain loop %.3f s (%.2fx)\n", n,
                compute::volkAvailable() ? ("VOLK (" + compute::volkMachine() + ")").c_str() : "fallback", volk, plain, plain / volk);
}
