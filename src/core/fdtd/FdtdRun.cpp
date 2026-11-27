#include "fdtd/FdtdRun.h"

#include <chrono>
#include <cmath>

namespace resonant::fdtd {

namespace {

// One way of advancing the grid and reading what run() records: the CPU
// reference (threads) or the CUDA device.
class Stepper {
public:
    virtual ~Stepper() = default;
    virtual bool step(std::string& error) = 0;
    virtual double time() const = 0;
    virtual bool probes(std::vector<double>& values, std::string& error) = 0;
    virtual bool accumulateHarmonic(std::complex<double> kernel, std::string& error) = 0;
    virtual bool harmonic(std::vector<std::complex<double>>& out, std::string& error) = 0;
    virtual bool pressureField(std::vector<float>& out, std::string& error) = 0;
    virtual bool energy(double& value, std::string& error) = 0;
    virtual bool maxAbsPressure(double& value, std::string& error) = 0;
    virtual bool finite() = 0;
    virtual std::string backend() const = 0;
};

class CpuStepper final : public Stepper {
public:
    CpuStepper(std::unique_ptr<FdtdSolver> solver, std::vector<std::array<int, 3>> probes) : solver_(std::move(solver)), probes_(std::move(probes)) {}
    bool step(std::string&) override
    {
        solver_->step();
        return true;
    }
    double time() const override { return solver_->time(); }
    bool probes(std::vector<double>& values, std::string&) override
    {
        values.resize(probes_.size());
        for (size_t q = 0; q < probes_.size(); ++q)
            values[q] = solver_->pressure(probes_[q][0], probes_[q][1], probes_[q][2]);
        return true;
    }
    bool accumulateHarmonic(std::complex<double> kernel, std::string&) override
    {
        const GridSpec& g = solver_->spec();
        if (accumulated_.empty())
            accumulated_.assign(g.interiorCells(), {0.0, 0.0});
        forRows([&](int j, int k, size_t first) {
            for (int i = 0; i < g.nx; ++i)
                accumulated_[first + static_cast<size_t>(i)] += solver_->pressure(i, j, k) * kernel;
        });
        return true;
    }
    bool harmonic(std::vector<std::complex<double>>& out, std::string&) override
    {
        if (!accumulated_.empty())
            out = accumulated_;
        return true;
    }
    bool pressureField(std::vector<float>& out, std::string&) override
    {
        const GridSpec& g = solver_->spec();
        out.assign(g.interiorCells(), 0.0f);
        forRows([&](int j, int k, size_t first) {
            for (int i = 0; i < g.nx; ++i)
                out[first + static_cast<size_t>(i)] = static_cast<float>(solver_->pressure(i, j, k));
        });
        return true;
    }
    bool energy(double& value, std::string&) override
    {
        value = solver_->energy();
        return true;
    }
    bool maxAbsPressure(double& value, std::string&) override
    {
        value = solver_->maxAbsPressure();
        return true;
    }
    bool finite() override { return solver_->finite(); }
    std::string backend() const override
    {
        const int t = solver_->threads();
        return "CPU, double precision, " + std::to_string(t) + (t == 1 ? " thread" : " threads");
    }

private:
    // Rows (j, k) of the region in parallel; `first` is the row's first cell index.
    template <typename F>
    void forRows(F&& body)
    {
        const GridSpec& g = solver_->spec();
        const size_t rows = static_cast<size_t>(g.ny) * static_cast<size_t>(g.nz);
        solver_->pool().run(rows, [&](size_t begin, size_t end) {
            for (size_t row = begin; row < end; ++row)
                body(static_cast<int>(row % static_cast<size_t>(g.ny)), static_cast<int>(row / static_cast<size_t>(g.ny)), row * static_cast<size_t>(g.nx));
        });
    }

