#include "fdtd/FdtdGrid.h"

#include <algorithm>
#include <cmath>

namespace resonant::fdtd {
namespace {

constexpr double pi = 3.14159265358979323846;

struct BoundaryName {
    BoundaryKind kind;
    std::string_view name;
};
constexpr BoundaryName kBoundaryNames[] = {{BoundaryKind::Rigid, "rigid"},
                                           {BoundaryKind::PressureRelease, "pressureRelease"},
                                           {BoundaryKind::Impedance, "impedance"},
                                           {BoundaryKind::Absorbing, "absorbing"}};

}  // namespace

std::string_view toString(BoundaryKind kind)
{
    for (const BoundaryName& n : kBoundaryNames)
        if (n.kind == kind)
            return n.name;
    return "rigid";
}

Result<BoundaryKind> boundaryKindFromString(std::string_view text)
{
    for (const BoundaryName& n : kBoundaryNames)
        if (n.name == text)
            return n.kind;
    return Result<BoundaryKind>::failure("unknown boundary condition '" + std::string(text) + "'");
}

std::array<int, 3> GridSpec::totalCells() const
{
    const auto pad = [this](Face face) { return boundary[face] == BoundaryKind::Absorbing ? pmlCells : 0; };
    return {nx + pad(XMin) + pad(XMax), ny + pad(YMin) + pad(YMax), is2D() ? 1 : nz + pad(ZMin) + pad(ZMax)};
}

double GridSpec::maxSoundSpeed() const
{
    std::vector<bool> used(materials.size(), false);
    if (!used.empty())
        used[0] = true;  // the background; with no map it is the only material in use
    for (const std::uint8_t m : cellMaterial)
        if (m < used.size())
            used[m] = true;
    double fastest = 0.0;
    for (size_t m = 0; m < materials.size(); ++m)
        if (used[m] && materials[m].isFluid())
            fastest = std::max(fastest, materials[m].soundSpeed);
    return fastest;
}

double GridSpec::minSoundSpeed() const
{
    std::vector<bool> used(materials.size(), false);
    if (!used.empty())
        used[0] = true;  // the background; with no map it is the only material in use
    for (const std::uint8_t m : cellMaterial)
        if (m < used.size())
            used[m] = true;
    double slowest = 0.0;
    for (size_t m = 0; m < materials.size(); ++m)
        if (used[m] && materials[m].isFluid() && (slowest == 0.0 || materials[m].soundSpeed < slowest))
            slowest = materials[m].soundSpeed;
    return slowest;
}

size_t GridSpec::stateBytes() const
{
    const std::array<int, 3> n = totalCells();
    const size_t cells = static_cast<size_t>(n[0]) * static_cast<size_t>(n[1]) * static_cast<size_t>(n[2]);
    // Pressure, its split parts and the velocity components (doubles), and a
    // material index per cell.
    const size_t fields = is2D() ? 5 : 7;
    return cells * (fields * sizeof(double) + 1);
}

Status GridSpec::validate() const
{
    if (nx < 2 || ny < 2 || nz < 1)
        return Status::failure("the grid needs at least 2 x 2 cells");
    if (nz == 2)
        return Status::failure("a three-dimensional grid needs at least 3 cells along its third direction; use 1 for a two-dimensional grid");
    if (!(std::isfinite(dx) && dx > 0.0))
        return Status::failure("the cell size must be positive");
    if (materials.empty() || materials.size() > 255)
        return Status::failure("the grid needs between 1 and 255 materials");
    if (!cellMaterial.empty() && cellMaterial.size() != interiorCells())
        return Status::failure("the material map does not match the grid");
    for (const std::uint8_t m : cellMaterial)
        if (m >= materials.size())
            return Status::failure("the material map refers to a material that does not exist");
    for (const CellMaterial& m : materials) {
        if (!m.isFluid())
            continue;
        if (!(std::isfinite(m.density) && m.density > 0.0) || !(std::isfinite(m.soundSpeed) && m.soundSpeed > 0.0))
            return Status::failure("material '" + m.name + "' needs a positive density and speed of sound");
        if (!(std::isfinite(m.attenuation) && m.attenuation >= 0.0))
            return Status::failure("material '" + m.name + "' has a negative attenuation");
    }
    if (!materials[0].isFluid())
        return Status::failure("the background medium must be a fluid");
    for (int face = 0; face < 6; ++face) {
        if (is2D() && face >= ZMin)
            break;
        if (boundary[static_cast<size_t>(face)] == BoundaryKind::Impedance
            && !(std::isfinite(impedance[static_cast<size_t>(face)]) && impedance[static_cast<size_t>(face)] > 0.0))
            return Status::failure("an impedance boundary needs a positive impedance");
        if (boundary[static_cast<size_t>(face)] == BoundaryKind::Absorbing && (pmlCells < 4 || pmlCells > 200))
            return Status::failure("an absorbing layer must be between 4 and 200 cells thick");
    }
    if (!(pmlTheoreticalReflection > 0.0 && pmlTheoreticalReflection < 1.0))
        return Status::failure("the design reflection of the absorbing layer must be between 0 and 1");
    if (!(std::isfinite(dt) && dt > 0.0))
        return Status::failure("the time step must be positive");
    // The Courant-Friedrichs-Lewy condition of this scheme.
    const double limit = stabilityLimit(dx, maxSoundSpeed(), dimensions());
    if (dt > limit * (1.0 + 1e-12))
        return Status::failure("the time step " + std::to_string(dt) + " s exceeds the stability limit of the scheme, "
                               + std::to_string(limit) + " s (dx / (c_max sqrt(" + std::to_string(dimensions())
                               + "))): the calculation would grow without bound");
    return Status::success();
}

double stabilityLimit(double dx, double maxSoundSpeed, int dimensions)
{
    return dx / (maxSoundSpeed * std::sqrt(static_cast<double>(dimensions)));
}

double axialPhaseVelocityRatio(double pointsPerWavelength, double courant)
{
    // With S = c dt / dx and N = lambda / dx for the TRUE wavelength at the
    // driving frequency, w dt / 2 = pi S / N and the numerical wavenumber
    // satisfies sin(k dx / 2) = sin(pi S / N) / S. Then
    //   v_p / c = w / (c k) = (pi / N) / asin(sin(pi S / N) / S).
    const double x = pi / pointsPerWavelength;
    return x / std::asin(std::min(1.0, std::sin(courant * x) / courant));
}

Result<std::unique_ptr<FdtdSolver>> FdtdSolver::create(GridSpec spec)
{
    if (const Status status = spec.validate(); !status)
        return Result<std::unique_ptr<FdtdSolver>>::failure(status.message());
    const int threads = compute::resolveThreads(spec.compute.threads);
    auto solver = std::unique_ptr<FdtdSolver>(new FdtdSolver(std::move(spec)));
    solver->pool_ = std::make_unique<compute::ThreadPool>(threads);
    return solver;
}

FdtdSolver::FdtdSolver(GridSpec spec) : spec_(std::move(spec))
{
    const std::array<int, 3> total = spec_.totalCells();
    nx_ = total[0];
    ny_ = total[1];
    nz_ = total[2];
    const auto pad = [this](Face face) { return spec_.boundary[face] == BoundaryKind::Absorbing ? spec_.pmlCells : 0; };
    padLo_ = {pad(XMin), pad(YMin), spec_.is2D() ? 0 : pad(ZMin)};

    const size_t cells = static_cast<size_t>(nx_) * static_cast<size_t>(ny_) * static_cast<size_t>(nz_);
    // Materials: the interior map, continued outward into the absorbing
    // layers so that an interface runs on into the layer.
    material_.assign(cells, 0);
    if (!spec_.cellMaterial.empty()) {
        for (int k = 0; k < nz_; ++k)
            for (int j = 0; j < ny_; ++j)
                for (int i = 0; i < nx_; ++i) {
                    const int ii = std::clamp(i - padLo_[0], 0, spec_.nx - 1);
                    const int jj = std::clamp(j - padLo_[1], 0, spec_.ny - 1);
                    const int kk = std::clamp(k - padLo_[2], 0, spec_.nz - 1);
                    material_[index(i, j, k)] = spec_.cellMaterial[spec_.interiorIndex(ii, jj, kk)];
                }
    }

    p_.assign(cells, 0.0);
    px_.assign(cells, 0.0);
    py_.assign(cells, 0.0);
    vx_.assign(static_cast<size_t>(nx_ + 1) * static_cast<size_t>(ny_) * static_cast<size_t>(nz_), 0.0);
    vy_.assign(static_cast<size_t>(nx_) * static_cast<size_t>(ny_ + 1) * static_cast<size_t>(nz_), 0.0);
    if (!spec_.is2D()) {
        pz_.assign(cells, 0.0);
        vz_.assign(static_cast<size_t>(nx_) * static_cast<size_t>(ny_) * static_cast<size_t>(nz_ + 1), 0.0);
    }

    const size_t count = spec_.materials.size();
    kappa_.assign(count, 0.0);
    loss_.assign(count, 1.0);
    cellInverseDensity_.assign(count, 0.0);
    faceCoefficient_.assign(count * count, 0.0);
    for (size_t a = 0; a < count; ++a) {
        const CellMaterial& ma = spec_.materials[a];
        if (ma.isFluid()) {
            kappa_[a] = ma.bulkModulus() * spec_.dt / spec_.dx;
            // p_tt + gamma p_t = c^2 lap p attenuates a plane wave by
            // gamma / (2 c) nepers per metre, so gamma = 2 alpha c.
            loss_[a] = std::exp(-2.0 * ma.attenuation * ma.soundSpeed * spec_.dt);
            cellInverseDensity_[a] = spec_.dt / (spec_.dx * ma.density);
        }
        for (size_t b = 0; b < count; ++b) {
            const CellMaterial& mb = spec_.materials[b];
            if (ma.rigid || mb.rigid)
                continue;  // no motion through the face of a rigid cell
            // A pressure-release cell has no inertia of its own: the face
            // sees the fluid's density on the fluid side only.
            double density = 0.0;
            if (ma.isFluid() && mb.isFluid())
                density = 0.5 * (ma.density + mb.density);
            else if (ma.isFluid())
                density = 0.5 * ma.density;
            else if (mb.isFluid())
                density = 0.5 * mb.density;
            else
                continue;
            faceCoefficient_[a * count + b] = spec_.dt / (spec_.dx * density);
        }
    }

    // Absorbing layers: sigma(d) = sigma_max (d / L)^2, with sigma_max chosen
    // for the design reflection R0 at normal incidence,
    //   R0 = exp(-2 sigma_max L / (3 c))   =>   sigma_max = -3 c ln(R0) / (2 L).
    const double c = spec_.maxSoundSpeed();
    const double thickness = spec_.pmlCells * spec_.dx;
    const double sigmaMax = -3.0 * c * std::log(spec_.pmlTheoreticalReflection) / (2.0 * thickness);
    const auto profile = [&](double depth) {
        if (depth <= 0.0)
            return 1.0;
        const double f = std::min(depth / thickness, 1.0);
        return std::exp(-0.5 * sigmaMax * f * f * spec_.dt);
    };
    const int n[3] = {nx_, ny_, nz_};
    const Face lo[3] = {XMin, YMin, ZMin};
    const Face hi[3] = {XMax, YMax, ZMax};
    for (int axis = 0; axis < 3; ++axis) {
        decayCell_[static_cast<size_t>(axis)].assign(static_cast<size_t>(n[axis]), 1.0);
        decayFace_[static_cast<size_t>(axis)].assign(static_cast<size_t>(n[axis]) + 1, 1.0);
        if (axis == 2 && spec_.is2D())
            continue;
        const bool absorbLo = spec_.boundary[lo[axis]] == BoundaryKind::Absorbing;
        const bool absorbHi = spec_.boundary[hi[axis]] == BoundaryKind::Absorbing;
        const double loEdge = absorbLo ? spec_.pmlCells * spec_.dx : 0.0;  // position where the low layer ends
        const double hiEdge = (n[axis] - (absorbHi ? spec_.pmlCells : 0)) * spec_.dx;
        for (int i = 0; i < n[axis]; ++i) {
            const double x = (i + 0.5) * spec_.dx;
            double depth = 0.0;
            if (absorbLo && x < loEdge)
                depth = loEdge - x;
            if (absorbHi && x > hiEdge)
                depth = x - hiEdge;
            decayCell_[static_cast<size_t>(axis)][static_cast<size_t>(i)] = profile(depth);
        }
        for (int i = 0; i <= n[axis]; ++i) {
            const double x = i * spec_.dx;
            double depth = 0.0;
            if (absorbLo && x < loEdge)
                depth = loEdge - x;
            if (absorbHi && x > hiEdge)
                depth = x - hiEdge;
            decayFace_[static_cast<size_t>(axis)][static_cast<size_t>(i)] = profile(depth);
        }
    }
}

void FdtdSolver::addSource(const GridSource& source)
{
    const size_t cell = index(source.i + padLo_[0], source.j + padLo_[1], source.k + padLo_[2]);
    const CellMaterial& material = spec_.materials[material_[cell]];
    if (!material.isFluid())
        return;  // a source inside a rigid body radiates nothing
    const double cellVolume = spec_.is2D() ? spec_.dx * spec_.dx : spec_.dx * spec_.dx * spec_.dx;
    sources_.push_back({cell, source.strength * material.bulkModulus() * spec_.dt / cellVolume, source.delay, source.waveform, 0.0, 0.0});
}

void FdtdSolver::updateVelocity()
{
    const size_t count = spec_.materials.size();
    const auto& ax = decayFace_[0];
    const auto& ay = decayFace_[1];
    const auto& az = decayFace_[2];

    // Boundary faces. `inside` is the pressure of the adjacent cell and
    // `sign` is +1 on a maximum face (outward normal along +axis).
    const auto boundaryFace = [&](double& v, Face face, size_t cell, double sign) {
        const std::uint8_t m = material_[cell];
        const double b = cellInverseDensity_[m];  // dt / (dx rho); 0 for a non-fluid cell
        switch (spec_.boundary[face]) {
        case BoundaryKind::Rigid:
        case BoundaryKind::Absorbing:
            v = 0.0;
            break;
        case BoundaryKind::PressureRelease:
            // p = 0 at the face, half a cell from the cell centre.
            v += sign * 2.0 * b * p_[cell];
            break;
        case BoundaryKind::Impedance: {
            // rho dv/dt = -(Z v - p) / (dx / 2), v the outward velocity,
            // with Z v taken as the mean of the old and the new value.
            const double beta = b * spec_.impedance[face];
            const double outward = sign * v;
            v = sign * ((1.0 - beta) * outward + 2.0 * b * p_[cell]) / (1.0 + beta);
            break;
        }
        }
    };

    // The passes split their rows across the pool's threads; every face is
    // computed exactly as in a single-threaded pass.
    const size_t rowsXY = static_cast<size_t>(ny_) * static_cast<size_t>(nz_);
    // vx
    pool_->run(rowsXY, [&](size_t begin, size_t end) {
        for (size_t row = begin; row < end; ++row) {
            const int j = static_cast<int>(row % static_cast<size_t>(ny_)), k = static_cast<int>(row / static_cast<size_t>(ny_));
            const size_t rowP = index(0, j, k);
            const size_t rowV = static_cast<size_t>(nx_ + 1) * (static_cast<size_t>(j) + static_cast<size_t>(ny_) * static_cast<size_t>(k));
            for (int i = 1; i < nx_; ++i) {
                const size_t c = rowP + static_cast<size_t>(i);
                const double b = faceCoefficient_[material_[c - 1] * count + material_[c]];
                const double a = ax[static_cast<size_t>(i)];
                double& v = vx_[rowV + static_cast<size_t>(i)];
                v = a * (a * v - b * (p_[c] - p_[c - 1]));
            }
            boundaryFace(vx_[rowV], XMin, rowP, -1.0);
            boundaryFace(vx_[rowV + static_cast<size_t>(nx_)], XMax, rowP + static_cast<size_t>(nx_ - 1), 1.0);
        }
    });
    // vy
    const size_t strideY = static_cast<size_t>(nx_);
    const size_t interiorRowsY = static_cast<size_t>(std::max(0, ny_ - 1)) * static_cast<size_t>(nz_);
    pool_->run(interiorRowsY, [&](size_t begin, size_t end) {
        for (size_t row = begin; row < end; ++row) {
            const int j = 1 + static_cast<int>(row % static_cast<size_t>(ny_ - 1)), k = static_cast<int>(row / static_cast<size_t>(ny_ - 1));
            const size_t slabV = static_cast<size_t>(nx_) * static_cast<size_t>(ny_ + 1) * static_cast<size_t>(k);
            const double a = ay[static_cast<size_t>(j)];
            const size_t rowP = index(0, j, k);
            const size_t rowV = slabV + static_cast<size_t>(nx_) * static_cast<size_t>(j);
            for (int i = 0; i < nx_; ++i) {
                const size_t c = rowP + static_cast<size_t>(i);
                const double b = faceCoefficient_[material_[c - strideY] * count + material_[c]];
                double& v = vy_[rowV + static_cast<size_t>(i)];
                v = a * (a * v - b * (p_[c] - p_[c - strideY]));
            }
        }
    });
    pool_->run(static_cast<size_t>(nz_), [&](size_t begin, size_t end) {
        for (size_t kk = begin; kk < end; ++kk) {
            const int k = static_cast<int>(kk);
            const size_t slabV = static_cast<size_t>(nx_) * static_cast<size_t>(ny_ + 1) * static_cast<size_t>(k);
            for (int i = 0; i < nx_; ++i) {
                boundaryFace(vy_[slabV + static_cast<size_t>(i)], YMin, index(i, 0, k), -1.0);
                boundaryFace(vy_[slabV + static_cast<size_t>(nx_) * static_cast<size_t>(ny_) + static_cast<size_t>(i)], YMax, index(i, ny_ - 1, k), 1.0);
            }
        }
    });
    // vz
    if (!spec_.is2D()) {
        const size_t strideZ = static_cast<size_t>(nx_) * static_cast<size_t>(ny_);
        const size_t interiorRowsZ = static_cast<size_t>(std::max(0, nz_ - 1)) * static_cast<size_t>(ny_);
        pool_->run(interiorRowsZ, [&](size_t begin, size_t end) {
            for (size_t row = begin; row < end; ++row) {
                const int j = static_cast<int>(row % static_cast<size_t>(ny_)), k = 1 + static_cast<int>(row / static_cast<size_t>(ny_));
                const double a = az[static_cast<size_t>(k)];
                const size_t first = index(0, j, k);
                for (size_t c = first, last = first + static_cast<size_t>(nx_); c < last; ++c) {
                    const double b = faceCoefficient_[material_[c - strideZ] * count + material_[c]];
                    double& v = vz_[c];
                    v = a * (a * v - b * (p_[c] - p_[c - strideZ]));
                }
            }
        });
        pool_->run(strideZ, [&](size_t begin, size_t end) {
            for (size_t c = begin; c < end; ++c) {
                boundaryFace(vz_[c], ZMin, c, -1.0);
                boundaryFace(vz_[strideZ * static_cast<size_t>(nz_) + c], ZMax, strideZ * static_cast<size_t>(nz_ - 1) + c, 1.0);
            }
        });
    }
}

void FdtdSolver::updatePressure()
{
    const auto& ax = decayCell_[0];
    const auto& ay = decayCell_[1];
    const auto& az = decayCell_[2];
    const bool threeD = !spec_.is2D();
    const size_t strideZ = static_cast<size_t>(nx_) * static_cast<size_t>(ny_);
    const size_t rows = static_cast<size_t>(ny_) * static_cast<size_t>(nz_);
    pool_->run(rows, [&](size_t begin, size_t end) {
        for (size_t row = begin; row < end; ++row) {
            const int j = static_cast<int>(row % static_cast<size_t>(ny_)), k = static_cast<int>(row / static_cast<size_t>(ny_));
            const double dz = az[static_cast<size_t>(k)];
            const double dy = ay[static_cast<size_t>(j)];
            const size_t rowP = index(0, j, k);
            const size_t rowVx = static_cast<size_t>(nx_ + 1) * (static_cast<size_t>(j) + static_cast<size_t>(ny_) * static_cast<size_t>(k));
            const size_t rowVy = static_cast<size_t>(nx_) * (static_cast<size_t>(j) + static_cast<size_t>(ny_ + 1) * static_cast<size_t>(k));
            for (int i = 0; i < nx_; ++i) {
                const size_t c = rowP + static_cast<size_t>(i);
                const std::uint8_t m = material_[c];
                const double kappa = kappa_[m];
                if (kappa == 0.0)
                    continue;  // rigid or pressure-release cell: p stays zero
                const double loss = loss_[m];
                const double dxf = ax[static_cast<size_t>(i)];
                const size_t ix = rowVx + static_cast<size_t>(i);
                const size_t iy = rowVy + static_cast<size_t>(i);
                px_[c] = loss * dxf * (dxf * px_[c] - kappa * (vx_[ix + 1] - vx_[ix]));
                py_[c] = loss * dy * (dy * py_[c] - kappa * (vy_[iy + static_cast<size_t>(nx_)] - vy_[iy]));
                double sum = px_[c] + py_[c];
                if (threeD) {
                    pz_[c] = loss * dz * (dz * pz_[c] - kappa * (vz_[c + strideZ] - vz_[c]));
                    sum += pz_[c];
                }
                p_[c] = sum;
            }
        }
    });
}

void FdtdSolver::applySources()
{
    // Pressure is now at time (step_ + 1) dt. The source term K q dt uses the
    // volume velocity at the middle of the step, Q(t + dt / 2), obtained by
    // integrating the volume acceleration with the trapezoidal rule.
    const double tMid = (static_cast<double>(step_) + 0.5) * spec_.dt;
    for (ActiveSource& s : sources_) {
        const double w = s.waveform.value(tMid - s.delay);
        s.integral += 0.5 * (w + s.previous) * spec_.dt;
        s.previous = w;
        px_[s.cell] += s.scale * s.integral;
        p_[s.cell] += s.scale * s.integral;
    }
}

void FdtdSolver::step()
{
    updateVelocity();  // v from t - dt/2 to t + dt/2, using p(t)
    updatePressure();  // p from t to t + dt, using v(t + dt/2)
    applySources();
    ++step_;
}

std::array<double, 3> FdtdSolver::velocity(int i, int j, int k) const
{
    const int ii = i + padLo_[0], jj = j + padLo_[1], kk = k + padLo_[2];
    const size_t ix = static_cast<size_t>(ii) + static_cast<size_t>(nx_ + 1) * (static_cast<size_t>(jj) + static_cast<size_t>(ny_) * static_cast<size_t>(kk));
    const size_t iy = static_cast<size_t>(ii) + static_cast<size_t>(nx_) * (static_cast<size_t>(jj) + static_cast<size_t>(ny_ + 1) * static_cast<size_t>(kk));
    std::array<double, 3> v{0.5 * (vx_[ix] + vx_[ix + 1]), 0.5 * (vy_[iy] + vy_[iy + static_cast<size_t>(nx_)]), 0.0};
    if (!spec_.is2D()) {
        const size_t c = index(ii, jj, kk);
        v[2] = 0.5 * (vz_[c] + vz_[c + static_cast<size_t>(nx_) * static_cast<size_t>(ny_)]);
    }
    return v;
}

double FdtdSolver::energy() const
{
    // A partial sum per row (j, k), added in row order: the same value for
    // any number of threads (and on the CUDA backend).
    const double cellVolume = spec_.is2D() ? spec_.dx * spec_.dx : spec_.dx * spec_.dx * spec_.dx;
    const size_t count = spec_.materials.size();
    // Kinetic energy. The stored velocity is half a step BEFORE the stored
    // pressure. The leapfrog scheme conserves p(t)^2 / K together with
    // rho v(t - dt/2) . v(t + dt/2) exactly, and
    // v(t + dt/2) = v(t - dt/2) - coefficient * (difference of p(t)) needs
    // no stored history. (Inside an absorbing layer this is approximate.)
    const auto faceEnergy = [&](double v, size_t lower, size_t upper) {
        const double coefficient = faceCoefficient_[material_[lower] * count + material_[upper]];
        if (coefficient <= 0.0)
            return 0.0;
        const double density = spec_.dt / (spec_.dx * coefficient);
        return density * v * (v - coefficient * (p_[upper] - p_[lower]));
    };
    const size_t strideZ = static_cast<size_t>(nx_) * static_cast<size_t>(ny_);
    const size_t rows = static_cast<size_t>(ny_) * static_cast<size_t>(nz_);
    std::vector<double> partial(rows, 0.0);
    pool_->run(rows, [&](size_t begin, size_t end) {
        for (size_t row = begin; row < end; ++row) {
            const int j = static_cast<int>(row % static_cast<size_t>(ny_)), k = static_cast<int>(row / static_cast<size_t>(ny_));
            double potential = 0.0, kinetic = 0.0;
            for (int i = 0; i < nx_; ++i) {
                const size_t c = index(i, j, k);
                const CellMaterial& m = spec_.materials[material_[c]];
                if (m.isFluid())
                    potential += p_[c] * p_[c] / m.bulkModulus();
                if (i >= 1)
                    kinetic += faceEnergy(vx_[static_cast<size_t>(i) + static_cast<size_t>(nx_ + 1) * (static_cast<size_t>(j) + static_cast<size_t>(ny_) * static_cast<size_t>(k))],
                                          c - 1, c);
                if (j >= 1)
                    kinetic += faceEnergy(vy_[static_cast<size_t>(i) + static_cast<size_t>(nx_) * (static_cast<size_t>(j) + static_cast<size_t>(ny_ + 1) * static_cast<size_t>(k))],
                                          c - static_cast<size_t>(nx_), c);
                if (!spec_.is2D() && k >= 1)
                    kinetic += faceEnergy(vz_[c], c - strideZ, c);
            }
            partial[row] = potential + kinetic;
        }
    });
    double total = 0.0;
    for (const double p : partial)
        total += p;
    return 0.5 * cellVolume * total;
}

FdtdDeviceLayout FdtdSolver::deviceLayout() const
{
    FdtdDeviceLayout l;
    l.nx = nx_;
    l.ny = ny_;
    l.nz = nz_;
    l.is2D = spec_.is2D() ? 1 : 0;
    for (int a = 0; a < 3; ++a) {
        l.padLo[a] = padLo_[static_cast<size_t>(a)];
        l.decayCell[a] = decayCell_[static_cast<size_t>(a)];
        l.decayFace[a] = decayFace_[static_cast<size_t>(a)];
    }
    l.interior[0] = spec_.nx;
    l.interior[1] = spec_.ny;
    l.interior[2] = spec_.nz;
    l.dx = spec_.dx;
    l.dt = spec_.dt;
    l.material = material_.data();
    l.p = p_.data();
    l.px = px_.data();
    l.py = py_.data();
    l.pz = spec_.is2D() ? nullptr : pz_.data();
    l.vx = vx_.data();
    l.vy = vy_.data();
    l.vz = spec_.is2D() ? nullptr : vz_.data();
    l.kappa = kappa_;
    l.loss = loss_;
    l.cellInverseDensity = cellInverseDensity_;
    l.faceCoefficient = faceCoefficient_;
    for (const CellMaterial& m : spec_.materials)
        l.bulkModulus.push_back(m.isFluid() ? m.bulkModulus() : 0.0);
    for (int f = 0; f < 6; ++f) {
        l.boundary[f] = static_cast<int>(spec_.boundary[static_cast<size_t>(f)]);
        l.impedance[f] = spec_.impedance[static_cast<size_t>(f)];
    }
    for (const ActiveSource& a : sources_)
        l.sourceCell.push_back(a.cell);
    return l;
}

void FdtdSolver::advanceSources(std::vector<double>& increments)
{
    const double tMid = (static_cast<double>(step_) + 0.5) * spec_.dt;
    increments.resize(sources_.size());
    for (size_t i = 0; i < sources_.size(); ++i) {
        ActiveSource& s = sources_[i];
        const double w = s.waveform.value(tMid - s.delay);
        s.integral += 0.5 * (w + s.previous) * spec_.dt;
        s.previous = w;
        increments[i] = s.scale * s.integral;
    }
}

double FdtdSolver::maxAbsPressure() const
{
    // The largest value is the same whatever the order: a partial per row.
    const size_t rows = static_cast<size_t>(spec_.ny) * static_cast<size_t>(spec_.nz);
    std::vector<double> partial(rows, 0.0);
    pool_->run(rows, [&](size_t begin, size_t end) {
        for (size_t row = begin; row < end; ++row) {
            const int j = static_cast<int>(row % static_cast<size_t>(spec_.ny)), k = static_cast<int>(row / static_cast<size_t>(spec_.ny));
            double largest = 0.0;
            for (int i = 0; i < spec_.nx; ++i)
                largest = std::max(largest, std::abs(pressure(i, j, k)));
            partial[row] = largest;
        }
    });
    double largest = 0.0;
    for (const double p : partial)
        largest = std::max(largest, p);
    return largest;
}

bool FdtdSolver::finite() const
{
    // A growing instability reaches every cell quickly; sampling is enough
    // and keeps the check cheap.
    const size_t stride = std::max<size_t>(1, p_.size() / 4096);
    for (size_t c = 0; c < p_.size(); c += stride)
        if (!std::isfinite(p_[c]))
            return false;
    return std::isfinite(p_[p_.size() / 2]);
}

}  // namespace resonant::fdtd
