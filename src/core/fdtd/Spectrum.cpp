#include "fdtd/Spectrum.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace resonant::fdtd {
namespace {
constexpr double pi = 3.14159265358979323846;
}

std::string Spectrum::describe() const
{
    char buffer[400];
    std::snprintf(buffer, sizeof buffer,
                  "%zu samples at %.6g Hz over %.6g s; %s window; frequency resolution %.6g Hz (points every %.6g Hz); "
                  "content above %.6g Hz would alias",
                  sampleCount, sampleRateHz, observationSeconds, window == Window::Hann ? "Hann" : "rectangular", resolutionHz,
                  binSpacingHz, nyquistHz);
    return buffer;
}

void fft(std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    if (n < 2)
        return;
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(a[i], a[j]);
    }
    for (size_t length = 2; length <= n; length <<= 1) {
        const double angle = -2.0 * pi / static_cast<double>(length);
        const std::complex<double> step(std::cos(angle), std::sin(angle));
        for (size_t start = 0; start < n; start += length) {
            std::complex<double> w(1.0, 0.0);
            for (size_t k = 0; k < length / 2; ++k) {
                const std::complex<double> u = a[start + k];
                const std::complex<double> v = a[start + k + length / 2] * w;
                a[start + k] = u + v;
                a[start + k + length / 2] = u - v;
                w *= step;
            }
        }
    }
}

Spectrum computeSpectrum(const std::vector<double>& samples, double dt, Window window, int zeroPadFactor)
{
    Spectrum spectrum;
    spectrum.window = window;
    spectrum.sampleCount = samples.size();
    if (samples.size() < 2 || !(dt > 0.0))
        return spectrum;
    const size_t n = samples.size();
    size_t padded = 1;
    while (padded < n * static_cast<size_t>(std::max(1, zeroPadFactor)))
        padded <<= 1;

    std::vector<std::complex<double>> data(padded, {0.0, 0.0});
    double gain = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double w = window == Window::Hann ? 0.5 * (1.0 - std::cos(2.0 * pi * static_cast<double>(i) / static_cast<double>(n))) : 1.0;
        data[i] = samples[i] * w;
        gain += w;
    }
    fft(data);

    spectrum.paddedCount = padded;
    spectrum.sampleRateHz = 1.0 / dt;
    spectrum.nyquistHz = 0.5 / dt;
    spectrum.observationSeconds = static_cast<double>(n) * dt;
    spectrum.resolutionHz = (window == Window::Hann ? 2.0 : 1.0) / spectrum.observationSeconds;
    spectrum.binSpacingHz = 1.0 / (static_cast<double>(padded) * dt);
    const size_t bins = padded / 2 + 1;
    spectrum.frequencyHz.resize(bins);
    spectrum.amplitude.resize(bins);
    for (size_t k = 0; k < bins; ++k) {
        spectrum.frequencyHz[k] = static_cast<double>(k) * spectrum.binSpacingHz;
        spectrum.amplitude[k] = data[k] * (2.0 / gain);
    }
    spectrum.amplitude[0] *= 0.5;  // the mean has no mirror image
    return spectrum;
}

std::vector<SpectralPeak> findPeaks(const Spectrum& spectrum, double minRelative, double minHz, double maxHz)
{
    std::vector<SpectralPeak> peaks;
    const size_t n = spectrum.amplitude.size();
    if (n < 3)
        return peaks;
    std::vector<double> magnitude(n);
    for (size_t k = 0; k < n; ++k)
        magnitude[k] = std::abs(spectrum.amplitude[k]);
    for (size_t k = 1; k + 1 < n; ++k) {
        const double f = spectrum.frequencyHz[k];
        if (f < minHz || f > maxHz)
            continue;
        if (!(magnitude[k] > magnitude[k - 1] && magnitude[k] >= magnitude[k + 1]))
            continue;
        const double denominator = magnitude[k - 1] - 2.0 * magnitude[k] + magnitude[k + 1];
        const double offset = denominator != 0.0 ? 0.5 * (magnitude[k - 1] - magnitude[k + 1]) / denominator : 0.0;
        peaks.push_back({f + offset * spectrum.binSpacingHz, magnitude[k] - 0.25 * (magnitude[k - 1] - magnitude[k + 1]) * offset});
    }
    double largest = 0.0;
    for (const SpectralPeak& p : peaks)
        largest = std::max(largest, p.magnitude);
    std::erase_if(peaks, [&](const SpectralPeak& p) { return p.magnitude < minRelative * largest; });
    std::sort(peaks.begin(), peaks.end(), [](const SpectralPeak& a, const SpectralPeak& b) { return a.magnitude > b.magnitude; });
    return peaks;
}

}  // namespace resonant::fdtd
