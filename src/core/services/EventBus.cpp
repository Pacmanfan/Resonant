#include "services/EventBus.h"

#include <algorithm>

namespace resonant::services {

std::string_view toString(EventCategory c)
{
    switch (c) {
    case EventCategory::Project: return "project";
    case EventCategory::Scene: return "scene";
    case EventCategory::Material: return "material";
    case EventCategory::Job: return "job";
    case EventCategory::Result: return "result";
    case EventCategory::Session: return "session";
    case EventCategory::Diagnostic: return "diagnostic";
    }
    return "project";
}

bool EventFilter::matches(const Event& e) const
{
    if (!categories.empty() && std::find(categories.begin(), categories.end(), e.category) == categories.end())
        return false;
    if (!sessionId.empty() && e.sessionId != sessionId)
        return false;
    const auto names = [&](const std::string& id) {
        return std::any_of(e.resources.begin(), e.resources.end(), [&](const EventResource& r) { return r.id == id; });
    };
    if (!jobId.empty() && !names(jobId))
        return false;
    if (!resourceIds.empty() && !std::any_of(resourceIds.begin(), resourceIds.end(), names))
        return false;
    return true;
}

EventBus::EventBus(std::size_t capacity) : capacity_(std::max<std::size_t>(capacity, 16)) {}

std::uint64_t EventBus::publish(Event event)
{
    std::vector<Listener> listeners;
    {
        std::lock_guard lock(mutex_);
        event.sequence = next_++;
        event.eventId = "EV-" + std::to_string(event.sequence);
        event.time = std::chrono::system_clock::now();
        events_.push_back(event);
        while (events_.size() > capacity_)
            events_.pop_front();
        for (const auto& [handle, listener] : listeners_)
            listeners.push_back(listener);
    }
    for (const Listener& l : listeners)
        l(event);
    return event.sequence;
}

EventBatch EventBus::since(std::uint64_t after, const EventFilter& filter, std::size_t limit) const
{
    std::lock_guard lock(mutex_);
    EventBatch batch;
    batch.lastSequence = next_ - 1;
    const std::uint64_t oldest = events_.empty() ? next_ : events_.front().sequence;
    if (after + 1 < oldest && after < next_ - 1)
        batch.gap = EventGap{after + 1, oldest - 1, oldest};
    for (const Event& e : events_) {
        if (e.sequence <= after || !filter.matches(e))
            continue;
        batch.events.push_back(e);
        if (batch.events.size() >= limit) {
            batch.lastSequence = e.sequence;  // resume after the last one returned
            break;
        }
    }
    return batch;
}

std::uint64_t EventBus::lastSequence() const
{
    std::lock_guard lock(mutex_);
    return next_ - 1;
}

std::uint64_t EventBus::oldestRetained() const
{
    std::lock_guard lock(mutex_);
    return events_.empty() ? next_ : events_.front().sequence;
}

int EventBus::subscribe(Listener listener)
{
    std::lock_guard lock(mutex_);
    listeners_[nextListener_] = std::move(listener);
    return nextListener_++;
}

void EventBus::unsubscribe(int handle)
{
    std::lock_guard lock(mutex_);
    listeners_.erase(handle);
}

}  // namespace resonant::services
