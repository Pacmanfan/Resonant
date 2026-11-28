#include "coupling/CoupledRun.h"

#include "coupling/CouplingModels.h"
#include "coupling/MultiphysicsParticipants.h"

namespace resonant::coupling {

std::vector<multiphysics::SystemIssue> validatePhysicalSystem(const multiphysics::PhysicalSystem& system, const MaterialLibrary& materials)
{
    return multiphysics::validateSystem(system, couplingModels(), [&materials](const std::string& id) { return materials.find(Id(id)) != nullptr; },
                                        participantSolvers());
}

Result<multiphysics::CoupledResult> runPhysicalSystem(const multiphysics::PhysicalSystem& system, const MaterialLibrary& materials,
                                                      const multiphysics::RunContext& context)
{
    using R = Result<multiphysics::CoupledResult>;
    std::string errors;
    for (const auto& issue : validatePhysicalSystem(system, materials))
        if (issue.severity >= 3)
            errors += (errors.empty() ? "" : "; ") + issue.field + ": " + issue.message;
    if (!errors.empty())
        return R::failure("the physical system is not valid: " + errors);
    std::vector<std::unique_ptr<multiphysics::Participant>> participants;
    for (const auto& d : system.domains) {
        auto p = makeParticipant(system, d, materials);
        if (!p)
            return R::failure(p.message());
        participants.push_back(p.take());
    }
    multiphysics::Orchestrator orchestrator(system, std::move(participants), couplingModels());
    multiphysics::CoupledResult result = orchestrator.run(context);
    for (const auto& issue : validatePhysicalSystem(system, materials))
        if (issue.severity == 2)
            result.warnings.push_back(issue.field + ": " + issue.message);
    return result;
}

CoupledSimulationJob::CoupledSimulationJob(multiphysics::PhysicalSystem system, MaterialLibrary materials)
    : system_(std::move(system)), materials_(std::move(materials))
{
}

std::string CoupledSimulationJob::name() const
{
    return "Coupled simulation: " + (system_.name.empty() ? system_.id : system_.name);
}

JobEnd CoupledSimulationJob::run(JobContext& context)
{
    multiphysics::RunContext rc;
    rc.cancelled = [&context] { return context.cancelRequested(); };
    rc.progress = [&context](double f, const std::string& m) { context.reportProgress(f, m); };
    auto r = runPhysicalSystem(system_, materials_, rc);
    if (!r)
        return JobEnd::failed(r.message());
    auto shared = std::make_shared<const multiphysics::CoupledResult>(r.take());
    {
        std::lock_guard lock(mutex_);
        result_ = shared;
    }
    switch (shared->status) {
    case multiphysics::RunStatus::Cancelled: return JobEnd::cancelled();
    case multiphysics::RunStatus::Failed: return JobEnd::failed(shared->failedStage + ": " + shared->message);
    default: return JobEnd::completed(shared->message);
    }
}

std::shared_ptr<const multiphysics::CoupledResult> CoupledSimulationJob::result() const
{
    std::lock_guard lock(mutex_);
    return result_;
}

}  // namespace resonant::coupling
