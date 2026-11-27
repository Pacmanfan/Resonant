// The in-app help (REQ-CHAT-20261126-010, RES-HELP-*): the strict build of
// help/src passes (links, anchors, related topics, images and screenshot
// references all resolve), broken references are detected, and the index
// answers lookups, breadcrumbs, reading order and ranked offline search.
#include "help/HelpIndex.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>

using namespace resonant::help;

namespace {

const std::filesystem::path kSource = RESONANT_SOURCE_DIR;

struct CommandRun {
    int status;
    std::string output;
};

CommandRun run(const std::string& command)
{
    std::unique_ptr<FILE, int (*)(FILE*)> pipe(popen((command + " 2>&1").c_str(), "r"), pclose);
    std::string output;
    std::array<char, 4096> buffer{};
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()))
        output += buffer.data();
    const int status = pclose(pipe.release());
    return {status, output};
}

std::filesystem::path strictBuild()
{
    static const std::filesystem::path out = std::filesystem::temp_directory_path() / "resonant-help-strict";
    static const CommandRun r = [&] {
        std::filesystem::remove_all(out);
        return run("python3 '" + (kSource / "tools/help/build_help.py").string() + "' --out '" + out.string() + "' --quiet");
    }();
    EXPECT_EQ(r.status, 0) << r.output;
    return out;
}

}  // namespace

TEST(Help, TheStrictBuildOfTheHelpSourcesPasses)
{
    const auto out = strictBuild();
    EXPECT_TRUE(std::filesystem::exists(out / "html" / "home.html"));
    EXPECT_TRUE(std::filesystem::exists(out / "html" / "images" / "screenshots" / "main-window-overview.png"));
    std::ifstream report(out / "build-report.json");
    const auto j = nlohmann::json::parse(report);
    EXPECT_TRUE(j["ok"].get<bool>());
    EXPECT_TRUE(j["errors"].empty()) << j["errors"].dump(1);
}

TEST(Help, BrokenLinksMissingImagesAndScreenshotsAreDetected)
{
    const auto src = std::filesystem::temp_directory_path() / "resonant-help-broken";
    std::filesystem::remove_all(src);
    std::filesystem::create_directories(src / "src" / "topics");
    std::ofstream(src / "src" / "categories.json") << R"({"categories": [{"folder": "topics", "id": "t", "title": "Topics"}]})";
    std::ofstream(src / "src" / "index.md") << "---\nid: home\ntitle: Home\ncategory: Home\nstatus: available\nsummary: s\n---\n[a](topic:t.one)\n";
    std::ofstream(src / "src" / "topics" / "one.md")
        << "---\nid: t.one\ntitle: One\ncategory: Topics\nstatus: planned\nsummary: s\nrelated: [t.none]\n---\n"
           "[missing](topic:t.missing) [anchor](../index.md#nowhere) [file](gone.md)\n\n![x](../images/screenshots/never.png)\n\n![y](../images/figure.png)\n";
    std::ofstream(src / "manifest.json") << R"({"screenshots": [{"name": "known.png", "page": "t.gone"}]})";
    const CommandRun r = run("python3 '" + (kSource / "tools/help/build_help.py").string() + "' --source '" + (src / "src").string() + "' --screenshots '" +
                      (src / "manifest.json").string() + "' --out '" + (src / "out").string() + "' --quiet");
    EXPECT_NE(r.status, 0);
    for (const char* expected : {"points to a missing page", "missing section", "'gone.md' points to a missing page", "related topic 't.none' does not exist",
                                 "images/figure.png' does not exist",
                                 "'never.png' is not in the screenshot manifest", "planned shows an application screenshot", "missing page 't.gone'"})
        EXPECT_NE(r.output.find(expected), std::string::npos) << expected << "\n" << r.output;
    EXPECT_FALSE(std::filesystem::exists(src / "out" / "html")) << "a failed build writes no HTML";
}

TEST(Help, TheIndexAnswersLookupsNavigationAndSearch)
{
    auto loaded = HelpIndex::load(strictBuild());
    ASSERT_TRUE(loaded.ok()) << loaded.message();
    const HelpIndex& h = loaded.value();
    EXPECT_EQ(h.home(), "home");
    EXPECT_GE(h.topics().size(), 120u);
    EXPECT_EQ(h.categories().size(), 12u);
    const Topic* focusing = h.find("acoustics.focusing");
    ASSERT_NE(focusing, nullptr);
    EXPECT_EQ(focusing->status, "available");
    EXPECT_FALSE(focusing->previous.empty());
    EXPECT_FALSE(focusing->next.empty());
    EXPECT_TRUE(std::filesystem::exists(h.pathOf("acoustics.focusing")));
    EXPECT_EQ(h.breadcrumbs("acoustics.focusing"), (std::vector<std::string>{"Help", "Acoustic Engineering", "Focusing"}));
    EXPECT_EQ(h.find("no.such.topic"), nullptr);
    // Every topic named by a category exists, and the reading order is one chain.
    size_t chain = 0;
    for (const Topic* t = h.find(h.home()); t && chain < 1000; t = t->next.empty() ? nullptr : h.find(t->next))
        ++chain;
    EXPECT_EQ(chain, h.topics().size());
    // Ranked search: a title match first; every word must match; a prefix while typing.
    auto hits = h.search("modal analysis");
    ASSERT_FALSE(hits.empty());
    EXPECT_EQ(hits.front().topic, "mechanics.modal");
    EXPECT_FALSE(hits.front().matchedTerms.empty());
    hits = h.search("parabolic refl");
    ASSERT_FALSE(hits.empty());
    EXPECT_EQ(hits.front().topic, "acoustics.reflectors");
    EXPECT_TRUE(h.search("xyzzy-nothing").empty());
    EXPECT_TRUE(h.search("the and of").empty());  // stop words only
    for (const auto& hit : h.search("dipole impedance"))
        EXPECT_NE(h.find(hit.topic), nullptr);
}

TEST(Help, AMissingBuildIsReportedClearly)
{
    auto loaded = HelpIndex::load(std::filesystem::temp_directory_path() / "resonant-no-help-here");
    ASSERT_FALSE(loaded.ok());
    EXPECT_NE(loaded.message().find("has not been built"), std::string::npos);
}
