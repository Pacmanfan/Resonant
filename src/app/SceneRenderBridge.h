// Turns the scene into what the renderer draws. This is the only place
// where scene objects meet rendering: the renderer never sees a
// SceneObject, and the scene never sees a mesh on the GPU.
//
// Display meshes are derived from each object's engineering geometry and
// cached until that geometry changes.
#pragma once

#include "render/Layers.h"
#include "scene/Scene.h"

#include <QList>

#include <functional>
#include <unordered_map>

class SceneRenderBridge {
public:
    struct Frame {
        std::vector<resonant::render::DrawItem> items;
        std::vector<resonant::render::LineItem> lines;
        resonant::Aabb bounds;           // of everything visible
        resonant::Aabb selectionBounds;  // of the selected, visible objects
    };

    // What can be picked in the viewport: one entry per drawn object.
    struct Pickable {
        resonant::Id id;
        std::shared_ptr<const resonant::TriangleMesh> mesh;
        resonant::Mat4 model;
    };

    // Adds what an object needs beyond its own geometry: markers for
    // objects without a shape (emitters, targets, probes), outlines, axes.
    // A decorator appends draw items and lines to the frame and, for what
    // should be clickable, pickables. Domain user interfaces install them.
    using Decorator = std::function<void(const resonant::Scene& scene, const resonant::SceneObject& object,
                                         const resonant::Mat4& world, bool selected, Frame& frame,
                                         std::vector<Pickable>& pickables)>;
    void addDecorator(Decorator decorator) { decorators_.push_back(std::move(decorator)); }
    // Objects for which this returns true are not drawn with their own mesh
    // (their decorators still run): a domain shows something in their place,
    // such as a deformed structure.
    void setReplaced(std::function<bool(const resonant::Id&)> replaced) { replaced_ = std::move(replaced); }

    Frame build(const resonant::Scene& scene, const QList<resonant::Id>& selection);
    const std::vector<Pickable>& pickables() const { return pickables_; }

    // The id of the nearest object hit by the ray, or a null id.
    resonant::Id pick(const resonant::Ray& ray) const;

    void clear();

private:
    struct CachedMesh {
        resonant::Geometry geometry;
        std::shared_ptr<const resonant::TriangleMesh> mesh;
    };

    std::shared_ptr<const resonant::TriangleMesh> meshFor(const resonant::SceneObject& object);

    std::unordered_map<resonant::Id, CachedMesh> cache_;
    std::vector<Pickable> pickables_;
    std::vector<Decorator> decorators_;
    std::function<bool(const resonant::Id&)> replaced_;
};
