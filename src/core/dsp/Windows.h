#pragma once

// Window functions for spectral analysis of measured records.
//
// Every window here is the periodic ("DFT-even") form of length N, the one
// that is used for averaged spectra (Welch). A window always changes a
// spectrum: it trades frequency resolution for leakage. Nothing in Resonant
// applies one silently; a processing step names the window it used, and the
// properties below are recorded with the result.
#include "foundation/Result.h"

#include <string_view>
#include <vector>

namespace resonant::dsp {

enum class WindowKind {
    Rectangular,     // best resolution, -13 dB sidelobes
    Hann,            // general purpose, -31 dB sidelobes
    Hamming,         // -43 dB first sidelobe, slow roll-off
    BlackmanHarris,  // 4-term, -92 dB sidelobes, wide main lobe
    FlatTop          // amplitude accuracy of sinusoids (scalloping < 0.01 dB), very wide main lobe
};

std::string_view toString(WindowKind kind);
Result<WindowKind> windowKindFromString(std::string_view text);

std::vector<double> windowCoefficients(WindowKind kind, size_t length);

struct WindowProperties {
    double coherentGain = 1.0;        // mean of w: the gain for a sinusoid at a bin centre
    double powerGain = 1.0;           // mean of w^2
    double noiseBandwidthBins = 1.0;  // equivalent noise bandwidth N sum(w^2) / (sum w)^2, in bins
};

WindowProperties windowProperties(const std::vector<double>& window);

}  // namespace resonant::dsp
