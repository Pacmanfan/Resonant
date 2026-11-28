#pragma once

// Laboratory studies (REQ-CHAT-20261126-023, RES-LAB-002, ADR-000227): what an
// experimental investigation is about and what it is compared with. The experiment
// framework (src/core/experiments) defines and runs measurements and holds their records
// and data; a study adds the physics: the objective and hypothesis, the specimen, the
// models it is compared with - each linked at a revision (a content fingerprint), so a
// later change is seen - the nominal and the measured configuration, and the roles of its
// datasets (calibration and validation are kept apart). The laboratory's own records
// (studies, import reports, comparisons, calibrations, validations, raw sources) are in
// the project extension "laboratory".

#include "foundation/Result.h"

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace resonant {
class Project;
}

namespace resonant::laboratory {

inline constexpr const char* kExtension = "laboratory";

// kinds: physical-system, functional-structure, material, material-state-study, design-revision,
// multiscale-representation, scene-object, accounting-study
struct ModelLink {
    std::string kind, id;
    std::string revision;  // the fingerprint of the linked content when it was linked
    std::string label, note;
    nlohmann::json toJson() const;
    static Result<ModelLink> fromJson(const nlohmann::json& j);
};
const std::vector<std::string>& modelLinkKinds();

// A measured deviation from the nominal configuration (an actual dimension, a material batch, a sensor offset).
struct Deviation {
    std::string id, parameter, unit, method, source, note;
    double nominal = 0.0, measured = 0.0, uncertainty = 0.0;  // the uncertainty of the measured value (standard)
    nlohmann::json toJson() const;
    static Result<Deviation> fromJson(const nlohmann::json& j);
};

enum class DatasetRole { Raw, Calibration, Validation, Holdout, Reference, Environment };
std::string_view toString(DatasetRole r);
std::optional<DatasetRole> datasetRoleFromString(std::string_view s);

struct DatasetUse {
    std::string measurementId;
    DatasetRole role = DatasetRole::Raw;
    std::string note;
};

struct Study {
    std::string id, name, objective, hypothesis, domain, experimentId, specimen, operatorName, created;
    std::string status = "planned";  // planned, measured, compared, calibrated, validated, inconclusive
    std::vector<ModelLink> models;
    std::vector<Deviation> deviations;
    nlohmann::json conditions = nlohmann::json::object();  // temperature, excitation, mounting ...
    std::vector<DatasetUse> datasets;
    std::vector<std::string> notes;

    std::vector<std::string> datasetsWith(DatasetRole role) const;
    Status validate() const;
    nlohmann::json toJson() const;
    static Result<Study> fromJson(const nlohmann::json& j);
};
const std::vector<std::string>& studyStatuses();

struct Workspace {
    std::vector<Study> studies;
    nlohmann::json imports = nlohmann::json::array();       // import reports
    nlohmann::json comparisons = nlohmann::json::array();   // comparison records
    nlohmann::json calibrations = nlohmann::json::array();  // calibration records
    nlohmann::json validations = nlohmann::json::array();   // validation records
    nlohmann::json sources = nlohmann::json::array();       // raw files kept: {sha256, file, format, bytes, path}

    Study* study(const std::string& studyId);
    const Study* study(const std::string& studyId) const;
    const nlohmann::json* record(const std::string& section, const std::string& recordId) const;
    nlohmann::json toJson() const;
    static Result<Workspace> fromJson(const nlohmann::json& j);
};

Result<Workspace> workspaceOf(const Project& project);  // empty when the project has none
void storeWorkspace(Project& project, const Workspace& workspace);

// The revision of a linked content: the first 16 hex digits of the SHA-256 of its canonical JSON.
std::string fingerprint(const nlohmann::json& content);

}  // namespace resonant::laboratory
