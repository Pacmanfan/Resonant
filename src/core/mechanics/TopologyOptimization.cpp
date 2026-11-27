#include "mechanics/TopologyOptimization.h"

#include "simulation/Job.h"

#include <Eigen/Sparse>
#include <Eigen/SparseCholesky>

#include <algorithm>
#include <cmath>
#include <deque>
#include <numeric>

namespace resonant::mechanics {

std::string_view toString(TopologyBenchmark benchmark) { return benchmark == TopologyBenchmark::Cantilever ? "cantilever" : "half-mbb-beam"; }

Status TopologySettings::validate() const
{
    if (nelx < 4 || nely < 2 || static_cast<long>(nelx) * nely > 200000)
        return Status::failure("the design region needs between 4 x 2 and 200 000 elements");
    if (!(width > 0.0) || !(thickness > 0.0) || !(youngsModulus > 0.0) || !(load > 0.0))
        return Status::failure("width, thickness, Young's modulus and load must be positive");
    if (poissonsRatio <= -1.0 || poissonsRatio >= 0.5)
        return Status::failure("Poisson's ratio must be in (-1, 0.5)");
    if (!(volumeFraction > 0.05 && volumeFraction < 0.95))
        return Status::failure("the volume fraction must be between 0.05 and 0.95");
    if (penalty < 1.0 || penalty > 6.0)
        return Status::failure("the penalty must be between 1 and 6");
    if (filterRadius < 0.0)
        return Status::failure("the filter radius must not be negative");
    return Status::success();
}

namespace {

using Matrix8 = Eigen::Matrix<double, 8, 8>;

// Stiffness of a square Q4 plane-stress element for E = 1 and unit thickness
// (it does not depend on the element size), node order: bottom-left,
// bottom-right, top-right, top-left; dofs x, y per node.
Matrix8 elementStiffness(double nu)
{
    const double a11[4][4] = {{12, 3, -6, -3}, {3, 12, 3, 0}, {-6, 3, 12, -3}, {-3, 0, -3, 12}};
    const double a12[4][4] = {{-6, -3, 0, 3}, {-3, -6, -3, -6}, {0, -3, -6, 3}, {3, -6, 3, -6}};
    const double b11[4][4] = {{-4, 3, -2, 9}, {3, -4, -9, 4}, {-2, -9, -4, -3}, {9, 4, -3, -4}};
    const double b12[4][4] = {{2, -3, 4, -9}, {-3, 2, 9, -2}, {4, 9, 2, 3}, {-9, -2, 3, 2}};
    Matrix8 k;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            k(i, j) = a11[i][j] + nu * b11[i][j];
            k(i, j + 4) = a12[i][j] + nu * b12[i][j];
            k(i + 4, j) = a12[j][i] + nu * b12[j][i];
            k(i + 4, j + 4) = a11[i][j] + nu * b11[i][j];
        }
    return k / (24.0 * (1.0 - nu * nu));
}

struct Problem {
    int nelx, nely, ndof;
    std::vector<std::array<int, 8>> edof;
    std::vector<int> fixed;
    Eigen::VectorXd force;
    Matrix8 ke;
    std::vector<int> loadedElements, supportedElements;
};

Problem setup(const TopologySettings& s)
{
    Problem p;
    p.nelx = s.nelx;
    p.nely = s.nely;
    p.ndof = 2 * (s.nelx + 1) * (s.nely + 1);
    p.ke = elementStiffness(s.poissonsRatio) * s.youngsModulus * s.thickness;
    const auto node = [&](int i, int j) { return j * (s.nely + 1) + i; };  // i row from the top, j column
    for (int elx = 0; elx < s.nelx; ++elx)
        for (int ely = 0; ely < s.nely; ++ely) {
            const int tl = node(ely, elx), bl = node(ely + 1, elx), br = node(ely + 1, elx + 1), tr = node(ely, elx + 1);
            p.edof.push_back({2 * bl, 2 * bl + 1, 2 * br, 2 * br + 1, 2 * tr, 2 * tr + 1, 2 * tl, 2 * tl + 1});
        }
    p.force = Eigen::VectorXd::Zero(p.ndof);
    // y up: a downward load is negative y. Node rows count from the top.
    if (s.benchmark == TopologyBenchmark::Cantilever) {
        for (int i = 0; i <= s.nely; ++i) {
            p.fixed.push_back(2 * node(i, 0));
            p.fixed.push_back(2 * node(i, 0) + 1);
        }
        const int mid = s.nely / 2;
        p.force(2 * node(mid, s.nelx) + 1) = -s.load;
        p.loadedElements.push_back(std::min(mid, s.nely - 1) + (s.nelx - 1) * s.nely);
        for (int ely = 0; ely < s.nely; ++ely)
            p.supportedElements.push_back(ely);
    } else {
        for (int i = 0; i <= s.nely; ++i)
            p.fixed.push_back(2 * node(i, 0));  // symmetry: no x motion on the left edge
        p.fixed.push_back(2 * node(s.nely, s.nelx) + 1);  // roller at the bottom right
        p.force(2 * node(0, 0) + 1) = -s.load;
        p.loadedElements.push_back(0);
        p.supportedElements.push_back(s.nely - 1 + (s.nelx - 1) * s.nely);
        for (int ely = 0; ely < s.nely; ++ely)
            p.supportedElements.push_back(ely);
    }
    std::sort(p.fixed.begin(), p.fixed.end());
    p.fixed.erase(std::unique(p.fixed.begin(), p.fixed.end()), p.fixed.end());
    return p;
}

Result<Eigen::VectorXd> solve(const Problem& p, const std::vector<double>& stiffness)
{
    std::vector<int> map(static_cast<size_t>(p.ndof), 0);
    for (int f : p.fixed)
        map[static_cast<size_t>(f)] = -1;
    int n = 0;
    for (int& m : map)
        if (m == 0)
            m = n++;
        else
            m = -1;
    std::vector<Eigen::Triplet<double>> t;
    t.reserve(p.edof.size() * 64);
    for (size_t e = 0; e < p.edof.size(); ++e)
        for (int a = 0; a < 8; ++a) {
            const int ra = map[static_cast<size_t>(p.edof[e][static_cast<size_t>(a)])];
            if (ra < 0)
                continue;
            for (int b = 0; b < 8; ++b) {
                const int rb = map[static_cast<size_t>(p.edof[e][static_cast<size_t>(b)])];
                if (rb >= 0)
                    t.emplace_back(ra, rb, stiffness[e] * p.ke(a, b));
            }
        }
    Eigen::SparseMatrix<double> k(n, n);
    k.setFromTriplets(t.begin(), t.end());
    Eigen::VectorXd f(n);
    for (int d = 0; d < p.ndof; ++d)
        if (map[static_cast<size_t>(d)] >= 0)
            f(map[static_cast<size_t>(d)]) = p.force(d);
    Eigen::SimplicialLLT<Eigen::SparseMatrix<double>> llt(k);
    if (llt.info() != Eigen::Success)
        return Result<Eigen::VectorXd>::failure("the stiffness matrix is not positive definite (a mechanism or a void region carrying load)");
    const Eigen::VectorXd ur = llt.solve(f);
    Eigen::VectorXd u = Eigen::VectorXd::Zero(p.ndof);
    for (int d = 0; d < p.ndof; ++d)
        if (map[static_cast<size_t>(d)] >= 0)
            u(d) = ur(map[static_cast<size_t>(d)]);
    return u;
}

double elementEnergy(const Problem& p, const Eigen::VectorXd& u, size_t e)
{
    Eigen::Matrix<double, 8, 1> ue;
    for (int a = 0; a < 8; ++a)
        ue(a) = u(p.edof[e][static_cast<size_t>(a)]);
    return ue.dot(p.ke * ue);
}

}  // namespace

