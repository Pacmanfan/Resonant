#include "Conversions.h"

#include <google/protobuf/util/time_util.h>

#include <chrono>

namespace resonant::automation_api {

using google::protobuf::util::TimeUtil;

int grpcStatusOf(services::ErrorCode code)
{
    using services::ErrorCode;
    switch (code) {
    case ErrorCode::InvalidArgument: return 3;
    case ErrorCode::NotFound: return 5;
    case ErrorCode::AlreadyExists: return 6;
    case ErrorCode::Conflict: return 10;  // ABORTED: a concurrency conflict
    case ErrorCode::FailedPrecondition: return 9;
    case ErrorCode::PermissionDenied: return 7;
    case ErrorCode::ResourceExhausted: return 8;
    case ErrorCode::Unimplemented: return 12;
    case ErrorCode::Unavailable: return 14;
    case ErrorCode::Cancelled: return 1;
    case ErrorCode::Internal: return 13;
    }
    return 13;
}

v1::ErrorCode toProto(services::ErrorCode code)
{
    using services::ErrorCode;
    switch (code) {
    case ErrorCode::InvalidArgument: return v1::ERROR_CODE_INVALID_ARGUMENT;
    case ErrorCode::NotFound: return v1::ERROR_CODE_NOT_FOUND;
    case ErrorCode::AlreadyExists: return v1::ERROR_CODE_ALREADY_EXISTS;
    case ErrorCode::Conflict: return v1::ERROR_CODE_CONFLICT;
    case ErrorCode::FailedPrecondition: return v1::ERROR_CODE_FAILED_PRECONDITION;
    case ErrorCode::PermissionDenied: return v1::ERROR_CODE_PERMISSION_DENIED;
    case ErrorCode::ResourceExhausted: return v1::ERROR_CODE_RESOURCE_EXHAUSTED;
    case ErrorCode::Unimplemented: return v1::ERROR_CODE_UNIMPLEMENTED;
    case ErrorCode::Unavailable: return v1::ERROR_CODE_UNAVAILABLE;
    case ErrorCode::Cancelled: return v1::ERROR_CODE_CANCELLED;
    case ErrorCode::Internal: return v1::ERROR_CODE_INTERNAL;
    }
    return v1::ERROR_CODE_INTERNAL;
}

namespace {
v1::ResourceType resourceTypeOf(const std::string& type)
{
    if (type == "scene-object") return v1::RESOURCE_TYPE_SCENE_OBJECT;
    if (type == "material") return v1::RESOURCE_TYPE_MATERIAL;
    if (type == "job") return v1::RESOURCE_TYPE_JOB;
    if (type == "result") return v1::RESOURCE_TYPE_RESULT;
    if (type == "transaction") return v1::RESOURCE_TYPE_TRANSACTION;
    if (type == "session") return v1::RESOURCE_TYPE_SESSION;
    if (type == "project") return v1::RESOURCE_TYPE_PROJECT;
    if (type == "optimization-problem") return v1::RESOURCE_TYPE_OPTIMIZATION_PROBLEM;
    if (type == "workflow") return v1::RESOURCE_TYPE_WORKFLOW;
    return v1::RESOURCE_TYPE_UNSPECIFIED;
}

google::protobuf::Timestamp stamp(std::chrono::system_clock::time_point t)
{
    return TimeUtil::NanosecondsToTimestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(t.time_since_epoch()).count());
}

void stamp(const std::string& iso, google::protobuf::Timestamp* out)
{
    if (!iso.empty())
        TimeUtil::FromString(iso, out);
}
}  // namespace

void toProto(const services::ServiceError& e, v1::Error* out)
{
    out->set_code(toProto(e.code));
    out->set_message(e.message);
    for (const auto& f : e.violations) {
        auto* v = out->add_violations();
        v->set_field(f.field);
        v->set_description(f.description);
    }
    if (!e.resourceId.empty()) {
        out->mutable_resource()->set_type(resourceTypeOf(e.resourceType));
        out->mutable_resource()->set_id(e.resourceId);
    }
    out->set_current_revision(e.currentRevision);
    out->set_retryable(e.retryable || e.code == services::ErrorCode::Unavailable);
    out->set_request_id(e.requestId);
}

