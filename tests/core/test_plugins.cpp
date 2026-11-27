// The plugin manager (REQ-CHAT-20261126-010, RES-PLUG-001..008): discovery,
// manifest and version validation, dependencies, explicit enabling, workflow
// and native loading, and failure reporting that leaves everything else intact.
#include "automation/WorkflowEngine.h"
#include "foundation/Version.h"
#include "plugins/PluginManager.h"
#include "project/Project.h"

#include <gtest/gtest.h>

#include <filesystem>

using namespace resonant;
using namespace resonant::plugins;

namespace {

const std::filesystem::path kSource = RESONANT_SOURCE_DIR;

std::filesystem::path freshConfig(const std::string& name)
{
    const auto file = std::filesystem::temp_directory_path() / ("resonant-plugins-" + name) / "plugins.json";
    std::filesystem::remove_all(file.parent_path());
    return file;
}

PluginManager manager(const std::string& name)
{
    return PluginManager({kSource / "plugins" / "examples", kSource / "tests" / "data" / "plugins"}, freshConfig(name), kApplicationVersion);
}

}  // namespace

TEST(Plugins, VersionsParseAndCompare)
{
    EXPECT_EQ(Version::parse("1.2.3")->str(), "1.2.3");
    EXPECT_EQ(Version::parse("2")->str(), "2.0.0");
    EXPECT_FALSE(Version::parse("one").has_value());
    EXPECT_FALSE(Version::parse("1.x").has_value());
    EXPECT_LT(*Version::parse("0.9.9"), *Version::parse("0.10.0"));
}

TEST(Plugins, DiscoveryValidatesEveryPluginAndLoadsNothingUnasked)
{
    PluginManager m = manager("discover");
    const size_t workflowsBefore = automation::WorkflowLibrary::instance().workflows().size();
    m.discover({"python"});
    const auto state = [&](const std::string& id) {
        const PluginRecord* r = m.find(id);
        EXPECT_NE(r, nullptr) << id;
        return r ? r->state : PluginState::Invalid;
    };
    // The examples are found but stay disabled until the user enables them.
    for (const char* id : {"examples.spiral-array-generator", "examples.array-report", "examples.csv-field-importer", "examples.workflow-pack"})
        EXPECT_EQ(state(id), PluginState::Disabled) << id;
    EXPECT_EQ(state("test.wrong-api"), PluginState::Incompatible);
    EXPECT_EQ(state("test.too-new"), PluginState::Incompatible);
    EXPECT_EQ(state("test.needs-missing"), PluginState::MissingDependency);
    EXPECT_NE(m.find("test.needs-missing")->message.find("test.not-installed"), std::string::npos);
    EXPECT_EQ(state("bad-manifest"), PluginState::Invalid);  // named after its directory
    m.loadEnabled();
    EXPECT_EQ(automation::WorkflowLibrary::instance().workflows().size(), workflowsBefore);
    // Without Python, Python plugins cannot run.
    PluginManager noPython = manager("nopython");
    noPython.discover({});
    EXPECT_EQ(noPython.find("examples.array-report")->state, PluginState::Incompatible);
}

TEST(Plugins, AnEnabledWorkflowPluginAddsWorkflowsThatRunWithRealActions)
{
    PluginManager m = manager("workflows");
    ASSERT_TRUE(m.setEnabled("examples.workflow-pack", true).ok());
    m.discover({"python"});
    EXPECT_EQ(m.find("examples.workflow-pack")->state, PluginState::Ready);
    m.loadEnabled();
    const PluginRecord* r = m.find("examples.workflow-pack");
    ASSERT_EQ(r->state, PluginState::Loaded) << r->message;
    EXPECT_EQ(r->extensions.size(), 2u);
    auto sweep = automation::WorkflowLibrary::instance().find("examples.acoustic-spacing-sweep");
    ASSERT_TRUE(sweep.has_value());
    EXPECT_EQ(automation::WorkflowLibrary::instance().sourceOf("examples.acoustic-spacing-sweep"), "plugin examples.workflow-pack");
    auto project = Project::createNew("sweep");
    const auto dir = std::filesystem::temp_directory_path() / "resonant-plugin-sweep";
    auto run = automation::runWorkflow(*project, *sweep, {}, nullptr, dir);
    ASSERT_TRUE(run.ok()) << run.message();
    EXPECT_EQ(run.value().status, automation::RunStatus::Completed) << run.value().message;
    EXPECT_EQ(run.value().outputs["fields"].size(), 4u);
    auto check = automation::runWorkflow(*project, *automation::WorkflowLibrary::instance().find("examples.em-frequency-check"));
    ASSERT_TRUE(check.ok()) << check.message();
    EXPECT_EQ(check.value().status, automation::RunStatus::Completed) << check.value().message;
    EXPECT_EQ(check.value().outputs["report"].size(), 3u);
    // The configuration persists; disabling takes effect at the next discovery.
    ASSERT_TRUE(m.setEnabled("examples.workflow-pack", false).ok());
    m.discover({"python"});
    EXPECT_EQ(m.find("examples.workflow-pack")->state, PluginState::Disabled);
}

TEST(Plugins, AFailingNativePluginIsReportedAndOthersStillLoad)
{
    PluginManager m = manager("native");
    ASSERT_TRUE(m.setEnabled("test.bad-native", true).ok());
    ASSERT_TRUE(m.setEnabled("examples.workflow-pack", true).ok());
    m.discover({"python"});
    m.loadEnabled();
    const PluginRecord* bad = m.find("test.bad-native");
    EXPECT_EQ(bad->state, PluginState::Failed);
    EXPECT_NE(bad->message.find("could not be loaded"), std::string::npos) << bad->message;
    EXPECT_EQ(m.find("examples.workflow-pack")->state, PluginState::Loaded);
    // Python plugins wait for the worker; their directories are handed over.
    ASSERT_TRUE(m.setEnabled("examples.array-report", true).ok());
    m.discover({"python"});
    const auto ready = m.readyPythonPlugins();
    ASSERT_EQ(ready.size(), 1u);
    EXPECT_EQ(ready.front().filename(), "array-report");
    m.markFailed("examples.array-report", "register() raised");
    EXPECT_EQ(m.find("examples.array-report")->state, PluginState::Failed);
}

TEST(Plugins, ManifestsAreChecked)
{
    EXPECT_TRUE(readManifest(kSource / "plugins" / "examples" / "spiral-array-generator").ok());
    EXPECT_FALSE(readManifest(kSource / "tests" / "data" / "plugins" / "bad-manifest").ok());
    EXPECT_FALSE(readManifest(kSource / "tests" / "data").ok());
    const auto m = readManifest(kSource / "plugins" / "examples" / "csv-field-importer").take();
    EXPECT_EQ(toJson(m)["provides"], nlohmann::json::array({"measurement-importer"}));
    EXPECT_FALSE(m.permissions.empty());
    // A second plugin with the same id (here: the examples twice) is refused.
    PluginManager twice({kSource / "plugins" / "examples", kSource / "plugins" / "examples"}, freshConfig("twice"), kApplicationVersion);
    twice.discover({"python"});
    int invalid = 0;
    for (const PluginRecord& r : twice.plugins())
        invalid += r.state == PluginState::Invalid ? 1 : 0;
    EXPECT_EQ(invalid, 4);
}