    std::unique_ptr<FdtdSolver> solver_;
    std::vector<std::array<int, 3>> probes_;
    std::vector<std::complex<double>> accumulated_;
};

class CudaStepper final : public Stepper {
public:
    CudaStepper(std::unique_ptr<FdtdSolver> solver, FdtdCudaState* state, std::vector<size_t> probeCells)
        : solver_(std::move(solver)), state_(state), probeCells_(std::move(probeCells))
    {
    }
    ~CudaStepper() override { fdtdCudaDestroy(state_); }
    bool step(std::string& error) override
    {
        solver_->advanceSources(increments_);
        if (!fdtdCudaStep(state_, increments_.data(), error))
            return false;
        solver_->advanceClock();
        return true;
    }
    double time() const override { return solver_->time(); }
    bool probes(std::vector<double>& values, std::string& error) override
    {
        values.resize(probeCells_.size());
        return probeCells_.empty() || fdtdCudaGather(state_, probeCells_.data(), static_cast<int>(probeCells_.size()), values.data(), error);
    }
    bool accumulateHarmonic(std::complex<double> kernel, std::string& error) override
    {
        accumulating_ = true;
        return fdtdCudaAccumulateHarmonic(state_, kernel.real(), kernel.imag(), error);
    }
    bool harmonic(std::vector<std::complex<double>>& out, std::string& error) override
    {
        if (!accumulating_)
            return true;
        std::vector<double> re, im;
        if (!fdtdCudaHarmonic(state_, re, im, error))
            return false;
        out.resize(re.size());
        for (size_t n = 0; n < re.size(); ++n)
            out[n] = {re[n], im[n]};
        return true;
    }
    bool pressureField(std::vector<float>& out, std::string& error) override { return fdtdCudaPressureField(state_, out, error); }
    bool energy(double& value, std::string& error) override { return fdtdCudaEnergy(state_, value, error); }
    bool maxAbsPressure(double& value, std::string& error) override { return fdtdCudaMaxAbsPressure(state_, value, error); }
    bool finite() override { return true; }  // a non-finite field makes the energy non-finite, which run() checks
    std::string backend() const override { return "CUDA on " + fdtdCudaDeviceName(state_) + ", double precision"; }

private:
    std::unique_ptr<FdtdSolver> solver_;  // keeps the sources' running integrals and the clock
    FdtdCudaState* state_;
    std::vector<size_t> probeCells_;
    std::vector<double> increments_;
    bool accumulating_ = false;
};

}  // namespace

RunEnd run(const Problem& problem, RunOutput& out, JobContext* context)
{
    constexpr double twoPi = 6.283185307179586476925;
    if (problem.steps < 1)
        return {RunEnd::Kind::Invalid, "the simulation needs at least one time step"};
    if (const Status status = compute::checkBackend(problem.grid.compute, problem.grid.stateBytes()); !status)
        return {RunEnd::Kind::Invalid, status.message()};
    auto created = FdtdSolver::create(problem.grid);
    if (!created)
        return {RunEnd::Kind::Invalid, created.message()};
    std::unique_ptr<FdtdSolver> solver = created.take();
    const GridSpec grid = solver->spec();
    for (const GridSource& s : problem.sources) {
        if (s.i < 0 || s.i >= grid.nx || s.j < 0 || s.j >= grid.ny || s.k < 0 || s.k >= grid.nz)
            return {RunEnd::Kind::Invalid, "a source lies outside the grid"};
        if (const Status status = s.waveform.validate(); !status)
            return {RunEnd::Kind::Invalid, status.message()};
        solver->addSource(s);
    }

    out = RunOutput{};
    out.dt = grid.dt;
    std::vector<std::array<int, 3>> probeCells;
    std::vector<size_t> paddedProbes;
    for (const ProbeSpec& p : problem.probes) {
        if (p.i < 0 || p.i >= grid.nx || p.j < 0 || p.j >= grid.ny || p.k < 0 || p.k >= grid.nz)
            return {RunEnd::Kind::Invalid, "probe '" + p.label + "' lies outside the grid"};
        out.probes.push_back({p.objectId, p.label, p.position, {}});
        out.probes.back().pressure.reserve(static_cast<size_t>(problem.steps));
        probeCells.push_back({p.i, p.j, p.k});
        paddedProbes.push_back(solver->paddedCell(p.i, p.j, p.k));
    }
    std::unique_ptr<Stepper> stepper;
    if (grid.compute.backend == compute::ComputeBackend::Cuda) {
        std::string error;
        FdtdCudaState* state = fdtdCudaCreate(solver->deviceLayout(), grid.compute.device, error);
        if (!state)
            return {RunEnd::Kind::Invalid, "the CUDA backend could not start: " + error};
        stepper = std::make_unique<CudaStepper>(std::move(solver), state, std::move(paddedProbes));
    } else {
        stepper = std::make_unique<CpuStepper>(std::move(solver), std::move(probeCells));
    }
    out.backend = stepper->backend();

    const Recording& rec = problem.recording;
    // Snapshot k of N is taken after step round(k * steps / N): evenly spaced, the last one at the end.
    const long snapshotTotal = std::min<long>(rec.snapshotCount, problem.steps);
    long snapshotsTaken = 0;
    const auto snapshotStep = [&](long index) { return std::lround(static_cast<double>(index) * static_cast<double>(problem.steps) / static_cast<double>(snapshotTotal)); };
    const long energyEvery = std::max<long>(1, problem.steps / std::max(1, rec.energySamples));

    long harmonicFirst = problem.steps;  // first step (1-based, after stepping) inside the window
    long harmonicCount = 0;
    if (rec.harmonicFrequencyHz > 0.0) {
        harmonicCount = std::lround(rec.harmonicPeriods / (rec.harmonicFrequencyHz * grid.dt));
        harmonicCount = std::clamp<long>(harmonicCount, 1, problem.steps);
        harmonicFirst = problem.steps - harmonicCount + 1;
        out.harmonic.assign(grid.interiorCells(), {0.0, 0.0});
        out.harmonicFrequencyHz = rec.harmonicFrequencyHz;
        out.harmonicStart = static_cast<double>(harmonicFirst) * grid.dt;
        out.harmonicEnd = static_cast<double>(problem.steps) * grid.dt;
    }

    const auto started = std::chrono::steady_clock::now();
    const auto elapsed = [&] { return std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count(); };
    const std::array<int, 3> total = grid.totalCells();
    const double cellsPerStep = static_cast<double>(total[0]) * total[1] * total[2];
    std::string error;
    std::vector<double> probeValues;
    const auto failed = [&](long n) {
        out.elapsedSeconds = elapsed();
        return RunEnd{RunEnd::Kind::Invalid, "the " + out.backend + " backend failed at step " + std::to_string(n) + ": " + error};
    };

    for (long n = 1; n <= problem.steps; ++n) {
        if (!stepper->step(error))
            return failed(n);
        const double t = stepper->time();

        if (!problem.probes.empty()) {
            if (!stepper->probes(probeValues, error))
                return failed(n);
            for (size_t q = 0; q < problem.probes.size(); ++q)
                out.probes[q].pressure.push_back(probeValues[q]);
        }

        if (n >= harmonicFirst && harmonicCount > 0) {
            const std::complex<double> kernel = std::polar(2.0 / static_cast<double>(harmonicCount), -twoPi * rec.harmonicFrequencyHz * t);
            if (!stepper->accumulateHarmonic(kernel, error))
                return failed(n);
        }

        if (snapshotsTaken < snapshotTotal && n >= snapshotStep(snapshotsTaken + 1)) {
            ++snapshotsTaken;
            std::vector<float> snapshot;
            if (!stepper->pressureField(snapshot, error))
                return failed(n);
            out.snapshots.push_back(std::move(snapshot));
            out.snapshotTimes.push_back(t);
        }

        if (n % energyEvery == 0 || n == problem.steps) {
            double energy = 0.0, peak = 0.0;
            if (!stepper->energy(energy, error) || !stepper->maxAbsPressure(peak, error))
                return failed(n);
            out.energyTimes.push_back(t);
            out.energy.push_back(energy);
            out.peakPressure = std::max(out.peakPressure, peak);
            out.steps = n;
            if (!std::isfinite(energy) || !stepper->finite()) {
                out.elapsedSeconds = elapsed();
                return {RunEnd::Kind::Unstable,
                        "the field stopped being finite at step " + std::to_string(n) + " (t = " + std::to_string(t)
                            + " s): the calculation is numerically unstable and its results are not usable"};
            }
            if (context) {
                if (context->cancelRequested()) {
                    out.elapsedSeconds = elapsed();
                    return {RunEnd::Kind::Cancelled, "cancelled"};
                }
                context->reportProgress(static_cast<double>(n) / static_cast<double>(problem.steps),
                                        "step " + std::to_string(n) + " of " + std::to_string(problem.steps));
            }
        }
    }
    if (harmonicCount > 0 && !stepper->harmonic(out.harmonic, error))
        return failed(problem.steps);
    out.steps = problem.steps;
    out.elapsedSeconds = elapsed();
    out.cellUpdatesPerSecond = out.elapsedSeconds > 0.0 ? cellsPerStep * static_cast<double>(problem.steps) / out.elapsedSeconds : 0.0;
    return {};
}

}  // namespace resonant::fdtd
