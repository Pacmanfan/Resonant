#pragma once

// The orchestrator of a coupled simulation (RES-CEME-006, RES-CEME-007,
// docs/physics/COUPLED_SIMULATION_EXECUTION.md).
//
// Participants adapt domain solvers (src/core/coupling); the orchestrator
// only knows this interface. A pass prepares and solves every participant
// once, in the order of the field transfers of the coupling graph (each sees
// the updates made earlier in the pass: Gauss-Seidel), and after each solve
// applies the couplings that start at it:
//
//   field-transfer   the source's quantity, on the target's support through a
//                    transfer operator, into the target (its inputs);
//   state-update     a temperature field into the material state (relaxed);
//   geometry-update  the source's geometry changes into the geometry state (relaxed).
//
// One-way: one pass (no state or geometry updates). Sequential: a fixed
// number of passes. Iterative: passes until the largest state change of a
// pass is below the tolerances. A participant failure, a non-finite value or
// a cancellation ends the run with its status; the completed passes stay in
// the result.
#include "foundation/Result.h"
#include "multiphysics/FieldData.h"
#include "multiphysics/GeometryState.h"
#include "multiphysics/Ledger.h"
#include "multiphysics/MaterialState.h"
#include "multiphysics/PhysicalSystem.h"

#include <nlohmann/json.hpp>

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace resonant::multiphysics {

struct SystemState {
    const PhysicalSystem* system = nullptr;
    MaterialState materials;
    GeometryState geometry;
};

struct RunContext {
    std::function<bool()> cancelled;
    std::function<void(double, const std::string&)> progress;
};

class Participant {
public:
    virtual ~Participant() = default;
    virtual std::string id() const = 0;        // the domain id in the physical system
    virtual std::string domain() const = 0;    // the domain kind
    virtual std::string solverId() const = 0;
    virtual std::string solverVersion() const = 0;
    // (Re)builds from the geometry and material state; called before every pass
    // (a participant may skip the work when the revisions it used are unchanged).
    virtual Status prepare(SystemState& state) = 0;
    virtual Status solve(SystemState& state, const RunContext& context) = 0;
    // A quantity this participant produces, after solve().
    virtual std::optional<FieldData> provide(const std::string& quantity, const SystemState& state) const = 0;
    // The grid on which it takes field inputs.
    virtual GridSupport support(const SystemState& state) const = 0;
    virtual Status accept(const FieldData& field) = 0;
    virtual std::vector<GeometryChange> geometryChanges(const SystemState&) const { return {}; }
    // External terms of the energy balance (input, boundary losses, dissipation, storage).
    // `transferred`: the quantities of this participant that went to another one as
    // energy (the engine records those as transfers): they are not dissipated here.
    virtual void account(EnergyLedger& ledger, const std::set<std::string>& transferred) const = 0;
    // Its own discretization's balance error, W or J (0 when exact).
    virtual double solverResidual() const { return 0.0; }
    // A few scalars per pass (monitored), and the summary of the final solution.
    virtual nlohmann::json monitor() const = 0;
    virtual nlohmann::json summary() const = 0;
    // Fields to keep in the result.
    virtual std::vector<std::string> resultQuantities() const { return {}; }
    // The geometry and material revisions its last solution was computed with.
    virtual std::uint64_t solvedGeometryRevision() const = 0;
    virtual std::uint64_t solvedMaterialRevision() const = 0;
};

enum class RunStatus { Converged, Completed, NotConverged, Failed, Cancelled };
std::string_view toString(RunStatus status);

struct PassRecord {
    int pass = 0;
    double maxTemperatureChange = 0.0;  // K
    double maxGeometryChange = 0.0;     // relative
    double transferredEnergy = 0.0;     // W or J: sum of energy transfers of the pass
    nlohmann::json monitored = nlohmann::json::object();
};

struct CoupledResult {
    RunStatus status = RunStatus::Completed;
    std::string message;
    std::string failedStage;
    int passes = 0;
    std::vector<PassRecord> history;
    nlohmann::json participants = nlohmann::json::array();
    EnergyLedger ledger;
    Audit audit;
    std::map<std::string, FieldData> fields;  // "participant.quantity"
    nlohmann::json geometry = nlohmann::json::object();
    nlohmann::json provenance = nlohmann::json::object();
    std::vector<std::string> warnings;

    bool ok() const { return status == RunStatus::Converged || status == RunStatus::Completed; }
    nlohmann::json toJson(bool values = true) const;
};

class Orchestrator {
public:
    Orchestrator(const PhysicalSystem& system, std::vector<std::unique_ptr<Participant>> participants,
                 std::vector<CouplingModelDescriptor> models);
    // The order passes solve the participants in (field transfers first-to-last).
    Result<std::vector<std::string>> executionOrder() const;
    CoupledResult run(const RunContext& context = {});
    const SystemState& state() const { return state_; }

private:
    Participant* participant(const std::string& id) const;
    const CouplingModelDescriptor* model(const std::string& id) const;

    PhysicalSystem system_;
    std::vector<std::unique_ptr<Participant>> participants_;
    std::vector<CouplingModelDescriptor> models_;
    SystemState state_;
};

}  // namespace resonant::multiphysics
