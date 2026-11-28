#pragma once

// Bloch-Floquet analysis of periodic lumped lattices (RES-META-003): a unit
// cell of coordinates with its mass and stiffness matrices, and springs from
// a coordinate of this cell to one of the next. With u(cell n) = U e^{j q n a}
// the stiffness becomes the Hermitian K(q) and K(q) U = w^2 M U gives the bands
// over the first Brillouin zone q a in [0, pi].

#include <Eigen/Dense>

#include <utility>
#include <vector>

namespace resonant::oscillators {

struct BlochCell {
    Eigen::MatrixXd mass;       // n x n
    Eigen::MatrixXd stiffness;  // within the cell, grounded springs included
    struct Link {
        int i = 0;   // coordinate in this cell
        int j = 0;   // coordinate in the next cell
        double k = 0.0;
    };
    std::vector<Link> links;
    double period = 1.0;        // a, m
};

// Natural frequencies (Hz, ascending) at a reduced wavenumber q a / pi in [0, 1].
std::vector<double> blochFrequencies(const BlochCell& cell, double reducedWavenumber);

struct BlochBands {
    std::vector<double> reducedWavenumber;           // q a / pi
    std::vector<std::vector<double>> frequencies;    // [band][q], Hz
    std::vector<std::pair<double, double>> gaps;     // between consecutive bands (lower, upper), Hz
};
BlochBands blochBands(const BlochCell& cell, int wavenumbers = 101);

}  // namespace resonant::oscillators
