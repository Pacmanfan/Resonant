#include "SceneRenderBridge.h"

#include <unordered_set>

using namespace resonant;

std::shared_ptr<const TriangleMesh> SceneRenderBridge::meshFor(const SceneObject& object)
{
    if (!object.state.geometry)
        return nullptr;
    const auto it = cache_.find(object.id());
    if (it != cache_.end() && it->second.geometry == *object.state.geometry)
        return it->second.mesh;
    auto mesh = std::make_shared<const TriangleMesh>(object.state.geometry->tessellate());
    cache_.insert_or_assign(object.id(), CachedMesh{*object.state.geometry, mesh});
    return mesh;
}

SceneRenderBridge::Frame SceneRenderBridge::build(const Scene& scene, const QList<Id>& selection)
{
    Frame frame;
    pickables_.clear();
    std::unordered_set<Id> alive;

    scene.forEach([&](const SceneObject& object) {
        alive.insert(object.id());
        if (!scene.isEffectivelyVisible(object))
            return;
        const auto mesh = meshFor(object);
        if (!mesh || mesh->empty())
            return;

        // An object is drawn selected when it or one of its ancestors is.
        bool selected = false;
        for (const SceneObject* node = &object; node && !scene.isRoot(*node); node = node->parent())
            if (selection.contains(node->id()))
                selected = true;

        render::DrawItem item;
        item.mesh = mesh;
        item.model = scene.worldMatrix(object);
        item.selected = selected;
        frame.items.push_back(item);
        pickables_.push_back({object.id(), mesh, item.model});

        const Aabb local = mesh->bounds();
        for (int corner = 0; corner < 8; ++corner) {
            const Vec3 point((corner & 1) ? local.max.x() : local.min.x(), (corner & 2) ? local.max.y() : local.min.y(),
                             (corner & 4) ? local.max.z() : local.min.z());
            const Vec3 world = transformPoint(item.model, point);
            frame.bounds.extend(world);
            if (selected)
                frame.selectionBounds.extend(world);
        }
    });

    std::erase_if(cache_, [&alive](const auto& entry) { return !alive.contains(entry.first); });
    return frame;
}

Id SceneRenderBridge::pick(const Ray& ray) const
{
    Id nearest;
    double nearestDistance = std::numeric_limits<double>::infinity();
    for (const Pickable& pickable : pickables_) {
        const auto hit = intersect(ray, *pickable.mesh, pickable.model);
        if (hit && *hit < nearestDistance) {
            nearestDistance = *hit;
            nearest = pickable.id;
        }
    }
    return nearest;
}

void SceneRenderBridge::clear()
{
    cache_.clear();
    pickables_.clear();
}
