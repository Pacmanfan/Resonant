#pragma once

// Accounting studies (REQ-CHAT-20261126-022, RES-ACC-012, -016, ADR-000226): a study names
// a source - a lumped network (transient or driven), a physical system of the project (a
// coupled run or a time-stepped evolution), a plane wave on a layered body, or a
// material-state study - the accounting systems to evaluate it for, and the tolerances.
// Running it builds the ledger from the source's own outputs and a report per system (the
// whole system first); a transient network can be repeated at half the step, to show how
// the residuals change with refinement. The result (format resonant.accounting-result) can
// be reclassified for any other system without running again, compared across systems,
// and exported as JSON, CSV or Markdown.
//
//   {"id", "name",
//    "source": {"kind": "network", "network": {...}, "transient": {...}, "refine": true, "resonance": {"frequencyHz", "driveOff"}}
//            | {"kind": "network-driven", "network": {...}, "force": [...], "frequencyHz" | "mode"}
//            | {"kind": "physical-system", "system": "<id>"}
//            | {"kind": "evolution", "system": "<id>", "duration", "steps", "substeps"}
//            | {"kind": "radiation", "frequencyHz", "intensity", "area", "points", "layers": [...]}
//            | {"kind": "material-state", "study": "<id>" | "inline": {...}, "temperature"},
//    "systems": [{"id", "name", "kind", "members": [...]}],
//    "tolerances": {"relative", "warning", "absolute"}}

#include "accounting/AccountingLedger.h"
#include "foundation/Result.h"

#include <nlohmann/json.hpp>

#include <string>

namespace resonant {
class JobContext;
class Project;
}  // namespace resonant

namespace resonant::coupling {

inline constexpr const char* kAccountingResultFormat = "resonant.accounting-result";

// The source kinds and what each needs.
nlohmann::json accountingSourceKinds();
Status validateAccountingStudy(const nlohmann::json& study);

Result<nlohmann::json> runAccountingStudy(const nlohmann::json& study, const Project& project, JobContext* context);

// The report of a result for another system (members of its ledger).
Result<nlohmann::json> accountingReport(const nlohmann::json& result, const nlohmann::json& system);
// Reports for several systems and the exchanges whose role differs between them.
Result<nlohmann::json> compareAccountingSystems(const nlohmann::json& result, const nlohmann::json& systems);
// format: json, csv, markdown
Result<std::string> exportAccounting(const nlohmann::json& result, const std::string& format);

// The quantities of one facet for a system, each with its identity, value or vector, unit,
// domain, context (entities, interval), method, provenance, uncertainty and role or status:
// facet = energy, momentum, angular-momentum, entropy, forces, torques, transfers, missing, residuals.
Result<nlohmann::json> queryAccounting(const nlohmann::json& result, const nlohmann::json& system, const std::string& facet);

// A record to keep in the project (extension "accounting"): the study, the ledger with its
// histories thinned to at most `samples` (selective retention: the reports are evaluated
// again from it), the systems, tolerances and status, and where it came from.
nlohmann::json accountingRecord(const nlohmann::json& result, const std::string& id, const std::string& name, int samples = 200);
// The reports of a stored record, evaluated from its ledger.
Result<nlohmann::json> accountingRecordReports(const nlohmann::json& record);

}  // namespace resonant::coupling
