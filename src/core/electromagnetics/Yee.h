#pragma once

// Full-wave electromagnetic solver: finite differences in the time domain
// on Yee's staggered grid (Yee 1966).
//
//   dH/dt = -(1/mu) curl E                dE/dt = (1/eps) (curl H - sigma E - J)
//
// Uniform cubic cells of size d. Node (i, j, k) sits at (i d, j d, k d);
// Ex at (i+1/2, j, k), Ey at (i, j+1/2, k), Ez at (i, j, k+1/2), Hx at
// (i, j+1/2, k+1/2), Hy at (i+1/2, j, k+1/2), Hz at (i+1/2, j+1/2, k). E at
// times n dt, H at (n+1/2) dt; second order in space and time.
//
// With nz = 1 the problem is two-dimensional (fields invariant along z): the
// TM polarization (Ez, Hx, Hy) and the TE polarization (Ex, Ey, Hz) evolve
// independently; a z-directed current excites TM only.
//
// Materials per cell: relative permittivity and permeability, conductivity,
// or perfect electric conductor (every E component on an edge of a PEC cell
// is held at zero: staircased surfaces). An edge takes the mean permittivity
// and conductivity of the cells around it, a face the mean permeability.
//
// Domain faces: perfect electric conductor, periodic, or a split-field
// perfectly matched layer (Berenger) outside the region backed by PEC.
#include "compute/Compute.h"
#include "compute/ThreadPool.h"
#include "dsp/Waveform.h"
#include "electromagnetics/Conventions.h"
#include "electromagnetics/YeeDevice.h"
#include "foundation/Result.h"
#include "simulation/Job.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace resonant::em {

enum class YeeBoundary { PEC, Periodic, Absorbing };
std::string_view toString(YeeBoundary boundary);
Result<YeeBoundary> yeeBoundaryFromString(std::string_view text);

struct YeeMaterial {
    std::string name = "vacuum";
    double relativePermittivity = 1.0;
    double relativePermeability = 1.0;
    double conductivity = 0.0;  // S/m
    bool pec = false;
};

struct YeeSpec {
    int nx = 2, ny = 2, nz = 1;  // cells; nz = 1: two-dimensional
    double cell = 1e-3;          // m
    double dt = 0.0;             // s
    std::array<YeeBoundary, 6> boundary{YeeBoundary::Absorbing, YeeBoundary::Absorbing, YeeBoundary::Absorbing,
                                        YeeBoundary::Absorbing, YeeBoundary::Absorbing, YeeBoundary::Absorbing};
    int pmlCells = 10;
    double pmlReflection = 1e-6;
    std::vector<YeeMaterial> materials{YeeMaterial{}};
    std::vector<std::uint8_t> cellMaterial;  // nx*ny*nz, empty = all materials[0]
    // Thin perfectly conducting wires: E edges held at zero, by component and
    // region node (the standard staircase thin wire; effective radius about 0.23 cells).
    struct Edge {
        int component = 2, i = 0, j = 0, k = 0;
        friend bool operator==(const Edge&, const Edge&) = default;
    };
    std::vector<Edge> pecEdges;
    // Where it runs: CPU threads (bit-identical for any count) or CUDA.
    compute::ComputeOptions compute;

    bool is2D() const { return nz == 1; }
    int dimensions() const { return is2D() ? 2 : 3; }
    size_t cellIndex(int i, int j, int k) const
    {
        return static_cast<size_t>(i) + static_cast<size_t>(nx) * (static_cast<size_t>(j) + static_cast<size_t>(ny) * static_cast<size_t>(k));
    }
    double maxSpeed() const;  // over the non-PEC materials in use
    double minSpeed() const;
    size_t stateBytes() const;
    Status validate() const;
};

// Largest stable time step: d / (c_max sqrt(D)).
double yeeStabilityLimit(double cell, double maxSpeed, int dimensions);

// A current element on one E edge: I(t) = amplitude w(t - delay) along
// `component` (0 x, 1 y, 2 z) at node (i, j, k) of the region. In 3D it is
// a short current element of length d; in 2D (z component) a line current.
struct YeeSource {
    int i = 0, j = 0, k = 0;
    int component = 2;
    double amplitude = 1.0;  // A
    double delay = 0.0;
    dsp::Waveform waveform;
};

class YeeSolver {
public:
    static Result<std::unique_ptr<YeeSolver>> create(YeeSpec spec);
    const YeeSpec& spec() const { return spec_; }
    void addSource(const YeeSource& source);
    void step();
    long stepIndex() const { return step_; }
    double time() const { return static_cast<double>(step_) * spec_.dt; }