services::CallContext contextOf(const v1::RequestHeader& h, services::Interface source)
{
    services::CallContext c;
    c.sessionId = h.session_id();
    c.requestId = h.request_id();
    c.idempotencyKey = h.idempotency_key();
    if (h.has_expected_project_revision())
        c.expectedProjectRevision = h.expected_project_revision();
    c.dryRun = h.dry_run();
    c.source = source;
    return c;
}

void toProto(const Vec3& v, v1::Vector3Meters* out)
{
    out->set_x_meters(v.x());
    out->set_y_meters(v.y());
    out->set_z_meters(v.z());
}

void toProto(const Vec3& v, v1::Vector3* out)
{
    out->set_x(v.x());
    out->set_y(v.y());
    out->set_z(v.z());
}

Vec3 fromProto(const v1::Vector3Meters& v)
{
    return Vec3(v.x_meters(), v.y_meters(), v.z_meters());
}

void toProto(const Transform& t, v1::Transform* out)
{
    toProto(t.position, out->mutable_position_meters());
    out->mutable_rotation_radians()->set_x_radians(t.rotation.x());
    out->mutable_rotation_radians()->set_y_radians(t.rotation.y());
    out->mutable_rotation_radians()->set_z_radians(t.rotation.z());
    toProto(t.scale, out->mutable_scale());
}

Transform fromProto(const v1::Transform& t, const Transform& base)
{
    Transform out = base;
    if (t.has_position_meters())
        out.position = fromProto(t.position_meters());
    if (t.has_rotation_radians())
        out.rotation = Vec3(t.rotation_radians().x_radians(), t.rotation_radians().y_radians(), t.rotation_radians().z_radians());
    if (t.has_scale())
        out.scale = Vec3(t.scale().x(), t.scale().y(), t.scale().z());
    return out;
}

void toProto(const services::ProjectView& v, v1::Project* out)
{
    out->set_project_id(v.id);
    out->set_name(v.name);
    out->set_file_name(v.fileName);
    out->set_revision(v.revision);
    out->set_modified(v.modified);
    out->set_object_count(v.objectCount);
    out->set_material_count(v.materialCount);
    out->set_can_undo(v.canUndo);
    out->set_can_redo(v.canRedo);
    out->set_undo_description(v.undoText);
    out->set_redo_description(v.redoText);
    stamp(v.createdUtc, out->mutable_created_time());
    stamp(v.modifiedUtc, out->mutable_modified_time());
}

void toProto(const services::ObjectView& v, v1::SceneObject* out)
{
    out->set_object_id(v.id.str());
    out->set_name(v.name);
    out->set_object_type(v.type);
    out->set_parent_id(v.parent.str());
    for (const Id& c : v.children)
        out->add_child_ids(c.str());
    out->set_revision(v.revision);
    toProto(v.local, out->mutable_local_transform());
    toProto(v.worldPosition, out->mutable_world_position_meters());
    out->set_visible(v.visible);
    out->set_material_id(v.material.str());
    if (!v.geometryKind.empty()) {
        out->mutable_geometry()->set_kind_id(v.geometryKind);
        for (const auto& [key, value] : v.parameters)
            (*out->mutable_geometry()->mutable_parameters())[key] = value;
    }
    if (!v.component.is_null())
        out->set_component_json(v.component.dump());
}

void toProto(const services::ChangeSummary& c, v1::ChangeSummary* out)
{
    const auto add = [](const std::vector<Id>& ids, google::protobuf::RepeatedPtrField<v1::ResourceRef>* field) {
        for (const Id& id : ids) {
            auto* r = field->Add();
            r->set_type(v1::RESOURCE_TYPE_SCENE_OBJECT);
            r->set_id(id.str());
        }
    };
    add(c.created, out->mutable_created());
    add(c.updated, out->mutable_updated());
    add(c.deleted, out->mutable_deleted());
    out->set_description(c.description);
}

void toProto(const services::ValidationView& v, v1::ValidationResult* out)
{
    out->set_valid(v.valid);
    for (const auto& i : v.issues) {
        auto* issue = out->add_issues();
        issue->set_severity(static_cast<v1::Severity>(i.severity));
        issue->set_field(i.field);
        issue->set_message(i.message);
        if (!i.resourceId.empty()) {
            issue->mutable_resource()->set_type(v1::RESOURCE_TYPE_SCENE_OBJECT);
            issue->mutable_resource()->set_id(i.resourceId);
        }
    }
}

