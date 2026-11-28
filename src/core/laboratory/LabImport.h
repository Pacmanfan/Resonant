#pragma once

// Import of measurement files (REQ-CHAT-20261126-023, RES-LAB-003, -004): CSV, TSV, JSON
// and WAV into the experiment framework's measurement records, with an import report that
// lists every issue instead of discarding anything silently.
//
//   # quantity: Displacement per Force        metadata lines ("# key: value") before the header
//   # instrument: ...   # provenance: measured | synthetic   # frame: ...   # uncertainty: 2 %
//   frequency [Hz], magnitude [m/N], phase [deg]
//   10, 1.02e-4, -1.5
//
// The first column decides the kind (time [s]: time series; frequency [Hz]: a frequency
// response, real/imag or magnitude/phase columns; x, y, z [m]: spatial points; a
// "quantity, value, unit, uncertainty" table: scalars). Units come from the header and are
// converted to SI with the conversion recorded in the record's acquisition ("importSteps": a
// raw record has no processing history); degC is converted to K by adding 273.15, also
// recorded. An import with an error creates no dataset unless
// acceptPartial is set, and then the rows or columns left out are recorded. The original
// bytes' SHA-256 goes into the report and every record.

#include "experiments/MeasurementRecord.h"

#include <nlohmann/json.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace resonant::laboratory {

struct ImportOptions {
    std::string format = "auto";      // auto (from the file name), csv, tsv, json, wav
    std::string fileName;             // the original name (for the report and the format)
    std::string experimentId = "laboratory";
    std::string idPrefix;             // dataset ids: <prefix>-<column>; default from the file name
    std::string provenance;           // measured, synthetic; empty: from the metadata
    std::string domain;               // acoustic, mechanical, electromagnetic, thermal; empty: from the metadata
    std::vector<std::string> expectedColumns;  // column names that must be present
    bool acceptPartial = false;
};

struct ImportIssue {
    std::string severity;  // error, warning, info
    std::string code;      // missing-unit, unknown-unit, malformed-row, missing-value, not-finite, non-monotonic-time,
                           // duplicate-time, inconsistent-sampling, sampling-mismatch, missing-column, missing-metadata,
                           // missing-frame, empty, unreadable, conversion, assumed-quantity
    std::string message;
    long row = -1;          // 1-based line of the file
    std::string column;
};

struct ImportedDataset {
    experiments::MeasurementRecord record;
    experiments::MeasurementData data;
};

struct ImportReport {
    std::string id, file, format, sha256, kind, provenance, created;
    size_t bytes = 0, rows = 0, columns = 0;
    std::vector<ImportIssue> issues;
    bool accepted = false;
    std::vector<std::string> datasets;
    nlohmann::json metadata = nlohmann::json::object();
    size_t count(const std::string& severity) const;
    nlohmann::json toJson() const;
};

struct ImportResult {
    ImportReport report;
    std::vector<ImportedDataset> datasets;
};

// Never fails: what goes wrong is in the report (accepted false, no datasets).
ImportResult importData(std::string_view content, const ImportOptions& options);

}  // namespace resonant::laboratory
