#include "simulation/Job.h"
#include "simulation/Simulation.h"

#include <gtest/gtest.h>

#include <atomic>
#include <stdexcept>

using namespace resonant;
using namespace std::chrono_literals;

namespace {

// Records every state a job passes through, in order.
struct StateRecorder {
    explicit StateRecorder(JobManager& manager)
    {
        manager.addListener([this](const JobInfo& info) {
            std::lock_guard lock(mutex);
            auto& states = history[info.id.str()];
            if (states.empty() || states.back() != info.state)
                states.push_back(info.state);
            if (info.progress > maxProgress[info.id.str()])
                maxProgress[info.id.str()] = info.progress;
        });
    }
    std::vector<JobState> statesOf(const Id& id)
    {
        std::lock_guard lock(mutex);
        return history[id.str()];
    }
    std::mutex mutex;
    std::map<std::string, std::vector<JobState>> history;
    std::map<std::string, double> maxProgress;
};

class ThrowingJob final : public Job {
public:
    std::string name() const override { return "throwing"; }
    JobEnd run(JobContext&) override { throw std::runtime_error("boom"); }
};

// Runs until cancelled; signals when it has started.
class BlockingJob final : public Job {
public:
    explicit BlockingJob(std::atomic<bool>& started) : started_(started) {}
    std::string name() const override { return "blocking"; }
    JobEnd run(JobContext& context) override
    {
        started_.store(true);
        while (!context.cancelRequested())
            std::this_thread::sleep_for(1ms);
        return JobEnd::cancelled();
    }

private:
    std::atomic<bool>& started_;
};

}  // namespace

TEST(Jobs, CompletedJobPassesThroughQueuedRunningCompleted)
{
    JobManager manager;
    StateRecorder recorder(manager);
    const Id id = manager.submit(std::make_shared<DemonstratorJob>(5, 1ms));
    const auto info = manager.wait(id, 5s);
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->state, JobState::Completed);
    EXPECT_DOUBLE_EQ(info->progress, 1.0);
    EXPECT_GT(info->elapsedSeconds, 0.0);
    EXPECT_NE(info->message.find("no calculation"), std::string::npos);
    EXPECT_EQ(recorder.statesOf(id), (std::vector<JobState>{JobState::Queued, JobState::Running, JobState::Completed}));
    EXPECT_FALSE(manager.hasUnfinishedJobs());
}

TEST(Jobs, FailureAndExceptionsEndInFailedWithAMessage)
{
    JobManager manager;
    const Id failing = manager.submit(std::make_shared<DemonstratorJob>(2, 1ms, true));
    const Id throwing = manager.submit(std::make_shared<ThrowingJob>());

    const auto failed = manager.wait(failing, 5s);
    ASSERT_TRUE(failed.has_value());
    EXPECT_EQ(failed->state, JobState::Failed);
    EXPECT_FALSE(failed->message.empty());

    const auto thrown = manager.wait(throwing, 5s);
    ASSERT_TRUE(thrown.has_value());
    EXPECT_EQ(thrown->state, JobState::Failed);
    EXPECT_NE(thrown->message.find("boom"), std::string::npos);
}

TEST(Jobs, RunningJobIsCancelledPromptly)
{
    JobManager manager;
    StateRecorder recorder(manager);
    std::atomic<bool> started{false};
    const Id id = manager.submit(std::make_shared<BlockingJob>(started));
    while (!started.load())
        std::this_thread::sleep_for(1ms);
    EXPECT_EQ(manager.info(id)->state, JobState::Running);

    EXPECT_TRUE(manager.cancel(id));
    const auto info = manager.wait(id, 5s);
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->state, JobState::Cancelled);
    EXPECT_EQ(recorder.statesOf(id), (std::vector<JobState>{JobState::Queued, JobState::Running, JobState::Cancelled}));
    EXPECT_FALSE(manager.cancel(id));  // already finished
}

TEST(Jobs, QueuedJobIsCancelledWithoutRunning)
{
    JobManager manager(1);
    StateRecorder recorder(manager);
    std::atomic<bool> started{false};
    const Id blocker = manager.submit(std::make_shared<BlockingJob>(started));
    while (!started.load())
        std::this_thread::sleep_for(1ms);

    std::atomic<bool> secondStarted{false};
    const Id queued = manager.submit(std::make_shared<BlockingJob>(secondStarted));
    EXPECT_EQ(manager.info(queued)->state, JobState::Queued);
    EXPECT_TRUE(manager.cancel(queued));
    EXPECT_EQ(manager.info(queued)->state, JobState::Cancelled);
    EXPECT_FALSE(secondStarted.load());
    EXPECT_EQ(recorder.statesOf(queued), (std::vector<JobState>{JobState::Queued, JobState::Cancelled}));

    EXPECT_TRUE(manager.cancel(blocker));
    EXPECT_EQ(manager.wait(blocker, 5s)->state, JobState::Cancelled);
}

TEST(Jobs, ProgressIsReportedAndJobsRunInSubmissionOrder)
{
    JobManager manager(1);
    StateRecorder recorder(manager);
    const Id first = manager.submit(std::make_shared<DemonstratorJob>(4, 1ms));
    const Id second = manager.submit(std::make_shared<DemonstratorJob>(4, 1ms));
    ASSERT_EQ(manager.wait(second, 5s)->state, JobState::Completed);
    EXPECT_EQ(manager.info(first)->state, JobState::Completed);
    EXPECT_DOUBLE_EQ(recorder.maxProgress[first.str()], 1.0);

    const auto jobs = manager.jobs();
    ASSERT_EQ(jobs.size(), 2u);
    EXPECT_EQ(jobs[0].id, first);
    EXPECT_EQ(jobs[1].id, second);
}

TEST(Jobs, DestroyingTheManagerStopsRunningJobs)
{
    std::atomic<bool> started{false};
    {
        JobManager manager;
        manager.submit(std::make_shared<BlockingJob>(started));
        while (!started.load())
            std::this_thread::sleep_for(1ms);
    }  // must not hang
    SUCCEED();
}

TEST(Jobs, UnknownJobIsReported)
{
    JobManager manager;
    EXPECT_FALSE(manager.info(Id("nope")).has_value());
    EXPECT_FALSE(manager.cancel(Id("nope")));
    EXPECT_FALSE(manager.wait(Id("nope"), 10ms).has_value());
}

TEST(Simulation, ConfigurationAndMetadataSerialize)
{
    SimulationConfig config;
    config.id = Id("sim-1");
    config.name = "Config";
    config.domain = PhysicsDomain::Acoustic;
    config.solverId = "demo";
    config.parameters = {{"n", 3}};
    const auto json = toJson(config);
    EXPECT_EQ(json["id"], "sim-1");
    EXPECT_EQ(json["domain"], "acoustic");
    EXPECT_EQ(json["parameters"]["n"], 3);

    ResultMetadata metadata;
    metadata.simulationId = config.id;
    metadata.solverId = "demo";
    metadata.assumptions = {"free field"};
    EXPECT_EQ(toJson(metadata)["assumptions"][0], "free field");
}
