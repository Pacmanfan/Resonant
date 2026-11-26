#include "ManipulatorController.h"

#include "AcousticController.h"
#include "DisplayUnits.h"
#include "DocumentController.h"
#include "manipulators/Analytics.h"

using namespace resonant;
using namespace resonant::acoustics;
using namespace resonant::manipulators;

ManipulatorController::ManipulatorController(DocumentController* document, AcousticController* acoustics, QObject* parent)
    : QObject(parent), document_(document), acoustics_(acoustics)
{
}

Result<Id> ManipulatorController::addManipulator(const QString& geometryKind)
{
    const GeometryKind* kind = GeometryRegistry::instance().find(geometryKind.toStdString());
    if (!kind)
        return Result<Id>::failure("unknown manipulator geometry '" + geometryKind.toStdString() + "'");
    auto made = makeManipulator(document_->scene().uniqueName(kind->displayName), kind->id);
    if (!made)
        return Result<Id>::failure(made.message());
    std::unique_ptr<SceneObject> object = made.take();
    // Record the shape's own focus as the design intent to start from.
    const auto points = characteristicPoints(*object->state.geometry);
    if (!points.empty()) {
        ManipulatorSettings settings = dynamic_cast<const ManipulatorComponent*>(object->state.component.get())->settings;
        settings.intendedFocus = points.back().position;  // world = local for a new, untransformed object
        if (points.size() == 2 && points.front().name == "Focus 1")
            settings.intendedSource = points.front().position;
        object->state.component = std::make_shared<ManipulatorComponent>(settings);
    }
    return document_->insertObject(std::move(object));
}

Status ManipulatorController::setSettings(const Id& id, const ManipulatorSettings& settings)
{
    const SceneObject* object = document_->scene().find(id);
    if (!object)
        return Status::failure("the object no longer exists");
    ObjectState state = object->state;
    state.component = std::make_shared<ManipulatorComponent>(settings);
    return document_->setObjectState(id, std::move(state), QStringLiteral("Edit ") + QString::fromStdString(object->state.name));
}

void ManipulatorController::setShowRays(bool show)
{
    showRays_ = show;
    emit displayChanged();
}

