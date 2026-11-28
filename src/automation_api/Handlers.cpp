// The unary methods of resonant.v1: decode, call the one service
// implementation, encode. No behaviour lives here beyond the mapping (and the
// batch operations, which reuse the same mapping against a staging project).
#include "HandlerSupport.h"

#include "foundation/Version.h"
#include "project/Project.h"
#include "resonant/v1/application/application_service.pb.h"
#include "resonant/v1/arrays/array_service.pb.h"
#include "resonant/v1/automation/audit_service.pb.h"
#include "resonant/v1/automation/event_service.pb.h"
#include "resonant/v1/automation/job_service.pb.h"
#include "resonant/v1/automation/session_service.pb.h"
#include "resonant/v1/automation/transaction_service.pb.h"
#include "resonant/v1/automation/workflow_service.pb.h"
#include "resonant/v1/emitters/emitter_service.pb.h"
#include "resonant/v1/geometry/geometry_service.pb.h"
#include "resonant/v1/materials/material_service.pb.h"
#include "resonant/v1/multiphysics/multiphysics_service.pb.h"
#include "resonant/v1/functional/functional_service.pb.h"
#include "resonant/v1/multiscale/multiscale_service.pb.h"
#include "resonant/v1/resonance/resonance_service.pb.h"
#include "resonant/v1/optimization/optimization_service.pb.h"
#include "resonant/v1/project/project_service.pb.h"
#include "resonant/v1/results/results_service.pb.h"
#include "resonant/v1/scene/scene_service.pb.h"
#include "resonant/v1/simulation/simulation_service.pb.h"
#include "scene/SceneObject.h"
#include "services/Services.h"

#include <google/protobuf/util/json_util.h>
#include <google/protobuf/util/time_util.h>

#include <chrono>
#include <limits>
#include <thread>

