#include "electromagnetics/Yee.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace resonant::em {

std::string_view toString(YeeBoundary b)
{
    switch (b) {
    case YeeBoundary::PEC: return "pec";
    case YeeBoundary::Periodic: return "periodic";
    case YeeBoundary::Absorbing: return "absorbing";
    }
    return "pec";
}

Result<YeeBoundary> yeeBoundaryFromString(std::string_view text)
{
    for (const YeeBoundary b : {YeeBoundary::PEC, YeeBoundary::Periodic, YeeBoundary::Absorbing})
        if (toString(b) == text)
            return b;
    return Result<YeeBoundary>::failure("unknown boundary '" + std::string(text) + "'");
}

namespace {

std::vector<bool> used(const YeeSpec& s)
{
    std::vector<bool> u(s.materials.size(), s.cellMaterial.empty());
    if (!u.empty())
        u[0] = true;
    for (const std::uint8_t m : s.cellMaterial)
        if (m < u.size())
            u[m] = true;
    return u;
}

}  // namespace

double YeeSpec::maxSpeed() const
{
    const std::vector<bool> u = used(*this);
    double fastest = 0.0;
    for (size_t m = 0; m < materials.size(); ++m)
        if (u[m] && !materials[m].pec)
            fastest = std::max(fastest, kSpeedOfLight / std::sqrt(materials[m].relativePermittivity * materials[m].relativePermeability));
    return fastest;
}

double YeeSpec::minSpeed() const
{
    const std::vector<bool> u = used(*this);
    double slowest = 0.0;
    for (size_t m = 0; m < materials.size(); ++m)
        if (u[m] && !materials[m].pec) {
            const double c = kSpeedOfLight / std::sqrt(materials[m].relativePermittivity * materials[m].relativePermeability);
            slowest = slowest == 0.0 ? c : std::min(slowest, c);
        }
    return slowest;
}

size_t YeeSpec::stateBytes() const
{
    const auto padded = [&](int n, int lo, int hi) {
        return static_cast<size_t>(n + (boundary[static_cast<size_t>(lo)] == YeeBoundary::Absorbing ? pmlCells : 0)
                                   + (boundary[static_cast<size_t>(hi)] == YeeBoundary::Absorbing ? pmlCells : 0) + 1);
    };
    const size_t nodes = padded(nx, 0, 1) * padded(ny, 2, 3) * (is2D() ? 1 : padded(nz, 4, 5));
    // 18 split field parts, 6 E coefficients, 3 H coefficients, one material byte.
    return nodes * (27 * sizeof(double) + 1);
}

Status YeeSpec::validate() const
{
    if (nx < 2 || ny < 2 || nz < 1 || nz == 2)
        return Status::failure("the grid needs at least 2 x 2 cells, and 1 (two-dimensional) or at least 3 cells along z");
    if (!(cell > 0.0 && std::isfinite(cell)))
        return Status::failure("the cell size must be positive");
    if (materials.empty() || materials.size() > 255)
        return Status::failure("the grid needs between 1 and 255 materials");
    if (materials[0].pec)
        return Status::failure("the background cannot be a perfect conductor");
    for (const YeeMaterial& m : materials)
        if (!m.pec && !(m.relativePermittivity >= 1.0 - 1e-12 && m.relativePermeability > 0.0 && m.conductivity >= 0.0))
            return Status::failure("material '" + m.name + "' needs a relative permittivity of at least 1, a positive permeability and a "
                                   "non-negative conductivity");
    if (!cellMaterial.empty() && cellMaterial.size() != static_cast<size_t>(nx) * ny * nz)
        return Status::failure("the material map does not match the grid");
    for (const std::uint8_t m : cellMaterial)
        if (m >= materials.size())
            return Status::failure("the material map refers to a material that does not exist");
    for (int axis = 0; axis < (is2D() ? 2 : 3); ++axis) {
        const bool lo = boundary[static_cast<size_t>(2 * axis)] == YeeBoundary::Periodic;
        const bool hi = boundary[static_cast<size_t>(2 * axis + 1)] == YeeBoundary::Periodic;
        if (lo != hi)
            return Status::failure("a periodic boundary must be set on both faces of an axis");
    }
    if (pmlCells < 4 || pmlCells > 200)
        return Status::failure("an absorbing layer must be between 4 and 200 cells thick");
    if (!(dt > 0.0))
        return Status::failure("the time step must be positive");
    const double limit = yeeStabilityLimit(cell, maxSpeed(), dimensions());
    if (dt > limit * (1.0 + 1e-12))
        return Status::failure("the time step " + std::to_string(dt) + " s exceeds the stability limit of the Yee scheme, " + std::to_string(limit)
                               + " s (d / (c_max sqrt(" + std::to_string(dimensions()) + ")))");
    return Status::success();
}

double yeeStabilityLimit(double cell, double maxSpeed, int dimensions)
{
    return cell / (maxSpeed * std::sqrt(static_cast<double>(dimensions)));
}