void toProto(const services::JobView& v, v1::Job* out)
{
    out->set_job_id(v.id);
    out->set_job_type(v.type);
    out->set_display_name(v.name);
    static const std::map<std::string, v1::JobState> states = {{"created", v1::JOB_STATE_CREATED},       {"queued", v1::JOB_STATE_QUEUED},
                                                               {"running", v1::JOB_STATE_RUNNING},       {"cancelling", v1::JOB_STATE_CANCELLING},
                                                               {"cancelled", v1::JOB_STATE_CANCELLED},   {"completed", v1::JOB_STATE_COMPLETED},
                                                               {"failed", v1::JOB_STATE_FAILED}};
    const auto it = states.find(v.state);
    out->set_state(it == states.end() ? v1::JOB_STATE_UNSPECIFIED : it->second);
    out->set_progress_fraction(v.progress);
    out->set_phase(v.phase);
    out->set_elapsed_seconds(v.elapsedSeconds);
    if (v.error)
        toProto(*v.error, out->mutable_error());
    for (const std::string& r : v.resultIds) {
        auto* ref = out->add_results();
        ref->set_type(v1::RESOURCE_TYPE_RESULT);
        ref->set_id(r);
    }
    out->set_cancellable(v.cancellable);
    out->set_session_id(v.sessionId);
}

v1::SourceInterface toProto(services::Interface source)
{
    switch (source) {
    case services::Interface::Gui: return v1::SOURCE_INTERFACE_GUI;
    case services::Interface::Python: return v1::SOURCE_INTERFACE_PYTHON;
    case services::Interface::Grpc: return v1::SOURCE_INTERFACE_GRPC;
    case services::Interface::Mcp: return v1::SOURCE_INTERFACE_MCP;
    case services::Interface::Internal: return v1::SOURCE_INTERFACE_INTERNAL;
    case services::Interface::Unspecified: break;
    }
    return v1::SOURCE_INTERFACE_UNSPECIFIED;
}

services::Interface fromProto(v1::SourceInterface source)
{
    switch (source) {
    case v1::SOURCE_INTERFACE_GUI: return services::Interface::Gui;
    case v1::SOURCE_INTERFACE_PYTHON: return services::Interface::Python;
    case v1::SOURCE_INTERFACE_MCP: return services::Interface::Mcp;
    case v1::SOURCE_INTERFACE_INTERNAL: return services::Interface::Internal;
    default: break;
    }
    return services::Interface::Grpc;
}

v1::Permission toProto(services::Permission p)
{
    switch (p) {
    case services::Permission::Read: return v1::PERMISSION_READ;
    case services::Permission::Modify: return v1::PERMISSION_MODIFY;
    case services::Permission::Execute: return v1::PERMISSION_EXECUTE;
    case services::Permission::Files: return v1::PERMISSION_FILES;
    case services::Permission::Admin: return v1::PERMISSION_ADMIN;
    }
    return v1::PERMISSION_UNSPECIFIED;
}

std::uint32_t permissionsOf(const google::protobuf::RepeatedField<int>& permissions)
{
    std::uint32_t out = 0;
    for (int p : permissions)
        if (p >= v1::PERMISSION_READ && p <= v1::PERMISSION_ADMIN)
            out |= 1u << (p - 1);
    return out;
}

