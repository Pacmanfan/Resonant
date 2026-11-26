#include "AcousticMarkers.h"

#include "acoustics/Components.h"

using namespace resonant;
using namespace resonant::acoustics;

namespace {

std::shared_ptr<const TriangleMesh> unitSphere()
{
    static const std::shared_ptr<const TriangleMesh> mesh = [] {
        Geometry sphere = Geometry::create("sphere").take();
        [[maybe_unused]] const Status status = sphere.setParameter("radius", 1.0);
        TessellationOptions options;
        options.segments = 16;
        return std::make_shared<const TriangleMesh>(sphere.tessellate(options));
    }();
    return mesh;
}

Mat4 sphereAt(const Vec3& centre, double radius)
{
    Mat4 model = Mat4::Identity();
    model.block<3, 3>(0, 0) *= radius;
    model.block<3, 1>(0, 3) = centre;
    return model;
}

void addSphere(SceneRenderBridge::Frame& frame, std::vector<SceneRenderBridge::Pickable>& pickables, const Id& id,
               const Vec3& centre, double radius, const Eigen::Vector4f& color, bool selected)
{
    render::DrawItem item;
    item.mesh = unitSphere();
    item.model = sphereAt(centre, radius);
    item.color = color;
    item.selected = selected;
    frame.items.push_back(item);
    pickables.push_back({id, item.mesh, item.model});
}

void addLine(SceneRenderBridge::Frame& frame, const Vec3& from, const Vec3& to, const Eigen::Vector4f& color)
{
    frame.lines.push_back({from, to, color});
}

const Eigen::Vector4f kEmitter(0.95f, 0.55f, 0.15f, 1.0f);
const Eigen::Vector4f kInactive(0.45f, 0.45f, 0.45f, 1.0f);
const Eigen::Vector4f kTarget(0.25f, 0.85f, 0.35f, 1.0f);
const Eigen::Vector4f kSuppress(0.85f, 0.25f, 0.25f, 1.0f);
const Eigen::Vector4f kProbe(0.40f, 0.70f, 1.00f, 1.0f);
const Eigen::Vector4f kRegion(0.30f, 0.85f, 0.90f, 1.0f);
const Eigen::Vector4f kSelected(1.00f, 0.75f, 0.20f, 1.0f);

}  // namespace

QString acousticTypeLabel(const SceneObject& object)
{
    if (componentOf<EmitterComponent>(object)) return QStringLiteral("Acoustic emitter");
    if (const auto* array = componentOf<ArrayComponent>(object))
        return QStringLiteral("Acoustic array (%1 elements)").arg(array->settings.elementCount());
    if (const auto* target = componentOf<TargetComponent>(object))
        return target->settings.suppress ? QStringLiteral("Target (suppress)") : QStringLiteral("Target point");
    if (componentOf<ProbeComponent>(object)) return QStringLiteral("Probe");
    if (const auto* region = componentOf<RegionComponent>(object))
        return region->settings.isVolume() ? QStringLiteral("Field volume") : QStringLiteral("Field plane");
    return {};
}

SceneRenderBridge::Decorator makeAcousticDecorator()
{
    return [](const Scene&, const SceneObject& object, const Mat4& world, bool selected,
              SceneRenderBridge::Frame& frame, std::vector<SceneRenderBridge::Pickable>& pickables) {
        const auto drawSource = [&](const EmitterSettings& settings, const Vec3& localPosition, const Vec3& localForward) {
            const Vec3 centre = transformPoint(world, localPosition);
            const Vec3 forward = transformDirection(world, localForward).normalized();
            const double radius = settings.radius;
            addSphere(frame, pickables, object.id(), centre, radius, settings.active ? kEmitter : kInactive, selected);
            if (settings.active)
                addLine(frame, centre, centre + 3.0 * radius * forward, selected ? kSelected : kEmitter);
        };

        if (const auto* emitter = componentOf<EmitterComponent>(object)) {
            drawSource(emitter->settings, Vec3::Zero(), Vec3::UnitZ());
        } else if (const auto* array = componentOf<ArrayComponent>(object)) {
            for (const ArrayElement& element : generateElements(array->settings))
                drawSource(element.settings, element.localPosition, element.localForward);
        } else if (const auto* target = componentOf<TargetComponent>(object)) {
            const Vec3 centre = transformPoint(world, Vec3::Zero());
            const Eigen::Vector4f color = target->settings.suppress ? kSuppress : kTarget;
            addSphere(frame, pickables, object.id(), centre, 0.0015, color, selected);
            for (const Vec3& axis : {Vec3(Vec3::UnitX()), Vec3(Vec3::UnitY()), Vec3(Vec3::UnitZ())})
                addLine(frame, centre - 0.006 * axis, centre + 0.006 * axis, selected ? kSelected : color);
        } else if (componentOf<ProbeComponent>(object)) {
            const Vec3 centre = transformPoint(world, Vec3::Zero());
            addSphere(frame, pickables, object.id(), centre, 0.0012, kProbe, selected);
            addLine(frame, centre, centre + Vec3(0.0, 0.0, 0.008), selected ? kSelected : kProbe);
        } else if (const auto* region = componentOf<RegionComponent>(object)) {
            const RegionSettings& r = region->settings;
            const Vec3 o = transformPoint(world, r.localOrigin());
            const Vec3 u = transformDirection(world, r.localAxisU());
            const Vec3 v = transformDirection(world, r.localAxisV());
            const Vec3 w = transformDirection(world, r.localAxisW());
            const Eigen::Vector4f color = selected ? kSelected : kRegion;
            const auto rectangle = [&](const Vec3& corner) {
                addLine(frame, corner, corner + u, color);
                addLine(frame, corner + u, corner + u + v, color);
                addLine(frame, corner + u + v, corner + v, color);
                addLine(frame, corner + v, corner, color);
            };
            rectangle(o);
            if (r.isVolume()) {
                rectangle(o + w);
                for (const Vec3& corner : {o, Vec3(o + u), Vec3(o + v), Vec3(o + u + v)})
                    addLine(frame, corner, corner + w, color);
            }
            // The outline counts towards the bounds (no mesh is drawn for it).
            for (const Vec3& corner : {o, Vec3(o + u + v + w)}) {
                frame.bounds.extend(corner);
                if (selected)
                    frame.selectionBounds.extend(corner);
            }
        }
    };
}