Result<std::unique_ptr<YeeSolver>> YeeSolver::create(YeeSpec spec)
{
    if (Status status = spec.validate(); !status)
        return Result<std::unique_ptr<YeeSolver>>::failure(status.message());
    const int threads = compute::resolveThreads(spec.compute.threads);
    auto solver = std::unique_ptr<YeeSolver>(new YeeSolver(std::move(spec)));
    solver->pool_ = std::make_unique<compute::ThreadPool>(threads);
    return solver;
}

int YeeSolver::wrap(int axis, int index) const
{
    const int n = axis == 0 ? px_ : axis == 1 ? py_ : pz_;
    return periodic_[static_cast<size_t>(axis)] ? (index % n + n) % n : index;
}

YeeSolver::YeeSolver(YeeSpec spec) : spec_(std::move(spec))
{
    const int n[3] = {spec_.nx, spec_.ny, spec_.nz};
    int p[3];
    for (int axis = 0; axis < 3; ++axis) {
        if (axis == 2 && spec_.is2D()) {
            p[2] = 1;
            periodic_[2] = true;
            continue;
        }
        periodic_[static_cast<size_t>(axis)] = spec_.boundary[static_cast<size_t>(2 * axis)] == YeeBoundary::Periodic;
        const int lo = spec_.boundary[static_cast<size_t>(2 * axis)] == YeeBoundary::Absorbing ? spec_.pmlCells : 0;
        const int hi = spec_.boundary[static_cast<size_t>(2 * axis + 1)] == YeeBoundary::Absorbing ? spec_.pmlCells : 0;
        pad_[static_cast<size_t>(axis)] = lo;
        p[axis] = periodic_[static_cast<size_t>(axis)] ? n[axis] : n[axis] + lo + hi + 1;
    }
    px_ = p[0];
    py_ = p[1];
    pz_ = p[2];
    const size_t nodes = static_cast<size_t>(px_) * py_ * pz_;
    for (auto& component : e_)
        for (auto& part : component)
            part.assign(nodes, 0.0);
    for (auto& component : h_)
        for (auto& part : component)
            part.assign(nodes, 0.0);

    // Cells (padded): the interior map, continued outward into the layers.
    const int cx = periodic_[0] ? px_ : px_ - 1, cy = periodic_[1] ? py_ : py_ - 1, cz = spec_.is2D() ? 1 : (periodic_[2] ? pz_ : pz_ - 1);
    material_.assign(static_cast<size_t>(cx) * cy * cz, 0);
    if (!spec_.cellMaterial.empty())
        for (int k = 0; k < cz; ++k)
            for (int j = 0; j < cy; ++j)
                for (int i = 0; i < cx; ++i)
                    material_[static_cast<size_t>(i) + static_cast<size_t>(cx) * (static_cast<size_t>(j) + static_cast<size_t>(cy) * k)] =
                        spec_.cellMaterial[spec_.cellIndex(std::clamp(i - pad_[0], 0, spec_.nx - 1), std::clamp(j - pad_[1], 0, spec_.ny - 1),
                                                           std::clamp(k - pad_[2], 0, spec_.nz - 1))];
    const auto cellAt = [&](int i, int j, int k) -> const YeeMaterial* {
        if (periodic_[0])
            i = (i % cx + cx) % cx;
        if (periodic_[1])
            j = (j % cy + cy) % cy;
        if (spec_.is2D())
            k = 0;
        else if (periodic_[2])
            k = (k % cz + cz) % cz;
        if (i < 0 || j < 0 || k < 0 || i >= cx || j >= cy || k >= cz)
            return nullptr;
        return &spec_.materials[material_[static_cast<size_t>(i) + static_cast<size_t>(cx) * (static_cast<size_t>(j) + static_cast<size_t>(cy) * k)]];
    };

    // E coefficients per component and node: cells around the edge.
    const double dt = spec_.dt, d = spec_.cell;
    for (int c = 0; c < 3; ++c) {
        ca_[static_cast<size_t>(c)].assign(nodes, 0.0);
        cb_[static_cast<size_t>(c)].assign(nodes, 0.0);
        chb_[static_cast<size_t>(c)].assign(nodes, 0.0);
    }
    for (int k = 0; k < pz_; ++k)
        for (int j = 0; j < py_; ++j)
            for (int i = 0; i < px_; ++i)
                for (int c = 0; c < 3; ++c) {
                    // The two axes across the edge.
                    const int a1 = (c + 1) % 3, a2 = (c + 2) % 3;
                    double eps = 0.0, sigma = 0.0;
                    int count = 0;
                    bool pec = false, outside = false;
                    for (int s1 = -1; s1 <= 0; ++s1)
                        for (int s2 = -1; s2 <= 0; ++s2) {
                            int idx[3] = {i, j, k};
                            idx[a1] += s1;
                            idx[a2] += s2;
                            if (spec_.is2D() && (a1 == 2 || a2 == 2) && ((a1 == 2 && s1 != 0) || (a2 == 2 && s2 != 0)))
                                continue;  // 2D: the only layer along z
                            const YeeMaterial* m = cellAt(idx[0], idx[1], idx[2]);
                            if (!m) {
                                outside = true;
                                continue;
                            }
                            pec = pec || m->pec;
                            eps += m->pec ? 1.0 : m->relativePermittivity;
                            sigma += m->pec ? 0.0 : m->conductivity;
                            ++count;
                        }
                    // An edge on the outer face of a non-periodic axis (tangential E there) stays zero: PEC wall.
                    const int along[3] = {i, j, k};
                    const int limits[3] = {px_, py_, pz_};
                    bool onWall = false;
                    for (const int a : {a1, a2})
                        if (!periodic_[static_cast<size_t>(a)] && (along[a] == 0 || along[a] == limits[a] - 1))
                            onWall = true;
                    if (!periodic_[static_cast<size_t>(c)] && along[c] >= limits[c] - 1)
                        onWall = true;  // past the last edge along its own axis
                    if (pec || onWall || count == 0 || (outside && count == 0))
                        continue;
                    eps = kEpsilon0 * eps / count;
                    sigma /= count;
                    const double loss = sigma * dt / (2.0 * eps);
                    ca_[static_cast<size_t>(c)][at(i, j, k)] = (1.0 - loss) / (1.0 + loss);
                    cb_[static_cast<size_t>(c)][at(i, j, k)] = dt / (eps * d) / (1.0 + loss);
                }
    for (const YeeSpec::Edge& e : spec_.pecEdges) {
        const int i = e.i + pad_[0], j = e.j + pad_[1], k = spec_.is2D() ? 0 : e.k + pad_[2];
        if (i >= 0 && j >= 0 && k >= 0 && i < px_ && j < py_ && k < pz_ && e.component >= 0 && e.component < 3) {
            ca_[static_cast<size_t>(e.component)][at(i, j, k)] = 0.0;
            cb_[static_cast<size_t>(e.component)][at(i, j, k)] = 0.0;
        }
    }
    // H coefficients: mean permeability of the two cells sharing the face.
    for (int k = 0; k < pz_; ++k)
        for (int j = 0; j < py_; ++j)
            for (int i = 0; i < px_; ++i)
                for (int c = 0; c < 3; ++c) {
                    double mu = 0.0;
                    int count = 0;
                    for (int s = -1; s <= 0; ++s) {
                        int idx[3] = {i, j, k};
                        idx[c] += s;
                        if (spec_.is2D() && c == 2 && s != 0)
                            continue;
                        if (const YeeMaterial* m = cellAt(idx[0], idx[1], idx[2])) {
                            mu += m->pec ? 1.0 : m->relativePermeability;
                            ++count;
                        }
                    }
                    if (count)
                        chb_[static_cast<size_t>(c)][at(i, j, k)] = dt / (kMu0 * (mu / count) * d);
                }

    // Absorbing layers: sigma(depth) = sigma_max (depth / L)^2 for both E and H
    // (matched: the same decay rate), factor exp(-sigma dt / (2 eps0)).
    const double c0 = spec_.maxSpeed();
    const double thickness = spec_.pmlCells * d;
    const double sigmaMax = -3.0 * kEpsilon0 * c0 * std::log(spec_.pmlReflection) / (2.0 * thickness);
    const int limits[3] = {px_, py_, pz_};
    for (int axis = 0; axis < 3; ++axis) {
        decayNode_[static_cast<size_t>(axis)].assign(static_cast<size_t>(limits[axis]), 1.0);
        decayHalf_[static_cast<size_t>(axis)].assign(static_cast<size_t>(limits[axis]), 1.0);
        if (periodic_[static_cast<size_t>(axis)])
            continue;
        const bool lo = spec_.boundary[static_cast<size_t>(2 * axis)] == YeeBoundary::Absorbing;
        const bool hi = spec_.boundary[static_cast<size_t>(2 * axis + 1)] == YeeBoundary::Absorbing;
        const double loEdge = lo ? spec_.pmlCells * d : 0.0, hiEdge = (limits[axis] - 1 - (hi ? spec_.pmlCells : 0)) * d;
        const auto factor = [&](double x) {
            double depth = 0.0;
            if (lo && x < loEdge)
                depth = loEdge - x;
            if (hi && x > hiEdge)
                depth = x - hiEdge;
            const double f = std::min(depth / thickness, 1.0);
            return std::exp(-0.5 * sigmaMax * f * f * dt / kEpsilon0);
        };
        for (int i = 0; i < limits[axis]; ++i) {
            decayNode_[static_cast<size_t>(axis)][static_cast<size_t>(i)] = factor(i * d);
            decayHalf_[static_cast<size_t>(axis)][static_cast<size_t>(i)] = factor((i + 0.5) * d);
        }
    }
}

