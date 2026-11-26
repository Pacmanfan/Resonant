// The scene graph: a tree of SceneObjects under an invisible root. The
// scene owns its objects, keeps an id index, computes world transforms and
// tells listeners what changed. It contains no rendering and no user
// interface state (selection lives in the application).
#pragma once

#include "foundation/Result.h"
#include "scene/SceneObject.h"

#include <functional>
#include <unordered_map>

namespace resonant {

struct SceneEvent {
    enum class Kind {
        Added,     // id was inserted (with its subtree) under parentId
        Removed,   // id was detached (with its subtree) from parentId
        Changed,   // the ObjectState of id changed
        Moved,     // id moved to parentId / a new position
        Reset,     // the whole scene was replaced
    };
    Kind kind = Kind::Reset;
    Id id;
    Id parentId;
};

class Scene {
public:
    using Listener = std::function<void(const SceneEvent&)>;

    Scene();
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    const SceneObject& root() const { return *root_; }
    bool isRoot(const SceneObject& object) const { return &object == root_.get(); }

    SceneObject* find(const Id& id);
    const SceneObject* find(const Id& id) const;
    size_t objectCount() const { return index_.size(); }  // the root is not counted

    // Inserts a detached object (with its subtree) under a parent (the root
    // when parentId is null) at a position (-1 appends). Fails, leaving the
    // scene unchanged, for an unknown parent or an id already in the scene.
    Result<SceneObject*> insert(std::unique_ptr<SceneObject> object, const Id& parentId = Id(), int index = -1);

    // Detaches an object and its subtree and returns it, or nullptr when the
    // id is unknown.
    std::unique_ptr<SceneObject> detach(const Id& id);

    // Moves an object under another parent / to another position. Fails when
    // the new parent is the object itself or one of its descendants.
    Status move(const Id& id, const Id& newParentId, int index = -1);

    // Replaces an object's editable state. Fails for an unknown id.
    Status setState(const Id& id, ObjectState state);

    void clear();

    // Local-to-world matrix: the product of the local matrices from the root
    // down to the object.
    Mat4 worldMatrix(const SceneObject& object) const;
    // False when the object or any ancestor is hidden.
    bool isEffectivelyVisible(const SceneObject& object) const;

    // Pre-order traversal of every object except the root.
    void forEach(const std::function<void(const SceneObject&)>& visit) const;

    // "Box", then "Box 2", "Box 3", ... - a name no object has yet.
    std::string uniqueName(const std::string& base) const;

    int addListener(Listener listener);
    void removeListener(int handle);

private:
    void indexSubtree(SceneObject& object);
    void unindexSubtree(const SceneObject& object);
    bool subtreeCollides(const SceneObject& object) const;
    void notify(const SceneEvent& event);
    SceneObject* resolveParent(const Id& parentId);
    Id publicParentId(const SceneObject& object) const;

    std::unique_ptr<SceneObject> root_;
    std::unordered_map<Id, SceneObject*> index_;
    std::vector<std::pair<int, Listener>> listeners_;
    int nextListener_ = 1;
};

// Convenience constructors for detached objects.
std::unique_ptr<SceneObject> makeGroup(std::string name);
std::unique_ptr<SceneObject> makeGeometryObject(std::string name, Geometry geometry);

}  // namespace resonant
