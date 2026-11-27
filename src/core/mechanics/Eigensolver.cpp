#include "mechanics/Eigensolver.h"

#include <Eigen/Eigenvalues>
#include <Eigen/SparseCholesky>
#include <Eigen/SparseLU>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>

namespace resonant::mechanics {
namespace {

// Backward error ||K x - lambda M x|| / ((||K|| + |lambda| ||M||) ||x||),
// norms as largest column sums. Unlike ||r|| / ||K x|| it is meaningful for
// rigid-body modes, where K x is itself only rounding.
double norm1(const Eigen::SparseMatrix<double>& a)
{
    double largest = 0.0;
    for (Eigen::Index c = 0; c < a.outerSize(); ++c) {
        double sum = 0.0;
        for (Eigen::SparseMatrix<double>::InnerIterator it(a, c); it; ++it)
            sum += std::abs(it.value());
        largest = std::max(largest, sum);
    }
    return largest;
}

double residual(const Eigen::SparseMatrix<double>& k, const Eigen::SparseMatrix<double>& m, double lambda, const Eigen::VectorXd& x,
                double kNorm, double mNorm)
{
    const double scale = (kNorm + std::abs(lambda) * mNorm) * x.norm();
    return scale > 0.0 ? (k * x - lambda * (m * x)).norm() / scale : 0.0;
}

}  // namespace

Result<EigenSolution> lowestEigenpairs(const Eigen::SparseMatrix<double>& k, const Eigen::SparseMatrix<double>& m, int count,
                                       double shift, double tolerance, JobContext* context, bool forceIterative)
{
    using Failure = Result<EigenSolution>;
    const auto n = k.rows();
    if (n == 0)
        return Failure::failure("the structure has no free degrees of freedom");
    count = std::clamp<int>(count, 1, static_cast<int>(n));
    EigenSolution out;
    const double kNorm = norm1(k), mNorm = norm1(m);

    if (!forceIterative && static_cast<size_t>(n) <= kDenseEigenLimit) {
        const Eigen::MatrixXd denseK(k), denseM(m);
        Eigen::GeneralizedSelfAdjointEigenSolver<Eigen::MatrixXd> solver(denseK, denseM);
        if (solver.info() != Eigen::Success)
            return Failure::failure("the dense eigenvalue solver did not converge");
        out.values = solver.eigenvalues().head(count);
        out.vectors = solver.eigenvectors().leftCols(count);
        out.method = "dense generalized symmetric eigensolver (all " + std::to_string(n) + " eigenpairs)";
    } else {
        // Factor K - sigma M once.
        const Eigen::SparseMatrix<double> a = k - shift * m;
        Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> ldlt(a);
        Eigen::SparseLU<Eigen::SparseMatrix<double>> lu;
        const bool useLu = ldlt.info() != Eigen::Success;
        if (useLu) {
            lu.compute(a);
            if (lu.info() != Eigen::Success)
                return Failure::failure("K - sigma M could not be factorized: the shift is an eigenvalue or the model is invalid");
        }
        const auto applyInverse = [&](const Eigen::VectorXd& v) -> Eigen::VectorXd {
            if (useLu)
                return lu.solve(v);
            return ldlt.solve(v);
        };

        int basis = std::min<int>(static_cast<int>(n), std::max(2 * count + 20, 40));
        std::mt19937 random(12345);  // a fixed start: reproducible results
        std::uniform_real_distribution<double> uniform(-1.0, 1.0);
        Eigen::VectorXd start(n);
        for (Eigen::Index i = 0; i < n; ++i)
            start(i) = uniform(random);
        for (;;) {
            Eigen::MatrixXd q(n, basis + 1), mq(n, basis + 1);
            std::vector<double> alpha, beta;
            Eigen::VectorXd v = start;
            double norm = std::sqrt(v.dot(m * v));
            q.col(0) = v / norm;
            mq.col(0) = m * q.col(0);
            int built = 0;
            for (int j = 0; j < basis; ++j) {
                if (context && context->cancelRequested())
                    return Failure::failure("cancelled");
                Eigen::VectorXd w = applyInverse(mq.col(j));
                const double diagonal = w.dot(mq.col(j));
                alpha.push_back(diagonal);
                // Full re-orthogonalization, twice ("twice is enough").
                for (int pass = 0; pass < 2; ++pass)
                    w -= q.leftCols(j + 1) * (mq.leftCols(j + 1).transpose() * w);
                const double b = std::sqrt(std::max(0.0, w.dot(m * w)));
                built = j + 1;
                if (b < 1e-14 * std::abs(diagonal) || j + 1 == basis) {
                    beta.push_back(b);
                    break;
                }
                beta.push_back(b);
                q.col(j + 1) = w / b;
                mq.col(j + 1) = m * q.col(j + 1);
            }
            // Ritz values of the tridiagonal matrix.
            Eigen::MatrixXd t = Eigen::MatrixXd::Zero(built, built);
            for (int j = 0; j < built; ++j) {
                t(j, j) = alpha[static_cast<size_t>(j)];
                if (j + 1 < built)
                    t(j, j + 1) = t(j + 1, j) = beta[static_cast<size_t>(j)];
            }
            Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> tri(t);
            // Largest theta = eigenvalue nearest above the shift.
            std::vector<int> order(static_cast<size_t>(built));
            std::iota(order.begin(), order.end(), 0);
            std::sort(order.begin(), order.end(), [&](int x, int y) { return tri.eigenvalues()(x) > tri.eigenvalues()(y); });
            const int found = std::min(count, built);
            out.values.resize(found);
            out.vectors.resize(n, found);
            bool converged = true;
            for (int r = 0; r < found; ++r) {
                const double theta = tri.eigenvalues()(order[static_cast<size_t>(r)]);
                out.values(r) = shift + 1.0 / theta;
                Eigen::VectorXd x = q.leftCols(built) * tri.eigenvectors().col(order[static_cast<size_t>(r)]);
                x /= std::sqrt(x.dot(m * x));
                out.vectors.col(r) = x;
                if (residual(k, m, out.values(r), x, kNorm, mNorm) > tolerance)
                    converged = false;
            }
            out.basisSize = built;
            if ((converged && found == count) || basis >= n)
                break;
            basis = std::min<int>(static_cast<int>(n), 2 * basis);
        }
        // Ascending order.
        std::vector<int> order(static_cast<size_t>(out.values.size()));
        std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(), [&](int x, int y) { return out.values(x) < out.values(y); });
        Eigen::VectorXd values(out.values.size());
        Eigen::MatrixXd vectors(n, out.values.size());
        for (size_t r = 0; r < order.size(); ++r) {
            values(static_cast<Eigen::Index>(r)) = out.values(order[r]);
            vectors.col(static_cast<Eigen::Index>(r)) = out.vectors.col(order[r]);
        }
        out.values = values;
        out.vectors = vectors;
        out.method = "shift-invert Lanczos with full re-orthogonalization (" + std::to_string(out.basisSize)
                     + " Lanczos vectors, one sparse factorization)";
    }
    for (Eigen::Index r = 0; r < out.values.size(); ++r) {
        // A sign convention for reproducible shapes: the largest component positive.
        Eigen::Index largest = 0;
        out.vectors.col(r).cwiseAbs().maxCoeff(&largest);
        if (out.vectors(largest, r) < 0.0)
            out.vectors.col(r) *= -1.0;
        out.residuals.push_back(residual(k, m, out.values(r), out.vectors.col(r), kNorm, mNorm));
    }
    return out;
}

}  // namespace resonant::mechanics
