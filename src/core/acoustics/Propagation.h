// Sources and propagation models of the frequency-domain analytical engine.
//
// A PointSource is what a propagation model sees: a position, an axis, a
// complex strength and a directivity. Scene emitters and array elements are
// turned into PointSources by collectSources(); nothing below knows about
// scenes, widgets or rendering.
#pragma once

#include "acoustics/Components.h"
#include "acoustics/Medium.h"

#include <Eigen/Core>

#include <memory>
#include <string>
#include <vector>

namespace resonant::acoustics {

struct PointSource {
    Id objectId;            // the emitter or array object
    int elementIndex = -1;  // index within an array; -1 for a single emitter
    std::string label;      // "Array / element 3"
    Vec3 position = Vec3::Zero();  // world, m
    Vec3 forward = Vec3::UnitZ();  // world, unit
    double frequencyHz = 40000.0;
    // Complex peak pressure amplitude at 1 m on axis, in Pa: the emitter's
    // amplitude with its phase and time delay folded in,
    //   amplitude = A * exp(j * (phase - omega * delay)).
    Complex amplitude = Complex(1.0, 0.0);
    Directivity directivity;
    double radius = 0.005;  // m, physical size; sets the near-source limit
    bool physicalTransducer = false;
};

// The complex strength of an emitter: amplitude, phase and delay combined.
Complex complexAmplitude(const EmitterSettings& settings);

// Every ACTIVE emitter and array element of the scene (or of the listed
// objects only, when `only` is not empty), in world coordinates.
std::vector<PointSource> collectSources(const Scene& scene, const std::vector<Id>& only = {});

struct PropagationOptions {
    bool includeAttenuation = true;   // exp(-alpha r) with the medium's attenuation
    bool includeDirectivity = true;   // otherwise every source is treated as omnidirectional
    // Lower limit of the distance used in 1/r, in addition to each source's
    // own radius. Inside max(radius, minimumDistance) of a source the field
    // of that source is held at its value on that sphere and the sample is
    // flagged: the point-source model is not valid there.
    double minimumDistance = 1e-4;    // m

    friend bool operator==(const PropagationOptions&, const PropagationOptions&) = default;
};

struct Transfer {
    Complex value = Complex(0.0, 0.0);              // pressure at the point per unit source amplitude (1/1)
    Eigen::Vector3cd gradient = Eigen::Vector3cd::Zero();  // its spatial gradient, 1/m
    bool nearSource = false;                        // the point is inside the source's near-source limit
};

// A propagation model answers one question: what complex pressure does a
// unit-strength source produce at a point? Models are interchangeable.
class PropagationModel {
public:
    virtual ~PropagationModel() = default;

    virtual std::string id() const = 0;
    virtual std::string name() const = 0;
    virtual std::string version() const = 0;
    // What the model assumes and what it ignores, in sentences for the user.
    virtual std::vector<std::string> assumptions() const = 0;

    virtual Transfer transfer(const PointSource& source, const Vec3& point, const Medium& medium,
                              bool withGradient = false) const = 0;
};

// Homogeneous free field:
//   G(x) = r_ref * D(theta) * exp(-alpha r) * exp(-j k r) / r
// with r the distance from the source, theta the angle from its axis. No
// reflection, transmission, refraction, diffraction or scattering: objects
// in the scene do not affect the field.
class FreeFieldModel final : public PropagationModel {
public:
    explicit FreeFieldModel(PropagationOptions options = {}) : options_(options) {}

    const PropagationOptions& options() const { return options_; }

    std::string id() const override { return "acoustic.freefield.analytical"; }
    std::string name() const override { return "Analytical free field (point-source superposition)"; }
    std::string version() const override { return "1"; }
    std::vector<std::string> assumptions() const override;

    Transfer transfer(const PointSource& source, const Vec3& point, const Medium& medium,
                      bool withGradient = false) const override;

private:
    PropagationOptions options_;
};

}  // namespace resonant::acoustics
