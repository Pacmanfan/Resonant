#pragma once

// Experiment reports: a self-contained HTML document describing an
// experiment and one run of it (description, domain, hardware and its
// configuration, sensors and actuators, calibrations, acquisition, timing,
// environment, safety, processing, results, comparisons, uncertainty and
// known limitations).
//
// Every data set is labelled with its category (raw, calibrated,
// processed, simulated, reconstructed), and data from simulated devices is
// marked as generated, not measured.
#include "experiments/ExperimentProject.h"
#include "experiments/ExperimentRunner.h"

#include <string>

namespace resonant::experiments {

std::string experimentReportHtml(const Experiment& experiment, const RunResult& run, const CalibrationLibrary& calibrations,
                                 const std::vector<ComparisonRecord>& comparisons);

// HTML escaping, for reports and anything else that writes HTML.
std::string escapeHtml(const std::string& text);

}  // namespace resonant::experiments
