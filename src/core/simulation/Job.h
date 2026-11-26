// Background jobs. A Job is a long calculation run on a worker thread of a
// JobManager, so the user interface stays responsive. A job moves through
//
//     Queued -> Running -> Completed | Failed | Cancelled
//     Queued -> Cancelled                       (cancelled before it started)
//
// reports progress, and is asked to stop through its JobContext.
#pragma once

#include "foundation/Id.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace resonant {

enum class JobState { Queued, Running, Completed, Failed, Cancelled };

std::string_view toString(JobState state);
bool isFinished(JobState state);

class JobContext {
public:
    virtual ~JobContext() = default;
    // True once cancellation was requested. A job checks this regularly and
    // returns JobEnd::Cancelled promptly.
    virtual bool cancelRequested() const = 0;
    // fraction in [0, 1]; message is optional status text.
    virtual void reportProgress(double fraction, std::string_view message = {}) = 0;
};

struct JobEnd {
    enum class Kind { Completed, Failed, Cancelled };
    Kind kind = Kind::Completed;
    std::string message;

    static JobEnd completed(std::string message = {}) { return {Kind::Completed, std::move(message)}; }
    static JobEnd failed(std::string message) { return {Kind::Failed, std::move(message)}; }
    static JobEnd cancelled() { return {Kind::Cancelled, "cancelled"}; }
};

class Job {
public:
    virtual ~Job() = default;
    virtual std::string name() const = 0;
    // Runs on a worker thread. An exception that escapes is recorded as a
    // failure with its message.
    virtual JobEnd run(JobContext& context) = 0;
};

struct JobInfo {
    Id id;
    std::string name;
    JobState state = JobState::Queued;
    double progress = 0.0;
    std::string message;
    double elapsedSeconds = 0.0;  // running time so far, or total once finished
};

class JobManager {
public:
    // Called whenever a job's state, progress or message changes. Runs on
    // the thread that caused the change (usually a worker thread); a user
    // interface must forward it to its own thread.
    using Listener = std::function<void(const JobInfo&)>;

    explicit JobManager(unsigned workerCount = 1);
    ~JobManager();  // cancels everything and joins the workers
    JobManager(const JobManager&) = delete;
    JobManager& operator=(const JobManager&) = delete;

    Id submit(std::shared_ptr<Job> job);

    // Requests cancellation. A queued job is cancelled at once; a running
    // job when it next checks. Returns false for an unknown or finished job.
    bool cancel(const Id& id);

    std::optional<JobInfo> info(const Id& id) const;
    std::vector<JobInfo> jobs() const;  // in submission order
    bool hasUnfinishedJobs() const;

    // Blocks until the job is finished (or the timeout passes). Returns the
    // job's information, or nullopt for an unknown job.
    std::optional<JobInfo> wait(const Id& id, std::chrono::milliseconds timeout = std::chrono::milliseconds::max());

    int addListener(Listener listener);
    void removeListener(int handle);

private:
    struct Entry;
    class Context;

    void workerLoop();
    void publish(const JobInfo& info);
    static JobInfo snapshot(const Entry& entry);

    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::condition_variable finished_;
    std::vector<std::shared_ptr<Entry>> entries_;
    std::deque<std::shared_ptr<Entry>> queue_;
    std::vector<std::thread> workers_;
    bool stopping_ = false;

    std::mutex listenerMutex_;
    std::vector<std::pair<int, Listener>> listeners_;
    int nextListener_ = 1;
};

// A job that exercises the job system and nothing else: it waits through a
// number of steps, reporting progress and honouring cancellation. It
// performs no calculation and produces no result; it is not a simulation.
class DemonstratorJob final : public Job {
public:
    DemonstratorJob(int steps, std::chrono::milliseconds stepDuration, bool failAtEnd = false);

    std::string name() const override { return "Job-system demonstrator (no physics)"; }
    JobEnd run(JobContext& context) override;

private:
    int steps_;
    std::chrono::milliseconds stepDuration_;
    bool failAtEnd_;
};

}  // namespace resonant
