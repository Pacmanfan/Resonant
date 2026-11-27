// The application service layer's vocabulary (REQ-CHAT-20261126-012,
// ADR-000216): structured errors, call contexts with permissions, change
// summaries. No transport, no Qt, no generated protobuf types.
#pragma once

#include "foundation/Id.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace resonant::services {

enum class ErrorCode {
    InvalidArgument,
    NotFound,
    AlreadyExists,
    Conflict,
    FailedPrecondition,
    PermissionDenied,
    ResourceExhausted,
    Unimplemented,
    Unavailable,
    Cancelled,
    Internal,
};
std::string_view toString(ErrorCode code);

struct FieldViolation {
    std::string field, description;
};

struct ServiceError {
    ErrorCode code = ErrorCode::Internal;
    std::string message;
    std::vector<FieldViolation> violations;
    std::string resourceType;  // "scene-object", "material", "job", "result", "transaction", "session"
    std::string resourceId;
    std::uint64_t currentRevision = 0;
    bool retryable = false;
    std::string requestId;

    static ServiceError make(ErrorCode code, std::string message) { return {code, std::move(message), {}, {}, {}, 0, false, {}}; }
};

// A value or a structured error.
template <class T>
class Outcome {
public:
    Outcome(T value) : data_(std::move(value)) {}
    Outcome(ServiceError error) : data_(std::move(error)) {}
    bool ok() const { return data_.index() == 0; }
    explicit operator bool() const { return ok(); }
    const T& value() const { return std::get<0>(data_); }
    T& value() { return std::get<0>(data_); }
    T take() { return std::move(std::get<0>(data_)); }
    const ServiceError& error() const { return std::get<1>(data_); }

private:
    std::variant<T, ServiceError> data_;
};

struct Unit {};
using Done = Outcome<Unit>;

inline ServiceError notFound(std::string what, std::string type, std::string id)
{
    ServiceError e = ServiceError::make(ErrorCode::NotFound, std::move(what));
    e.resourceType = std::move(type);
    e.resourceId = std::move(id);
    return e;
}
inline ServiceError invalid(std::string message, std::string field = {})
{
    ServiceError e = ServiceError::make(ErrorCode::InvalidArgument, std::move(message));
    if (!field.empty())
        e.violations.push_back({std::move(field), e.message});
    return e;
}

// Who calls, through which interface.
enum class Interface { Unspecified, Gui, Python, Grpc, Mcp, Internal };
std::string_view toString(Interface source);

// Permissions (bit set). No permission ever starts a physical output.
enum class Permission : std::uint32_t { Read = 1, Modify = 2, Execute = 4, Files = 8, Admin = 16 };
inline constexpr std::uint32_t kAllPermissions = 31;
inline constexpr std::uint32_t kDefaultRemotePermissions = 1 | 2 | 4;  // read, modify, execute
inline bool has(std::uint32_t set, Permission p) { return (set & static_cast<std::uint32_t>(p)) != 0; }
std::string_view toString(Permission p);

struct CallContext {
    std::string sessionId;
    std::string clientName;
    std::string requestId;
    std::string idempotencyKey;
    Interface source = Interface::Internal;
    std::uint32_t permissions = kAllPermissions;
    std::optional<std::uint64_t> expectedProjectRevision;
    bool dryRun = false;
};

// What a mutation changed (or would change, in a dry run).
struct ChangeSummary {
    std::vector<Id> created, updated, deleted;
    bool materials = false;
    bool settings = false;
    std::string description;
    bool empty() const { return created.empty() && updated.empty() && deleted.empty() && !materials && !settings; }
};

struct Mutation {
    ChangeSummary changes;
    std::uint64_t revisionBefore = 0, revisionAfter = 0;
    bool dryRun = false;
    bool replayed = false;
    std::vector<std::string> warnings;
};

}  // namespace resonant::services
