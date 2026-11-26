// The reflector field model: what rigid, pressure-release, impedance or
// absorbing reflectors add to the free field of the sources, by the
// Kirchhoff (physical-optics) approximation.
//
// For a source with incident field p_i and a surface S with unit normal n
// towards the source, each surface element is given the total pressure and
// normal derivative an infinite plane with the element's reflection
// coefficient R would have,
//
//     p        = (1 + R) p_i          dp/dn = (1 - R) dp_i/dn
//
// and the scattered field follows from the Helmholtz-Kirchhoff integral
//
//     p_s(x) = integral over S of [ p dG/dn  -  G dp/dn ] dS,
//     G(x, y) = exp(-j k |x - y|) / (4 pi |x - y|).
//
// R is +1 for a rigid surface, -1 for a pressure-release surface, 0 for an
// absorbing one, and (Z cos(theta) - rho c) / (Z cos(theta) + rho c) for a
// locally reacting surface of impedance Z at the angle of incidence theta.
//
// WHAT THIS IS: a high-frequency approximation that includes the phase of
// every path and the diffraction of the finite aperture, so it produces
// focal spots of realistic, wavelength-limited size.
// WHAT THIS IS NOT: a full-wave solution. It has one reflection only (no
// multiple reflections, no cavity resonance), no shadowing of one part of a
// surface or of one reflector by another, and it degrades when the surface
// or its radii of curvature are not large against the wavelength.
#pragma once

#include "acoustics/FieldEvaluator.h"
#include "acoustics/FieldGrid.h"
#include "acoustics/Focusing.h"
#include "manipulators/Manipulator.h"
#include "manipulators/ReflectorSurface.h"

namespace resonant::manipulators {

struct ReflectingSurface {
    Id objectId;
    std::string label;
    BoundaryCondition boundary = BoundaryCondition::Rigid;
    double impedance = 0.0;                 // Pa s/m, for BoundaryCondition::Impedance
    std::vector<SurfaceElement> elements;   // world coordinates
};

struct ReflectorOptions {
    double elementsPerWavelength = 6.0;  // surface elements per wavelength, >= 2
    friend bool operator==(const ReflectorOptions&, const ReflectorOptions&) = default;
};

struct ReflectorCollection {
    std::vector<ReflectingSurface> surfaces;
    size_t elementCount = 0;
    // Manipulators that are present but have no field model (lenses,
    // waveguides, ...), and other things the user must know: reported, never
    // silently ignored.
    std::vector<std::string> notes;
};

// The visible reflector manipulators of a scene as surface elements at the
// resolution the wavelength needs. A reflector with an impedance boundary
// takes rho c from the acoustic properties of its assigned material.
ReflectorCollection collectReflectors(const Scene& scene, const MaterialLibrary& materials, double wavelength,
                                      const ReflectorOptions& options = {});
// How many surface elements that would be (for preflight), without building them.
size_t estimateReflectorElements(const Scene& scene, double wavelength, const ReflectorOptions& options = {});

// Free field plus the Kirchhoff scattering of reflectors, source by source.
// Used where per-source responses are needed (inverse design, hybrid
// focusing). Cost per evaluation: one pass over every surface element.
class ReflectorModel final : public acoustics::PropagationModel {
public:
    ReflectorModel(acoustics::PropagationOptions options, std::vector<ReflectingSurface> surfaces);

    std::string id() const override { return "acoustic.reflector.kirchhoff"; }
    std::string name() const override { return "Free field + reflectors (Kirchhoff approximation)"; }
    std::string version() const override { return "1"; }
    std::vector<std::string> assumptions() const override;

    // The gradient is not provided by this model (it is returned as zero).
    acoustics::Transfer transfer(const acoustics::PointSource& source, const Vec3& point, const acoustics::Medium& medium,
                                 bool withGradient = false) const override;

    // Only the scattered part.
    Complex scattered(const acoustics::PointSource& source, const Vec3& point, const acoustics::Medium& medium) const;

