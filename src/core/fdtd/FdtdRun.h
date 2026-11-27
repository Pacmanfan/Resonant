#pragma once

// Running a numerical problem and recording what was asked for.
//
// Nothing is recorded by default except what is cheap: a field is never
// stored at every time step unless snapshots are requested.
#include "fdtd/FdtdGrid.h"
#include "foundation/Id.h"
#include "foundation/Types.h"
#include "simulation/Job.h"

#include <complex>

namespace resonant::fdtd {

struct ProbeSpec {
    Id objectId;
    std::string label;
    Vec3 position = Vec3::Zero();  // world position of the cell centre used
    int i = 0, j = 0, k = 0;       // interior cell
};

struct Recording {
    // Snapshots of the pressure in the region of interest, evenly spaced
    // over the run, the last one at the end. 0 = none.
    int snapshotCount = 0;
    // Harmonic analysis: the complex amplitude at this frequency, from the
    // last `harmonicPeriods` periods of the run. 0 = none.
    double harmonicFrequencyHz = 0.0;
    double harmonicPeriods = 8.0;
    // Energy is sampled this many times over the run.
    int energySamples = 200;
};

struct Problem {
    GridSpec grid;
    std::vector<GridSource> sources;
    std::vector<ProbeSpec> probes;
    long steps = 0;
    Recording recording;
};

struct ProbeTrace {
    Id objectId;
    std::string label;
    Vec3 position = Vec3::Zero();
    std::vector<double> pressure;  // Pa; sample n is the pressure at time (n + 1) dt
};

struct RunOutput {
    double dt = 0.0;
    long steps = 0;
    std::vector<ProbeTrace> probes;
    std::vector<double> snapshotTimes;         // s
    std::vector<std::vector<float>> snapshots; // each nx*ny*nz, Pa
    // Harmonic field, complex peak amplitude with the e^{+jwt} convention:
    // p(t) = Re{ P e^{jwt} } during the analysis window.
    std::vector<std::complex<double>> harmonic;
    double harmonicFrequencyHz = 0.0;
    double harmonicStart = 0.0, harmonicEnd = 0.0;  // s, the analysis window
    std::vector<double> energyTimes, energy;        // J (3D) or J/m (2D)
    double peakPressure = 0.0;                      // largest |p| seen in the region at a sampled instant
    double elapsedSeconds = 0.0;
    double cellUpdatesPerSecond = 0.0;
    std::string backend;  // where it ran, with the precision
};

struct RunEnd {
    enum class Kind { Completed, Cancelled, Unstable, Invalid };
    Kind kind = Kind::Completed;
    std::string message;
};

// Runs the problem on the backend of problem.grid.compute (the CPU, or CUDA
// when chosen and usable; never switched silently). `context` may be null.
// On Unstable the output holds what was recorded up to the step at which the
// field stopped being finite.
RunEnd run(const Problem& problem, RunOutput& output, JobContext* context = nullptr);

}  // namespace resonant::fdtd
