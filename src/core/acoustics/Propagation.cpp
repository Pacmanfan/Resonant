#include "acoustics/Propagation.h"

#include "acoustics/Conventions.h"
#include "foundation/Units.h"

#include <algorithm>
#include <cmath>

namespace resonant::acoustics {

Complex complexAmplitude(const EmitterSettings& settings)
{
    const double omega = 2.0 * units::pi * settings.frequencyHz;
    return std::polar(settings.amplitudePa, settings.phaseRad - omega * settings.delaySec);
}

namespace {

PointSource makeSource(const SceneObject& object, const Mat4& world, const EmitterSettings& settings,
                       const Vec3& localPosition, const Vec3& localForward, int elementIndex)
{
    PointSource source;
    source.objectId = object.id();
    source.elementIndex = elementIndex;
    source.label = elementIndex < 0 ? object.state.name
                                    : object.state.name + " / element " + std::to_string(elementIndex);
    source.position = transformPoint(world, localPosition);
    source.forward = transformDirection(world, localForward).normalized();
    source.frequencyHz = settings.frequencyHz;
    source.amplitude = complexAmplitude(settings);
    source.amplitudePa = settings.amplitudePa;
    source.phaseRad = settings.phaseRad;
    source.delaySec = settings.delaySec;
    source.directivity = settings.directivity;
    source.radius = settings.radius;
    source.physicalTransducer = settings.physicalTransducer;
    return source;
}

}  // namespace

std::vector<PointSource> collectSources(const Scene& scene, const std::vector<Id>& only)
{
    std::vector<PointSource> sources;
    scene.forEach([&](const SceneObject& object) {
        if (!only.empty() && std::find(only.begin(), only.end(), object.id()) == only.end())
            return;
        const Mat4 world = scene.worldMatrix(object);
        if (const auto* emitter = componentOf<EmitterComponent>(object)) {
            if (emitter->settings.active)
                sources.push_back(makeSource(object, world, emitter->settings, Vec3::Zero(), Vec3::UnitZ(), -1));
        } else if (const auto* array = componentOf<ArrayComponent>(object)) {
            for (const ArrayElement& element : generateElements(array->settings))
                if (element.settings.active)
                    sources.push_back(makeSource(object, world, element.settings, element.localPosition,
                                                 element.localForward, element.index));
        }
    });
    return sources;
}

std::vector<std::string> FreeFieldModel::assumptions() const
{
    std::vector<std::string> list = {
        "Free field: a homogeneous medium at rest with no boundaries.",
        "Objects in the scene do NOT influence the field: no reflection, transmission, refraction, diffraction or "
        "scattering is calculated.",
        "Each source is a point source with spherical spreading, p = A (1 m / r) D(theta) exp(-j k r); the total "
        "field is the linear superposition of the sources.",
        "Harmonic steady state at one frequency; time convention exp(+j omega t). Amplitudes are peak values.",
        "Within a source's radius the point-source model is not valid; the field there is held at its value on "
        "that sphere and flagged.",
    };
    list.push_back(options_.includeDirectivity
                       ? "Directivity is the far-field pattern of each source, applied at every distance (an "
                         "approximation close to a physical transducer)."
                       : "Directivity is ignored: every source radiates equally in all directions.");
    list.push_back(options_.includeAttenuation
                       ? "Attenuation is a single exponential factor exp(-alpha r) with the medium's coefficient at "
                         "the simulated frequency."
                       : "Attenuation is ignored.");
    return list;
}

Transfer FreeFieldModel::transfer(const PointSource& source, const Vec3& point, const Medium& medium,
                                  bool withGradient) const
{
    Transfer result;
    const Vec3 offset = point - source.position;
    const double distance = offset.norm();
    const double limit = std::max(source.radius, options_.minimumDistance);
    result.nearSource = distance < limit;
    const double r = std::max(distance, limit);
    // Inside the limit the direction is undefined at the centre; use the
    // source axis there.
    const Vec3 direction = distance > 1e-15 ? Vec3(offset / distance) : source.forward;

    const double k = medium.wavenumber(source.frequencyHz);
    const double alpha = options_.includeAttenuation ? medium.attenuation : 0.0;
    const double directivity =
        options_.includeDirectivity ? source.directivity.evaluate(direction.dot(source.forward), k) : 1.0;

    const Complex spreading = std::polar(kReferenceDistance * std::exp(-alpha * r) / r, -k * r);
    result.value = directivity * spreading;
    if (withGradient && !result.nearSource) {
        // d/dr of exp(-(alpha + j k) r) / r, along the radial direction. The
        // angular derivative of the directivity is neglected, which is exact
        // for omnidirectional sources.
        const Complex radial = -(Complex(alpha, k) + 1.0 / r) * result.value;
        result.gradient = radial * direction.cast<Complex>();
    }
    return result;
}

}  // namespace resonant::acoustics
