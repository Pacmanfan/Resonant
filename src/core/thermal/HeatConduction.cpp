#include "thermal/HeatConduction.h"

#include <Eigen/Sparse>
#include <Eigen/SparseCholesky>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace resonant::thermal {
namespace {

// The outer-face conductance of a boundary cell (W/K), 0 when adiabatic.
double boundaryConductance(const FaceCondition& f, double area, double halfWidth, double k)
{
    switch (f.kind) {
    case FaceKind::Adiabatic: return 0.0;
    case FaceKind::Temperature: return area / (halfWidth / k);
    case FaceKind::Convective: return f.heatTransferCoefficient > 0.0 ? area / (halfWidth / k + 1.0 / f.heatTransferCoefficient) : 0.0;
    }
    return 0.0;
}

double boundaryTemperature(const FaceCondition& f)
{
    return f.kind == FaceKind::Temperature ? f.temperature : f.ambient;
}

struct Assembly {
    Eigen::SparseMatrix<double> conduction;  // K (W/K)
    Eigen::VectorXd load;                    // sources and boundary temperatures (W)
    Eigen::VectorXd capacity;                // C V (J/K)
    // Boundary terms: (cell, face, conductance, temperature).
    struct Boundary {
        size_t cell;
        int face;
        double g, t;
    };
    std::vector<Boundary> boundary;
    double sourcePower = 0.0;
};

Assembly assemble(const ConductionProblem& p)
{
    const CellGrid& g = p.grid;
    const size_t n = g.cellCount();
    const size_t nx = g.count(0), ny = g.count(1), nz = g.count(2);
    Assembly a;
    a.load = Eigen::VectorXd::Zero(static_cast<Eigen::Index>(n));
    a.capacity = Eigen::VectorXd::Zero(static_cast<Eigen::Index>(n));
    std::vector<Eigen::Triplet<double>> t;
    t.reserve(n * 7);
    const std::array<size_t, 3> counts{nx, ny, nz};
    for (size_t k = 0; k < nz; ++k)
        for (size_t j = 0; j < ny; ++j)
            for (size_t i = 0; i < nx; ++i) {
                const size_t c = g.index(i, j, k);
                const std::array<size_t, 3> ijk{i, j, k};
                const double v = g.volume(i, j, k);
                if (!p.source.empty()) {
                    a.load(static_cast<Eigen::Index>(c)) += p.source[c] * v;
                    a.sourcePower += p.source[c] * v;
                }
                if (!p.volumetricHeatCapacity.empty())
                    a.capacity(static_cast<Eigen::Index>(c)) = p.volumetricHeatCapacity[c] * v;
                for (int axis = 0; axis < 3; ++axis) {
                    const size_t ia = ijk[static_cast<size_t>(axis)];
                    const double w = g.width(axis, ia);
                    const int o1 = (axis + 1) % 3, o2 = (axis + 2) % 3;
                    const double area = g.width(o1, ijk[static_cast<size_t>(o1)]) * g.width(o2, ijk[static_cast<size_t>(o2)]);
                    // Towards the next cell along +axis (each internal face once).
                    if (ia + 1 < counts[static_cast<size_t>(axis)]) {
                        std::array<size_t, 3> next = ijk;
                        ++next[static_cast<size_t>(axis)];
                        const size_t cn = g.index(next[0], next[1], next[2]);
                        const double wn = g.width(axis, ia + 1);
                        const double cond = area / (w / (2 * p.conductivity[c]) + wn / (2 * p.conductivity[cn]));
                        t.emplace_back(static_cast<int>(c), static_cast<int>(c), cond);
                        t.emplace_back(static_cast<int>(cn), static_cast<int>(cn), cond);
                        t.emplace_back(static_cast<int>(c), static_cast<int>(cn), -cond);
                        t.emplace_back(static_cast<int>(cn), static_cast<int>(c), -cond);
                    }
                    for (int side = 0; side < 2; ++side) {
                        const bool atFace = side == 0 ? ia == 0 : ia + 1 == counts[static_cast<size_t>(axis)];
                        if (!atFace)
                            continue;
                        const int face = 2 * axis + side;
                        const FaceCondition& f = p.faces[static_cast<size_t>(face)];
                        const double gb = boundaryConductance(f, area, w / 2, p.conductivity[c]);
                        if (gb <= 0.0)
                            continue;
                        const double tb = boundaryTemperature(f);
                        t.emplace_back(static_cast<int>(c), static_cast<int>(c), gb);
                        a.load(static_cast<Eigen::Index>(c)) += gb * tb;
                        a.boundary.push_back({c, face, gb, tb});
                    }
                }
            }
    a.conduction.resize(static_cast<Eigen::Index>(n), static_cast<Eigen::Index>(n));
    a.conduction.setFromTriplets(t.begin(), t.end());
    return a;
}

std::array<double, 6> faceFlows(const Assembly& a, const Eigen::VectorXd& temperature)
{
    std::array<double, 6> out{};
    for (const auto& b : a.boundary)
        out[static_cast<size_t>(b.face)] += b.g * (temperature(static_cast<Eigen::Index>(b.cell)) - b.t);
    return out;
}

std::vector<double> toVector(const Eigen::VectorXd& v)
{
    return {v.data(), v.data() + v.size()};
}

}  // namespace

