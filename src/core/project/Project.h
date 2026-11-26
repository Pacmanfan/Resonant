// A Resonant project: the scene, the material library and the simulation
// configurations, with identity and bookkeeping. This is the scientific
// document. It holds no user-interface state (window layout, selection,
// camera) and no GPU objects.
#pragma once

#include "foundation/Id.h"
#include "materials/Material.h"
#include "scene/Scene.h"
#include "simulation/Simulation.h"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace resonant {

class Project {
public:
    Project();

    // A new project with the example material library and an empty scene.
    static std::unique_ptr<Project> createNew(std::string name = "Untitled");

    Id id;
    std::string name;
    std::string createdUtc;   // ISO 8601
    std::string modifiedUtc;  // ISO 8601, updated on save

    Scene scene;
    MaterialLibrary materials;
    std::vector<SimulationConfig> simulations;

    // Data of future or optional features, keyed by feature name. It is
    // preserved through load and save even when this build does not use it.
    nlohmann::json extensions = nlohmann::json::object();
};

}  // namespace resonant
