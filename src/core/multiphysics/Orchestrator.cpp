#include "multiphysics/Orchestrator.h"

#include "foundation/Version.h"
#include "multiphysics/Transfer.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace resonant::multiphysics {

std::string_view toString(RunStatus status)
{
    switch (status) {
    case RunStatus::Converged: return "converged";
    case RunStatus::Completed: return "completed";
    case RunStatus::NotConverged: return "not-converged";
    case RunStatus::Failed: return "failed";
    case RunStatus::Cancelled: return "cancelled";
    }
    return "failed";
}

nlohmann::json CoupledResult::toJson(bool values) const
{
    nlohmann::json j;
    j["format"] = "resonant.coupled-result";
    j["version"] = 1;
    j["status"] = std::string(toString(status));
    j["message"] = message;
    if (!failedStage.empty())
        j["failedStage"] = failedStage;
    j["passes"] = passes;
    j["history"] = nlohmann::json::array();
    for (const PassRecord& p : history)
        j["history"].push_back({{"pass", p.pass}, {"maxTemperatureChangeK", p.maxTemperatureChange}, {"maxGeometryChange", p.maxGeometryChange},
                                {"transferredEnergy", p.transferredEnergy}, {"monitored", p.monitored}});
    j["participants"] = participants;
    j["ledger"] = ledger.toJson();
    j["audit"] = audit.toJson();
    j["fields"] = nlohmann::json::object();
    for (const auto& [key, f] : fields) {
        nlohmann::json fj = f.summary();
        if (values) {
            fj["values"] = f.values;
            fj["edges"] = {f.support.edges[0], f.support.edges[1], f.support.edges[2]};
        }
        j["fields"][key] = fj;
    }
    j["geometry"] = geometry;
    j["provenance"] = provenance;
    j["warnings"] = warnings;
    return j;
}

Orchestrator::Orchestrator(const PhysicalSystem& system, std::vector<std::unique_ptr<Participant>> participants,
                           std::vector<CouplingModelDescriptor> models)
    : system_(system), participants_(std::move(participants)), models_(std::move(models))
{
    state_.system = &system_;
    state_.materials.setReferenceTemperature(system_.ambientTemperature);
    for (const PropertyLaw& l : system_.laws)
        state_.materials.addLaw(l);
    for (const RegionDef& r : system_.regions) {
        if (r.thickness > 0.0)
            state_.geometry.setReference(r.id, "thickness", r.thickness);
        for (const auto& [dimension, value] : r.dimensions)
            state_.geometry.setReference(r.id, dimension, value);
    }
}

Participant* Orchestrator::participant(const std::string& id) const
{
    for (const auto& p : participants_)
        if (p->id() == id)
            return p.get();
    return nullptr;
}

const CouplingModelDescriptor* Orchestrator::model(const std::string& id) const
{
    const auto it = std::find_if(models_.begin(), models_.end(), [&id](const CouplingModelDescriptor& m) { return m.id == id; });
    return it == models_.end() ? nullptr : &*it;
}

Result<std::vector<std::string>> Orchestrator::executionOrder() const
{
    // Kahn's algorithm over the field transfers; ties keep the system's domain order.
    std::vector<std::string> ids;
    for (const auto& p : participants_)
        ids.push_back(p->id());
    std::map<std::string, int> indegree;
    std::multimap<std::string, std::string> edges;
    for (const std::string& id : ids)
        indegree[id] = 0;
    for (const CouplingDef& c : system_.couplings) {
        const CouplingModelDescriptor* m = model(c.model);
        if (!m || m->kind != "field-transfer" || !indegree.contains(c.from) || !indegree.contains(c.to))
            continue;
        edges.emplace(c.from, c.to);
        ++indegree[c.to];
    }
    std::vector<std::string> order;
    std::set<std::string> done;
    while (order.size() < ids.size()) {
        bool progressed = false;
        for (const std::string& id : ids) {
            if (done.contains(id) || indegree[id] != 0)
                continue;
            order.push_back(id);
            done.insert(id);
            const auto [lo, hi] = edges.equal_range(id);
            for (auto it = lo; it != hi; ++it)
                --indegree[it->second];
            progressed = true;
            break;
        }
        if (!progressed)
            return Result<std::vector<std::string>>::failure(
                "the field transfers form a cycle: make one direction of the loop a state or geometry update");
    }
    return order;
}

