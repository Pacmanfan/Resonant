// Every project under examples/ must load in the current version and hold
// something: scene objects or experiments.
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
        // Not empty: scene objects, or experiments (examples/experiments holds experiment definitions).
        const auto& project = *loaded.value().project;
        const bool hasExperiments = project.extensions.contains("experiments") &&
                                    !project.extensions["experiments"].value("experiments", nlohmann::json::array()).empty();
        EXPECT_TRUE(project.scene.objectCount() > 0u || hasExperiments) << entry.path();
        // Saving what was loaded reproduces the file's content.
        EXPECT_EQ(serializeProject(*loaded.value().project)["scene"],
                  nlohmann::json::parse(std::ifstream(entry.path()))["scene"])
            << entry.path();
    }
    EXPECT_GT(count, 0);
}
