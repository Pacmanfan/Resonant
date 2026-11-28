// A session on a copy of an example project (with its data files) in a temporary directory,
// for the laboratory tests: imports write the measurement store next to the project file.
#pragma once

#include "project/ProjectSerializer.h"
#include "services/Services.h"
#include "services/Session.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <random>
#include <thread>

namespace laboratory_test {

using namespace resonant;
using namespace resonant::services;
using nlohmann::json;
namespace fs = std::filesystem;

inline fs::path temporaryCopy(const std::string& group, const std::string& file)
{
    std::random_device rd;
    const fs::path dir = fs::temp_directory_path() / ("resonant-lab-" + std::to_string(rd()) + std::to_string(rd()));
    fs::create_directories(dir);
    const fs::path source = fs::path(RESONANT_SOURCE_DIR) / "examples" / group;
    fs::copy_file(source / file, dir / file);
    if (fs::exists(source / "data"))
        fs::copy(source / "data", dir / "data", fs::copy_options::recursive);
    return dir;
}

inline std::unique_ptr<Project> loadAt(const fs::path& file)
{
    auto loaded = loadProjectFile(file);
    EXPECT_TRUE(loaded) << loaded.message();
    return std::move(loaded.value().project);
}

struct Fixture {
    fs::path dir, file;
    OwnedProjectHost host;
    Session session{host};
    Services& s = session.services();
    explicit Fixture(const std::string& name, const std::string& group = "laboratory")
        : dir(temporaryCopy(group, name)), file(dir / name), host(loadAt(dir / name), 2)
    {
        host.setFilePath(file.string());
        host.setSession(&session);
    }
    ~Fixture()
    {
        std::error_code ec;
        fs::remove_all(dir, ec);
    }
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
        for (int k = 0; k < 12000; ++k) {
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
        return view && !view.value().resultIds.empty() ? view.value().resultIds[0] : std::string();
    }
    // Imports a file of the copy (relative to the project), for a study and a role; the report.
    json import(const std::string& path, const std::string& study = {}, const std::string& role = "raw", bool partial = false)
    {
        json spec{{"path", path}, {"role", role}, {"acceptPartial", partial}};
        if (!study.empty())
            spec["study"] = study;
        auto r = s.laboratory.importDataset(local(), spec);
        EXPECT_TRUE(r) << (r ? "" : r.error().message);
        return r ? r.value().result : json::object();
    }
    // The dataset of an import report for a column: ids end with -<column>-<8 hex digits of the file's digest>.
    static std::string datasetOf(const json& report, const std::string& column = "")
    {
        for (const json& d : report.value("datasets", json::array())) {
            const std::string id = d.get<std::string>();
            const std::string stem = id.size() > 9 ? id.substr(0, id.size() - 9) : id;  // without -<digest>
            if (column.empty() || (stem.size() > column.size() && stem.compare(stem.size() - column.size() - 1, column.size() + 1, "-" + column) == 0))
                return id;
        }
        ADD_FAILURE() << "no dataset for the column '" << column << "' in " << report.dump();
        return {};
    }
    json calibrate(const json& spec)
    {
        const std::string id = resultOf(s.laboratory.calibrate(local(), spec));
        auto kept = s.laboratory.acceptCalibration(local(), id);
        EXPECT_TRUE(kept) << (kept ? "" : kept.error().message);
        return kept ? kept.value().result : json::object();
    }
};

inline const json* parameter(const json& calibration, const std::string& id)
{
    for (const json& p : calibration["result"]["parameters"])
        if (p["id"] == id)
            return &p;
    return nullptr;
}

}  // namespace laboratory_test
