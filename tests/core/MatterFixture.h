// The architected-matter examples, loaded for the tests (examples/matter, RES-FAM-019):
// the project, its architectures, and a copy of one to vary.
#pragma once

#include "matter/MatterArchitecture.h"
#include "project/Project.h"
#include "project/ProjectSerializer.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>

namespace matter_test {

using namespace resonant;
using nlohmann::json;

inline std::unique_ptr<Project> example(const std::string& file)
{
    auto loaded = loadProjectFile(std::filesystem::path(RESONANT_SOURCE_DIR) / "examples" / "matter" / file);
    EXPECT_TRUE(loaded) << loaded.message();
    return loaded ? std::move(loaded.value().project) : nullptr;
}

inline matter::Architecture architecture(const Project& project, const std::string& id)
{
    auto w = matter::workspaceOf(project);
    EXPECT_TRUE(w) << w.message();
    const matter::Architecture* a = w ? w.value().find(id) : nullptr;
    EXPECT_NE(a, nullptr) << "no architecture " << id;
    return a ? *a : matter::Architecture{};
}

// Replaces (or adds) an architecture in the project.
inline void put(Project& project, const matter::Architecture& a)
{
    auto w = matter::workspaceOf(project);
    ASSERT_TRUE(w) << w.message();
    matter::Workspace ws = w.value();
    if (matter::Architecture* existing = ws.find(a.id))
        *existing = a;
    else
        ws.architectures.push_back(a);
    matter::storeWorkspace(project, ws);
}

}  // namespace matter_test
