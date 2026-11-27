#include "HandlerSupport.h"

namespace resonant::automation_api {

namespace detail {
Reply failure(services::ServiceError e, const std::string& requestId)
{
    if (e.requestId.empty())
        e.requestId = requestId;
    v1::Error error;
    toProto(e, &error);
    Reply r;
    r.status = grpcStatusOf(e.code);
    error.SerializeToString(&r.error);
    return r;
}
}  // namespace detail

Dispatcher::Dispatcher(services::Session& session, Executor run, std::uint32_t policy)
    : session_(session), run_(run ? std::move(run) : Executor([](const std::function<void()>& f) { f(); })), policy_(policy)
{
    registerApplication();
    registerProjectAndScene();
    registerPhysics();
    registerAutomation();
    registerStreams();
}

Dispatcher::~Dispatcher() = default;

Reply Dispatcher::call(const std::string& method, const std::string& payload, services::Interface source)
{
    const auto it = handlers_.find(method);
    if (it == handlers_.end())
        return detail::failure(services::ServiceError::make(services::ErrorCode::Unimplemented, "the method '" + method + "' does not exist"), {});
    if (!it->second.unary)
        return detail::failure(services::ServiceError::make(services::ErrorCode::InvalidArgument, "'" + method + "' is a streaming method"), {});
    return it->second.unary(payload, source);
}

Reply Dispatcher::stream(const std::string& method, const std::string& payload, services::Interface source, const Sink& sink,
                         const std::atomic<bool>* stop)
{
    const auto it = handlers_.find(method);
    if (it == handlers_.end())
        return detail::failure(services::ServiceError::make(services::ErrorCode::Unimplemented, "the method '" + method + "' does not exist"), {});
    if (!it->second.streaming)
        return detail::failure(services::ServiceError::make(services::ErrorCode::InvalidArgument, "'" + method + "' is not a streaming method"), {});
    return it->second.streaming(payload, source, sink, stop);
}

const MethodInfo* Dispatcher::method(const std::string& name) const
{
    const auto it = handlers_.find(name);
    return it == handlers_.end() ? nullptr : &it->second.info;
}

std::vector<MethodInfo> Dispatcher::methods() const
{
    std::vector<MethodInfo> out;
    for (const auto& [name, h] : handlers_)
        out.push_back(h.info);
    return out;
}

}  // namespace resonant::automation_api
