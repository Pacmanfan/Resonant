#include "coupling/StateEvolution.h"

#include "coupling/CoupledRun.h"
#include "foundation/Time.h"
#include "simulation/Job.h"

#include <cmath>

namespace resonant::coupling {

using nlohmann::json;
using multiphysics::PhysicalSystem;

Status EvolutionSettings::validate() const
{
    if (!(duration > 0.0))
        return Status::failure("the duration must be positive");
    if (steps < 1 || steps > 2000 || substeps < 1 || substeps > 200)
        return Status::failure("1..2000 steps and 1..200 substeps");
    return Status::success();
}

Result<json> evolveSystem(const PhysicalSystem& system, const MaterialLibrary& materials, const EvolutionSettings& s, JobContext* ctx)
{
    using R = Result<json>;
    if (Status v = s.validate(); !v)
        return R::failure(v.message());
    if (system.geometryKind != "layered-slab")
        return R::failure("time-stepped evolution needs a layered-slab system");
    int thermal = -1;
    bool mechanical = false;
    for (size_t i = 0; i < system.domains.size(); ++i) {
        if (system.domains[i].kind == "thermal" && system.domains[i].solver == "thermal.conduction.fv")
            thermal = static_cast<int>(i);
        mechanical = mechanical || system.domains[i].kind == "mechanical";
    }
    if (thermal < 0)
        return R::failure("time-stepped evolution needs a thermal domain (thermal.conduction.fv): its state carries the run from step to step");
    const std::string tid = system.domains[static_cast<size_t>(thermal)].id;
    const double dt = s.duration / s.steps;
    // The cells' z centres and their regions (reference geometry).
    std::vector<double> z;
    std::vector<std::string> region;
    {
        double z0 = 0.0;
        for (const auto& r : system.regions) {
            for (int c = 0; c < r.cells; ++c) {
                z.push_back(z0 + (c + 0.5) * r.thickness / r.cells);
                region.push_back(r.id);
            }
            z0 += r.thickness;
        }
    }
    json history = json::array();
    json previous;  // temperature field
    double input = 0.0, out = 0.0, stored = 0.0, dissipated = 0.0, worstResidual = 0.0;
    bool allBalanced = true, completed = true;
    std::string failure;
    for (int k = 1; k <= s.steps; ++k) {
        if (ctx && ctx->cancelRequested())
            return R::failure("cancelled");
        PhysicalSystem step = system;
        json transient{{"duration", dt}, {"steps", s.substeps}, {"snapshots", 1}};
        if (!previous.is_null())
            transient["initialTemperature"] = previous;
        step.domains[static_cast<size_t>(thermal)].settings["transient"] = transient;
        auto run = runPhysicalSystem(step, materials);
        if (!run || !run.value().ok()) {
            completed = false;
            failure = "step " + std::to_string(k) + " did not complete: " + (run ? run.value().message : run.message()) +
                      " (the state of step " + std::to_string(k - 1) + " is kept)";
            history.push_back({{"step", k}, {"time", k * dt}, {"convergence", {{"converged", false}, {"message", failure}}}});
            break;
        }
        const auto& r = run.value();
        const auto field = r.fields.find(tid + ".temperature");
        if (field == r.fields.end())
            return R::failure("the thermal domain provided no temperature field");
        const std::vector<double>& T = field->second.values;
        previous = T;
        json regions = json::object();
        for (const auto& reg : system.regions) {
            double sum = 0.0, maxT = -1e300;
            int n = 0;
            for (size_t c = 0; c < T.size() && c < region.size(); ++c)
                if (region[c] == reg.id) {
                    sum += T[c];
                    maxT = std::max(maxT, T[c]);
                    ++n;
                }
            const double mean = n ? sum / n : 0.0;
            json laws = json::object();
            for (const auto& law : system.laws)
                if (law.region == reg.id) {
                    const auto v = multiphysics::evaluate(law, mean);
                    laws[law.property] = {{"value", v.value}, {"extrapolated", v.extrapolated}};
                }
            json thickness;
            if (r.geometry.contains("regions") && r.geometry["regions"].contains(reg.id) && r.geometry["regions"][reg.id].contains("thickness"))
                thickness = r.geometry["regions"][reg.id]["thickness"].value("current", reg.thickness);
            regions[reg.id] = {{"meanTemperatureK", mean}, {"maxTemperatureK", maxT}, {"properties", laws}, {"thickness", thickness}};
        }
        json snapshot{{"step", k},
                      {"time", k * dt},
                      {"temperature", T},
                      {"regions", regions},
                      {"energy", r.audit.toJson()},
                      {"ledger", r.ledger.toJson()},
                      {"convergence", {{"converged", true}, {"passes", r.passes}, {"status", r.status == multiphysics::RunStatus::Converged ? "converged" : "completed"}}}};
        if (mechanical)
            for (const auto& [name, f] : r.fields)
                if (name.size() > 7 && name.compare(name.size() - 7, 7, ".stress") == 0) {
                    std::vector<double> sxx;
                    for (size_t e = 0; e * 6 < f.values.size(); ++e)
                        sxx.push_back(f.values[e * 6]);
                    snapshot["stressXX"] = sxx;
                }
        history.push_back(std::move(snapshot));
        input += r.audit.externalInput;
        out += r.audit.boundaryOut;
        stored += r.audit.stored;
        dissipated += r.audit.dissipated;
        worstResidual = std::max(worstResidual, std::abs(r.audit.relativeResidual));
        allBalanced = allBalanced && r.audit.balanced;
        if (ctx)
            ctx->reportProgress(static_cast<double>(k) / s.steps, "step " + std::to_string(k));
    }
    json committable = json::array();
    if (!history.empty() && history.back().contains("regions"))
        for (const auto& [id, reg] : history.back()["regions"].items())
            for (const auto& [property, v] : reg["properties"].items())
                for (const auto& r : system.regions)
                    if (r.id == id)
                        committable.push_back({{"domain", property.substr(0, property.find('.'))},
                                               {"key", property.substr(property.find('.') + 1)},
                                               {"material", r.materialId},
                                               {"value", v["value"]},
                                               {"note", "the region '" + id + "' at its final mean temperature " +
                                                            std::to_string(reg["meanTemperatureK"].get<double>()).substr(0, 7) + " K"}});
    json reliability = json::array();
    if (!completed)
        reliability.push_back(failure);
    if (!allBalanced)
        reliability.push_back("a step's energy audit is not balanced (largest relative residual " + std::to_string(worstResidual) + ")");
    return json{{"format", "resonant.material-state-evolution"},
                {"version", 1},
                {"kind", "evolution"},
                {"system", system.id},
                {"created", utcNowIso8601()},
                {"settings", {{"duration", s.duration}, {"steps", s.steps}, {"substeps", s.substeps}, {"timeStep", dt}}},
                {"positions", z},
                {"cellRegions", region},
                {"history", history},
                {"energy", {{"externalInput", input}, {"boundaryOut", out}, {"stored", stored}, {"dissipated", dissipated},
                            {"residual", input - out - stored - dissipated}, {"largestStepResidual", worstResidual}}},
                {"completed", completed},
                {"committable", committable},
                {"reliability", reliability},
                {"reliable", reliability.empty()}};
}

}  // namespace resonant::coupling
