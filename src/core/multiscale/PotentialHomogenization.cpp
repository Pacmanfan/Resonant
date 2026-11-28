#include "multiscale/PotentialHomogenization.h"

#include <Eigen/IterativeLinearSolvers>
#include <Eigen/Sparse>

#include <algorithm>
#include <cmath>

namespace resonant::multiscale {

namespace {

using Mat8 = Eigen::Matrix<double, 8, 8>;
using Vec8 = Eigen::Matrix<double, 8, 1>;
using Mat38 = Eigen::Matrix<double, 3, 8>;

// The element of a box h0 x h1 x h2: M[i][j] = int dN/dx_i dN/dx_j dV, D = int grad N dV.
struct Element {
    std::array<std::array<Mat8, 3>, 3> M;
    Mat38 D = Mat38::Zero();
    double volume = 0.0;

    explicit Element(const std::array<double, 3>& h)
    {
        for (auto& row : M)
            for (auto& m : row)
                m.setZero();
        volume = h[0] * h[1] * h[2];
        const double g = 1.0 / std::sqrt(3.0), w = volume / 8.0;
        for (int gz = 0; gz < 2; ++gz)
            for (int gy = 0; gy < 2; ++gy)
                for (int gx = 0; gx < 2; ++gx) {
                    const double p[3] = {gx ? g : -g, gy ? g : -g, gz ? g : -g};
                    Mat38 B;
                    for (int a = 0; a < 8; ++a) {
                        const double s[3] = {(a & 1) ? 1.0 : -1.0, (a & 2) ? 1.0 : -1.0, (a & 4) ? 1.0 : -1.0};
                        const double f[3] = {1 + s[0] * p[0], 1 + s[1] * p[1], 1 + s[2] * p[2]};
                        B(0, a) = 0.125 * s[0] * f[1] * f[2] * 2.0 / h[0];
                        B(1, a) = 0.125 * f[0] * s[1] * f[2] * 2.0 / h[1];
                        B(2, a) = 0.125 * f[0] * f[1] * s[2] * 2.0 / h[2];
                    }
                    for (int i = 0; i < 3; ++i)
                        for (int j = 0; j < 3; ++j)
                            M[static_cast<size_t>(i)][static_cast<size_t>(j)] += w * B.row(i).transpose() * B.row(j);
                    D += w * B;
                }
    }

