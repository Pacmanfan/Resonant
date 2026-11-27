#pragma once

// Lowest eigenpairs of the generalized symmetric problem K x = lambda M x
// (K symmetric positive semi-definite, M symmetric positive definite).
//
// Small problems are solved densely (all eigenpairs, LAPACK-quality
// Eigen::GeneralizedSelfAdjointEigenSolver). Larger ones use shift-invert
// Lanczos with full re-orthogonalization in the M inner product: one sparse
// factorization of K - sigma M, then a Krylov basis of (K - sigma M)^-1 M.
// The shift sigma lies below the lowest eigenvalue, so free structures with
// rigid-body modes (lambda = 0) are handled. Every returned pair carries its
// backward error ||K x - lambda M x|| / ((||K|| + |lambda| ||M||) ||x||),
// never just a claim of convergence.
#include "foundation/Result.h"
#include "simulation/Job.h"

#include <Eigen/Sparse>

#include <string>
#include <vector>

namespace resonant::mechanics {

struct EigenSolution {
    Eigen::VectorXd values;    // lambda = omega^2, ascending
    Eigen::MatrixXd vectors;   // columns, M-normalized: x^T M x = 1
    std::vector<double> residuals;
    std::string method;
    int basisSize = 0;         // Lanczos vectors used (0 for the dense method)
};

inline constexpr size_t kDenseEigenLimit = 1200;

Result<EigenSolution> lowestEigenpairs(const Eigen::SparseMatrix<double>& k, const Eigen::SparseMatrix<double>& m, int count,
                                       double shift, double tolerance = 1e-8, JobContext* context = nullptr,
                                       bool forceIterative = false);

}  // namespace resonant::mechanics
