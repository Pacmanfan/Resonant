#include "dsp/Resonance.h"

#include <Eigen/Dense>

#include <algorithm>
#include <cmath>
#include <complex>
#include <numeric>

namespace resonant::dsp {

namespace {

constexpr double kPi = 3.14159265358979323846;
using cd = std::complex<double>;

struct Pole {
    double f = 0.0, alpha = 0.0;
    cd amplitude;
};

// The core pencil on a (decimated) record; poles of positive frequency inside the band.
Result<std::pair<std::vector<Pole>, double>> pencil(const std::vector<double>& x, double dt, const PencilOptions& o)
{
    using R = Result<std::pair<std::vector<Pole>, double>>;
    const int n = static_cast<int>(x.size());
    const int L = std::max(2, n / 3);
    const int rows = n - L;
    if (rows < 4)
        return R::failure("too few samples for the pencil");
    Eigen::MatrixXd Y(rows, L + 1);
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c <= L; ++c)
            Y(r, c) = x[static_cast<size_t>(r + c)];
    Eigen::BDCSVD<Eigen::MatrixXd> svd(Y, Eigen::ComputeThinV);
    const Eigen::VectorXd& s = svd.singularValues();
    if (!(s(0) > 0.0))
        return R::failure("the signal has no energy");
    int M = 0;
    while (M < s.size() && s(M) > o.singularValueThreshold * s(0) && M < 2 * o.maxModes)
        ++M;
    M = std::max(M, 1);
    const Eigen::MatrixXd V = svd.matrixV().leftCols(M);
    const Eigen::MatrixXd V1 = V.topRows(L), V2 = V.bottomRows(L);
    const Eigen::MatrixXd A = V1.completeOrthogonalDecomposition().solve(V2);  // V1 A = V2
    Eigen::EigenSolver<Eigen::MatrixXd> eig(A.transpose());
    std::vector<cd> z;
    for (int i = 0; i < eig.eigenvalues().size(); ++i)
        z.push_back(eig.eigenvalues()(i));
    // Amplitudes by least squares on the Vandermonde system.
    Eigen::MatrixXcd Z(n, static_cast<int>(z.size()));
    for (int r = 0; r < n; ++r)
        for (size_t k = 0; k < z.size(); ++k)
            Z(r, static_cast<int>(k)) = std::pow(z[k], r);
    Eigen::VectorXcd b(n);
    for (int r = 0; r < n; ++r)
        b(r) = x[static_cast<size_t>(r)];
    const Eigen::VectorXcd h = Z.completeOrthogonalDecomposition().solve(b);
    const double residual = (Z * h - b).norm() / b.norm();
    std::vector<Pole> poles;
    for (size_t k = 0; k < z.size(); ++k) {
        const double f = std::arg(z[k]) / (2.0 * kPi * dt);
        if (f <= 0.0)
            continue;  // the conjugate of a positive-frequency pole (real signals), or a constant
        const double alpha = -std::log(std::abs(z[k])) / dt;
        if (f < o.minHz || f > o.maxHz)
            continue;
        poles.push_back({f, alpha, 2.0 * h(static_cast<int>(k))});
    }
    // Spurious poles carry little of the signal.
    double largest = 0.0;
    for (const Pole& p : poles)
        largest = std::max(largest, std::abs(p.amplitude));
    poles.erase(std::remove_if(poles.begin(), poles.end(), [&](const Pole& p) { return std::abs(p.amplitude) < 1e-4 * largest; }), poles.end());
    std::sort(poles.begin(), poles.end(), [](const Pole& p1, const Pole& p2) { return p1.f < p2.f; });
    return std::make_pair(poles, residual);
}

}  // namespace

