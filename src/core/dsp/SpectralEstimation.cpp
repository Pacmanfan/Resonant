#include "dsp/SpectralEstimation.h"

#include "dsp/Spectrum.h"
#include "foundation/Units.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <sstream>

namespace resonant::dsp {

namespace {

bool isPowerOfTwo(size_t n) { return n >= 2 && (n & (n - 1)) == 0; }

struct Segments {
    size_t length = 0, hop = 0, count = 0;
};

Segments segmentsOf(size_t recordLength, const WelchSettings& s)
{
    Segments g;
    g.length = s.segmentLength;
    g.hop = std::max<size_t>(1, static_cast<size_t>(std::lround(static_cast<double>(s.segmentLength) * (1.0 - s.overlap))));
    g.count = recordLength < g.length ? 0 : 1 + (recordLength - g.length) / g.hop;
    return g;
}

// The DFT of segment k of x, windowed.
std::vector<std::complex<double>> segmentSpectrum(const std::vector<double>& x, size_t start, const std::vector<double>& w,
                                                  bool removeMean)
{
    const size_t n = w.size();
    double mean = 0.0;
    if (removeMean) {
        for (size_t i = 0; i < n; ++i)
            mean += x[start + i];
        mean /= static_cast<double>(n);
    }
    std::vector<std::complex<double>> data(n);
    for (size_t i = 0; i < n; ++i)
        data[i] = (x[start + i] - mean) * w[i];
    fft(data);
    return data;
}

SpectralGrid gridOf(const WelchSettings& s, double fs, size_t averages, const std::vector<double>& w)
{
    SpectralGrid grid;
    grid.settings = s;
    grid.sampleRateHz = fs;
    grid.averages = averages;
    grid.binSpacingHz = fs / static_cast<double>(s.segmentLength);
    grid.resolutionHz = windowProperties(w).noiseBandwidthBins * grid.binSpacingHz;
    const size_t bins = s.segmentLength / 2 + 1;
    grid.frequencyHz.resize(bins);
    for (size_t k = 0; k < bins; ++k)
        grid.frequencyHz[k] = static_cast<double>(k) * grid.binSpacingHz;
    return grid;
}

// One-sided factor: 2 except at DC and Nyquist.
double sideFactor(size_t k, size_t n) { return (k == 0 || k == n / 2) ? 1.0 : 2.0; }

}  // namespace

Status WelchSettings::validate(size_t recordLength) const
{
    if (!isPowerOfTwo(segmentLength))
        return Status::failure("the segment length must be a power of two (it is " + std::to_string(segmentLength) + ")");
    if (!(overlap >= 0.0 && overlap <= 0.95))
        return Status::failure("the overlap must be between 0 and 0.95");
    if (recordLength < segmentLength)
        return Status::failure("the record (" + std::to_string(recordLength) + " samples) is shorter than one segment (" +
                               std::to_string(segmentLength) + ")");
    return Status::success();
}

std::string WelchSettings::describe() const
{
    std::ostringstream out;
    out << "Welch: " << segmentLength << "-sample segments, " << std::lround(overlap * 100.0) << " % overlap, "
        << toString(window) << " window" << (removeMean ? ", segment mean removed" : "");
    return out.str();
}

Result<PowerSpectralDensity> welchPsd(const std::vector<double>& x, double fs, const WelchSettings& settings)
{
    if (!(fs > 0.0))
        return Result<PowerSpectralDensity>::failure("the sample rate must be positive");
    if (Status ok = settings.validate(x.size()); !ok)
        return Result<PowerSpectralDensity>::failure(ok.message());
    const std::vector<double> w = windowCoefficients(settings.window, settings.segmentLength);
    const Segments g = segmentsOf(x.size(), settings);
    const double scale = 1.0 / (fs * std::inner_product(w.begin(), w.end(), w.begin(), 0.0));
    PowerSpectralDensity psd;
    psd.grid = gridOf(settings, fs, g.count, w);
    psd.density.assign(psd.grid.frequencyHz.size(), 0.0);
    for (size_t s = 0; s < g.count; ++s) {
        const auto X = segmentSpectrum(x, s * g.hop, w, settings.removeMean);
        for (size_t k = 0; k < psd.density.size(); ++k)
            psd.density[k] += sideFactor(k, g.length) * std::norm(X[k]) * scale;
    }
    for (double& v : psd.density)
        v /= static_cast<double>(g.count);
    return psd;
}

Result<CrossSpectra> crossSpectra(const std::vector<double>& x, const std::vector<double>& y, double fs, const WelchSettings& settings)
{
    if (x.size() != y.size())
        return Result<CrossSpectra>::failure("the two records have different lengths (" + std::to_string(x.size()) + " and " +
                                             std::to_string(y.size()) + " samples)");
    if (!(fs > 0.0))
        return Result<CrossSpectra>::failure("the sample rate must be positive");
    if (Status ok = settings.validate(x.size()); !ok)
        return Result<CrossSpectra>::failure(ok.message());
    const std::vector<double> w = windowCoefficients(settings.window, settings.segmentLength);
    const Segments g = segmentsOf(x.size(), settings);
    const double scale = 1.0 / (fs * std::inner_product(w.begin(), w.end(), w.begin(), 0.0));
    CrossSpectra c;
    c.grid = gridOf(settings, fs, g.count, w);
    const size_t bins = c.grid.frequencyHz.size();
    c.gxx.assign(bins, 0.0);
    c.gyy.assign(bins, 0.0);
    c.gxy.assign(bins, {0.0, 0.0});
    for (size_t s = 0; s < g.count; ++s) {
        const auto X = segmentSpectrum(x, s * g.hop, w, settings.removeMean);
        const auto Y = segmentSpectrum(y, s * g.hop, w, settings.removeMean);
        for (size_t k = 0; k < bins; ++k) {
            const double f = sideFactor(k, g.length) * scale;
            c.gxx[k] += f * std::norm(X[k]);
            c.gyy[k] += f * std::norm(Y[k]);
            c.gxy[k] += f * std::conj(X[k]) * Y[k];
        }
    }
    const double n = static_cast<double>(g.count);
    for (size_t k = 0; k < bins; ++k) {
        c.gxx[k] /= n;
        c.gyy[k] /= n;
        c.gxy[k] /= n;
    }
    return c;
}

Result<TransferFunctionEstimate> estimateTransferFunction(const std::vector<double>& x, const std::vector<double>& y, double fs,
                                                          const WelchSettings& settings)
{
    auto spectra = crossSpectra(x, y, fs, settings);
    if (!spectra)
        return Result<TransferFunctionEstimate>::failure(spectra.message());
    const CrossSpectra& c = spectra.value();
    TransferFunctionEstimate t;
    t.grid = c.grid;
    const size_t bins = c.gxx.size();
    t.h1.resize(bins);
    t.coherence.resize(bins);
    t.magnitudeRandomError.resize(bins);
    const double nd = static_cast<double>(c.grid.averages);
    // A bin whose input power is below this fraction of the strongest input
    // bin carries no information about the system; H1 there is reported as 0
    // with coherence 0 (not divided by numerical noise).
    const double floor = 1e-14 * *std::max_element(c.gxx.begin(), c.gxx.end());
    for (size_t k = 0; k < bins; ++k) {
        if (!(c.gxx[k] > floor) || !(c.gyy[k] > 0.0)) {
            t.h1[k] = {0.0, 0.0};
            t.coherence[k] = 0.0;
            t.magnitudeRandomError[k] = std::numeric_limits<double>::infinity();
            continue;
        }
        t.h1[k] = c.gxy[k] / c.gxx[k];
        const double gamma2 = std::clamp(std::norm(c.gxy[k]) / (c.gxx[k] * c.gyy[k]), 0.0, 1.0);
        t.coherence[k] = gamma2;
        // With a single average the coherence is identically 1 and says nothing.
        t.magnitudeRandomError[k] = (gamma2 > 0.0 && nd > 1.0) ? std::sqrt(1.0 - gamma2) / (std::sqrt(gamma2) * std::sqrt(2.0 * nd))
                                                               : std::numeric_limits<double>::infinity();
    }
    return t;
}

ImpulseResponse impulseResponse(const TransferFunctionEstimate& estimate)
{
    ImpulseResponse r;
    r.sampleRateHz = estimate.grid.sampleRateHz;
    const size_t n = estimate.grid.settings.segmentLength;
    if (estimate.h1.size() != n / 2 + 1)
        return r;
    // Hermitian spectrum of the real response; inverse DFT through the forward
    // FFT of the conjugate.
    std::vector<std::complex<double>> spectrum(n);
    for (size_t k = 0; k <= n / 2; ++k)
        spectrum[k] = estimate.h1[k];
    for (size_t k = n / 2 + 1; k < n; ++k)
        spectrum[k] = std::conj(estimate.h1[n - k]);
    for (auto& v : spectrum)
        v = std::conj(v);
    fft(spectrum);
    r.samples.resize(n);
    for (size_t i = 0; i < n; ++i)
        r.samples[i] = spectrum[i].real() / static_cast<double>(n);
    return r;
}

std::complex<double> toneAmplitude(const std::vector<double>& x, double fs, double frequencyHz, WindowKind window)
{
    if (x.empty() || !(fs > 0.0))
        return {0.0, 0.0};
    const std::vector<double> w = windowCoefficients(window, x.size());
    const double gain = windowProperties(w).coherentGain;
    std::complex<double> sum(0.0, 0.0);
    const double step = 2.0 * units::pi * frequencyHz / fs;
    for (size_t i = 0; i < x.size(); ++i) {
        const double angle = step * static_cast<double>(i);
        sum += x[i] * w[i] * std::complex<double>(std::cos(angle), -std::sin(angle));
    }
    return 2.0 * sum / (static_cast<double>(x.size()) * gain);
}

double rms(const std::vector<double>& x)
{
    if (x.empty())
        return 0.0;
    double sum = 0.0;
    for (double v : x)
        sum += v * v;
    return std::sqrt(sum / static_cast<double>(x.size()));
}

double bandPower(const PowerSpectralDensity& psd, double f1, double f2)
{
    double total = 0.0;
    const auto& f = psd.grid.frequencyHz;
    for (size_t k = 0; k + 1 < f.size(); ++k) {
        const double a = std::max(f[k], f1), b = std::min(f[k + 1], f2);
        if (b <= a)
            continue;
        // Linear interpolation of the density over [f_k, f_k+1], integrated over [a, b].
        const double span = f[k + 1] - f[k];
        auto at = [&](double v) { return psd.density[k] + (psd.density[k + 1] - psd.density[k]) * (v - f[k]) / span; };
        total += 0.5 * (at(a) + at(b)) * (b - a);
    }
    return total;
}

}  // namespace resonant::dsp