void YeeSolver::addSource(const YeeSource& s)
{
    const size_t node = at(s.i + pad_[0], s.j + pad_[1], spec_.is2D() ? 0 : s.k + pad_[2]);
    if (cb_[static_cast<size_t>(s.component)][node] == 0.0)
        return;  // on a perfect conductor or a wall: radiates nothing
    sources_.push_back({s.component, node, s.amplitude / spec_.cell, s.delay, s.waveform});
}

void YeeSolver::updateH()
{
    // Rows (j, k) are independent within each component: they are split
    // across the pool's threads and every row is computed exactly as alone.
    const int limits[3] = {px_, py_, pz_};
    const size_t rows = static_cast<size_t>(py_) * static_cast<size_t>(pz_);
    for (int c = 0; c < 3; ++c) {
        const int a1 = (c + 1) % 3, a2 = (c + 2) % 3;
        auto& part1 = h_[static_cast<size_t>(c)][static_cast<size_t>(a1)];
        auto& part2 = h_[static_cast<size_t>(c)][static_cast<size_t>(a2)];
        const auto& eA2 = e_[static_cast<size_t>(a2)];  // E component along a2, differentiated along a1
        const auto& eA1 = e_[static_cast<size_t>(a1)];
        const auto& chb = chb_[static_cast<size_t>(c)];
        pool_->run(rows, [&](size_t begin, size_t end) {
            for (size_t row = begin; row < end; ++row) {
                const int j = static_cast<int>(row % static_cast<size_t>(py_)), k = static_cast<int>(row / static_cast<size_t>(py_));
                for (int i = 0; i < px_; ++i) {
                    const int idx[3] = {i, j, k};
                    // H_c is at half positions along a1 and a2.
                    if ((!periodic_[static_cast<size_t>(a1)] && idx[a1] >= limits[a1] - 1)
                        || (!periodic_[static_cast<size_t>(a2)] && idx[a2] >= limits[a2] - 1))
                        continue;
                    const size_t n = at(i, j, k);
                    int up1[3] = {i, j, k}, up2[3] = {i, j, k};
                    up1[a1] = wrap(a1, up1[a1] + 1);
                    up2[a2] = wrap(a2, up2[a2] + 1);
                    const size_t n1 = at(up1[0], up1[1], up1[2]), n2 = at(up2[0], up2[1], up2[2]);
                    const double ea2Up = eA2[0][n1] + eA2[1][n1] + eA2[2][n1], ea2 = eA2[0][n] + eA2[1][n] + eA2[2][n];
                    const double ea1Up = eA1[0][n2] + eA1[1][n2] + eA1[2][n2], ea1 = eA1[0][n] + eA1[1][n] + eA1[2][n];
                    const double d1 = decayHalf_[static_cast<size_t>(a1)][static_cast<size_t>(idx[a1])];
                    const double d2 = decayHalf_[static_cast<size_t>(a2)][static_cast<size_t>(idx[a2])];
                    part1[n] = d1 * (d1 * part1[n] - chb[n] * (ea2Up - ea2));
                    part2[n] = d2 * (d2 * part2[n] + chb[n] * (ea1Up - ea1));
                }
            }
        });
    }
}