Result<Identification> identifyByMatrixPencil(const std::vector<double>& samples, double dt, const PencilOptions& o)
{
    using R = Result<Identification>;
    if (!(dt > 0.0) || samples.size() <= o.skipSamples + 16)
        return R::failure("the record is too short for an identification (at least 16 samples after the skipped ones)");
    std::vector<double> x(samples.begin() + static_cast<long>(o.skipSamples), samples.end());
    // Decimate (boxcar average) to at most maxSamples, keeping 4 samples per period at maxHz.
    size_t factor = 1;
    if (x.size() > o.maxSamples) {
        factor = (x.size() + o.maxSamples - 1) / o.maxSamples;
        if (std::isfinite(o.maxHz))
            factor = std::min(factor, std::max<size_t>(1, static_cast<size_t>(std::floor(1.0 / (4.0 * o.maxHz * dt)))));
        if (factor > 1) {
            std::vector<double> d;
            for (size_t i = 0; i + factor <= x.size(); i += factor)
                d.push_back(std::accumulate(x.begin() + static_cast<long>(i), x.begin() + static_cast<long>(i + factor), 0.0) / static_cast<double>(factor));
            x.swap(d);
        }
    }
    const double step = dt * static_cast<double>(factor);
    Identification out;
    out.durationSeconds = step * static_cast<double>(x.size());
    out.sampleRateHz = 1.0 / step;
    if (std::isfinite(o.maxHz) && o.maxHz * step > 1.0 / 2.5)
        return R::failure("insufficient sampling: " + std::to_string(1.0 / (o.maxHz * step)).substr(0, 5) +
                          " samples per period at the highest frequency of interest (at least 2.5)");
    if (o.minHz > 0.0 && out.durationSeconds * o.minHz < 3.0)
        return R::failure("insufficient duration: the record covers " + std::to_string(out.durationSeconds * o.minHz).substr(0, 5) +
                          " periods of the lowest frequency of interest (at least 3)");
    if (x.size() > 1600) {
        out.warnings.push_back("the record was cut to its first 1600 samples (" + std::to_string(1600 * step).substr(0, 8) + " s) after decimation");
        x.resize(1600);
        out.durationSeconds = step * 1600.0;
    }
    auto full = pencil(x, step, o);
    if (!full)
        return R::failure(full.message());
    out.relativeResidual = full.value().second;
    // Uncertainty: the spread over sub-windows (the record without its first and without its last tenth).
    const size_t tenth = std::max<size_t>(1, x.size() / 10);
    std::vector<std::vector<Pole>> others;
    for (int w = 0; w < 2; ++w) {
        std::vector<double> sub = w == 0 ? std::vector<double>(x.begin() + static_cast<long>(tenth), x.end())
                                         : std::vector<double>(x.begin(), x.end() - static_cast<long>(tenth));
        if (auto p = pencil(sub, step, o))
            others.push_back(p.value().first);
    }
    // The boxcar decimation scaled and delayed each mode by its average over the window: undo it.
    const auto boxcarGain = [&](const Pole& p) {
        cd g = 0.0;
        const cd s(-p.alpha, 2.0 * kPi * p.f);
        for (size_t k = 0; k < factor; ++k)
            g += std::exp(s * (static_cast<double>(k) * dt));
        return g / static_cast<double>(factor);
    };
    for (Pole p : full.value().first) {
        if (factor > 1)
            p.amplitude /= boxcarGain(p);
        IdentifiedResonance r;
        r.method = "matrix-pencil";
        r.frequencyHz = p.f;
        r.decayRate = p.alpha;
        r.quality = p.alpha > 0.0 ? 2.0 * kPi * p.f / (2.0 * p.alpha) : std::numeric_limits<double>::infinity();
        r.bandwidthHz = std::max(0.0, p.alpha / kPi);
        r.amplitude = std::abs(p.amplitude);
        r.phase = std::arg(p.amplitude);
        r.fitQuality = 1.0 - out.relativeResidual * out.relativeResidual;
        for (const auto& list : others) {
            const Pole* best = nullptr;
            for (const Pole& q : list)
                if (!best || std::abs(q.f - p.f) < std::abs(best->f - p.f))
                    best = &q;
            if (!best)
                continue;
            r.frequencyUncertainty = std::max(r.frequencyUncertainty, std::abs(best->f - p.f));
            const double qq = best->alpha > 0.0 ? kPi * best->f / best->alpha : std::numeric_limits<double>::infinity();
            if (std::isfinite(qq) && std::isfinite(r.quality))
                r.qualityUncertainty = std::max(r.qualityUncertainty, std::abs(qq - r.quality));
        }
        if (p.alpha * out.durationSeconds < 0.05) {
            r.qualityUncertainty = std::max(r.qualityUncertainty, r.quality);
            out.warnings.push_back("the decay at " + std::to_string(p.f).substr(0, 8) + " Hz is below what the record resolves (alpha T = " +
                                   std::to_string(p.alpha * out.durationSeconds).substr(0, 6) + "): its Q is a lower bound at best");
        }
        out.resonances.push_back(r);
    }
    for (size_t i = 0; i + 1 < out.resonances.size(); ++i) {
        IdentifiedResonance& a = out.resonances[i];
        IdentifiedResonance& b = out.resonances[i + 1];
        if (b.frequencyHz - a.frequencyHz < 3.0 * std::max(a.bandwidthHz, b.bandwidthHz))
            a.overlapping = b.overlapping = true;
    }
    if (out.resonances.empty())
        out.warnings.push_back("no decaying oscillation in the band");
    if (out.relativeResidual > 0.1)
        out.warnings.push_back("the poles explain the record only to " + std::to_string(out.relativeResidual).substr(0, 5) +
                               " relative residual: noise, a non-modal response or more modes than kept");
    return out;
}

