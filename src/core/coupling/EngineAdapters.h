#pragma once

// Adapters to atomistic engines and the atomistic-to-continuum bridge, in the coupling
// layer because they run the atomistic domain
// (REQ-CHAT-20261128-006, RES-MSC-015, -016, ADR-000234).
//
// An adapter states its engine's applicability (the material classes and
// properties it is suited to, and what it is not for), detects whether the
// engine can run here, prepares the engine's input for a task, and runs it.
// Engines are never linked and never required: a missing engine is reported
// with what is missing, and no result is invented. The adapters:
//   resonant.lattice-statics  the in-house static lattice (atomistic/LatticeStatics):
//                             elastic constants, density and cohesive energy of
//                             cubic crystals of the built-in pair potentials, at 0 K
//   lammps                    LAMMPS: input decks for elastic constants by +-strain
//                             deformations and the parser of their output; runs
//                             only when an executable is found
//   gromacs                   GROMACS: its applicability (soft matter, liquids,
//                             biomolecules) only; no input generation
// Results cross to the continuum as TransferredProperty records. Each carries:
// - its representation and units, the axes it is in;
// - temperature, pressure, composition and simulation conditions;
// - its uncertainty, with its kind (statistical or numerical);
// - the engine, its version, the potential, the method and a digest of the
//   input.
// A record can become a computed material field for a region.

#include "atomistic/Crystal.h"
#include "foundation/Result.h"
#include "materials/MaterialField.h"

#include <Eigen/Core>
#include <nlohmann/json.hpp>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace resonant::coupling {

struct EngineApplicability {
    std::string engine, title;
    std::vector<std::string> materialClasses;
    std::vector<std::string> properties;  // elastic-constants, density, cohesive-energy, thermal-conductivity, diffusion, ...
    std::vector<std::string> notFor;
    std::string note;
    nlohmann::json toJson() const;
};

struct AtomisticTask {
    std::string property;          // elastic-constants, density, cohesive-energy
    atomistic::Crystal structure;
    std::string forceField;        // a built-in force field id (atomistic/ForceField)
    double temperature = 0.0;      // K; 0: static
    double pressure = 0.0;         // Pa
    double strain = 1e-4;          // deformation amplitude for elastic constants
};

struct TransferredProperty {
    std::string quantity;           // elastic-constants, density, cohesive-energy
    std::string representation;     // how `values` are ordered
    std::vector<double> values;
    std::vector<double> uncertainty;   // standard, per value
    std::string uncertaintyKind = "none";  // statistical, numerical, none
    std::string unit;
    std::string frame;              // the axes of a tensor
    double temperature = 0.0, pressure = 0.0;
    std::map<std::string, int> composition;
    nlohmann::json conditions = nlohmann::json::object();
    std::string engine, engineVersion, forceField, method, inputDigest;
    std::string provenance = "computed";
    std::vector<std::string> warnings;
    nlohmann::json toJson() const;
    // A constant, computed material field of a region (elastic-constants: mechanical.stiffness;
    // density: mechanical.density).
    Result<materials::MaterialField> toMaterialField(const std::string& id, const std::string& region) const;
};

class EngineAdapter {
public:
    virtual ~EngineAdapter() = default;
    virtual EngineApplicability applicability() const = 0;
    // Whether the engine can run here; if not, what is missing.
    virtual Status available() const = 0;
    // The engine's input for the task; refused outside its applicability.
    virtual Result<std::string> prepare(const AtomisticTask& task) const = 0;
    // Runs the task (in `workDirectory` for an external engine); refused when the engine is not available.
    virtual Result<std::vector<TransferredProperty>> run(const AtomisticTask& task, const std::string& workDirectory) const = 0;
};

std::vector<std::string> engineAdapterIds();
std::unique_ptr<EngineAdapter> engineAdapter(const std::string& id);

// The elastic constants (Pa, the mechanics Voigt order xx yy zz xy yz zx) from the
// RESONANT-STRESS lines a prepared LAMMPS deck prints: for each strain k (0..5) and
// sign, the pressure components pxx pyy pzz pxy pxz pyz in bar.
Result<Eigen::Matrix<double, 6, 6>> elasticConstantsFromLammpsLog(const std::string& log, double strain);

}  // namespace resonant::coupling
