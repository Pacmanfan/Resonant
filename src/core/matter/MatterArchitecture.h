#pragma once

// Functionally architected matter (REQ-CHAT-20261127-001, RES-FAM-002..009,
// -015, -016, ADR-000228): the canonical record of how the matter of one or
// more scene objects is organized - functional regions with their
// composition, phases, microstructure, orientation and anisotropic
// properties; layer stacks with patterns; interfaces with their models;
// property fields; intended functions and transformation pathways; the
// adaptive class; manufacturing processes, constraints and records. Pure
// data: no physics domain, no coupling. Regions reference library materials,
// which stay as they are.

#include "foundation/Result.h"
#include "matter/PropertyField.h"

#include <nlohmann/json.hpp>

#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace resonant {
class Project;
}

namespace resonant::matter {

inline constexpr const char* kExtension = "matter";
inline constexpr const char* kFormat = "resonant.matter";

// Where a value comes from (RES-FAM-012 of the request: measured, manufacturer,
// literature, first-principles, simulation, estimated, assumed) - the library's
// provenance words plus those of the request.
const std::vector<std::string>& provenanceKinds();
bool isProvenance(const std::string& text);

// A value with its unit, provenance and (standard) uncertainty.
struct SourcedValue {
    double value = 0.0;
    std::string unit;
    std::string provenance = "unstated";
    std::string source;
    std::optional<double> uncertainty;