    Mat8 stiffness(const Eigen::Matrix3d& K) const
    {
        Mat8 k = Mat8::Zero();
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                if (K(i, j) != 0.0)
                    k += K(i, j) * M[static_cast<size_t>(i)][static_cast<size_t>(j)];
        return k;
    }
};

// The eight nodes of voxel (i, j, k) in the local order a = ax + 2 ay + 4 az.
template <typename NodeOf>
std::array<int, 8> nodesOf(int i, int j, int k, NodeOf node)
{
    std::array<int, 8> n{};
    for (int a = 0; a < 8; ++a)
        n[static_cast<size_t>(a)] = node(i + (a & 1), j + ((a >> 1) & 1), k + ((a >> 2) & 1));
    return n;
}

struct Solved {
    Eigen::Vector3d flux = Eigen::Vector3d::Zero();  // <K grad u>
    double energy = 0.0;                              // <grad u . K grad u>
    double residual = 0.0;
    int iterations = 0;
};

Result<Eigen::VectorXd> solveSystem(const Eigen::SparseMatrix<double>& A, const Eigen::VectorXd& b, double tol, double& error, int& iterations)
{
    Eigen::ConjugateGradient<Eigen::SparseMatrix<double>, Eigen::Lower | Eigen::Upper, Eigen::IncompleteCholesky<double>> cg;
    cg.setTolerance(tol);
    cg.setMaxIterations(std::max<Eigen::Index>(1000, 4 * A.rows()));
    cg.compute(A);
    if (cg.info() != Eigen::Success)
        return Result<Eigen::VectorXd>::failure("the preconditioner could not be built");
    Eigen::VectorXd x = cg.solve(b);
    error = cg.error();
    iterations = static_cast<int>(cg.iterations());
    if (cg.info() != Eigen::Success && error > 1e3 * tol)
        return Result<Eigen::VectorXd>::failure("conjugate gradients did not converge (relative residual " + std::to_string(error) + ")");
    return x;
}

// Periodic: u = G.x + phi, phi periodic on the nx x ny x nz node grid; node 0 fixed.
Result<Solved> periodic(const PotentialProblem& p, const Element& e, const Eigen::Vector3d& G, double tol)
{
    const int nx = p.n[0], ny = p.n[1], nz = p.n[2];
    const auto node = [&](int i, int j, int k) { return (i % nx) + nx * ((j % ny) + ny * (k % nz)); };
    const int N = nx * ny * nz;
    std::vector<Eigen::Triplet<double>> t;
    t.reserve(static_cast<size_t>(N) * 64);
    Eigen::VectorXd b = Eigen::VectorXd::Zero(N);
    std::vector<Mat8> Ke(static_cast<size_t>(N));
    for (int k = 0; k < nz; ++k)
        for (int j = 0; j < ny; ++j)
            for (int i = 0; i < nx; ++i) {
                const size_t v = static_cast<size_t>(i) + static_cast<size_t>(nx) * (j + static_cast<size_t>(ny) * k);
                Ke[v] = e.stiffness(p.K[v]);
                const auto n = nodesOf(i, j, k, node);
                const Vec8 f = -(e.D.transpose() * (p.K[v] * G));
                for (int a = 0; a < 8; ++a) {
                    b(n[static_cast<size_t>(a)]) += f(a);
                    for (int c = 0; c < 8; ++c)
                        t.emplace_back(n[static_cast<size_t>(a)], n[static_cast<size_t>(c)], Ke[v](a, c));
                }
            }
    Eigen::SparseMatrix<double> A(N, N);
    A.setFromTriplets(t.begin(), t.end());
    // Fix phi(node 0) = 0: drop its row and column.
    Eigen::SparseMatrix<double> Ar = A.bottomRightCorner(N - 1, N - 1);
    Eigen::VectorXd br = b.tail(N - 1);
    Solved s;
    double err = 0.0;
    auto x = solveSystem(Ar, br, tol, err, s.iterations);
    if (!x)
        return Result<Solved>::failure(x.message());
    Eigen::VectorXd phi(N);
    phi(0) = 0.0;
    phi.tail(N - 1) = x.value();
    s.residual = err;
    double volume = 0.0;
    for (int k = 0; k < nz; ++k)
        for (int j = 0; j < ny; ++j)
            for (int i = 0; i < nx; ++i) {
                const size_t v = static_cast<size_t>(i) + static_cast<size_t>(nx) * (j + static_cast<size_t>(ny) * k);
                const auto n = nodesOf(i, j, k, node);
                Vec8 pe;
                for (int a = 0; a < 8; ++a)
                    pe(a) = phi(n[static_cast<size_t>(a)]);
                const Eigen::Vector3d gradIntegral = G * e.volume + e.D * pe;  // int grad u dV
                s.flux += p.K[v] * gradIntegral;
                s.energy += G.dot(p.K[v] * G) * e.volume + 2.0 * G.dot(p.K[v] * (e.D * pe)) + pe.dot(Ke[v] * pe);
                volume += e.volume;
            }
    s.flux /= volume;
    s.energy /= volume;
    return s;
}

// Linear (all boundary nodes) or mixed (the faces normal to `axis`): u = G.x prescribed there.
Result<Solved> prescribed(const PotentialProblem& p, const Element& e, const Eigen::Vector3d& G, int axis, double tol)
{
    const int nx = p.n[0], ny = p.n[1], nz = p.n[2];
    const int mx = nx + 1, my = ny + 1, mz = nz + 1;
    const auto node = [&](int i, int j, int k) { return i + mx * (j + my * k); };
    const int N = mx * my * mz;
    const double h[3] = {p.size[0] / nx, p.size[1] / ny, p.size[2] / nz};
    std::vector<int> unknown(static_cast<size_t>(N), -1);
    Eigen::VectorXd u = Eigen::VectorXd::Zero(N);
    int free = 0;
    for (int k = 0; k < mz; ++k)
        for (int j = 0; j < my; ++j)
            for (int i = 0; i < mx; ++i) {
                const int idx[3] = {i, j, k}, m[3] = {nx, ny, nz};
                bool fixed = false;
                if (axis < 0) {
                    for (int a = 0; a < 3; ++a)
                        fixed = fixed || idx[a] == 0 || idx[a] == m[a];
                } else {
                    fixed = idx[axis] == 0 || idx[axis] == m[axis];
                }
                const int n = node(i, j, k);
                if (fixed)
                    u(n) = G(0) * i * h[0] + G(1) * j * h[1] + G(2) * k * h[2];
                else
                    unknown[static_cast<size_t>(n)] = free++;
            }
    std::vector<Eigen::Triplet<double>> t;
    t.reserve(static_cast<size_t>(nx) * ny * nz * 64);
    Eigen::VectorXd b = Eigen::VectorXd::Zero(free);
    std::vector<Mat8> Ke(static_cast<size_t>(nx) * ny * nz);
    for (int k = 0; k < nz; ++k)
        for (int j = 0; j < ny; ++j)
            for (int i = 0; i < nx; ++i) {
                const size_t v = static_cast<size_t>(i) + static_cast<size_t>(nx) * (j + static_cast<size_t>(ny) * k);
                Ke[v] = e.stiffness(p.K[v]);
                const auto n = nodesOf(i, j, k, node);
                for (int a = 0; a < 8; ++a) {
                    const int ra = unknown[static_cast<size_t>(n[static_cast<size_t>(a)])];
                    if (ra < 0)
                        continue;
                    for (int c = 0; c < 8; ++c) {
                        const int rc = unknown[static_cast<size_t>(n[static_cast<size_t>(c)])];
                        if (rc >= 0)
                            t.emplace_back(ra, rc, Ke[v](a, c));
                        else
                            b(ra) -= Ke[v](a, c) * u(n[static_cast<size_t>(c)]);
                    }
                }
            }
    Solved s;
    if (free > 0) {
        Eigen::SparseMatrix<double> A(free, free);
        A.setFromTriplets(t.begin(), t.end());
        double err = 0.0;
        auto x = solveSystem(A, b, tol, err, s.iterations);
        if (!x)
            return Result<Solved>::failure(x.message());
        s.residual = err;
        for (int n = 0; n < N; ++n)
            if (unknown[static_cast<size_t>(n)] >= 0)
                u(n) = x.value()(unknown[static_cast<size_t>(n)]);
    }
    double volume = 0.0;
    for (int k = 0; k < nz; ++k)
        for (int j = 0; j < ny; ++j)
            for (int i = 0; i < nx; ++i) {
                const size_t v = static_cast<size_t>(i) + static_cast<size_t>(nx) * (j + static_cast<size_t>(ny) * k);
                const auto n = nodesOf(i, j, k, node);
                Vec8 ue;
                for (int a = 0; a < 8; ++a)
                    ue(a) = u(n[static_cast<size_t>(a)]);
                s.flux += p.K[v] * (e.D * ue);
                s.energy += ue.dot(Ke[v] * ue);
                volume += e.volume;
            }
    s.flux /= volume;
    s.energy /= volume;
    return s;
}

// Static uniform: K grad u . n = Q.n on every boundary face; node 0 fixed (the problem is
// defined up to a constant). Returns <grad u> in `flux` (sic: the averaged gradient) and the energy.
Result<Solved> uniformFlux(const PotentialProblem& p, const Element& e, const Eigen::Vector3d& Q, double tol)
{
    const int nx = p.n[0], ny = p.n[1], nz = p.n[2];
    const int mx = nx + 1, my = ny + 1, mz = nz + 1;
    const auto node = [&](int i, int j, int k) { return i + mx * (j + my * k); };
    const int N = mx * my * mz;
    const double h[3] = {p.size[0] / nx, p.size[1] / ny, p.size[2] / nz};
    Eigen::VectorXd b = Eigen::VectorXd::Zero(N);
    // A bilinear face's four nodes share its area equally.
    for (int axis = 0; axis < 3; ++axis) {
        const int u = (axis + 1) % 3, w = (axis + 2) % 3;
        const int m[3] = {nx, ny, nz};
        const double area = h[u] * h[w];
        for (int side = 0; side < 2; ++side) {
            const double qn = side ? Q(axis) : -Q(axis);
            for (int a = 0; a < m[u]; ++a)
                for (int c = 0; c < m[w]; ++c)
                    for (int da = 0; da < 2; ++da)
                        for (int dc = 0; dc < 2; ++dc) {
                            int idx[3];
                            idx[axis] = side ? m[axis] : 0;
                            idx[u] = a + da;
                            idx[w] = c + dc;
                            b(node(idx[0], idx[1], idx[2])) += 0.25 * area * qn;
                        }
        }
    }
    std::vector<Eigen::Triplet<double>> t;
    t.reserve(static_cast<size_t>(nx) * ny * nz * 64);
    std::vector<Mat8> Ke(static_cast<size_t>(nx) * ny * nz);
    for (int k = 0; k < nz; ++k)
        for (int j = 0; j < ny; ++j)
            for (int i = 0; i < nx; ++i) {
                const size_t v = static_cast<size_t>(i) + static_cast<size_t>(nx) * (j + static_cast<size_t>(ny) * k);
                Ke[v] = e.stiffness(p.K[v]);
                const auto n = nodesOf(i, j, k, node);
                for (int a = 0; a < 8; ++a)
                    for (int c = 0; c < 8; ++c)
                        t.emplace_back(n[static_cast<size_t>(a)], n[static_cast<size_t>(c)], Ke[v](a, c));
            }
    Eigen::SparseMatrix<double> A(N, N);
    A.setFromTriplets(t.begin(), t.end());
    Eigen::SparseMatrix<double> Ar = A.bottomRightCorner(N - 1, N - 1);
    Solved s;
    double err = 0.0;
    auto x = solveSystem(Ar, b.tail(N - 1), tol, err, s.iterations);
    if (!x)
        return Result<Solved>::failure(x.message());
    s.residual = err;
    Eigen::VectorXd u(N);
    u(0) = 0.0;
    u.tail(N - 1) = x.value();
    double volume = 0.0;
    for (int k = 0; k < nz; ++k)
        for (int j = 0; j < ny; ++j)
            for (int i = 0; i < nx; ++i) {
                const size_t v = static_cast<size_t>(i) + static_cast<size_t>(nx) * (j + static_cast<size_t>(ny) * k);
                const auto n = nodesOf(i, j, k, node);
                Vec8 ue;
                for (int a = 0; a < 8; ++a)
                    ue(a) = u(n[static_cast<size_t>(a)]);
                s.flux += e.D * ue;
                s.energy += ue.dot(Ke[v] * ue);
                volume += e.volume;
            }
    s.flux /= volume;
    s.energy /= volume;
    return s;
}

}  // namespace

std::string toString(Boundary b)
{
    switch (b) {
    case Boundary::Periodic: return "periodic";
    case Boundary::Linear: return "linear";
    case Boundary::Mixed: return "mixed";
    case Boundary::Flux: return "flux";
    }
    return "periodic";
}

Result<PotentialResult> homogenize(const PotentialProblem& p, Boundary boundary, double tol, const std::function<bool()>& cancelled)
{
    using R = Result<PotentialResult>;
    for (int a = 0; a < 3; ++a)
        if (p.n[static_cast<size_t>(a)] < 1 || !(p.size[static_cast<size_t>(a)] > 0.0))
            return R::failure("the RVE needs voxels and a positive size along every axis");
    const size_t count = static_cast<size_t>(p.n[0]) * p.n[1] * p.n[2];
    if (p.K.size() != count)
        return R::failure("one conductivity tensor per voxel");
    for (const Eigen::Matrix3d& K : p.K) {
        if (!K.allFinite() || (K - K.transpose()).norm() > 1e-9 * K.norm())
            return R::failure("every voxel's tensor must be finite and symmetric");
        if (Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d>(K).eigenvalues().minCoeff() <= 0.0)
            return R::failure("every voxel's tensor must be positive definite");
    }
    if (boundary == Boundary::Periodic && !p.periodic)
        return R::failure("periodic boundary conditions need a periodic microstructure; use linear or mixed conditions");
    if (boundary == Boundary::Periodic && count < 2)
        return R::failure("a periodic RVE needs at least two voxels");
    const Element e({p.size[0] / p.n[0], p.size[1] / p.n[1], p.size[2] / p.n[2]});
    PotentialResult out;
    out.boundary = boundary;
    out.unknowns = boundary == Boundary::Periodic ? static_cast<int>(count) - 1
                                                  : static_cast<int>((p.n[0] + 1) * (p.n[1] + 1) * (p.n[2] + 1));
    if (boundary == Boundary::Flux) {
        // Three uniform-flux cases: <grad u> for Q = e_j; K_eff = (grad matrix)^-1.
        Eigen::Matrix3d grads;
        double energy[3] = {0, 0, 0};
        for (int axis = 0; axis < 3; ++axis) {
            if (cancelled && cancelled())
                return R::failure("cancelled");
            auto f = uniformFlux(p, e, Eigen::Vector3d::Unit(axis), tol);
            if (!f)
                return R::failure(f.message());
            grads.col(axis) = f.value().flux;
            energy[axis] = f.value().energy;
            out.residual = std::max(out.residual, f.value().residual);
            out.iterations = std::max(out.iterations, f.value().iterations);
        }
        grads = 0.5 * (grads + grads.transpose()).eval();
        out.effective = grads.inverse();
        // Hill-Mandel: <grad u . K grad u> = Q . <grad u>.
        for (int axis = 0; axis < 3; ++axis)
            out.hillMandel = std::max(out.hillMandel, std::abs(energy[axis] - grads(axis, axis)) / std::max(1e-300, std::abs(grads(axis, axis))));
        out.effective = 0.5 * (out.effective + out.effective.transpose()).eval();
        return out;
    }
    for (int axis = 0; axis < 3; ++axis) {
        if (cancelled && cancelled())
            return R::failure("cancelled");
        const Eigen::Vector3d G = Eigen::Vector3d::Unit(axis);
        auto s = boundary == Boundary::Periodic ? periodic(p, e, G, tol)
                                                : prescribed(p, e, G, boundary == Boundary::Linear ? -1 : axis, tol);
        if (!s)
            return R::failure(s.message());
        out.effective.col(axis) = s.value().flux;
        const double gkg = G.dot(s.value().flux);
        out.hillMandel = std::max(out.hillMandel, std::abs(s.value().energy - gkg) / std::max(1e-300, std::abs(gkg)));
        out.residual = std::max(out.residual, s.value().residual);
        out.iterations = std::max(out.iterations, s.value().iterations);
    }
    if (boundary != Boundary::Mixed)
        out.effective = 0.5 * (out.effective + out.effective.transpose()).eval();
    return out;
}

TwoPhaseBounds twoPhaseBounds(double k1, double k2, double f)
{
    TwoPhaseBounds b;
    b.voigt = (1 - f) * k1 + f * k2;
    b.reuss = 1.0 / ((1 - f) / k1 + f / k2);
    // Hashin-Shtrikman with either phase as the matrix (3D).
    const auto hs = [](double km, double ki, double fi) {
        if (ki == km)
            return km;
        return km + fi / (1.0 / (ki - km) + (1.0 - fi) / (3.0 * km));
    };
    const double a = hs(k1, k2, f), c = hs(k2, k1, 1.0 - f);
    b.hashinLower = std::min(a, c);
    b.hashinUpper = std::max(a, c);
    b.maxwellGarnett = k1 * (k2 + 2 * k1 + 2 * f * (k2 - k1)) / (k2 + 2 * k1 - f * (k2 - k1));
    return b;
}

double selfConsistentUniaxial(double ka, double kc)
{
    const auto g = [&](double k) { return 2.0 * (ka - k) / (ka + 2.0 * k) + (kc - k) / (kc + 2.0 * k); };
    double lo = std::min(ka, kc), hi = std::max(ka, kc);
    for (int i = 0; i < 200; ++i) {
        const double mid = 0.5 * (lo + hi);
        (g(mid) > 0.0 ? lo : hi) = mid;
    }
    return 0.5 * (lo + hi);
}

}  // namespace resonant::multiscale
