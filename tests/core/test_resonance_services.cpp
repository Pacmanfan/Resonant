// Resonance through the application services (RES-RSN-001, RES-RSN-013):
// identification of user signals and spectra, the refusals, the resonance
// analyses as jobs whose results carry natural and driven modes with their
// loss breakdown, and permissions.
#include "project/Project.h"
#include "project/ProjectSerializer.h"
#include "services/Services.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <thread>

using namespace resonant;
using namespace resonant::services;
using nlohmann::json;

namespace {
constexpr double kPi = 3.14159265358979323846;

std::unique_ptr<Project> example(const std::string& file)
{
    auto loaded = loadProjectFile(std::filesystem::path(RESONANT_SOURCE_DIR) / "examples" / "resonance" / file);
    EXPECT_TRUE(loaded) << loaded.message();
    return std::move(loaded.value().project);
}

struct Fixture {
    OwnedProjectHost host;
    Session session{host};
    Services& s = session.services();
    explicit Fixture(std::unique_ptr<Project> p) : host(std::move(p), 2) { host.setSession(&session); }
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
        for (int k = 0; k < 3000; ++k) {
            if (!host.jobs().hasUnfinishedJobs()) {
                session.collectFinishedJobs();
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return false;
    }
};
}  // namespace

TEST(ResonanceServices, SignalsAndSpectraAreIdentified)
{
    Fixture f(Project::createNew("resonance"));
    std::vector<double> x;
    for (int i = 0; i < 4000; ++i) {
        const double t = i * 1e-5;
        x.push_back(std::exp(-kPi * 800.0 / 40.0 * t) * std::cos(2 * kPi * 800.0 * t));
    }
    auto id = f.s.resonance.identify(f.reader(), {{"samples", x}, {"sampleInterval", 1e-5}, {"minHz", 100.0}, {"maxHz", 4000.0}});
    ASSERT_TRUE(id) << id.error().message;
    EXPECT_EQ(id.value()["method"], "matrix-pencil");
    ASSERT_EQ(id.value()["resonances"].size(), 1u);
    EXPECT_NEAR(id.value()["resonances"][0]["frequencyHz"].get<double>(), 800.0, 1e-6);
    EXPECT_NEAR(id.value()["resonances"][0]["quality"].get<double>(), 40.0, 1e-6);
    std::vector<double> fr, y;
    for (int i = 0; i <= 600; ++i) {
        fr.push_back(700.0 + 0.5 * i);
        const double u = (fr.back() - 850.0) / (850.0 / 200.0);
        y.push_back(1.0 / (1.0 + u * u));
    }
    auto spectrum = f.s.resonance.identify(f.local(), {{"frequenciesHz", fr}, {"values", y}});
    ASSERT_TRUE(spectrum) << spectrum.error().message;
    EXPECT_EQ(spectrum.value()["method"], "lorentzian-fit");
    EXPECT_NEAR(spectrum.value()["resonances"][0]["quality"].get<double>(), 100.0, 1e-6);  // f0 / (2 g) with g = f0 / 200
    // Refusals with the reason.
    auto shortRecord = f.s.resonance.identify(f.local(), {{"samples", std::vector<double>(100, 0.1)}, {"sampleInterval", 1e-5}, {"minHz", 100.0}});
    ASSERT_FALSE(shortRecord);
    EXPECT_EQ(shortRecord.error().code, ErrorCode::InvalidArgument);
    EXPECT_FALSE(f.s.resonance.identify(f.local(), json::object()));
    EXPECT_FALSE(f.s.resonance.identify(f.local(), {{"frequenciesHz", {1.0, 2.0}}, {"values", {1.0}}}));
}

TEST(ResonanceServices, ResonanceAnalysesRunAsJobsAndReportTheirModes)
{
    Fixture f(example("a-helmholtz-resonator.resonant"));
    auto job = f.s.functional.analyze(f.local(), "fs-helmholtz", "");
    ASSERT_TRUE(job) << job.error().message;
    ASSERT_TRUE(f.waitForJobs());
    auto view = f.s.jobs.get(f.local(), job.value().id).value();
    ASSERT_EQ(view.state, "completed");
    const json d = f.s.functional.result(f.local(), view.resultIds.at(0)).value();
    ASSERT_GE(d["modes"].size(), 2u);
    EXPECT_EQ(d["modes"][0]["nature"], "natural");
    EXPECT_EQ(d["modes"][1]["nature"], "driven-peak");
    EXPECT_GT(d["modes"][0]["lossQ"]["radiation"].get<double>(), d["modes"][0]["lossQ"]["viscous (neck)"].get<double>());
    EXPECT_FALSE(d["stale"].get<bool>());
    // Tuning the geometry through the service makes the result stale.
    ASSERT_TRUE(f.s.functional.setParameters(f.local(), "fs-helmholtz", {{"neckLength", 0.02}}, std::nullopt));
    EXPECT_TRUE(f.s.functional.result(f.local(), view.resultIds.at(0)).value()["stale"].get<bool>());
    // Capabilities list the resonance analyses.
    bool listed = false;
    const auto capabilities = f.s.application.capabilities(f.local(), "functional-structure");
    ASSERT_TRUE(capabilities);
    for (const auto& c : capabilities.value())
        listed = listed || c.id == "functional.analysis.acoustic.helmholtz";
    EXPECT_TRUE(listed);
}
