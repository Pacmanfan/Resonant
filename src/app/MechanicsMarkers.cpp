#include "MechanicsMarkers.h"

#include "foundation/Transform.h"
#include "mechanics/MechanicalComponents.h"

using namespace resonant;
using namespace resonant::mechanics;

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

const Eigen::Vector4f kActuator(0.95f, 0.50f, 0.10f, 1.0f);
const Eigen::Vector4f kInactive(0.45f, 0.45f, 0.45f, 1.0f);
const Eigen::Vector4f kSensor(0.30f, 0.80f, 0.35f, 1.0f);
const Eigen::Vector4f kTarget(0.85f, 0.30f, 0.85f, 1.0f);
const Eigen::Vector4f kQuiet(0.85f, 0.20f, 0.20f, 1.0f);

}  // namespace

QString mechanicsTypeLabel(const SceneObject& object)
{
    const Component* c = object.state.component.get();
    if (const auto* a = dynamic_cast<const ActuatorComponent*>(c))
        return a->settings.excitation == Excitation::PointForce ? QStringLiteral("Actuator (point force)") : QStringLiteral("Actuator (distributed force)");
    if (dynamic_cast<const SensorComponent*>(c))
        return QStringLiteral("Sensor");
    if (const auto* t = dynamic_cast<const VibrationTargetComponent*>(c))
        return t->settings.suppress ? QStringLiteral("Vibration target (quiet)") : QStringLiteral("Vibration target");
    return {};
}

QString structureSuffix(const SceneObject& object)
{
    if (const auto* s = dynamic_cast<const StructureComponent*>(object.state.component.get()))
        return QStringLiteral(" (%1)").arg(QString::fromUtf8(toString(s->settings.idealization).data()));
    return {};
}

SceneRenderBridge::Decorator makeMechanicsDecorator()
{
    return [](const Scene&, const SceneObject& object, const Mat4& world, bool selected, SceneRenderBridge::Frame& frame,
              std::vector<SceneRenderBridge::Pickable>& pickables) {
        const Component* c = object.state.component.get();
        Eigen::Vector4f colour;
        bool arrow = false;
        if (const auto* a = dynamic_cast<const ActuatorComponent*>(c)) {
            colour = a->settings.active ? kActuator : kInactive;
            arrow = a->settings.active;
        } else if (dynamic_cast<const SensorComponent*>(c)) {
            colour = kSensor;
        } else if (const auto* t = dynamic_cast<const VibrationTargetComponent*>(c)) {
            colour = t->settings.suppress ? kQuiet : kTarget;
        } else {
            return;
        }
        const Vec3 centre = transformPoint(world, Vec3::Zero());
        const Vec3 forward = transformDirection(world, Vec3::UnitZ()).normalized();
        const double size = 0.004;
        render::DrawItem item;
        item.mesh = unitCube();
        item.model = Mat4::Identity();
        item.model.block<3, 3>(0, 0) = world.block<3, 3>(0, 0).colwise().normalized() * size;
        item.model.block<3, 1>(0, 3) = centre;
        item.color = colour;
        item.selected = selected;
        frame.items.push_back(item);
        pickables.push_back({object.id(), item.mesh, item.model});
        // The direction it acts or measures along.
        const Vec3 tip = centre + (arrow ? 0.03 : 0.015) * forward;
        frame.lines.push_back({centre, tip, colour});
        if (arrow) {
            const Vec3 side = forward.unitOrthogonal();
            frame.lines.push_back({tip, tip - 0.008 * forward + 0.004 * side, colour});
            frame.lines.push_back({tip, tip - 0.008 * forward - 0.004 * side, colour});
        }
    };
}
