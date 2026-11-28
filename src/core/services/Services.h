// The application services (REQ-CHAT-20261126-012, ADR-000216): the one
// implementation of each meaningful operation, used by the GUI, in-process
// Python, the protobuf/gRPC adapter and future MCP and web adapters. They take
// a CallContext (who, with which permissions, dry run, expected revisions,
// idempotency key), return structured errors, change the project only through
// undoable commands (Session::mutate) and never depend on Qt, a transport or
// generated protobuf types.
#pragma once

#include "acoustics/FieldGrid.h"
#include "foundation/Transform.h"
#include "materials/Material.h"
#include "multiphysics/PhysicalSystem.h"
#include "services/Session.h"

#include <nlohmann/json.hpp>

#include <complex>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace resonant {
class SceneObject;
}

namespace resonant::services {

// "group", "geometry", or the component type id ("acoustic.array", ...).
std::string objectTypeOf(const SceneObject& object);

// --- read views -------------------------------------------------------------------

struct ProjectView {
    std::string id, name, fileName;
    std::uint64_t revision = 0;
    bool modified = false;
    int objectCount = 0, materialCount = 0;
    bool canUndo = false, canRedo = false;
    std::string undoText, redoText, createdUtc, modifiedUtc;
};

struct ObjectView {
    Id id;
    std::string name, type;
    Id parent;
    std::vector<Id> children;
    std::uint64_t revision = 0;
    Transform local;
    Vec3 worldPosition = Vec3::Zero();
    bool visible = true;
    Id material;
    std::string geometryKind;  // empty: no geometry
    std::map<std::string, double> parameters;
    nlohmann::json component;  // null: no component
};

struct SceneSnapshotView {
    std::string projectId;
    std::uint64_t revision = 0, eventSequence = 0;
    std::vector<ObjectView> objects;
};

struct ValidationIssueView {
    int severity = 3;  // 1 info, 2 warning, 3 error
    std::string field, message, resourceId;
};
struct ValidationView {
    bool valid = true;
    std::vector<ValidationIssueView> issues;
};

struct JobView {
    std::string id, type, name, phase, sessionId;
    std::string state;  // created, queued, running, cancelling, cancelled, completed, failed
    double progress = 0.0, elapsedSeconds = 0.0;
    std::optional<ServiceError> error;
    std::vector<std::string> resultIds;
    bool cancellable = false;
};

struct CapabilityView {
    std::string id, displayName, description, category, maturity;  // maturity: stable, experimental, planned, unavailable
    bool available = false;
    std::vector<std::string> dependencies, dimensions, limitations;
    std::string domain;
};

struct DiagnosticsView {
    std::string health = "ok";
    int activeSessions = 0, activeJobs = 0;
    std::uint64_t lastEventSequence = 0, oldestEventSequence = 0;
    int auditRecords = 0;
    std::vector<ServiceError> recentErrors;
    std::uint64_t residentMemoryBytes = 0;
    double uptimeSeconds = 0.0;
};

struct FieldSlice {
    int rows = 0, columns = 0;
    std::vector<std::complex<float>> samples;  // row-major
};

struct FieldPointSample {
    Vec3 point = Vec3::Zero();
    bool inside = false;
    std::complex<float> value;
};

// --- services --------------------------------------------------------------------------

class ProjectService {
public:
    explicit ProjectService(Session& s) : s_(s) {}
    Outcome<ProjectView> get(const CallContext& c);
    Outcome<ProjectView> create(const CallContext& c, const std::string& name, bool discardUnsaved);
    // In-process callers (GUI, Python) may give any path; remote clients a name
    // relative to the project directory (no "..", no absolute path).
    Outcome<ProjectView> open(const CallContext& c, const std::string& file, bool discardUnsaved, std::vector<std::string>* warnings = nullptr);
    Outcome<ProjectView> save(const CallContext& c, const std::string& file = {});
    Outcome<ValidationView> validate(const CallContext& c);
    Outcome<std::string> undo(const CallContext& c) { return s_.undo(c); }
    Outcome<std::string> redo(const CallContext& c) { return s_.redo(c); }
    Outcome<std::string> exportJson(const CallContext& c);
    // The directory remote file names resolve against (empty: remote file access is refused).
    void setProjectDirectory(std::string directory) { directory_ = std::move(directory); }
    const std::string& projectDirectory() const { return directory_; }
    ProjectView view() const;

private:
    Outcome<std::string> resolve(const CallContext& c, const std::string& file) const;
    Session& s_;
    std::string directory_;
};

class SceneService {
public:
    explicit SceneService(Session& s) : s_(s) {}
    Outcome<SceneSnapshotView> snapshot(const CallContext& c);
    Outcome<std::vector<ObjectView>> list(const CallContext& c, const std::string& type, const std::string& parent);
    Outcome<ObjectView> object(const CallContext& c, const Id& id);
    struct Update {
        std::optional<std::string> name;
        std::optional<Transform> transform;
        std::optional<bool> visible;
        std::optional<std::string> parent;  // "" or "root": top level
        std::optional<std::string> description;  // the undo step's name (default "Rename X" / "Edit X")
    };
    Outcome<Mutation> update(const CallContext& c, const Id& id, std::optional<std::uint64_t> expectedRevision, const Update& update);
    Outcome<Mutation> remove(const CallContext& c, const std::vector<Id>& ids, const std::map<Id, std::uint64_t>& expectedRevisions = {});
    Outcome<Mutation> duplicate(const CallContext& c, const std::vector<Id>& ids);
    Outcome<Mutation> createGroup(const CallContext& c, const std::string& name, const Id& parent, const std::vector<Id>& members);
    Outcome<ValidationView> validate(const CallContext& c, const Id& id);
    ObjectView view(const SceneObject& object) const;

private:
    Session& s_;
};

class GeometryService {
public:
    explicit GeometryService(Session& s) : s_(s) {}
    struct Kind {
        std::string id, name, category;
        struct Parameter {
            std::string key, label, unit;
            double defaultValue = 0, minValue = 0, maxValue = 0;
            bool integer = false;
        };
        std::vector<Parameter> parameters;
    };
    Outcome<std::vector<Kind>> kinds(const CallContext& c);
    struct Create {
        std::string kind, name;
        std::map<std::string, double> parameters;
        std::optional<Transform> transform;
        Id parent, material;
    };
    Outcome<Mutation> createPrimitive(const CallContext& c, const Create& create);
    Outcome<Mutation> updateParameters(const CallContext& c, const Id& id, std::optional<std::uint64_t> expectedRevision,
                                       const std::map<std::string, double>& parameters);
    Outcome<ValidationView> validate(const CallContext& c, const std::string& kind, const std::map<std::string, double>& parameters);

private:
    Session& s_;
};

class MaterialService {
public:
    explicit MaterialService(Session& s) : s_(s) {}
    Outcome<std::vector<nlohmann::json>> list(const CallContext& c, const std::string& domain);  // project-file material JSON
    Outcome<nlohmann::json> get(const CallContext& c, const Id& id);
    Outcome<Mutation> create(const CallContext& c, const nlohmann::json& material);
    Outcome<Mutation> remove(const CallContext& c, const Id& id);
    Outcome<Mutation> assign(const CallContext& c, const Id& material, const std::vector<Id>& objects);
    Outcome<Mutation> unassign(const CallContext& c, const std::vector<Id>& objects);
    // The whole library at once (the GUI's material dialog).
    Outcome<Mutation> replaceLibrary(const CallContext& c, const MaterialLibrary& library, const std::string& description);

private:
    Session& s_;
};

// Emitters and arrays: component data is exchanged in the project file's JSON
// form (EmitterSettings / ArraySettings keys) and validated by the component.
class EmitterService {
public:
    explicit EmitterService(Session& s) : s_(s) {}
    Outcome<Mutation> create(const CallContext& c, const std::string& name, std::optional<Transform> transform, const Id& parent,
                             const nlohmann::json& settings);
    Outcome<ObjectView> get(const CallContext& c, const Id& id);
    Outcome<std::vector<ObjectView>> list(const CallContext& c);
    Outcome<Mutation> update(const CallContext& c, const Id& id, std::optional<std::uint64_t> expectedRevision, const nlohmann::json& settingsPatch,
                             std::optional<Transform> transform);

private:
    Session& s_;
};

class ArrayService {
public:
    explicit ArrayService(Session& s) : s_(s) {}
    Outcome<Mutation> create(const CallContext& c, const std::string& name, std::optional<Transform> transform, const Id& parent,
                             const nlohmann::json& settings);
    Outcome<ObjectView> get(const CallContext& c, const Id& id);
    struct Element {
        int index = 0;
        std::optional<double> phaseRad, amplitudePa, delaySec;
        std::optional<bool> active;
    };
    Outcome<Mutation> setExcitation(const CallContext& c, const Id& id, std::optional<std::uint64_t> expectedRevision, const std::vector<Element>& elements,
                                    bool clearOthers);
    Outcome<Mutation> focus(const CallContext& c, const Id& id, std::optional<std::uint64_t> expectedRevision, const Id& target,
                            std::optional<Vec3> point, bool timeDelay);
    // Element positions (array frame) and effective settings.
    std::vector<std::pair<Vec3, nlohmann::json>> elements(const SceneObject& array) const;

private:
    Session& s_;
};

class SimulationService {
public:
    explicit SimulationService(Session& s) : s_(s) {}
    struct Solver {
        std::string id, name, domain, method;
        bool available = false, startable = false;
        std::vector<std::string> assumptions;
    };
    Outcome<std::vector<Solver>> solvers(const CallContext& c);
    struct Plan {
        acoustics::FieldEstimate estimate;
        int activeSources = 0;
        std::string modelId;
        double frequencyHz = 0.0;
    };
    Outcome<Plan> estimateAcousticField(const CallContext& c, const Id& region);
    struct Started {
        JobView job;
        Plan plan;
        std::shared_ptr<acoustics::FieldJob> fieldJob;  // the GUI keeps it to display the result
    };
    // `runNumber` names the run (the GUI counts its runs).
    Outcome<Started> startAcousticField(const CallContext& c, const Id& region, int runNumber = 0);
    // A field region (RegionSettings keys: kind, size, countU, countV, countW), merged over the defaults.
    Outcome<Mutation> createFieldRegion(const CallContext& c, const std::string& name, std::optional<Transform> transform, const Id& parent,
                                        const nlohmann::json& settings);

private:
    Session& s_;
};

class JobService {
public:
    explicit JobService(Session& s) : s_(s) {}
    Outcome<JobView> get(const CallContext& c, const std::string& id);
    Outcome<std::vector<JobView>> list(const CallContext& c, bool activeOnly);
    Outcome<JobView> cancel(const CallContext& c, const std::string& id);
    JobView view(const JobInfo& info) const;

private:
    Session& s_;
};

class ResultService {
public:
    explicit ResultService(Session& s) : s_(s) {}
    Outcome<std::vector<const ResultEntry*>> list(const CallContext& c);
    Outcome<const ResultEntry*> get(const CallContext& c, const std::string& id);
    // axis 0 = u, 1 = v, 2 = w (fixed); stride >= 1; at most `maxSamples` samples.
    Outcome<FieldSlice> slice(const CallContext& c, const std::string& id, int fixedAxis, int fixedIndex, int stride, std::size_t maxSamples = 4194304);
    Outcome<std::vector<FieldPointSample>> samples(const CallContext& c, const std::string& id, const std::vector<Vec3>& points);

private:
    Session& s_;
};

class OptimizationService {
public:
    explicit OptimizationService(Session& s) : s_(s) {}
    Outcome<nlohmann::json> problems(const CallContext& c);  // [{id, name, algorithm, variables, objectives, constraints}]
    Outcome<nlohmann::json> runs(const CallContext& c);      // [{id, problem, algorithm, candidates, termination, best, bestValues}]
    Outcome<JobView> start(const CallContext& c, const std::string& problemId, int maxEvaluations);

private:
    Session& s_;
};

class WorkflowService {
public:
    explicit WorkflowService(Session& s) : s_(s) {}
    Outcome<nlohmann::json> templates(const CallContext& c);
    Outcome<JobView> execute(const CallContext& c, const std::string& workflowId, const nlohmann::json& definition, const nlohmann::json& parameters,
                             bool applyChanges);
    Outcome<std::pair<nlohmann::json, bool>> run(const CallContext& c, const std::string& jobId);

private:
    Session& s_;
};

// The Coupled Energy-Matter Engine (REQ-CHAT-20261126-016, RES-CEME-015):
// physical systems stored in the project (extension "multiphysics"), their
// validation, coupled simulations as jobs, and their result documents.
class MultiphysicsService {
public:
    explicit MultiphysicsService(Session& s) : s_(s) {}
    Outcome<std::vector<multiphysics::CouplingModelDescriptor>> couplingModels(const CallContext& c);
    Outcome<std::vector<nlohmann::json>> systems(const CallContext& c);
    Outcome<nlohmann::json> system(const CallContext& c, const std::string& id);
    // Adds the system, or replaces the one with its id; one undoable step.
    Outcome<Mutation> setSystem(const CallContext& c, const nlohmann::json& document);
    Outcome<Mutation> deleteSystem(const CallContext& c, const std::string& id);
    // A stored system (id) or a document (inline), against the project's materials.
    Outcome<ValidationView> validate(const CallContext& c, const std::string& id, const nlohmann::json& document);
    Outcome<JobView> start(const CallContext& c, const std::string& id, const nlohmann::json& document);
    Outcome<std::shared_ptr<const nlohmann::json>> result(const CallContext& c, const std::string& resultId);

