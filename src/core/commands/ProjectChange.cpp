#include "commands/ProjectChange.h"

#include "commands/SceneCommands.h"
#include "project/Project.h"

#include <functional>
#include <set>

namespace resonant {

namespace {
Id parentIdOf(const Scene& scene, const SceneObject& object)
{
    const SceneObject* parent = object.parent();
    return parent && !scene.isRoot(*parent) ? parent->id() : Id();
}

int indexInParent(const SceneObject& object)
{
    const SceneObject* parent = object.parent();
    if (!parent)
        return -1;
    const auto& siblings = parent->children();
    for (size_t i = 0; i < siblings.size(); ++i)
        if (siblings[i].get() == &object)
            return static_cast<int>(i);
    return -1;
}
}  // namespace

Result<ProjectChange> projectChangeCommand(Project& live, const Project& changed, const std::string& description)
{
    using R = Result<ProjectChange>;
    ProjectChange out;
    out.command = std::make_unique<CompositeCommand>(description);

    // 1. Objects the copy no longer has (the top-most of each removed subtree).
    std::vector<Id> removals;
    live.scene.forEach([&](const SceneObject& o) {
        if (changed.scene.find(o.id()))
            return;
        const SceneObject* p = o.parent();
        if (p && !live.scene.isRoot(*p) && !changed.scene.find(p->id()))
            return;  // removed with its parent
        removals.push_back(o.id());
    });
    for (const Id& id : removals) {
        out.command->add(std::make_unique<RemoveObjectCommand>(live.scene, id));
        ++out.removed;
    }

    // 2. New objects (the top-most of each new subtree, with the subtree).
    std::string failure;
    changed.scene.forEach([&](const SceneObject& o) {
        if (!failure.empty() || live.scene.find(o.id()))
            return;
        const SceneObject* p = o.parent();
        if (p && !changed.scene.isRoot(*p) && !live.scene.find(p->id()))
            return;  // inserted with its parent
        std::function<bool(const SceneObject&)> hasExisting = [&](const SceneObject& node) {
            for (const auto& child : node.children())
                if (live.scene.find(child->id()) || hasExisting(*child))
                    return true;
            return false;
        };
        if (hasExisting(o)) {
            failure = "an existing object was moved under the new object '" + o.state.name + "'; apply that as two steps";
            return;
        }
        out.command->add(std::make_unique<InsertObjectCommand>(live.scene, o.cloneSubtree(false), parentIdOf(changed.scene, o), -1, "Add"));
        ++out.added;
    });
    if (!failure.empty())
        return R::failure(failure);

    // 3. Objects under another parent, and 4. changed object state.
    changed.scene.forEach([&](const SceneObject& o) {
        const SceneObject* existing = live.scene.find(o.id());
        if (!existing)
            return;
        const Id newParent = parentIdOf(changed.scene, o);
        if (parentIdOf(live.scene, *existing) != newParent) {
            out.command->add(std::make_unique<MoveObjectCommand>(live.scene, o.id(), newParent, indexInParent(o)));
            ++out.moved;
        }
        if (!(existing->state == o.state)) {
            out.command->add(std::make_unique<SetObjectStateCommand>(live.scene, o.id(), o.state, description));
            ++out.changed;
        }
    });

    // 5. The material library, and 6. project-level settings (extensions).
    if (!(live.materials == changed.materials)) {
        out.command->add(std::make_unique<SetMaterialLibraryCommand>(live.materials, changed.materials, description));
        out.materials = true;
    }
    for (const auto& [key, value] : changed.extensions.items()) {
        if (!live.extensions.contains(key) || live.extensions[key] != value) {
            out.command->add(std::make_unique<SetProjectExtensionCommand>(live, key, value.dump(), description));
            ++out.extensions;
        }
    }
    for (const auto& [key, value] : live.extensions.items())
        if (!changed.extensions.contains(key))
            out.notApplied.push_back("the removal of the project setting '" + key + "'");
    if (live.name != changed.name)
        out.notApplied.push_back("the project name (rename the project file instead)");
    return out;
}

}  // namespace resonant
