#pragma once

// One-port Touchstone (.s1p) files: reading measured reflection data and
// writing calculated data (Touchstone 1.1: "# <unit> S <RI|MA|DB> R <Z0>").
#include "electromagnetics/Conventions.h"
#include "foundation/Result.h"

#include <string>
#include <vector>

namespace resonant::em {

struct OnePortData {
    std::vector<double> frequencyHz;
    std::vector<Complex> reflection;  // S11
    double referenceImpedance = 50.0;
    std::string comments;
    // Input impedance Z0 (1 + S) / (1 - S).
    std::vector<Complex> impedance() const;
};

Result<OnePortData> parseTouchstone1(const std::string& text);
// Written as GHz, RI, with the comments as "!" lines.
std::string writeTouchstone1(const OnePortData& data);

}  // namespace resonant::em
