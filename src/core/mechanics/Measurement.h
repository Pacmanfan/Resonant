#pragma once

// Measured frequency-response data and its comparison with simulation.
//
// Import only: no hardware acquisition. A CSV file with a header line and
// columns frequency_Hz, then either magnitude and phase_deg or real and
// imag; lines starting with '#' are comments and may carry the metadata
// keys sensor, quantity, unit, position (x y z, m), direction (x y z),
// sensitivity, source.
#include "foundation/Result.h"
#include "foundation/Types.h"
#include "mechanics/MechanicalComponents.h"

#include <complex>
#include <string>
#include <vector>

namespace resonant::mechanics {

struct MeasuredResponse {
    std::string name;
    std::string sensor;
    Quantity quantity = Quantity::Acceleration;
    std::string unit;           // of the values, e.g. "m/s^2/N"
    std::optional<Vec3> position, direction;
    double sensitivity = 0.0;
    std::string source;         // file, laboratory, date
    std::vector<double> frequencyHz;
    std::vector<std::complex<double>> values;
};

Result<MeasuredResponse> parseMeasuredResponse(const std::string& csvText, const std::string& name);

struct ResponseComparison {
    size_t points = 0;                 // measured frequencies inside the simulated range
    double rmsLevelDifferenceDb = 0.0; // RMS of 20 log10(|sim| / |meas|) over those points
    double largestPeakMeasuredHz = 0.0, largestPeakSimulatedHz = 0.0;
    std::string note;
};

// Simulated values are interpolated (linearly in magnitude and in unwrapped
// phase) onto the measured frequencies.
ResponseComparison compareResponses(const std::vector<double>& simulatedHz, const std::vector<std::complex<double>>& simulated,
                                    const MeasuredResponse& measured);

}  // namespace resonant::mechanics
