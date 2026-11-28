#pragma once

// Running a physical system (RES-CEME-006, RES-CEME-014): validation against
// the registered coupling models and participants, building the participants,
// the orchestrator, and a job of the unified job system.
#include "materials/Material.h"
#include "multiphysics/Orchestrator.h"
#include "simulation/Job.h"

#include <memory>
#include <mutex>

namespace resonant::coupling {

std::vector<multiphysics::SystemIssue> validatePhysicalSystem(const multiphysics::PhysicalSystem& system, const MaterialLibrary& materials);

// Fails (with every error) when the system does not validate; otherwise the
// result, whatever its status (converged, not converged, failed, cancelled).
Result<multiphysics::CoupledResult> runPhysicalSystem(const multiphysics::PhysicalSystem& system, const MaterialLibrary& materials,
                                                      const multiphysics::RunContext& context = {});

class CoupledSimulationJob final : public Job {
public:
    CoupledSimulationJob(multiphysics::PhysicalSystem system, MaterialLibrary materials);
    std::string name() const override;
    JobEnd run(JobContext& context) override;
    // After the job ended: the result (also for failed and cancelled runs that got that far).
    std::shared_ptr<const multiphysics::CoupledResult> result() const;
    const multiphysics::PhysicalSystem& system() const { return system_; }

private:
    multiphysics::PhysicalSystem system_;
    MaterialLibrary materials_;
    mutable std::mutex mutex_;
    std::shared_ptr<const multiphysics::CoupledResult> result_;
};

}  // namespace resonant::coupling