QString ManipulatorController::estimatesText(const SceneObject& object, const DisplayUnits& units) const
{
    if (!object.state.geometry)
        return {};
    const Geometry& g = *object.state.geometry;
    const AcousticSettings s = acoustics_->settings();
    const QString unit = units.lengthSymbol();
    const auto len = [&units](double meters) { return QString::number(units.lengthFromSI(meters), 'g', 5); };
    const auto hz = [](double f) { return f >= 1000.0 ? QStringLiteral("%1 kHz").arg(f / 1000.0, 0, 'g', 5) : QStringLiteral("%1 Hz").arg(f, 0, 'g', 5); };
    QStringList lines;

    const Mat4 world = document_->scene().worldMatrix(object);
    for (const NamedPoint& point : characteristicPoints(g)) {
        const Vec3 p = transformPoint(world, point.position);
        lines << QStringLiteral("%1 (world): (%2, %3, %4) %5").arg(QString::fromStdString(point.name), len(p.x()), len(p.y()), len(p.z()), unit);
    }
    const double wavelength = s.medium.wavelength(s.frequencyHz);
    lines << QStringLiteral("Wavelength at %1 in %2: %3 %4").arg(hz(s.frequencyHz), QString::fromStdString(s.medium.name), len(wavelength), unit);

    const std::string& kind = g.kindId();
    if (kind == "reflector.parabolic") {
        lines << QStringLiteral("Rim depth D^2/(16 f): %1 %2").arg(len(parabolaDepth(g.parameter("focalLength"), g.parameter("aperture"))), unit);
        lines << QStringLiteral("Aperture: %1 wavelengths").arg(g.parameter("aperture") / wavelength, 0, 'f', 1);
        lines << QStringLiteral("Ideal property: rays parallel to the axis meet at the focus. The real field depends on wavelength, aperture and source; simulate it.");
    } else if (kind == "reflector.elliptical") {
        lines << QStringLiteral("Focal distance c = sqrt(a^2 - b^2): %1 %2").arg(len(ellipseFocalDistance(g.parameter("semiMajor"), g.parameter("semiMinor"))), unit);
        lines << QStringLiteral("Ideal property: rays from one focus meet at the other. The real field depends on wavelength, aperture and source; simulate it.");
    } else if (kind == "reflector.spherical") {
        const double R = g.parameter("curvatureRadius");
        const double edge = sphericalMirrorAxisCrossing(R, 0.5 * g.parameter("aperture"));
        lines << QStringLiteral("Paraxial focus R/2: %1 %2 from the vertex").arg(len(0.5 * R), unit);
        lines << QStringLiteral("Edge rays cross the axis at %1 %2 (spherical aberration: %3 %2)").arg(len(edge), unit, len(0.5 * R - edge));
    } else if (kind == "lens.spherical") {
        const Material* material = document_->project().materials.find(object.state.materialId);
        const PropertyValue* density = material ? material->property(PhysicsDomain::Acoustic, "density") : nullptr;
        const PropertyValue* speed = material ? material->property(PhysicsDomain::Acoustic, "soundSpeed") : nullptr;
        if (!density || !speed) {
            lines << QStringLiteral("Assign a material with acoustic density and speed of sound to estimate the focal length.");
        } else {
            const LensEstimate e = estimateLens(g.parameter("radius1"), g.parameter("radius2"), g.parameter("centerThickness"),
                                                s.medium.soundSpeed, s.medium.density, speed->value, density->value);
            lines << QStringLiteral("Lens material: %1 (%2)").arg(QString::fromStdString(material->name()), QString::fromUtf8(toString(speed->provenance).data()));
            lines << QStringLiteral("Refractive index n = c_medium / c_lens: %1").arg(e.refractiveIndex, 0, 'g', 4);
            lines << (std::isinf(e.focalLength) ? QStringLiteral("Focal length: none (no focusing power)")
                                                : QStringLiteral("Focal length (thick-lens estimate): %1 %2%3").arg(len(e.focalLength), unit, e.focalLength < 0 ? QStringLiteral(" - DIVERGING") : QString()));
            lines << QStringLiteral("Intensity transmitted at each surface (normal incidence): %1 %, %2 %").arg(100.0 * e.surfaceTransmission1, 0, 'g', 3).arg(100.0 * e.surfaceTransmission2, 0, 'g', 3);
            for (const std::string& assumption : e.assumptions)
                lines << QStringLiteral("  - ") + QString::fromStdString(assumption);
        }
    } else if (kind == "waveguide.rectangular" || kind == "waveguide.circular") {
        const auto modes = kind == "waveguide.rectangular"
                               ? rectangularDuctCutoffs(g.parameter("width"), g.parameter("height"), s.medium.soundSpeed, 4)
                               : circularDuctCutoffs(g.parameter("radius"), s.medium.soundSpeed, 4);
        lines << QStringLiteral("Cut-off frequencies (rigid walls, lossless, uniform cross-section at the entry size):");
        for (const Mode& mode : modes)
            lines << QStringLiteral("  mode (%1,%2): %3").arg(mode.m).arg(mode.n).arg(hz(mode.frequencyHz));
        if (!modes.empty())
            lines << (s.frequencyHz < modes.front().frequencyHz
                          ? QStringLiteral("At %1 only the plane wave propagates.").arg(hz(s.frequencyHz))
                          : QStringLiteral("At %1 higher modes can propagate.").arg(hz(s.frequencyHz)));
        if (kind == "waveguide.circular" && g.parameter("radius") != g.parameter("endRadius"))
            lines << QStringLiteral("The guide is tapered; the estimates above are for the entry radius only.");
    } else if (kind.rfind("cavity.", 0) == 0) {
        std::vector<Mode> modes;
        if (kind == "cavity.rectangular")
            modes = rectangularCavityModes(g.parameter("sizeX"), g.parameter("sizeY"), g.parameter("sizeZ"), s.medium.soundSpeed, 6);
        else if (kind == "cavity.cylindrical")
            modes = cylindricalCavityModes(g.parameter("radius"), g.parameter("length"), s.medium.soundSpeed, 6);
        else
            modes = sphericalCavityModes(g.parameter("radius"), s.medium.soundSpeed, 6);
        lines << QStringLiteral("Lowest resonances (rigid walls, lossless fluid, ideal shape):");
        for (const Mode& mode : modes)
            lines << QStringLiteral("  (%1,%2,%3): %4").arg(mode.m).arg(mode.n).arg(mode.q).arg(hz(mode.frequencyHz));
    }
    lines << QString() << QString::fromStdString(supportDescription(g));
    return lines.join(QLatin1Char('\n'));
}

