#pragma once

// Frequency analysis of sampled time signals.
//
// What a sampled record can and cannot tell is part of the result: the
// spectrum carries its sampling rate, the observation time, the frequency
// resolution that follows from it, and the window that was used. Zero
// padding gives more points on the same curve; it does NOT improve the
// resolution, and the resolution reported here never depends on it.
#include "foundation/Types.h"

#include <complex>
#include <limits>
#include <string>
#include <vector>

namespace resonant::dsp {

enum class Window { Rectangular, Hann };

struct Spectrum {
    std::vector<double> frequencyHz;               // 0 ... Nyquist
    std::vector<std::complex<double>> amplitude;   // see computeSpectrum for the scaling
    double sampleRateHz = 0.0;
    double nyquistHz = 0.0;            // frequencies above this alias
    double observationSeconds = 0.0;   // length of the analysed record
    double resolutionHz = 0.0;         // smallest separation of two components that can be told apart
    double binSpacingHz = 0.0;         // spacing of the frequency points (finer than the resolution when zero-padded)
    size_t sampleCount = 0;
    size_t paddedCount = 0;
    Window window = Window::Hann;

    std::string describe() const;
};

// In-place radix-2 FFT; the size must be a power of two.
void fft(std::vector<std::complex<double>>& data);

// Spectrum of a real record sampled every dt seconds.
//
// Scaling: a steady sinusoid A cos(2 pi f t + phi) that lasts the whole
// record reads A e^{j phi} at its frequency (the window's coherent gain is
// divided out). For a transient the values are therefore relative to the
// record length; ratios between probes and between frequencies are
// meaningful, absolute values are not spectral densities.
//
// Resolution: 1 / T for the rectangular window and 2 / T for the Hann
// window (the width of its main lobe), T the observation time. Leakage: the
// rectangular window's sidelobes start at -13 dB, the Hann window's at
// -31 dB.
Spectrum computeSpectrum(const std::vector<double>& samples, double dt, Window window = Window::Hann, int zeroPadFactor = 4);

struct SpectralPeak {
    double frequencyHz = 0.0;
    double magnitude = 0.0;
};

// Local maxima of the magnitude that reach `minRelative` of the largest one,
// within [minHz, maxHz], strongest first. A peak's frequency is refined by a
// parabola through its three points, but is still uncertain by about half
// the spectrum's resolution.
//
// These are peaks of a response at one position for one excitation. They
// indicate resonances; they are not calculated eigenmodes.
std::vector<SpectralPeak> findPeaks(const Spectrum& spectrum, double minRelative = 0.05, double minHz = 0.0,
                                    double maxHz = std::numeric_limits<double>::infinity());

}  // namespace resonant::dsp
