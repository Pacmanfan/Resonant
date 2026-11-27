#include "dsp/CrossCorrelation.h"

#include "dsp/Spectrum.h"
#include "dsp/Windows.h"
#include "foundation/Units.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <string>

namespace resonant::dsp {

namespace {

size_t nextPowerOfTwo(size_t n)
{
    size_t p = 2;
    while (p < n)
        p <<= 1;
    return p;
}

std::vector<std::complex<double>> padded(const std::vector<double>& x, size_t n, size_t start = 0, size_t count = SIZE_MAX,
                                         const std::vector<double>* window = nullptr)
{
    std::vector<std::complex<double>> data(n, {0.0, 0.0});
    const size_t m = std::min(count, x.size() - std::min(start, x.size()));
    for (size_t i = 0; i < m && i < n; ++i)
        data[i] = x[start + i] * (window ? (*window)[i] : 1.0);
    return data;
}

// Inverse DFT through the forward FFT of the conjugate.
void inverseFft(std::vector<std::complex<double>>& data)
{
    for (auto& v : data)
        v = std::conj(v);
    fft(data);
    const double n = static_cast<double>(data.size());
    for (auto& v : data)
        v = std::conj(v) / n;
}

}  // namespace

Result<Correlation> crossCorrelation(const std::vector<double>& x, const std::vector<double>& y, long maxLag)
{
    if (x.empty() || y.empty())
        return Result<Correlation>::failure("cannot correlate an empty record");
    if (maxLag < 0)
        return Result<Correlation>::failure("the largest lag must not be negative");
    const size_t n = nextPowerOfTwo(x.size() + y.size());
    auto X = padded(x, n);
    auto Y = padded(y, n);
    fft(X);
    fft(Y);
    for (size_t k = 0; k < n; ++k)
        X[k] = std::conj(X[k]) * Y[k];
    inverseFft(X);
    Correlation c;
    c.maxLag = maxLag;
    c.values.resize(static_cast<size_t>(2 * maxLag + 1));
    for (long lag = -maxLag; lag <= maxLag; ++lag) {
        const size_t index = lag >= 0 ? static_cast<size_t>(lag) : n - static_cast<size_t>(-lag);
        c.values[static_cast<size_t>(lag + maxLag)] = std::labs(lag) < static_cast<long>(n) ? X[index].real() : 0.0;
    }
    double ex = 0.0, ey = 0.0;
    for (double v : x)
        ex += v * v;
    for (double v : y)
        ey += v * v;
    c.normalization = std::sqrt(ex * ey);
    return c;
}

Result<Correlation> autoCorrelation(const std::vector<double>& x, long maxLag) { return crossCorrelation(x, x, maxLag); }

Result<DelayEstimate> estimateDelay(const std::vector<double>& reference, const std::vector<double>& delayed, double fs,
                                    long maxLagSamples)
{
    if (!(fs > 0.0))
        return Result<DelayEstimate>::failure("the sample rate must be positive");
    if (reference.size() < 64 || delayed.size() < 64)
        return Result<DelayEstimate>::failure("the records are too short for a delay estimate (at least 64 samples)");
    auto correlation = crossCorrelation(reference, delayed, maxLagSamples);
    if (!correlation)
        return Result<DelayEstimate>::failure(correlation.message());
    const Correlation& c = correlation.value();
    long best = -maxLagSamples;
    for (long lag = -maxLagSamples; lag <= maxLagSamples; ++lag)
        if (c.at(lag) > c.at(best))
            best = lag;
    DelayEstimate d;
    d.integerLag = best;
    d.peakCorrelation = c.normalization > 0.0 ? c.at(best) / c.normalization : 0.0;
    if (d.peakCorrelation < 0.1)
        return Result<DelayEstimate>::failure("the records share no common signal (normalized correlation peak " +
                                              std::to_string(d.peakCorrelation) + " < 0.1)");

    // Fractional part: phase slope of the cross spectrum of the aligned overlap.
    const size_t startX = best >= 0 ? 0 : static_cast<size_t>(-best);
    const size_t startY = best >= 0 ? static_cast<size_t>(best) : 0;
    const size_t overlap = std::min(reference.size() - startX, delayed.size() - startY);
    const size_t n = nextPowerOfTwo(overlap);
    const std::vector<double> w = windowCoefficients(WindowKind::Hann, overlap);
    auto X = padded(reference, n, startX, overlap, &w);
    auto Y = padded(delayed, n, startY, overlap, &w);
    fft(X);
    fft(Y);
    std::vector<std::complex<double>> g(n / 2);
    double largest = 0.0;
    for (size_t k = 1; k < n / 2; ++k) {
        g[k] = std::conj(X[k]) * Y[k];
        largest = std::max(largest, std::abs(g[k]));
    }
    double swo2 = 0.0, swop = 0.0, sw = 0.0;
    std::vector<double> omega, phase, weight;
    for (size_t k = 1; k < n / 2; ++k) {
        const double m = std::abs(g[k]);
        if (m < 0.01 * largest)
            continue;
        const double om = 2.0 * units::pi * static_cast<double>(k) / static_cast<double>(n);
        const double ph = std::arg(g[k]);
        omega.push_back(om);
        phase.push_back(ph);
        weight.push_back(m);
        swo2 += m * om * om;
        swop += m * om * ph;
        sw += m;
    }
    double fraction = 0.0, se = 0.0;
    if (swo2 > 0.0) {
        fraction = -swop / swo2;  // Y = X exp(-j omega d)
        double residual = 0.0;
        for (size_t i = 0; i < omega.size(); ++i) {
            const double r = phase[i] + omega[i] * fraction;
            residual += weight[i] * r * r;
        }
        const double count = static_cast<double>(omega.size());
        if (count > 2.0)
            se = std::sqrt(residual / sw * count / (count - 1.0) / (swo2 / sw) / count);
    }
    d.delaySamples = static_cast<double>(best) + fraction;
    d.delaySeconds = d.delaySamples / fs;
    d.standardUncertaintySeconds = se / fs;
    d.method = "integer lag at the cross-correlation peak; fraction from the |G_xy|-weighted slope of the cross-spectrum phase "
               "of the aligned, Hann-windowed overlap (" +
               std::to_string(omega.size()) + " bins)";
    return d;
}

}  // namespace resonant::dsp
