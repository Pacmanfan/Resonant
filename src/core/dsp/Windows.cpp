#include "dsp/Windows.h"

#include "foundation/Units.h"

#include <cmath>
#include <string>

namespace resonant::dsp {

std::string_view toString(WindowKind kind)
{
    switch (kind) {
    case WindowKind::Rectangular: return "rectangular";
    case WindowKind::Hann: return "hann";
    case WindowKind::Hamming: return "hamming";
    case WindowKind::BlackmanHarris: return "blackman-harris";
    case WindowKind::FlatTop: return "flat-top";
    }
    return "rectangular";
}

Result<WindowKind> windowKindFromString(std::string_view text)
{
    for (WindowKind kind : {WindowKind::Rectangular, WindowKind::Hann, WindowKind::Hamming, WindowKind::BlackmanHarris,
                            WindowKind::FlatTop})
        if (text == toString(kind))
            return kind;
    return Result<WindowKind>::failure("unknown window '" + std::string(text) +
                                       "' (rectangular, hann, hamming, blackman-harris, flat-top)");
}

std::vector<double> windowCoefficients(WindowKind kind, size_t length)
{
    std::vector<double> w(length, 1.0);
    const double n = static_cast<double>(length);
    // Cosine-sum windows: w[i] = sum_k (-1)^k a_k cos(2 pi k i / N).
    auto cosineSum = [&](std::initializer_list<double> a) {
        for (size_t i = 0; i < length; ++i) {
            double value = 0.0;
            int k = 0;
            for (double ak : a) {
                value += ((k % 2) ? -ak : ak) * std::cos(2.0 * units::pi * k * static_cast<double>(i) / n);
                ++k;
            }
            w[i] = value;
        }
    };
    switch (kind) {
    case WindowKind::Rectangular: break;
    case WindowKind::Hann: cosineSum({0.5, 0.5}); break;
    case WindowKind::Hamming: cosineSum({0.54, 0.46}); break;
    case WindowKind::BlackmanHarris: cosineSum({0.35875, 0.48829, 0.14128, 0.01168}); break;
    // The 5-term flat-top window of common analysers (the coefficients of MATLAB's flattopwin).
    case WindowKind::FlatTop: cosineSum({0.21557895, 0.41663158, 0.277263158, 0.083578947, 0.006947368}); break;
    }
    return w;
}

WindowProperties windowProperties(const std::vector<double>& window)
{
    WindowProperties p;
    if (window.empty())
        return p;
    double sum = 0.0, sumSquares = 0.0;
    for (double v : window) {
        sum += v;
        sumSquares += v * v;
    }
    const double n = static_cast<double>(window.size());
    p.coherentGain = sum / n;
    p.powerGain = sumSquares / n;
    p.noiseBandwidthBins = sum != 0.0 ? n * sumSquares / (sum * sum) : 0.0;
    return p;
}

}  // namespace resonant::dsp