void YeeSolver::updateE()
{
    const size_t rows = static_cast<size_t>(py_) * static_cast<size_t>(pz_);
    for (int c = 0; c < 3; ++c) {
        const int a1 = (c + 1) % 3, a2 = (c + 2) % 3;
        auto& part1 = e_[static_cast<size_t>(c)][static_cast<size_t>(a1)];
        auto& part2 = e_[static_cast<size_t>(c)][static_cast<size_t>(a2)];
        const auto& hA2 = h_[static_cast<size_t>(a2)];
        const auto& hA1 = h_[static_cast<size_t>(a1)];
        const auto& ca = ca_[static_cast<size_t>(c)];
        const auto& cb = cb_[static_cast<size_t>(c)];
        pool_->run(rows, [&](size_t begin, size_t end) {
            for (size_t row = begin; row < end; ++row) {
                const int j = static_cast<int>(row % static_cast<size_t>(py_)), k = static_cast<int>(row / static_cast<size_t>(py_));
                for (int i = 0; i < px_; ++i) {
                    const size_t n = at(i, j, k);
                    if (cb[n] == 0.0)
                        continue;
                    const int idx[3] = {i, j, k};
                    int dn1[3] = {i, j, k}, dn2[3] = {i, j, k};
                    dn1[a1] = wrap(a1, dn1[a1] - 1);
                    dn2[a2] = wrap(a2, dn2[a2] - 1);
                    const size_t n1 = at(dn1[0], dn1[1], dn1[2]), n2 = at(dn2[0], dn2[1], dn2[2]);
                    const double ha2 = hA2[0][n] + hA2[1][n] + hA2[2][n], ha2Down = hA2[0][n1] + hA2[1][n1] + hA2[2][n1];
                    const double ha1 = hA1[0][n] + hA1[1][n] + hA1[2][n], ha1Down = hA1[0][n2] + hA1[1][n2] + hA1[2][n2];
                    const double d1 = decayNode_[static_cast<size_t>(a1)][static_cast<size_t>(idx[a1])];
                    const double d2 = decayNode_[static_cast<size_t>(a2)][static_cast<size_t>(idx[a2])];
                    part1[n] = d1 * (d1 * ca[n] * part1[n] + cb[n] * (ha2 - ha2Down));
                    part2[n] = d2 * (d2 * ca[n] * part2[n] - cb[n] * (ha1 - ha1Down));
                }
            }
        });
    }
}

