// The event methods and the streamed field slices (REQ-CHAT-20261126-012).
// A subscription waits on its own thread and visits the owner thread only to
// authorize and to take the snapshot; it resumes after any sequence, reports
// what retention lost as a gap, and ends when the client goes away, its session
// closes or the endpoint stops.
#include "HandlerSupport.h"

#include "resonant/v1/automation/event_service.pb.h"
#include "resonant/v1/results/results_service.pb.h"
#include "services/Services.h"

#include <condition_variable>
#include <cstring>
#include <mutex>

namespace resonant::automation_api {

using services::CallContext;
using services::ErrorCode;
using services::Permission;
using services::ServiceError;
using detail::Failure;

namespace {

services::EventFilter filterOf(const v1::EventFilter& f)
{
    static const std::map<int, services::EventCategory> categories = {
        {v1::EVENT_CATEGORY_PROJECT, services::EventCategory::Project}, {v1::EVENT_CATEGORY_SCENE, services::EventCategory::Scene},
        {v1::EVENT_CATEGORY_MATERIAL, services::EventCategory::Material}, {v1::EVENT_CATEGORY_JOB, services::EventCategory::Job},
        {v1::EVENT_CATEGORY_RESULT, services::EventCategory::Result},     {v1::EVENT_CATEGORY_SESSION, services::EventCategory::Session},
        {v1::EVENT_CATEGORY_DIAGNOSTIC, services::EventCategory::Diagnostic}};
    services::EventFilter out;
    for (int c : f.categories())
        if (const auto it = categories.find(c); it != categories.end())
            out.categories.push_back(it->second);
    out.resourceIds.assign(f.resource_ids().begin(), f.resource_ids().end());
    out.jobId = f.job_id();
    out.sessionId = f.session_id();
    return out;
}

void gapToProto(const services::EventGap& g, v1::GapNotice* out)
{
    out->set_first_missed_sequence(g.firstMissed);
    out->set_last_missed_sequence(g.lastMissed);
    out->set_oldest_retained_sequence(g.oldestRetained);
}

// Wakes a waiting subscription; shared so a listener copy that is still running stays valid.
struct Wakeup {
    std::mutex mutex;
    std::condition_variable cv;
    bool news = false;
};

Reply ended(int status, const std::string& message, const std::string& requestId)
{
    ServiceError e = ServiceError::make(status == 1 ? ErrorCode::Cancelled : ErrorCode::Unavailable, message);
    return detail::failure(e, requestId);
}

}  // namespace

void Dispatcher::registerStreams()
{
    unary<v1::ListEventsRequest, v1::ListEventsResponse>(
        "EventService", "ListEvents", Permission::Read, false, true,
        [this](const v1::ListEventsRequest& in, v1::ListEventsResponse& out, const CallContext& c) -> Failure {
            auto a = session_.authorize(c, Permission::Read);
            if (!a)
                return a.error();
            const std::size_t limit = in.limit() > 0 ? static_cast<std::size_t>(std::min(in.limit(), 1000)) : 1000;
            const services::EventBatch b = session_.events().since(in.after_sequence(), filterOf(in.filter()), limit);
            for (const services::Event& e : b.events)
                toProto(e, out.add_events());
            if (b.gap)
                gapToProto(*b.gap, out.mutable_gap());
            out.set_last_sequence(b.lastSequence);
            return std::nullopt;
        });

    {
        Handler h;
        h.info = {"/resonant.v1.EventService/SubscribeEvents", Permission::Read, false, true, true};
        h.streaming = [this](const std::string& payload, services::Interface source, const Sink& sink, const std::atomic<bool>* stop) -> Reply {
            v1::SubscribeEventsRequest in;
            if (!in.ParseFromString(payload))
                return detail::failure(ServiceError::make(ErrorCode::InvalidArgument, "the request is not a valid SubscribeEventsRequest"), {});
            CallContext c = detail::contextFor(in, source);
            Failure failed;
            std::uint64_t after = in.after_sequence();
            v1::SubscribeEventsResponse snapshot;
            run_([&] {
                auto a = session_.authorize(c, Permission::Read);
                if (!a) {
                    failed = a.error();
                    return;
                }
                c = a.value();
                if (in.include_snapshot()) {
                    auto s = session_.services().scene.snapshot(c);
                    if (!s) {
                        failed = s.error();
                        return;
                    }
                    v1::SceneSnapshot* o = snapshot.mutable_snapshot();
                    o->set_project_id(s.value().projectId);
                    o->set_project_revision(s.value().revision);
                    o->set_event_sequence(s.value().eventSequence);
                    for (const auto& object : s.value().objects)
                        toProto(object, o->add_objects());
                    after = s.value().eventSequence;
                } else if (after == 0) {
                    after = session_.events().lastSequence();  // from now
                }
            });
            if (failed)
                return detail::failure(*failed, c.requestId);
            if (in.include_snapshot() && !sink(snapshot.SerializeAsString()))
                return ended(1, "the client went away", c.requestId);

            services::EventBus& bus = session_.events();
            const services::EventFilter filter = filterOf(in.filter());
            auto wake = std::make_shared<Wakeup>();
            const int handle = bus.subscribe([wake](const services::Event&) {
                {
                    std::lock_guard lock(wake->mutex);
                    wake->news = true;
                }
                wake->cv.notify_one();
            });
            struct Unsubscribe {
                services::EventBus& bus;
                int handle;
                ~Unsubscribe() { bus.unsubscribe(handle); }
            } unsubscribe{bus, handle};

            auto lastCheck = std::chrono::steady_clock::now();
            for (;;) {
                if (stop && stop->load())
                    return ended(14, "the automation endpoint stopped", c.requestId);
                const services::EventBatch batch = bus.since(after, filter, 500);
                if (batch.gap) {
                    v1::SubscribeEventsResponse item;
                    gapToProto(*batch.gap, item.mutable_gap());
                    if (!sink(item.SerializeAsString()))
                        return ended(1, "the client went away", c.requestId);
                }
                for (const services::Event& e : batch.events) {
                    v1::SubscribeEventsResponse item;
                    toProto(e, item.mutable_event());
                    if (!sink(item.SerializeAsString()))
                        return ended(1, "the client went away", c.requestId);
                }
                after = std::max(after, batch.lastSequence);
                if (batch.events.size() >= 500)
                    continue;  // more are waiting
                {
                    std::unique_lock lock(wake->mutex);
                    wake->cv.wait_for(lock, std::chrono::milliseconds(500), [&] { return wake->news; });
                    wake->news = false;
                }
                // Every few seconds: is the client still there, is its session still open?
                if (std::chrono::steady_clock::now() - lastCheck > std::chrono::seconds(2)) {
                    lastCheck = std::chrono::steady_clock::now();
                    if (!sink(std::string()))
                        return ended(1, "the client went away", c.requestId);
                    bool open = true;
                    run_([&] {
                        open = false;
                        for (const services::SessionInfo& s : session_.sessions())
                            open = open || s.id == c.sessionId;
                    });
                    if (!open)
                        return detail::failure(services::notFound("the session closed", "session", c.sessionId), c.requestId);
                }
            }
        };
        handlers_[h.info.method] = std::move(h);
    }

    {
        Handler h;
        h.info = {"/resonant.v1.ResultsService/ReadFieldSlice", Permission::Read, false, true, true};
        h.streaming = [this](const std::string& payload, services::Interface source, const Sink& sink, const std::atomic<bool>* stop) -> Reply {
            v1::ReadFieldSliceRequest in;
            if (!in.ParseFromString(payload))
                return detail::failure(ServiceError::make(ErrorCode::InvalidArgument, "the request is not a valid ReadFieldSliceRequest"), {});
            const CallContext c = detail::contextFor(in, source);
            const int axis = in.fixed_axis() == v1::SLICE_AXIS_U ? 0 : in.fixed_axis() == v1::SLICE_AXIS_V ? 1 : 2;
            services::Outcome<services::FieldSlice> slice = services::FieldSlice{};
            run_([&] { slice = session_.services().results.slice(c, in.result_id(), axis, in.fixed_index(), in.stride() > 0 ? in.stride() : 1); });
            if (!slice)
                return detail::failure(slice.error(), c.requestId);
            const services::FieldSlice& s = slice.value();
            const std::size_t chunk = in.chunk_samples() > 0 ? std::min<std::size_t>(in.chunk_samples(), 1u << 20) : 65536;
            const std::size_t total = s.samples.size();
            std::size_t offset = 0;
            do {
                if (stop && stop->load())
                    return ended(14, "the automation endpoint stopped", c.requestId);
                const std::size_t count = std::min(chunk, total - offset);
                v1::FieldDataChunk item;
                item.set_rows(s.rows);
                item.set_columns(s.columns);
                item.set_offset(static_cast<std::int64_t>(offset));
                item.set_count(static_cast<int>(count));
                // Interleaved little-endian float32 (real, imaginary), as std::complex<float> stores them on x86-64 / AArch64.
                std::string bytes(count * 2 * sizeof(float), '\0');
                if (count > 0)
                    std::memcpy(bytes.data(), s.samples.data() + offset, bytes.size());
                item.set_samples(std::move(bytes));
                item.set_unit("Pa");
                item.set_last(offset + count >= total);
                if (!sink(item.SerializeAsString()))
                    return ended(1, "the client went away", c.requestId);
                offset += count;
            } while (offset < total);
            return Reply{};
        };
        handlers_[h.info.method] = std::move(h);
    }
}

}  // namespace resonant::automation_api
