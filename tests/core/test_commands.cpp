#include "commands/SceneCommands.h"

#include <gtest/gtest.h>

using namespace resonant;

namespace {
std::unique_ptr<SceneObject> boxObject(const char* name)
{
    return makeGeometryObject(name, Geometry::create("box").take());
}
}  // namespace

TEST(CommandStack, InsertUndoRedoKeepsTheSameObjectId)
{
    Scene scene;
    CommandStack stack;
    auto command = std::make_unique<InsertObjectCommand>(scene, boxObject("Box"));
    const Id id = command->objectId();

    ASSERT_TRUE(stack.push(std::move(command)).ok());
    EXPECT_NE(scene.find(id), nullptr);
    EXPECT_TRUE(stack.canUndo());
    EXPECT_FALSE(stack.canRedo());
    EXPECT_EQ(stack.undoText(), "Add Box");

    ASSERT_TRUE(stack.undo().ok());
    EXPECT_EQ(scene.find(id), nullptr);
    EXPECT_TRUE(stack.canRedo());

    ASSERT_TRUE(stack.redo().ok());
    ASSERT_NE(scene.find(id), nullptr);
    EXPECT_EQ(scene.find(id)->state.name, "Box");
}

TEST(CommandStack, RemoveUndoRestoresSubtreeAtItsPosition)
{
    Scene scene;
    CommandStack stack;
    const Id first = scene.insert(makeGroup("First")).value()->id();
    const Id middle = scene.insert(makeGroup("Middle")).value()->id();
    const Id last = scene.insert(makeGroup("Last")).value()->id();
    const Id child = scene.insert(boxObject("Child"), middle).value()->id();

    ASSERT_TRUE(stack.push(std::make_unique<RemoveObjectCommand>(scene, middle)).ok());
    EXPECT_EQ(scene.find(middle), nullptr);
    EXPECT_EQ(scene.find(child), nullptr);
    EXPECT_EQ(scene.objectCount(), 2u);

    ASSERT_TRUE(stack.undo().ok());
    ASSERT_NE(scene.find(middle), nullptr);
    EXPECT_EQ(scene.find(middle)->indexInParent(), 1);
    EXPECT_EQ(scene.find(child)->parent(), scene.find(middle));
    EXPECT_EQ(scene.root().children()[0]->id(), first);
    EXPECT_EQ(scene.root().children()[2]->id(), last);
}

TEST(CommandStack, StateChangesUndoAndRedo)
{
    Scene scene;
    CommandStack stack;
    const Id id = scene.insert(boxObject("Box")).value()->id();
    const ObjectState original = scene.find(id)->state;

    ObjectState moved = original;
    moved.local.position = Vec3(1, 2, 3);
    ASSERT_TRUE(stack.push(std::make_unique<SetObjectStateCommand>(scene, id, moved, "Move Box")).ok());

    ObjectState renamed = moved;
    renamed.name = "Crate";
    renamed.visible = false;
    ASSERT_TRUE(renamed.geometry->setParameter("width", 4.0).ok());
    ASSERT_TRUE(stack.push(std::make_unique<SetObjectStateCommand>(scene, id, renamed, "Edit Box")).ok());
    EXPECT_EQ(scene.find(id)->state, renamed);

    ASSERT_TRUE(stack.undo().ok());
    EXPECT_EQ(scene.find(id)->state, moved);
    ASSERT_TRUE(stack.undo().ok());
    EXPECT_EQ(scene.find(id)->state, original);
    EXPECT_FALSE(stack.canUndo());
    EXPECT_FALSE(stack.undo().ok());

    ASSERT_TRUE(stack.redo().ok());
    ASSERT_TRUE(stack.redo().ok());
    EXPECT_EQ(scene.find(id)->state, renamed);
    EXPECT_FALSE(stack.redo().ok());
}

TEST(CommandStack, AFailedCommandIsNotRecorded)
{
    Scene scene;
    CommandStack stack;
    EXPECT_FALSE(stack.push(std::make_unique<RemoveObjectCommand>(scene, Id("missing"))).ok());
    EXPECT_FALSE(stack.push(std::make_unique<SetObjectStateCommand>(scene, Id("missing"), ObjectState{}, "x")).ok());
    EXPECT_FALSE(stack.push(std::make_unique<InsertObjectCommand>(scene, boxObject("B"), Id("missing"))).ok());
    EXPECT_EQ(stack.size(), 0u);
    EXPECT_FALSE(stack.canUndo());
}

TEST(CommandStack, NewCommandDiscardsRedoHistory)
{
    Scene scene;
    CommandStack stack;
    ASSERT_TRUE(stack.push(std::make_unique<InsertObjectCommand>(scene, boxObject("A"))).ok());
    ASSERT_TRUE(stack.push(std::make_unique<InsertObjectCommand>(scene, boxObject("B"))).ok());
    ASSERT_TRUE(stack.undo().ok());
    ASSERT_TRUE(stack.canRedo());
    ASSERT_TRUE(stack.push(std::make_unique<InsertObjectCommand>(scene, boxObject("C"))).ok());
    EXPECT_FALSE(stack.canRedo());
    EXPECT_EQ(stack.size(), 2u);
}