CoupledResult Orchestrator::run(const RunContext& context)
{
    CoupledResult result;
    const SchemeDef& scheme = system_.scheme;
    result.ledger = EnergyLedger(BalanceMode::SteadyPower);
    for (const DomainDef& d : system_.domains)
        if (d.kind == "thermal" && d.settings.contains("transient"))
            result.ledger = EnergyLedger(BalanceMode::EnergyOverInterval, d.settings["transient"].value("duration", 0.0));
    const auto fail = [&result](RunStatus status, std::string stage, std::string message) {
        result.status = status;
        result.failedStage = std::move(stage);
        result.message = std::move(message);
    };
    const auto cancelled = [&context] { return context.cancelled && context.cancelled(); };
    const auto progress = [&context](double f, const std::string& m) {
        if (context.progress)
            context.progress(f, m);
    };

    auto order = executionOrder();
    if (!order) {
        fail(RunStatus::Failed, "order", order.message());
        return result;
    }
    for (const auto& p : participants_)
        if (Status s = p->prepare(state_); !s) {
            fail(RunStatus::Failed, "prepare " + p->id(), s.message());
            return result;
        }
    const int maxPasses = scheme.kind == "one-way" ? 1 : scheme.kind == "sequential" ? std::max(1, scheme.passes) : std::max(2, scheme.maxIterations);
    const double interval = [this]() {
        for (const DomainDef& d : system_.domains)
            if (d.kind == "thermal" && d.settings.contains("transient"))
                return d.settings["transient"].value("duration", 0.0);
        return 0.0;
    }();
    bool converged = false;
    double previousTransfer = std::nan("");
    double couplingError = 0.0;
    for (int pass = 1; pass <= maxPasses && !converged; ++pass) {
        if (cancelled()) {
            fail(RunStatus::Cancelled, "pass " + std::to_string(pass), "cancelled");
            break;
        }
        result.ledger.clear();
        PassRecord record;
        record.pass = pass;
        bool stop = false;
        for (size_t step = 0; step < order.value().size() && !stop; ++step) {
            Participant* p = participant(order.value()[step]);
            progress((pass - 1 + static_cast<double>(step) / static_cast<double>(order.value().size())) / maxPasses,
                     "pass " + std::to_string(pass) + ": " + p->id());
            // Before every solve (Gauss-Seidel): a participant sees the state and geometry updates made
            // earlier in this pass; it skips the work when the revisions it used are unchanged.
            if (Status s = p->prepare(state_); !s) {
                fail(RunStatus::Failed, "prepare " + p->id() + " (pass " + std::to_string(pass) + ")", s.message());
                stop = true;
                break;
            }
            if (cancelled()) {
                fail(RunStatus::Cancelled, p->id() + " (pass " + std::to_string(pass) + ")", "cancelled");
                stop = true;
                break;
            }
            if (Status s = p->solve(state_, context); !s) {
                fail(s.message() == "cancelled" ? RunStatus::Cancelled : RunStatus::Failed, "solve " + p->id() + " (pass " + std::to_string(pass) + ")",
                     s.message());
                stop = true;
                break;
            }
            for (const CouplingDef& c : system_.couplings) {
                if (c.from != p->id())
                    continue;
                const CouplingModelDescriptor* m = model(c.model);
                Participant* to = participant(c.to);
                const std::string stage = "coupling " + c.id + " (pass " + std::to_string(pass) + ")";
                if (!m || !to) {
                    fail(RunStatus::Failed, stage, "the coupling's model or target participant is missing");
                    stop = true;
                    break;
                }
                auto field = p->provide(m->sourceQuantity, state_);
                if (!field) {
                    fail(RunStatus::Failed, stage, p->id() + " does not provide " + m->sourceQuantity);
                    stop = true;
                    break;
                }
                if (Status s = field->validate(); !s) {
                    fail(RunStatus::Failed, stage, "invalid values, not passed on: " + s.message());
                    stop = true;
                    break;
                }
                if (m->kind == "field-transfer") {
                    FieldData source = *field;
                    if (m->targetQuantity != m->sourceQuantity) {
                        const QuantityDescriptor* target = findQuantity(m->targetQuantity);
                        if (!target) {
                            fail(RunStatus::Failed, stage, "unknown target quantity " + m->targetQuantity);
                            stop = true;
                            break;
                        }
                        source.quantity = *target;  // the conversion this model declares
                    }
                    const auto method = transferMethodFromString(c.transfer.empty() ? m->defaultTransfer : c.transfer).value_or(TransferMethod::Interpolating);
                    TransferReport report;
                    auto moved = transfer(source, to->support(state_), method, &report);
                    if (!moved) {
                        fail(RunStatus::Failed, stage, moved.message());
                        stop = true;
                        break;
                    }
                    if (Status s = to->accept(moved.value()); !s) {
                        fail(RunStatus::Failed, stage, to->id() + " refused " + m->targetQuantity + ": " + s.message());
                        stop = true;
                        break;
                    }
                    const bool energy = source.quantity.conservative && source.quantity.unit == "W/m^3";
                    const double scale = result.ledger.mode() == BalanceMode::EnergyOverInterval ? interval : 1.0;
                    TransferRecord t;
                    t.coupling = c.id;
                    t.model = m->id;
                    t.sourceParticipant = p->id();
                    t.sourceDomain = p->domain();
                    t.destinationParticipant = to->id();
                    t.destinationDomain = to->domain();
                    t.mechanism = m->mechanism;
                    t.sourceQuantity = m->sourceQuantity;
                    t.targetQuantity = m->targetQuantity;
                    t.unit = energy ? (scale == 1.0 ? "W" : "J") : source.quantity.unit;
                    t.region = "all regions";
                    t.interval = scale == 1.0 ? "steady" : std::to_string(interval) + " s";
                    t.method = std::string(toString(method));
                    t.sourceIntegral = report.sourceIntegral * (energy ? scale : 1.0);
                    t.targetIntegral = report.targetIntegral * (energy ? scale : 1.0);
                    t.amount = t.targetIntegral;
                    t.relativeDifference = report.relativeDifference;
                    t.approximation = report.description;
                    t.limitation = m->limitations;
                    t.energy = energy;
                    if (energy) {
                        result.ledger.add(LedgerTerm{p->id(), p->domain(), TermKind::TransferOut, m->mechanism, t.sourceIntegral, "all regions",
                                                     "to " + to->id() + " (" + c.id + ")"});
                        result.ledger.add(LedgerTerm{to->id(), to->domain(), TermKind::TransferIn, m->mechanism, t.targetIntegral, "all regions",
                                                     "from " + p->id() + " (" + c.id + ")"});
                        record.transferredEnergy += t.targetIntegral;
                    }
                    result.ledger.add(std::move(t));
                } else if (m->kind == "state-update") {
                    auto change = state_.materials.updateTemperature(*field, scheme.relaxation);
                    if (!change) {
                        fail(RunStatus::Failed, stage, change.message());
                        stop = true;
                        break;
                    }
                    record.maxTemperatureChange = std::max(record.maxTemperatureChange, change.value());
                } else if (m->kind == "geometry-update") {
                    auto change = state_.geometry.apply(p->geometryChanges(state_), scheme.relaxation);
                    if (!change) {
                        fail(RunStatus::Failed, stage, change.message());
                        stop = true;
                        break;
                    }
                    record.maxGeometryChange = std::max(record.maxGeometryChange, change.value());
                }
            }
        }
        for (const auto& p : participants_)
            record.monitored[p->id()] = p->monitor();
        result.history.push_back(record);
        result.passes = pass;
        if (stop)
            break;
        if (!std::isnan(previousTransfer))
            couplingError = record.transferredEnergy - previousTransfer;
        previousTransfer = record.transferredEnergy;
        if (scheme.kind == "iterative" && pass >= 2 && record.maxTemperatureChange <= scheme.temperatureTolerance &&
            record.maxGeometryChange <= scheme.geometryTolerance)
            converged = true;
    }
    if (result.failedStage.empty()) {
        if (scheme.kind == "iterative") {
            result.status = converged ? RunStatus::Converged : RunStatus::NotConverged;
            result.message = converged ? "converged in " + std::to_string(result.passes) + " passes"
                                       : "not converged in " + std::to_string(result.passes) + " passes: the last state change was " +
                                             std::to_string(result.history.back().maxTemperatureChange) + " K";
        } else {
            result.status = RunStatus::Completed;
            result.message = "completed in " + std::to_string(result.passes) + (result.passes == 1 ? " pass" : " passes");
        }
    }
    // Accounting of the last pass (also for failed runs: what was solved).
    std::map<std::string, std::set<std::string>> transferred;
    for (const TransferRecord& t : result.ledger.transfers())
        if (t.energy)
            transferred[t.sourceParticipant].insert(t.sourceQuantity);
    for (const auto& p : participants_) {
        p->account(result.ledger, transferred[p->id()]);
        result.ledger.setSolverResidual(p->id(), p->solverResidual());
    }
    result.audit = result.ledger.audit(scheme.balanceTolerance, couplingError);
    for (const auto& p : participants_) {
        nlohmann::json s = p->summary();
        s["id"] = p->id();
        s["domain"] = p->domain();
        s["solver"] = p->solverId();
        s["solverVersion"] = p->solverVersion();
        s["geometryRevision"] = p->solvedGeometryRevision();
        s["materialRevision"] = p->solvedMaterialRevision();
        const bool staleGeometry = p->solvedGeometryRevision() != state_.geometry.revision();
        const bool staleMaterial = p->solvedMaterialRevision() != state_.materials.revision();
        s["stale"] = staleGeometry || staleMaterial;
        if ((staleGeometry || staleMaterial) && result.ok())
            result.warnings.push_back(p->id() + " was last solved with the " + (staleGeometry ? "geometry" : "material state") +
                                      " of an earlier update; the change since is below the tolerances of the scheme" +
                                      (scheme.kind == "iterative" ? "" : " only if a further pass would not change it (not checked by a " + scheme.kind + " scheme)"));
        result.participants.push_back(s);
        for (const std::string& q : p->resultQuantities())
            if (auto f = p->provide(q, state_))
                result.fields[p->id() + "." + q] = *f;
    }
    for (const std::string& w : state_.materials.warnings())
        result.warnings.push_back(w);
    result.geometry = state_.geometry.toJson();
    result.provenance = {{"system", system_.toJson()},
                         {"systemChecksum", system_.checksum()},
                         {"scheme", system_.toJson()["scheme"]},
                         {"materialRevision", state_.materials.revision()},
                         {"geometryRevision", state_.geometry.revision()},
                         {"application", std::string(kApplicationName) + " " + kApplicationVersion},
                         {"engine", "resonant.multiphysics 1.0"}};
    return result;
}

}  // namespace resonant::multiphysics
