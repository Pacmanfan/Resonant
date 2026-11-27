// Internal to the adapter: how a handler is registered and answers.
#pragma once

#include "Conversions.h"
#include "Dispatcher.h"
#include "foundation/Id.h"
#include "services/Session.h"

#include <exception>
#include <optional>

namespace resonant::automation_api::detail {

// What a handler returns: nothing, or the error.
using Failure = std::optional<services::ServiceError>;

Reply failure(services::ServiceError e, const std::string& requestId);

inline void mutated(v1::ResponseHeader* h, const services::Mutation& m)
{
    h->set_dry_run(m.dryRun);
    h->set_replayed(m.replayed);
    for (const std::string& w : m.warnings)
        h->add_warnings(w);
}

inline std::optional<std::uint64_t> expected(bool has, std::uint64_t value)
{
    return has ? std::optional<std::uint64_t>(value) : std::nullopt;
}

template <class Request>
services::CallContext contextFor(const Request& request, services::Interface source)
{
    services::CallContext c = contextOf(request.header(), source);
    if (c.requestId.empty())
        c.requestId = "Q-" + Id::generate().str();
    return c;
}

}  // namespace resonant::automation_api::detail

namespace resonant::automation_api {

// A unary method run on the owner thread: f(request, response, context) -> Failure.
template <class Request, class Response, class F>
void Dispatcher::unary(const std::string& service, const std::string& name, services::Permission permission, bool mutating, bool idempotent, F f)
{
    Handler h;
    h.info = {"/resonant.v1." + service + "/" + name, permission, mutating, idempotent, false};
    h.unary = [this, f](const std::string& payload, services::Interface source) -> Reply {
        Request request;
        if (!request.ParseFromString(payload))
            return detail::failure(services::ServiceError::make(services::ErrorCode::InvalidArgument,
                                                                "the request is not a valid " + std::string(Request::descriptor()->full_name())),
                                   {});
        services::CallContext context = detail::contextFor(request, source);
        Response response;
        detail::Failure failed;
        run_([&] {
            try {
                failed = f(request, response, context);
            } catch (const std::exception& e) {
                failed = services::ServiceError::make(services::ErrorCode::Internal, std::string("internal error: ") + e.what());
            }
            if (!failed) {
                response.mutable_header()->set_request_id(context.requestId);
                response.mutable_header()->set_project_revision(session_.projectRevision());
            }
        });
        if (failed)
            return detail::failure(*failed, context.requestId);
        Reply r;
        response.SerializeToString(&r.payload);
        return r;
    };
    handlers_[h.info.method] = std::move(h);
}

// A unary method that waits on the transport's thread (it visits the owner thread itself).
template <class Request, class Response, class F>
void Dispatcher::blocking(const std::string& service, const std::string& name, services::Permission permission, F f)
{
    Handler h;
    h.info = {"/resonant.v1." + service + "/" + name, permission, false, true, false};
    h.unary = [this, f](const std::string& payload, services::Interface source) -> Reply {
        Request request;
        if (!request.ParseFromString(payload))
            return detail::failure(services::ServiceError::make(services::ErrorCode::InvalidArgument,
                                                                "the request is not a valid " + std::string(Request::descriptor()->full_name())),
                                   {});
        services::CallContext context = detail::contextFor(request, source);
        Response response;
        detail::Failure failed;
        try {
            failed = f(request, response, context);
        } catch (const std::exception& e) {
            failed = services::ServiceError::make(services::ErrorCode::Internal, std::string("internal error: ") + e.what());
        }
        if (failed)
            return detail::failure(*failed, context.requestId);
        response.mutable_header()->set_request_id(context.requestId);
        run_([&] { response.mutable_header()->set_project_revision(session_.projectRevision()); });
        Reply r;
        response.SerializeToString(&r.payload);
        return r;
    };
    handlers_[h.info.method] = std::move(h);
}

}  // namespace resonant::automation_api
