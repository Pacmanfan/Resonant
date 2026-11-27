// Demonstrations (REQ-CHAT-20261126-010, RES-DEMO-*): every bundled profile
// and sequence loads and validates; broken ones are refused with the reason;
// run manifests carry what a recording needs.
#include "demo/Demo.h"
#include "foundation/Version.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <set>

using namespace resonant::demo;

namespace {
const std::filesystem::path kSource = RESONANT_SOURCE_DIR;
}

TEST(Demo, EveryBundledDemonstrationLoadsAndTellsItsStory)
{
    const DemoLibrary library = loadDemoLibrary(kSource);
    EXPECT_TRUE(library.errors.empty()) << library.errors.front();
    ASSERT_EQ(library.demos.size(), 8u);
    std::set<std::string> domains;
    for (const LoadedDemo& d : library.demos) {
        SCOPED_TRACE(d.profile.id);
        EXPECT_TRUE(d.profile.validate().ok());
        EXPECT_TRUE(d.sequence.validate().ok());
        EXPECT_FALSE(d.sequence.steps.empty());
        EXPECT_TRUE(std::filesystem::exists(kSource / d.profile.project)) << d.profile.project;
        EXPECT_TRUE(std::filesystem::exists(kSource / d.profile.notes)) << d.profile.notes;
        EXPECT_FALSE(d.profile.story.meaning.empty());
        EXPECT_FALSE(d.profile.story.inspect.empty());
        EXPECT_FALSE(d.profile.story.value.empty());
        for (const auto& shot : d.profile.screenshots)
            EXPECT_TRUE(std::filesystem::exists(kSource / "help/src/images/screenshots" / shot)) << shot;
        // Every project a step opens exists.
        for (const DemoStep& s : d.sequence.steps)
            if (s.action == "open-project")
                EXPECT_TRUE(std::filesystem::exists(kSource / s.params["path"].get<std::string>()));
        domains.insert(d.profile.domain);
        // A round trip keeps the profile and the sequence.
        EXPECT_EQ(toJson(profileFromJson(toJson(d.profile)).take()), toJson(d.profile));
        EXPECT_EQ(toJson(sequenceFromJson(toJson(d.sequence)).take()), toJson(d.sequence));
    }
    for (const char* domain : {"acoustic", "mechanical", "electromagnetic", "cross-domain", "optimization", "experimental", "automation"})
        EXPECT_TRUE(domains.contains(domain)) << domain;
    EXPECT_NE(library.find("demo-g-experimental-correlation"), nullptr);
    EXPECT_EQ(library.find("demo-g-experimental-correlation")->profile.status, "partial");
    EXPECT_FALSE(library.find("demo-g-experimental-correlation")->profile.remaining.empty());
}

TEST(Demo, BrokenSequencesAndProfilesAreRefusedWithTheReason)
{
    nlohmann::json seq = {{"format", kSequenceFormat}, {"version", 1}, {"id", "s"}, {"steps", {{{"id", "a"}, {"action", "teleport"}}}}};
    auto r = sequenceFromJson(seq);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.message().find("unknown action 'teleport'"), std::string::npos);
    seq["steps"] = {{{"id", "a"}, {"action", "run"}}};
    EXPECT_NE(sequenceFromJson(seq).message().find("needs the parameter 'what'"), std::string::npos);
    seq["steps"] = {{{"id", "a"}, {"action", "pause"}}, {{"id", "a"}, {"action", "pause"}}};
    EXPECT_NE(sequenceFromJson(seq).message().find("used twice"), std::string::npos);
    seq["steps"] = {{{"id", "a"}, {"action", "capture"}, {"params", {{"name", "../escape.png"}}}}};
    EXPECT_NE(sequenceFromJson(seq).message().find("file name ending in .png"), std::string::npos);
    seq["steps"] = {{{"id", "a"}, {"action", "pause"}, {"timeoutSeconds", 0}}};
    EXPECT_NE(sequenceFromJson(seq).message().find("timeout must be positive"), std::string::npos);
    seq["format"] = "something-else";
    EXPECT_FALSE(sequenceFromJson(seq).ok());
    nlohmann::json profile = {{"format", kProfileFormat}, {"version", 1}, {"id", "p"}, {"name", "P"}, {"status", "partial"}, {"sequence", "x.json"},
                              {"story", {{"problem", "a"}, {"calculate", "b"}, {"assumptions", "c"}}}};
    EXPECT_NE(profileFromJson(profile).message().find("says what remains"), std::string::npos);
    profile["status"] = "available";
    profile["story"].erase("assumptions");
    EXPECT_NE(profileFromJson(profile).message().find("assumptions"), std::string::npos);
}

TEST(Demo, RunManifestsCarryWhatARecordingNeeds)
{
    DemoRun run;
    run.profileId = "demo-b-acoustic-focusing";
    run.sequenceId = "demo-b-acoustic-focusing";
    run.sequenceVersion = "1";
    run.applicationVersion = resonant::kApplicationVersion;
    run.projectFile = "examples/acoustics/b-linear-array.resonant";
    run.projectSha256 = sha256OfFile(kSource / run.projectFile);
    run.status = "completed";
    run.screenshots = {"focus.png"};
    run.notes = {"open: say what the array is"};
    StepRun s;
    s.id = "open";
    s.action = "open-project";
    run.steps.push_back(s);
    const nlohmann::json j = toJson(run);
    EXPECT_EQ(j["format"], kRunFormat);
    for (const char* key : {"profile", "sequenceVersion", "applicationVersion", "project", "simulation", "screenshots", "notes", "status", "startedUtc",
                            "finishedUtc", "steps"})
        EXPECT_TRUE(j.contains(key)) << key;
    EXPECT_EQ(j["project"]["sha256"].get<std::string>().size(), 64u);
    EXPECT_EQ(j["steps"][0]["outcome"], "done");
}

TEST(Demo, Sha256MatchesTheStandardTestVectors)
{
    const auto file = std::filesystem::temp_directory_path() / "resonant-sha-test";
    std::ofstream(file, std::ios::binary) << "abc";
    EXPECT_EQ(sha256OfFile(file), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    std::ofstream(file, std::ios::binary | std::ios::trunc) << "";
    EXPECT_EQ(sha256OfFile(file), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    std::ofstream(file, std::ios::binary | std::ios::trunc) << std::string(1000, 'a');
    EXPECT_EQ(sha256OfFile(file), "41edece42d63e8d9bf515a9ba6932e1c20cbc9f5a5d134645adb5db1b9737ea3");
    EXPECT_EQ(sha256OfFile(file.string() + "-missing"), "");
}
