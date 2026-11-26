// The native Resonant project format: one JSON document.
//
//   {
//     "format": "resonant-project",
//     "schemaVersion": 1,
//     "application": {"name": "Resonant", "version": "0.1.0"},
//     "project": {"id", "name", "createdUtc", "modifiedUtc"},
//     "units": {...},                 // the SI conventions, stated explicitly
//     "materials": [...],
//     "scene": {"objects": [...]},    // nested: each object has "children"
//     "simulations": [...],
//     "extensions": {...}             // preserved as found
//   }
//
// Large numerical data does not belong in this file; a later format version
// stores it in binary files next to the project and references them here.
//
// Loading validates the whole document before a Project is returned: a
// project is either loaded completely or not at all.
#pragma once

#include "foundation/Result.h"
#include "project/Project.h"

#include <filesystem>
#include <functional>
#include <map>

namespace resonant {

inline constexpr const char* kProjectFormatTag = "resonant-project";
inline constexpr int kProjectSchemaVersion = 1;
inline constexpr const char* kProjectFileExtension = "resonant";

struct LoadedProject {
    std::unique_ptr<Project> project;
    // Things that were repaired or ignored while loading (a reference to a
    // missing material, a migration that ran). Empty for a clean load.
    std::vector<std::string> warnings;
};

nlohmann::json serializeProject(const Project& project);
Result<LoadedProject> deserializeProject(nlohmann::json document);

// Writes atomically: the document goes to a temporary file in the same
// directory which then replaces the target, so a failed save never leaves a
// truncated project. Updates project.modifiedUtc.
Status saveProjectFile(Project& project, const std::filesystem::path& path);
Result<LoadedProject> loadProjectFile(const std::filesystem::path& path);

// --- schema migration -------------------------------------------------------

// A step that rewrites a document of version N into version N + 1.
using MigrationStep = std::function<Status(nlohmann::json& document)>;
// Keyed by the version a step migrates FROM.
using MigrationSteps = std::map<int, MigrationStep>;

// Brings `document` from its "schemaVersion" up to targetVersion by applying
// the steps in order, updating "schemaVersion" after each. Fails when the
// version is missing or not an integer, is newer than targetVersion, or a
// step is missing or fails. `notes` receives one line per step applied.
Status migrateDocument(nlohmann::json& document, const MigrationSteps& steps, int targetVersion,
                       std::vector<std::string>& notes);

// The steps of the real format history (empty while version 1 is current).
const MigrationSteps& projectMigrationSteps();

// --- pieces reused by other modules ------------------------------------------

nlohmann::json toJson(const Vec3& vector);
Result<Vec3> vec3FromJson(const nlohmann::json& value, const std::string& what);
nlohmann::json toJson(const Material& material);
Result<Material> materialFromJson(const nlohmann::json& value);

}  // namespace resonant
