#pragma once

// Averaged spectral estimates of measured records (Welch's method): power
// spectral densities, cross spectra, the H1 transfer-function estimate, the
// ordinary coherence and the impulse response that follows from H1.
//
// Conventions (one-sided, for real records sampled at fs):
//   G_xx(f) = 2 <|X(f)|^2> / (fs sum w^2)        unit^2 / Hz   (DC and Nyquist: factor 1)
//   G_xy(f) = 2 <conj(X(f)) Y(f)> / (fs sum w^2)  unitx unity / Hz
//   H1(f)   = G_xy / G_xx                          unity / unitx
//   gamma^2 = |G_xy|^2 / (G_xx G_yy)               0 ... 1
// with X the DFT of one windowed segment and <> the average over segments.
// Integrating G_xx over frequency gives the mean square of the record
// (Parseval), whatever the window.
//
// H1 is unbiased by noise on the output and biased low by noise on the
// input. The coherence tells how much of the output is linearly explained
// by the input; the normalized random error of |H1| for n_d averages is
// sqrt(1 - gamma^2) / (|gamma| sqrt(2 n_d)) (Bendat and Piersol).
#include "dsp/Windows.h"
#include "foundation/Result.h"

#include <complex>
#include <string>
#include <vector>

namespace resonant::dsp {

struct WelchSettings {
    size_t segmentLength = 1024;  // samples, a power of two
    double overlap = 0.5;         // fraction of a segment, [0, 0.95]
    WindowKind window = WindowKind::Hann;
    bool removeMean = false;      // subtract each segment's mean (only when asked)

    Status validate(size_t recordLength) const;
    std::string describe() const;
};

struct SpectralGrid {
    std::vector<double> frequencyHz;  // 0 ... fs/2, spacing fs / segmentLength
    double sampleRateHz = 0.0;
    double binSpacingHz = 0.0;
    double resolutionHz = 0.0;        // equivalent noise bandwidth of the window
    size_t averages = 0;              // number of segments averaged
    WelchSettings settings;
};

struct PowerSpectralDensity {
    SpectralGrid grid;
    std::vector<double> density;  // unit^2 / Hz
};

struct CrossSpectra {
    SpectralGrid grid;
    std::vector<double> gxx, gyy;
    std::vector<std::complex<double>> gxy;
};

struct TransferFunctionEstimate {
    SpectralGrid grid;
    std::vector<std::complex<double>> h1;
    std::vector<double> coherence;         // gamma^2
    std::vector<double> magnitudeRandomError;  // normalized standard error of |H1| (fraction)
};

Result<PowerSpectralDensity> welchPsd(const std::vector<double>& x, double sampleRateHz, const WelchSettings& settings);
Result<CrossSpectra> crossSpectra(const std::vector<double>& x, const std::vector<double>& y, double sampleRateHz,
                                  const WelchSettings& settings);
// x is the input (reference), y the output (response).
Result<TransferFunctionEstimate> estimateTransferFunction(const std::vector<double>& x, const std::vector<double>& y,
                                                          double sampleRateHz, const WelchSettings& settings);

// The impulse response of an H1 estimate: the inverse DFT over the segment
// length (it is periodic with that length; a response longer than one
// segment wraps around and is wrong).
struct ImpulseResponse {
    std::vector<double> samples;
    double sampleRateHz = 0.0;
};
ImpulseResponse impulseResponse(const TransferFunctionEstimate& estimate);

// The complex amplitude of the component at exactly `frequencyHz` in a
// windowed record (single-bin DFT, Goertzel-equivalent), scaled so that a
// steady sinusoid A cos(2 pi f t + phi) reads A e^{j phi} when the record
// holds a whole number of its periods (the window's coherent gain is
// divided out).
std::complex<double> toneAmplitude(const std::vector<double>& x, double sampleRateHz, double frequencyHz,
                                   WindowKind window = WindowKind::Hann);

// Root-mean-square of a record.
double rms(const std::vector<double>& x);

// The integral of a density over [f1, f2] (trapezoids on its grid).
double bandPower(const PowerSpectralDensity& psd, double f1, double f2);

}  // namespace resonant::dsp