void YeeSolver::step()
{
    updateH();
    updateE();
    // Currents at the middle of the step: dE = -(dt / eps) J, J = I / d^2.
    const double t = (static_cast<double>(step_) + 0.5) * spec_.dt;
    for (const Active& s : sources_) {
        const int a1 = (s.component + 1) % 3;
        e_[static_cast<size_t>(s.component)][static_cast<size_t>(a1)][s.node] -=
            cb_[static_cast<size_t>(s.component)][s.node] * s.scale * s.waveform.value(t - s.delay);
    }
    ++step_;
}

double YeeSolver::e(int c, int i, int j, int k) const
{
    const size_t n = paddedNode(i, j, k);
    return e_[static_cast<size_t>(c)][0][n] + e_[static_cast<size_t>(c)][1][n] + e_[static_cast<size_t>(c)][2][n];
}

double YeeSolver::h(int c, int i, int j, int k) const
{
    const size_t n = paddedNode(i, j, k);
    return h_[static_cast<size_t>(c)][0][n] + h_[static_cast<size_t>(c)][1][n] + h_[static_cast<size_t>(c)][2][n];
}

double YeeSolver::energy() const
{
    // A partial sum per row, added in row order: the same value for any
    // number of threads.
    const double volume = spec_.cell * spec_.cell * (spec_.is2D() ? 1.0 : spec_.cell);
    const size_t rows = static_cast<size_t>(py_) * static_cast<size_t>(pz_);
    std::vector<double> partial(rows, 0.0);
    pool_->run(rows, [&](size_t begin, size_t end) {
        for (size_t row = begin; row < end; ++row) {
            double sum = 0.0;
            const size_t first = row * static_cast<size_t>(px_);
            for (size_t n = first; n < first + static_cast<size_t>(px_); ++n)
                for (int c = 0; c < 3; ++c) {
                    const double cb = cb_[static_cast<size_t>(c)][n];
                    if (cb > 0.0) {
                        const double eps = spec_.dt / (cb * spec_.cell);  // ignores the loss factor: an estimate in lossy cells
                        const double value = e_[static_cast<size_t>(c)][0][n] + e_[static_cast<size_t>(c)][1][n] + e_[static_cast<size_t>(c)][2][n];
                        sum += 0.5 * eps * value * value;
                    }
                    const double chb = chb_[static_cast<size_t>(c)][n];
                    if (chb > 0.0) {
                        const double mu = spec_.dt / (chb * spec_.cell);
                        const double value = h_[static_cast<size_t>(c)][0][n] + h_[static_cast<size_t>(c)][1][n] + h_[static_cast<size_t>(c)][2][n];
                        sum += 0.5 * mu * value * value;
                    }
                }
            partial[row] = sum;
        }
    });
    double total = 0.0;
    for (const double p : partial)
        total += p;
    return total * volume;
}

double YeeSolver::eAtNode(int c, int i, int j, int k) const
{
    if (c == 2 && spec_.is2D())
        return e(2, i, j, 0);
    int lo[3] = {i, j, k};
    lo[c] -= 1;
    // Along a periodic axis every node has an edge on either side (the
    // indices wrap; region node n is node 0).
    if (periodic_[static_cast<size_t>(c)])
        return 0.5 * (e(c, i, j, k) + e(c, lo[0], lo[1], lo[2]));
    const int limit = c == 0 ? spec_.nx : c == 1 ? spec_.ny : spec_.nz;
    const int along = c == 0 ? i : c == 1 ? j : k;
    if (along >= limit)
        return e(c, lo[0], lo[1], lo[2]);  // the last node: the edge before it
    const double here = e(c, i, j, k);
    return along > 0 ? 0.5 * (here + e(c, lo[0], lo[1], lo[2])) : here;
}

