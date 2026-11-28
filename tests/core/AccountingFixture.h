// A session on an accounting example project, for the accounting service tests.
#pragma once

#include "project/Project.h"
#include "project/ProjectSerializer.h"
#include "services/Services.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <thread>

namespace accounting_test {

using namespace resonant;
using namespace resonant::services;
using nlohmann::json;

inline std::unique_ptr<Project> example(const std::string& file)
{
    auto loaded = loadProjectFile(std::filesystem::path(RESONANT_SOURCE_DIR) / "examples" / "accounting" / file);
    EXPECT_TRUE(loaded) << loaded.message();
    return std::move(loaded.value().project);
}

struct Fixture {
    OwnedProjectHost host;
    Session session{host};
    Services& s = session.services();
    explicit Fixture(const std::string& file) : host(example(file), 2) { host.setSession(&session); }
    CallContext local() const { return CallContext{}; }
    CallContext reader()
    {
        auto opened = session.openSession("reader", Interface::Grpc, static_cast<std::uint32_t>(Permission::Read), kDefaultRemotePermissions);
        CallContext c;
        c.sessionId = opened.value().id;
        c.source = Interface::Grpc;
        return c;
    }
    bool waitForJobs()
    {
        for (int k = 0; k < 6000; ++k) {
            if (!host.jobs().hasUnfinishedJobs()) {
                session.collectFinishedJobs();
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return false;
    }
    std::string resultOf(const Outcome<JobView>& job)
    {
        EXPECT_TRUE(job) << (job ? "" : job.error().message);
        if (!job)
            return {};
        EXPECT_TRUE(waitForJobs());
        auto view = s.jobs.get(local(), job.value().id);
        EXPECT_TRUE(view);
        EXPECT_EQ(view.value().state, "completed") << (view.value().error ? view.value().error->message : "");
        return view.value().resultIds.empty() ? std::string() : view.value().resultIds[0];
    }
    json ran(const Outcome<JobView>& job)
    {
        const std::string id = resultOf(job);
        auto r = s.accounting.result(local(), id);
        EXPECT_TRUE(r) << (r ? "" : r.error().message);
        return r ? r.value() : json::object();
    }
    const Material* material(const std::string& id) { return host.project().materials.find(Id(id)); }
};

inline const json* curve(const json& d, const std::string& id)
{
    for (const json& c : d["curves"])
        if (c["id"] == id)
            return &c;
    return nullptr;
}

}  // namespace accounting_test
