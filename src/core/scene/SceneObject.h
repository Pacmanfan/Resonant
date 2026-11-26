// A node of the scene graph.
//
// Identity (id, type) is fixed for the life of the object. Everything the
// user can edit is in ObjectState, so one snapshot captures an object for
// undo, duplication and persistence. The tree links are managed by Scene.
#pragma once

#include "foundation/Id.h"
#include "foundation/Transform.h"
#include "geometry/Geometry.h"
#include "scene/Component.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace resonant {

namespace object_types {
inline constexpr const char* Group = "group";
inline constexpr const char* Geometry = "geometry";
}  // namespace object_types

struct ObjectState {
    std::string name;
    Transform local;                    // relative to the parent
    bool visible = true;
    std::optional<Geometry> geometry;   // engineering geometry, if the object has a shape
    Id materialId;                      // null when no material is assigned
    ComponentPtr component;             // domain data, if any

    friend bool operator==(const ObjectState& a, const ObjectState& b);
};

class SceneObject {
public:
    SceneObject(Id id, std::string type) : id_(std::move(id)), type_(std::move(type)) {}

    const Id& id() const { return id_; }
    const std::string& type() const { return type_; }

    ObjectState state;

    SceneObject* parent() const { return parent_; }
    const std::vector<std::unique_ptr<SceneObject>>& children() const { return children_; }
    // Position among the parent's children, or -1 for a detached object.
    int indexInParent() const;
    bool isAncestorOf(const SceneObject& other) const;

    // A deep copy of this object and its descendants. With newIds every copy
    // gets a fresh id; otherwise ids are preserved (for undo snapshots).
    std::unique_ptr<SceneObject> cloneSubtree(bool newIds) const;

    // Adds a child to a detached object under construction (for building a
    // subtree before it is inserted into a scene).
    SceneObject* addChild(std::unique_ptr<SceneObject> child);

private:
    friend class Scene;

    Id id_;
    std::string type_;
    SceneObject* parent_ = nullptr;
    std::vector<std::unique_ptr<SceneObject>> children_;
};

}  // namespace resonant