YeeDeviceLayout YeeSolver::deviceLayout() const
{
    YeeDeviceLayout l;
    l.px = px_;
    l.py = py_;
    l.pz = pz_;
    for (int a = 0; a < 3; ++a) {
        l.periodic[a] = periodic_[static_cast<size_t>(a)] ? 1 : 0;
        l.pad[a] = pad_[static_cast<size_t>(a)];
        l.ca[a] = ca_[static_cast<size_t>(a)].data();
        l.cb[a] = cb_[static_cast<size_t>(a)].data();
        l.chb[a] = chb_[static_cast<size_t>(a)].data();
        l.decayNode[a] = decayNode_[static_cast<size_t>(a)].data();
        l.decayHalf[a] = decayHalf_[static_cast<size_t>(a)].data();
        for (int p = 0; p < 3; ++p) {
            l.e[a][p] = e_[static_cast<size_t>(a)][static_cast<size_t>(p)].data();
            l.h[a][p] = h_[static_cast<size_t>(a)][static_cast<size_t>(p)].data();
        }
    }
    l.nx = spec_.nx;
    l.ny = spec_.ny;
    l.nz = spec_.nz;
    l.is2D = spec_.is2D() ? 1 : 0;
    l.cell = spec_.cell;
    l.dt = spec_.dt;
    for (const Active& a : sources_) {
        l.sourceComponent.push_back(a.component);
        l.sourcePart.push_back((a.component + 1) % 3);
        l.sourceNode.push_back(a.node);
    }
    return l;
}

void YeeSolver::sourceValues(std::vector<double>& out) const
{
    const double t = (static_cast<double>(step_) + 0.5) * spec_.dt;
    out.resize(sources_.size());
    for (size_t i = 0; i < sources_.size(); ++i) {
        const Active& a = sources_[i];
        out[i] = cb_[static_cast<size_t>(a.component)][a.node] * a.scale * a.waveform.value(t - a.delay);
    }
}

bool YeeSolver::finite() const
{
    const size_t stride = std::max<size_t>(1, e_[2][0].size() / 4096);
    for (size_t n = 0; n < e_[2][0].size(); n += stride)
        for (int c = 0; c < 3; ++c)
            if (!std::isfinite(e_[static_cast<size_t>(c)][0][n] + e_[static_cast<size_t>(c)][1][n] + e_[static_cast<size_t>(c)][2][n]))
                return false;
    return true;
}

namespace {

// One way of advancing the grid and reading what runYee records: the CPU
// reference (threads) or the CUDA device.
class YeeStepper {
public:
    virtual ~YeeStepper() = default;
    virtual bool step(std::string& error) = 0;
    virtual bool probe(const std::vector<YeeProbe>& probes, std::vector<double>& values, std::string& error) = 0;
    virtual bool accumulateHarmonic(std::complex<double> kernel, std::string& error) = 0;
    virtual bool harmonic(std::array<std::vector<std::complex<double>>, 3>& out, std::string& error) = 0;
    virtual bool snapshot(int component, std::vector<float>& out, std::string& error) = 0;
    virtual bool energy(double& value, std::string& error) = 0;
    virtual bool finite() = 0;
    virtual double time() const = 0;
    virtual std::string backend() const = 0;
};

size_t regionNodes(const YeeSpec& s)
{
    return static_cast<size_t>(s.nx + 1) * static_cast<size_t>(s.ny + 1) * static_cast<size_t>(s.is2D() ? 1 : s.nz + 1);
}

class CpuYeeStepper final : public YeeStepper {
public:
    explicit CpuYeeStepper(std::unique_ptr<YeeSolver> solver) : solver_(std::move(solver)) {}
    bool step(std::string&) override
    {
        solver_->step();
        return true;
    }
    bool probe(const std::vector<YeeProbe>& probes, std::vector<double>& values, std::string&) override
    {
        values.resize(probes.size());
        for (size_t q = 0; q < probes.size(); ++q)
            values[q] = solver_->e(probes[q].component, probes[q].i, probes[q].j, probes[q].k);
        return true;
    }
    bool accumulateHarmonic(std::complex<double> kernel, std::string&) override
    {
        const YeeSpec& s = solver_->spec();
        if (accumulated_[0].empty())
            for (auto& a : accumulated_)
                a.assign(regionNodes(s), {0.0, 0.0});
        forNodeRows([&](int j, int k, size_t first) {
            for (int i = 0; i <= s.nx; ++i)
                for (int c = 0; c < 3; ++c)
                    accumulated_[static_cast<size_t>(c)][first + static_cast<size_t>(i)] += solver_->eAtNode(c, i, j, k) * kernel;
        });
        return true;
    }
    bool harmonic(std::array<std::vector<std::complex<double>>, 3>& out, std::string&) override
    {
        for (int c = 0; c < 3; ++c)
            if (!accumulated_[static_cast<size_t>(c)].empty())
                out[static_cast<size_t>(c)] = accumulated_[static_cast<size_t>(c)];
        return true;
    }
    bool snapshot(int component, std::vector<float>& out, std::string&) override
    {
        out.assign(regionNodes(solver_->spec()), 0.0f);
        forNodeRows([&](int j, int k, size_t first) {
            for (int i = 0; i <= solver_->spec().nx; ++i)
                out[first + static_cast<size_t>(i)] = static_cast<float>(solver_->eAtNode(component, i, j, k));
        });
        return true;
    }
    bool energy(double& value, std::string&) override
    {
        value = solver_->energy();
        return true;
    }
    bool finite() override { return solver_->finite(); }
    double time() const override { return solver_->time(); }
    std::string backend() const override
    {
        const int t = solver_->threads();
        return "CPU, double precision, " + std::to_string(t) + (t == 1 ? " thread" : " threads");
    }

private:
    // Rows (j, k) of region nodes in parallel; `first` is the row's first node index.
    template <typename F>
    void forNodeRows(F&& body)
    {
        const YeeSpec& s = solver_->spec();
        const int nk = s.is2D() ? 1 : s.nz + 1;
        const size_t rows = static_cast<size_t>(s.ny + 1) * static_cast<size_t>(nk);
        solver_->pool().run(rows, [&](size_t begin, size_t end) {
            for (size_t row = begin; row < end; ++row) {
                const int j = static_cast<int>(row % static_cast<size_t>(s.ny + 1)), k = static_cast<int>(row / static_cast<size_t>(s.ny + 1));
                body(j, k, row * static_cast<size_t>(s.nx + 1));
            }
        });
    }