namespace {

double lorentz(double f, const Eigen::Vector4d& p)
{
    const double u = (f - p(1)) / p(2);
    return p(0) / (1.0 + u * u) + p(3);
}

}  // namespace

Result<LorentzianFit> fitLorentzian(const std::vector<double>& f, const std::vector<double>& y, double near)
{
    using R = Result<LorentzianFit>;
    if (f.size() != y.size() || f.size() < 7)
        return R::failure("a response needs at least 7 points with as many values");
    size_t peak = 0;
    if (near > 0.0) {
        size_t closest = 0;
        for (size_t i = 0; i < f.size(); ++i)
            if (std::abs(f[i] - near) < std::abs(f[closest] - near))
                closest = i;
        peak = closest;
        while (peak > 0 && y[peak - 1] > y[peak])
            --peak;
        while (peak + 1 < y.size() && y[peak + 1] > y[peak])
            ++peak;
    } else {
        peak = static_cast<size_t>(std::max_element(y.begin(), y.end()) - y.begin());
    }
    const double base = *std::min_element(y.begin(), y.end());
    const double half = base + 0.5 * (y[peak] - base);
    size_t lo = peak, hi = peak;
    while (lo > 0 && y[lo] > half)
        --lo;
    while (hi + 1 < y.size() && y[hi] > half)
        ++hi;
    LorentzianFit fit;
    fit.pointsInLinewidth = static_cast<int>(hi - lo) - 1;
    if (fit.pointsInLinewidth < 5)
        return R::failure("insufficient frequency resolution: " + std::to_string(std::max(0, fit.pointsInLinewidth)) +
                          " points within the linewidth (at least 5); sample the response more finely near " +
                          std::to_string(f[peak]).substr(0, 10) + " Hz");
    double gamma = 0.5 * (f[hi] - f[lo]);
    // Fit within +-4 half widths.
    std::vector<size_t> use;
    for (size_t i = 0; i < f.size(); ++i)
        if (std::abs(f[i] - f[peak]) <= 4.0 * gamma)
            use.push_back(i);
    Eigen::Vector4d p(y[peak] - base, f[peak], gamma, base);
    double lambda = 1e-3;
    const auto cost = [&](const Eigen::Vector4d& q) {
        double c = 0.0;
        for (size_t i : use)
            c += std::pow(y[i] - lorentz(f[i], q), 2);
        return c;
    };
    double current = cost(p);
    Eigen::Matrix4d JtJ;
    for (int iteration = 0; iteration < 200; ++iteration) {
        JtJ.setZero();
        Eigen::Vector4d Jtr = Eigen::Vector4d::Zero();
        for (size_t i : use) {
            const double u = (f[i] - p(1)) / p(2), d = 1.0 + u * u;
            Eigen::Vector4d J(1.0 / d, 2.0 * p(0) * u / (p(2) * d * d), 2.0 * p(0) * u * u / (p(2) * d * d), 1.0);
            JtJ += J * J.transpose();
            Jtr += J * (y[i] - lorentz(f[i], p));
        }
        Eigen::Matrix4d Aug = JtJ;
        Aug.diagonal() *= (1.0 + lambda);
        const Eigen::Vector4d step = Aug.ldlt().solve(Jtr);
        const Eigen::Vector4d trial = p + step;
        const double c = trial(2) > 0.0 ? cost(trial) : std::numeric_limits<double>::infinity();
        if (c < current) {
            p = trial;
            lambda *= 0.3;
            if (current - c < 1e-15 * (1.0 + current)) {
                current = c;
                break;
            }
            current = c;
        } else {
            lambda *= 10.0;
            if (lambda > 1e12)
                break;
        }
    }
    double energy = 0.0, signedLeft = 0.0, signedRight = 0.0, maxResidual = 0.0;
    for (size_t i : use) {
        const double r = y[i] - lorentz(f[i], p);
        energy += std::pow(y[i] - p(3), 2);
        (f[i] < p(1) ? signedLeft : signedRight) += r;
        maxResidual = std::max(maxResidual, r);
    }
    fit.centerHz = p(1);
    fit.halfWidthHz = std::abs(p(2));
    fit.amplitude = p(0);
    fit.baseline = p(3);
    fit.quality = fit.centerHz / (2.0 * fit.halfWidthHz);
    fit.relativeResidual = energy > 0.0 ? std::sqrt(current / energy) : 0.0;
    const double dof = std::max<double>(1.0, static_cast<double>(use.size()) - 4.0);
    const Eigen::Matrix4d covariance = JtJ.inverse() * (current / dof);
    fit.centerUncertainty = std::sqrt(std::max(0.0, covariance(1, 1)));
    const double sigmaGamma = std::sqrt(std::max(0.0, covariance(2, 2)));
    fit.qualityUncertainty = fit.quality * std::hypot(fit.centerUncertainty / fit.centerHz, sigmaGamma / fit.halfWidthHz);
    // Asymmetry: the residual leans to one side; overlap: a second maximum the fit does not explain.
    const double scale = std::abs(p(0)) * static_cast<double>(use.size());
    fit.asymmetric = fit.relativeResidual > 0.02 && std::abs(signedLeft - signedRight) > 0.02 * scale;
    fit.overlapping = maxResidual > 0.1 * std::abs(p(0));
    return fit;
}