void toProto(const services::Event& e, v1::Event* out)
{
    out->set_event_id(e.eventId);
    out->set_sequence(e.sequence);
    *out->mutable_time() = stamp(e.time);
    static const std::map<services::EventCategory, v1::EventCategory> categories = {
        {services::EventCategory::Project, v1::EVENT_CATEGORY_PROJECT},       {services::EventCategory::Scene, v1::EVENT_CATEGORY_SCENE},
        {services::EventCategory::Material, v1::EVENT_CATEGORY_MATERIAL},     {services::EventCategory::Job, v1::EVENT_CATEGORY_JOB},
        {services::EventCategory::Result, v1::EVENT_CATEGORY_RESULT},         {services::EventCategory::Session, v1::EVENT_CATEGORY_SESSION},
        {services::EventCategory::Diagnostic, v1::EVENT_CATEGORY_DIAGNOSTIC}};
    out->set_category(categories.at(e.category));
    out->set_type(e.type);
    out->mutable_source()->set_session_id(e.sessionId);
    out->mutable_source()->set_client_name(e.clientName);
    out->mutable_source()->set_source_interface(toProto(e.source));
    out->set_project_id(e.projectId);
    for (const auto& r : e.resources) {
        auto* ref = out->add_resources();
        ref->set_type(resourceTypeOf(r.type));
        ref->set_id(r.id);
    }
    out->set_project_revision(e.projectRevision);
    out->set_request_id(e.requestId);
    const nlohmann::json& p = e.payload;
    const auto text = [&](const char* key) { return p.contains(key) && p[key].is_string() ? p[key].get<std::string>() : std::string(); };
    switch (e.category) {
    case services::EventCategory::Project:
        out->mutable_project()->set_project_id(text("projectId"));
        out->mutable_project()->set_project_name(text("projectName"));
        out->mutable_project()->set_description(text("description"));
        break;
    case services::EventCategory::Scene:
        out->mutable_object()->set_object_id(text("objectId"));
        out->mutable_object()->set_object_type(text("objectType"));
        out->mutable_object()->set_parent_id(text("parentId"));
        out->mutable_object()->set_object_revision(p.value("objectRevision", std::uint64_t{0}));
        break;
    case services::EventCategory::Material:
        for (const auto& id : p.value("objectIds", nlohmann::json::array()))
            out->mutable_material()->add_object_ids(id.get<std::string>());
        break;
    case services::EventCategory::Job: {
        v1::Job* job = out->mutable_job()->mutable_job();
        job->set_job_id(text("jobId"));
        job->set_display_name(text("name"));
        const std::string state = text("state");
        job->set_state(state == "queued"      ? v1::JOB_STATE_QUEUED
                       : state == "running"   ? v1::JOB_STATE_RUNNING
                       : state == "completed" ? v1::JOB_STATE_COMPLETED
                       : state == "failed"    ? v1::JOB_STATE_FAILED
                       : state == "cancelled" ? v1::JOB_STATE_CANCELLED
                                              : v1::JOB_STATE_UNSPECIFIED);
        job->set_progress_fraction(p.value("progress", 0.0));
        job->set_phase(text("message"));
        job->set_elapsed_seconds(p.value("elapsedSeconds", 0.0));
        break;
    }
    case services::EventCategory::Result:
        out->mutable_result()->set_result_id(text("resultId"));
        out->mutable_result()->set_result_kind(text("kind"));
        out->mutable_result()->set_job_id(text("jobId"));
        break;
    case services::EventCategory::Session:
        out->mutable_session()->set_session_id(text("sessionId"));
        out->mutable_session()->set_client_name(text("clientName"));
        break;
    case services::EventCategory::Diagnostic: break;
    }
}

void toProto(const services::CapabilityView& v, v1::Capability* out)
{
    out->set_capability_id(v.id);
    out->set_display_name(v.displayName);
    out->set_description(v.description);
    out->set_category(v.category);
    out->set_maturity(v.maturity == "stable"         ? v1::MATURITY_STABLE
                      : v.maturity == "experimental" ? v1::MATURITY_EXPERIMENTAL
                      : v.maturity == "planned"      ? v1::MATURITY_PLANNED
                                                     : v1::MATURITY_UNAVAILABLE);
    out->set_available(v.available);
    for (const auto& d : v.dependencies)
        out->add_required_dependencies(d);
    out->set_physics_domain(v.domain);
    for (const auto& d : v.dimensions)
        out->add_dimensions(d);
    for (const auto& l : v.limitations)
        out->add_limitations(l);
}

void toProto(const services::AuditRecord& r, v1::AuditRecord* out)
{
    out->set_sequence(r.sequence);
    *out->mutable_time() = stamp(r.time);
    out->set_request_id(r.requestId);
    out->set_session_id(r.sessionId);
    out->set_client_name(r.clientName);
    out->set_source_interface(toProto(r.source));
    out->set_operation(r.operation);
    for (const auto& id : r.resourceIds)
        out->add_resource_ids(id);
    out->set_revision_before(r.revisionBefore);
    out->set_revision_after(r.revisionAfter);
    out->set_succeeded(r.succeeded);
    out->set_dry_run(r.dryRun);
    if (r.error)
        toProto(*r.error, out->mutable_error());
    out->set_duration_seconds(r.durationSeconds);
}

