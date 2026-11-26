#include "scene/Scene.h"

#include "foundation/Units.h"

#include <gtest/gtest.h>

using namespace resonant;

namespace {
Geometry box()
{
    return Geometry::create("box").take();
}

struct EventLog {
    explicit EventLog(Scene& scene)
    {
        scene.addListener([this](const SceneEvent& event) { events.push_back(event); });
    }
    std::vector<SceneEvent> events;
};
}  // namespace

TEST(Scene, CreatedObjectsGetStableUniqueIdsAndAreIndexed)
{
    Scene scene;
    auto first = scene.insert(makeGeometryObject("A", box()));
    auto second = scene.insert(makeGeometryObject("B", box()));
    ASSERT_TRUE(first.ok());
    ASSERT_TRUE(second.ok());
    const Id idA = first.value()->id();
    const Id idB = second.value()->id();
    EXPECT_FALSE(idA.isNull());
    EXPECT_NE(idA, idB);
    EXPECT_EQ(scene.objectCount(), 2u);
    EXPECT_EQ(scene.find(idA)->state.name, "A");
    EXPECT_EQ(scene.find(idA)->type(), object_types::Geometry);
    EXPECT_TRUE(scene.find(idA)->state.geometry.has_value());

    // Editing the object does not change its id.
    ObjectState state = scene.find(idA)->state;
    state.name = "Renamed";
    state.local.position = Vec3(1, 2, 3);
    ASSERT_TRUE(scene.setState(idA, state).ok());
    EXPECT_EQ(scene.find(idA)->id(), idA);
    EXPECT_EQ(scene.find(idA)->state.name, "Renamed");
}

TEST(Scene, RejectsDuplicateIdsAndUnknownParents)
{
    Scene scene;
    auto object = makeGroup("G");
    const Id id = object->id();
    ASSERT_TRUE(scene.insert(std::move(object)).ok());

    auto duplicate = std::make_unique<SceneObject>(id, object_types::Group);
    EXPECT_FALSE(scene.insert(std::move(duplicate)).ok());
    EXPECT_FALSE(scene.insert(makeGroup("X"), Id("no-such-parent")).ok());
    EXPECT_FALSE(scene.insert(std::make_unique<SceneObject>(Id(), object_types::Group)).ok());
    EXPECT_EQ(scene.objectCount(), 1u);
}

TEST(Scene, ParentChildRelationships)
{
    Scene scene;
    const Id group = scene.insert(makeGroup("Group")).value()->id();
    const Id child = scene.insert(makeGeometryObject("Child", box()), group).value()->id();
    const Id grandchild = scene.insert(makeGeometryObject("Grandchild", box()), child).value()->id();

    EXPECT_EQ(scene.find(child)->parent(), scene.find(group));
    EXPECT_EQ(scene.find(group)->children().size(), 1u);
    EXPECT_TRUE(scene.find(group)->isAncestorOf(*scene.find(grandchild)));
    EXPECT_FALSE(scene.find(grandchild)->isAncestorOf(*scene.find(group)));
    EXPECT_TRUE(scene.isRoot(*scene.find(group)->parent()));

    // Detaching a parent takes its subtree with it.
    auto detached = scene.detach(group);
    ASSERT_NE(detached, nullptr);
    EXPECT_EQ(scene.objectCount(), 0u);
    EXPECT_EQ(scene.find(grandchild), nullptr);
    EXPECT_EQ(detached->children().size(), 1u);

    // ... and inserting it brings the subtree back with the same ids.
    ASSERT_TRUE(scene.insert(std::move(detached)).ok());
    EXPECT_EQ(scene.objectCount(), 3u);
    EXPECT_NE(scene.find(grandchild), nullptr);
}

TEST(Scene, MoveReparentsButNeverIntoOwnSubtree)
{
    Scene scene;
    const Id a = scene.insert(makeGroup("A")).value()->id();
    const Id b = scene.insert(makeGroup("B")).value()->id();
    const Id c = scene.insert(makeGroup("C"), a).value()->id();

    ASSERT_TRUE(scene.move(c, b).ok());
    EXPECT_EQ(scene.find(c)->parent(), scene.find(b));
    EXPECT_TRUE(scene.find(a)->children().empty());

    EXPECT_FALSE(scene.move(b, c).ok());  // b is c's parent
    EXPECT_FALSE(scene.move(b, b).ok());
    EXPECT_FALSE(scene.move(Id("missing"), a).ok());

    // Reorder at the top level.
    ASSERT_TRUE(scene.move(b, Id(), 0).ok());
    EXPECT_EQ(scene.root().children()[0]->id(), b);
    EXPECT_EQ(scene.find(b)->indexInParent(), 0);
}

