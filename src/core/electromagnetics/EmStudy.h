#pragma once

// From a project to electromagnetic analyses: antenna characteristics,
// patterns, analytical fields on a region, and settings.
#include "electromagnetics/Antennas.h"
#include "electromagnetics/EmComponents.h"
#include "electromagnetics/EmMaterial.h"
#include "electromagnetics/Lines.h"
#include "project/Project.h"
#include "simulation/SimulationResult.h"

#include <array>
#include <complex>
#include <memory>

namespace resonant::em {

inline constexpr const char* kEmSettingsKey = "electromagnetics";

struct EmSettings {
    double sweepMinHz = 0.5e9, sweepMaxHz = 1.5e9;
    int sweepPoints = 201;
    double patternResolutionDeg = 1.0;
    // Steering of the selected array.
    double steerThetaDeg = 0.0, steerPhiDeg = 0.0;
    bool trueTimeDelay = false;
    double chebyshevSidelobeDb = 30.0;
    // Beam synthesis: directions (deg) and wanted relative levels (dB; a null below -200 dB).
    struct Beam {
        double thetaDeg = 0.0, phiDeg = 0.0, levelDb = 0.0;
        bool null = false;
        friend bool operator==(const Beam&, const Beam&) = default;
    };
    std::vector<Beam> beams;
    double regularization = 1e-3;
    double amplitudeLimit = 0.0;  // 0 = none

    Status validate() const;
    friend bool operator==(const EmSettings&, const EmSettings&) = default;
};
nlohmann::json toJson(const EmSettings& settings);
Result<EmSettings> emSettingsFromJson(const nlohmann::json& value);
EmSettings emSettingsOf(const Project& project);
void storeEmSettings(Project& project, const EmSettings& settings);

// --- antennas ---------------------------------------------------------------------------

struct AntennaReport {
    bool analytical = false;
    std::string kind, model;               // geometry kind; what the numbers come from
    double frequencyHz = 0.0, wavelength = 0.0;
    std::optional<AntennaImpedance> impedance;
    std::optional<MatchQuantities> matching;
    double directivity = 0.0;              // linear, of the analytical model; 0 when none
    double radiationResistance = 0.0, lossResistance = 0.0;
    double radiationEfficiency = 1.0;      // conductor loss of the assigned material, if any
    double gain = 0.0, realizedGain = 0.0; // linear
    std::string polarization;
    double farFieldDistance = 0.0;         // m: the largest of 2 D^2 / lambda, 3 lambda... (see estimates)
    std::vector<std::string> estimates;    // design formulas, labelled
    std::vector<std::string> notes;        // assumptions and warnings
};

// The analytical description of an antenna object (geometry kind + component + material).
AntennaReport analyzeAntenna(const Project& project, const Id& antennaId);
// Impedance over a frequency range (analytical kinds only).
std::vector<std::pair<double, AntennaImpedance>> impedanceSweep(const Project& project, const Id& antennaId, double fMin, double fMax, int points);

// Normalized far-field pattern |E| / max over (theta, phi) in the object's
// local frame, for an antenna (analytical kinds) or an array.
struct PatternGrid {
    int thetaCount = 0, phiCount = 0;
    double stepRad = 0.0;
    std::vector<double> magnitude;  // [i * phiCount + j], normalized to 1
    double directivity = 0.0;
    std::string model;
    double value(int i, int j) const { return magnitude[static_cast<size_t>(i) * static_cast<size_t>(phiCount) + static_cast<size_t>(j)]; }
};
Result<PatternGrid> patternOf(const Project& project, const Id& objectId, double resolutionDeg);

// --- waveguides and cavities ----------------------------------------------------------------

struct GuideReport {
    bool valid = false;
    std::string kind, model;
    double frequencyHz = 0.0;
    double relativePermittivity = 1.0, relativePermeability = 1.0;  // filling
    std::vector<GuideMode> modes;                 // guides: lowest modes, ascending cutoff
    std::vector<ModePropagation> propagation;     // at the frequency, per mode
    std::vector<CavityMode> cavityModes;          // cavities
    std::optional<double> characteristicImpedance;  // coax (TEM)
    std::vector<std::string> notes;
};
GuideReport analyzeGuide(const Project& project, const Id& guideId);

// --- fields on a region -------------------------------------------------------------------

struct SampleGrid {
    Vec3 origin = Vec3::Zero(), axisU = Vec3::UnitX(), axisV = Vec3::UnitY(), axisW = Vec3::Zero();
    int nu = 2, nv = 2, nw = 1;
    size_t count() const { return static_cast<size_t>(nu) * nv * nw; }
    size_t index(int u, int v, int w) const { return static_cast<size_t>(u) + static_cast<size_t>(nu) * (static_cast<size_t>(v) + static_cast<size_t>(nv) * w); }
    Vec3 point(int u, int v, int w) const;
};

enum class FieldComponent { EMagnitude, Ex, Ey, Ez, HMagnitude, Hx, Hy, Hz, PowerDensity };
std::string_view toString(FieldComponent component);

class EmFieldResult final : public SimulationResult {
public:
    std::string kind() const override { return "em.field"; }
    SampleGrid grid;
    double frequencyHz = 0.0;
    std::array<std::vector<std::complex<float>>, 3> e, h;  // peak phasors, V/m and A/m
    bool hasH = true;
    std::vector<std::uint8_t> excluded;  // on a source, behind a ground plane, inside a conductor
    std::vector<std::string> assumptions;
    // Complex value for display (magnitudes and power density are real).
    std::complex<float> component(FieldComponent c, size_t index) const;
    double largest(FieldComponent c) const;
};

// The fields of every active analytical radiator of the scene (antennas of
// analytical kinds, sources, arrays with dipole elements) on a grid, at one
// frequency (radiators at other frequencies are left out and reported).
Result<std::shared_ptr<EmFieldResult>> evaluateFields(const Project& project, const SampleGrid& grid, double frequencyHz, const std::string& name);

// The TE10 field of a rectangular guide object on its cross-section at z = 0
// (local frame), peak E0 = 1 V/m, at its frequency of interest; refused below cutoff.
Result<std::shared_ptr<EmFieldResult>> te10CrossSection(const Project& project, const Id& guideId, int samples = 41);

}  // namespace resonant::em
