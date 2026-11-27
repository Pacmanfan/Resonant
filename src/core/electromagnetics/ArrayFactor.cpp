#include "electromagnetics/ArrayFactor.h"

#include <Eigen/SVD>

#include <algorithm>
#include <cmath>

namespace resonant::em {

std::string_view toString(ElementPattern pattern)
{
    switch (pattern) {
    case ElementPattern::Isotropic: return "isotropic";
    case ElementPattern::ShortDipole: return "shortDipole";
    case ElementPattern::HalfWaveDipole: return "halfWaveDipole";
    case ElementPattern::CosineHemisphere: return "cosineHemisphere";
    }
    return "isotropic";
}

Vec3 directionOf(double theta, double phi)
{
    return Vec3(std::sin(theta) * std::cos(phi), std::sin(theta) * std::sin(phi), std::cos(theta));
}

double elementPatternAmplitude(ElementPattern pattern, const Vec3& axis, const Vec3& d)
{
    const double c = std::clamp(axis.normalized().dot(d), -1.0, 1.0), s = std::sqrt(std::max(0.0, 1.0 - c * c));
    switch (pattern) {
    case ElementPattern::Isotropic: return 1.0;
    case ElementPattern::ShortDipole: return s;
    case ElementPattern::HalfWaveDipole: return s > 1e-9 ? std::cos(0.5 * kPi * c) / s : 0.0;
    case ElementPattern::CosineHemisphere: return std::max(0.0, c);
    }
    return 1.0;
}

Complex arrayFactor(const ArrayModel& array, const Vec3& d, double f)
{
    const double k = wavenumber(f), omega = 2.0 * kPi * f;
    Complex sum(0.0, 0.0);
    for (const ArrayElement& e : array.elements)
        if (e.active)
            sum += e.weight * std::polar(1.0, k * d.dot(e.position) - omega * e.delay);
    return sum;
}

Complex farFieldResponse(const ArrayModel& array, const Vec3& d, double f)
{
    const double k = wavenumber(f), omega = 2.0 * kPi * f;
    Complex sum(0.0, 0.0);
    for (const ArrayElement& e : array.elements)
        if (e.active)
            sum += elementPatternAmplitude(array.pattern, e.axis, d) * e.weight * std::polar(1.0, k * d.dot(e.position) - omega * e.delay);
    return sum;
}

void steer(ArrayModel& array, const Vec3& direction, double f, SteeringMethod method)
{
    const Vec3 d = direction.normalized();
    const double k = wavenumber(f);
    double lowest = 0.0;
    for (const ArrayElement& e : array.elements)
        lowest = std::min(lowest, d.dot(e.position));
    for (ArrayElement& e : array.elements) {
        const double amplitude = std::abs(e.weight);
        if (method == SteeringMethod::Phase) {
            e.weight = std::polar(amplitude, -k * d.dot(e.position));
            e.delay = 0.0;
        } else {
            e.weight = Complex(amplitude, 0.0);
            e.delay = (d.dot(e.position) - lowest) / kSpeedOfLight;
        }
    }
}

std::vector<double> chebyshevWeights(int m, double sidelobeDb)
{
    if (m < 2)
        return std::vector<double>(static_cast<size_t>(std::max(m, 1)), 1.0);
    const int order = m - 1;
    const double r = std::pow(10.0, std::abs(sidelobeDb) / 20.0);
    const double beta = std::cosh(std::acosh(r) / order);
    std::vector<std::complex<double>> p(static_cast<size_t>(m));
    for (int k = 0; k < m; ++k) {
        const double x = beta * std::cos(kPi * k / m);
        double value;
        if (x > 1.0)
            value = std::cosh(order * std::acosh(x));
        else if (x < -1.0)
            value = (2 * (m % 2) - 1) * std::cosh(order * std::acosh(-x));
        else
            value = std::cos(order * std::acos(x));
        p[static_cast<size_t>(k)] = value;
        if (m % 2 == 0)
            p[static_cast<size_t>(k)] *= std::polar(1.0, kPi * k / m);
    }
    // Discrete Fourier transform (real part).
    std::vector<double> w(static_cast<size_t>(m));
    for (int n = 0; n < m; ++n) {
        std::complex<double> sum(0.0, 0.0);
        for (int k = 0; k < m; ++k)
            sum += p[static_cast<size_t>(k)] * std::polar(1.0, -2.0 * kPi * n * k / m);
        w[static_cast<size_t>(n)] = sum.real();
    }
    std::vector<double> out;
    if (m % 2) {
        const int half = (m + 1) / 2;
        for (int i = half - 1; i >= 1; --i)
            out.push_back(w[static_cast<size_t>(i)]);
        for (int i = 0; i < half; ++i)
            out.push_back(w[static_cast<size_t>(i)]);
    } else {
        const int half = m / 2 + 1;
        for (int i = half - 1; i >= 1; --i)
            out.push_back(w[static_cast<size_t>(i)]);
        for (int i = 1; i < half; ++i)
            out.push_back(w[static_cast<size_t>(i)]);
    }
    const double largest = *std::max_element(out.begin(), out.end());
    for (double& v : out)
        v /= largest;
    return out;
}

PatternAnalysis analyzePattern(const ArrayModel& array, double f, double resolutionDeg)
{
    PatternAnalysis a;
    a.resolutionDeg = resolutionDeg;
    const int nt = static_cast<int>(std::lround(180.0 / resolutionDeg)) + 1;
    const int np = static_cast<int>(std::lround(360.0 / (2.0 * resolutionDeg)));
    const double dt = kPi / (nt - 1), dp = 2.0 * kPi / np;
    std::vector<double> u(static_cast<size_t>(nt) * static_cast<size_t>(np));
    const auto at = [&](int i, int j) -> double& { return u[static_cast<size_t>(i) * static_cast<size_t>(np) + static_cast<size_t>((j % np + np) % np)]; };
    double total = 0.0, peak = -1.0;
    int pi = 0, pj = 0;
    for (int i = 0; i < nt; ++i) {
        const double theta = i * dt;
        for (int j = 0; j < np; ++j) {
            const double value = std::norm(farFieldResponse(array, directionOf(theta, j * dp), f));
            at(i, j) = value;
            // Midpoint-like weights: sin(theta) dtheta dphi, half cells at the poles.
            const double band = (i == 0 || i == nt - 1) ? (1.0 - std::cos(0.5 * dt)) : std::sin(theta) * dt;
            total += value * band * dp;
            if (value > peak) {
                peak = value;
                pi = i;
                pj = j;
            }
        }
    }
    a.mainTheta = pi * dt;
    a.mainPhi = pj * dp;
    a.mainLobe = directionOf(a.mainTheta, a.mainPhi);
    a.directivity = total > 0.0 ? 4.0 * kPi * peak / total : 0.0;
    a.directivityDbi = 10.0 * std::log10(std::max(1e-300, a.directivity));
    // Half-power widths through the peak, interpolated.
    const auto width = [&](bool alongTheta) {
        double sum = 0.0;
        for (const int sign : {-1, 1}) {
            int steps = 0;
            double previous = peak;
            while (steps < (alongTheta ? nt : np)) {
                ++steps;
                const int i = alongTheta ? pi + sign * steps : pi, j = alongTheta ? pj : pj + sign * steps;
                double value;
                if (alongTheta && (i < 0 || i >= nt)) {
                    // Over the pole: continue on the opposite meridian.
                    const int mirrored = i < 0 ? -i : 2 * (nt - 1) - i;
                    value = at(std::clamp(mirrored, 0, nt - 1), pj + np / 2);
                } else {
                    value = at(i, j);
                }
                if (value < 0.5 * peak) {
                    const double frac = (previous - 0.5 * peak) / (previous - value);
                    sum += (steps - 1 + frac);
                    break;
                }
                previous = value;
            }
        }
        return sum * (alongTheta ? dt : dp * std::sin(a.mainTheta));
    };
    a.beamwidthTheta = width(true);
    a.beamwidthPhi = width(false);
    // Local maxima other than the main one.
    double side = 0.0;
    for (int i = 0; i < nt; ++i)
        for (int j = 0; j < np; ++j) {
            const double v = at(i, j);
            if (v <= 1e-12 * peak || (i == pi && j == pj))
                continue;
            bool maximum = true;
            for (int di = -1; di <= 1 && maximum; ++di)
                for (int dj = -1; dj <= 1; ++dj) {
                    if (!di && !dj)
                        continue;
                    const int ii = i + di;
                    if (ii < 0 || ii >= nt)
                        continue;
                    if (at(ii, j + dj) > v) {
                        maximum = false;
                        break;
                    }
                }
            if (!maximum)
                continue;
            const Vec3 d = directionOf(i * dt, j * dp);
            const double separation = std::acos(std::clamp(d.dot(a.mainLobe), -1.0, 1.0));
            if (separation < 0.5 * std::max(a.beamwidthTheta, dt * 2))
                continue;  // part of the main lobe (a sampling ridge)
            // The poles repeat for every phi; count them once.
            if ((i == 0 || i == nt - 1) && j != 0)
                continue;
            if (v > std::pow(10.0, -0.1) * peak) {
                bool known = false;
                for (const Vec3& g : a.equalMaxima)
                    known = known || std::acos(std::clamp(g.dot(d), -1.0, 1.0)) < a.beamwidthTheta;
                if (!known)
                    a.equalMaxima.push_back(d);
            } else {
                side = std::max(side, v);
            }
        }
    a.sidelobeDb = side > 0.0 ? 10.0 * std::log10(side / peak) : 0.0;
    return a;
}

std::vector<Vec3> predictedGratingLobes(const std::vector<Vec3>& lattice, const Vec3& steering, double f)
{
    std::vector<Vec3> lobes;
    if (lattice.empty() || lattice.size() > 2)
        return lobes;
    const double lambda = wavelength(f);
    // Reciprocal vectors in the plane of the lattice.
    std::vector<Vec3> b;
    if (lattice.size() == 1) {
        b.push_back(lattice[0] / lattice[0].squaredNorm());
    } else {
        const Vec3 n = lattice[0].cross(lattice[1]);
        b.push_back(lattice[1].cross(n) / lattice[0].dot(lattice[1].cross(n)));
        b.push_back(n.cross(lattice[0]) / lattice[1].dot(n.cross(lattice[0])));
    }
    const Vec3 s = steering.normalized();
    const int range = 8;
    for (int m = -range; m <= range; ++m)
        for (int n = (b.size() == 2 ? -range : 0); n <= (b.size() == 2 ? range : 0); ++n) {
            if (m == 0 && n == 0)
                continue;
            const Vec3 shift = lambda * (m * b[0] + (b.size() == 2 ? n * b[1] : Vec3::Zero().eval()));
            // Only the component of the direction along the lattice is fixed;
            // the rest adjusts to keep a unit vector (visible if possible).
            Vec3 d = s + shift;
            if (b.size() == 1) {
                const Vec3 u = lattice[0].normalized();
                const double along = d.dot(u);
                if (std::abs(along) > 1.0)
                    continue;
                Vec3 across = s - s.dot(u) * u;
                const double rest = std::sqrt(std::max(0.0, 1.0 - along * along));
                across = across.norm() > 1e-12 ? Vec3(across.normalized() * rest) : Vec3(u.unitOrthogonal() * rest);
                d = along * u + across;
            } else {
                const Vec3 normal = lattice[0].cross(lattice[1]).normalized();
                const Vec3 inPlane = d - d.dot(normal) * normal;
                if (inPlane.norm() > 1.0)
                    continue;
                const double side = s.dot(normal) >= 0.0 ? 1.0 : -1.0;
                d = inPlane + side * std::sqrt(std::max(0.0, 1.0 - inPlane.squaredNorm())) * normal;
            }
            lobes.push_back(d.normalized());
        }
    return lobes;
}

Result<BeamSynthesis> synthesizeBeams(const ArrayModel& array, const std::vector<BeamTarget>& targets, double f, double regularization,
                                      std::optional<double> amplitudeLimit)
{
    using Failure = Result<BeamSynthesis>;
    std::vector<size_t> active;
    for (size_t i = 0; i < array.elements.size(); ++i)
        if (array.elements[i].active)
            active.push_back(i);
    if (active.empty())
        return Failure::failure("the array has no active element");
    if (targets.empty())
        return Failure::failure("no beam or null directions are given");
    const double k = wavenumber(f);
    const auto rows = static_cast<Eigen::Index>(targets.size()), cols = static_cast<Eigen::Index>(active.size());
    Eigen::MatrixXcd a(rows, cols);
    Eigen::VectorXcd b(rows);
    for (Eigen::Index i = 0; i < rows; ++i) {
        const BeamTarget& t = targets[static_cast<size_t>(i)];
        const Vec3 d = t.direction.normalized();
        const double w = std::sqrt(t.weight);
        for (Eigen::Index j = 0; j < cols; ++j) {
            const ArrayElement& e = array.elements[active[static_cast<size_t>(j)]];
            a(i, j) = w * elementPatternAmplitude(array.pattern, e.axis, d) * std::polar(1.0, k * d.dot(e.position));
        }
        b(i) = w * t.desired;
    }
    Eigen::BDCSVD<Eigen::MatrixXcd> svd(a, Eigen::ComputeThinU | Eigen::ComputeThinV);
    const Eigen::VectorXd sigma = svd.singularValues();
    const double lambda = regularization * (sigma.size() ? sigma(0) : 0.0);
    const auto solve = [&](const Eigen::VectorXcd& rhs) {
        Eigen::VectorXcd x = Eigen::VectorXcd::Zero(cols);
        for (Eigen::Index r = 0; r < sigma.size(); ++r)
            if (sigma(r) > 0.0)
                x += (sigma(r) / (sigma(r) * sigma(r) + lambda * lambda) * svd.matrixU().col(r).dot(rhs)) * svd.matrixV().col(r);
        return x;
    };
    Eigen::VectorXcd x = solve(b);
    BeamSynthesis out;
    if (amplitudeLimit && x.cwiseAbs().maxCoeff() > *amplitudeLimit) {
        out.amplitudeLimited = true;
        // Alternate: clip the largest weights, solve for the rest with the clipped ones fixed.
        std::vector<bool> fixed(static_cast<size_t>(cols), false);
        for (int iteration = 0; iteration < 50; ++iteration) {
            bool changed = false;
            for (Eigen::Index j = 0; j < cols; ++j)
                if (!fixed[static_cast<size_t>(j)] && std::abs(x(j)) > *amplitudeLimit) {
                    x(j) = std::polar(*amplitudeLimit, std::arg(x(j)));
                    fixed[static_cast<size_t>(j)] = true;
                    changed = true;
                }
            if (!changed)
                break;
            std::vector<Eigen::Index> freeColumns;
            Eigen::VectorXcd rhs = b;
            for (Eigen::Index j = 0; j < cols; ++j) {
                if (fixed[static_cast<size_t>(j)])
                    rhs -= a.col(j) * x(j);
                else
                    freeColumns.push_back(j);
            }
            if (freeColumns.empty())
                break;
            Eigen::MatrixXcd af(rows, static_cast<Eigen::Index>(freeColumns.size()));
            for (size_t c = 0; c < freeColumns.size(); ++c)
                af.col(static_cast<Eigen::Index>(c)) = a.col(freeColumns[c]);
            Eigen::BDCSVD<Eigen::MatrixXcd> s2(af, Eigen::ComputeThinU | Eigen::ComputeThinV);
            Eigen::VectorXcd y = Eigen::VectorXcd::Zero(af.cols());
            const Eigen::VectorXd sg = s2.singularValues();
            for (Eigen::Index r = 0; r < sg.size(); ++r)
                if (sg(r) > 0.0)
                    y += (sg(r) / (sg(r) * sg(r) + lambda * lambda) * s2.matrixU().col(r).dot(rhs)) * s2.matrixV().col(r);
            for (size_t c = 0; c < freeColumns.size(); ++c)
                x(freeColumns[c]) = y(static_cast<Eigen::Index>(c));
        }
        for (Eigen::Index j = 0; j < cols; ++j)
            if (std::abs(x(j)) > *amplitudeLimit)
                x(j) = std::polar(*amplitudeLimit, std::arg(x(j)));
    }
    out.weights.assign(array.elements.size(), Complex(0.0, 0.0));
    for (Eigen::Index j = 0; j < cols; ++j)
        out.weights[active[static_cast<size_t>(j)]] = x(j);
    const Eigen::VectorXcd achieved = a * x;
    for (Eigen::Index i = 0; i < rows; ++i)
        out.achieved.push_back(achieved(i) / std::sqrt(targets[static_cast<size_t>(i)].weight));
    out.relativeResidual = b.norm() > 0.0 ? (achieved - b).norm() / b.norm() : 0.0;
    out.largestWeight = x.cwiseAbs().maxCoeff();
    out.notes = "Weighted least squares with Tikhonov regularization (" + std::to_string(regularization) + " x the largest singular value)";
    if (out.amplitudeLimited)
        out.notes += "; weights limited to |w| <= " + std::to_string(*amplitudeLimit) + " by iterative clipping (the residual shows the cost)";
    out.notes += ". Array-factor model: no mutual coupling.";
    return out;
}

}  // namespace resonant::em