TEST(CommandStack, CleanStateTracksTheSavePoint)
{
    Scene scene;
    CommandStack stack;
    EXPECT_TRUE(stack.isClean());
    ASSERT_TRUE(stack.push(std::make_unique<InsertObjectCommand>(scene, boxObject("A"))).ok());
    EXPECT_FALSE(stack.isClean());
    stack.markClean();  // "saved"
    EXPECT_TRUE(stack.isClean());

    ASSERT_TRUE(stack.undo().ok());
    EXPECT_FALSE(stack.isClean());
    ASSERT_TRUE(stack.redo().ok());
    EXPECT_TRUE(stack.isClean());

    // Saved state becomes unreachable once its redo branch is discarded.
    ASSERT_TRUE(stack.undo().ok());
    ASSERT_TRUE(stack.push(std::make_unique<InsertObjectCommand>(scene, boxObject("B"))).ok());
    EXPECT_FALSE(stack.isClean());
    ASSERT_TRUE(stack.undo().ok());
    EXPECT_FALSE(stack.isClean());
}

TEST(CommandStack, CompositeAppliesAndRevertsAsOneStep)
{
    Scene scene;
    CommandStack stack;
    const Id a = scene.insert(boxObject("A")).value()->id();
    const Id b = scene.insert(boxObject("B")).value()->id();

    auto composite = std::make_unique<CompositeCommand>("Delete 2 objects");
    composite->add(std::make_unique<RemoveObjectCommand>(scene, a));
    composite->add(std::make_unique<RemoveObjectCommand>(scene, b));
    ASSERT_TRUE(stack.push(std::move(composite)).ok());
    EXPECT_EQ(scene.objectCount(), 0u);
    ASSERT_TRUE(stack.undo().ok());
    EXPECT_EQ(scene.objectCount(), 2u);
    EXPECT_EQ(scene.find(a)->indexInParent(), 0);
    EXPECT_EQ(scene.find(b)->indexInParent(), 1);

    // A composite that fails midway leaves nothing applied.
    auto failing = std::make_unique<CompositeCommand>("Broken");
    failing->add(std::make_unique<RemoveObjectCommand>(scene, a));
    failing->add(std::make_unique<RemoveObjectCommand>(scene, Id("missing")));
    EXPECT_FALSE(stack.push(std::move(failing)).ok());
    EXPECT_EQ(scene.objectCount(), 2u);
}

TEST(CommandStack, MoveInHierarchyUndoes)
{
    Scene scene;
    CommandStack stack;
    const Id group = scene.insert(makeGroup("Group")).value()->id();
    const Id box = scene.insert(boxObject("Box")).value()->id();
    ASSERT_TRUE(stack.push(std::make_unique<MoveObjectCommand>(scene, box, group)).ok());
    EXPECT_EQ(scene.find(box)->parent(), scene.find(group));
    ASSERT_TRUE(stack.undo().ok());
    EXPECT_TRUE(scene.isRoot(*scene.find(box)->parent()));
    EXPECT_EQ(scene.find(box)->indexInParent(), 1);
}

TEST(CommandStack, MaterialLibraryEditsUndo)
{
    MaterialLibrary library = createExampleMaterialLibrary();
    CommandStack stack;
    MaterialLibrary edited = library;
    ASSERT_TRUE(edited.add(Material(Id("custom"), "Custom")).ok());
    int notified = 0;
    ASSERT_TRUE(stack.push(std::make_unique<SetMaterialLibraryCommand>(library, edited, "Add material Custom",
                                                                        [&] { ++notified; }))
                    .ok());
    EXPECT_NE(library.find(Id("custom")), nullptr);
    ASSERT_TRUE(stack.undo().ok());
    EXPECT_EQ(library.find(Id("custom")), nullptr);
    EXPECT_EQ(notified, 2);
}

TEST(CommandStack, GeometryKindCanBeReplacedUndoably)
{
    Scene scene;
    CommandStack stack;
    const Id id = scene.insert(boxObject("Box")).value()->id();
    ObjectState state = scene.find(id)->state;
    state.geometry = Geometry::create("sphere").take();
    ASSERT_TRUE(stack.push(std::make_unique<SetObjectStateCommand>(scene, id, state, "Change shape")).ok());
    EXPECT_EQ(scene.find(id)->state.geometry->kindId(), "sphere");
    ASSERT_TRUE(stack.undo().ok());
    EXPECT_EQ(scene.find(id)->state.geometry->kindId(), "box");
}
