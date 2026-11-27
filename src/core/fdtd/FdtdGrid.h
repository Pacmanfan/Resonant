#pragma once

// Acoustic finite-difference time-domain solver on a uniform staggered grid.
//
// Equations (linear acoustics, fluid at rest, no flow):
//
//     rho dv/dt = -grad p
//     dp/dt     = -K div v - gamma p + K q          K = rho c^2
//
// q is a volume-velocity source density (1/s), gamma a frequency-independent
// loss (1/s). Pressure lives at cell centres and integer time steps, the
// velocity components at the cell faces and half time steps (the staggered
// "leapfrog" scheme, second-order accurate in space and time). Density and
// bulk modulus may differ from cell to cell; a face uses the mean density of
// its two cells, which makes pressure and normal velocity continuous across
// a material interface.
//
// The scheme, its stability limit, its numerical dispersion and the
// boundary treatments are documented in docs/advanced_acoustics/FDTD_SOLVER.md.
//
// This file knows nothing about scenes: a GridSpec is a complete numerical
// problem. FdtdSetup.h builds one from a project.
#include "fdtd/Waveform.h"
#include "foundation/Result.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace resonant::fdtd {

enum class BoundaryKind {
    Rigid,            // normal velocity zero at the face
    PressureRelease,  // pressure zero at the face
    Impedance,        // locally reacting: p = Z v_n at the face
    Absorbing         // perfectly matched layer, backed by a rigid wall
};

std::string_view toString(BoundaryKind kind);
Result<BoundaryKind> boundaryKindFromString(std::string_view text);

enum Face { XMin = 0, XMax, YMin, YMax, ZMin, ZMax };

struct CellMaterial {
    std::string name;
    double density = 1.2;          // kg/m^3
    double soundSpeed = 343.0;     // m/s
    double attenuation = 0.0;      // Np/m, frequency-independent amplitude attenuation
    bool rigid = false;            // no motion: normal velocity zero on all its faces
    bool pressureRelease = false;  // pressure held at zero

    double bulkModulus() const { return density * soundSpeed * soundSpeed; }
    bool isFluid() const { return !rigid && !pressureRelease; }
};

struct GridSpec {
    // Cells of the region of interest. nz == 1 selects the two-dimensional
    // scheme (fields do not vary along the third direction).
    int nx = 1, ny = 1, nz = 1;
    double dx = 1e-3;  // m, cell size in every direction
    double dt = 0.0;   // s
    std::array<BoundaryKind, 6> boundary{BoundaryKind::Absorbing, BoundaryKind::Absorbing, BoundaryKind::Absorbing,
                                         BoundaryKind::Absorbing, BoundaryKind::Absorbing, BoundaryKind::Absorbing};
    std::array<double, 6> impedance{};  // Pa s/m, for Impedance faces
    int pmlCells = 12;                  // thickness of each absorbing layer, added outside the region
    double pmlTheoreticalReflection = 1e-6;  // design value of the profile at normal incidence
    std::vector<CellMaterial> materials{CellMaterial{}};  // [0] is the background medium
    std::vector<std::uint8_t> cellMaterial;                // nx*ny*nz indices into `materials`; empty = all background

    bool is2D() const { return nz == 1; }
    int dimensions() const { return is2D() ? 2 : 3; }
    size_t interiorCells() const { return static_cast<size_t>(nx) * static_cast<size_t>(ny) * static_cast<size_t>(nz); }
    size_t interiorIndex(int i, int j, int k) const
    {
        return static_cast<size_t>(i) + static_cast<size_t>(nx) * (static_cast<size_t>(j) + static_cast<size_t>(ny) * static_cast<size_t>(k));
    }
    // Cells including the absorbing layers.
    std::array<int, 3> totalCells() const;
    // Fastest and slowest sound speed among the fluid materials in use.
    double maxSoundSpeed() const;
    double minSoundSpeed() const;
    // Bytes of solver state for this grid.
    size_t stateBytes() const;