void toProto(const nlohmann::json& m, v1::Material* out)
{
    out->set_material_id(m.value("id", std::string()));
    out->set_name(m.value("name", std::string()));
    out->set_description(m.value("description", std::string()));
    out->set_material_json(m.dump());
    // Properties by domain: {"domains": {"acoustic": {"density": {"value": ..., ...}}}} (the project file's form).
    const nlohmann::json domains = m.value("domains", nlohmann::json::object());
    for (const auto& [domain, properties] : domains.items()) {
        out->add_domains(domain);
        if (!properties.is_object())
            continue;
        for (const auto& [key, value] : properties.items()) {
            auto* p = out->add_properties();
            p->set_domain(domain);
            p->set_key(key);
            p->set_value_si(value.is_object() ? value.value("value", 0.0) : (value.is_number() ? value.get<double>() : 0.0));
            if (value.is_object()) {
                p->set_source(value.value("source", std::string()));
                const std::string provenance = value.value("provenance", std::string());
                p->set_provenance(provenance == "measured"       ? v1::MATERIAL_PROVENANCE_MEASURED
                                  : provenance == "manufacturer" ? v1::MATERIAL_PROVENANCE_MANUFACTURER
                                  : provenance == "literature"   ? v1::MATERIAL_PROVENANCE_LITERATURE
                                  : provenance == "theoretical"  ? v1::MATERIAL_PROVENANCE_THEORETICAL
                                  : provenance == "illustrative" ? v1::MATERIAL_PROVENANCE_ILLUSTRATIVE
                                                                 : v1::MATERIAL_PROVENANCE_UNSPECIFIED);
            }
        }
    }
}

void toProto(const services::ResultEntry& r, std::uint64_t, v1::ResultSummary* out)
{
    out->set_result_id(r.id);
    out->set_result_kind(r.kind);
    out->set_display_name(r.name);
    *out->mutable_created_time() = stamp(r.created);
    if (!r.field)
        return;
    const acoustics::FieldResult& f = *r.field;
    out->set_quantity("pressure");
    out->set_unit("Pa");
    out->set_value_type("complex64 (peak amplitude)");
    out->set_frequency_hz(f.frequencyHz);
    v1::FieldGrid* g = out->mutable_grid();
    toProto(f.frame.origin, g->mutable_origin_world_meters());
    const auto axis = [](const Vec3& extent, int count, v1::Vector3* unit) -> double {
        const double length = extent.norm();
        toProto(length > 0.0 ? Vec3(extent / length) : Vec3(Vec3::Zero()), unit);
        return count > 1 ? length / (count - 1) : 0.0;
    };
    g->set_step_u_meters(axis(f.frame.axisU, f.frame.nu, g->mutable_axis_u()));
    g->set_step_v_meters(axis(f.frame.axisV, f.frame.nv, g->mutable_axis_v()));
    g->set_step_w_meters(axis(f.frame.axisW, f.frame.nw, g->mutable_axis_w()));
    g->set_count_u(f.frame.nu);
    g->set_count_v(f.frame.nv);
    g->set_count_w(f.frame.nw);
    out->set_max_magnitude(f.statistics.maxMagnitude);
    v1::ResultProvenance* p = out->mutable_provenance();
    p->set_physics_domain("acoustic");
    p->set_solver_id(f.metadata.solverId);
    p->set_solver_version(f.metadata.solverVersion);
    p->set_approximation(f.metadata.solverId.find("reflector") != std::string::npos ? "Kirchhoff physical optics (not full-wave)"
                                                                                    : "analytical free field (not full-wave)");
    for (const auto& a : f.assumptions)
        p->add_assumptions(a);
    p->set_project_revision(r.projectRevision);
    p->set_job_id(r.jobId);
    p->set_configuration_json(f.metadata.configuration.dump());
}