    static constexpr const char* kExtension = "multiphysics";

private:
    Outcome<multiphysics::PhysicalSystem> resolve(const std::string& id, const nlohmann::json& document);
    Session& s_;
};

// Batches and transactions: operations run on a staging copy of the project
// (with the same revisions), and are committed as one undoable step.
class TransactionService {
public:
    explicit TransactionService(Session& s) : s_(s) {}
    // One staged operation: applies itself through the staging session's services.
    struct Operation {
        std::string id;
        std::string name;  // e.g. "GeometryService.CreatePrimitive"
        std::function<Outcome<Mutation>(Services& staging, const CallContext& context)> apply;
    };
    struct OperationResult {
        std::string id;
        bool succeeded = false;
        std::optional<ServiceError> error;
        ChangeSummary changes;
    };
    struct BatchResult {
        bool committed = false;
        std::vector<OperationResult> results;
        Mutation mutation;
    };
    // Atomic: all operations or none. A dry run (context.dryRun) validates and plans only.
    Outcome<BatchResult> executeBatch(const CallContext& c, const std::vector<Operation>& operations, const std::string& description);

    struct TransactionView {
        std::string id, state, description;  // state: open, committed, aborted, failed
        int staged = 0;
        std::uint64_t baseRevision = 0;
        ValidationView validation;
    };
    Outcome<TransactionView> begin(const CallContext& c, const std::string& description);
    Outcome<TransactionView> stage(const CallContext& c, const std::string& id, std::vector<Operation> operations);
    Outcome<std::pair<TransactionView, std::vector<OperationResult>>> validate(const CallContext& c, const std::string& id);
    Outcome<std::pair<TransactionView, BatchResult>> commit(const CallContext& c, const std::string& id);
    Outcome<TransactionView> abort(const CallContext& c, const std::string& id);
    Outcome<TransactionView> get(const CallContext& c, const std::string& id);

private:
    struct Staged {
        TransactionView view;
        std::string sessionId;
        std::vector<Operation> operations;
    };
    Outcome<BatchResult> run(const CallContext& c, const std::vector<Operation>& operations, const std::string& description, bool commitAtEnd,
                             std::optional<std::uint64_t> baseRevision);
    Session& s_;
    std::map<std::string, Staged> transactions_;
};

class ApplicationService {
public:
    explicit ApplicationService(Session& s) : s_(s) {}
    Outcome<CapabilityView> capability(const CallContext& c, const std::string& id);
    Outcome<std::vector<CapabilityView>> capabilities(const CallContext& c, const std::string& category);
    Outcome<DiagnosticsView> diagnostics(const CallContext& c);
    Outcome<std::vector<AuditRecord>> audit(const CallContext& c, const AuditQuery& query);

private:
    Session& s_;
};

// Every service of one session.
class Services {
public:
    explicit Services(Session& session);
    Session& session() { return session_; }
    ProjectService project;
    SceneService scene;
    GeometryService geometry;
    MaterialService materials;
    EmitterService emitters;
    ArrayService arrays;
    SimulationService simulation;
    JobService jobs;
    ResultService results;
    OptimizationService optimization;
    WorkflowService workflows;
    TransactionService transactions;
    MultiphysicsService multiphysics;
    ApplicationService application;

private:
    Session& session_;
};

}  // namespace resonant::services
