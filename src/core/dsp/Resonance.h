#pragma once

// Resonance spectroscopy (RES-RSN-003): identifying resonances from signals
// and spectra, independent of the physics that produced them.
//
//  - Complex poles of a sampled signal x[n] = sum_k A_k e^{(-alpha_k + j w_k) n dt}
//    by the matrix pencil method (Hua and Sarkar, IEEE Trans. ASSP 38, 1990):
//    frequency, decay rate, Q = w / (2 alpha), amplitude and phase of several,
//    also overlapping, modes; the spread over sub-windows as the uncertainty.
//  - Lorentzian fits of response peaks P(f) = A / (1 + ((f - f0) / g)^2) + B by
//    Levenberg-Marquardt, with fit quality, uncertainty from the covariance,
//    and flags for asymmetric line shapes and overlapping modes.
//
// Records too short or too coarse for what is asked are refused with the
// reason: an identification is reported only when the data can support it.

#include "foundation/Result.h"

#include <limits>
#include <string>
#include <vector>

namespace resonant::dsp {

struct IdentifiedResonance {
    double frequencyHz = 0.0;
    double decayRate = 0.0;           // alpha, 1/s: amplitude ~ e^{-alpha t}; energy ~ e^{-2 alpha t}
    double quality = 0.0;             // w / (2 alpha); infinity for a decay below the resolution
    double bandwidthHz = 0.0;         // full width at half power: alpha / pi
    double amplitude = 0.0;           // peak amplitude at the first sample used
    double phase = 0.0;               // rad
    double frequencyUncertainty = 0.0;  // Hz
    double qualityUncertainty = 0.0;
    std::string method;               // "matrix-pencil", "lorentzian-fit"
    bool overlapping = false;         // another resonance lies within a few bandwidths
    bool asymmetric = false;          // the line shape is not Lorentzian (Fano-like)
    double fitQuality = 0.0;          // 1 - relative residual energy (1 = exact)
};

struct PencilOptions {
    double minHz = 0.0;
    double maxHz = std::numeric_limits<double>::infinity();
    int maxModes = 12;
    double singularValueThreshold = 1e-5;  // relative to the largest
    size_t skipSamples = 0;                 // e.g. the excitation
    size_t maxSamples = 800;                // longer records are decimated (boxcar)
};

struct Identification {
    std::vector<IdentifiedResonance> resonances;  // ascending frequency
    double relativeResidual = 0.0;                // |x - reconstruction| / |x|
    double durationSeconds = 0.0;
    double sampleRateHz = 0.0;
    std::vector<std::string> warnings;
};

// Fails for fewer than 3 periods of the lowest frequency of interest, fewer than 2.5
// samples per period at maxHz, or a signal without energy.
Result<Identification> identifyByMatrixPencil(const std::vector<double>& samples, double dt, const PencilOptions& options = {});

struct LorentzianFit {
    double centerHz = 0.0;
    double halfWidthHz = 0.0;   // half width at half maximum (of power)
    double quality = 0.0;       // f0 / (2 g)
    double amplitude = 0.0;
    double baseline = 0.0;
    double relativeResidual = 0.0;
    double centerUncertainty = 0.0;
    double qualityUncertainty = 0.0;
    bool asymmetric = false;
    bool overlapping = false;
    int pointsInLinewidth = 0;
};

// Fits the largest peak (or the one nearest `near` when > 0) of a power response.
// Fails when fewer than 5 points fall within the linewidth (insufficient frequency resolution).
Result<LorentzianFit> fitLorentzian(const std::vector<double>& frequencyHz, const std::vector<double>& power, double near = 0.0);

// Every peak of a power response, fitted in turn (largest first, at most `count`).
std::vector<LorentzianFit> fitResonancePeaks(const std::vector<double>& frequencyHz, const std::vector<double>& power, int count,
                                             std::vector<std::string>* warnings = nullptr);

}  // namespace resonant::dsp
