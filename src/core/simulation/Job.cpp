#include "simulation/Job.h"

#include "foundation/Log.h"

#include <algorithm>
#include <exception>

namespace resonant {

std::string_view toString(JobState state)
{
    switch (state) {
    case JobState::Queued: return "queued";
    case JobState::Running: return "running";
    case JobState::Completed: return "completed";
    case JobState::Failed: return "failed";
    case JobState::Cancelled: return "cancelled";
    }
    return "queued";
}

bool isFinished(JobState state)
{
    return state == JobState::Completed || state == JobState::Failed || state == JobState::Cancelled;
}

struct JobManager::Entry {
    Id id;
    std::shared_ptr<Job> job;
    std::string name;
    JobState state = JobState::Queued;
    double progress = 0.0;
    std::string message;
    std::atomic<bool> cancelRequested{false};
    std::chrono::steady_clock::time_point started{};
    double elapsedSeconds = 0.0;
};

class JobManager::Context final : public JobContext {
public:
    Context(JobManager& manager, std::shared_ptr<Entry> entry) : manager_(manager), entry_(std::move(entry)) {}

    bool cancelRequested() const override { return entry_->cancelRequested.load(); }

    void reportProgress(double fraction, std::string_view message) override
    {
        JobInfo info;
        {
            std::lock_guard lock(manager_.mutex_);
            entry_->progress = std::clamp(fraction, 0.0, 1.0);
            if (!message.empty())
                entry_->message = std::string(message);
            info = snapshot(*entry_);
        }
        manager_.publish(info);
    }

private:
    JobManager& manager_;
    std::shared_ptr<Entry> entry_;
};

JobManager::JobManager(unsigned workerCount)
{
    workerCount = std::max(1u, workerCount);
    workers_.reserve(workerCount);
    for (unsigned i = 0; i < workerCount; ++i)
        workers_.emplace_back([this] { workerLoop(); });
}

JobManager::~JobManager()
{
    {
        std::lock_guard lock(mutex_);
        stopping_ = true;
        for (const auto& entry : entries_)
            entry->cancelRequested.store(true);
    }
    wake_.notify_all();
    for (std::thread& worker : workers_)
        worker.join();
}

JobInfo JobManager::snapshot(const Entry& entry)
{
    JobInfo info;
    info.id = entry.id;
    info.name = entry.name;
    info.state = entry.state;
    info.progress = entry.progress;
    info.message = entry.message;
    info.elapsedSeconds = entry.state == JobState::Running
                              ? std::chrono::duration<double>(std::chrono::steady_clock::now() - entry.started).count()
                              : entry.elapsedSeconds;
    return info;
}

Id JobManager::submit(std::shared_ptr<Job> job)
{
    auto entry = std::make_shared<Entry>();
    entry->id = Id::generate();
    entry->name = job->name();
    entry->job = std::move(job);
    JobInfo info;
    {
        std::lock_guard lock(mutex_);
        entries_.push_back(entry);
        queue_.push_back(entry);
        info = snapshot(*entry);
    }
    logInfo("simulation", "job queued: " + info.name);
    publish(info);
    wake_.notify_one();
    return info.id;
}

bool JobManager::cancel(const Id& id)
{
    JobInfo info;
    bool cancelledWhileQueued = false;
    {
        std::lock_guard lock(mutex_);
        const auto it = std::find_if(entries_.begin(), entries_.end(),
                                     [&](const std::shared_ptr<Entry>& entry) { return entry->id == id; });
        if (it == entries_.end() || isFinished((*it)->state))
            return false;
        (*it)->cancelRequested.store(true);
        if ((*it)->state == JobState::Queued) {
            std::erase(queue_, *it);
            (*it)->state = JobState::Cancelled;
            (*it)->message = "cancelled before it started";
            cancelledWhileQueued = true;
        }
        info = snapshot(**it);
    }
    logInfo("simulation", "cancellation requested: " + info.name);
    if (cancelledWhileQueued) {
        publish(info);
        finished_.notify_all();
    }
    return true;
}

std::optional<JobInfo> JobManager::info(const Id& id) const
{
    std::lock_guard lock(mutex_);
    const auto it = std::find_if(entries_.begin(), entries_.end(),
                                 [&](const std::shared_ptr<Entry>& entry) { return entry->id == id; });
    if (it == entries_.end())
        return std::nullopt;
    return snapshot(**it);
}

std::vector<JobInfo> JobManager::jobs() const
{
    std::lock_guard lock(mutex_);
    std::vector<JobInfo> result;
    result.reserve(entries_.size());
    for (const auto& entry : entries_)
        result.push_back(snapshot(*entry));
    return result;
}

bool JobManager::hasUnfinishedJobs() const
{
    std::lock_guard lock(mutex_);
    return std::any_of(entries_.begin(), entries_.end(),
                       [](const std::shared_ptr<Entry>& entry) { return !isFinished(entry->state); });
}

std::optional<JobInfo> JobManager::wait(const Id& id, std::chrono::milliseconds timeout)
{
    std::unique_lock lock(mutex_);
    const auto it = std::find_if(entries_.begin(), entries_.end(),
                                 [&](const std::shared_ptr<Entry>& entry) { return entry->id == id; });
    if (it == entries_.end())
        return std::nullopt;
    const std::shared_ptr<Entry> entry = *it;
    const auto done = [&entry] { return isFinished(entry->state); };
    if (timeout == std::chrono::milliseconds::max())
        finished_.wait(lock, done);
    else
        finished_.wait_for(lock, timeout, done);
    return snapshot(*entry);
}

int JobManager::addListener(Listener listener)
{
    std::lock_guard lock(listenerMutex_);
    const int handle = nextListener_++;
    listeners_.emplace_back(handle, std::move(listener));
    return handle;
}

void JobManager::removeListener(int handle)
{
    std::lock_guard lock(listenerMutex_);
    std::erase_if(listeners_, [handle](const auto& entry) { return entry.first == handle; });
}

void JobManager::publish(const JobInfo& info)
{
    std::vector<Listener> listeners;
    {
        std::lock_guard lock(listenerMutex_);
        for (const auto& [handle, listener] : listeners_)
            listeners.push_back(listener);
    }
    for (const auto& listener : listeners)
        listener(info);
}

void JobManager::workerLoop()
{
    for (;;) {
        std::shared_ptr<Entry> entry;
        JobInfo info;
        {
            std::unique_lock lock(mutex_);
            wake_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
            if (stopping_)
                return;
            entry = queue_.front();
            queue_.pop_front();
            entry->state = JobState::Running;
            entry->started = std::chrono::steady_clock::now();
            info = snapshot(*entry);
        }
        logInfo("simulation", "job started: " + info.name);
        publish(info);

        JobEnd end;
        try {
            Context context(*this, entry);
            end = entry->job->run(context);
        } catch (const std::exception& error) {
            end = JobEnd::failed(std::string("unexpected error: ") + error.what());
        } catch (...) {
            end = JobEnd::failed("unexpected error of unknown type");
        }

        {
            std::lock_guard lock(mutex_);
            entry->elapsedSeconds =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - entry->started).count();
            switch (end.kind) {
            case JobEnd::Kind::Completed:
                entry->state = JobState::Completed;
                entry->progress = 1.0;
                break;
            case JobEnd::Kind::Failed: entry->state = JobState::Failed; break;
            case JobEnd::Kind::Cancelled: entry->state = JobState::Cancelled; break;
            }
            if (!end.message.empty())
                entry->message = end.message;
            info = snapshot(*entry);
        }
        const std::string summary = "job " + std::string(toString(info.state)) + ": " + info.name
                                    + (info.message.empty() ? "" : " (" + info.message + ")");
        if (info.state == JobState::Failed)
            logError("simulation", summary);
        else
            logInfo("simulation", summary);
        publish(info);
        finished_.notify_all();
    }
}

DemonstratorJob::DemonstratorJob(int steps, std::chrono::milliseconds stepDuration, bool failAtEnd)
    : steps_(std::max(1, steps)), stepDuration_(stepDuration), failAtEnd_(failAtEnd)
{
}

JobEnd DemonstratorJob::run(JobContext& context)
{
    for (int step = 0; step < steps_; ++step) {
        if (context.cancelRequested())
            return JobEnd::cancelled();
        std::this_thread::sleep_for(stepDuration_);
        context.reportProgress(static_cast<double>(step + 1) / steps_,
                               "step " + std::to_string(step + 1) + " of " + std::to_string(steps_));
    }
    if (failAtEnd_)
        return JobEnd::failed("demonstrator was asked to fail");
    return JobEnd::completed("demonstrator finished; no calculation was performed");
}

}  // namespace resonant
