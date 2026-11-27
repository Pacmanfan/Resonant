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
    return std::unique_ptr<YeeSolver>(new YeeSolver(std::move(spec)));
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
    const int limits[3] = {px_, py_, pz_};
    for (int c = 0; c < 3; ++c) {
        const int a1 = (c + 1) % 3, a2 = (c + 2) % 3;
        auto& part1 = h_[static_cast<size_t>(c)][static_cast<size_t>(a1)];
        auto& part2 = h_[static_cast<size_t>(c)][static_cast<size_t>(a2)];
        const auto& eA2 = e_[static_cast<size_t>(a2)];  // E component along a2, differentiated along a1
        const auto& eA1 = e_[static_cast<size_t>(a1)];
        const auto& chb = chb_[static_cast<size_t>(c)];
        for (int k = 0; k < pz_; ++k)
            for (int j = 0; j < py_; ++j)
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
}

void YeeSolver::updateE()
{
    for (int c = 0; c < 3; ++c) {
        const int a1 = (c + 1) % 3, a2 = (c + 2) % 3;
        auto& part1 = e_[static_cast<size_t>(c)][static_cast<size_t>(a1)];
        auto& part2 = e_[static_cast<size_t>(c)][static_cast<size_t>(a2)];
        const auto& hA2 = h_[static_cast<size_t>(a2)];
        const auto& hA1 = h_[static_cast<size_t>(a1)];
        const auto& ca = ca_[static_cast<size_t>(c)];
        const auto& cb = cb_[static_cast<size_t>(c)];
        for (int k = 0; k < pz_; ++k)
            for (int j = 0; j < py_; ++j)
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
    const size_t n = at(i + pad_[0], j + pad_[1], spec_.is2D() ? 0 : k + pad_[2]);
    return e_[static_cast<size_t>(c)][0][n] + e_[static_cast<size_t>(c)][1][n] + e_[static_cast<size_t>(c)][2][n];
}

double YeeSolver::h(int c, int i, int j, int k) const
{
    const size_t n = at(i + pad_[0], j + pad_[1], spec_.is2D() ? 0 : k + pad_[2]);
    return h_[static_cast<size_t>(c)][0][n] + h_[static_cast<size_t>(c)][1][n] + h_[static_cast<size_t>(c)][2][n];
}

double YeeSolver::energy() const
{
    const double volume = spec_.cell * spec_.cell * (spec_.is2D() ? 1.0 : spec_.cell);
    double sum = 0.0;
    for (size_t n = 0; n < e_[0][0].size(); ++n)
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
    return sum * volume;
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

YeeRunEnd runYee(const YeeSpec& spec, const std::vector<YeeSource>& sources, const std::vector<YeeProbe>& probes, long steps,
                 const YeeRecording& rec, YeeOutput& out, JobContext* context)
{
    if (steps < 1)
        return {YeeRunEnd::Kind::Invalid, "the simulation needs at least one time step"};
    auto created = YeeSolver::create(spec);
    if (!created)
        return {YeeRunEnd::Kind::Invalid, created.message()};
    const auto solver = created.take();
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
    const size_t nodes = static_cast<size_t>(spec.nx + 1) * static_cast<size_t>(spec.ny + 1) * static_cast<size_t>(nk);
    const auto nodeIndex = [&](int i, int j, int k) {
        return static_cast<size_t>(i) + static_cast<size_t>(spec.nx + 1) * (static_cast<size_t>(j) + static_cast<size_t>(spec.ny + 1) * k);
    };
    // E component at a node: the mean of the edges on either side along the component's axis.
    const auto atNode = [&](int c, int i, int j, int k) {
        if (c == 2 && spec.is2D())
            return solver->e(2, i, j, 0);
        int lo[3] = {i, j, k};
        lo[c] -= 1;
        const int limit = c == 0 ? spec.nx : c == 1 ? spec.ny : spec.nz;
        const int along = c == 0 ? i : c == 1 ? j : k;
        if (along >= limit)
            return solver->e(c, lo[0], lo[1], lo[2]);  // the last node: the edge before it
        const double here = solver->e(c, i, j, k);
        return along > 0 ? 0.5 * (here + solver->e(c, lo[0], lo[1], lo[2])) : here;
    };
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
    for (long n = 1; n <= steps; ++n) {
        solver->step();
        const double t = solver->time();
        for (size_t q = 0; q < probes.size(); ++q)
            out.probes[q].push_back(solver->e(probes[q].component, probes[q].i, probes[q].j, probes[q].k));
        if (n >= harmonicFirst) {
            const std::complex<double> kernel = std::polar(2.0 / static_cast<double>(harmonicCount), -twoPi * rec.harmonicFrequencyHz * t);
            for (int k = 0; k < nk; ++k)
                for (int j = 0; j <= spec.ny; ++j)
                    for (int i = 0; i <= spec.nx; ++i)
                        for (int c = 0; c < 3; ++c) {
                            if (spec.is2D() && c != 2) {
                                // 2D: the TE components at nodes as well.
                                out.harmonic[static_cast<size_t>(c)][nodeIndex(i, j, k)] += atNode(c, i, j, 0) * kernel;
                                continue;
                            }
                            out.harmonic[static_cast<size_t>(c)][nodeIndex(i, j, k)] += atNode(c, i, j, k) * kernel;
                        }
        }
        if (snapshotsTaken < snapshotTotal
            && n >= std::lround(static_cast<double>(snapshotsTaken + 1) * static_cast<double>(steps) / static_cast<double>(snapshotTotal))) {
            ++snapshotsTaken;
            std::vector<float> snapshot(nodes);
            for (int k = 0; k < nk; ++k)
                for (int j = 0; j <= spec.ny; ++j)
                    for (int i = 0; i <= spec.nx; ++i)
                        snapshot[nodeIndex(i, j, k)] = static_cast<float>(atNode(rec.snapshotComponent, i, j, k));
            out.snapshots.push_back(std::move(snapshot));
            out.snapshotTimes.push_back(t);
        }
        if (n % energyEvery == 0 || n == steps) {
            const double energy = solver->energy();
            out.energyTimes.push_back(t);
            out.energy.push_back(energy);
            out.steps = n;
            if (!std::isfinite(energy) || !solver->finite())
                return {YeeRunEnd::Kind::Unstable, "the field stopped being finite at step " + std::to_string(n)};
            if (context) {
                if (context->cancelRequested())
                    return {YeeRunEnd::Kind::Cancelled, "cancelled"};
                context->reportProgress(static_cast<double>(n) / static_cast<double>(steps), "step " + std::to_string(n) + " of " + std::to_string(steps));
            }
        }
    }
    out.steps = steps;
    out.elapsedSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    const double cells = static_cast<double>(spec.stateBytes()) / (27.0 * sizeof(double) + 1.0);
    out.cellUpdatesPerSecond = out.elapsedSeconds > 0.0 ? cells * static_cast<double>(steps) / out.elapsedSeconds : 0.0;
    return {};
}

}  // namespace resonant::em