    nlohmann::json toJson() const;
    // A number (provenance unstated) or {"value", "unit", "provenance", "source", "uncertainty"}.
    static Result<SourcedValue> fromJson(const nlohmann::json& j);
};

// --- composition, phases, microstructure (RES-FAM-003, -004) -----------------------------------

struct Constituent {
    std::string id;
    std::string materialId;     // a library material, or
    std::string substance;      // a named substance ("void" for pores)
    std::string formula;
    std::string materialClass;  // metal, ceramic, polymer, piezoelectric, ...
    double fraction = 0.0;
    std::optional<double> fractionUncertainty;
};

struct Composition {
    std::string kind = "homogeneous";  // homogeneous, mixture, alloy, compound, composite, porous, cellular
    std::string basis = "volume";      // volume, mass, atomic
    std::vector<Constituent> constituents;
    std::string method;  // the effective-property method (Mixing.h), empty: none chosen
    std::string matrix;  // the matrix constituent (Maxwell Garnett, cellular solids)
    std::string note;
    bool empty() const { return constituents.empty(); }
};

struct Phase {
    std::string id, name;
    std::string materialId;
    double fraction = 0.0;
    std::string distribution = "continuous";  // continuous, dispersed, layered, lamellar, random, graded
    nlohmann::json stability = nlohmann::json::object();  // {"temperature": [K, K], "pressure": [Pa, Pa], "field": ...}
    bool metastable = false;
    std::string materialStateStudy;  // a material-state study (RES-DMS) for its evolution
};

struct Microstructure {
    std::string crystalStructure;
    nlohmann::json lattice = nlohmann::json::object();  // {"a", "b", "c" (m), "alpha", "beta", "gamma" (rad)}
    std::optional<SourcedValue> grainSize;               // m
    std::string grainSizeDistribution;
    std::optional<SourcedValue> porosity;                // volume fraction
    std::string texture;
    std::vector<std::string> defects, inclusions;
    std::string domains;
    std::string representation;  // a multiscale representation (RES-MSC) that describes it
    std::string provenance = "unstated";
    std::string note;
};

// Bunge (ZXZ) Euler angles of the material frame in the stack's (or object's) frame, rad.
struct Orientation {
    double phi1 = 0.0, Phi = 0.0, phi2 = 0.0;
    std::string frame = "stack";
    std::array<std::array<double, 3>, 3> rotation() const;  // material -> frame
    bool inPlane() const;  // a rotation about the stack axis only
};

// Anisotropic property sets that the library's scalar properties cannot hold:
//   elastic        E1, E2, G12, nu12 (Pa, Pa, Pa, -): in-plane orthotropic
//   thermal        k1, k2, k3 (W/(m K)): principal conductivities
//   expansion      alpha1, alpha2, alpha3 (1/K)
//   piezoelectric  d31, d32, d33 (C/N), eps33T (relative), with the poling direction
struct AnisotropicSet {
    std::map<std::string, SourcedValue> values;
    std::string poling;  // piezoelectric: "+3" or "-3" (along or against the stack axis)
    const SourcedValue* value(const std::string& key) const;
};
const std::vector<std::string>& anisotropicKinds();

// --- regions, stacks, interfaces (RES-FAM-005, -006, -009) --------------------------------------

struct FunctionalRegion {
    std::string id, name;
    std::string object;                 // the scene object it belongs to
    std::string extent = "object";      // object, layer, subvolume
    nlohmann::json subvolume = nlohmann::json::object();  // {"min": [x, y, z], "max": [x, y, z]} in the object's frame
    std::string role;                   // substrate, transduction, electrode, isolation, adhesion, encapsulation, ...
    std::string materialId;             // the library material (the base when a field or composition refines it)
    std::string materialClass;          // metal, ceramic, polymer, piezoelectric, ...
    Composition composition;
    std::vector<Phase> phases;
    std::optional<Microstructure> microstructure;
    std::optional<Orientation> orientation;
    std::map<std::string, AnisotropicSet> anisotropic;
    std::map<std::string, SourcedValue> properties;  // "domain.key" -> a value for this region (a measured batch)
    nlohmann::json operating = nlohmann::json::object();  // operating conditions ("temperature": [K, K], ...)
    nlohmann::json limits = nlohmann::json::object();     // {"maxTemperature": K, ...} with their source
    std::string process;                // the manufacturing process that makes it
    std::vector<std::string> functions;
    std::string note;
};

struct Pattern {
    std::string kind = "uniform";  // uniform, segmented, periodic, inclusions
    double start = 0.0, end = 1.0; // segmented: the covered span along the length (normalized)
    int segments = 1;
    double gap = 0.0;              // m between segments
    double period = 0.0;           // m
    double fill = 1.0;             // periodic: area fraction of the region's material
    double featureSize = 0.0;      // m, the smallest feature
    std::string note;
    double coverage() const { return end - start; }
};

struct Layer {
    std::string id;
    std::string region;
    double thickness = 0.0;  // m
    std::optional<double> thicknessUncertainty;  // m (standard)
    double angle = 0.0;      // rad, in-plane rotation of the region's material axes about the stack axis
    Pattern pattern;
};

struct Stack {
    std::string id, name;
    std::string object;      // the scene object the layers lie on
    std::string axis = "+z"; // the stack normal in the object's frame
    double length = 0.0, width = 0.0;  // m, the in-plane extent (beam and plate analyses)
    std::vector<Layer> layers;          // from the substrate side outwards
    double thickness() const;
    int layerOf(const std::string& region) const;  // -1 when absent
};

// Interface models and the physics they govern.
struct InterfaceModelKind {
    std::string id, title;
    std::string physics;  // mechanical, thermal, electrical, chemical, all
    std::vector<std::pair<std::string, std::string>> parameters;  // (key, unit)
    std::string description;
};
const std::vector<InterfaceModelKind>& interfaceModelKinds();
const InterfaceModelKind* findInterfaceModelKind(const std::string& id);

struct InterfaceModel {
    std::string kind;
    std::map<std::string, SourcedValue> parameters;
    const SourcedValue* parameter(const std::string& key) const;
};

struct Interface {
    std::string id, name;
    std::array<std::string, 2> regions;  // the two regions it joins
    std::vector<InterfaceModel> models;
    std::optional<SourcedValue> adhesionEnergy;   // J/m^2
    std::optional<SourcedValue> residualStress;   // Pa
    std::string chemicalCompatibility;
    std::vector<std::string> defects;
    std::string preparation;
    std::string note;
    bool joins(const std::string& a, const std::string& b) const;
    const InterfaceModel* model(const std::string& kind) const;
    // The model governing `physics` (mechanical, thermal, electrical): one of that
    // physics, else "perfect"; nullptr when the interface says nothing about it.
    const InterfaceModel* modelFor(const std::string& physics) const;
};

// --- functions, adaptive class, manufacturing (RES-FAM-012, -015, -016) ------------------------

struct Stage {
    std::string id;
    std::string from, to;  // domains: mechanical, electrical, thermal, electromagnetic, acoustic, magnetic, optical, chemical, fluid
    std::string mechanism;
    std::vector<std::string> regions;
    nlohmann::json settings = nlohmann::json::object();
};

struct Function {
    std::string id, name;
    std::string kind;  // load-bearing, transformation, sensing, damping, thermal-management, wave-control, ...
    std::vector<std::string> regions;
    std::vector<Stage> stages;
    nlohmann::json targets = nlohmann::json::object();
    std::string note;
};

struct AdaptiveState {
    std::string id, name;
    nlohmann::json condition = nlohmann::json::object();
    std::string materialStateStudy;
    std::string description;
};

struct Adaptive {
    std::string kind = "static";  // static, passive-adaptive, externally-controlled, reconfigurable
    std::string controlInput;     // externally controlled: what controls it
    std::optional<bool> reversible;
    std::vector<AdaptiveState> states;
};

struct Process {
    std::string id, name;
    std::string kind;  // one of processKinds() (Manufacturing.h)
    nlohmann::json parameters = nlohmann::json::object();
    // minLayerThickness, maxLayerThickness, minFeatureSize, maxDimension (m), temperature (K),
    // maxResidualStress (Pa), roughness (m), repeatability (relative), materialClasses [..]; "source" says where they come from.
    nlohmann::json constraints = nlohmann::json::object();
    std::string note;
};

struct ManufacturingRecord {
    std::string id;
    std::string date;
    std::string batch;
    std::string process;
    nlohmann::json composition = nlohmann::json::object();
    nlohmann::json parameters = nlohmann::json::object();
    nlohmann::json depositionConditions = nlohmann::json::object();
    nlohmann::json heatTreatment = nlohmann::json::object();
    std::vector<std::string> layerSequence;
    std::string interfacePreparation;
    std::vector<std::string> postProcessing;
    nlohmann::json inspections = nlohmann::json::array();
    std::vector<std::string> measuredDatasets;  // laboratory / experiment record ids
    nlohmann::json measuredProperties = nlohmann::json::object();
    std::string note;
};

// --- the architecture ---------------------------------------------------------------------------

struct Architecture {
    std::string id, name, description;
    std::string representation = "idealized";  // idealized, as-designed, as-built
    std::string parent;                        // the architecture this is an alternative of
    std::vector<std::string> objects;
    std::vector<FunctionalRegion> regions;
    std::vector<Stack> stacks;
    std::vector<Interface> interfaces;
    std::vector<PropertyField> fields;
    std::vector<Function> functions;
    Adaptive adaptive;
    std::vector<Process> processes;
    std::vector<ManufacturingRecord> records;
    std::string provenance = "unstated";
    std::string note;

