#include "mechanics/Bloch.h"

#include <Eigen/Eigenvalues>

#include <cmath>
#include <complex>

namespace resonant::mechanics {

namespace {
constexpr double pi = 3.14159265358979323846;
}

BandStructure rodBandStructure(const PeriodicRod& rod, int bands, int wavenumbers)
{
    BandStructure out;
    const int nA = rod.elementsPerSegment, nB = rod.elementsPerSegment, n = nA + nB;
    // Global matrices of the cell (n + 1 nodes).
    Eigen::MatrixXd k = Eigen::MatrixXd::Zero(n + 1, n + 1), m = Eigen::MatrixXd::Zero(n + 1, n + 1);
    const double E = rod.material.youngsModulus, rho = rod.material.density;
    for (int e = 0; e < n; ++e) {
        const bool inA = e < nA;
        const double area = inA ? rod.areaA : rod.areaB, h = inA ? rod.lengthA / nA : rod.lengthB / nB;
        const double ke = E * area / h, me = rho * area * h / 6.0;
        k(e, e) += ke;
        k(e + 1, e + 1) += ke;
        k(e, e + 1) -= ke;
        k(e + 1, e) -= ke;
        m(e, e) += 2 * me;
        m(e + 1, e + 1) += 2 * me;
        m(e, e + 1) += me;
        m(e + 1, e) += me;
    }
    out.frequencies.assign(static_cast<size_t>(bands), {});
    const double a = rod.period();
    for (int iq = 0; iq < wavenumbers; ++iq) {
        const double reduced = static_cast<double>(iq) / (wavenumbers - 1);
        const std::complex<double> phase = std::polar(1.0, pi * reduced);
        // Reduction: the last node follows the first.
        Eigen::MatrixXcd t = Eigen::MatrixXcd::Zero(n + 1, n);
        for (int i = 0; i < n; ++i)
            t(i, i) = 1.0;
        t(n, 0) = phase;
        const Eigen::MatrixXcd kq = t.adjoint() * k.cast<std::complex<double>>() * t;
        const Eigen::MatrixXcd mq = t.adjoint() * m.cast<std::complex<double>>() * t;
        Eigen::GeneralizedSelfAdjointEigenSolver<Eigen::MatrixXcd> solver(kq, mq);
        out.reducedWavenumber.push_back(reduced);
        for (int b = 0; b < bands; ++b)
            out.frequencies[static_cast<size_t>(b)].push_back(std::sqrt(std::max(0.0, solver.eigenvalues()(b))) / (2 * pi));
    }
    for (int b = 0; b + 1 < bands; ++b) {
        const auto& lower = out.frequencies[static_cast<size_t>(b)];
        const auto& upper = out.frequencies[static_cast<size_t>(b + 1)];
        const double top = *std::max_element(lower.begin(), lower.end());
        const double bottom = *std::min_element(upper.begin(), upper.end());
        if (bottom > top * (1.0 + 1e-9))
            out.stopBands.push_back({top, bottom});
    }
    out.method = "Bloch-periodic unit cell, " + std::to_string(n) + " linear rod elements, consistent mass, period "
                 + std::to_string(a * 1e3) + " mm";
    return out;
}

double rodDispersionRhs(const PeriodicRod& rod, double frequencyHz)
{
    const double k = 2 * pi * frequencyHz / rod.material.barSpeed();
    const double ratio = rod.areaA / rod.areaB;
    return std::cos(k * rod.lengthA) * std::cos(k * rod.lengthB)
           - 0.5 * (ratio + 1.0 / ratio) * std::sin(k * rod.lengthA) * std::sin(k * rod.lengthB);
}

std::vector<std::pair<double, double>> analyticalStopBands(const PeriodicRod& rod, double fmax, int samples)
{
    std::vector<std::pair<double, double>> bands;
    bool inside = false;
    double start = 0.0;
    for (int i = 1; i <= samples; ++i) {
        const double f = fmax * i / samples;
        const bool stop = std::abs(rodDispersionRhs(rod, f)) > 1.0;
        if (stop && !inside)
            start = f;
        if (!stop && inside)
            bands.push_back({start, f});
        inside = stop;
    }
    if (inside)
        bands.push_back({start, fmax});
    return bands;
}

Result<PeriodicRod> periodicRodOf(const Geometry& g, const ElasticMaterial& material, double lengthScale, double radiusScale)
{
    if (g.kindId() != "rod.periodic")
        return Result<PeriodicRod>::failure("a band structure is calculated for periodic rods only");
    PeriodicRod rod;
    rod.material = material;
    rod.areaA = pi * std::pow(g.parameter("radiusA") * radiusScale, 2);
    rod.areaB = pi * std::pow(g.parameter("radiusB") * radiusScale, 2);
    rod.lengthA = g.parameter("lengthA") * lengthScale;
    rod.lengthB = g.parameter("lengthB") * lengthScale;
    return rod;
}

}  // namespace resonant::mechanics
