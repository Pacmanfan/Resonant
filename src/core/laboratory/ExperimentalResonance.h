#pragma once

// Resonances from measurements (REQ-CHAT-20261126-023, RES-LAB-012), with the experiment's
// evidence rather than an assumption that every peak is a resonance of the structure:
//   frequency responses  Lorentzian fits of |H|^2 (dsp), each peak with its frequency, Q,
//                        damping ratio, bandwidth, uncertainty, signal-to-noise ratio and phase
//                        change, then classified:
//                          artifact                  below the signal-to-noise margin
//                          excitation or instrument  the reference channel peaks there too
//                          possible interference     at a multiple of the mains frequency, narrow
//                          natural mode              the phase turns by at least 90 degrees across it
//                          driven-response peak      the phase hardly turns (a forced or excitation peak)
//                          unclassified              the phase is not measured, or the line is too coarse
//                        and matched with a predicted mode when one lies within the tolerance
//   ring-downs           the matrix pencil (dsp) on the free decay: frequency, decay rate,
//                        Q, damping ratio, the energy decay rate (model-derived: from the
//                        amplitude, not a measured energy) and the frequency drift between the
//                        record's halves, with the assumptions the fit needs

#include "foundation/Result.h"

#include <nlohmann/json.hpp>

#include <complex>
#include <string>
#include <vector>

namespace resonant::laboratory {

struct ResonanceOptions {
    int maxPeaks = 6;
    double minSnrDb = 10.0;
    std::vector<double> referencePeaksHz;  // peaks of the excitation or reference channel
    std::vector<double> predictedHz;       // predicted natural frequencies
    double matchTolerance = 0.05;          // relative
    double mainsHz = 50.0;                 // 0: do not look for interference
};

struct ResonanceCandidate {
    double frequencyHz = 0.0, frequencyUncertainty = 0.0, quality = 0.0, qualityUncertainty = 0.0, dampingRatio = 0.0, bandwidthHz = 0.0;
    double amplitude = 0.0, snrDb = 0.0, phaseChangeDeg = 0.0;
    bool phaseKnown = false;
    std::string classification, method;
    std::vector<std::string> evidence;
    int predictedIndex = -1;
    double predictedHz = 0.0;
    nlohmann::json toJson() const;
};

Result<std::vector<ResonanceCandidate>> resonancesFromResponse(const std::vector<double>& frequencyHz, const std::vector<std::complex<double>>& response,
                                                               bool phaseMeasured, const ResonanceOptions& options = {});

struct RingDown {
    double frequencyHz = 0.0, frequencyUncertainty = 0.0, decayRate = 0.0, quality = 0.0, qualityUncertainty = 0.0, dampingRatio = 0.0;
    double energyDecayRate = 0.0, frequencyDriftHz = 0.0, relativeResidual = 0.0;
    std::string method = "matrix pencil (free decay)";
    std::vector<std::string> assumptions, warnings;
    nlohmann::json toJson() const;
};

Result<RingDown> ringDown(const std::vector<double>& samples, double dt, double minHz = 0.0, double maxHz = 0.0);

}  // namespace resonant::laboratory
