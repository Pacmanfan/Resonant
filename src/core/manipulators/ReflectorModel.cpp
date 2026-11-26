#include "manipulators/ReflectorModel.h"

#include "foundation/Units.h"

#include <cmath>

namespace resonant::manipulators {

using acoustics::FieldSample;
using acoustics::Medium;
using acoustics::PointSource;
using acoustics::Transfer;
using units::pi;

namespace {

double reflectionCoefficient(BoundaryCondition boundary, double surfaceImpedance, double mediumImpedance, double cosTheta)
{
    switch (boundary) {
    case BoundaryCondition::Rigid: return 1.0;
    case BoundaryCondition::PressureRelease: return -1.0;
    case BoundaryCondition::Absorbing: return 0.0;
    case BoundaryCondition::Impedance: {
        const double zc = surfaceImpedance * std::max(cosTheta, 0.0);
        return (zc - mediumImpedance) / (zc + mediumImpedance);
    }
    }
    return 1.0;
}

// Contribution of one surface element with surface pressure P and normal
// derivative D (for the unit normal n) to the field at x.
Complex elementField(const Vec3& x, const Vec3& y, const Vec3& n, double area, const Complex& P, const Complex& D,
                     double k, double alpha, bool* tooClose)
{
    const Vec3 d = x - y;
    const double distance = d.norm();
    // Closer than half an element the element is no longer a point: hold
    // the kernel at that distance. Any point on the surface is within 0.71
    // element sizes of an element centre, so points that close are flagged.
    const double limit = 0.5 * std::sqrt(area);
    if (distance < 1.5 * limit && tooClose)
        *tooClose = true;
    const double rho = std::max(distance, limit);
    const Complex green = std::polar(std::exp(-alpha * rho) / (4.0 * pi * rho), -k * rho);
    const double cosine = distance > 1e-15 ? n.dot(d) / distance : 0.0;
    const Complex greenNormal = (Complex(alpha + 1.0 / rho, k)) * green * cosine;
    return (P * greenNormal - D * green) * area;
}

}  // namespace

ReflectorCollection collectReflectors(const Scene& scene, const MaterialLibrary& materials, double wavelength,
                                      const ReflectorOptions& options)
{
    ReflectorCollection collection;
    const double elementSize = wavelength / std::max(2.0, options.elementsPerWavelength);
    scene.forEach([&](const SceneObject& object) {
        const auto* manipulator = dynamic_cast<const ManipulatorComponent*>(object.state.component.get());
        if (!manipulator || !object.state.geometry)
            return;
        if (!scene.isEffectivelyVisible(object)) {
            collection.notes.push_back("\"" + object.state.name + "\" is hidden and was left out of the calculation.");
            return;
        }
        if (!isReflectorGeometry(*object.state.geometry)) {
            collection.notes.push_back("\"" + object.state.name + "\" (" + object.state.geometry->kind().displayName
                                       + ") has no field model: it does NOT affect the calculated field.");
            return;
        }
        ReflectingSurface surface;
        surface.objectId = object.id();
        surface.label = object.state.name;
        surface.boundary = manipulator->settings.boundary;
        if (surface.boundary == BoundaryCondition::Impedance) {
            const Material* material = materials.find(object.state.materialId);
            const PropertyValue* density = material ? material->property(PhysicsDomain::Acoustic, "density") : nullptr;
            const PropertyValue* speed = material ? material->property(PhysicsDomain::Acoustic, "soundSpeed") : nullptr;
            if (density && speed) {
                surface.impedance = density->value * speed->value;
            } else {
                surface.boundary = BoundaryCondition::Rigid;
                collection.notes.push_back("\"" + object.state.name + "\" has an impedance boundary but no material "
                                           "with acoustic density and speed of sound; it was treated as rigid.");
            }
        }
        const Mat4 world = scene.worldMatrix(object);
        const Mat3 linear = world.block<3, 3>(0, 0);
        const Mat3 normalMatrix = linear.inverse().transpose();
        const double determinant = std::abs(linear.determinant());
        for (const SurfaceElement& local : sampleReflectorSurface(*object.state.geometry, elementSize)) {
            const Vec3 transformed = normalMatrix * local.normal;
            SurfaceElement element;
            element.position = transformPoint(world, local.position);
            element.normal = transformed.normalized();
            // Area of a transformed surface patch: |det M| |M^-T n|.
            element.area = local.area * determinant * transformed.norm();
            surface.elements.push_back(element);
        }
        collection.elementCount += surface.elements.size();
        collection.surfaces.push_back(std::move(surface));
    });
    return collection;
}

size_t estimateReflectorElements(const Scene& scene, double wavelength, const ReflectorOptions& options)
{
    size_t count = 0;
    const double elementSize = wavelength / std::max(2.0, options.elementsPerWavelength);
    scene.forEach([&](const SceneObject& object) {
        if (dynamic_cast<const ManipulatorComponent*>(object.state.component.get()) && object.state.geometry
            && isReflectorGeometry(*object.state.geometry) && scene.isEffectivelyVisible(object)) {
            // The local estimate, scaled by the object's mean linear scale squared.
            const double scale = std::cbrt(std::abs(scene.worldMatrix(object).block<3, 3>(0, 0).determinant()));
            count += estimateSurfaceElements(*object.state.geometry, elementSize / std::max(scale, 1e-12));
        }
    });
    return count;
}

ReflectorModel::ReflectorModel(acoustics::PropagationOptions options, std::vector<ReflectingSurface> surfaces)
    : direct_(options), surfaces_(std::move(surfaces))
{
}

std::vector<std::string> ReflectorModel::assumptions() const
{
    size_t elements = 0;
    for (const ReflectingSurface& surface : surfaces_)
        elements += surface.elements.size();
    std::vector<std::string> list = {
        "Sources radiate as in the analytical free-field model (point sources, spherical spreading, harmonic "
        "steady state at one frequency, exp(+j omega t), peak amplitudes).",
        "Reflectors are included by the Kirchhoff (physical-optics) approximation: each surface element carries "
        "the field an infinite plane with its reflection coefficient would have, and the scattered field is the "
        "Helmholtz-Kirchhoff integral over the " + std::to_string(elements) + " surface element(s) of "
            + std::to_string(surfaces_.size()) + " reflector(s).",
        "This is an APPROXIMATION, not a full-wave solution: one reflection only (no multiple reflections, no "
        "resonance between surfaces), no shadowing of one surface by another, thin surfaces reflecting on both "
        "sides, and reduced accuracy where a reflector or its radius of curvature is not large against the "
        "wavelength.",
        "Diffraction from the finite aperture and the interference of all paths are included, so focal spots "
        "have a realistic, wavelength-limited size.",
        "Only reflector manipulators take part. Lenses, waveguides, cavities, diffraction structures and ordinary "
        "geometry do NOT affect the calculated field.",
        "Particle velocity and intensity are not calculated by this model.",
    };
    for (const std::string& assumption : direct_.assumptions())
        if (assumption.find("Directivity") != std::string::npos || assumption.find("Attenuation") != std::string::npos)
            list.push_back(assumption);
    return list;
}

Complex ReflectorModel::scattered(const PointSource& source, const Vec3& point, const Medium& medium) const
{
    const double k = medium.wavenumber(source.frequencyHz);
    const double alpha = direct_.options().includeAttenuation ? medium.attenuation : 0.0;
    Complex total(0.0, 0.0);
    for (const ReflectingSurface& surface : surfaces_) {
        for (const SurfaceElement& element : surface.elements) {
            const Vec3 toSource = source.position - element.position;
            const double distance = toSource.norm();
            const Vec3 n = element.normal.dot(toSource) >= 0.0 ? element.normal : Vec3(-element.normal);
            const double cosTheta = distance > 1e-15 ? n.dot(toSource) / distance : 1.0;
            const double R = reflectionCoefficient(surface.boundary, surface.impedance, medium.impedance(), cosTheta);
            const Transfer incident = direct_.transfer(source, element.position, medium, true);
            const Complex normalDerivative = n.cast<Complex>().dot(incident.gradient);  // conj of real n is n
            total += elementField(point, element.position, n, element.area, (1.0 + R) * incident.value,
                                  (1.0 - R) * normalDerivative, k, alpha, nullptr);
        }
    }
    return total;
}

Transfer ReflectorModel::transfer(const PointSource& source, const Vec3& point, const Medium& medium, bool) const
{
    Transfer result = direct_.transfer(source, point, medium, false);
    result.value += scattered(source, point, medium);
    return result;
}

ReflectorFieldEvaluator::ReflectorFieldEvaluator(const std::vector<PointSource>& sources, double frequencyHz, Medium medium,
                                                 std::shared_ptr<const ReflectorModel> model)
    : FieldEvaluator(sources, frequencyHz, std::move(medium),
                     // The base class evaluates the direct field only; the
                     // scattered part is added by sample() below.
                     std::make_shared<acoustics::FreeFieldModel>(model->directOptions())),
      reflector_(std::move(model))
{
    // Sum the surface values of every source once.
    for (const ReflectingSurface& surface : reflector_->surfaces()) {
        for (const SurfaceElement& element : surface.elements) {
            Loaded loaded;
            loaded.position = element.position;
            loaded.normal = element.normal;
            loaded.area = element.area;
            for (const PointSource& source : sources_) {
                const Vec3 toSource = source.position - element.position;
                const double distance = toSource.norm();
                const bool front = element.normal.dot(toSource) >= 0.0;
                const Vec3 n = front ? element.normal : Vec3(-element.normal);
                const double cosTheta = distance > 1e-15 ? n.dot(toSource) / distance : 1.0;
                const double R = reflectionCoefficient(surface.boundary, surface.impedance, medium_.impedance(), cosTheta);
                const Transfer incident = model_->transfer(source, element.position, medium_, true);
                const Complex pressure = source.amplitude * (1.0 + R) * incident.value;
                const Complex derivative = source.amplitude * (1.0 - R) * n.cast<Complex>().dot(incident.gradient);
                if (front) {
                    loaded.pressureFront += pressure;
                    loaded.derivativeFront += derivative;
                    loaded.front = true;
                } else {
                    loaded.pressureBack += pressure;
                    loaded.derivativeBack += derivative;
                    loaded.back = true;
                }
            }
            elements_.push_back(loaded);
        }
    }
}

Complex ReflectorFieldEvaluator::direct(const Vec3& point) const
{
    return FieldEvaluator::sample(point).pressure;
}

Complex ReflectorFieldEvaluator::scatteredField(const Vec3& point) const
{
    const double k = medium_.wavenumber(frequencyHz_);
    const double alpha = reflector_->directOptions().includeAttenuation ? medium_.attenuation : 0.0;
    Complex total(0.0, 0.0);
    for (const Loaded& e : elements_) {
        if (e.front)
            total += elementField(point, e.position, e.normal, e.area, e.pressureFront, e.derivativeFront, k, alpha, nullptr);
        if (e.back)
            total += elementField(point, e.position, -e.normal, e.area, e.pressureBack, e.derivativeBack, k, alpha, nullptr);
    }
    return total;
}

FieldSample ReflectorFieldEvaluator::sample(const Vec3& point) const
{
    FieldSample result = FieldEvaluator::sample(point);
    const double k = medium_.wavenumber(frequencyHz_);
    const double alpha = reflector_->directOptions().includeAttenuation ? medium_.attenuation : 0.0;
    bool tooClose = false;
    for (const Loaded& e : elements_) {
        if (e.front)
            result.pressure += elementField(point, e.position, e.normal, e.area, e.pressureFront, e.derivativeFront, k, alpha, &tooClose);
        if (e.back)
            result.pressure += elementField(point, e.position, -e.normal, e.area, e.pressureBack, e.derivativeBack, k, alpha, &tooClose);
    }
    // On the surface itself the integral is not valid; flag it like a point
    // inside a source.
    result.nearSource = result.nearSource || tooClose;
    return result;
}

std::vector<RayPath> traceRays(const Scene& scene, const SceneObject& reflector, const Vec3& source, int rayCount,
                               double reflectedLength)
{
    std::vector<RayPath> paths;
    if (!reflector.state.geometry || !isReflectorGeometry(*reflector.state.geometry) || rayCount < 1)
        return paths;
    const Geometry& geometry = *reflector.state.geometry;
    const Mat4 world = scene.worldMatrix(reflector);
    const Mat4 inverse = world.inverse();
    const Vec3 localSource = transformPoint(inverse, source);

    // Aim at points spread over the surface: a coarse sampling of it.
    const Aabb bounds = geometry.tessellate().bounds();
    double spacing = bounds.isValid() ? bounds.size().maxCoeff() / std::sqrt(static_cast<double>(rayCount)) : 1.0;
    std::vector<SurfaceElement> aims = sampleReflectorSurface(geometry, spacing);
    while (static_cast<int>(aims.size()) > 2 * rayCount && spacing < 1e6) {
        spacing *= 1.3;
        aims = sampleReflectorSurface(geometry, spacing);
    }
    for (const SurfaceElement& aim : aims) {
        const Vec3 direction = aim.position - localSource;
        if (direction.norm() < 1e-12)
            continue;
        RayPath path;
        path.points.push_back(source);
        if (const auto hit = reflectRay(geometry, Ray{localSource, direction})) {
            path.hit = true;
            path.points.push_back(transformPoint(world, hit->point));
            const Vec3 reflected = transformDirection(world, hit->reflected).normalized();
            path.points.push_back(path.points.back() + reflectedLength * reflected);
        } else {
            path.points.push_back(transformPoint(world, aim.position));
        }
        paths.push_back(std::move(path));
    }
    return paths;
}

namespace {

double magnitudeAt(const acoustics::FieldResult& result, int iu, int iv, int iw)
{
    return std::abs(result.pressure[result.frame.index(iu, iv, iw)]);
}

// Width at half the peak along one axis through the peak; 0 if the level
// does not fall below half on both sides inside the grid.
double halfWidth(const acoustics::FieldResult& result, const int peak[3], int axis)
{
    const acoustics::GridFrame& frame = result.frame;
    const int counts[3] = {frame.nu, frame.nv, frame.nw};
    if (counts[axis] < 3)
        return 0.0;
    const Vec3 axes[3] = {frame.axisU, frame.axisV, frame.axisW};
    const double step = axes[axis].norm() / (counts[axis] - 1);
    const double half = 0.5 * magnitudeAt(result, peak[0], peak[1], peak[2]);
    double width = 0.0;
    for (const int direction : {-1, 1}) {
        int index[3] = {peak[0], peak[1], peak[2]};
        double previous = 2.0 * half;
        bool crossed = false;
        for (int n = 1;; ++n) {
            index[axis] = peak[axis] + direction * n;
            if (index[axis] < 0 || index[axis] >= counts[axis])
                break;
            const double value = magnitudeAt(result, index[0], index[1], index[2]);
            if (value < half) {
                // Linear interpolation of the crossing between the last two samples.
                width += step * ((n - 1) + (previous - half) / (previous - value));
                crossed = true;
                break;
            }
            previous = value;
        }
        if (!crossed)
            return 0.0;
    }
    return width;
}

}  // namespace

FocusMetrics analyzeFocus(const acoustics::FieldResult& result, const std::optional<Vec3>& intended)
{
    FocusMetrics metrics;
    const acoustics::GridFrame& frame = result.frame;
    if (result.pressure.empty())
        return metrics;
    int peak[3] = {0, 0, 0};
    double best = -1.0;
    for (int iw = 0; iw < frame.nw; ++iw)
        for (int iv = 0; iv < frame.nv; ++iv)
            for (int iu = 0; iu < frame.nu; ++iu) {
                if (result.nearSource[frame.index(iu, iv, iw)])
                    continue;
                const double value = magnitudeAt(result, iu, iv, iw);
                if (value > best) {
                    best = value;
                    peak[0] = iu;
                    peak[1] = iv;
                    peak[2] = iw;
                }
            }
    if (best < 0.0)
        return metrics;
    metrics.valid = true;
    metrics.peakMagnitude = best;
    metrics.peakPoint = frame.point(peak[0], peak[1], peak[2]);
    metrics.widthU = halfWidth(result, peak, 0);
    metrics.widthV = halfWidth(result, peak, 1);
    metrics.widthW = halfWidth(result, peak, 2);

    // Largest other local maximum along U through the peak.
    double sidelobe = -1.0;
    for (int iu = 1; iu + 1 < frame.nu; ++iu) {
        if (iu == peak[0])
            continue;
        const double value = magnitudeAt(result, iu, peak[1], peak[2]);
        if (value > magnitudeAt(result, iu - 1, peak[1], peak[2]) && value >= magnitudeAt(result, iu + 1, peak[1], peak[2]))
            sidelobe = std::max(sidelobe, value);
    }
    if (sidelobe >= 0.0 && best > 0.0)
        metrics.sidelobeRatio = sidelobe / best;

    if (intended) {
        metrics.displacement = (metrics.peakPoint - *intended).norm();
        // Trilinear sample at the intended point, when it lies inside the grid.
        const Vec3 relative = *intended - frame.origin;
        const Vec3 axes[3] = {frame.axisU, frame.axisV, frame.axisW};
        const int counts[3] = {frame.nu, frame.nv, frame.nw};
        double fraction[3] = {0.0, 0.0, 0.0};
        bool inside = true;
        for (int a = 0; a < 3; ++a) {
            if (counts[a] < 2) {
                // A plane: the point must lie in it (within half a sample spacing).
                const Vec3 normal = axes[0].cross(axes[1]);
                if (normal.norm() > 0.0 && std::abs(relative.dot(normal.normalized())) > 0.5 * frame.maxSpacing())
                    inside = false;
                continue;
            }
            fraction[a] = relative.dot(axes[a]) / axes[a].squaredNorm() * (counts[a] - 1);
            if (fraction[a] < -1e-9 || fraction[a] > counts[a] - 1 + 1e-9)
                inside = false;
        }
        if (inside) {
            int base[3];
            double weight[3];
            for (int a = 0; a < 3; ++a) {
                base[a] = counts[a] < 2 ? 0 : std::clamp(static_cast<int>(std::floor(fraction[a])), 0, counts[a] - 2);
                weight[a] = counts[a] < 2 ? 0.0 : std::clamp(fraction[a] - base[a], 0.0, 1.0);
            }
            double value = 0.0;
            for (int corner = 0; corner < 8; ++corner) {
                double w = 1.0;
                int index[3];
                bool skip = false;
                for (int a = 0; a < 3; ++a) {
                    const int bit = (corner >> a) & 1;
                    if (counts[a] < 2 && bit)
                        skip = true;
                    index[a] = base[a] + (counts[a] < 2 ? 0 : bit);
                    w *= bit ? weight[a] : 1.0 - weight[a];
                }
                if (!skip && w > 0.0)
                    value += w * magnitudeAt(result, index[0], index[1], index[2]);
            }
            metrics.magnitudeAtIntended = value;
        }
    }
    return metrics;
}

double focalGain(const ReflectorFieldEvaluator& evaluator, const Vec3& point)
{
    const double without = std::abs(evaluator.direct(point));
    return without > 0.0 ? std::abs(evaluator.pressure(point)) / without : std::numeric_limits<double>::infinity();
}

acoustics::FocusSolution computeModelFocus(const std::vector<PointSource>& arraySources, const Vec3& target,
                                           const Medium& medium, const acoustics::PropagationModel& model)
{
    acoustics::FocusSolution solution;
    solution.control = acoustics::FocusControl::Phase;
    solution.soundSpeed = medium.soundSpeed;
    if (arraySources.empty())
        return solution;
    solution.frequencyHz = arraySources.front().frequencyHz;
    for (const PointSource& source : arraySources) {
        acoustics::FocusElement element;
        element.index = source.elementIndex;
        element.distance = (target - source.position).norm();
        double phase = std::fmod(-std::arg(model.transfer(source, target, medium).value), 2.0 * pi);
        if (phase < 0.0)
            phase += 2.0 * pi;
        element.phaseRad = phase;
        solution.elements.push_back(element);
    }
    return solution;
}

}  // namespace resonant::manipulators
