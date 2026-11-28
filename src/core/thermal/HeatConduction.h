#pragma once

// Heat conduction (REQ-CHAT-20261126-016, RES-CEME-010, docs/thermal/HEAT_CONDUCTION.md).
//
//   rho c_p dT/dt = div(k grad T) + q
//
// Finite volumes on a rectilinear cell grid (cell widths may differ along
// each axis): one temperature per cell, face conductances in series between
// neighbouring cells (harmonic mean of the half-cell resistances), the six
// outer faces adiabatic, at a fixed temperature, or convective (h, T_ambient).
// Steady state by a sparse Cholesky factorization; transient by implicit
// Euler, which keeps the discrete energy balance exact: source energy =
// stored energy + heat out through the faces, to rounding.
//
// SI units: m, K, W, W/m^3, W/(m K), J/(m^3 K). Linear (properties do not
// depend on temperature inside one solve; coupled runs update them between
// solves). No radiation, no convection inside the grid.
#include "foundation/Result.h"
#include "foundation/Types.h"

#include <array>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace resonant::thermal {

// Faces in the order -x, +x, -y, +y, -z, +z.
enum class Face { MinusX, PlusX, MinusY, PlusY, MinusZ, PlusZ };
std::string_view toString(Face face);

enum class FaceKind { Adiabatic, Temperature, Convective };
std::string_view toString(FaceKind kind);

struct FaceCondition {
    FaceKind kind = FaceKind::Adiabatic;
    double temperature = 293.15;          // K, for Temperature
    double heatTransferCoefficient = 0.0; // W/(m^2 K), for Convective
    double ambient = 293.15;              // K, for Convective
};

// A rectilinear grid: edges[axis] has count+1 ascending coordinates (m, world).
struct CellGrid {
    std::array<std::vector<double>, 3> edges;

    size_t count(int axis) const { return edges[static_cast<size_t>(axis)].size() - 1; }
    size_t cellCount() const { return count(0) * count(1) * count(2); }
    size_t index(size_t i, size_t j, size_t k) const { return i + count(0) * (j + count(1) * k); }
    double width(int axis, size_t i) const { return edges[static_cast<size_t>(axis)][i + 1] - edges[static_cast<size_t>(axis)][i]; }
    double volume(size_t i, size_t j, size_t k) const { return width(0, i) * width(1, j) * width(2, k); }
    double faceArea(int axis) const;  // area of one outer face normal to `axis` (the grid is a box)
    Status validate() const;

    static CellGrid uniform(const Vec3& origin, const Vec3& size, size_t nx, size_t ny, size_t nz);
};

struct ConductionProblem {
    CellGrid grid;
    std::vector<double> conductivity;          // W/(m K), per cell
    std::vector<double> volumetricHeatCapacity; // rho c_p, J/(m^3 K), per cell (transient only)
    std::vector<double> source;                // W/m^3, per cell (empty: none)
    std::array<FaceCondition, 6> faces;
    std::vector<double> initialTemperature;    // K, per cell (transient only; empty: the reference)
    double referenceTemperature = 293.15;      // K: stored energy is counted from it

    Status validate(bool transient) const;
};

struct ConductionSolution {
    std::vector<double> temperature;   // K, per cell
    std::array<double, 6> faceHeatOut{};  // W leaving through each outer face (steady), or J over the run (transient)
    double sourcePower = 0.0;          // W (steady), or J over the run (transient)
    double storedEnergyChange = 0.0;   // J (transient), 0 for a steady state
    double balanceResidual = 0.0;      // source - stored - out (W or J): rounding for this discretization
    bool transient = false;
    double duration = 0.0;             // s
    std::vector<double> times;         // s, transient snapshots
    std::vector<std::vector<double>> snapshots;
    std::string method;
    double heatOut() const;            // sum over faces
    double minTemperature() const;
    double maxTemperature() const;
};

Result<ConductionSolution> solveSteady(const ConductionProblem& problem);

struct TransientSettings {
    double duration = 1.0;  // s
    int steps = 100;
    int snapshots = 10;
};
// `cancelled` is polled every step (may be empty).
Result<ConductionSolution> solveTransient(const ConductionProblem& problem, const TransientSettings& settings,
                                          const std::function<bool()>& cancelled = {});

// The solver's identity for result provenance.
inline constexpr const char* kSolverId = "thermal.conduction.fv";
inline constexpr const char* kSolverVersion = "1.0";

}  // namespace resonant::thermal
