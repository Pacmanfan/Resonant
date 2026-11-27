// Service views <-> resonant.v1 messages (REQ-CHAT-20261126-012). The only
// place (with the dispatcher) where generated protobuf types meet the
// application: services and domains never see them (ADR-000216).
#pragma once

#include "services/Services.h"

#include "resonant/v1/application/application_service.pb.h"
#include "resonant/v1/arrays/array.pb.h"
#include "resonant/v1/automation/audit_service.pb.h"
#include "resonant/v1/common/errors.pb.h"
#include "resonant/v1/common/events.pb.h"
#include "resonant/v1/common/jobs.pb.h"
#include "resonant/v1/common/metadata.pb.h"
#include "resonant/v1/common/spatial.pb.h"
#include "resonant/v1/common/validation.pb.h"
#include "resonant/v1/emitters/emitter.pb.h"
#include "resonant/v1/materials/material.pb.h"
#include "resonant/v1/project/project.pb.h"
#include "resonant/v1/results/result.pb.h"
#include "resonant/v1/scene/scene.pb.h"

#include <nlohmann/json.hpp>

namespace resonant::automation_api {

namespace v1 = ::resonant::v1;

// gRPC status codes (the transport's language) for service error codes.
int grpcStatusOf(services::ErrorCode code);
v1::ErrorCode toProto(services::ErrorCode code);
void toProto(const services::ServiceError& e, v1::Error* out);

services::CallContext contextOf(const v1::RequestHeader& header, services::Interface source);

void toProto(const Vec3& v, v1::Vector3Meters* out);
void toProto(const Vec3& v, v1::Vector3* out);
Vec3 fromProto(const v1::Vector3Meters& v);
void toProto(const Transform& t, v1::Transform* out);
// A transform with unset parts kept from `base` (scale defaults to 1 when absent).
Transform fromProto(const v1::Transform& t, const Transform& base = {});

void toProto(const services::ProjectView& v, v1::Project* out);
void toProto(const services::ObjectView& v, v1::SceneObject* out);
void toProto(const services::ChangeSummary& c, v1::ChangeSummary* out);
void toProto(const services::ValidationView& v, v1::ValidationResult* out);
void toProto(const services::JobView& v, v1::Job* out);
void toProto(const services::Event& e, v1::Event* out);
void toProto(const services::CapabilityView& v, v1::Capability* out);
void toProto(const services::AuditRecord& r, v1::AuditRecord* out);
void toProto(const nlohmann::json& material, v1::Material* out);
void toProto(const services::ResultEntry& r, std::uint64_t, v1::ResultSummary* out);

// Emitter settings: the component's JSON form <-> SourceSettings.
void sourceToProto(const nlohmann::json& settings, v1::SourceSettings* out);
nlohmann::json sourceFromProto(const v1::SourceSettings& s);
nlohmann::json sourcePatchFromProto(const v1::SourceSettingsPatch& p);
std::string_view arrayLayoutName(v1::ArrayLayout layout);
v1::ArrayLayout arrayLayoutOf(const std::string& name);

v1::SourceInterface toProto(services::Interface source);
services::Interface fromProto(v1::SourceInterface source);
v1::Permission toProto(services::Permission p);
std::uint32_t permissionsOf(const google::protobuf::RepeatedField<int>& permissions);

}  // namespace resonant::automation_api
