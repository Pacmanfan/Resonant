#include "fdtd/FdtdRun.h"

#include <chrono>
#include <cmath>

namespace resonant::fdtd {

RunEnd run(const Problem& problem, RunOutput& out, JobContext* context)
{
    constexpr double twoPi = 6.283185307179586476925;
    if (problem.steps < 1)
        return {RunEnd::Kind::Invalid, "the simulation needs at least one time step"};
    auto created = FdtdSolver::create(problem.grid);
    if (!created)
        return {RunEnd::Kind::Invalid, created.message()};
    const std::unique_ptr<FdtdSolver> solver = created.take();
    const GridSpec& grid = solver->spec();
    for (const GridSource& s : problem.sources) {
        if (s.i < 0 || s.i >= grid.nx || s.j < 0 || s.j >= grid.ny || s.k < 0 || s.k >= grid.nz)
            return {RunEnd::Kind::Invalid, "a source lies outside the grid"};
        if (const Status status = s.waveform.validate(); !status)
            return {RunEnd::Kind::Invalid, status.message()};
        solver->addSource(s);
    }

    out = RunOutput{};
    out.dt = grid.dt;
    for (const ProbeSpec& p : problem.probes) {
        if (p.i < 0 || p.i >= grid.nx || p.j < 0 || p.j >= grid.ny || p.k < 0 || p.k >= grid.nz)
            return {RunEnd::Kind::Invalid, "probe '" + p.label + "' lies outside the grid"};
        out.probes.push_back({p.objectId, p.label, p.position, {}});
        out.probes.back().pressure.reserve(static_cast<size_t>(problem.steps));
    }

    const Recording& rec = problem.recording;
    const size_t cells = grid.interiorCells();
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
        out.harmonic.assign(cells, {0.0, 0.0});
        out.harmonicFrequencyHz = rec.harmonicFrequencyHz;
        out.harmonicStart = static_cast<double>(harmonicFirst) * grid.dt;
        out.harmonicEnd = static_cast<double>(problem.steps) * grid.dt;
    }

    const auto started = std::chrono::steady_clock::now();
    const auto elapsed = [&] { return std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count(); };
    const std::array<int, 3> total = grid.totalCells();
    const double cellsPerStep = static_cast<double>(total[0]) * total[1] * total[2];

    for (long n = 1; n <= problem.steps; ++n) {
        solver->step();
        const double t = solver->time();

        for (size_t q = 0; q < problem.probes.size(); ++q)
            out.probes[q].pressure.push_back(solver->pressure(problem.probes[q].i, problem.probes[q].j, problem.probes[q].k));

        if (n >= harmonicFirst && harmonicCount > 0) {
            const std::complex<double> kernel = std::polar(2.0 / static_cast<double>(harmonicCount), -twoPi * rec.harmonicFrequencyHz * t);
            size_t c = 0;
            for (int k = 0; k < grid.nz; ++k)
                for (int j = 0; j < grid.ny; ++j)
                    for (int i = 0; i < grid.nx; ++i)
                        out.harmonic[c++] += solver->pressure(i, j, k) * kernel;
        }

        if (snapshotsTaken < snapshotTotal && n >= snapshotStep(snapshotsTaken + 1)) {
            ++snapshotsTaken;
            std::vector<float> snapshot(cells);
            size_t c = 0;
            for (int k = 0; k < grid.nz; ++k)
                for (int j = 0; j < grid.ny; ++j)
                    for (int i = 0; i < grid.nx; ++i)
                        snapshot[c++] = static_cast<float>(solver->pressure(i, j, k));
            out.snapshots.push_back(std::move(snapshot));
            out.snapshotTimes.push_back(t);
        }

        if (n % energyEvery == 0 || n == problem.steps) {
            const double energy = solver->energy();
            out.energyTimes.push_back(t);
            out.energy.push_back(energy);
            out.peakPressure = std::max(out.peakPressure, solver->maxAbsPressure());
            out.steps = n;
            if (!std::isfinite(energy) || !solver->finite()) {
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
    out.steps = problem.steps;
    out.elapsedSeconds = elapsed();
    out.cellUpdatesPerSecond = out.elapsedSeconds > 0.0 ? cellsPerStep * static_cast<double>(problem.steps) / out.elapsedSeconds : 0.0;
    return {};
}

}  // namespace resonant::fdtd