    // E component at its own edge position next to region node (i, j, k).
    double e(int component, int i, int j, int k = 0) const;
    // E component at region node (i, j, k): the mean of the edges on either
    // side along the component's axis (the edge before it at the last node).
    double eAtNode(int component, int i, int j, int k = 0) const;
    // H component at its own face position.
    double h(int component, int i, int j, int k = 0) const;
    // Energy in the grid (J; J per metre in 2D), an estimate: E is taken at
    // n dt and H at (n - 1/2) dt, so the sum ripples with the field by about
    // omega dt; it does not grow in a stable closed lossless domain.
    double energy() const;
    bool finite() const;

    int threads() const { return pool_ ? pool_->threads() : 1; }
    compute::ThreadPool& pool() { return *pool_; }
    // The state as plain arrays, for the CUDA backend (valid while this lives).
    YeeDeviceLayout deviceLayout() const;
    // cb * scale * w(t_mid - delay) of every source for the step from the
    // current time, in the order of deviceLayout()'s sources.
    void sourceValues(std::vector<double>& out) const;
    // Advances the step counter without updating fields (the CUDA backend
    // steps the fields on the device).
    void advanceClock() { ++step_; }
    // Padded node index of region node (i, j, k); indices along a periodic
    // axis wrap (that axis keeps no duplicate last node: node n is node 0).
    size_t paddedNode(int i, int j, int k) const
    {
        return at(wrap(0, i + pad_[0]), wrap(1, j + pad_[1]), spec_.is2D() ? 0 : wrap(2, k + pad_[2]));
    }

private:
    explicit YeeSolver(YeeSpec spec);
    size_t at(int i, int j, int k) const
    {
        return static_cast<size_t>(i) + static_cast<size_t>(px_) * (static_cast<size_t>(j) + static_cast<size_t>(py_) * static_cast<size_t>(k));
    }
    int wrap(int axis, int index) const;
    void updateH();
    void updateE();

    YeeSpec spec_;
    int px_ = 1, py_ = 1, pz_ = 1;          // nodes (including absorbing layers); periodic axes have no duplicate node
    std::array<int, 3> pad_{0, 0, 0};
    std::array<bool, 3> periodic_{false, false, false};
    long step_ = 0;
    // Split fields: e_[c][p] is the part of component c driven by the derivative along axis p.
    std::array<std::array<std::vector<double>, 3>, 3> e_, h_;
    std::array<std::vector<double>, 3> ca_, cb_;  // per E component and node
    std::array<std::vector<double>, 3> chb_;       // per H component: dt / (mu d)
    std::array<std::vector<double>, 3> decayNode_, decayHalf_;  // PML factor per axis at nodes and half nodes
    std::vector<std::uint8_t> material_;           // per padded cell
    struct Active {
        int component;
        size_t node;
        double scale;  // amplitude / d^2 (current density) times cb / cell factor handled in step
        double delay;
        dsp::Waveform waveform;
    };
    std::vector<Active> sources_;
    std::unique_ptr<compute::ThreadPool> pool_;
};

struct YeeProbe {
    std::string label;
    int i = 0, j = 0, k = 0;
    int component = 2;
};

struct YeeRecording {
    int snapshotCount = 0;           // of the chosen component over the region
    int snapshotComponent = 2;
    double harmonicFrequencyHz = 0;  // > 0: complex E (all components, at nodes) over the last periods
    double harmonicPeriods = 8.0;
    int energySamples = 200;
};

struct YeeOutput {
    double dt = 0.0;
    long steps = 0;
    std::vector<std::vector<double>> probes;          // [probe][step], sample n at time (n+1) dt
    std::vector<double> snapshotTimes;
    std::vector<std::vector<float>> snapshots;        // node values, (nx+1)(ny+1)(nz+1) (2D: (nx+1)(ny+1))
    std::array<std::vector<std::complex<double>>, 3> harmonic;  // per E component at region nodes (averaged from edges)
    double harmonicFrequencyHz = 0.0;
    std::vector<double> energyTimes, energy;
    double elapsedSeconds = 0.0;
    double cellUpdatesPerSecond = 0.0;
    std::string backend;  // where it ran, with the precision
};

struct YeeRunEnd {
    enum class Kind { Completed, Cancelled, Unstable, Invalid } kind = Kind::Completed;
    std::string message;
};

YeeRunEnd runYee(const YeeSpec& spec, const std::vector<YeeSource>& sources, const std::vector<YeeProbe>& probes, long steps,
                 const YeeRecording& recording, YeeOutput& output, JobContext* context = nullptr);

}  // namespace resonant::em