    const FunctionalRegion* region(const std::string& id) const;
    FunctionalRegion* region(const std::string& id);
    const Stack* stack(const std::string& id) const;
    Stack* stack(const std::string& id);
    const Interface* interfaceBetween(const std::string& a, const std::string& b) const;
    Interface* interfaceById(const std::string& id);
    const Process* process(const std::string& id) const;
    const Function* function(const std::string& id) const;
    // The fields refining a region's property ("domain.key"); nullptr when none.
    const PropertyField* field(const std::string& region, const std::string& property) const;

    nlohmann::json toJson() const;
    static Result<Architecture> fromJson(const nlohmann::json& j);
};

struct Issue {
    std::string severity;  // error, warning, info
    std::string code;
    std::string element;   // the region, layer, interface ... it concerns
    std::string message;
    nlohmann::json toJson() const;
};

// Structural validation (RES-FAM-002): identities, references (objects in the
// scene, materials in the library, regions, processes, fields), fractions,
// thicknesses, interfaces between touching regions, consecutive layers without
// an interface (a warning: nothing is assumed perfect), adaptive classes.
std::vector<Issue> validate(const Architecture& architecture, const Project* project = nullptr);
bool hasErrors(const std::vector<Issue>& issues);

// The content revision (16 hex digits of SHA-256 of the canonical JSON).
std::string revision(const Architecture& architecture);

struct Workspace {
    std::vector<Architecture> architectures;
    nlohmann::json comparisons = nlohmann::json::array();  // kept comparisons of alternatives
    const Architecture* find(const std::string& id) const;
    Architecture* find(const std::string& id);
};
Result<Workspace> workspaceOf(const Project& project);
void storeWorkspace(Project& project, const Workspace& workspace);

}  // namespace resonant::matter
