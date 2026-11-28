#include "oscillators/BlochLattice.h"

#include <Eigen/Eigenvalues>

#include <algorithm>
#include <cmath>
#include <complex>

namespace resonant::oscillators {

namespace {
constexpr double kPi = 3.14159265358979323846;
}

std::vector<double> blochFrequencies(const BlochCell& cell, double reduced)
{
    const int n = static_cast<int>(cell.mass.rows());
    const std::complex<double> phase = std::polar(1.0, reduced * kPi);  // e^{j q a}
    Eigen::MatrixXcd K = cell.stiffness.cast<std::complex<double>>();
    for (const auto& l : cell.links) {
        // Spring between u_i (this cell) and u_j of the next cell, u_j(next) = U_j e^{j q a}.
        K(l.i, l.i) += l.k;
        K(l.j, l.j) += l.k;
        K(l.i, l.j) -= l.k * phase;
        K(l.j, l.i) -= l.k * std::conj(phase);
    }
    // M^{-1/2} K M^{-1/2} is Hermitian for a diagonal (or symmetric positive) mass.
    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> mass(cell.mass);
    const Eigen::MatrixXd root = mass.operatorInverseSqrt();
    const Eigen::MatrixXcd A = root.cast<std::complex<double>>() * K * root.cast<std::complex<double>>();
    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXcd> eig(A);
    std::vector<double> f;
    for (int i = 0; i < n; ++i)
        f.push_back(std::sqrt(std::max(0.0, eig.eigenvalues()(i))) / (2 * kPi));
    std::sort(f.begin(), f.end());
    return f;
}

BlochBands blochBands(const BlochCell& cell, int wavenumbers)
{
    BlochBands b;
    const int n = static_cast<int>(cell.mass.rows());
    b.frequencies.assign(static_cast<size_t>(n), {});
    for (int i = 0; i < wavenumbers; ++i) {
        const double q = wavenumbers > 1 ? static_cast<double>(i) / (wavenumbers - 1) : 0.0;
        b.reducedWavenumber.push_back(q);
        const auto f = blochFrequencies(cell, q);
        for (int k = 0; k < n; ++k)
            b.frequencies[static_cast<size_t>(k)].push_back(f[static_cast<size_t>(k)]);
    }
    for (int k = 0; k + 1 < n; ++k) {
        const double top = *std::max_element(b.frequencies[static_cast<size_t>(k)].begin(), b.frequencies[static_cast<size_t>(k)].end());
        const double bottom = *std::min_element(b.frequencies[static_cast<size_t>(k + 1)].begin(), b.frequencies[static_cast<size_t>(k + 1)].end());
        if (bottom > top)
            b.gaps.emplace_back(top, bottom);
    }
    return b;
}

}  // namespace resonant::oscillators