TEST(Scene, WorldTransformComposesParentTransforms)
{
    Scene scene;
    auto parent = makeGroup("Parent");
    parent->state.local.position = Vec3(10.0, 0.0, 0.0);
    parent->state.local.rotation = Vec3(0.0, 0.0, units::pi / 2.0);  // yaw 90 degrees
    const Id parentId = scene.insert(std::move(parent)).value()->id();

    auto child = makeGeometryObject("Child", box());
    child->state.local.position = Vec3(1.0, 0.0, 0.0);
    child->state.local.scale = Vec3(2.0, 2.0, 2.0);
    const Id childId = scene.insert(std::move(child), parentId).value()->id();

    const Mat4 world = scene.worldMatrix(*scene.find(childId));
    // Child origin: (1,0,0) yawed to (0,1,0), then translated by (10,0,0).
    EXPECT_TRUE(transformPoint(world, Vec3::Zero()).isApprox(Vec3(10.0, 1.0, 0.0), 1e-12));
    // Child local +X: scaled by 2, yawed to +Y.
    EXPECT_TRUE(transformDirection(world, Vec3::UnitX()).isApprox(Vec3(0.0, 2.0, 0.0), 1e-12));
    // A top-level object's world matrix is its local matrix.
    EXPECT_TRUE(scene.worldMatrix(*scene.find(parentId)).isApprox(scene.find(parentId)->state.local.matrix()));
}

TEST(Scene, VisibilityIsInheritedFromAncestors)
{
    Scene scene;
    const Id group = scene.insert(makeGroup("Group")).value()->id();
    const Id child = scene.insert(makeGeometryObject("Child", box()), group).value()->id();
    EXPECT_TRUE(scene.isEffectivelyVisible(*scene.find(child)));

    ObjectState state = scene.find(group)->state;
    state.visible = false;
    ASSERT_TRUE(scene.setState(group, state).ok());
    EXPECT_TRUE(scene.find(child)->state.visible);              // its own flag is untouched
    EXPECT_FALSE(scene.isEffectivelyVisible(*scene.find(child)));
}

TEST(Scene, ListenersAreToldWhatChanged)
{
    Scene scene;
    EventLog log(scene);
    const Id group = scene.insert(makeGroup("Group")).value()->id();
    const Id child = scene.insert(makeGroup("Child"), group).value()->id();
    ASSERT_TRUE(scene.setState(child, scene.find(child)->state).ok());
    ASSERT_TRUE(scene.move(child, Id()).ok());
    scene.detach(child);
    scene.clear();

    ASSERT_EQ(log.events.size(), 6u);
    EXPECT_EQ(log.events[0].kind, SceneEvent::Kind::Added);
    EXPECT_TRUE(log.events[0].parentId.isNull());
    EXPECT_EQ(log.events[1].kind, SceneEvent::Kind::Added);
    EXPECT_EQ(log.events[1].parentId, group);
    EXPECT_EQ(log.events[2].kind, SceneEvent::Kind::Changed);
    EXPECT_EQ(log.events[3].kind, SceneEvent::Kind::Moved);
    EXPECT_EQ(log.events[4].kind, SceneEvent::Kind::Removed);
    EXPECT_EQ(log.events[4].id, child);
    EXPECT_EQ(log.events[5].kind, SceneEvent::Kind::Reset);
    EXPECT_EQ(scene.objectCount(), 0u);
}

TEST(Scene, CloneSubtreeCopiesStateAndCanAssignNewIds)
{
    Scene scene;
    const Id group = scene.insert(makeGroup("Group")).value()->id();
    const Id child = scene.insert(makeGeometryObject("Child", box()), group).value()->id();

    auto same = scene.find(group)->cloneSubtree(false);
    EXPECT_EQ(same->id(), group);
    EXPECT_EQ(same->children()[0]->id(), child);

    auto fresh = scene.find(group)->cloneSubtree(true);
    EXPECT_NE(fresh->id(), group);
    EXPECT_NE(fresh->children()[0]->id(), child);
    EXPECT_EQ(fresh->children()[0]->state, scene.find(child)->state);
    ASSERT_TRUE(scene.insert(std::move(fresh)).ok());
    EXPECT_EQ(scene.objectCount(), 4u);
}

TEST(Scene, UniqueNameAddsANumber)
{
    Scene scene;
    EXPECT_EQ(scene.uniqueName("Box"), "Box");
    ASSERT_TRUE(scene.insert(makeGeometryObject("Box", box())).ok());
    EXPECT_EQ(scene.uniqueName("Box"), "Box 2");
    ASSERT_TRUE(scene.insert(makeGeometryObject("Box 2", box())).ok());
    EXPECT_EQ(scene.uniqueName("Box"), "Box 3");
}
