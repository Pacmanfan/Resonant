// Every project under examples/ must load in the current version.
#include "project/ProjectSerializer.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

using namespace resonant;

TEST(Examples, EveryExampleProjectLoads)
{
    const std::filesystem::path root = std::filesystem::path(RESONANT_SOURCE_DIR) / "examples";
    ASSERT_TRUE(std::filesystem::exists(root)) << root;
    int count = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (entry.path().extension() != std::string(".") + kProjectFileExtension)
            continue;
        ++count;
        auto loaded = loadProjectFile(entry.path());
        ASSERT_TRUE(loaded.ok()) << entry.path() << ": " << loaded.message();
        EXPECT_TRUE(loaded.value().warnings.empty()) << entry.path();
        EXPECT_GT(loaded.value().project->scene.objectCount(), 0u) << entry.path();
        // Saving what was loaded reproduces the file's content.
        EXPECT_EQ(serializeProject(*loaded.value().project)["scene"],
                  nlohmann::json::parse(std::ifstream(entry.path()))["scene"])
            << entry.path();
    }
    EXPECT_GT(count, 0);
}