std::vector<LorentzianFit> fitResonancePeaks(const std::vector<double>& f, const std::vector<double>& y, int count, std::vector<std::string>* warnings)
{
    std::vector<LorentzianFit> fits;
    if (f.size() != y.size() || f.size() < 7)
        return fits;
    std::vector<size_t> peaks;
    const double top = *std::max_element(y.begin(), y.end()), bottom = *std::min_element(y.begin(), y.end());
    for (size_t i = 1; i + 1 < y.size(); ++i)
        if (y[i] > y[i - 1] && y[i] >= y[i + 1] && y[i] - bottom > 0.05 * (top - bottom))
            peaks.push_back(i);
    std::sort(peaks.begin(), peaks.end(), [&](size_t a, size_t b) { return y[a] > y[b]; });
    for (size_t i : peaks) {
        if (static_cast<int>(fits.size()) >= count)
            break;
        auto fit = fitLorentzian(f, y, f[i]);
        if (!fit) {
            if (warnings)
                warnings->push_back(fit.message());
            continue;
        }
        fits.push_back(fit.value());
    }
    std::sort(fits.begin(), fits.end(), [](const LorentzianFit& a, const LorentzianFit& b) { return a.centerHz < b.centerHz; });
    return fits;
}

}  // namespace resonant::dsp