std::string_view toString(Face face)
{
    static constexpr std::string_view names[] = {"-x", "+x", "-y", "+y", "-z", "+z"};
    return names[static_cast<int>(face)];
}

std::string_view toString(FaceKind kind)
{
    switch (kind) {
    case FaceKind::Adiabatic: return "adiabatic";
    case FaceKind::Temperature: return "temperature";
    case FaceKind::Convective: return "convective";
    }
    return "adiabatic";
}

double CellGrid::faceArea(int axis) const
{
    const int o1 = (axis + 1) % 3, o2 = (axis + 2) % 3;
    const auto& e1 = edges[static_cast<size_t>(o1)];
    const auto& e2 = edges[static_cast<size_t>(o2)];
    return (e1.back() - e1.front()) * (e2.back() - e2.front());
}

Status CellGrid::validate() const
{
    for (int axis = 0; axis < 3; ++axis) {
        const auto& e = edges[static_cast<size_t>(axis)];
        if (e.size() < 2)
            return Status::failure("the grid needs at least one cell along every axis");
        for (size_t i = 0; i + 1 < e.size(); ++i)
            if (!(e[i + 1] > e[i]) || !std::isfinite(e[i + 1]))
                return Status::failure("grid edges must be finite and strictly ascending");
    }
    return Status::success();
}

CellGrid CellGrid::uniform(const Vec3& origin, const Vec3& size, size_t nx, size_t ny, size_t nz)
{
    CellGrid g;
    const std::array<size_t, 3> n{nx, ny, nz};
    for (int axis = 0; axis < 3; ++axis) {
        auto& e = g.edges[static_cast<size_t>(axis)];
        const size_t m = std::max<size_t>(1, n[static_cast<size_t>(axis)]);
        e.resize(m + 1);
        for (size_t i = 0; i <= m; ++i)
            e[i] = origin[axis] + size[axis] * static_cast<double>(i) / static_cast<double>(m);
    }
    return g;
}

Status ConductionProblem::validate(bool transient) const
{
    if (Status s = grid.validate(); !s)
        return s;
    const size_t n = grid.cellCount();
    if (conductivity.size() != n)
        return Status::failure("one conductivity per cell is needed");
    for (double k : conductivity)
        if (!(k > 0.0) || !std::isfinite(k))
            return Status::failure("conductivities must be positive and finite");
    if (!source.empty() && source.size() != n)
        return Status::failure("the source needs one value per cell");
    for (double q : source)
        if (!std::isfinite(q))
            return Status::failure("the heat source contains a value that is not finite");
    for (const FaceCondition& f : faces)
        if (f.kind == FaceKind::Convective && f.heatTransferCoefficient < 0.0)
            return Status::failure("a heat transfer coefficient cannot be negative");
    if (transient) {
        if (volumetricHeatCapacity.size() != n)
            return Status::failure("a transient solution needs a heat capacity per cell");
        for (double c : volumetricHeatCapacity)
            if (!(c > 0.0))
                return Status::failure("heat capacities must be positive");
        if (!initialTemperature.empty() && initialTemperature.size() != n)
            return Status::failure("the initial temperature needs one value per cell");
    } else {
        const bool anchored = std::any_of(faces.begin(), faces.end(), [](const FaceCondition& f) {
            return f.kind == FaceKind::Temperature || (f.kind == FaceKind::Convective && f.heatTransferCoefficient > 0.0);
        });
        if (!anchored)
            return Status::failure("a steady state needs at least one face at a fixed temperature or with convection: with every face "
                                   "adiabatic the temperature is undetermined (and grows without bound under a source)");
    }
    return Status::success();
}

