#include "foundation/Types.h"
#include "geometry/Geometry.h"
#include "optimization/Algorithms.h"
#include "project/Project.h"
#include "services/Services.h"

#include <fstream>
#include <unistd.h>

namespace resonant::services {

Outcome<std::vector<CapabilityView>> ApplicationService::capabilities(const CallContext& c, const std::string& category)
{
    auto a = s_.authorize(c, Permission::Read);
    if (!a)
        return a.error();
    std::vector<CapabilityView> out;
    const auto add = [&](CapabilityView v) {
        if (category.empty() || v.category == category)
            out.push_back(std::move(v));
    };
    // Physics domains (what the application implements; the API exposes part of each: see the solvers).
    add({"domain.acoustic", "Acoustics", "Sources, arrays, focusing, reflectors, field analysis, full-wave FDTD", "domain", "stable", true, {}, {"2D", "3D"}, {}, "acoustic"});
    add({"domain.mechanical", "Mechanics", "Structures, modal / harmonic / transient / static analyses, waves", "domain", "stable", true, {}, {"3D"},
         {"through the automation API: planned (the Mechanics panel and Python)"}, "mechanical"});
    add({"domain.electromagnetic", "Electromagnetics", "Antennas, arrays, patterns, lines, guides, Yee FDTD", "domain", "stable", true, {}, {"3D"},
         {"through the automation API: planned (the Electromagnetics panel and Python)"}, "electromagnetic"});
    // Solvers.
    if (auto solvers = s_.services().simulation.solvers(a.value()))
        for (const auto& sv : solvers.value()) {
            CapabilityView v{"solver." + sv.id, sv.name, sv.method, "solver", sv.startable ? "stable" : "stable", sv.available, {}, {}, {}, sv.domain};
            if (!sv.startable)
                v.limitations.push_back("not yet startable through the automation API (available in the application)");
            add(std::move(v));
        }
#ifdef RESONANT_HAVE_CUDA
    add({"backend.cuda", "CUDA backend of the full-wave solvers", "GPU execution, chosen explicitly per run", "solver", "experimental", true, {"CUDA"}, {}, {"not validated on a GPU in this build's tests"}, ""});
#else
    add({"backend.cuda", "CUDA backend of the full-wave solvers", "GPU execution", "solver", "unavailable", false, {"CUDA toolkit at build time"}, {}, {}, ""});
#endif
    // Geometry kinds.
    for (const GeometryKind* k : GeometryRegistry::instance().kinds())
        add({"geometry." + k->id, k->displayName, "geometry kind (" + k->category + ")", "geometry", "stable", true, {}, {"3D"}, {}, ""});
    // Material models (the physics domains materials carry properties for).
    for (PhysicsDomain d : {PhysicsDomain::Acoustic, PhysicsDomain::Mechanical, PhysicsDomain::Electromagnetic})
        add({"material-model." + std::string(toString(d)), std::string(toString(d)) + " material properties", "isotropic properties with provenance",
             "material-model", "stable", true, {}, {}, {}, std::string(toString(d))});
    // Optimization algorithms.
    for (const optimization::Algorithm* algorithm : optimization::algorithms()) {
        const auto info = algorithm->info();
        add({"optimization." + info.id, info.name, info.description, "optimization", "stable", true, {}, {}, {}, ""});
    }
    // File formats.
    add({"file-format.resonant", "Resonant project (JSON)", "open, save, export", "file-format", "stable", true, {}, {}, {}, ""});
    add({"file-format.stl", "STL mesh export", "candidate geometry export (Optimization Studio)", "file-format", "stable", true, {}, {},
         {"through the automation API: planned"}, ""});
    add({"file-format.touchstone", "Touchstone S-parameters", "import of measured networks", "file-format", "stable", true, {}, {},
         {"through the automation API: planned"}, "electromagnetic"});
    // Optional dependencies.
#ifdef RESONANT_HAVE_HDF5
    add({"dependency.hdf5", "HDF5", "measurement data next to the project", "dependency", "stable", true, {"HDF5"}, {}, {}, ""});
#else
    add({"dependency.hdf5", "HDF5", "measurement data next to the project", "dependency", "unavailable", false, {"HDF5 at build time"}, {}, {}, ""});
#endif
#ifdef RESONANT_HAVE_VOLK
    add({"dependency.volk", "VOLK", "SIMD kernels", "dependency", "stable", true, {"VOLK"}, {}, {}, ""});
#else
    add({"dependency.volk", "VOLK", "SIMD kernels", "dependency", "unavailable", false, {"VOLK at build time"}, {}, {}, ""});
#endif
    // Application features.
    add({"feature.workflows", "Workflow automation", "resonant.workflow templates run as jobs", "service", "stable", true, {}, {}, {}, ""});
    add({"feature.transactions", "Batches and transactions", "atomic, validated, one undo step", "service", "stable", true, {}, {}, {"only project changes (no files, jobs or hardware)"}, ""});
    add({"feature.hardware-outputs", "Physical outputs", "experiments that drive hardware", "service", "unavailable", false, {}, {},
         {"never through the automation API: outputs need an operator's authorization in the Experiments panel"}, ""});
    return out;
}

Outcome<CapabilityView> ApplicationService::capability(const CallContext& c, const std::string& id)
{
    auto all = capabilities(c, {});
    if (!all)
        return all.error();
    for (const CapabilityView& v : all.value())
        if (v.id == id)
            return v;
    return notFound("no capability '" + id + "'", "capability", id);
}

Outcome<DiagnosticsView> ApplicationService::diagnostics(const CallContext& c)
{
    auto a = s_.authorize(c, Permission::Read);
    if (!a)
        return a.error();
    DiagnosticsView d;
    d.activeSessions = static_cast<int>(s_.sessions().size());
    for (const JobInfo& info : s_.host().jobs().jobs())
        if (!isFinished(info.state))
            ++d.activeJobs;
    d.lastEventSequence = s_.events().lastSequence();
    d.oldestEventSequence = s_.events().oldestRetained();
    d.auditRecords = static_cast<int>(s_.auditSize());
    d.recentErrors = s_.recentErrors();
    std::ifstream statm("/proc/self/statm");
    std::uint64_t size = 0, resident = 0;
    if (statm >> size >> resident)
        d.residentMemoryBytes = resident * static_cast<std::uint64_t>(sysconf(_SC_PAGESIZE));
    d.uptimeSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - s_.started()).count();
    return d;
}

Outcome<std::vector<AuditRecord>> ApplicationService::audit(const CallContext& c, const AuditQuery& query)
{
    auto a = s_.authorize(c, Permission::Read);
    if (!a)
        return a.error();
    AuditQuery q = query;
    // Another session's records need the admin permission.
    if (!has(a.value().permissions, Permission::Admin))
        q.sessionId = a.value().sessionId;
    return s_.audit(q);
}

}  // namespace resonant::services
