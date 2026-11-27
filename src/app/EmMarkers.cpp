#include "EmMarkers.h"

#include "electromagnetics/EmComponents.h"
#include "foundation/Transform.h"

using namespace resonant;
using namespace resonant::em;

namespace {

std::shared_ptr<const TriangleMesh> unitCube()
{
    static const std::shared_ptr<const TriangleMesh> mesh = [] {
        Geometry box = Geometry::create("box").take();
        for (const char* key : {"width", "depth", "height"})
            [[maybe_unused]] const Status status = box.setParameter(key, 2.0);
        return std::make_shared<const TriangleMesh>(box.tessellate());
    }();
    return mesh;
}

const Eigen::Vector4f kSource(0.20f, 0.65f, 0.95f, 1.0f);
const Eigen::Vector4f kElectric(0.95f, 0.85f, 0.20f, 1.0f);
const Eigen::Vector4f kInactive(0.45f, 0.45f, 0.45f, 1.0f);

void cube(const SceneObject& object, const Mat4& world, const Vec3& local, double size, const Eigen::Vector4f& colour, bool selected,
          SceneRenderBridge::Frame& frame, std::vector<SceneRenderBridge::Pickable>& pickables)
{
    render::DrawItem item;
    item.mesh = unitCube();
    item.model = Mat4::Identity();
    item.model.block<3, 3>(0, 0) = world.block<3, 3>(0, 0).colwise().normalized() * size;
    item.model.block<3, 1>(0, 3) = transformPoint(world, local);
    item.color = colour;
    item.selected = selected;
    frame.items.push_back(item);
    pickables.push_back({object.id(), item.mesh, item.model});
}

}  // namespace

SceneRenderBridge::Decorator makeEmDecorator()
{
    return [](const Scene&, const SceneObject& object, const Mat4& world, bool selected, SceneRenderBridge::Frame& frame,
              std::vector<SceneRenderBridge::Pickable>& pickables) {
        const Component* c = object.state.component.get();
        const Vec3 centre = transformPoint(world, Vec3::Zero());
        const Vec3 z = transformDirection(world, Vec3::UnitZ()).normalized(), x = transformDirection(world, Vec3::UnitX()).normalized();
        if (const auto* s = dynamic_cast<const SourceComponent*>(c)) {
            const Eigen::Vector4f colour = s->settings.active ? kSource : kInactive;
            cube(object, world, Vec3::Zero(), 0.004, colour, selected, frame, pickables);
            const double length = s->settings.kind == SourceKind::PlaneWave ? 0.05 : 0.02;
            const Vec3 tip = centre + length * z;
            frame.lines.push_back({centre - (s->settings.kind == SourceKind::PlaneWave ? 0.0 : length) * z, tip, colour});
            if (s->settings.kind == SourceKind::PlaneWave) {
                const Vec3 side = x;
                frame.lines.push_back({tip, tip - 0.01 * z + 0.005 * side, colour});
                frame.lines.push_back({tip, tip - 0.01 * z - 0.005 * side, colour});
                frame.lines.push_back({centre, centre + 0.025 * x, kElectric});  // E along local X
            }
            return;
        }
        const auto* a = dynamic_cast<const EmArrayComponent*>(c);
        if (!a)
            return;
        const ArrayModel model = arrayModelOf(a->settings);
        double largest = 0.0;
        for (const ArrayElement& e : model.elements)
            largest = std::max(largest, std::abs(e.weight));
        const double size = std::clamp(0.08 * (a->settings.layout == EmArrayLayout::Circular ? a->settings.radius : a->settings.spacingX), 0.002, 0.02);
        for (const ArrayElement& e : model.elements) {
            const float level = largest > 0.0 ? static_cast<float>(0.25 + 0.75 * std::abs(e.weight) / largest) : 1.0f;
            const Eigen::Vector4f colour = e.active && a->settings.active ? Eigen::Vector4f(kSource.x() * level, kSource.y() * level, kSource.z() * level, 1.0f)
                                                                            : kInactive;
            cube(object, world, e.position, size, colour, selected, frame, pickables);
        }
        frame.lines.push_back({centre, centre + 6.0 * size * z, kSource});  // the array's local +Z (broadside of a planar array)
    };
}