Result<OptimizationOutcome> ManipulatorController::optimizeReflector(const Id& id, const std::string& parameter, double lower,
                                                                     double upper) const
{
    using Outcome = Result<OptimizationOutcome>;
    const SceneObject* object = document_->scene().find(id);
    const auto* manipulator = object ? dynamic_cast<const ManipulatorComponent*>(object->state.component.get()) : nullptr;
    if (!manipulator || !object->state.geometry || !isReflectorGeometry(*object->state.geometry))
        return Outcome::failure("geometry optimization is available for reflectors only");
    if (!manipulator->settings.intendedFocus)
        return Outcome::failure("give the reflector an intended focal point first");
    if (!acoustics_->reflectorModelSelected())
        return Outcome::failure("select the reflector field model (Acoustics panel, Medium tab): the free-field model "
                                "ignores the reflector, so there would be nothing to optimize");
    const AcousticSettings s = acoustics_->settings();
    std::vector<PointSource> sources;
    for (const PointSource& source : collectSources(document_->scene()))
        if (std::abs(source.frequencyHz - s.frequencyHz) <= 1e-9 * s.frequencyHz)
            sources.push_back(source);
    if (sources.empty())
        return Outcome::failure("there is no active emitter at the simulation frequency");

    const Vec3 focus = *manipulator->settings.intendedFocus;
    ReflectorOptions options;
    options.elementsPerWavelength = s.reflectorElementsPerWavelength;
    const double wavelength = s.medium.wavelength(s.frequencyHz);

    const GeometryObjective objective = [&](const Geometry& candidate) {
        // A private copy of the scene with the candidate geometry in place:
        // the document is never touched.
        Scene trial;
        for (const auto& child : document_->scene().root().children())
            [[maybe_unused]] const auto inserted = trial.insert(child->cloneSubtree(false));
        ObjectState state = trial.find(id)->state;
        state.geometry = candidate;
        [[maybe_unused]] const Status status = trial.setState(id, state);
        const ReflectorModel model(s.propagation,
                                   collectReflectors(trial, document_->project().materials, wavelength, options).surfaces);
        Complex pressure(0.0, 0.0);
        for (const PointSource& source : sources)
            pressure += source.amplitude * model.transfer(source, focus, s.medium).value;
        return std::abs(pressure);
    };
    OptimizationSettings settings;
    settings.maxEvaluations = 40;
    settings.tolerance = 5e-3;
    OptimizationOutcome outcome = optimizeGeometry(*object->state.geometry, {{parameter, lower, upper}}, objective, settings);
    if (!outcome.bestGeometry)
        return Outcome::failure(outcome.notes.empty() ? std::string("the optimization could not run") : outcome.notes.front());
    return outcome;
}

Status ManipulatorController::applyGeometry(const Id& id, const Geometry& geometry)
{
    const SceneObject* object = document_->scene().find(id);
    if (!object)
        return Status::failure("the object no longer exists");
    ObjectState state = object->state;
    state.geometry = geometry;
    return document_->setObjectState(id, std::move(state),
                                     QStringLiteral("Apply optimized geometry to ") + QString::fromStdString(object->state.name));
}

SceneRenderBridge::Decorator ManipulatorController::makeDecorator()
{
    return [this](const Scene& scene, const SceneObject& object, const Mat4& world, bool selected,
                  SceneRenderBridge::Frame& frame, std::vector<SceneRenderBridge::Pickable>&) {
        const auto* manipulator = dynamic_cast<const ManipulatorComponent*>(object.state.component.get());
        if (!manipulator || !object.state.geometry)
            return;
        const Aabb bounds = object.state.geometry->tessellate().bounds();
        const double size = bounds.isValid() ? 0.05 * bounds.size().maxCoeff() : 0.01;
        const auto cross = [&frame, size](const Vec3& centre, const Eigen::Vector4f& color) {
            for (const Vec3& axis : {Vec3(Vec3::UnitX()), Vec3(Vec3::UnitY()), Vec3(Vec3::UnitZ())})
                frame.lines.push_back({centre - size * axis, centre + size * axis, color});
            frame.bounds.extend(centre);
        };
        // The shape's own focal points (geometry), the designer's intended
        // focus and source (design intent): different colours, never merged.
        for (const NamedPoint& point : characteristicPoints(*object.state.geometry))
            cross(transformPoint(world, point.position), Eigen::Vector4f(0.85f, 0.45f, 0.95f, 1.0f));
        if (manipulator->settings.intendedFocus)
            cross(*manipulator->settings.intendedFocus, Eigen::Vector4f(0.25f, 0.95f, 0.45f, 1.0f));
        if (manipulator->settings.intendedSource)
            cross(*manipulator->settings.intendedSource, Eigen::Vector4f(1.0f, 0.65f, 0.2f, 1.0f));

        if (showRays_ && selected && isReflectorGeometry(*object.state.geometry)) {
            std::optional<Vec3> source = manipulator->settings.intendedSource;
            if (!source) {
                const auto sources = collectSources(scene);
                if (!sources.empty())
                    source = sources.front().position;
            }
            if (source) {
                const double length = bounds.isValid() ? 2.0 * bounds.size().maxCoeff() : 0.2;
                for (const RayPath& path : traceRays(scene, object, *source, 24, length)) {
                    const Eigen::Vector4f color = path.hit ? Eigen::Vector4f(1.0f, 0.9f, 0.3f, 1.0f) : Eigen::Vector4f(0.5f, 0.5f, 0.5f, 1.0f);
                    for (size_t i = 0; i + 1 < path.points.size(); ++i)
                        frame.lines.push_back({path.points[i], path.points[i + 1], color});
                }
            }
        }
    };
}