    std::unique_ptr<YeeSolver> solver_;
    std::array<std::vector<std::complex<double>>, 3> accumulated_;
};

class CudaYeeStepper final : public YeeStepper {
public:
    CudaYeeStepper(std::unique_ptr<YeeSolver> solver, YeeCudaState* state, std::vector<int> components, std::vector<size_t> nodes)
        : solver_(std::move(solver)), state_(state), probeComponents_(std::move(components)), probeNodes_(std::move(nodes))
    {
    }
    ~CudaYeeStepper() override { yeeCudaDestroy(state_); }
    bool step(std::string& error) override
    {
        solver_->sourceValues(sourceValues_);
        if (!yeeCudaStep(state_, sourceValues_.data(), error))
            return false;
        solver_->advanceClock();
        return true;
    }
    bool probe(const std::vector<YeeProbe>&, std::vector<double>& values, std::string& error) override
    {
        values.resize(probeNodes_.size());
        return probeNodes_.empty() || yeeCudaGather(state_, probeComponents_.data(), probeNodes_.data(), static_cast<int>(probeNodes_.size()), values.data(), error);
    }
    bool accumulateHarmonic(std::complex<double> kernel, std::string& error) override
    {
        accumulating_ = true;
        return yeeCudaAccumulateHarmonic(state_, kernel.real(), kernel.imag(), error);
    }
    bool harmonic(std::array<std::vector<std::complex<double>>, 3>& out, std::string& error) override
    {
        if (!accumulating_)
            return true;
        std::vector<double> re[3], im[3];
        if (!yeeCudaHarmonic(state_, re, im, error))
            return false;
        for (int c = 0; c < 3; ++c) {
            auto& o = out[static_cast<size_t>(c)];
            o.resize(re[c].size());
            for (size_t n = 0; n < o.size(); ++n)
                o[n] = {re[c][n], im[c][n]};
        }
        return true;
    }
    bool snapshot(int component, std::vector<float>& out, std::string& error) override { return yeeCudaNodeField(state_, component, out, error); }
    bool energy(double& value, std::string& error) override { return yeeCudaEnergy(state_, value, error); }
    bool finite() override { return true; }  // a non-finite field makes the energy non-finite, which runYee checks
    double time() const override { return solver_->time(); }
    std::string backend() const override { return "CUDA on " + yeeCudaDeviceName(state_) + ", double precision"; }

private:
    std::unique_ptr<YeeSolver> solver_;  // keeps the sources' waveforms and the clock
    YeeCudaState* state_;
    std::vector<int> probeComponents_;
    std::vector<size_t> probeNodes_;
    std::vector<double> sourceValues_;
    bool accumulating_ = false;
};

}  // namespace