Result<double> topologyCompliance(const TopologySettings& s, const std::vector<double>& density, std::vector<double>* displacement)
{
    if (Status v = s.validate(); !v)
        return Result<double>::failure(v.message());
    const Problem p = setup(s);
    if (density.size() != p.edof.size())
        return Result<double>::failure("one density per element is needed");
    std::vector<double> stiffness(density.size());
    for (size_t e = 0; e < density.size(); ++e)
        stiffness[e] = s.minimumStiffness + std::pow(std::clamp(density[e], 0.0, 1.0), s.penalty) * (1.0 - s.minimumStiffness);
    auto u = solve(p, stiffness);
    if (!u)
        return Result<double>::failure(u.message());
    if (displacement)
        displacement->assign(u.value().data(), u.value().data() + u.value().size());
    return p.force.dot(u.value());
}

Result<TopologyResult> optimizeTopology(const TopologySettings& s, JobContext* context)
{
    using R = Result<TopologyResult>;
    if (Status v = s.validate(); !v)
        return R::failure(v.message());
    TopologyResult r;
    r.settings = s;
    const Problem p = setup(s);
    const size_t n = p.edof.size();
    const double h = s.elementSize();
    const double rmin = (s.filterRadius > 0.0 ? s.filterRadius : 1.5 * h) / h;  // in elements
    r.minimumFeature = 2.0 * rmin * h;
    // Density filter weights H_ij = max(0, rmin - dist).
    std::vector<std::vector<std::pair<size_t, double>>> filter(n);
    std::vector<double> hs(n, 0.0);
    const int reach = static_cast<int>(std::ceil(rmin)) - 1;
    for (int i1 = 0; i1 < s.nelx; ++i1)
        for (int j1 = 0; j1 < s.nely; ++j1) {
            const size_t e1 = static_cast<size_t>(i1 * s.nely + j1);
            for (int i2 = std::max(i1 - reach, 0); i2 <= std::min(i1 + reach, s.nelx - 1); ++i2)
                for (int j2 = std::max(j1 - reach, 0); j2 <= std::min(j1 + reach, s.nely - 1); ++j2) {
                    const double w = rmin - std::sqrt(static_cast<double>((i1 - i2) * (i1 - i2) + (j1 - j2) * (j1 - j2)));
                    if (w > 0.0) {
                        filter[e1].emplace_back(static_cast<size_t>(i2 * s.nely + j2), w);
                        hs[e1] += w;
                    }
                }
        }
    const auto apply = [&](const std::vector<double>& x) {
        std::vector<double> y(n, 0.0);
        for (size_t e = 0; e < n; ++e) {
            for (const auto& [k, w] : filter[e])
                y[e] += w * x[k];
            y[e] /= hs[e];
        }
        return y;
    };
    const auto applyTransposed = [&](const std::vector<double>& g) {  // H^T (g ./ Hs)
        std::vector<double> y(n, 0.0);
        for (size_t e = 0; e < n; ++e)
            for (const auto& [k, w] : filter[e])
                y[k] += w * g[e] / hs[e];
        return y;
    };
    std::vector<double> x(n, s.volumeFraction), phys = x;
    for (int it = 1; it <= s.maxIterations; ++it) {
        if (context && context->cancelRequested())
            return R::failure("cancelled");
        std::vector<double> stiffness(n);
        for (size_t e = 0; e < n; ++e)
            stiffness[e] = s.minimumStiffness + std::pow(phys[e], s.penalty) * (1.0 - s.minimumStiffness);
        auto u = solve(p, stiffness);
        if (!u)
            return R::failure(u.message());
        double c = 0.0;
        std::vector<double> dc(n), dv(n, 1.0);
        for (size_t e = 0; e < n; ++e) {
            const double ce = elementEnergy(p, u.value(), e);
            c += stiffness[e] * ce;
            dc[e] = -s.penalty * (1.0 - s.minimumStiffness) * std::pow(phys[e], s.penalty - 1.0) * ce;
        }
        if (it == 1)
            r.initialCompliance = c;
        dc = applyTransposed(dc);
        dv = applyTransposed(dv);
        // Optimality criteria with bisection on the volume multiplier.
        double l1 = 0.0, l2 = 1e12;
        const double move = 0.2;
        std::vector<double> xnew(n);
        while ((l2 - l1) / (l1 + l2) > 1e-6) {
            const double lmid = 0.5 * (l1 + l2);
            for (size_t e = 0; e < n; ++e)
                xnew[e] = std::clamp(x[e] * std::sqrt(std::max(0.0, -dc[e] / dv[e] / lmid)), std::max(0.0, x[e] - move), std::min(1.0, x[e] + move));
            const std::vector<double> pn = apply(xnew);
            if (std::accumulate(pn.begin(), pn.end(), 0.0) > s.volumeFraction * n)
                l1 = lmid;
            else
                l2 = lmid;
        }
        double change = 0.0;
        for (size_t e = 0; e < n; ++e)
            change = std::max(change, std::abs(xnew[e] - x[e]));
        x = xnew;
        phys = apply(x);
        r.compliance.push_back(c);
        r.volume.push_back(std::accumulate(phys.begin(), phys.end(), 0.0) / n);
        r.change.push_back(change);
        r.iterations = it;
        if (context)
            context->reportProgress(static_cast<double>(it) / s.maxIterations, "compliance " + std::to_string(c) + " J");
        if (change < s.tolerance) {
            r.converged = true;
            break;
        }
    }
    r.density = phys;
    size_t gray = 0;
    for (double v : phys)
        if (v > 0.1 && v < 0.9)
            ++gray;
    r.grayness = static_cast<double>(gray) / n;

    // Cleanup: threshold, then keep only material connected to a support and the load.
    std::vector<std::uint8_t> solid(n, 0);
    for (size_t e = 0; e < n; ++e)
        solid[e] = phys[e] >= s.threshold ? 1 : 0;
    std::vector<int> component(n, -1);
    int components = 0;
    for (size_t start = 0; start < n; ++start) {
        if (!solid[start] || component[start] >= 0)
            continue;
        std::deque<size_t> queue{start};
        component[start] = components;
        while (!queue.empty()) {
            const size_t e = queue.front();
            queue.pop_front();
            const int i = static_cast<int>(e) / s.nely, j = static_cast<int>(e) % s.nely;
            const int di[4] = {1, -1, 0, 0}, dj[4] = {0, 0, 1, -1};
            for (int k = 0; k < 4; ++k) {
                const int a = i + di[k], b = j + dj[k];
                if (a < 0 || b < 0 || a >= s.nelx || b >= s.nely)
                    continue;
                const size_t q = static_cast<size_t>(a * s.nely + b);
                if (solid[q] && component[q] < 0) {
                    component[q] = components;
                    queue.push_back(q);
                }
            }
        }
        ++components;
    }
    std::vector<char> keep(static_cast<size_t>(components), 0);
    for (int e : p.supportedElements)
        if (component[static_cast<size_t>(e)] >= 0)
            keep[static_cast<size_t>(component[static_cast<size_t>(e)])] = 1;
    bool loadConnected = false;
    for (int e : p.loadedElements)
        if (component[static_cast<size_t>(e)] >= 0 && keep[static_cast<size_t>(component[static_cast<size_t>(e)])])
            loadConnected = true;
    for (int c = 0; c < components; ++c)
        if (!keep[static_cast<size_t>(c)])
            ++r.islandsRemoved;
    for (size_t e = 0; e < n; ++e)
        if (solid[e] && !keep[static_cast<size_t>(component[e])])
            solid[e] = 0;
    r.solid = solid;
    r.cleanedVolumeFraction = std::accumulate(solid.begin(), solid.end(), 0.0) / n;
    std::vector<double> binary(solid.begin(), solid.end());
    if (auto c = topologyCompliance(s, binary); c)
        r.cleanedCompliance = c.value();
    if (!loadConnected)
        r.notes.push_back("after cleanup the load point is not connected to a support: the thresholded design does not carry the load");
    r.notes.push_back("a numerical material distribution on a " + std::to_string(s.nelx) + " x " + std::to_string(s.nely) +
                      " mesh, not a finished physical design: re-analyse and detail it before manufacture");
    r.notes.push_back("smallest feature about " + std::to_string(r.minimumFeature * 1e3) + " mm (twice the filter radius); mesh-independent only "
                      "while the filter radius is kept in metres");
    if (r.grayness > 0.1)
        r.notes.push_back(std::to_string(static_cast<int>(std::round(100 * r.grayness))) +
                          " % of the elements have intermediate densities, which no real material has");
    if (!r.converged)
        r.notes.push_back("the iteration limit was reached before the density change fell below the tolerance");
    return r;
}