    Status validate() const;
};

// Largest stable time step of the scheme: dx / (c_max sqrt(D)).
double stabilityLimit(double dx, double maxSoundSpeed, int dimensions);

// Phase velocity of the scheme relative to the true one, for a plane wave
// along a grid axis (the worst direction) driven at a frequency whose true
// wavelength is `pointsPerWavelength` cells, from the numerical dispersion
// relation sin(w dt / 2) / (c dt) = sin(k dx / 2) / dx.
double axialPhaseVelocityRatio(double pointsPerWavelength, double courant);

// A volume-velocity point source in one cell. The volume acceleration is
//     dQ/dt = strength * w(t - delay)
// In 3D (Q in m^3/s) the radiated pressure in a homogeneous medium is
// rho * strength * w(t - delay - r/c) / (4 pi r). In 2D the source is a line
// source along the invariant direction and Q is per metre of its length.
struct GridSource {
    int i = 0, j = 0, k = 0;  // interior cell
    double strength = 0.0;    // m^3/s^2 (3D) or m^2/s^2 (2D)
    double delay = 0.0;       // s
    Waveform waveform;
};

class FdtdSolver {
public:
    static Result<std::unique_ptr<FdtdSolver>> create(GridSpec spec);

    const GridSpec& spec() const { return spec_; }
    void addSource(const GridSource& source);

    // Advances pressure and velocity by one time step.
    void step();

    long stepIndex() const { return step_; }
    double time() const { return static_cast<double>(step_) * spec_.dt; }  // time of the pressure field

    // Pressure in an interior cell, Pa.
    double pressure(int i, int j, int k = 0) const { return p_[index(i + padLo_[0], j + padLo_[1], k + padLo_[2])]; }
    // Velocity at the centre of an interior cell (mean of its faces), m/s.
    std::array<double, 3> velocity(int i, int j, int k = 0) const;

    // Acoustic energy in the whole grid including absorbing layers: joules
    // in 3D, joules per metre in 2D. Constant to rounding in a closed
    // lossless domain once the sources have ended.
    double energy() const;
    // Largest |p| in the region of interest.
    double maxAbsPressure() const;
    bool finite() const;

private:
    explicit FdtdSolver(GridSpec spec);
    size_t index(int i, int j, int k) const
    {
        return static_cast<size_t>(i) + static_cast<size_t>(nx_) * (static_cast<size_t>(j) + static_cast<size_t>(ny_) * static_cast<size_t>(k));
    }
    void updateVelocity();
    void updatePressure();
    void applySources();

    GridSpec spec_;
    int nx_ = 1, ny_ = 1, nz_ = 1;       // including absorbing layers
    std::array<int, 3> padLo_{0, 0, 0};  // absorbing cells before the interior
    long step_ = 0;

    std::vector<std::uint8_t> material_;        // per cell
    std::vector<double> p_, px_, py_, pz_;      // pressure and its split parts
    std::vector<double> vx_, vy_, vz_;          // (nx+1)*ny*nz, nx*(ny+1)*nz, nx*ny*(nz+1)
    std::vector<double> kappa_;                 // per material: K dt / dx
    std::vector<double> loss_;                  // per material: exp(-gamma dt)
    std::vector<double> faceCoefficient_;       // [a * M + b]: dt / (dx * mean density), 0 next to a rigid cell
    std::vector<double> cellInverseDensity_;    // per material: dt / (dx rho)
    std::array<std::vector<double>, 3> decayCell_, decayFace_;  // PML factors exp(-sigma dt / 2)

    struct ActiveSource {
        size_t cell;
        double scale;     // strength * K dt / cell volume
        double delay;
        Waveform waveform;
        double integral;  // running integral of w(t - delay)
        double previous;  // w at the previous step, for the trapezoidal rule
    };
    std::vector<ActiveSource> sources_;
};

}  // namespace resonant::fdtd
