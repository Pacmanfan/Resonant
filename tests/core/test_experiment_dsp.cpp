// Signal processing for measurements (REQ-CHAT-20261126-008, RES-EXP-012):
// windows, Welch spectra (Parseval), the H1 transfer function and the
// coherence of a known system, delay estimation, filters and resampling.
#include "dsp/CrossCorrelation.h"
#include "dsp/Filters.h"
#include "dsp/SpectralEstimation.h"
#include "dsp/Windows.h"

#include <gtest/gtest.h>

#include <cmath>
#include <complex>
#include <numbers>
#include <random>

using namespace resonant::dsp;

namespace {

constexpr double kPi = std::numbers::pi;

std::vector<double> whiteNoise(size_t n, double sigma, unsigned seed)
{
    std::mt19937 generator(seed);
    std::normal_distribution<double> d(0.0, sigma);
    std::vector<double> x(n);
    for (auto& v : x)
        v = d(generator);
    return x;
}

// A known second-order system (bilinear, prewarped resonance): the reference.
struct Resonator {
    double b0, b1, b2, a1, a2, z1 = 0, z2 = 0;
    Resonator(double f0, double zeta, double fs)
    {
        const double w = 2 * kPi * f0, K = w / std::tan(w / (2 * fs)), K2 = K * K;
        const double a0 = K2 + 2 * zeta * w * K + w * w;
        // H(s) = w^2 / (s^2 + 2 zeta w s + w^2)
        b0 = w * w / a0;
        b1 = 2 * w * w / a0;
        b2 = w * w / a0;
        a1 = (2 * w * w - 2 * K2) / a0;
        a2 = (K2 - 2 * zeta * w * K + w * w) / a0;
    }
    double operator()(double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    std::complex<double> response(double f, double fs) const
    {
        const std::complex<double> z = std::polar(1.0, -2 * kPi * f / fs);
        return (b0 + b1 * z + b2 * z * z) / (1.0 + a1 * z + a2 * z * z);
    }
};

}  // namespace

TEST(ExperimentDsp, WindowPropertiesMatchTheirDefinitions)
{
    const auto hann = windowProperties(windowCoefficients(WindowKind::Hann, 4096));
    EXPECT_NEAR(hann.coherentGain, 0.5, 1e-9);
    EXPECT_NEAR(hann.noiseBandwidthBins, 1.5, 1e-3);
    const auto rect = windowProperties(windowCoefficients(WindowKind::Rectangular, 1024));
    EXPECT_NEAR(rect.noiseBandwidthBins, 1.0, 1e-12);
    EXPECT_NEAR(windowProperties(windowCoefficients(WindowKind::BlackmanHarris, 4096)).noiseBandwidthBins, 2.0, 0.01);
    EXPECT_NEAR(windowProperties(windowCoefficients(WindowKind::FlatTop, 4096)).noiseBandwidthBins, 3.77, 0.02);
    EXPECT_FALSE(windowKindFromString("triangle").ok());
}

TEST(ExperimentDsp, WelchDensityIntegratesToTheMeanSquare)
{
    const double fs = 10000.0;
    const auto x = whiteNoise(1 << 18, 0.7, 1);
    WelchSettings s;
    s.segmentLength = 2048;
    auto psd = welchPsd(x, fs, s);
    ASSERT_TRUE(psd.ok()) << psd.message();
    EXPECT_GT(psd.value().grid.averages, 200u);
    EXPECT_NEAR(bandPower(psd.value(), 0.0, fs / 2), 0.49, 0.49 * 0.02);  // Parseval: sigma^2
    // A sinusoid: its power A^2/2 is concentrated around its frequency.
    std::vector<double> tone(1 << 16);
    for (size_t i = 0; i < tone.size(); ++i)
        tone[i] = 2.0 * std::cos(2 * kPi * 1234.5 * static_cast<double>(i) / fs);
    auto p2 = welchPsd(tone, fs, s);
    ASSERT_TRUE(p2.ok());
    EXPECT_NEAR(bandPower(p2.value(), 1200, 1270), 2.0, 0.02);
    // Refusals: not a power of two, a record shorter than a segment.
    s.segmentLength = 1000;
    EXPECT_FALSE(welchPsd(x, fs, s).ok());
    s.segmentLength = 1 << 20;
    EXPECT_FALSE(welchPsd(x, fs, s).ok());
}

TEST(ExperimentDsp, H1RecoversAKnownSystemAndItsCoherence)
{
    const double fs = 8192.0;
    Resonator system(300.0, 0.05, fs);
    const auto x = whiteNoise(1 << 18, 1.0, 2);
    const auto noise = whiteNoise(x.size(), 0.01, 3);
    std::vector<double> y(x.size());
    for (size_t i = 0; i < x.size(); ++i)
        y[i] = system(x[i]) + noise[i];
    WelchSettings s;
    s.segmentLength = 4096;
    auto h = estimateTransferFunction(x, y, fs, s);
    ASSERT_TRUE(h.ok()) << h.message();
    double worstDb = 0.0, worstPhase = 0.0;
    for (size_t k = 0; k < h.value().h1.size(); ++k) {
        const double f = h.value().grid.frequencyHz[k];
        // Above 1 kHz the response falls towards the output noise: the random error grows (see coherence).
        if (f < 20 || f > 1000)
            continue;
        const auto truth = system.response(f, fs);
        worstDb = std::max(worstDb, std::abs(20 * std::log10(std::abs(h.value().h1[k]) / std::abs(truth))));
        worstPhase = std::max(worstPhase, std::abs(std::arg(h.value().h1[k] / truth)));
        EXPECT_GT(h.value().coherence[k], 0.98) << f;
    }
    EXPECT_LT(worstDb, 0.3);
    EXPECT_LT(worstPhase, 0.03);
    // Uncorrelated output: coherence near 0 (it is biased by 1 / n_d).
    auto u = estimateTransferFunction(x, whiteNoise(x.size(), 1.0, 9), fs, s);
    ASSERT_TRUE(u.ok());
    double mean = 0;
    for (double c : u.value().coherence)
        mean += c / static_cast<double>(u.value().coherence.size());
    EXPECT_LT(mean, 0.05);
    // The impulse response of a pure delay of 5 samples peaks at 5.
    std::vector<double> delayed(x.size(), 0.0);
    for (size_t i = 5; i < x.size(); ++i)
        delayed[i] = x[i - 5];
    auto d = estimateTransferFunction(x, delayed, fs, s);
    const auto ir = impulseResponse(d.value());
    EXPECT_EQ(std::max_element(ir.samples.begin(), ir.samples.end()) - ir.samples.begin(), 5);
    EXPECT_NEAR(ir.samples[5], 1.0, 0.01);
}

TEST(ExperimentDsp, ToneAmplitudeReadsAmplitudeAndPhase)
{
    const double fs = 48000.0;
    std::vector<double> x(48000);
    for (size_t i = 0; i < x.size(); ++i)
        x[i] = 0.3 * std::cos(2 * kPi * 1000.0 * static_cast<double>(i) / fs + 0.7);
    const auto a = toneAmplitude(x, fs, 1000.0);
    EXPECT_NEAR(std::abs(a), 0.3, 1e-6);
    EXPECT_NEAR(std::arg(a), 0.7, 1e-6);
    EXPECT_NEAR(rms(x), 0.3 / std::sqrt(2.0), 1e-6);
}

TEST(ExperimentDsp, DelayEstimateResolvesFractionalDelays)
{
    const double fs = 48000.0;
    // Band-limited noise, delayed by 37.3 samples through band-limited resampling.
    const auto raw = whiteNoise(1 << 15, 1.0, 4);
    auto filter = designFir(FilterBand::LowPass, fs, 12000.0, 0.0, 129);
    ASSERT_TRUE(filter.ok());
    const auto x = applyFir(filter.value(), raw, true).samples;
    const double delay = 37.3;
    std::vector<double> y(x.size(), 0.0);
    for (size_t n = 0; n < y.size(); ++n) {
        const double t = static_cast<double>(n) - delay;
        double sum = 0;
        for (long k = static_cast<long>(t) - 40; k <= static_cast<long>(t) + 40; ++k) {
            if (k < 0 || k >= static_cast<long>(x.size()))
                continue;
            const double d = t - static_cast<double>(k);
            const double w = 0.5 * (1 + std::cos(kPi * d / 41.0));
            sum += x[static_cast<size_t>(k)] * (std::abs(d) < 1e-12 ? 1.0 : std::sin(kPi * d) / (kPi * d)) * w;
        }
        y[n] = sum;
    }
    auto e = estimateDelay(x, y, fs, 100);
    ASSERT_TRUE(e.ok()) << e.message();
    EXPECT_EQ(e.value().integerLag, 37);
    EXPECT_NEAR(e.value().delaySamples, delay, 0.02);
    EXPECT_NEAR(e.value().delaySeconds, delay / fs, 0.02 / fs);
    EXPECT_FALSE(estimateDelay(x, whiteNoise(x.size(), 1.0, 5), fs, 100).ok());  // nothing in common
}

TEST(ExperimentDsp, FiltersDecimationAndResampling)
{
    const double fs = 48000.0;
    std::vector<double> low(9600), high(9600);
    for (size_t i = 0; i < low.size(); ++i) {
        low[i] = std::sin(2 * kPi * 500.0 * static_cast<double>(i) / fs);
        high[i] = std::sin(2 * kPi * 12000.0 * static_cast<double>(i) / fs);
    }
    auto lp = designFir(FilterBand::LowPass, fs, 2000.0, 0.0, 201, WindowKind::BlackmanHarris);
    ASSERT_TRUE(lp.ok());
    const auto passed = applyFir(lp.value(), low, true);
    const auto stopped = applyFir(lp.value(), high, true);
    // With the delay compensated the pass-band tone keeps its phase (compare in the middle).
    double err = 0;
    for (size_t i = 1000; i < 8000; ++i)
        err = std::max(err, std::abs(passed.samples[i] - low[i]));
    EXPECT_LT(err, 2e-3);
    EXPECT_LT(rms(std::vector<double>(stopped.samples.begin() + 1000, stopped.samples.begin() + 8000)), 1e-4);
    EXPECT_EQ(passed.edgeSamples, 100u);
    EXPECT_FALSE(designFir(FilterBand::LowPass, fs, 30000.0, 0.0, 101).ok());  // above Nyquist
    EXPECT_FALSE(designFir(FilterBand::BandPass, fs, 2000.0, 1000.0, 101).ok());
    EXPECT_FALSE(designFir(FilterBand::LowPass, fs, 1000.0, 0.0, 100).ok());   // even
    auto dec = decimate(low, fs, 4);
    ASSERT_TRUE(dec.ok());
    EXPECT_DOUBLE_EQ(dec.value().sampleRateHz, 12000.0);
    EXPECT_NEAR(std::abs(toneAmplitude(std::vector<double>(dec.value().samples.begin() + 200, dec.value().samples.begin() + 2200),
                                       12000.0, 500.0)),
                1.0, 5e-3);  // the anti-alias filter's pass-band ripple
    auto rs = resample(low, fs, 44100.0);
    ASSERT_TRUE(rs.ok());
    const std::vector<double> mid(rs.value().samples.begin() + 500, rs.value().samples.begin() + 8000);
    EXPECT_NEAR(std::abs(toneAmplitude(mid, 44100.0, 500.0)), 1.0, 3e-3);
}