TriangleMesh extrudeTopology(const TopologyResult& r)
{
    TriangleMesh mesh;
    const TopologySettings& s = r.settings;
    const double h = s.elementSize(), t = s.thickness, top = s.height();
    const auto solidAt = [&](int i, int j) { return i >= 0 && j >= 0 && i < s.nelx && j < s.nely && r.solid[static_cast<size_t>(i * s.nely + j)]; };
    const auto quad = [&](const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, const Vec3& n) {
        const auto ia = mesh.addVertex(a, n), ib = mesh.addVertex(b, n), ic = mesh.addVertex(c, n), id = mesh.addVertex(d, n);
        mesh.addTriangle(ia, ib, ic);
        mesh.addTriangle(ia, ic, id);
    };
    for (int i = 0; i < s.nelx; ++i)
        for (int j = 0; j < s.nely; ++j) {
            if (!solidAt(i, j))
                continue;
            const double x0 = i * h, x1 = x0 + h, y1 = top - j * h, y0 = y1 - h;
            quad({x0, y0, t}, {x1, y0, t}, {x1, y1, t}, {x0, y1, t}, Vec3::UnitZ());
            quad({x0, y0, 0}, {x0, y1, 0}, {x1, y1, 0}, {x1, y0, 0}, -Vec3::UnitZ());
            if (!solidAt(i - 1, j))
                quad({x0, y0, 0}, {x0, y0, t}, {x0, y1, t}, {x0, y1, 0}, -Vec3::UnitX());
            if (!solidAt(i + 1, j))
                quad({x1, y0, 0}, {x1, y1, 0}, {x1, y1, t}, {x1, y0, t}, Vec3::UnitX());
            if (!solidAt(i, j - 1))  // the row above
                quad({x0, y1, 0}, {x0, y1, t}, {x1, y1, t}, {x1, y1, 0}, Vec3::UnitY());
            if (!solidAt(i, j + 1))
                quad({x0, y0, 0}, {x1, y0, 0}, {x1, y0, t}, {x0, y0, t}, -Vec3::UnitY());
        }
    return mesh;
}

}  // namespace resonant::mechanics