namespace resonant::automation_api {

using services::CallContext;
using services::ErrorCode;
using services::Mutation;
using services::Outcome;
using services::Permission;
using services::ServiceError;
using services::Services;
using detail::Failure;

namespace {

Id idOf(const std::string& s)
{
    return s.empty() ? Id() : Id(s);
}

std::vector<Id> idsOf(const google::protobuf::RepeatedPtrField<std::string>& ids)
{
    std::vector<Id> out;
    for (const std::string& id : ids)
        out.push_back(Id(id));
    return out;
}

std::map<std::string, double> mapOf(const google::protobuf::Map<std::string, double>& m)
{
    return std::map<std::string, double>(m.begin(), m.end());
}

google::protobuf::Timestamp stamp(std::chrono::system_clock::time_point t)
{
    return google::protobuf::util::TimeUtil::NanosecondsToTimestamp(
        std::chrono::duration_cast<std::chrono::nanoseconds>(t.time_since_epoch()).count());
}

Failure structOf(const nlohmann::json& j, google::protobuf::Struct* out)
{
    if (!j.is_object())
        return std::nullopt;
    if (!google::protobuf::util::JsonStringToMessage(j.dump(), out).ok())
        return ServiceError::make(ErrorCode::Internal, "a value could not be converted to google.protobuf.Struct");
    return std::nullopt;
}

nlohmann::json jsonOf(const google::protobuf::Struct& s)
{
    std::string text;
    if (!google::protobuf::util::MessageToJsonString(s, &text).ok())
        return nlohmann::json::object();
    return nlohmann::json::parse(text, nullptr, false);
}

void addMaterialRef(google::protobuf::RepeatedPtrField<v1::ResourceRef>* field, const std::string& id)
{
    auto* r = field->Add();
    r->set_type(v1::RESOURCE_TYPE_MATERIAL);
    r->set_id(id);
}

// --- the mutations shared by the unary methods and batches ------------------------------

Outcome<Mutation> applyCreatePrimitive(Services& s, const CallContext& c, const v1::CreatePrimitiveRequest& r)
{
    services::GeometryService::Create create;
    create.kind = r.kind_id();
    create.name = r.name();
    create.parameters = mapOf(r.parameters());
    if (r.has_local_transform())
        create.transform = fromProto(r.local_transform());
    create.parent = idOf(r.parent_id());
    create.material = idOf(r.material_id());
    return s.geometry.createPrimitive(c, create);
}

Outcome<Mutation> applyUpdateGeometryParameters(Services& s, const CallContext& c, const v1::UpdateGeometryParametersRequest& r)
{
    return s.geometry.updateParameters(c, Id(r.object_id()), detail::expected(r.has_expected_revision(), r.expected_revision()), mapOf(r.parameters()));
}

Outcome<Mutation> applyUpdateObject(Services& s, const CallContext& c, const v1::UpdateObjectRequest& r)
{
    services::SceneService::Update u;
    if (r.has_name())
        u.name = r.name();
    if (r.has_local_transform()) {
        auto current = s.scene.object(c, Id(r.object_id()));
        if (!current)
            return current.error();
        u.transform = fromProto(r.local_transform(), current.value().local);
    }
    if (r.has_visible())
        u.visible = r.visible();
    if (r.has_parent_id())
        u.parent = r.parent_id();
    return s.scene.update(c, Id(r.object_id()), detail::expected(r.has_expected_revision(), r.expected_revision()), u);
}

Outcome<Mutation> applyDeleteObjects(Services& s, const CallContext& c, const v1::DeleteObjectsRequest& r)
{
    std::map<Id, std::uint64_t> expected;
    for (const auto& [id, revision] : r.expected_revisions())
        expected[Id(id)] = revision;
    return s.scene.remove(c, idsOf(r.object_ids()), expected);
}

Outcome<Mutation> applyAssignMaterial(Services& s, const CallContext& c, const v1::AssignMaterialRequest& r)
{
    return s.materials.assign(c, Id(r.material_id()), idsOf(r.object_ids()));
}

Outcome<Mutation> applyCreateEmitter(Services& s, const CallContext& c, const v1::CreateEmitterRequest& r)
{
    std::optional<Transform> transform;
    if (r.has_local_transform())
        transform = fromProto(r.local_transform());
    return s.emitters.create(c, r.name(), transform, idOf(r.parent_id()), r.has_source() ? sourcePatchFromProto(r.source()) : nlohmann::json());
}

Outcome<Mutation> applyUpdateEmitter(Services& s, const CallContext& c, const v1::UpdateEmitterRequest& r)
{
    std::optional<Transform> transform;
    if (r.has_local_transform()) {
        auto current = s.scene.object(c, Id(r.object_id()));
        if (!current)
            return current.error();
        transform = fromProto(r.local_transform(), current.value().local);
    }
    return s.emitters.update(c, Id(r.object_id()), detail::expected(r.has_expected_revision(), r.expected_revision()),
                             r.has_source() ? sourcePatchFromProto(r.source()) : nlohmann::json(), transform);
}

Outcome<Mutation> applyCreateArray(Services& s, const CallContext& c, const v1::CreateArrayRequest& r)
{
    nlohmann::json settings = nlohmann::json::object();
    if (r.layout() != v1::ARRAY_LAYOUT_UNSPECIFIED)
        settings["layout"] = std::string(arrayLayoutName(r.layout()));
    if (r.has_count_x())
        settings["countX"] = r.count_x();
    if (r.has_count_y())
        settings["countY"] = r.count_y();
    if (r.has_spacing_x_meters())
        settings["spacingX"] = r.spacing_x_meters();
    if (r.has_spacing_y_meters())
        settings["spacingY"] = r.spacing_y_meters();
    if (r.has_radius_meters())
        settings["radius"] = r.radius_meters();
    if (r.has_element_defaults())
        settings["element"] = sourcePatchFromProto(r.element_defaults());
    std::optional<Transform> transform;
    if (r.has_local_transform())
        transform = fromProto(r.local_transform());
    return s.arrays.create(c, r.name(), transform, idOf(r.parent_id()), settings);
}

Outcome<Mutation> applySetArrayExcitation(Services& s, const CallContext& c, const v1::SetArrayExcitationRequest& r)
{
    std::vector<services::ArrayService::Element> elements;
    for (const v1::ElementExcitation& e : r.elements()) {
        services::ArrayService::Element el;
        el.index = e.index();
        if (e.has_phase_radians())
            el.phaseRad = e.phase_radians();
        if (e.has_amplitude_peak_pa_at_1m())
            el.amplitudePa = e.amplitude_peak_pa_at_1m();
        if (e.has_delay_seconds())
            el.delaySec = e.delay_seconds();
        if (e.has_active())
            el.active = e.active();
        elements.push_back(el);
    }
    return s.arrays.setExcitation(c, Id(r.object_id()), detail::expected(r.has_expected_revision(), r.expected_revision()), elements,
                                  r.clear_other_overrides());
}

Outcome<Mutation> applyFocusArray(Services& s, const CallContext& c, const v1::FocusArrayRequest& r)
{
    std::optional<Vec3> point;
    Id target;
    if (r.has_point_world_meters())
        point = fromProto(r.point_world_meters());
    else if (r.has_target_object_id())
        target = Id(r.target_object_id());
    else
        return services::invalid("give a target object or a point", "focus");
    return s.arrays.focus(c, Id(r.object_id()), detail::expected(r.has_expected_revision(), r.expected_revision()), target, point,
                          r.control() == v1::FOCUS_CONTROL_TIME_DELAY);
}

Outcome<Mutation> applyCreateFieldRegion(Services& s, const CallContext& c, const v1::CreateFieldRegionRequest& r)
{
    nlohmann::json settings = nlohmann::json::object();
    static const std::map<int, const char*> kinds = {{v1::FIELD_REGION_KIND_PLANE_XY, "planeXY"},
                                                     {v1::FIELD_REGION_KIND_PLANE_XZ, "planeXZ"},
                                                     {v1::FIELD_REGION_KIND_PLANE_YZ, "planeYZ"},
                                                     {v1::FIELD_REGION_KIND_VOLUME, "volume"}};
    if (const auto it = kinds.find(r.kind()); it != kinds.end())
        settings["kind"] = it->second;
    if (r.has_size_x_meters() || r.has_size_y_meters() || r.has_size_z_meters())
        settings["size"] = {r.has_size_x_meters() ? r.size_x_meters() : 0.2, r.has_size_y_meters() ? r.size_y_meters() : 0.2,
                            r.has_size_z_meters() ? r.size_z_meters() : 0.2};
    if (r.has_count_u())
        settings["countU"] = r.count_u();
    if (r.has_count_v())
        settings["countV"] = r.count_v();
    if (r.has_count_w())
        settings["countW"] = r.count_w();
    std::optional<Transform> transform;
    if (r.has_local_transform())
        transform = fromProto(r.local_transform());
    return s.simulation.createFieldRegion(c, r.name(), transform, idOf(r.parent_id()), settings);
}

services::TransactionService::Operation operationOf(const v1::BatchOperation& op)
{
    services::TransactionService::Operation out;
    out.id = op.operation_id();
    using Apply = std::function<Outcome<Mutation>(Services&, const CallContext&)>;
    switch (op.operation_case()) {
#define RESONANT_BATCH_CASE(Case, getter, Name, fn)                                          \
    case v1::BatchOperation::Case: {                                                         \
        const auto request = op.getter();                                                    \
        out.name = Name;                                                                     \
        out.apply = Apply([request](Services& s, const CallContext& c) { return fn(s, c, request); }); \
        break;                                                                               \
    }
        RESONANT_BATCH_CASE(kCreatePrimitive, create_primitive, "GeometryService.CreatePrimitive", applyCreatePrimitive)
        RESONANT_BATCH_CASE(kUpdateGeometryParameters, update_geometry_parameters, "GeometryService.UpdateGeometryParameters", applyUpdateGeometryParameters)
        RESONANT_BATCH_CASE(kUpdateObject, update_object, "SceneService.UpdateObject", applyUpdateObject)
        RESONANT_BATCH_CASE(kDeleteObjects, delete_objects, "SceneService.DeleteObjects", applyDeleteObjects)
        RESONANT_BATCH_CASE(kAssignMaterial, assign_material, "MaterialService.AssignMaterial", applyAssignMaterial)
        RESONANT_BATCH_CASE(kCreateEmitter, create_emitter, "EmitterService.CreateEmitter", applyCreateEmitter)
        RESONANT_BATCH_CASE(kUpdateEmitter, update_emitter, "EmitterService.UpdateEmitter", applyUpdateEmitter)
        RESONANT_BATCH_CASE(kCreateArray, create_array, "ArrayService.CreateArray", applyCreateArray)
        RESONANT_BATCH_CASE(kSetArrayExcitation, set_array_excitation, "ArrayService.SetArrayExcitation", applySetArrayExcitation)
        RESONANT_BATCH_CASE(kFocusArray, focus_array, "ArrayService.FocusArray", applyFocusArray)
        RESONANT_BATCH_CASE(kCreateFieldRegion, create_field_region, "SimulationService.CreateFieldRegion", applyCreateFieldRegion)
#undef RESONANT_BATCH_CASE
    case v1::BatchOperation::OPERATION_NOT_SET:
        out.name = "unset";
        out.apply = [](Services&, const CallContext&) -> Outcome<Mutation> {
            return services::invalid("the batch operation has no operation set", "operations");
        };
        break;
    }
    return out;
}

void resultsToProto(const std::vector<services::TransactionService::OperationResult>& results,
                    google::protobuf::RepeatedPtrField<v1::OperationResult>* out)
{
    for (const auto& r : results) {
        v1::OperationResult* o = out->Add();
        o->set_operation_id(r.id);
        o->set_succeeded(r.succeeded);
        if (r.error)
            toProto(*r.error, o->mutable_error());
        toProto(r.changes, o->mutable_changes());
        for (const Id& id : r.changes.created)
            o->add_created_ids(id.str());
    }
}

void transactionToProto(const services::TransactionService::TransactionView& t, v1::Transaction* out)
{
    out->set_transaction_id(t.id);
    out->set_state(t.state == "open"        ? v1::TRANSACTION_STATE_OPEN
                   : t.state == "committed" ? v1::TRANSACTION_STATE_COMMITTED
                   : t.state == "aborted"   ? v1::TRANSACTION_STATE_ABORTED
                   : t.state == "failed"    ? v1::TRANSACTION_STATE_FAILED
                                            : v1::TRANSACTION_STATE_UNSPECIFIED);
    out->set_description(t.description);
    out->set_staged_operations(t.staged);
    out->set_base_project_revision(t.baseRevision);
    toProto(t.validation, out->mutable_validation());
}

std::vector<services::TransactionService::Operation> operationsOf(const google::protobuf::RepeatedPtrField<v1::BatchOperation>& ops)
{
    std::vector<services::TransactionService::Operation> out;
    for (const v1::BatchOperation& op : ops)
        out.push_back(operationOf(op));
    return out;
}

void sessionToProto(const services::SessionInfo& s, v1::Session* out)
{
    out->set_session_id(s.id);
    out->set_client_name(s.clientName);
    out->set_source_interface(toProto(s.source));
    for (services::Permission p : {Permission::Read, Permission::Modify, Permission::Execute, Permission::Files, Permission::Admin})
        if (services::has(s.permissions, p))
            out->add_permissions(toProto(p));
    *out->mutable_opened_time() = stamp(s.opened);
    *out->mutable_last_seen_time() = stamp(s.lastSeen);
}

bool remote(services::Interface source)
{
    return source == services::Interface::Grpc || source == services::Interface::Mcp || source == services::Interface::Unspecified;
}

// --- functional structures ------------------------------------------------------------------

double numberOf(const nlohmann::json& v)
{
    if (v.is_number())
        return v.get<double>();
    if (v.is_string())
        return v.get<std::string>() == "-inf" ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity();
    return std::numeric_limits<double>::quiet_NaN();
}

void structureToProto(const nlohmann::json& document, v1::FunctionalStructure* out)
{
    out->set_structure_id(document.value("id", std::string()));
    out->set_name(document.value("name", std::string()));
    [[maybe_unused]] const Failure f = structOf(document, out->mutable_definition());
}

void parametersToProto(const nlohmann::json& list, google::protobuf::RepeatedPtrField<v1::StructureParameter>* out)
{
    for (const auto& p : list) {
        auto* x = out->Add();
        x->set_parameter_id(p.value("id", std::string()));
        x->set_name(p.value("name", std::string()));
        x->set_kind(p.value("kind", std::string()));
        x->set_value(p.value("value", 0.0));
        x->set_unit(p.value("unit", std::string()));
        x->set_min(p["min"].is_number() ? p["min"].get<double>() : -std::numeric_limits<double>::infinity());
        x->set_max(p["max"].is_number() ? p["max"].get<double>() : std::numeric_limits<double>::infinity());
        x->set_editable(p.value("editable", false));
        x->set_binding(p.value("binding", std::string()));
        x->set_object_name(p.value("object", std::string()));
        x->set_group(p.value("group", std::string()));
    }
}

Failure functionalResultToProto(const nlohmann::json& d, bool includeDocument, v1::FunctionalResult* out)
{
    out->set_result_id(d.value("resultId", std::string()));
    out->set_kind(d.value("kind", std::string()));
    out->set_structure_id(d.value("structure", nlohmann::json::object()).value("id", std::string()));
    out->set_model_id(d.value("analysis", nlohmann::json::object()).value("model", nlohmann::json::object()).value("id", std::string()));
    if (d.value("kind", std::string()) == "analysis")
        for (const auto& m : d.value("metrics", nlohmann::json::array())) {
            auto* x = out->add_metrics();
            x->set_key(m.value("key", std::string()));
            x->set_label(m.value("label", std::string()));
            x->set_unit(m.value("unit", std::string()));
            x->set_value(numberOf(m["value"]));
        }
    out->set_stale(d.value("stale", false));
    for (const auto& r : d.value("currentStaleness", nlohmann::json::array())) {
        auto* x = out->add_staleness();
        x->set_what(r.value("what", std::string()));
        x->set_detail(r.value("detail", std::string()));
    }
    out->set_stale_on_completion(d.value("staleOnCompletion", false));
    out->set_feasible(d.value("feasible", true));
    out->set_behaviour(d.value("behaviour", std::string()));
    if (d.value("kind", std::string()) == "analysis")
        for (const auto& m : d.value("modes", nlohmann::json::array())) {
            auto* x = out->add_modes();
            x->set_label(m.value("label", std::string()));
            x->set_nature(m.value("nature", std::string()));
            x->set_frequency_hz(numberOf(m["frequencyHz"]));
            x->set_quality(numberOf(m["quality"]));
            x->set_decay_rate(m.value("decayRate", 0.0));
            x->set_bandwidth_hz(m.value("bandwidthHz", 0.0));
            const nlohmann::json losses = m.value("lossQ", nlohmann::json::object());  // items() must not outlive its object
            for (const auto& [mechanism, q] : losses.items())
                (*x->mutable_loss_quality())[mechanism] = numberOf(q);
        }
    for (const auto& w : d.value("warnings", nlohmann::json::array()))
        if (w.is_string())
            out->add_warnings(w.get<std::string>());
    if (includeDocument)
        return structOf(d, out->mutable_document());
    return std::nullopt;
}

// --- multiphysics ----------------------------------------------------------------------------

void systemToProto(const nlohmann::json& document, v1::PhysicalSystem* out)
{
    out->set_system_id(document.value("id", std::string()));
    out->set_name(document.value("name", std::string()));
    [[maybe_unused]] const Failure f = structOf(document, out->mutable_definition());
}

Failure coupledResultToProto(const std::string& resultId, const nlohmann::json& d, bool includeDocument, v1::CoupledResult* out)
{
    out->set_result_id(resultId);
    out->set_system_id(d.contains("provenance") ? d["provenance"]["system"].value("id", std::string()) : std::string());
    out->set_status(d.value("status", std::string()));
    out->set_message(d.value("message", std::string()));
    out->set_failed_stage(d.value("failedStage", std::string()));
    out->set_passes(d.value("passes", 0));
    out->set_job_id(d.value("jobId", std::string()));
    for (const auto& h : d.value("history", nlohmann::json::array())) {
        v1::CouplingIteration* it = out->add_history();
        it->set_pass(h.value("pass", 0));
        it->set_max_temperature_change_kelvin(h.value("maxTemperatureChangeK", 0.0));
        it->set_max_geometry_change(h.value("maxGeometryChange", 0.0));
        it->set_transferred_energy(h.value("transferredEnergy", 0.0));
    }
    const nlohmann::json ledger = d.value("ledger", nlohmann::json::object());
    for (const auto& t : ledger.value("terms", nlohmann::json::array())) {
        v1::EnergyTerm* e = out->add_terms();
        e->set_participant(t.value("participant", std::string()));
        e->set_domain(t.value("domain", std::string()));
        e->set_kind(t.value("kind", std::string()));
        e->set_mechanism(t.value("mechanism", std::string()));
        e->set_value(t.value("value", 0.0));
        e->set_description(t.value("description", std::string()));
    }
    for (const auto& t : ledger.value("transfers", nlohmann::json::array())) {
        v1::EnergyTransfer* e = out->add_transfers();
        e->set_coupling_id(t.value("coupling", std::string()));
        e->set_model_id(t.value("model", std::string()));
        e->set_source_participant(t.value("sourceParticipant", std::string()));
        e->set_destination_participant(t.value("destinationParticipant", std::string()));
        e->set_mechanism(t.value("mechanism", std::string()));
        e->set_source_quantity(t.value("sourceQuantity", std::string()));
        e->set_target_quantity(t.value("targetQuantity", std::string()));
        e->set_unit(t.value("unit", std::string()));
        e->set_method(t.value("method", std::string()));
        e->set_source_integral(t.value("sourceIntegral", 0.0));
        e->set_target_integral(t.value("targetIntegral", 0.0));
        e->set_relative_difference(t.value("relativeDifference", 0.0));
        e->set_energy(t.value("energy", false));
    }
    const nlohmann::json a = d.value("audit", nlohmann::json::object());
    v1::ConservationAudit* audit = out->mutable_audit();
    audit->set_mode(a.value("mode", std::string()));
    audit->set_external_input(a.value("externalInput", 0.0));
    audit->set_boundary_out(a.value("boundaryOut", 0.0));
    audit->set_dissipated(a.value("dissipated", 0.0));
    audit->set_stored(a.value("stored", 0.0));
    audit->set_transfer_error(a.value("transferError", 0.0));
    audit->set_system_residual(a.value("systemResidual", 0.0));
    audit->set_relative_residual(a.value("relativeResidual", 0.0));
    audit->set_tolerance(a.value("tolerance", 0.0));
    audit->set_balanced(a.value("balanced", false));
    for (const auto& line : a.value("attribution", nlohmann::json::array()))
        audit->add_attribution(line.get<std::string>());
    for (const auto& b : a.value("participants", nlohmann::json::array())) {
        v1::ParticipantBalance* p = audit->add_participants();
        p->set_participant(b.value("participant", std::string()));
        p->set_domain(b.value("domain", std::string()));
        p->set_input(b.value("input", 0.0));
        p->set_transfer_in(b.value("transferIn", 0.0));
        p->set_boundary_out(b.value("boundaryOut", 0.0));
        p->set_dissipated(b.value("dissipated", 0.0));
        p->set_stored(b.value("stored", 0.0));
        p->set_transfer_out(b.value("transferOut", 0.0));
        p->set_residual(b.value("residual", 0.0));
        p->set_relative_residual(b.value("relativeResidual", 0.0));
        p->set_solver_residual(b.value("solverResidual", 0.0));
    }
    for (const auto& w : d.value("warnings", nlohmann::json::array()))
        out->add_warnings(w.get<std::string>());
    if (includeDocument)
        return structOf(d, out->mutable_document());
    return std::nullopt;
}


}  // namespace

// --- application, sessions, audit ---------------------------------------------------------------

void Dispatcher::registerApplication()
{
    unary<v1::GetApplicationInfoRequest, v1::GetApplicationInfoResponse>(
        "ApplicationService", "GetApplicationInfo", Permission::Read, false, true,
        [this](const v1::GetApplicationInfoRequest&, v1::GetApplicationInfoResponse& out, const CallContext&) -> Failure {
            // Needs no session: a client checks the version before it opens one.
            out.mutable_info()->set_application_name("Resonant");
            out.mutable_info()->set_application_version(kApplicationVersion);
            out.mutable_info()->set_api_version("resonant.v1");
            out.mutable_info()->set_api_revision(kApiRevision);
            out.mutable_info()->set_host(session_.hostKind());
            return std::nullopt;
        });
    unary<v1::GetCapabilitiesRequest, v1::GetCapabilitiesResponse>(
        "ApplicationService", "GetCapabilities", Permission::Read, false, true,
        [this](const v1::GetCapabilitiesRequest& in, v1::GetCapabilitiesResponse& out, const CallContext& c) -> Failure {
            auto caps = session_.services().application.capabilities(c, in.category());
            if (!caps)
                return caps.error();
            for (const auto& cap : caps.value())
                toProto(cap, out.add_capabilities());
            if (in.category().empty() || in.category() == "operation")
                for (const MethodInfo& m : methods()) {
                    v1::OperationCapability* o = out.add_operations();
                    o->set_method(m.method);
                    o->set_maturity(v1::MATURITY_STABLE);
                    o->set_required_permission(toProto(m.permission));
                    o->set_mutating(m.mutating);
                    o->set_idempotent(m.idempotent);
                }
            return std::nullopt;
        });
    unary<v1::GetDiagnosticsRequest, v1::GetDiagnosticsResponse>(
        "ApplicationService", "GetDiagnostics", Permission::Read, false, true,
        [this](const v1::GetDiagnosticsRequest&, v1::GetDiagnosticsResponse& out, const CallContext& c) -> Failure {
            auto d = session_.services().application.diagnostics(c);
            if (!d)
                return d.error();
            v1::Diagnostics* o = out.mutable_diagnostics();
            o->set_health(d.value().health);
            std::set<std::string> serviceNames;
            for (const auto& [name, h] : handlers_)
                serviceNames.insert(name.substr(1, name.find('/', 1) - 1));
            for (const std::string& name : serviceNames) {
                v1::ServiceStatus* st = o->add_services();
                st->set_service(name);
                st->set_available(true);
            }
            o->set_active_sessions(d.value().activeSessions);
            o->set_active_jobs(d.value().activeJobs);
            o->set_last_event_sequence(d.value().lastEventSequence);
            o->set_oldest_retained_event_sequence(d.value().oldestEventSequence);
            o->set_audit_records(d.value().auditRecords);
            for (const ServiceError& e : d.value().recentErrors)
                toProto(e, o->add_recent_errors());
            o->set_resident_memory_bytes(d.value().residentMemoryBytes);
            o->set_uptime_seconds(d.value().uptimeSeconds);
            *o->mutable_time() = stamp(std::chrono::system_clock::now());
            return std::nullopt;
        });

    unary<v1::OpenSessionRequest, v1::OpenSessionResponse>(
        "SessionService", "OpenSession", Permission::Read, false, false,
        [this](const v1::OpenSessionRequest& in, v1::OpenSessionResponse& out, const CallContext& c) -> Failure {
            // A remote client may say it is an MCP adapter; it cannot claim to be the GUI or Python.
            services::Interface source = c.source;
            if (remote(source))
                source = in.source_interface() == v1::SOURCE_INTERFACE_MCP ? services::Interface::Mcp : services::Interface::Grpc;
            const std::uint32_t policy = remote(c.source) ? policy_ : services::kAllPermissions;
            auto s = session_.openSession(in.client_name(), source, permissionsOf(in.requested_permissions()), policy);
            if (!s)
                return s.error();
            sessionToProto(s.value(), out.mutable_session());
            return std::nullopt;
        });
    unary<v1::CloseSessionRequest, v1::CloseSessionResponse>(
        "SessionService", "CloseSession", Permission::Read, false, true,
        [this](const v1::CloseSessionRequest&, v1::CloseSessionResponse&, const CallContext& c) -> Failure {
            if (c.sessionId.empty())
                return services::invalid("give the session to close in the request header", "header.session_id");
            auto a = session_.authorize(c, Permission::Read);
            if (!a)
                return a.error();
            if (auto closed = session_.closeSession(a.value().sessionId); !closed)
                return closed.error();
            return std::nullopt;
        });
    unary<v1::ListSessionsRequest, v1::ListSessionsResponse>(
        "SessionService", "ListSessions", Permission::Read, false, true,
        [this](const v1::ListSessionsRequest&, v1::ListSessionsResponse& out, const CallContext& c) -> Failure {
            auto a = session_.authorize(c, Permission::Read);
            if (!a)
                return a.error();
            const bool admin = services::has(a.value().permissions, Permission::Admin);
            for (const services::SessionInfo& s : session_.sessions())
                if (admin || s.id == a.value().sessionId)
                    sessionToProto(s, out.add_sessions());
            return std::nullopt;
        });

    unary<v1::QueryAuditRequest, v1::QueryAuditResponse>(
        "AuditService", "QueryAudit", Permission::Read, false, true,
        [this](const v1::QueryAuditRequest& in, v1::QueryAuditResponse& out, const CallContext& c) -> Failure {
            services::AuditQuery q;
            q.sessionId = in.session_id();
            q.operation = in.operation();
            q.resourceId = in.resource_id();
            q.afterSequence = in.after_sequence();
            q.limit = in.limit() > 0 ? std::min(in.limit(), 1000) : 1000;
            q.failuresOnly = in.failures_only();
            auto records = session_.services().application.audit(c, q);
            if (!records)
                return records.error();
            for (const services::AuditRecord& r : records.value())
                toProto(r, out.add_records());
            return std::nullopt;
        });
}

// --- project, scene, geometry, materials ----------------------------------------------------------

void Dispatcher::registerProjectAndScene()
{
    const auto project = [this](const CallContext& c, v1::Project* out) -> Failure {
        auto p = session_.services().project.get(c);
        if (!p)
            return p.error();
        toProto(p.value(), out);
        return std::nullopt;
    };
    unary<v1::GetProjectRequest, v1::GetProjectResponse>("ProjectService", "GetProject", Permission::Read, false, true,
                                                         [project](const v1::GetProjectRequest&, v1::GetProjectResponse& out, const CallContext& c) {
                                                             return project(c, out.mutable_project());
                                                         });
    unary<v1::CreateProjectRequest, v1::CreateProjectResponse>(
        "ProjectService", "CreateProject", Permission::Modify, true, false,
        [this](const v1::CreateProjectRequest& in, v1::CreateProjectResponse& out, const CallContext& c) -> Failure {
            auto p = session_.services().project.create(c, in.name(), in.discard_unsaved_changes());
            if (!p)
                return p.error();
            toProto(p.value(), out.mutable_project());
            return std::nullopt;
        });
    unary<v1::OpenProjectRequest, v1::OpenProjectResponse>(
        "ProjectService", "OpenProject", Permission::Files, true, false,
        [this](const v1::OpenProjectRequest& in, v1::OpenProjectResponse& out, const CallContext& c) -> Failure {
            std::vector<std::string> warnings;
            auto p = session_.services().project.open(c, in.file_name(), in.discard_unsaved_changes(), &warnings);
            if (!p)
                return p.error();
            toProto(p.value(), out.mutable_project());
            for (const std::string& w : warnings)
                out.add_load_warnings(w);
            return std::nullopt;
        });
    unary<v1::SaveProjectRequest, v1::SaveProjectResponse>(
        "ProjectService", "SaveProject", Permission::Files, false, true,
        [this](const v1::SaveProjectRequest& in, v1::SaveProjectResponse& out, const CallContext& c) -> Failure {
            auto p = session_.services().project.save(c, in.file_name());
            if (!p)
                return p.error();
            toProto(p.value(), out.mutable_project());
            return std::nullopt;
        });
    unary<v1::ValidateProjectRequest, v1::ValidateProjectResponse>(
        "ProjectService", "ValidateProject", Permission::Read, false, true,
        [this](const v1::ValidateProjectRequest&, v1::ValidateProjectResponse& out, const CallContext& c) -> Failure {
            auto v = session_.services().project.validate(c);
            if (!v)
                return v.error();
            toProto(v.value(), out.mutable_validation());
            return std::nullopt;
        });
    unary<v1::UndoRequest, v1::UndoResponse>("ProjectService", "Undo", Permission::Modify, true, false,
                                             [this, project](const v1::UndoRequest&, v1::UndoResponse& out, const CallContext& c) -> Failure {
                                                 auto u = session_.services().project.undo(c);
                                                 if (!u)
                                                     return u.error();
                                                 out.set_undone(u.value());
                                                 return project(c, out.mutable_project());
                                             });
    unary<v1::RedoRequest, v1::RedoResponse>("ProjectService", "Redo", Permission::Modify, true, false,
                                             [this, project](const v1::RedoRequest&, v1::RedoResponse& out, const CallContext& c) -> Failure {
                                                 auto r = session_.services().project.redo(c);
                                                 if (!r)
                                                     return r.error();
                                                 out.set_redone(r.value());
                                                 return project(c, out.mutable_project());
                                             });
    unary<v1::ExportProjectJsonRequest, v1::ExportProjectJsonResponse>(
        "ProjectService", "ExportProjectJson", Permission::Read, false, true,
        [this](const v1::ExportProjectJsonRequest&, v1::ExportProjectJsonResponse& out, const CallContext& c) -> Failure {
            auto j = session_.services().project.exportJson(c);
            if (!j)
                return j.error();
            out.set_project_json(j.value());
            return std::nullopt;
        });

    // An object after a mutation (nothing for a dry run: it was not created here).
    const auto objectAfter = [this](const CallContext& c, const Mutation& m, const Id& id, v1::SceneObject* out) {
        if (m.dryRun || id.isNull())
            return;
        if (auto v = session_.services().scene.object(c, id))
            toProto(v.value(), out);
    };

    unary<v1::GetSceneSnapshotRequest, v1::GetSceneSnapshotResponse>(
        "SceneService", "GetSceneSnapshot", Permission::Read, false, true,
        [this](const v1::GetSceneSnapshotRequest&, v1::GetSceneSnapshotResponse& out, const CallContext& c) -> Failure {
            auto s = session_.services().scene.snapshot(c);
            if (!s)
                return s.error();
            out.mutable_snapshot()->set_project_id(s.value().projectId);
            out.mutable_snapshot()->set_project_revision(s.value().revision);
            out.mutable_snapshot()->set_event_sequence(s.value().eventSequence);
            for (const auto& o : s.value().objects)
                toProto(o, out.mutable_snapshot()->add_objects());
            return std::nullopt;
        });
    unary<v1::ListObjectsRequest, v1::ListObjectsResponse>(
        "SceneService", "ListObjects", Permission::Read, false, true,
        [this](const v1::ListObjectsRequest& in, v1::ListObjectsResponse& out, const CallContext& c) -> Failure {
            auto list = session_.services().scene.list(c, in.object_type(), in.parent_id());
            if (!list)
                return list.error();
            const int size = in.page().page_size() <= 0 ? 100 : std::min(in.page().page_size(), 1000);
            std::size_t offset = 0;
            if (!in.page().page_token().empty()) {
                try {
                    offset = std::stoul(in.page().page_token());
                } catch (const std::exception&) {
                    return services::invalid("the page token is not one this server issued", "page.page_token");
                }
            }
            const auto& objects = list.value();
            for (std::size_t k = offset; k < objects.size() && k < offset + static_cast<std::size_t>(size); ++k)
                toProto(objects[k], out.add_objects());
            if (offset + size < objects.size())
                out.mutable_page()->set_next_page_token(std::to_string(offset + size));
            out.mutable_page()->set_total_size(static_cast<int>(objects.size()));
            return std::nullopt;
        });
    unary<v1::GetObjectRequest, v1::GetObjectResponse>("SceneService", "GetObject", Permission::Read, false, true,
                                                       [this](const v1::GetObjectRequest& in, v1::GetObjectResponse& out, const CallContext& c) -> Failure {
                                                           auto v = session_.services().scene.object(c, Id(in.object_id()));
                                                           if (!v)
                                                               return v.error();
                                                           toProto(v.value(), out.mutable_object());
                                                           return std::nullopt;
                                                       });
    unary<v1::UpdateObjectRequest, v1::UpdateObjectResponse>(
        "SceneService", "UpdateObject", Permission::Modify, true, true,
        [this, objectAfter](const v1::UpdateObjectRequest& in, v1::UpdateObjectResponse& out, const CallContext& c) -> Failure {
            auto m = applyUpdateObject(session_.services(), c, in);
            if (!m)
                return m.error();
            detail::mutated(out.mutable_header(), m.value());
            toProto(m.value().changes, out.mutable_changes());
            objectAfter(c, m.value(), Id(in.object_id()), out.mutable_object());
            return std::nullopt;
        });
    unary<v1::DeleteObjectsRequest, v1::DeleteObjectsResponse>(
        "SceneService", "DeleteObjects", Permission::Modify, true, false,
        [this](const v1::DeleteObjectsRequest& in, v1::DeleteObjectsResponse& out, const CallContext& c) -> Failure {
            auto m = applyDeleteObjects(session_.services(), c, in);
            if (!m)
                return m.error();
            detail::mutated(out.mutable_header(), m.value());
            toProto(m.value().changes, out.mutable_changes());
            return std::nullopt;
        });
    unary<v1::DuplicateObjectsRequest, v1::DuplicateObjectsResponse>(
        "SceneService", "DuplicateObjects", Permission::Modify, true, false,
        [this, objectAfter](const v1::DuplicateObjectsRequest& in, v1::DuplicateObjectsResponse& out, const CallContext& c) -> Failure {
            auto m = session_.services().scene.duplicate(c, idsOf(in.object_ids()));
            if (!m)
                return m.error();
            detail::mutated(out.mutable_header(), m.value());
            toProto(m.value().changes, out.mutable_changes());
            if (!m.value().dryRun)
                for (const Id& id : m.value().changes.created)
                    objectAfter(c, m.value(), id, out.add_objects());
            return std::nullopt;
        });
    unary<v1::CreateGroupRequest, v1::CreateGroupResponse>(
        "SceneService", "CreateGroup", Permission::Modify, true, false,
        [this, objectAfter](const v1::CreateGroupRequest& in, v1::CreateGroupResponse& out, const CallContext& c) -> Failure {
            auto m = session_.services().scene.createGroup(c, in.name(), idOf(in.parent_id()), idsOf(in.member_ids()));
            if (!m)
                return m.error();
            detail::mutated(out.mutable_header(), m.value());
            toProto(m.value().changes, out.mutable_changes());
            if (!m.value().changes.created.empty())
                objectAfter(c, m.value(), m.value().changes.created.front(), out.mutable_group());
            return std::nullopt;
        });
    unary<v1::ValidateObjectRequest, v1::ValidateObjectResponse>(
        "SceneService", "ValidateObject", Permission::Read, false, true,
        [this](const v1::ValidateObjectRequest& in, v1::ValidateObjectResponse& out, const CallContext& c) -> Failure {
            auto v = session_.services().scene.validate(c, Id(in.object_id()));
            if (!v)
                return v.error();
            toProto(v.value(), out.mutable_validation());
            return std::nullopt;
        });

    unary<v1::ListGeometryKindsRequest, v1::ListGeometryKindsResponse>(
        "GeometryService", "ListGeometryKinds", Permission::Read, false, true,
        [this](const v1::ListGeometryKindsRequest&, v1::ListGeometryKindsResponse& out, const CallContext& c) -> Failure {
            auto kinds = session_.services().geometry.kinds(c);
            if (!kinds)
                return kinds.error();
            for (const auto& k : kinds.value()) {
                v1::GeometryKind* o = out.add_kinds();
                o->set_kind_id(k.id);
                o->set_display_name(k.name);
                o->set_category(k.category);
                for (const auto& p : k.parameters) {
                    v1::GeometryParameterDefinition* d = o->add_parameters();
                    d->set_key(p.key);
                    d->set_label(p.label);
                    d->set_unit(p.unit);
                    d->set_default_value(p.defaultValue);
                    d->set_min_value(p.minValue);
                    d->set_max_value(p.maxValue);
                    d->set_integer(p.integer);
                }
            }
            return std::nullopt;
        });
    unary<v1::CreatePrimitiveRequest, v1::CreatePrimitiveResponse>(
        "GeometryService", "CreatePrimitive", Permission::Modify, true, false,
        [this, objectAfter](const v1::CreatePrimitiveRequest& in, v1::CreatePrimitiveResponse& out, const CallContext& c) -> Failure {
            auto m = applyCreatePrimitive(session_.services(), c, in);
            if (!m)
                return m.error();
            detail::mutated(out.mutable_header(), m.value());
            toProto(m.value().changes, out.mutable_changes());
            if (!m.value().changes.created.empty())
                objectAfter(c, m.value(), m.value().changes.created.front(), out.mutable_object());
            return std::nullopt;
        });
    unary<v1::UpdateGeometryParametersRequest, v1::UpdateGeometryParametersResponse>(
        "GeometryService", "UpdateGeometryParameters", Permission::Modify, true, true,
        [this, objectAfter](const v1::UpdateGeometryParametersRequest& in, v1::UpdateGeometryParametersResponse& out, const CallContext& c) -> Failure {
            auto m = applyUpdateGeometryParameters(session_.services(), c, in);
            if (!m)
                return m.error();
            detail::mutated(out.mutable_header(), m.value());
            toProto(m.value().changes, out.mutable_changes());
            objectAfter(c, m.value(), Id(in.object_id()), out.mutable_object());
            return std::nullopt;
        });
    unary<v1::ValidateGeometryRequest, v1::ValidateGeometryResponse>(
        "GeometryService", "ValidateGeometry", Permission::Read, false, true,
        [this](const v1::ValidateGeometryRequest& in, v1::ValidateGeometryResponse& out, const CallContext& c) -> Failure {
            auto v = session_.services().geometry.validate(c, in.kind_id(), mapOf(in.parameters()));
            if (!v)
                return v.error();
            toProto(v.value(), out.mutable_validation());
            return std::nullopt;
        });

    unary<v1::ListMaterialsRequest, v1::ListMaterialsResponse>(
        "MaterialService", "ListMaterials", Permission::Read, false, true,
        [this](const v1::ListMaterialsRequest& in, v1::ListMaterialsResponse& out, const CallContext& c) -> Failure {
            auto list = session_.services().materials.list(c, in.domain());
            if (!list)
                return list.error();
            for (const nlohmann::json& m : list.value())
                toProto(m, out.add_materials());
            return std::nullopt;
        });
    unary<v1::GetMaterialRequest, v1::GetMaterialResponse>(
        "MaterialService", "GetMaterial", Permission::Read, false, true,
        [this](const v1::GetMaterialRequest& in, v1::GetMaterialResponse& out, const CallContext& c) -> Failure {
            auto m = session_.services().materials.get(c, Id(in.material_id()));
            if (!m)
                return m.error();
            toProto(m.value(), out.mutable_material());
            return std::nullopt;
        });
    unary<v1::CreateMaterialRequest, v1::CreateMaterialResponse>(
        "MaterialService", "CreateMaterial", Permission::Modify, true, false,
        [this](const v1::CreateMaterialRequest& in, v1::CreateMaterialResponse& out, const CallContext& c) -> Failure {
            nlohmann::json material;
            if (!in.material_json().empty()) {
                material = nlohmann::json::parse(in.material_json(), nullptr, false);
                if (material.is_discarded() || !material.is_object())
                    return services::invalid("material_json is not a JSON object", "material_json");
            } else {
                material = {{"name", in.name()}, {"description", in.description()}, {"domains", nlohmann::json::object()}};
                static const std::map<int, const char*> provenance = {{v1::MATERIAL_PROVENANCE_MEASURED, "measured"},
                                                                      {v1::MATERIAL_PROVENANCE_MANUFACTURER, "manufacturer"},
                                                                      {v1::MATERIAL_PROVENANCE_LITERATURE, "literature"},
                                                                      {v1::MATERIAL_PROVENANCE_THEORETICAL, "theoretical"},
                                                                      {v1::MATERIAL_PROVENANCE_ILLUSTRATIVE, "illustrative"},
                                                                      {v1::MATERIAL_PROVENANCE_HOMOGENIZED, "homogenized"},
                                                                      {v1::MATERIAL_PROVENANCE_COMPUTED, "computed"},
                                                                      {v1::MATERIAL_PROVENANCE_DATABASE, "database"},
                                                                      {v1::MATERIAL_PROVENANCE_FITTED, "fitted"},
                                                                      {v1::MATERIAL_PROVENANCE_SURROGATE, "surrogate"}};
                for (const v1::MaterialProperty& p : in.properties()) {
                    const auto it = provenance.find(p.provenance());
                    if (it == provenance.end())
                        return services::invalid("every property needs its provenance (measured, manufacturer, literature, theoretical, illustrative, homogenized, computed, database, fitted, surrogate)",
                                                 "properties.provenance");
                    material["domains"][p.domain()][p.key()] = {{"value", p.value_si()}, {"provenance", it->second}, {"source", p.source()}};
                }
            }
            if (!material.contains("id"))
                material["id"] = Id::generate().str();
            const std::string id = material["id"].is_string() ? material["id"].get<std::string>() : std::string();
            auto m = session_.services().materials.create(c, material);
            if (!m)
                return m.error();
            detail::mutated(out.mutable_header(), m.value());
            toProto(m.value().changes, out.mutable_changes());
            addMaterialRef(out.mutable_changes()->mutable_created(), id);
            if (!m.value().dryRun)
                if (auto created = session_.services().materials.get(c, Id(id)))
                    toProto(created.value(), out.mutable_material());
            return std::nullopt;
        });
    unary<v1::DeleteMaterialRequest, v1::DeleteMaterialResponse>(
        "MaterialService", "DeleteMaterial", Permission::Modify, true, false,
        [this](const v1::DeleteMaterialRequest& in, v1::DeleteMaterialResponse& out, const CallContext& c) -> Failure {
            auto m = session_.services().materials.remove(c, Id(in.material_id()));
            if (!m)
                return m.error();
            detail::mutated(out.mutable_header(), m.value());
            toProto(m.value().changes, out.mutable_changes());
            addMaterialRef(out.mutable_changes()->mutable_deleted(), in.material_id());
            return std::nullopt;
        });
    unary<v1::AssignMaterialRequest, v1::AssignMaterialResponse>(
        "MaterialService", "AssignMaterial", Permission::Modify, true, true,
        [this](const v1::AssignMaterialRequest& in, v1::AssignMaterialResponse& out, const CallContext& c) -> Failure {
            auto m = applyAssignMaterial(session_.services(), c, in);
            if (!m)
                return m.error();
            detail::mutated(out.mutable_header(), m.value());
            toProto(m.value().changes, out.mutable_changes());
            return std::nullopt;
        });
    unary<v1::RemoveMaterialAssignmentRequest, v1::RemoveMaterialAssignmentResponse>(
        "MaterialService", "RemoveMaterialAssignment", Permission::Modify, true, true,
        [this](const v1::RemoveMaterialAssignmentRequest& in, v1::RemoveMaterialAssignmentResponse& out, const CallContext& c) -> Failure {
            auto m = session_.services().materials.unassign(c, idsOf(in.object_ids()));
            if (!m)
                return m.error();
            detail::mutated(out.mutable_header(), m.value());
            toProto(m.value().changes, out.mutable_changes());
            return std::nullopt;
        });
}

// --- emitters, arrays, simulation, results, optimization -------------------------------------------

namespace {
void emitterToProto(const services::ObjectView& v, v1::Emitter* out)
{
    out->set_object_id(v.id.str());
    out->set_name(v.name);
    out->set_revision(v.revision);
    toProto(v.local, out->mutable_local_transform());
    toProto(v.worldPosition, out->mutable_world_position_meters());
    sourceToProto(v.component, out->mutable_source());
}

void planToProto(const services::SimulationService::Plan& p, v1::AcousticFieldPlan* out)
{
    out->set_allowed(p.estimate.allowed);
    out->set_refusal(p.estimate.refusal);
    out->set_sample_points(static_cast<std::int64_t>(p.estimate.points));
    out->set_memory_bytes(static_cast<std::int64_t>(p.estimate.bytes));
    out->set_source_evaluations(p.estimate.evaluations);
    out->set_active_sources(p.activeSources);
    out->set_model_id(p.modelId);
    out->set_frequency_hz(p.frequencyHz);
    for (const std::string& w : p.estimate.warnings)
        out->add_warnings(w);
}
}  // namespace

void Dispatcher::registerPhysics()
{
    const auto emitterAfter = [this](const CallContext& c, const Mutation& m, const Id& id, v1::Emitter* out) {
        if (m.dryRun || id.isNull())
            return;
        if (auto v = session_.services().emitters.get(c, id))
            emitterToProto(v.value(), out);
    };
    const auto arrayOf = [this](const CallContext& c, const Id& id, v1::AcousticArray* out) -> Failure {
        auto v = session_.services().arrays.get(c, id);
        if (!v)
            return v.error();
        const services::ObjectView& o = v.value();
        out->set_object_id(o.id.str());
        out->set_name(o.name);
        out->set_revision(o.revision);
        toProto(o.local, out->mutable_local_transform());
        out->set_layout(arrayLayoutOf(o.component.value("layout", std::string("linear"))));
        out->set_count_x(o.component.value("countX", 0));
        out->set_count_y(o.component.value("countY", 0));
        out->set_spacing_x_meters(o.component.value("spacingX", 0.0));
        out->set_spacing_y_meters(o.component.value("spacingY", 0.0));
        out->set_radius_meters(o.component.value("radius", 0.0));
        sourceToProto(o.component.value("element", nlohmann::json::object()), out->mutable_element_defaults());
        if (const SceneObject* object = session_.project().scene.find(id)) {
            int index = 0;
            for (const auto& [position, settings] : session_.services().arrays.elements(*object)) {
                v1::ArrayElement* e = out->add_elements();
                e->set_index(index++);
                toProto(position, e->mutable_local_position_meters());
                sourceToProto(settings, e->mutable_source());
            }
        }
        return std::nullopt;
    };

    unary<v1::CreateEmitterRequest, v1::CreateEmitterResponse>(
        "EmitterService", "CreateEmitter", Permission::Modify, true, false,
        [this, emitterAfter](const v1::CreateEmitterRequest& in, v1::CreateEmitterResponse& out, const CallContext& c) -> Failure {
            auto m = applyCreateEmitter(session_.services(), c, in);
            if (!m)
                return m.error();
            detail::mutated(out.mutable_header(), m.value());
            toProto(m.value().changes, out.mutable_changes());
            if (!m.value().changes.created.empty())
                emitterAfter(c, m.value(), m.value().changes.created.front(), out.mutable_emitter());
            return std::nullopt;
        });
    unary<v1::GetEmitterRequest, v1::GetEmitterResponse>("EmitterService", "GetEmitter", Permission::Read, false, true,
                                                         [this](const v1::GetEmitterRequest& in, v1::GetEmitterResponse& out, const CallContext& c) -> Failure {
                                                             auto v = session_.services().emitters.get(c, Id(in.object_id()));
                                                             if (!v)
                                                                 return v.error();
                                                             emitterToProto(v.value(), out.mutable_emitter());
                                                             return std::nullopt;
                                                         });
    unary<v1::ListEmittersRequest, v1::ListEmittersResponse>("EmitterService", "ListEmitters", Permission::Read, false, true,
                                                             [this](const v1::ListEmittersRequest&, v1::ListEmittersResponse& out, const CallContext& c) -> Failure {
                                                                 auto list = session_.services().emitters.list(c);
                                                                 if (!list)
                                                                     return list.error();
                                                                 for (const auto& v : list.value())
                                                                     emitterToProto(v, out.add_emitters());
                                                                 return std::nullopt;
                                                             });
    unary<v1::UpdateEmitterRequest, v1::UpdateEmitterResponse>(
        "EmitterService", "UpdateEmitter", Permission::Modify, true, true,
        [this, emitterAfter](const v1::UpdateEmitterRequest& in, v1::UpdateEmitterResponse& out, const CallContext& c) -> Failure {
            auto m = applyUpdateEmitter(session_.services(), c, in);
            if (!m)
                return m.error();
            detail::mutated(out.mutable_header(), m.value());
            toProto(m.value().changes, out.mutable_changes());
            emitterAfter(c, m.value(), Id(in.object_id()), out.mutable_emitter());
            return std::nullopt;
        });

    unary<v1::CreateArrayRequest, v1::CreateArrayResponse>(
        "ArrayService", "CreateArray", Permission::Modify, true, false,
        [this, arrayOf](const v1::CreateArrayRequest& in, v1::CreateArrayResponse& out, const CallContext& c) -> Failure {
            auto m = applyCreateArray(session_.services(), c, in);
            if (!m)
                return m.error();
            detail::mutated(out.mutable_header(), m.value());
            toProto(m.value().changes, out.mutable_changes());
            if (!m.value().dryRun && !m.value().changes.created.empty())
                arrayOf(c, m.value().changes.created.front(), out.mutable_array());
            return std::nullopt;
        });
    unary<v1::GetArrayRequest, v1::GetArrayResponse>("ArrayService", "GetArray", Permission::Read, false, true,
                                                     [arrayOf](const v1::GetArrayRequest& in, v1::GetArrayResponse& out, const CallContext& c) -> Failure {
                                                         return arrayOf(c, Id(in.object_id()), out.mutable_array());
                                                     });
    unary<v1::SetArrayExcitationRequest, v1::SetArrayExcitationResponse>(
        "ArrayService", "SetArrayExcitation", Permission::Modify, true, true,
        [this, arrayOf](const v1::SetArrayExcitationRequest& in, v1::SetArrayExcitationResponse& out, const CallContext& c) -> Failure {
            auto m = applySetArrayExcitation(session_.services(), c, in);
            if (!m)
                return m.error();
            detail::mutated(out.mutable_header(), m.value());
            toProto(m.value().changes, out.mutable_changes());
            if (!m.value().dryRun)
                arrayOf(c, Id(in.object_id()), out.mutable_array());
            return std::nullopt;
        });
    unary<v1::FocusArrayRequest, v1::FocusArrayResponse>(
        "ArrayService", "FocusArray", Permission::Modify, true, true,
        [this, arrayOf](const v1::FocusArrayRequest& in, v1::FocusArrayResponse& out, const CallContext& c) -> Failure {
            auto m = applyFocusArray(session_.services(), c, in);
            if (!m)
                return m.error();
            detail::mutated(out.mutable_header(), m.value());
            toProto(m.value().changes, out.mutable_changes());
            if (!m.value().dryRun)
                arrayOf(c, Id(in.object_id()), out.mutable_array());
            return std::nullopt;
        });

    unary<v1::ListSolversRequest, v1::ListSolversResponse>("SimulationService", "ListSolvers", Permission::Read, false, true,
                                                           [this](const v1::ListSolversRequest&, v1::ListSolversResponse& out, const CallContext& c) -> Failure {
                                                               auto list = session_.services().simulation.solvers(c);
                                                               if (!list)
                                                                   return list.error();
                                                               for (const auto& s : list.value()) {
                                                                   v1::Solver* o = out.add_solvers();
                                                                   o->set_solver_id(s.id);
                                                                   o->set_display_name(s.name);
                                                                   o->set_physics_domain(s.domain);
                                                                   o->set_method(s.method);
                                                                   o->set_available(s.available);
                                                                   o->set_startable_through_api(s.startable);
                                                                   for (const auto& a : s.assumptions)
                                                                       o->add_assumptions(a);
                                                               }
                                                               return std::nullopt;
                                                           });
    unary<v1::CreateFieldRegionRequest, v1::CreateFieldRegionResponse>(
        "SimulationService", "CreateFieldRegion", Permission::Modify, true, false,
        [this](const v1::CreateFieldRegionRequest& in, v1::CreateFieldRegionResponse& out, const CallContext& c) -> Failure {
            auto m = applyCreateFieldRegion(session_.services(), c, in);
            if (!m)
                return m.error();
            detail::mutated(out.mutable_header(), m.value());
            toProto(m.value().changes, out.mutable_changes());
            if (!m.value().dryRun && !m.value().changes.created.empty())
                if (auto v = session_.services().scene.object(c, m.value().changes.created.front()))
                    toProto(v.value(), out.mutable_region());
            return std::nullopt;
        });
    unary<v1::EstimateAcousticFieldRequest, v1::EstimateAcousticFieldResponse>(
        "SimulationService", "EstimateAcousticField", Permission::Read, false, true,
        [this](const v1::EstimateAcousticFieldRequest& in, v1::EstimateAcousticFieldResponse& out, const CallContext& c) -> Failure {
            auto p = session_.services().simulation.estimateAcousticField(c, Id(in.region_id()));
            if (!p)
                return p.error();
            planToProto(p.value(), out.mutable_plan());
            return std::nullopt;
        });
    unary<v1::StartAcousticFieldSimulationRequest, v1::StartAcousticFieldSimulationResponse>(
        "SimulationService", "StartAcousticFieldSimulation", Permission::Execute, false, false,
        [this](const v1::StartAcousticFieldSimulationRequest& in, v1::StartAcousticFieldSimulationResponse& out, const CallContext& c) -> Failure {
            auto s = session_.services().simulation.startAcousticField(c, Id(in.region_id()));
            if (!s)
                return s.error();
            toProto(s.value().job, out.mutable_job());
            planToProto(s.value().plan, out.mutable_plan());
            return std::nullopt;
        });

    unary<v1::ListResultsRequest, v1::ListResultsResponse>("ResultsService", "ListResults", Permission::Read, false, true,
                                                           [this](const v1::ListResultsRequest&, v1::ListResultsResponse& out, const CallContext& c) -> Failure {
                                                               auto list = session_.services().results.list(c);
                                                               if (!list)
                                                                   return list.error();
                                                               for (const services::ResultEntry* r : list.value())
                                                                   toProto(*r, 0, out.add_results());
                                                               return std::nullopt;
                                                           });
    unary<v1::GetResultRequest, v1::GetResultResponse>("ResultsService", "GetResult", Permission::Read, false, true,
                                                       [this](const v1::GetResultRequest& in, v1::GetResultResponse& out, const CallContext& c) -> Failure {
                                                           auto r = session_.services().results.get(c, in.result_id());
                                                           if (!r)
                                                               return r.error();
                                                           toProto(*r.value(), 0, out.mutable_result());
                                                           return std::nullopt;
                                                       });
    unary<v1::ReadFieldSamplesRequest, v1::ReadFieldSamplesResponse>(
        "ResultsService", "ReadFieldSamples", Permission::Read, false, true,
        [this](const v1::ReadFieldSamplesRequest& in, v1::ReadFieldSamplesResponse& out, const CallContext& c) -> Failure {
            std::vector<Vec3> points;
            for (const auto& p : in.points_world_meters())
                points.push_back(fromProto(p));
            auto samples = session_.services().results.samples(c, in.result_id(), points);
            if (!samples)
                return samples.error();
            for (const auto& s : samples.value()) {
                v1::FieldSample* o = out.add_samples();
                toProto(s.point, o->mutable_point_world_meters());
                o->set_inside(s.inside);
                if (s.inside) {
                    o->mutable_value()->set_real(s.value.real());
                    o->mutable_value()->set_imag(s.value.imag());
                    o->set_magnitude(std::abs(s.value));
                }
            }
            out.set_unit("Pa");
            return std::nullopt;
        });

    unary<v1::ListOptimizationProblemsRequest, v1::ListOptimizationProblemsResponse>(
        "OptimizationService", "ListOptimizationProblems", Permission::Read, false, true,
        [this](const v1::ListOptimizationProblemsRequest&, v1::ListOptimizationProblemsResponse& out, const CallContext& c) -> Failure {
            auto list = session_.services().optimization.problems(c);
            if (!list)
                return list.error();
            for (const nlohmann::json& p : list.value()) {
                v1::OptimizationProblemSummary* o = out.add_problems();
                o->set_problem_id(p.value("id", std::string()));
                o->set_name(p.value("name", std::string()));
                o->set_algorithm(p.value("algorithm", std::string()));
                o->set_variable_count(p.value("variables", 0));
                o->set_objective_count(p.value("objectives", 0));
                o->set_constraint_count(p.value("constraints", 0));
            }
            return std::nullopt;
        });
    unary<v1::ListOptimizationRunsRequest, v1::ListOptimizationRunsResponse>(
        "OptimizationService", "ListOptimizationRuns", Permission::Read, false, true,
        [this](const v1::ListOptimizationRunsRequest&, v1::ListOptimizationRunsResponse& out, const CallContext& c) -> Failure {
            auto list = session_.services().optimization.runs(c);
            if (!list)
                return list.error();
            for (const nlohmann::json& r : list.value()) {
                v1::OptimizationRunSummary* o = out.add_runs();
                o->set_run_id(r.value("id", std::string()));
                o->set_problem_id(r.value("problem", std::string()));
                o->set_algorithm(r.value("algorithm", std::string()));
                o->set_candidate_count(r.value("candidates", 0));
                o->set_termination(r.value("termination", std::string()));
                o->set_best_candidate_id(r.value("best", std::string()));
                for (const auto& v : r.value("bestValues", nlohmann::json::array()))
                    o->add_best_objective_values(v.get<double>());
            }
            return std::nullopt;
        });
    unary<v1::StartOptimizationRequest, v1::StartOptimizationResponse>(
        "OptimizationService", "StartOptimization", Permission::Execute, false, false,
        [this](const v1::StartOptimizationRequest& in, v1::StartOptimizationResponse& out, const CallContext& c) -> Failure {
            auto job = session_.services().optimization.start(c, in.problem_id(), in.max_evaluations());
            if (!job)
                return job.error();
            toProto(job.value(), out.mutable_job());
            return std::nullopt;
        });
}

// --- jobs, workflows, transactions, events ----------------------------------------------------------

void Dispatcher::registerAutomation()
{
    unary<v1::GetJobRequest, v1::GetJobResponse>("JobService", "GetJob", Permission::Read, false, true,
                                                 [this](const v1::GetJobRequest& in, v1::GetJobResponse& out, const CallContext& c) -> Failure {
                                                     auto j = session_.services().jobs.get(c, in.job_id());
                                                     if (!j)
                                                         return j.error();
                                                     toProto(j.value(), out.mutable_job());
                                                     return std::nullopt;
                                                 });
    unary<v1::ListJobsRequest, v1::ListJobsResponse>("JobService", "ListJobs", Permission::Read, false, true,
                                                     [this](const v1::ListJobsRequest& in, v1::ListJobsResponse& out, const CallContext& c) -> Failure {
                                                         auto list = session_.services().jobs.list(c, in.active_only());
                                                         if (!list)
                                                             return list.error();
                                                         for (const auto& j : list.value())
                                                             toProto(j, out.add_jobs());
                                                         return std::nullopt;
                                                     });
    unary<v1::CancelJobRequest, v1::CancelJobResponse>("JobService", "CancelJob", Permission::Execute, false, true,
                                                       [this](const v1::CancelJobRequest& in, v1::CancelJobResponse& out, const CallContext& c) -> Failure {
                                                           auto j = session_.services().jobs.cancel(c, in.job_id());
                                                           if (!j)
                                                               return j.error();
                                                           toProto(j.value(), out.mutable_job());
                                                           return std::nullopt;
                                                       });
    blocking<v1::WaitForJobRequest, v1::WaitForJobResponse>(
        "JobService", "WaitForJob", Permission::Read,
        [this](const v1::WaitForJobRequest& in, v1::WaitForJobResponse& out, const CallContext& c) -> Failure {
            const double timeout = std::clamp(in.timeout_seconds() > 0 ? in.timeout_seconds() : 30.0, 0.0, kMaxWaitSeconds);
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::duration<double>(timeout);
            for (;;) {
                Outcome<services::JobView> job = services::JobView{};
                bool handled = false;
                run_([&] {
                    job = session_.services().jobs.get(c, in.job_id());
                    const services::JobRecord* r = session_.jobRecord(in.job_id());
                    handled = !r || r->handled;  // a tracked job is finished once its results are collected
                });
                if (!job)
                    return job.error();
                const std::string& state = job.value().state;
                const bool finished = (state == "completed" || state == "failed" || state == "cancelled") && handled;
                if (finished || std::chrono::steady_clock::now() >= deadline) {
                    toProto(job.value(), out.mutable_job());
                    out.set_finished(finished);
                    return std::nullopt;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        });

    unary<v1::ListWorkflowTemplatesRequest, v1::ListWorkflowTemplatesResponse>(
        "WorkflowService", "ListWorkflowTemplates", Permission::Read, false, true,
        [this](const v1::ListWorkflowTemplatesRequest&, v1::ListWorkflowTemplatesResponse& out, const CallContext& c) -> Failure {
            auto list = session_.services().workflows.templates(c);
            if (!list)
                return list.error();
            for (const nlohmann::json& w : list.value()) {
                v1::WorkflowTemplate* o = out.add_workflows();
                o->set_workflow_id(w.value("id", std::string()));
                o->set_name(w.value("name", std::string()));
                o->set_description(w.value("description", std::string()));
                o->set_category(w.value("category", std::string()));
                o->set_source(w.value("source", std::string()));
                o->set_step_count(w.value("steps", 0));
                if (Failure f = structOf(w.value("definition", nlohmann::json::object()), o->mutable_definition()))
                    return f;
            }
            return std::nullopt;
        });
    unary<v1::ExecuteWorkflowRequest, v1::ExecuteWorkflowResponse>(
        "WorkflowService", "ExecuteWorkflow", Permission::Execute, false, false,
        [this](const v1::ExecuteWorkflowRequest& in, v1::ExecuteWorkflowResponse& out, const CallContext& c) -> Failure {
            const nlohmann::json definition = in.has_definition() ? jsonOf(in.definition()) : nlohmann::json();
            const nlohmann::json parameters = in.has_parameters() ? jsonOf(in.parameters()) : nlohmann::json();
            auto job = session_.services().workflows.execute(c, in.workflow_id(), definition, parameters, in.apply_changes());
            if (!job)
                return job.error();
            toProto(job.value(), out.mutable_job());
            return std::nullopt;
        });
    unary<v1::GetWorkflowRunRequest, v1::GetWorkflowRunResponse>(
        "WorkflowService", "GetWorkflowRun", Permission::Read, false, true,
        [this](const v1::GetWorkflowRunRequest& in, v1::GetWorkflowRunResponse& out, const CallContext& c) -> Failure {
            auto run = session_.services().workflows.run(c, in.job_id());
            if (!run)
                return run.error();
            out.set_finished(run.value().second);
            return structOf(run.value().first, out.mutable_run());
        });

    unary<v1::ExecuteBatchRequest, v1::ExecuteBatchResponse>(
        "TransactionService", "ExecuteBatch", Permission::Modify, true, false,
        [this](const v1::ExecuteBatchRequest& in, v1::ExecuteBatchResponse& out, const CallContext& c) -> Failure {
            auto r = session_.services().transactions.executeBatch(c, operationsOf(in.operations()), in.description());
            if (!r)
                return r.error();
            out.set_committed(r.value().committed);
            resultsToProto(r.value().results, out.mutable_results());
            toProto(r.value().mutation.changes, out.mutable_changes());
            detail::mutated(out.mutable_header(), r.value().mutation);
            return std::nullopt;
        });
    unary<v1::BeginTransactionRequest, v1::BeginTransactionResponse>(
        "TransactionService", "BeginTransaction", Permission::Modify, false, false,
        [this](const v1::BeginTransactionRequest& in, v1::BeginTransactionResponse& out, const CallContext& c) -> Failure {
            auto t = session_.services().transactions.begin(c, in.description());
            if (!t)
                return t.error();
            transactionToProto(t.value(), out.mutable_transaction());
            return std::nullopt;
        });
    unary<v1::StageOperationsRequest, v1::StageOperationsResponse>(
        "TransactionService", "StageOperations", Permission::Modify, false, false,
        [this](const v1::StageOperationsRequest& in, v1::StageOperationsResponse& out, const CallContext& c) -> Failure {
            auto t = session_.services().transactions.stage(c, in.transaction_id(), operationsOf(in.operations()));
            if (!t)
                return t.error();
            transactionToProto(t.value(), out.mutable_transaction());
            return std::nullopt;
        });
    unary<v1::ValidateTransactionRequest, v1::ValidateTransactionResponse>(
        "TransactionService", "ValidateTransaction", Permission::Modify, false, true,
        [this](const v1::ValidateTransactionRequest& in, v1::ValidateTransactionResponse& out, const CallContext& c) -> Failure {
            auto t = session_.services().transactions.validate(c, in.transaction_id());
            if (!t)
                return t.error();
            transactionToProto(t.value().first, out.mutable_transaction());
            resultsToProto(t.value().second, out.mutable_results());
            return std::nullopt;
        });
    unary<v1::CommitTransactionRequest, v1::CommitTransactionResponse>(
        "TransactionService", "CommitTransaction", Permission::Modify, true, false,
        [this](const v1::CommitTransactionRequest& in, v1::CommitTransactionResponse& out, const CallContext& c) -> Failure {
            auto t = session_.services().transactions.commit(c, in.transaction_id());
            if (!t)
                return t.error();
            transactionToProto(t.value().first, out.mutable_transaction());
            resultsToProto(t.value().second.results, out.mutable_results());
            toProto(t.value().second.mutation.changes, out.mutable_changes());
            detail::mutated(out.mutable_header(), t.value().second.mutation);
            return std::nullopt;
        });
    unary<v1::AbortTransactionRequest, v1::AbortTransactionResponse>(
        "TransactionService", "AbortTransaction", Permission::Modify, false, true,
        [this](const v1::AbortTransactionRequest& in, v1::AbortTransactionResponse& out, const CallContext& c) -> Failure {
            auto t = session_.services().transactions.abort(c, in.transaction_id());
            if (!t)
                return t.error();
            transactionToProto(t.value(), out.mutable_transaction());
            return std::nullopt;
        });
    unary<v1::GetTransactionRequest, v1::GetTransactionResponse>(
        "TransactionService", "GetTransaction", Permission::Read, false, true,
        [this](const v1::GetTransactionRequest& in, v1::GetTransactionResponse& out, const CallContext& c) -> Failure {
            auto t = session_.services().transactions.get(c, in.transaction_id());
            if (!t)
                return t.error();
            transactionToProto(t.value(), out.mutable_transaction());
            return std::nullopt;
        });

    // --- the Coupled Energy-Matter Engine (REQ-CHAT-20261126-016) ---
    unary<v1::ListCouplingModelsRequest, v1::ListCouplingModelsResponse>(
        "MultiphysicsService", "ListCouplingModels", Permission::Read, false, true,
        [this](const v1::ListCouplingModelsRequest&, v1::ListCouplingModelsResponse& out, const CallContext& c) -> Failure {
            auto models = session_.services().multiphysics.couplingModels(c);
            if (!models)
                return models.error();
            for (const auto& m : models.value()) {
                v1::CouplingModel* o = out.add_models();
                o->set_model_id(m.id);
                o->set_name(m.name);
                o->set_source_domain(m.sourceDomain);
                o->set_target_domain(m.targetDomain);
                o->set_kind(m.kind);
                o->set_source_quantity(m.sourceQuantity);
                o->set_target_quantity(m.targetQuantity);
                o->set_default_transfer(m.defaultTransfer);
                o->set_mechanism(m.mechanism);
                o->set_equations(m.equations);
                o->set_status(m.status);
                o->set_validation(m.validation);
                o->set_limitations(m.limitations);
            }
            return std::nullopt;
        });
    unary<v1::ListPhysicalSystemsRequest, v1::ListPhysicalSystemsResponse>(
        "MultiphysicsService", "ListPhysicalSystems", Permission::Read, false, true,
        [this](const v1::ListPhysicalSystemsRequest&, v1::ListPhysicalSystemsResponse& out, const CallContext& c) -> Failure {
            auto list = session_.services().multiphysics.systems(c);
            if (!list)
                return list.error();
            for (const nlohmann::json& d : list.value())
                systemToProto(d, out.add_systems());
            return std::nullopt;
        });
    unary<v1::GetPhysicalSystemRequest, v1::GetPhysicalSystemResponse>(
        "MultiphysicsService", "GetPhysicalSystem", Permission::Read, false, true,
        [this](const v1::GetPhysicalSystemRequest& in, v1::GetPhysicalSystemResponse& out, const CallContext& c) -> Failure {
            auto d = session_.services().multiphysics.system(c, in.system_id());
            if (!d)
                return d.error();
            systemToProto(d.value(), out.mutable_system());
            return std::nullopt;
        });
    unary<v1::SetPhysicalSystemRequest, v1::SetPhysicalSystemResponse>(
        "MultiphysicsService", "SetPhysicalSystem", Permission::Modify, true, true,
        [this](const v1::SetPhysicalSystemRequest& in, v1::SetPhysicalSystemResponse& out, const CallContext& c) -> Failure {
            const nlohmann::json definition = in.has_definition() ? jsonOf(in.definition()) : nlohmann::json::object();
            auto m = session_.services().multiphysics.setSystem(c, definition);
            if (!m)
                return m.error();
            detail::mutated(out.mutable_header(), m.value());
            toProto(m.value().changes, out.mutable_changes());
            if (auto stored = session_.services().multiphysics.system(c, definition.value("id", std::string())))
                systemToProto(stored.value(), out.mutable_system());
            return std::nullopt;
        });
    unary<v1::DeletePhysicalSystemRequest, v1::DeletePhysicalSystemResponse>(
        "MultiphysicsService", "DeletePhysicalSystem", Permission::Modify, true, false,
        [this](const v1::DeletePhysicalSystemRequest& in, v1::DeletePhysicalSystemResponse& out, const CallContext& c) -> Failure {
            auto m = session_.services().multiphysics.deleteSystem(c, in.system_id());
            if (!m)
                return m.error();
            detail::mutated(out.mutable_header(), m.value());
            toProto(m.value().changes, out.mutable_changes());
            return std::nullopt;
        });
    unary<v1::ValidatePhysicalSystemRequest, v1::ValidatePhysicalSystemResponse>(
        "MultiphysicsService", "ValidatePhysicalSystem", Permission::Read, false, true,
        [this](const v1::ValidatePhysicalSystemRequest& in, v1::ValidatePhysicalSystemResponse& out, const CallContext& c) -> Failure {
            auto v = session_.services().multiphysics.validate(c, in.system_id(), in.has_definition() ? jsonOf(in.definition()) : nlohmann::json());
            if (!v)
                return v.error();
            toProto(v.value(), out.mutable_validation());
            return std::nullopt;
        });
    unary<v1::StartCoupledSimulationRequest, v1::StartCoupledSimulationResponse>(
        "MultiphysicsService", "StartCoupledSimulation", Permission::Execute, false, false,
        [this](const v1::StartCoupledSimulationRequest& in, v1::StartCoupledSimulationResponse& out, const CallContext& c) -> Failure {
            auto job = session_.services().multiphysics.start(c, in.system_id(), in.has_definition() ? jsonOf(in.definition()) : nlohmann::json());
            if (!job)
                return job.error();
            toProto(job.value(), out.mutable_job());
            return std::nullopt;
        });
    unary<v1::GetCoupledResultRequest, v1::GetCoupledResultResponse>(
        "MultiphysicsService", "GetCoupledResult", Permission::Read, false, true,
        [this](const v1::GetCoupledResultRequest& in, v1::GetCoupledResultResponse& out, const CallContext& c) -> Failure {
            auto r = session_.services().multiphysics.result(c, in.result_id());
            if (!r)
                return r.error();
            return coupledResultToProto(in.result_id(), *r.value(), in.include_document(), out.mutable_result());
        });
    unary<v1::ListAnalysisKindsRequest, v1::ListAnalysisKindsResponse>(
        "FunctionalStructureService", "ListAnalysisKinds", Permission::Read, false, true,
        [this](const v1::ListAnalysisKindsRequest&, v1::ListAnalysisKindsResponse& out, const CallContext& c) -> Failure {
            auto kinds = session_.services().functional.analysisKinds(c);
            if (!kinds)
                return kinds.error();
            for (const auto& k : kinds.value()) {
                auto* x = out.add_kinds();
                x->set_kind_id(k.value("id", std::string()));
                x->set_title(k.value("title", std::string()));
                x->set_description(k.value("description", std::string()));
                for (const auto& d : k.value("domains", nlohmann::json::array()))
                    x->add_domains(d.get<std::string>());
                x->set_category(k.value("category", std::string()));
                x->set_mechanism(k.value("mechanism", std::string()));
                for (const auto& m : k.value("models", nlohmann::json::array())) {
                    auto* y = x->add_models();
                    y->set_model_id(m.value("id", std::string()));
                    y->set_name(m.value("name", std::string()));
                    y->set_version(m.value("version", std::string()));
                    y->set_fidelity(m.value("fidelity", std::string()));
                    for (const auto& a : m.value("assumptions", nlohmann::json::array()))
                        y->add_assumptions(a.get<std::string>());
                    y->set_validity(m.value("validity", std::string()));
                }
                for (const auto& m : k.value("metrics", nlohmann::json::array())) {
                    auto* y = x->add_metrics();
                    y->set_key(m.value("key", std::string()));
                    y->set_label(m.value("label", std::string()));
                    y->set_unit(m.value("unit", std::string()));
                    y->set_definition(m.value("definition", std::string()));
                }
            }
            return std::nullopt;
        });
    unary<v1::ListFunctionalStructuresRequest, v1::ListFunctionalStructuresResponse>(
        "FunctionalStructureService", "ListFunctionalStructures", Permission::Read, false, true,
        [this](const v1::ListFunctionalStructuresRequest&, v1::ListFunctionalStructuresResponse& out, const CallContext& c) -> Failure {
            auto list = session_.services().functional.structures(c);
            if (!list)
                return list.error();
            for (const auto& d : list.value())
                structureToProto(d, out.add_structures());
            return std::nullopt;
        });
    unary<v1::GetFunctionalStructureRequest, v1::GetFunctionalStructureResponse>(
        "FunctionalStructureService", "GetFunctionalStructure", Permission::Read, false, true,
        [this](const v1::GetFunctionalStructureRequest& in, v1::GetFunctionalStructureResponse& out, const CallContext& c) -> Failure {
            auto d = session_.services().functional.structure(c, in.structure_id());
            if (!d)
                return d.error();
            structureToProto(d.value(), out.mutable_structure());
            return std::nullopt;
        });
    unary<v1::SetFunctionalStructureRequest, v1::SetFunctionalStructureResponse>(
        "FunctionalStructureService", "SetFunctionalStructure", Permission::Modify, true, true,
        [this](const v1::SetFunctionalStructureRequest& in, v1::SetFunctionalStructureResponse& out, const CallContext& c) -> Failure {
            const nlohmann::json definition = in.has_definition() ? jsonOf(in.definition()) : nlohmann::json::object();
            auto m = session_.services().functional.setStructure(c, definition);
            if (!m)
                return m.error();
            detail::mutated(out.mutable_header(), m.value());
            toProto(m.value().changes, out.mutable_changes());
            if (auto stored = session_.services().functional.structure(c, definition.value("id", std::string())))
                structureToProto(stored.value(), out.mutable_structure());
            return std::nullopt;
        });
    unary<v1::DeleteFunctionalStructureRequest, v1::DeleteFunctionalStructureResponse>(
        "FunctionalStructureService", "DeleteFunctionalStructure", Permission::Modify, true, false,
        [this](const v1::DeleteFunctionalStructureRequest& in, v1::DeleteFunctionalStructureResponse& out, const CallContext& c) -> Failure {
            auto m = session_.services().functional.deleteStructure(c, in.structure_id());
            if (!m)
                return m.error();
            detail::mutated(out.mutable_header(), m.value());
            toProto(m.value().changes, out.mutable_changes());
            return std::nullopt;
        });
    unary<v1::GetStructureParametersRequest, v1::GetStructureParametersResponse>(
        "FunctionalStructureService", "GetStructureParameters", Permission::Read, false, true,
        [this](const v1::GetStructureParametersRequest& in, v1::GetStructureParametersResponse& out, const CallContext& c) -> Failure {
            auto p = session_.services().functional.parameters(c, in.structure_id());
            if (!p)
                return p.error();
            parametersToProto(p.value(), out.mutable_parameters());
            return std::nullopt;
        });
    unary<v1::SetStructureParametersRequest, v1::SetStructureParametersResponse>(
        "FunctionalStructureService", "SetStructureParameters", Permission::Modify, true, true,
        [this](const v1::SetStructureParametersRequest& in, v1::SetStructureParametersResponse& out, const CallContext& c) -> Failure {
            std::map<std::string, double, std::less<>> values(in.values().begin(), in.values().end());
            const std::optional<std::uint64_t> expected = in.expected_revision() ? std::optional<std::uint64_t>(in.expected_revision()) : std::nullopt;
            auto m = session_.services().functional.setParameters(c, in.structure_id(), values, expected);
            if (!m)
                return m.error();
            detail::mutated(out.mutable_header(), m.value());
            toProto(m.value().changes, out.mutable_changes());
            if (auto p = session_.services().functional.parameters(c, in.structure_id()))
                parametersToProto(p.value(), out.mutable_parameters());
            return std::nullopt;
        });
    unary<v1::ValidateFunctionalStructureRequest, v1::ValidateFunctionalStructureResponse>(
        "FunctionalStructureService", "ValidateFunctionalStructure", Permission::Read, false, true,
        [this](const v1::ValidateFunctionalStructureRequest& in, v1::ValidateFunctionalStructureResponse& out, const CallContext& c) -> Failure {
            auto v = session_.services().functional.validate(c, in.structure_id(), in.has_definition() ? jsonOf(in.definition()) : nlohmann::json());
            if (!v)
                return v.error();
            toProto(v.value(), out.mutable_validation());
            return std::nullopt;
        });
    unary<v1::AnalyzeStructureRequest, v1::AnalyzeStructureResponse>(
        "FunctionalStructureService", "AnalyzeStructure", Permission::Execute, false, false,
        [this](const v1::AnalyzeStructureRequest& in, v1::AnalyzeStructureResponse& out, const CallContext& c) -> Failure {
            auto job = session_.services().functional.analyze(c, in.structure_id(), in.model_id());
            if (!job)
                return job.error();
            toProto(job.value(), out.mutable_job());
            return std::nullopt;
        });
    unary<v1::SweepStructureRequest, v1::SweepStructureResponse>(
        "FunctionalStructureService", "SweepStructure", Permission::Execute, false, false,
        [this](const v1::SweepStructureRequest& in, v1::SweepStructureResponse& out, const CallContext& c) -> Failure {
            nlohmann::json spec{{"axes", nlohmann::json::array()}, {"model", in.model_id()}};
            for (const auto& a : in.axes()) {
                nlohmann::json axis{{"parameter", a.parameter_id()}};
                if (a.values_size() > 0)
                    axis["values"] = std::vector<double>(a.values().begin(), a.values().end());
                else
                    axis.update({{"from", a.first()}, {"to", a.last()}, {"points", a.points() > 0 ? a.points() : 11}});
                spec["axes"].push_back(std::move(axis));
            }
            auto job = session_.services().functional.sweep(c, in.structure_id(), spec);
            if (!job)
                return job.error();
            toProto(job.value(), out.mutable_job());
            return std::nullopt;
        });
    unary<v1::ComputeStructureSensitivityRequest, v1::ComputeStructureSensitivityResponse>(
        "FunctionalStructureService", "ComputeStructureSensitivity", Permission::Execute, false, false,
        [this](const v1::ComputeStructureSensitivityRequest& in, v1::ComputeStructureSensitivityResponse& out, const CallContext& c) -> Failure {
            const nlohmann::json spec{{"parameters", std::vector<std::string>(in.parameter_ids().begin(), in.parameter_ids().end())},
                                      {"relativeStep", in.relative_step() > 0.0 ? in.relative_step() : 1e-3},
                                      {"model", in.model_id()}};
            auto job = session_.services().functional.sensitivity(c, in.structure_id(), spec);
            if (!job)
                return job.error();
            toProto(job.value(), out.mutable_job());
            return std::nullopt;
        });
    unary<v1::GetFunctionalResultRequest, v1::GetFunctionalResultResponse>(
        "FunctionalStructureService", "GetFunctionalResult", Permission::Read, false, true,
        [this](const v1::GetFunctionalResultRequest& in, v1::GetFunctionalResultResponse& out, const CallContext& c) -> Failure {
            auto r = session_.services().functional.result(c, in.result_id());
            if (!r)
                return r.error();
            return functionalResultToProto(r.value(), in.include_document(), out.mutable_result());
        });
    unary<v1::OptimizeStructureRequest, v1::OptimizeStructureResponse>(
        "FunctionalStructureService", "OptimizeStructure", Permission::Execute, false, false,
        [this](const v1::OptimizeStructureRequest& in, v1::OptimizeStructureResponse& out, const CallContext& c) -> Failure {
            nlohmann::json spec = nlohmann::json::object();
            if (!in.algorithm().empty())
                spec["algorithm"] = in.algorithm();
            if (in.max_evaluations() > 0)
                spec["maxEvaluations"] = in.max_evaluations();
            if (!in.model_id().empty())
                spec["model"] = in.model_id();
            if (in.seed() > 0)
                spec["seed"] = in.seed();
            auto job = session_.services().functional.optimize(c, in.structure_id(), spec);
            if (!job)
                return job.error();
            toProto(job.value(), out.mutable_job());
            return std::nullopt;
        });
    unary<v1::IdentifyResonancesRequest, v1::IdentifyResonancesResponse>(
        "ResonanceService", "IdentifyResonances", Permission::Read, false, true,
        [this](const v1::IdentifyResonancesRequest& in, v1::IdentifyResonancesResponse& out, const CallContext& c) -> Failure {
            nlohmann::json request = nlohmann::json::object();
            if (in.samples_size() > 0) {
                request["samples"] = std::vector<double>(in.samples().begin(), in.samples().end());
                request["sampleInterval"] = in.sample_interval();
                if (in.min_frequency_hz() > 0.0)
                    request["minHz"] = in.min_frequency_hz();
                if (in.max_frequency_hz() > 0.0)
                    request["maxHz"] = in.max_frequency_hz();
                if (in.max_modes() > 0)
                    request["maxModes"] = in.max_modes();
                request["skipSamples"] = in.skip_samples();
            } else {
                request["frequenciesHz"] = std::vector<double>(in.frequencies_hz().begin(), in.frequencies_hz().end());
                request["values"] = std::vector<double>(in.values().begin(), in.values().end());
                if (in.count() > 0)
                    request["count"] = in.count();
            }
            auto r = session_.services().resonance.identify(c, request);
            if (!r)
                return r.error();
            out.set_method(r.value().value("method", std::string()));
            out.set_relative_residual(r.value().value("relativeResidual", 0.0));
            for (const auto& w : r.value().value("warnings", nlohmann::json::array()))
                out.add_warnings(w.get<std::string>());
            for (const auto& m : r.value()["resonances"]) {
                auto* x = out.add_resonances();
                x->set_frequency_hz(m.value("frequencyHz", 0.0));
                x->set_decay_rate(m.value("decayRate", 0.0));
                x->set_quality(numberOf(m["quality"]));
                x->set_bandwidth_hz(m.value("bandwidthHz", 0.0));
                x->set_amplitude(m.value("amplitude", 0.0));
                x->set_phase(m.value("phase", 0.0));
                x->set_frequency_uncertainty_hz(m.value("frequencyUncertaintyHz", 0.0));
                x->set_quality_uncertainty(numberOf(m["qualityUncertainty"]));
                x->set_overlapping(m.value("overlapping", false));
                x->set_asymmetric(m.value("asymmetric", false));
                x->set_fit_quality(m.value("fitQuality", 0.0));
            }
            return std::nullopt;
        });
    unary<v1::CreateEffectiveMaterialRequest, v1::CreateEffectiveMaterialResponse>(
        "FunctionalStructureService", "CreateEffectiveMaterial", Permission::Modify, true, false,
        [this](const v1::CreateEffectiveMaterialRequest& in, v1::CreateEffectiveMaterialResponse& out, const CallContext& c) -> Failure {
            const std::string id = in.material_id().empty() ? Id::generate().str() : in.material_id();
            nlohmann::json options{{"id", id}, {"components", nlohmann::json::object()}};
            if (!in.name().empty())
                options["name"] = in.name();
            for (const auto& [key, component] : in.components())
                options["components"][key] = component;
            auto m = session_.services().functional.createEffectiveMaterial(c, in.result_id(), options);
            if (!m)
                return m.error();
            detail::mutated(out.mutable_header(), m.value());
            toProto(m.value().changes, out.mutable_changes());
            addMaterialRef(out.mutable_changes()->mutable_created(), id);
            if (!m.value().dryRun)
                if (auto created = session_.services().materials.get(c, Id(id)))
                    toProto(created.value(), out.mutable_material());
            return std::nullopt;
        });
    // --- MultiscaleService (REQ-CHAT-20261126-020, RES-MSC-014) ---------------------------------
    const auto representationToProto = [](const nlohmann::json& r, v1::MaterialRepresentation* out) {
        out->set_representation_id(r.value("id", std::string()));
        out->set_name(r.value("name", std::string()));
        out->set_scale(r.value("scale", std::string()));
        out->set_kind(r.value("kind", std::string()));
        out->set_material_id(r.value("materialId", std::string()));
        out->set_provenance(r.value("provenance", std::string()));
        out->set_validation(r.value("validation", std::string()));
        out->set_revision(r.value("revision", std::string()));
        out->set_model_card(r.value("modelCard", std::string()));
        for (const auto& c : r.value("constituents", nlohmann::json::array()))
            out->add_constituents(c.get<std::string>());
    };
    const auto mutation = [](v1::ResponseHeader* header, v1::ChangeSummary* changes, const services::Mutation& m) {
        detail::mutated(header, m);
        toProto(m.changes, changes);
    };
    unary<v1::GetMultiscaleModelRequest, v1::GetMultiscaleModelResponse>(
        "MultiscaleService", "GetMultiscaleModel", Permission::Read, false, true,
        [this](const v1::GetMultiscaleModelRequest&, v1::GetMultiscaleModelResponse& out, const CallContext& c) -> Failure {
            auto m = session_.services().multiscale.model(c);
            if (!m)
                return m.error();
            return structOf(m.value(), out.mutable_model());
        });
    unary<v1::ListRepresentationsRequest, v1::ListRepresentationsResponse>(
        "MultiscaleService", "ListRepresentations", Permission::Read, false, true,
        [this, representationToProto](const v1::ListRepresentationsRequest&, v1::ListRepresentationsResponse& out, const CallContext& c) -> Failure {
            auto list = session_.services().multiscale.representations(c);
            if (!list)
                return list.error();
            for (const auto& r : list.value())
                representationToProto(r, out.add_representations());
            return std::nullopt;
        });
    unary<v1::GetRepresentationRequest, v1::GetRepresentationResponse>(
        "MultiscaleService", "GetRepresentation", Permission::Read, false, true,
        [this, representationToProto](const v1::GetRepresentationRequest& in, v1::GetRepresentationResponse& out, const CallContext& c) -> Failure {
            auto r = session_.services().multiscale.representation(c, in.representation_id());
            if (!r)
                return r.error();
            representationToProto(r.value(), out.mutable_representation());
            return structOf(r.value(), out.mutable_document());
        });
    unary<v1::SetRepresentationRequest, v1::SetRepresentationResponse>(
        "MultiscaleService", "SetRepresentation", Permission::Modify, true, false,
        [this, mutation](const v1::SetRepresentationRequest& in, v1::SetRepresentationResponse& out, const CallContext& c) -> Failure {
            auto m = session_.services().multiscale.setRepresentation(c, jsonOf(in.definition()));
            if (!m)
                return m.error();
            mutation(out.mutable_header(), out.mutable_changes(), m.value());
            return std::nullopt;
        });
    unary<v1::DeleteRepresentationRequest, v1::DeleteRepresentationResponse>(
        "MultiscaleService", "DeleteRepresentation", Permission::Modify, true, false,
        [this, mutation](const v1::DeleteRepresentationRequest& in, v1::DeleteRepresentationResponse& out, const CallContext& c) -> Failure {
            auto m = session_.services().multiscale.deleteRepresentation(c, in.representation_id());
            if (!m)
                return m.error();
            mutation(out.mutable_header(), out.mutable_changes(), m.value());
            return std::nullopt;
        });
    unary<v1::GenerateMicrostructureRequest, v1::GenerateMicrostructureResponse>(
        "MultiscaleService", "GenerateMicrostructure", Permission::Modify, true, false,
        [this, mutation](const v1::GenerateMicrostructureRequest& in, v1::GenerateMicrostructureResponse& out, const CallContext& c) -> Failure {
            auto m = session_.services().multiscale.generateMicrostructure(c, jsonOf(in.request()));
            if (!m)
                return m.error();
            mutation(out.mutable_header(), out.mutable_changes(), m.value());
            return std::nullopt;
        });
    unary<v1::CreateAtomisticStructureRequest, v1::CreateAtomisticStructureResponse>(
        "MultiscaleService", "CreateAtomisticStructure", Permission::Modify, true, false,
        [this, mutation](const v1::CreateAtomisticStructureRequest& in, v1::CreateAtomisticStructureResponse& out, const CallContext& c) -> Failure {
            nlohmann::json request = nlohmann::json::object();
            if (!in.representation_id().empty())
                request["id"] = in.representation_id();
            if (!in.name().empty())
                request["name"] = in.name();
            if (!in.text().empty()) {
                request["text"] = in.text();
                request["format"] = in.format();
            } else {
                request["structure"] = in.structure();
                request["latticeParameter"] = in.lattice_parameter();
                request["species"] = in.species();
            }
            request["forceField"] = in.force_field();
            request["allowIncompatible"] = in.allow_incompatible();
            if (!in.material_id().empty())
                request["materialId"] = in.material_id();
            auto m = session_.services().multiscale.createAtomisticStructure(c, request);
            if (!m)
                return m.error();
            mutation(out.mutable_header(), out.mutable_changes(), m.value());
            return std::nullopt;
        });
    const auto structList = [](const nlohmann::json& list, auto* field) -> Failure {
        for (const auto& item : list)
            if (Failure f = structOf(item, field->Add()))
                return f;
        return std::nullopt;
    };
    unary<v1::ListDerivationMethodsRequest, v1::ListDerivationMethodsResponse>(
        "MultiscaleService", "ListDerivationMethods", Permission::Read, false, true,
        [this, structList](const v1::ListDerivationMethodsRequest& in, v1::ListDerivationMethodsResponse& out, const CallContext& c) -> Failure {
            auto m = session_.services().multiscale.methods(c, in.representation_id());
            if (!m)
                return m.error();
            return structList(m.value(), out.mutable_methods());
        });
    unary<v1::ListModelCardsRequest, v1::ListModelCardsResponse>(
        "MultiscaleService", "ListModelCards", Permission::Read, false, true,
        [this, structList](const v1::ListModelCardsRequest&, v1::ListModelCardsResponse& out, const CallContext& c) -> Failure {
            auto m = session_.services().multiscale.modelCards(c);
            if (!m)
                return m.error();
            return structList(m.value(), out.mutable_cards());
        });
    unary<v1::ListMultiscaleCouplingsRequest, v1::ListMultiscaleCouplingsResponse>(
        "MultiscaleService", "ListMultiscaleCouplings", Permission::Read, false, true,
        [this, structList](const v1::ListMultiscaleCouplingsRequest&, v1::ListMultiscaleCouplingsResponse& out, const CallContext& c) -> Failure {
            auto m = session_.services().multiscale.couplings(c);
            if (!m)
                return m.error();
            return structList(m.value(), out.mutable_couplings());
        });
    unary<v1::ListExternalSolversRequest, v1::ListExternalSolversResponse>(
        "MultiscaleService", "ListExternalSolvers", Permission::Read, false, true,
        [this](const v1::ListExternalSolversRequest&, v1::ListExternalSolversResponse& out, const CallContext& c) -> Failure {
            auto m = session_.services().multiscale.externalSolvers(c);
            if (!m)
                return m.error();
            for (const auto& x : m.value()) {
                auto* s = out.add_solvers();
                s->set_solver_id(x.value("id", std::string()));
                s->set_name(x.value("name", std::string()));
                s->set_scale(x.value("scale", std::string()));
                s->set_license(x.value("license", std::string()));
                s->set_status(x.value("status", std::string()));
                s->set_found(x.value("found", false));
                s->set_install(x.value("install", std::string()));
                for (const auto& p : x.value("provides", nlohmann::json::array()))
                    s->add_provides(p.get<std::string>());
            }
            return std::nullopt;
        });
    unary<v1::DeriveMaterialPropertiesRequest, v1::DeriveMaterialPropertiesResponse>(
        "MultiscaleService", "DeriveMaterialProperties", Permission::Execute, false, false,
        [this](const v1::DeriveMaterialPropertiesRequest& in, v1::DeriveMaterialPropertiesResponse& out, const CallContext& c) -> Failure {
            auto job = session_.services().multiscale.derive(c, in.representation_id(), in.method_id(),
                                                             in.has_settings() ? jsonOf(in.settings()) : nlohmann::json::object());
            if (!job)
                return job.error();
            toProto(job.value(), out.mutable_job());
            return std::nullopt;
        });
    unary<v1::GetMultiscaleResultRequest, v1::GetMultiscaleResultResponse>(
        "MultiscaleService", "GetMultiscaleResult", Permission::Read, false, true,
        [this](const v1::GetMultiscaleResultRequest& in, v1::GetMultiscaleResultResponse& out, const CallContext& c) -> Failure {
            auto r = session_.services().multiscale.result(c, in.result_id());
            if (!r)
                return r.error();
            out.set_stale(r.value().value("stale", false));
            for (const auto& s : r.value().value("staleReasons", nlohmann::json::array()))
                out.add_stale_reasons(s.get<std::string>());
            const nlohmann::json& d = r.value();
            out.set_reliable(d.contains("derivation") ? d["derivation"].value("reliable", false) : d.value("reliable", false));
            return structOf(r.value(), out.mutable_document());
        });
    unary<v1::AssignDerivedPropertiesRequest, v1::AssignDerivedPropertiesResponse>(
        "MultiscaleService", "AssignDerivedProperties", Permission::Modify, true, false,
        [this, mutation](const v1::AssignDerivedPropertiesRequest& in, v1::AssignDerivedPropertiesResponse& out, const CallContext& c) -> Failure {
            nlohmann::json options = nlohmann::json::object();
            std::string id = in.material_id();
            if (!id.empty()) {
                options["materialId"] = id;
            } else {
                id = in.new_material_id().empty() ? Id::generate().str() : in.new_material_id();
                options["newMaterial"] = {{"id", id}};
                if (!in.new_material_name().empty())
                    options["newMaterial"]["name"] = in.new_material_name();
            }
            if (in.properties_size() > 0)
                options["properties"] = std::vector<std::string>(in.properties().begin(), in.properties().end());
            for (const auto& [key, component] : in.components())
                options["components"][key] = component;
            auto m = session_.services().multiscale.assign(c, in.result_id(), options);
            if (!m)
                return m.error();
            mutation(out.mutable_header(), out.mutable_changes(), m.value());
            addMaterialRef(in.material_id().empty() ? out.mutable_changes()->mutable_created() : out.mutable_changes()->mutable_updated(), id);
            if (!m.value().dryRun)
                if (auto material = session_.services().materials.get(c, Id(id)))
                    toProto(material.value(), out.mutable_material());
            return std::nullopt;
        });
    unary<v1::GetMaterialLineageRequest, v1::GetMaterialLineageResponse>(
        "MultiscaleService", "GetMaterialLineage", Permission::Read, false, true,
        [this](const v1::GetMaterialLineageRequest& in, v1::GetMaterialLineageResponse& out, const CallContext& c) -> Failure {
            auto l = session_.services().multiscale.lineage(c, in.material_id());
            if (!l)
                return l.error();
            return structOf(l.value(), out.mutable_lineage());
        });
    unary<v1::GetLocalStateRequest, v1::GetLocalStateResponse>(
        "MultiscaleService", "GetLocalState", Permission::Read, false, true,
        [this](const v1::GetLocalStateRequest& in, v1::GetLocalStateResponse& out, const CallContext& c) -> Failure {
            auto s = session_.services().multiscale.localState(c, in.system_id(), in.region_id(), in.result_id());
            if (!s)
                return s.error();
            return structOf(s.value(), out.mutable_state());
        });
    unary<v1::IterateRegionCouplingRequest, v1::IterateRegionCouplingResponse>(
        "MultiscaleService", "IterateRegionCoupling", Permission::Execute, false, false,
        [this](const v1::IterateRegionCouplingRequest& in, v1::IterateRegionCouplingResponse& out, const CallContext& c) -> Failure {
            nlohmann::json spec{{"system", in.system_id()}, {"region", in.region_id()}, {"representation", in.representation_id()}};
            if (in.tolerance() > 0.0)
                spec["tolerance"] = in.tolerance();
            if (in.max_iterations() > 0)
                spec["maxIterations"] = in.max_iterations();
            if (in.has_derive_settings())
                spec["derive"] = jsonOf(in.derive_settings());
            auto job = session_.services().multiscale.iterate(c, spec);
            if (!job)
                return job.error();
            toProto(job.value(), out.mutable_job());
            return std::nullopt;
        });
    unary<v1::ExplainCandidatesRequest, v1::ExplainCandidatesResponse>(
        "FunctionalStructureService", "ExplainCandidates", Permission::Read, false, true,
        [this](const v1::ExplainCandidatesRequest& in, v1::ExplainCandidatesResponse& out, const CallContext& c) -> Failure {
            auto e = session_.services().functional.explain(c, in.run_id(), in.count() > 0 ? in.count() : 3);
            if (!e)
                return e.error();
            out.set_structure_id(e.value().value("structure", std::string()));
            for (const auto& k : e.value().value("candidates", nlohmann::json::array())) {
                auto* x = out.add_candidates();
                x->set_candidate_id(k.value("candidate", std::string()));
                x->set_weighted_score(k.value("weightedScore", 0.0));
                x->set_feasible(k.value("feasible", false));
                x->set_model_id(k.value("model", std::string()));
                if (Failure f = structOf(k.value("values", nlohmann::json::object()), x->mutable_values()))
                    return f;
                if (Failure f = structOf(k.value("metrics", nlohmann::json::object()), x->mutable_metrics()))
                    return f;
                if (Failure f = structOf(k.value("explanation", nlohmann::json::object()), x->mutable_explanation()))
                    return f;
            }
            return std::nullopt;
        });
}

}  // namespace resonant::automation_api
