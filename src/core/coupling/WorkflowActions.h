#pragma once

// The workflow actions of every domain and the built-in workflow templates
// (ADR-000215). They call the same functions the application's panels call.
#include "automation/WorkflowEngine.h"

#include <nlohmann/json.hpp>

#include <string>

namespace resonant {
class Project;
class SceneObject;
}  // namespace resonant

namespace resonant::coupling {

void registerWorkflowActions(automation::ActionRegistry& registry);
void registerBuiltinWorkflows(automation::WorkflowLibrary& library);

// Scene helpers shared with the Python bindings.
//   type: geometry, emitter, array, target, probe, region, manipulator,
//         structure, actuator, sensor, vibration-target, antenna, em-array, em-source
// Fields: id, name, kind (geometry kind), parameters (geometry), component
// (merged into the type's default component data), position, rotation,
// material, parent.
Result<std::string> addSceneObject(Project& project, const nlohmann::json& description);
// Changes an object: name, position, rotation, visible, parameters (geometry,
// set together and validated), component (merge patch), material.
Status changeSceneObject(Project& project, const std::string& reference, const nlohmann::json& changes);
// An object by id, or else by name.
const SceneObject* findSceneObject(const Project& project, const std::string& reference);
// The object types and component types addSceneObject knows.
std::vector<std::string> sceneObjectTypes();

}  // namespace resonant::coupling
