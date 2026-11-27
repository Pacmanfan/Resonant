// The application-wide event stream (REQ-CHAT-20261126-012, RES-API-EVT-*):
// every change of the project (from the GUI, Python or a remote client), job
// progress, results, sessions. Events have a sequence number increasing by one
// per event and are retained in a ring (default 10 000) so a client resumes
// after the last sequence it saw; asking for events older than the ring is
// reported as a gap (the client resynchronizes from a snapshot).
// Thread-safe: jobs publish from worker threads, transports read from theirs.
#pragma once

#include "services/ServiceTypes.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace resonant::services {

enum class EventCategory { Project, Scene, Material, Job, Result, Session, Diagnostic };
std::string_view toString(EventCategory category);

struct EventResource {
    std::string type;  // "scene-object", "material", "job", "result", "session", "project"
    std::string id;
};

struct Event {
    std::uint64_t sequence = 0;
    std::string eventId;
    std::chrono::system_clock::time_point time;
    EventCategory category = EventCategory::Project;
    std::string type;  // "scene.object.updated", "job.updated", ...
    std::string sessionId, clientName;
    Interface source = Interface::Unspecified;
    std::string projectId;
    std::vector<EventResource> resources;
    std::uint64_t projectRevision = 0;
    std::string requestId;
    nlohmann::json payload = nlohmann::json::object();  // typed per event type (docs/reference/EVENTS_AND_JOBS.md)
};

struct EventFilter {
    std::vector<EventCategory> categories;  // empty: all
    std::vector<std::string> resourceIds;   // empty: all
    std::string jobId;
    std::string sessionId;
    bool matches(const Event& event) const;
};

struct EventGap {
    std::uint64_t firstMissed = 0, lastMissed = 0, oldestRetained = 0;
};

struct EventBatch {
    std::vector<Event> events;
    std::optional<EventGap> gap;
    std::uint64_t lastSequence = 0;  // resume after this (the last one examined; the bus's last unless `limit` ended the scan)
};

class EventBus {
public:
    explicit EventBus(std::size_t capacity = 10000);
    // Assigns the sequence, id and time; returns the sequence.
    std::uint64_t publish(Event event);
    // Retained events after `afterSequence` that match (at most `limit`), with a gap when some are no longer retained.
    EventBatch since(std::uint64_t afterSequence, const EventFilter& filter = {}, std::size_t limit = 1000) const;
    std::uint64_t lastSequence() const;
    std::uint64_t oldestRetained() const;
    // Called (on the publishing thread) for every event; keep it short.
    using Listener = std::function<void(const Event&)>;
    int subscribe(Listener listener);
    void unsubscribe(int handle);

private:
    mutable std::mutex mutex_;
    std::size_t capacity_;
    std::deque<Event> events_;
    std::uint64_t next_ = 1;
    std::map<int, Listener> listeners_;
    int nextListener_ = 1;
};

}  // namespace resonant::services
