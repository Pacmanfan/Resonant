// The protobuf adapter of the application services (REQ-CHAT-20261126-012,
// ADR-000216): every method of the resonant.v1 contract, keyed by its full
// gRPC name ("/resonant.v1.SceneService/GetObject"), decoded, run against the
// one Session's services and encoded. Transports (the local endpoint, the
// in-process Python dispatch) hand it serialized messages; nothing here knows
// about sockets or gRPC.
//
// Threading: `run` executes a function on the session's owner thread and
// returns when it is done (the GUI marshals to its thread; the headless host
// takes its lock). Streams wait for events on the caller's thread and only
// visit the owner thread to read.
#pragma once

#include "services/ServiceTypes.h"

#include <atomic>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace resonant::services {
class Session;
}

namespace resonant::automation_api {

struct Reply {
    int status = 0;        // gRPC status code; 0 = OK
    std::string payload;   // the serialized response (OK)
    std::string error;     // a serialized resonant.v1.Error (otherwise)
};

struct MethodInfo {
    std::string method;    // "/resonant.v1.SceneService/GetObject"
    services::Permission permission = services::Permission::Read;
    bool mutating = false;
    bool idempotent = true;   // safe to retry without an idempotency key
    bool streaming = false;   // server streaming
};

class Dispatcher {
public:
    using Executor = std::function<void(const std::function<void()>&)>;
    // Receives each streamed message; returns false when the client went away.
    using Sink = std::function<bool(const std::string& message)>;

    // `policy`: the permissions a remote session may hold (default read, modify, execute).
    Dispatcher(services::Session& session, Executor run = {}, std::uint32_t policy = services::kDefaultRemotePermissions);
    ~Dispatcher();

    // A unary method. `source` is the transport's interface (gRPC, MCP, Python).
    Reply call(const std::string& method, const std::string& payload, services::Interface source);
    // A server-streaming method; returns the final status. `stop` ends it early (shutdown).
    Reply stream(const std::string& method, const std::string& payload, services::Interface source, const Sink& sink,
                 const std::atomic<bool>* stop = nullptr);

    const MethodInfo* method(const std::string& name) const;
    std::vector<MethodInfo> methods() const;
    std::uint32_t policy() const { return policy_; }
    services::Session& session() { return session_; }
    void runOnOwner(const std::function<void()>& f) { run_(f); }

    // The longest a WaitForJob may wait, and how often streams look for news.
    static constexpr double kMaxWaitSeconds = 600.0;

private:
    struct Handler {
        MethodInfo info;
        std::function<Reply(const std::string& payload, services::Interface source)> unary;
        std::function<Reply(const std::string& payload, services::Interface source, const Sink&, const std::atomic<bool>*)> streaming;
    };
    template <class Request, class Response, class F>
    void unary(const std::string& service, const std::string& name, services::Permission permission, bool mutating, bool idempotent, F f);
    template <class Request, class Response, class F>
    void blocking(const std::string& service, const std::string& name, services::Permission permission, F f);
    void registerApplication();
    void registerProjectAndScene();
    void registerPhysics();
    void registerAutomation();
    void registerStreams();

    services::Session& session_;
    Executor run_;
    std::uint32_t policy_;
    std::map<std::string, Handler> handlers_;
};

// The schema revision within resonant.v1 (incremented with each compatible addition).
inline constexpr const char* kApiRevision = "1.0";

}  // namespace resonant::automation_api