YeeRunEnd runYee(const YeeSpec& spec, const std::vector<YeeSource>& sources, const std::vector<YeeProbe>& probes, long steps,
                 const YeeRecording& rec, YeeOutput& out, JobContext* context)
{
    if (steps < 1)
        return {YeeRunEnd::Kind::Invalid, "the simulation needs at least one time step"};
    if (Status status = compute::checkBackend(spec.compute, spec.stateBytes()); !status)
        return {YeeRunEnd::Kind::Invalid, status.message()};
    auto created = YeeSolver::create(spec);
    if (!created)
        return {YeeRunEnd::Kind::Invalid, created.message()};
    auto solver = created.take();
    const int nk = spec.is2D() ? 1 : spec.nz + 1;
    const auto inside = [&](int i, int j, int k) { return i >= 0 && j >= 0 && k >= 0 && i <= spec.nx && j <= spec.ny && k < nk; };
    for (const YeeSource& s : sources) {
        if (!inside(s.i, s.j, s.k) || s.component < 0 || s.component > 2)
            return {YeeRunEnd::Kind::Invalid, "a source lies outside the grid"};
        if (Status status = s.waveform.validate(); !status)
            return {YeeRunEnd::Kind::Invalid, status.message()};
        solver->addSource(s);
    }
    out = YeeOutput{};
    out.dt = spec.dt;
    for (const YeeProbe& p : probes) {
        if (!inside(p.i, p.j, p.k))
            return {YeeRunEnd::Kind::Invalid, "probe '" + p.label + "' lies outside the grid"};
        out.probes.emplace_back();
        out.probes.back().reserve(static_cast<size_t>(steps));
    }
    std::unique_ptr<YeeStepper> stepper;
    if (spec.compute.backend == compute::ComputeBackend::Cuda) {
        std::vector<int> components;
        std::vector<size_t> nodes;
        for (const YeeProbe& p : probes) {
            components.push_back(p.component);
            nodes.push_back(solver->paddedNode(p.i, p.j, p.k));
        }
        std::string error;
        YeeCudaState* state = yeeCudaCreate(solver->deviceLayout(), spec.compute.device, error);
        if (!state)
            return {YeeRunEnd::Kind::Invalid, "the CUDA backend could not start: " + error};
        stepper = std::make_unique<CudaYeeStepper>(std::move(solver), state, std::move(components), std::move(nodes));
    } else {
        stepper = std::make_unique<CpuYeeStepper>(std::move(solver));
    }
    out.backend = stepper->backend();
    const size_t nodes = regionNodes(spec);
    long harmonicFirst = steps + 1, harmonicCount = 0;
    if (rec.harmonicFrequencyHz > 0.0) {
        harmonicCount = std::clamp<long>(std::lround(rec.harmonicPeriods / (rec.harmonicFrequencyHz * spec.dt)), 1, steps);
        harmonicFirst = steps - harmonicCount + 1;
        for (auto& h : out.harmonic)
            h.assign(nodes, {0.0, 0.0});
        out.harmonicFrequencyHz = rec.harmonicFrequencyHz;
    }
    const long snapshotTotal = std::min<long>(rec.snapshotCount, steps);
    long snapshotsTaken = 0;
    const long energyEvery = std::max<long>(1, steps / std::max(1, rec.energySamples));
    const auto started = std::chrono::steady_clock::now();
    const double twoPi = 2.0 * kPi;
    std::string error;
    std::vector<double> probeValues;
    const auto failed = [&](long n) {
        return YeeRunEnd{YeeRunEnd::Kind::Invalid, "the " + out.backend + " backend failed at step " + std::to_string(n) + ": " + error};
    };
    for (long n = 1; n <= steps; ++n) {
        if (!stepper->step(error))
            return failed(n);
        const double t = stepper->time();
        if (!probes.empty()) {
            if (!stepper->probe(probes, probeValues, error))
                return failed(n);
            for (size_t q = 0; q < probes.size(); ++q)
                out.probes[q].push_back(probeValues[q]);
        }
        if (n >= harmonicFirst) {
            const std::complex<double> kernel = std::polar(2.0 / static_cast<double>(harmonicCount), -twoPi * rec.harmonicFrequencyHz * t);
            if (!stepper->accumulateHarmonic(kernel, error))
                return failed(n);
        }
        if (snapshotsTaken < snapshotTotal
            && n >= std::lround(static_cast<double>(snapshotsTaken + 1) * static_cast<double>(steps) / static_cast<double>(snapshotTotal))) {
            ++snapshotsTaken;
            std::vector<float> snapshot;
            if (!stepper->snapshot(rec.snapshotComponent, snapshot, error))
                return failed(n);
            out.snapshots.push_back(std::move(snapshot));
            out.snapshotTimes.push_back(t);
        }
        if (n % energyEvery == 0 || n == steps) {
            double energy = 0.0;
            if (!stepper->energy(energy, error))
                return failed(n);
            out.energyTimes.push_back(t);
            out.energy.push_back(energy);
            out.steps = n;
            if (!std::isfinite(energy) || !stepper->finite())
                return {YeeRunEnd::Kind::Unstable, "the field stopped being finite at step " + std::to_string(n)};
            if (context) {
                if (context->cancelRequested())
                    return {YeeRunEnd::Kind::Cancelled, "cancelled"};
                context->reportProgress(static_cast<double>(n) / static_cast<double>(steps), "step " + std::to_string(n) + " of " + std::to_string(steps));
            }
        }
    }
    if (harmonicCount > 0 && !stepper->harmonic(out.harmonic, error))
        return failed(steps);
    out.steps = steps;
    out.elapsedSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    const double cells = static_cast<double>(spec.stateBytes()) / (27.0 * sizeof(double) + 1.0);
    out.cellUpdatesPerSecond = out.elapsedSeconds > 0.0 ? cells * static_cast<double>(steps) / out.elapsedSeconds : 0.0;
    return {};
}

}  // namespace resonant::em