double ConductionSolution::heatOut() const
{
    return std::accumulate(faceHeatOut.begin(), faceHeatOut.end(), 0.0);
}

double ConductionSolution::minTemperature() const
{
    return temperature.empty() ? 0.0 : *std::min_element(temperature.begin(), temperature.end());
}

double ConductionSolution::maxTemperature() const
{
    return temperature.empty() ? 0.0 : *std::max_element(temperature.begin(), temperature.end());
}

Result<ConductionSolution> solveSteady(const ConductionProblem& problem)
{
    if (Status s = problem.validate(false); !s)
        return Result<ConductionSolution>::failure(s.message());
    const Assembly a = assemble(problem);
    Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> solver(a.conduction);
    if (solver.info() != Eigen::Success)
        return Result<ConductionSolution>::failure("the conduction matrix could not be factorized");
    const Eigen::VectorXd temperature = solver.solve(a.load);
    if (solver.info() != Eigen::Success || !temperature.allFinite())
        return Result<ConductionSolution>::failure("the steady conduction solve failed");
    ConductionSolution out;
    out.temperature = toVector(temperature);
    out.faceHeatOut = faceFlows(a, temperature);
    out.sourcePower = a.sourcePower;
    out.balanceResidual = out.sourcePower - out.heatOut();
    out.method = "finite volumes on a rectilinear grid, steady state, sparse LDLT";
    return out;
}

Result<ConductionSolution> solveTransient(const ConductionProblem& problem, const TransientSettings& settings,
                                          const std::function<bool()>& cancelled)
{
    if (Status s = problem.validate(true); !s)
        return Result<ConductionSolution>::failure(s.message());
    if (!(settings.duration > 0.0) || settings.steps < 1)
        return Result<ConductionSolution>::failure("a transient solution needs a positive duration and at least one step");
    const Assembly a = assemble(problem);
    const double dt = settings.duration / settings.steps;
    const Eigen::Index n = static_cast<Eigen::Index>(problem.grid.cellCount());
    Eigen::SparseMatrix<double> system = a.conduction;
    for (Eigen::Index c = 0; c < n; ++c)
        system.coeffRef(c, c) += a.capacity(c) / dt;
    Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> solver(system);
    if (solver.info() != Eigen::Success)
        return Result<ConductionSolution>::failure("the transient conduction matrix could not be factorized");
    Eigen::VectorXd temperature(n);
    for (Eigen::Index c = 0; c < n; ++c)
        temperature(c) = problem.initialTemperature.empty() ? problem.referenceTemperature : problem.initialTemperature[static_cast<size_t>(c)];
    const Eigen::VectorXd start = temperature;
    ConductionSolution out;
    out.transient = true;
    out.duration = settings.duration;
    const int every = std::max(1, settings.steps / std::max(1, settings.snapshots));
    out.times.push_back(0.0);
    out.snapshots.push_back(toVector(temperature));
    for (int step = 1; step <= settings.steps; ++step) {
        if (cancelled && cancelled())
            return Result<ConductionSolution>::failure("cancelled");
        const Eigen::VectorXd rhs = a.load + (a.capacity.array() * temperature.array() / dt).matrix();
        temperature = solver.solve(rhs);
        if (!temperature.allFinite())
            return Result<ConductionSolution>::failure("the transient conduction solve produced values that are not finite");
        const auto flows = faceFlows(a, temperature);
        for (size_t f = 0; f < 6; ++f)
            out.faceHeatOut[f] += flows[f] * dt;
        if (step % every == 0 || step == settings.steps) {
            out.times.push_back(step * dt);
            out.snapshots.push_back(toVector(temperature));
        }
    }
    out.temperature = toVector(temperature);
    out.sourcePower = a.sourcePower * settings.duration;
    out.storedEnergyChange = (a.capacity.array() * (temperature - start).array()).sum();
    out.balanceResidual = out.sourcePower - out.storedEnergyChange - out.heatOut();
    out.method = "finite volumes on a rectilinear grid, implicit Euler, sparse LDLT";
    return out;
}

}  // namespace resonant::thermal
