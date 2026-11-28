#pragma once

// The physical system of a coupled simulation (RES-CEME-005,
// docs/architecture/PHYSICAL_SYSTEM_MODEL.md): regions with materials and
// dimensions, domains with their solver and settings, couplings between the
// domains, material laws, environment, scheme and convergence settings, and
// observation points. Serialized as JSON ("resonant.physical-system",
// version 1) and stored in the project's extension "multiphysics".
//
// Ids are stable and chosen once; materials are referenced by their library
// id, so renaming a material or a system keeps every reference.
//
// Geometry kinds: "layered-slab" (regions are layers along +z from z = 0,
// with a lateral area). Systems built from scene objects are planned.
#include "foundation/Result.h"
#include "foundation/Types.h"
#include "multiphysics/MaterialState.h"

#include <nlohmann/json.hpp>

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace resonant::multiphysics {

struct RegionDef {
    std::string id, name, materialId;
    double thickness = 0.0;  // m (layered-slab)
    int cells = 20;          // cells across the region for the participants' grids
    std::string objectId;    // optional scene object this region stands for
    std::map<std::string, double> dimensions;  // rectangular-cavity: "a", "b", "d" (m)
};

struct DomainDef {
    std::string id;      // "em", "thermal", ...
    std::string kind;    // "electromagnetic", "thermal", "mechanical", "acoustic"
    std::string solver;  // the participant's solver id
    nlohmann::json settings = nlohmann::json::object();
};

struct CouplingDef {
    std::string id, model, from, to;
    std::string transfer;  // empty: the model's default (conservative / interpolating)
};

struct SchemeDef {
    std::string kind = "one-way";       // one-way, sequential, iterative
    int passes = 2;                     // sequential
    int maxIterations = 30;             // iterative
    double temperatureTolerance = 1e-3; // K: the largest change of the last state update
    double geometryTolerance = 1e-9;    // relative to the reference dimension
    double relaxation = 1.0;            // (0, 1]: of state and geometry updates
    double balanceTolerance = 1e-6;     // relative energy residual accepted as balanced
};

struct ObservationDef {
    std::string id;
    Vec3 point = Vec3::Zero();  // m, world
};

struct PhysicalSystem {
    std::string id, name, description;
    std::string geometryKind = "layered-slab";
    double lateralArea = 1e-2;          // m^2 (layered-slab)
    double ambientTemperature = 293.15; // K: the reference (stress-free) temperature
    std::string incidenceMaterial;      // material id of the half-space before the layers (empty: vacuum)
    std::string exitMaterial;           // after the layers (empty: vacuum)
    std::vector<RegionDef> regions;
    std::vector<DomainDef> domains;
    std::vector<CouplingDef> couplings;
    std::vector<PropertyLaw> laws;
    SchemeDef scheme;
    std::vector<ObservationDef> observations;

    const RegionDef* region(const std::string& id) const;
    const DomainDef* domain(const std::string& id) const;
    double totalThickness() const;

    nlohmann::json toJson() const;
    static Result<PhysicalSystem> fromJson(const nlohmann::json& j);
    std::string checksum() const;  // of the canonical JSON (FNV-1a, hex)
};

// What a coupling model is (the concrete list is in src/core/coupling).
struct CouplingModelDescriptor {
    std::string id, name;
    std::string sourceDomain, targetDomain;  // domain kinds
    std::string kind;                        // field-transfer, state-update, geometry-update
    std::string sourceQuantity, targetQuantity;
    std::string defaultTransfer = "conservative";
    std::string mechanism, equations;
    std::string status;  // validated, experimental, planned
    std::string validation, limitations;
};

struct SystemIssue {
    int severity = 3;  // 1 info, 2 warning, 3 error
    std::string field, message;
};

// Checks references, ids, models, quantities and the scheme. `materialExists`
// answers whether a material id is in the library; `solvers` maps a domain
// kind to the solver ids that have a participant.
std::vector<SystemIssue> validateSystem(const PhysicalSystem& system, const std::vector<CouplingModelDescriptor>& models,
                                        const std::function<bool(const std::string&)>& materialExists,
                                        const std::vector<std::pair<std::string, std::string>>& solvers);

}  // namespace resonant::multiphysics
