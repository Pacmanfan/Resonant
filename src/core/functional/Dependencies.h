#pragma once

// Revisions and staleness of functional results (RES-FUNC-004): content keys
// (FNV-1a of canonical JSON) of what a result depends on — the referenced
// objects' geometry, components and world placement, the referenced
// materials, the structure document and the discretization — compared with
// the current project. Content keys survive save and load and see changes
// made by any client, which a counter in the scene would not.

#include "functional/FunctionalStructure.h"

#include <nlohmann/json.hpp>

#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace resonant::functional {

struct DependencyRecord {
    std::string geometry;        // all referenced objects together
    std::string materials;       // all referenced materials together
    std::string definition;      // the structure document
    std::string discretization;  // the model and its settings
    std::map<std::string, std::string> objects;    // per object id
    std::map<std::string, std::string> materialKeys;  // per material id

    nlohmann::json toJson() const;
    static DependencyRecord fromJson(const nlohmann::json& value);
};

std::set<std::string> referencedObjects(const FunctionalStructure& structure);
std::set<std::string> referencedMaterials(const FunctionalStructure& structure, const Project& project);

DependencyRecord dependenciesOf(const FunctionalStructure& structure, const Project& project, const nlohmann::json& discretization);

struct StaleReason {
    std::string what;    // geometry, material, definition, discretization
    std::string detail;
};
// Empty: the result is current.
std::vector<StaleReason> staleness(const DependencyRecord& recorded, const DependencyRecord& current, const Project& project);
nlohmann::json toJson(const std::vector<StaleReason>& reasons);

// Simulation geometry (meshes, grids, prepared studies) reused only while its
// key — the geometry key plus the discretization key — is unchanged.
class SimulationGeometryCache {
public:
    static SimulationGeometryCache& shared();
    std::shared_ptr<const void> find(const std::string& key);
    void store(const std::string& key, std::shared_ptr<const void> value);
    void clear();
    size_t hits() const;
    size_t misses() const;

private:
    mutable std::mutex mutex_;
    std::vector<std::pair<std::string, std::shared_ptr<const void>>> entries_;  // most recent last
    size_t hits_ = 0, misses_ = 0;
    static constexpr size_t kCapacity = 16;
};

}  // namespace resonant::functional
