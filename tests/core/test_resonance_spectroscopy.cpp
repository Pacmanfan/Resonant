// Resonance spectroscopy (RES-RSN-003): complex poles by the matrix pencil and
// Lorentzian fits, against signals and spectra of known resonances, and the
// refusals for records too short or too coarse.
#include "dsp/Resonance.h"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <random>

using namespace resonant;
using namespace resonant::dsp;

namespace {
constexpr double kPi = 3.14159265358979323846;

std::vector<double> ringDown(const std::vector<std::array<double, 4>>& modes, double dt, size_t n, double noise = 0.0)
{
    std::mt19937 rng(7);
    std::normal_distribution<double> g(0.0, 1.0);
    std::vector<double> x(n, 0.0);
    for (size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) * dt;
        for (const auto& [f, q, a, phase] : modes) {
            const double alpha = kPi * f / q;
            x[i] += a * std::exp(-alpha * t) * std::cos(2 * kPi * f * t + phase);
        }
        x[i] += noise * g(rng);
    }
    return x;
}

double lorentzian(double f, double f0, double q)
{
    const double g = f0 / (2 * q), u = (f - f0) / g;
    return 1.0 / (1.0 + u * u);
}
}  // namespace

TEST(ResonanceSpectroscopy, MatrixPencilRecoversOverlappingDampedModes)
{
    // Two modes 5 % apart whose bandwidths overlap in a spectrum.
    const double dt = 1e-5;
    const auto x = ringDown({{1000.0, 50.0, 1.0, 0.3}, {1050.0, 80.0, 0.6, -1.1}}, dt, 6000);
    PencilOptions o;
    o.minHz = 500;
    o.maxHz = 3000;
    auto id = identifyByMatrixPencil(x, dt, o);
    ASSERT_TRUE(id) << id.message();
    ASSERT_EQ(id.value().resonances.size(), 2u);
    const auto& a = id.value().resonances[0];
    const auto& b = id.value().resonances[1];
    EXPECT_NEAR(a.frequencyHz, 1000.0, 1e-6);
    EXPECT_NEAR(a.quality, 50.0, 1e-5);
    EXPECT_NEAR(a.amplitude, 1.0, 1e-6);
    EXPECT_NEAR(b.frequencyHz, 1050.0, 1e-6);
    EXPECT_NEAR(b.quality, 80.0, 1e-5);
    EXPECT_NEAR(b.bandwidthHz, 1050.0 / 80.0, 1e-6);
    EXPECT_TRUE(a.overlapping && b.overlapping);
    EXPECT_LT(id.value().relativeResidual, 1e-8);
}

TEST(ResonanceSpectroscopy, MatrixPencilWithNoiseReportsItsUncertainty)
{
    const double dt = 1e-5;
    const auto x = ringDown({{2000.0, 100.0, 1.0, 0.0}}, dt, 8000, 0.01);
    PencilOptions o;
    o.minHz = 500;
    o.maxHz = 5000;
    o.singularValueThreshold = 0.05;  // above the noise floor
    auto id = identifyByMatrixPencil(x, dt, o);
    ASSERT_TRUE(id) << id.message();
    ASSERT_GE(id.value().resonances.size(), 1u);
    const auto& r = id.value().resonances[0];
    EXPECT_NEAR(r.frequencyHz, 2000.0, 0.5);
    EXPECT_NEAR(r.quality, 100.0, 5.0);
    EXPECT_GT(r.qualityUncertainty, 0.0);
    EXPECT_LT(std::abs(r.quality - 100.0), 5.0 * r.qualityUncertainty + 1.0);
}

TEST(ResonanceSpectroscopy, RecordsTooShortOrTooCoarseAreRefused)
{
    const double dt = 1e-4;
    const auto x = ringDown({{100.0, 20.0, 1.0, 0.0}}, dt, 200);  // 20 ms: 2 periods of 100 Hz
    PencilOptions o;
    o.minHz = 100.0;
    auto shortRecord = identifyByMatrixPencil(x, dt, o);
    ASSERT_FALSE(shortRecord);
    EXPECT_NE(shortRecord.message().find("insufficient duration"), std::string::npos);
    o.minHz = 0.0;
    o.maxHz = 5000.0;  // above Nyquist / 1.25 of a 10 kHz record
    auto coarse = identifyByMatrixPencil(x, dt, o);
    ASSERT_FALSE(coarse);
    EXPECT_NE(coarse.message().find("insufficient sampling"), std::string::npos);
    EXPECT_FALSE(identifyByMatrixPencil(std::vector<double>(500, 0.0), dt));  // no energy
    // An undecaying sine: its Q is unresolved and said so.
    auto pure = identifyByMatrixPencil(ringDown({{1000.0, 1e12, 1.0, 0.0}}, 1e-5, 2000), 1e-5, {});
    ASSERT_TRUE(pure);
    EXPECT_FALSE(pure.value().warnings.empty());
}

TEST(ResonanceSpectroscopy, LorentzianFitsRecoverCenterAndQAndFlagOtherShapes)
{
    std::vector<double> f, y;
    for (int i = 0; i <= 400; ++i) {
        f.push_back(900.0 + 0.5 * i);
        y.push_back(3.0 * lorentzian(f.back(), 1000.0, 200.0) + 0.1);
    }
    auto fit = fitLorentzian(f, y);
    ASSERT_TRUE(fit) << fit.message();
    EXPECT_NEAR(fit.value().centerHz, 1000.0, 1e-8);
    EXPECT_NEAR(fit.value().quality, 200.0, 1e-6);
    EXPECT_NEAR(fit.value().amplitude, 3.0, 1e-8);
    EXPECT_NEAR(fit.value().baseline, 0.1, 1e-8);
    EXPECT_FALSE(fit.value().asymmetric);
    EXPECT_FALSE(fit.value().overlapping);
    // A Fano (asymmetric) line: flagged.
    std::vector<double> fano;
    for (double fr : f) {
        const double e = (fr - 1000.0) / 2.5, qf = 2.0;
        fano.push_back((qf + e) * (qf + e) / (1.0 + e * e) + 0.1);
    }
    auto af = fitLorentzian(f, fano);
    ASSERT_TRUE(af) << af.message();
    EXPECT_TRUE(af.value().asymmetric);
    // Two overlapping peaks: the largest fitted, the overlap flagged; both found by the peak search.
    std::vector<double> two;
    for (double fr : f)
        two.push_back(lorentzian(fr, 1000.0, 200.0) + 0.7 * lorentzian(fr, 1012.0, 200.0));
    auto ov = fitLorentzian(f, two);
    ASSERT_TRUE(ov);
    EXPECT_TRUE(ov.value().overlapping);
    EXPECT_EQ(fitResonancePeaks(f, two, 4).size(), 2u);
    // Coarse sampling: refused.
    std::vector<double> fc, yc;
    for (int i = 0; i <= 40; ++i) {
        fc.push_back(900.0 + 5.0 * i);
        yc.push_back(lorentzian(fc.back(), 1000.0, 200.0));
    }
    auto coarse = fitLorentzian(fc, yc);
    ASSERT_FALSE(coarse);
    EXPECT_NE(coarse.message().find("insufficient frequency resolution"), std::string::npos);
}
