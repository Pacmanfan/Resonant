// A script's changed project copy becomes ONE undoable change (ProjectChange,
// REQ-CHAT-20261126-010, RES-PY-006): additions, removals, moves, edits,
// materials and settings, and undo restores the project exactly.
#include "commands/ProjectChange.h"
#include "coupling/WorkflowActions.h"
#include "project/ProjectSerializer.h"

#include <gtest/gtest.h>

using namespace resonant;

namespace {
std::unique_ptr<Project> copyOf(const Project& p)
{
    auto loaded = deserializeProject(serializeProject(p));
    EXPECT_TRUE(loaded.ok()) << loaded.message();
    return std::move(loaded.value().project);
}
}  // namespace

TEST(ProjectChange, AChangedCopyBecomesOneUndoableStep)
{
    auto live = Project::createNew("live");
    ASSERT_TRUE(coupling::addSceneObject(*live, {{"type", "geometry"}, {"id", "box"}, {"name", "Box"}}).ok());
    ASSERT_TRUE(coupling::addSceneObject(*live, {{"type", "geometry"}, {"id", "gone"}, {"name", "Gone"}}).ok());
    ASSERT_TRUE(coupling::addSceneObject(*live, {{"type", "geometry"}, {"id", "group"}, {"name", "Holder"}}).ok());
    const nlohmann::json before = serializeProject(*live);

    auto copy = copyOf(*live);
    ASSERT_TRUE(coupling::changeSceneObject(*copy, "box", {{"position", {0.1, 0.2, 0.3}}}).ok());
    copy->scene.detach(Id("gone"));
    ASSERT_TRUE(coupling::addSceneObject(*copy, {{"type", "emitter"}, {"id", "new"}, {"name", "New"}, {"parent", "group"}}).ok());
    ASSERT_TRUE(copy->scene.move(Id("box"), Id("group")).ok());
    copy->extensions["acoustics"] = {{"frequencyHz", 25000.0}};

    auto change = projectChangeCommand(*live, *copy, "Script changes");
    ASSERT_TRUE(change.ok()) << change.message();
    EXPECT_EQ(change.value().removed, 1);
    EXPECT_EQ(change.value().added, 1);
    EXPECT_EQ(change.value().moved, 1);
    EXPECT_EQ(change.value().changed, 1);
    EXPECT_EQ(change.value().extensions, 1);

    CommandStack stack;
    ASSERT_TRUE(stack.push(std::move(change.value().command)).ok());
    EXPECT_EQ(stack.size(), 1u);
    nlohmann::json after = serializeProject(*live), expected = serializeProject(*copy);
    for (auto* j : {&after, &expected})
        j->erase("modifiedUtc");
    EXPECT_EQ(after["scene"], expected["scene"]);
    EXPECT_EQ(after["extensions"], expected["extensions"]);
    ASSERT_TRUE(stack.undo().ok());
    EXPECT_EQ(serializeProject(*live)["scene"], before["scene"]);
    EXPECT_EQ(serializeProject(*live)["extensions"], before["extensions"]);

    // Nothing changed: an empty change.
    auto none = projectChangeCommand(*live, *copyOf(*live), "nothing");
    ASSERT_TRUE(none.ok());
    EXPECT_TRUE(none.value().empty());
}