    const std::vector<ReflectingSurface>& surfaces() const { return surfaces_; }
    const acoustics::PropagationOptions& directOptions() const { return direct_.options(); }

private:
    acoustics::FreeFieldModel direct_;
    std::vector<ReflectingSurface> surfaces_;
};

// The same physics for all sources at once: the surface values of every
// source are summed once, so sampling a point costs one pass over the
// surface elements whatever the number of sources. Used for field grids.
class ReflectorFieldEvaluator final : public acoustics::FieldEvaluator {
public:
    ReflectorFieldEvaluator(const std::vector<acoustics::PointSource>& sources, double frequencyHz,
                            acoustics::Medium medium, std::shared_ptr<const ReflectorModel> model);

    acoustics::FieldSample sample(const Vec3& point) const override;
    bool providesVelocity() const override { return false; }
    Eigen::Vector3cd particleVelocity(const Vec3&) const override { return Eigen::Vector3cd::Zero(); }
    std::vector<std::string> assumptions() const override { return reflector_->assumptions(); }
    std::string solverId() const override { return reflector_->id(); }
    std::string solverVersion() const override { return reflector_->version(); }

    // Free field of the sources only, and the scattered part only.
    Complex direct(const Vec3& point) const;
    Complex scatteredField(const Vec3& point) const;
    size_t surfaceElementCount() const { return elements_.size(); }

private:
    // One surface element with the summed surface values of the sources on
    // its front (+normal) and on its back.
    struct Loaded {
        Vec3 position;
        Vec3 normal;
        double area;
        Complex pressureFront, derivativeFront;   // (1 + R) p_i  and  (1 - R) dp_i/dn, n = +normal
        Complex pressureBack, derivativeBack;     // the same for sources behind, n = -normal
        bool front = false;
        bool back = false;
    };
    std::shared_ptr<const ReflectorModel> reflector_;
    std::vector<Loaded> elements_;
};

// --- geometrical rays ----------------------------------------------------------------

struct RayPath {
    std::vector<Vec3> points;  // origin, hit point, and a point along the reflected ray (world)
    bool hit = false;
};

// A fan of rays from a point source towards a reflector object, each
// reflected once. A geometrical-acoustics picture for checking a design; it
// carries no amplitude and no phase and is not a field result.
std::vector<RayPath> traceRays(const Scene& scene, const SceneObject& reflector, const Vec3& source, int rayCount,
                               double reflectedLength);

// --- focus metrics ---------------------------------------------------------------------

struct FocusMetrics {
    bool valid = false;
    Vec3 peakPoint = Vec3::Zero();   // world position of the largest |P| on the grid (outside sources)
    double peakMagnitude = 0.0;      // Pa (peak)
    // Full width of the region around the peak where |P| >= peak / 2 (-6 dB),
    // along each sampled grid axis through the peak; 0 when the half-level is
    // not reached inside the grid on both sides.
    double widthU = 0.0, widthV = 0.0, widthW = 0.0;  // m
    // Distance from the peak to the intended point, when one was given.
    std::optional<double> displacement;               // m
    // |P| at the intended point (trilinear sample of the grid), when given.
    std::optional<double> magnitudeAtIntended;        // Pa
    // Largest local maximum of |P| along the U axis through the peak, other
    // than the peak itself, relative to the peak; nullopt when there is none.
    std::optional<double> sidelobeRatio;
};

// Where the field on a sampled grid peaks, how wide the peak is and how far
// it is from where it was intended. Definitions are documented in
// docs/manipulators/PASSIVE_FOCUSING.md.
FocusMetrics analyzeFocus(const acoustics::FieldResult& result, const std::optional<Vec3>& intended = {});

// Focal gain at a point: |P| with the reflectors divided by |P| of the same
// sources without them. Dimensionless; 1 means the reflectors change nothing.
double focalGain(const ReflectorFieldEvaluator& evaluator, const Vec3& point);

// Hybrid focusing: per-element phases that make every element's field -
// direct plus reflected, as the model calculates it - arrive in phase at the
// target: phi_i = -arg H_i, H_i the model's response of element i at the
// target. With a free-field model this is the ordinary phase focus k r_i.
acoustics::FocusSolution computeModelFocus(const std::vector<acoustics::PointSource>& arraySources, const Vec3& target,
                                           const acoustics::Medium& medium, const acoustics::PropagationModel& model);

}  // namespace resonant::manipulators