void sourceToProto(const nlohmann::json& s, v1::SourceSettings* out)
{
    out->set_frequency_hz(s.value("frequencyHz", 0.0));
    out->set_amplitude_peak_pa_at_1m(s.value("amplitudePa", 0.0));
    out->set_phase_radians(s.value("phaseRad", 0.0));
    out->set_delay_seconds(s.value("delaySec", 0.0));
    out->set_active(s.value("active", true));
    const nlohmann::json d = s.value("directivity", nlohmann::json::object());
    const std::string model = d.value("model", std::string("omnidirectional"));
    out->set_directivity(model == "cardioid"        ? v1::DIRECTIVITY_MODEL_CARDIOID
                         : model == "baffledPiston" ? v1::DIRECTIVITY_MODEL_BAFFLED_PISTON
                                                    : v1::DIRECTIVITY_MODEL_OMNIDIRECTIONAL);
    out->set_cardioid_order(d.value("order", 1.0));
    out->set_piston_radius_meters(d.value("pistonRadius", 0.0));
    out->set_radius_meters(s.value("radius", 0.0));
    out->set_waveform(s.value("waveform", std::string("sine")));
}

namespace {
std::string directivityName(v1::DirectivityModel m)
{
    switch (m) {
    case v1::DIRECTIVITY_MODEL_CARDIOID: return "cardioid";
    case v1::DIRECTIVITY_MODEL_BAFFLED_PISTON: return "baffledPiston";
    default: return "omnidirectional";
    }
}
}  // namespace

nlohmann::json sourceFromProto(const v1::SourceSettings& s)
{
    nlohmann::json j = {{"frequencyHz", s.frequency_hz()},
                        {"amplitudePa", s.amplitude_peak_pa_at_1m()},
                        {"phaseRad", s.phase_radians()},
                        {"delaySec", s.delay_seconds()},
                        {"active", s.active()},
                        {"directivity", {{"model", directivityName(s.directivity())}}}};
    if (s.cardioid_order() > 0.0)
        j["directivity"]["order"] = s.cardioid_order();
    if (s.piston_radius_meters() > 0.0)
        j["directivity"]["pistonRadius"] = s.piston_radius_meters();
    if (s.radius_meters() > 0.0)
        j["radius"] = s.radius_meters();
    if (!s.waveform().empty())
        j["waveform"] = s.waveform();
    return j;
}

nlohmann::json sourcePatchFromProto(const v1::SourceSettingsPatch& p)
{
    nlohmann::json j = nlohmann::json::object();
    if (p.has_frequency_hz())
        j["frequencyHz"] = p.frequency_hz();
    if (p.has_amplitude_peak_pa_at_1m())
        j["amplitudePa"] = p.amplitude_peak_pa_at_1m();
    if (p.has_phase_radians())
        j["phaseRad"] = p.phase_radians();
    if (p.has_delay_seconds())
        j["delaySec"] = p.delay_seconds();
    if (p.has_active())
        j["active"] = p.active();
    if (p.has_directivity())
        j["directivity"]["model"] = directivityName(p.directivity());
    if (p.has_cardioid_order())
        j["directivity"]["order"] = p.cardioid_order();
    if (p.has_piston_radius_meters())
        j["directivity"]["pistonRadius"] = p.piston_radius_meters();
    if (p.has_radius_meters())
        j["radius"] = p.radius_meters();
    return j;
}

std::string_view arrayLayoutName(v1::ArrayLayout layout)
{
    switch (layout) {
    case v1::ARRAY_LAYOUT_RECTANGULAR_PLANAR: return "rectangularPlanar";
    case v1::ARRAY_LAYOUT_CIRCULAR: return "circular";
    case v1::ARRAY_LAYOUT_CYLINDRICAL: return "cylindrical";
    case v1::ARRAY_LAYOUT_SPHERICAL: return "spherical";
    case v1::ARRAY_LAYOUT_CUSTOM: return "custom";
    default: return "linear";
    }
}

v1::ArrayLayout arrayLayoutOf(const std::string& name)
{
    if (name == "rectangularPlanar") return v1::ARRAY_LAYOUT_RECTANGULAR_PLANAR;
    if (name == "circular") return v1::ARRAY_LAYOUT_CIRCULAR;
    if (name == "cylindrical") return v1::ARRAY_LAYOUT_CYLINDRICAL;
    if (name == "spherical") return v1::ARRAY_LAYOUT_SPHERICAL;
    if (name == "custom") return v1::ARRAY_LAYOUT_CUSTOM;
    return v1::ARRAY_LAYOUT_LINEAR;
}

}  // namespace resonant::automation_api
