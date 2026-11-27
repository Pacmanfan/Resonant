#include "electromagnetics/EmStudy.h"

#include "foundation/Transform.h"
#include "electromagnetics/Lines.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdarg>
#include <cstdio>

namespace resonant::em {
namespace {

std::string format(const char* pattern, ...)
{
    char buffer[700];
    va_list arguments;
    va_start(arguments, pattern);
    std::vsnprintf(buffer, sizeof buffer, pattern, arguments);
    va_end(arguments);
    return buffer;
}

struct Placement {
    Mat4 world, inverse;
    Eigen::Matrix3d rotation;  // columns: local axes in world, unit length
    double axialScale = 1.0, radialScale = 1.0;
};

Placement placementOf(const Scene& scene, const SceneObject& object)
{
    Placement p;
    p.world = scene.worldMatrix(object);
    p.inverse = p.world.inverse();
    for (int c = 0; c < 3; ++c) {
        const Vec3 column = p.world.block<3, 1>(0, c);
        p.rotation.col(c) = column.normalized();
    }
    p.axialScale = p.world.block<3, 1>(0, 2).norm();
    p.radialScale = std::max(p.world.block<3, 1>(0, 0).norm(), p.world.block<3, 1>(0, 1).norm());
    return p;
}

// Conductivity of an antenna's material when it is a good conductor at f; 0 = treated as perfect.
double conductorOf(const Project& project, const SceneObject& object, double f, std::vector<std::string>& notes)
{
    if (object.state.materialId.isNull()) {
        notes.push_back("No material assigned: the conductor is treated as perfect (no ohmic loss).");
        return 0.0;
    }
    const Material* record = project.materials.find(object.state.materialId);
    if (!record)
        return 0.0;
    auto m = emMaterialOf(*record, f);
    if (!m || m.value().classify(f) != MaterialClass::GoodConductor) {
        notes.push_back("The assigned material '" + record->name() + "' is not a good conductor at this frequency; the wire is treated "
                        "as a perfect conductor.");
        return 0.0;
    }
    notes.push_back(format("Conductor loss of '%s' (sigma = %.3g S/m, skin depth %.3g um at %.4g MHz) included.", record->name().c_str(),
                           m.value().conductivity, m.value().skinDepth(f) * 1e6, f / 1e6));
    return m.value().conductivity;
}

double surfaceResistance(double sigma, double f)
{
    return std::sqrt(2.0 * kPi * f * kMu0 / (2.0 * sigma));
}

}  // namespace

Vec3 SampleGrid::point(int u, int v, int w) const
{
    const auto t = [](int i, int n) { return n > 1 ? static_cast<double>(i) / (n - 1) : 0.0; };
    return origin + t(u, nu) * axisU + t(v, nv) * axisV + t(w, nw) * axisW;
}

std::string_view toString(FieldComponent c)
{
    switch (c) {
    case FieldComponent::EMagnitude: return "|E| (V/m, peak)";
    case FieldComponent::Ex: return "Ex (V/m)";
    case FieldComponent::Ey: return "Ey (V/m)";
    case FieldComponent::Ez: return "Ez (V/m)";
    case FieldComponent::HMagnitude: return "|H| (A/m, peak)";
    case FieldComponent::Hx: return "Hx (A/m)";
    case FieldComponent::Hy: return "Hy (A/m)";
    case FieldComponent::Hz: return "Hz (A/m)";
    case FieldComponent::PowerDensity: return "|S| time-averaged power density (W/m^2)";
    }
    return "";
}

std::complex<float> EmFieldResult::component(FieldComponent c, size_t i) const
{
    const auto norm3 = [&](const std::array<std::vector<std::complex<float>>, 3>& f) {
        return std::sqrt(std::norm(f[0][i]) + std::norm(f[1][i]) + std::norm(f[2][i]));
    };
    switch (c) {
    case FieldComponent::EMagnitude: return {norm3(e), 0.0f};
    case FieldComponent::Ex: return e[0][i];
    case FieldComponent::Ey: return e[1][i];
    case FieldComponent::Ez: return e[2][i];
    case FieldComponent::HMagnitude: return hasH ? std::complex<float>(norm3(h), 0.0f) : std::complex<float>(0.0f, 0.0f);
    case FieldComponent::Hx: return hasH ? h[0][i] : std::complex<float>();
    case FieldComponent::Hy: return hasH ? h[1][i] : std::complex<float>();
    case FieldComponent::Hz: return hasH ? h[2][i] : std::complex<float>();
    case FieldComponent::PowerDensity: {
        if (!hasH)
            return {};
        const Eigen::Vector3cf ev(e[0][i], e[1][i], e[2][i]), hv(h[0][i], h[1][i], h[2][i]);
        const Eigen::Vector3f s = 0.5f * (ev.cross(hv.conjugate())).real();
        return {s.norm(), 0.0f};
    }
    }
    return {};
}

double EmFieldResult::largest(FieldComponent c) const
{
    double best = 0.0;
    for (size_t i = 0; i < grid.count(); ++i)
        if (excluded.empty() || !excluded[i])
            best = std::max(best, static_cast<double>(std::abs(component(c, i))));
    return best;
}

// --- settings ---------------------------------------------------------------------------

Status EmSettings::validate() const
{
    if (!(sweepMinHz > 0.0 && sweepMaxHz > sweepMinHz) || sweepPoints < 2 || sweepPoints > 100000)
        return Status::failure("the sweep needs a positive, increasing range and at least 2 points");
    if (!(patternResolutionDeg >= 0.1 && patternResolutionDeg <= 10.0))
        return Status::failure("the pattern resolution must be between 0.1 and 10 degrees");
    if (!(chebyshevSidelobeDb >= 10.0 && chebyshevSidelobeDb <= 100.0))
        return Status::failure("the Chebyshev sidelobe level must be between 10 and 100 dB");
    if (!(regularization >= 0.0 && regularization < 1.0) || !(amplitudeLimit >= 0.0))
        return Status::failure("the regularization must be in [0, 1) and the amplitude limit zero (none) or positive");
    return Status::success();
}

nlohmann::json toJson(const EmSettings& s)
{
    nlohmann::json beams = nlohmann::json::array();
    for (const EmSettings::Beam& b : s.beams)
        beams.push_back({{"thetaDeg", b.thetaDeg}, {"phiDeg", b.phiDeg}, {"levelDb", b.levelDb}, {"null", b.null}});
    return {{"sweepMinHz", s.sweepMinHz}, {"sweepMaxHz", s.sweepMaxHz}, {"sweepPoints", s.sweepPoints},
            {"patternResolutionDeg", s.patternResolutionDeg}, {"steerThetaDeg", s.steerThetaDeg}, {"steerPhiDeg", s.steerPhiDeg},
            {"trueTimeDelay", s.trueTimeDelay}, {"chebyshevSidelobeDb", s.chebyshevSidelobeDb}, {"beams", beams},
            {"regularization", s.regularization}, {"amplitudeLimit", s.amplitudeLimit}};
}

Result<EmSettings> emSettingsFromJson(const nlohmann::json& v)
{
    if (!v.is_object())
        return Result<EmSettings>::failure("electromagnetic settings must be an object");
    EmSettings s;
    try {
        s.sweepMinHz = v.value("sweepMinHz", s.sweepMinHz);
        s.sweepMaxHz = v.value("sweepMaxHz", s.sweepMaxHz);
        s.sweepPoints = v.value("sweepPoints", s.sweepPoints);
        s.patternResolutionDeg = v.value("patternResolutionDeg", s.patternResolutionDeg);
        s.steerThetaDeg = v.value("steerThetaDeg", s.steerThetaDeg);
        s.steerPhiDeg = v.value("steerPhiDeg", s.steerPhiDeg);
        s.trueTimeDelay = v.value("trueTimeDelay", s.trueTimeDelay);
        s.chebyshevSidelobeDb = v.value("chebyshevSidelobeDb", s.chebyshevSidelobeDb);
        for (const auto& b : v.value("beams", nlohmann::json::array()))
            s.beams.push_back({b.value("thetaDeg", 0.0), b.value("phiDeg", 0.0), b.value("levelDb", 0.0), b.value("null", false)});
        s.regularization = v.value("regularization", s.regularization);
        s.amplitudeLimit = v.value("amplitudeLimit", s.amplitudeLimit);
    } catch (const nlohmann::json::exception& error) {
        return Result<EmSettings>::failure(std::string("invalid electromagnetic settings: ") + error.what());
    }
    if (Status status = s.validate(); !status)
        return Result<EmSettings>::failure(status.message());
    return s;
}

EmSettings emSettingsOf(const Project& project)
{
    const auto it = project.extensions.find(kEmSettingsKey);
    if (it == project.extensions.end())
        return EmSettings();
    auto parsed = emSettingsFromJson(*it);
    return parsed ? parsed.take() : EmSettings();
}

void storeEmSettings(Project& project, const EmSettings& settings)
{
    project.extensions[kEmSettingsKey] = toJson(settings);
}

// --- antennas ---------------------------------------------------------------------------

AntennaReport analyzeAntenna(const Project& project, const Id& id)
{
    AntennaReport r;
    const SceneObject* object = project.scene.find(id);
    const auto* antenna = object ? dynamic_cast<const AntennaComponent*>(object->state.component.get()) : nullptr;
    if (!antenna || !object->state.geometry || !isAntennaGeometry(*object->state.geometry)) {
        r.notes.push_back("not an antenna");
        return r;
    }
    const Geometry& g = *object->state.geometry;
    const Placement place = placementOf(project.scene, *object);
    const double f = antenna->settings.frequencyHz, lambda = wavelength(f), k = wavenumber(f);
    const double z0 = antenna->settings.referenceImpedance;
    r.kind = g.kindId();
    r.frequencyHz = f;
    r.wavelength = lambda;
    r.analytical = supportOf(g) == AntennaModel::Analytical;
    r.model = supportDescription(g);
    const auto addMatch = [&] {
        r.matching = match(r.impedance->input, z0);
    };
    if (r.kind == "antenna.dipole" || r.kind == "antenna.monopole") {
        const bool monopole = r.kind == "antenna.monopole";
        const double length = (monopole ? 2.0 * g.parameter("height") : g.parameter("length")) * place.axialScale;
        const double a = g.parameter("wireRadius") * place.radialScale, h = 0.5 * length;
        r.impedance = monopole ? monopoleImpedance(0.5 * length, a, f) : thinDipoleImpedance(length, a, f);
        addMatch();
        const double dDipole = directivityOfAxisymmetric([&](double t) {
            const double v = thinDipolePattern(t, length, f);
            return v * v;
        });
        r.directivity = monopole ? 2.0 * dDipole : dDipole;
        r.polarization = monopole ? "linear, vertical (along the monopole), above the ground plane only" : "linear, along the dipole axis";
        const double sigma = conductorOf(project, *object, f, r.notes);
        r.radiationResistance = r.impedance->input.real();
        if (sigma > 0.0) {
            const double perLength = surfaceResistance(sigma, f) / (2.0 * kPi * a);
            const double s = std::sin(k * h);
            double rl = perLength * (h - std::sin(2.0 * k * h) / (2.0 * k)) / (s * s);
            if (monopole)
                rl *= 0.5;  // only the real half carries current
            r.lossResistance = rl;
            r.radiationEfficiency = r.radiationResistance / (r.radiationResistance + rl);
        }
        r.gain = r.directivity * r.radiationEfficiency;
        r.realizedGain = r.gain * r.matching->mismatchEfficiency;
        const double size = monopole ? length : length;  // the largest dimension of the radiating structure (with its image)
        r.farFieldDistance = std::max({2.0 * size * size / lambda, 3.0 * lambda, 3.0 * size});
        const double resonant = resonantDipoleLength(a, f);
        r.estimates.push_back(format("Half a free-space wavelength: %.4g mm. Thin-wire resonant length (zero input reactance, induced-EMF "
                                     "model, this wire radius): %.4g mm%s.",
                                     0.5 * lambda * 1e3, (monopole ? 0.5 : 1.0) * resonant * 1e3, monopole ? " (monopole height)" : ""));
        r.estimates.push_back(format("Electrical length: %.4g wavelengths%s.", length / lambda, monopole ? " (with its image)" : ""));
        if (!r.impedance->valid)
            r.notes.push_back("Near an antiresonance (length close to a whole wavelength) the feed current of the assumed sinusoidal "
                              "distribution vanishes and the induced-EMF input impedance is meaningless.");
        if (length / lambda > 1.25)
            r.notes.push_back("Longer than about 1.25 wavelengths the assumed sinusoidal current is a poor approximation.");
        r.notes.push_back("The pattern and directivity follow from the assumed current; mutual effects of nearby objects are not included.");
    } else if (r.kind == "antenna.loop") {
        const double b = g.parameter("loopRadius") * place.radialScale, a = g.parameter("wireRadius") * place.radialScale;
        r.impedance = smallLoopImpedance(b, a, f);
        addMatch();
        r.directivity = 1.5;
        r.polarization = "linear, in the plane of the loop (azimuthal)";
        const double sigma = conductorOf(project, *object, f, r.notes);
        r.radiationResistance = r.impedance->input.real();
        if (sigma > 0.0) {
            r.lossResistance = surfaceResistance(sigma, f) * b / a;  // C R' with R' = Rs / (2 pi a)
            r.radiationEfficiency = r.radiationResistance / (r.radiationResistance + r.lossResistance);
        }
        r.gain = r.directivity * r.radiationEfficiency;
        r.realizedGain = r.gain * r.matching->mismatchEfficiency;
        r.farFieldDistance = std::max({2.0 * 4.0 * b * b / lambda, 3.0 * lambda, 6.0 * b});
        const double c = 2.0 * kPi * b;
        r.estimates.push_back(format("Circumference: %.3g wavelengths.", c / lambda));
        if (!r.impedance->valid)
            r.notes.push_back("The loop is not electrically small (circumference above 0.3 wavelengths): the small-loop model does not apply.");
    } else if (r.kind == "antenna.patch") {
        const Material* sub = object->state.materialId.isNull() ? nullptr : project.materials.find(object->state.materialId);
        double er = 1.0;
        if (sub) {
            if (auto m = emMaterialOf(*sub, f))
                er = m.value().relativePermittivity;
        }
        const double w = g.parameter("patchWidth") * place.radialScale, l = g.parameter("patchLength") * place.radialScale;
        const double h = g.parameter("substrateThickness") * place.axialScale;
        const LineParameters line = microstripLine(w, h, er);
        const double ee = line.effectivePermittivity;
        const double dl = 0.412 * h * (ee + 0.3) * (w / h + 0.264) / ((ee - 0.258) * (w / h + 0.8));
        const double fr = kSpeedOfLight / (2.0 * (l + 2.0 * dl) * std::sqrt(ee));
        r.estimates.push_back(format("Resonant frequency of the length (transmission-line model, Hammerstad fringing): %.5g GHz with "
                                     "substrate permittivity %.3g%s.",
                                     fr / 1e9, er, sub ? " of the assigned material" : " (no material: air)"));
        r.estimates.push_back("Typical directivity of a thin rectangular patch: 6 - 8 dBi (not calculated here).");
        r.polarization = "linear, along the resonant length (X)";
    } else if (r.kind == "antenna.horn") {
        const double A = g.parameter("apertureA") * place.radialScale, B = g.parameter("apertureB") * place.radialScale;
        const double ga = g.parameter("guideA") * place.radialScale;
        const double gain = 0.51 * 4.0 * kPi * A * B / (lambda * lambda);
        r.estimates.push_back(format("Gain of an optimum pyramidal horn of this aperture (aperture efficiency 0.51): %.3g dBi.", 10.0 * std::log10(gain)));
        r.estimates.push_back(format("TE10 cut-off of the feed guide: %.4g GHz.", kSpeedOfLight / (2.0 * ga) / 1e9));
        r.polarization = "linear, along the short side of the feed guide (Y)";
        r.farFieldDistance = 2.0 * (A * A + B * B) / lambda;
    } else if (r.kind == "antenna.helix") {
        const double c = kPi * g.parameter("diameter") * place.radialScale, s = g.parameter("pitch") * place.axialScale;
        const double n = g.parameter("turns");
        r.estimates.push_back(format("Circumference %.3g wavelengths: axial mode needs about 0.75 - 1.33.", c / lambda));
        r.estimates.push_back(format("Kraus' empirical directivity 15 (C/lambda)^2 N S/lambda = %.3g dBi (known to overestimate); input "
                                     "resistance about 140 C/lambda = %.3g ohm.",
                                     10.0 * std::log10(15.0 * (c / lambda) * (c / lambda) * n * s / lambda), 140.0 * c / lambda));
        r.polarization = "circular in axial mode (sense from the winding)";
    }
    if (!r.analytical)
        r.notes.push_back("Design estimates only: no field, pattern or impedance is calculated for this antenna kind.");
    return r;
}

std::vector<std::pair<double, AntennaImpedance>> impedanceSweep(const Project& project, const Id& id, double fMin, double fMax, int points)
{
    std::vector<std::pair<double, AntennaImpedance>> out;
    const SceneObject* object = project.scene.find(id);
    if (!object || !object->state.geometry)
        return out;
    const Geometry& g = *object->state.geometry;
    const Placement place = placementOf(project.scene, *object);
    for (int i = 0; i < points; ++i) {
        const double f = fMin + (fMax - fMin) * i / std::max(1, points - 1);
        if (g.kindId() == "antenna.dipole")
            out.emplace_back(f, thinDipoleImpedance(g.parameter("length") * place.axialScale, g.parameter("wireRadius") * place.radialScale, f));
        else if (g.kindId() == "antenna.monopole")
            out.emplace_back(f, monopoleImpedance(g.parameter("height") * place.axialScale, g.parameter("wireRadius") * place.radialScale, f));
        else if (g.kindId() == "antenna.loop")
            out.emplace_back(f, smallLoopImpedance(g.parameter("loopRadius") * place.radialScale, g.parameter("wireRadius") * place.radialScale, f));
    }
    return out;
}

Result<PatternGrid> patternOf(const Project& project, const Id& id, double resolutionDeg)
{
    using Failure = Result<PatternGrid>;
    const SceneObject* object = project.scene.find(id);
    if (!object)
        return Failure::failure("select an antenna or an array");
    PatternGrid p;
    p.stepRad = resolutionDeg * kPi / 180.0;
    p.thetaCount = static_cast<int>(std::lround(180.0 / resolutionDeg)) + 1;
    p.phiCount = static_cast<int>(std::lround(360.0 / resolutionDeg));
    p.magnitude.assign(static_cast<size_t>(p.thetaCount) * p.phiCount, 0.0);
    std::function<double(double, double)> amplitude;
    bool upperHalfSpace = false;  // monopole: no field below the ground plane (theta > 90 deg)
    if (const auto* array = dynamic_cast<const EmArrayComponent*>(object->state.component.get())) {
        const ArrayModel model = arrayModelOf(array->settings);
        const double f = array->settings.frequencyHz;
        amplitude = [model, f](double t, double ph) { return std::abs(farFieldResponse(model, directionOf(t, ph), f)); };
        p.model = "array factor x element pattern (" + std::string(toString(array->settings.pattern)) + "): no mutual coupling";
    } else if (const auto* antenna = dynamic_cast<const AntennaComponent*>(object->state.component.get())) {
        if (!object->state.geometry || supportOf(*object->state.geometry) != AntennaModel::Analytical)
            return Failure::failure("no pattern is calculated for this antenna kind (design estimates only)");
        const Geometry& g = *object->state.geometry;
        const Placement place = placementOf(project.scene, *object);
        const double f = antenna->settings.frequencyHz;
        if (g.kindId() == "antenna.loop") {
            amplitude = [](double t, double) { return std::abs(std::sin(t)); };
            p.model = "small loop (magnetic dipole) pattern sin(theta)";
        } else {
            const bool monopole = g.kindId() == "antenna.monopole";
            const double length = (monopole ? 2.0 * g.parameter("height") : g.parameter("length")) * place.axialScale;
            amplitude = [length, f, monopole](double t, double) { return (monopole && t > 0.5 * kPi) ? 0.0 : std::abs(thinDipolePattern(t, length, f)); };
            p.model = monopole ? "monopole over an infinite ground plane (image theory), field above the plane only"
                               : "thin-wire dipole with sinusoidal current";
            upperHalfSpace = monopole;
        }
    } else {
        return Failure::failure("select an antenna or an array");
    }
    double peak = 0.0, total = 0.0;
    for (int i = 0; i < p.thetaCount; ++i)
        for (int j = 0; j < p.phiCount; ++j) {
            const double t = i * p.stepRad, value = amplitude(t, j * p.stepRad);
            p.magnitude[static_cast<size_t>(i) * p.phiCount + j] = value;
            peak = std::max(peak, value);
            // Exact solid angle of the band around this sample (per radian of phi), clipped to the
            // half-space above the ground plane for a monopole.
            const double upper = std::min(upperHalfSpace ? 0.5 * kPi : kPi, t + 0.5 * p.stepRad);
            const double band = upper > t - 0.5 * p.stepRad ? std::cos(std::max(0.0, t - 0.5 * p.stepRad)) - std::cos(upper) : 0.0;
            total += value * value * band * p.stepRad;
        }
    if (peak <= 0.0)
        return Failure::failure("the pattern is zero everywhere (no active element)");
    p.directivity = 4.0 * kPi * peak * peak / total;
    for (double& v : p.magnitude)
        v /= peak;
    return p;
}

// --- guides ---------------------------------------------------------------------------------

GuideReport analyzeGuide(const Project& project, const Id& id)
{
    GuideReport r;
    const SceneObject* object = project.scene.find(id);
    const auto* guide = object ? dynamic_cast<const GuideComponent*>(object->state.component.get()) : nullptr;
    if (!guide || !object->state.geometry || !isGuideGeometry(*object->state.geometry)) {
        r.notes.push_back("not a waveguide or cavity");
        return r;
    }
    const Geometry& g = *object->state.geometry;
    const Placement place = placementOf(project.scene, *object);
    const double f = guide->settings.frequencyHz;
    r.kind = g.kindId();
    r.frequencyHz = f;
    if (!object->state.materialId.isNull())
        if (const Material* m = project.materials.find(object->state.materialId)) {
            if (auto em = emMaterialOf(*m, f); em && em.value().classify(f) != MaterialClass::GoodConductor) {
                r.relativePermittivity = em.value().relativePermittivity;
                r.relativePermeability = em.value().relativePermeability;
                r.notes.push_back("Filled with '" + m->name() + "' (losses not included in the cut-offs).");
            } else {
                r.notes.push_back("The assigned material is a conductor or has no electromagnetic data; the guide is taken as empty (vacuum).");
            }
        }
    const double er = r.relativePermittivity, mr = r.relativePermeability;
    r.valid = true;
    r.model = "ideal uniform guide with perfectly conducting walls (closed-form modes)";
    if (r.kind == "guide.rectangular") {
        const double a = g.parameter("a") * place.radialScale, b = g.parameter("b") * place.radialScale;
        r.modes = rectangularGuideModes(a, b, 8, er, mr);
    } else if (r.kind == "guide.circular") {
        r.modes = circularGuideModes(g.parameter("radius") * place.radialScale, 8, er);
    } else if (r.kind == "guide.coaxial") {
        const double ri = g.parameter("innerRadius") * place.radialScale, ro = g.parameter("outerRadius") * place.radialScale;
        r.characteristicImpedance = coaxialLine(ri, ro, er).characteristicImpedance;
        r.modes.push_back({ModeKind::TEM, 0, 0, 0.0});
        // TE11 cut-off of a coaxial line: approximately c / (pi (a + b) / 2 sqrt(er)) (Pozar), the first higher mode.
        r.modes.push_back({ModeKind::TE, 1, 1, kSpeedOfLight / (kPi * (ri + ro) * std::sqrt(er * mr))});
        r.notes.push_back("TE11 cut-off of the coaxial line from the approximation k_c = 2 / (a + b) (within a few percent for b / a < 5).");
    } else if (r.kind == "cavity.rectangular.em") {
        r.cavityModes = rectangularCavityModes(g.parameter("a") * place.radialScale, g.parameter("b") * place.radialScale,
                                               g.parameter("d") * place.axialScale, 8, er);
        r.model = "ideal rectangular cavity with perfectly conducting walls (closed-form resonances); no losses, no coupling ports";
    }
    for (const GuideMode& m : r.modes)
        r.propagation.push_back(propagate(m, f, er, mr));
    return r;
}

Result<std::shared_ptr<EmFieldResult>> te10CrossSection(const Project& project, const Id& id, int samples)
{
    using Failure = Result<std::shared_ptr<EmFieldResult>>;
    const SceneObject* object = project.scene.find(id);
    const auto* guide = object ? dynamic_cast<const GuideComponent*>(object->state.component.get()) : nullptr;
    if (!guide || !object->state.geometry || object->state.geometry->kindId() != "guide.rectangular")
        return Failure::failure("the TE10 field is shown for rectangular waveguides");
    const Placement place = placementOf(project.scene, *object);
    const double a = object->state.geometry->parameter("a") * place.radialScale, b = object->state.geometry->parameter("b") * place.radialScale;
    const double f = guide->settings.frequencyHz;
    if (f <= kSpeedOfLight / (2.0 * a))
        return Failure::failure("the frequency is below the TE10 cut-off: the mode does not propagate");
    auto result = std::make_shared<EmFieldResult>();
    result->id = Id::generate();
    result->name = object->state.name + ", TE10 cross-section";
    result->frequencyHz = f;
    // The cross-section x in [-a/2, a/2], y in [-b/2, b/2] at z = 0, in world coordinates.
    result->grid.origin = transformPoint(place.world, Vec3(-0.5 * a / place.radialScale, -0.5 * b / place.radialScale, 0.0));
    result->grid.axisU = place.rotation.col(0) * a;
    result->grid.axisV = place.rotation.col(1) * b;
    result->grid.nu = samples;
    result->grid.nv = std::max(2, samples * static_cast<int>(std::lround(b / a * 100)) / 100);
    for (int c = 0; c < 3; ++c) {
        result->e[static_cast<size_t>(c)].assign(result->grid.count(), {});
        result->h[static_cast<size_t>(c)].assign(result->grid.count(), {});
    }
    for (int v = 0; v < result->grid.nv; ++v)
        for (int u = 0; u < result->grid.nu; ++u) {
            const double x = a * u / (result->grid.nu - 1);
            const TE10Field fld = te10Field(a, x, 0.0, f);
            const Eigen::Vector3cd e = place.rotation.cast<Complex>() * Vec3c(0.0, fld.ey, 0.0);
            const Eigen::Vector3cd h = place.rotation.cast<Complex>() * Vec3c(fld.hx, 0.0, fld.hz);
            const size_t i = result->grid.index(u, v, 0);
            for (int c = 0; c < 3; ++c) {
                result->e[static_cast<size_t>(c)][i] = std::complex<float>(e(c));
                result->h[static_cast<size_t>(c)][i] = std::complex<float>(h(c));
            }
        }
    result->assumptions = {"TE10 mode of an ideal, empty rectangular guide with perfectly conducting walls; peak E0 = 1 V/m; time "
                           "convention e^{+j omega t}."};
    result->metadata.domain = PhysicsDomain::Electromagnetic;
    result->metadata.solverId = "em.analytical.waveguide";
    result->metadata.solverVersion = "1";
    result->metadata.assumptions = result->assumptions;
    return result;
}

// --- fields -------------------------------------------------------------------------------

Result<std::shared_ptr<EmFieldResult>> evaluateFields(const Project& project, const SampleGrid& grid, double f, const std::string& name)
{
    using Failure = Result<std::shared_ptr<EmFieldResult>>;
    auto result = std::make_shared<EmFieldResult>();
    result->id = Id::generate();
    result->name = name;
    result->grid = grid;
    result->frequencyHz = f;
    for (int c = 0; c < 3; ++c) {
        result->e[static_cast<size_t>(c)].assign(grid.count(), {0.0f, 0.0f});
        result->h[static_cast<size_t>(c)].assign(grid.count(), {0.0f, 0.0f});
    }
    result->excluded.assign(grid.count(), 0);
    std::vector<std::string> notes;
    size_t radiators = 0;
    const double omega = 2.0 * kPi * f;
    const auto sameFrequency = [&](double other, const std::string& label) {
        if (std::abs(other - f) <= 1e-9 * f)
            return true;
        notes.push_back("'" + label + "' radiates at another frequency and is left out.");
        return false;
    };
    // Accumulate a local field function over the grid.
    const auto accumulate = [&](const Placement& place, const std::function<FieldPoint(const Vec3&)>& field, const std::function<bool(const Vec3&)>& exclude) {
        for (size_t i = 0; i < grid.count(); ++i) {
            const int u = static_cast<int>(i % static_cast<size_t>(grid.nu));
            const int v = static_cast<int>((i / static_cast<size_t>(grid.nu)) % static_cast<size_t>(grid.nv));
            const int w = static_cast<int>(i / (static_cast<size_t>(grid.nu) * static_cast<size_t>(grid.nv)));
            const Vec3 local = transformPoint(place.inverse, grid.point(u, v, w));
            if (exclude && exclude(local)) {
                result->excluded[i] = 1;
                continue;
            }
            const FieldPoint fp = field(local);
            if (fp.singular) {
                result->excluded[i] = 1;
                continue;
            }
            const Eigen::Vector3cd e = place.rotation.cast<Complex>() * fp.e, h = place.rotation.cast<Complex>() * fp.h;
            for (int c = 0; c < 3; ++c) {
                result->e[static_cast<size_t>(c)][i] += std::complex<float>(e(c));
                result->h[static_cast<size_t>(c)][i] += std::complex<float>(h(c));
            }
        }
    };
    project.scene.forEach([&](const SceneObject& object) {
        if (!project.scene.isEffectivelyVisible(object))
            return;
        const Placement place = placementOf(project.scene, object);
        if (const auto* a = dynamic_cast<const AntennaComponent*>(object.state.component.get())) {
            if (!a->settings.active || !object.state.geometry)
                return;
            const Geometry& g = *object.state.geometry;
            if (supportOf(g) != AntennaModel::Analytical) {
                notes.push_back("'" + object.state.name + "' (" + g.kind().displayName + ") has no field model and is left out.");
                return;
            }
            if (!sameFrequency(a->settings.frequencyHz, object.state.name))
                return;
            const Complex current = std::polar(a->settings.feedCurrent, a->settings.feedPhase - omega * a->settings.delay);
            ++radiators;
            if (g.kindId() == "antenna.loop") {
                const double b = g.parameter("loopRadius") * place.radialScale, wire = g.parameter("wireRadius") * place.radialScale;
                accumulate(place, [&](const Vec3& p) { return smallLoopField(p, current * kPi * b * b, f); },
                           [&](const Vec3& p) { return std::abs(std::hypot(p.x(), p.y()) - b) < 2.0 * wire && std::abs(p.z()) < 2.0 * wire; });
                if (2.0 * kPi * b > 0.3 * wavelength(f))
                    notes.push_back("'" + object.state.name + "' is not electrically small; its small-loop field is inaccurate.");
            } else {
                const bool monopole = g.kindId() == "antenna.monopole";
                const double length = (monopole ? 2.0 * g.parameter("height") : g.parameter("length")) * place.axialScale;
                const double wire = g.parameter("wireRadius") * place.radialScale;
                accumulate(place, [&](const Vec3& p) { return thinDipoleField(p, length, current, f); },
                           [&](const Vec3& p) {
                               return (monopole && p.z() < 0.0) || (std::hypot(p.x(), p.y()) < wire && std::abs(p.z()) <= 0.5 * length);
                           });
                if (monopole)
                    notes.push_back("'" + object.state.name + "': monopole over an infinite ground plane; samples below the plane are excluded.");
            }
        } else if (const auto* s = dynamic_cast<const SourceComponent*>(object.state.component.get())) {
            if (!s->settings.active || !sameFrequency(s->settings.frequencyHz, object.state.name))
                return;
            const Complex amplitude = std::polar(s->settings.amplitude, s->settings.phase - omega * s->settings.delay);
            ++radiators;
            switch (s->settings.kind) {
            case SourceKind::ElectricDipole: accumulate(place, [&](const Vec3& p) { return hertzianDipoleField(p, amplitude, f); }, {}); break;
            case SourceKind::MagneticDipole: accumulate(place, [&](const Vec3& p) { return smallLoopField(p, amplitude, f); }, {}); break;
            case SourceKind::PlaneWave: {
                const double k = wavenumber(f);
                accumulate(place,
                           [&](const Vec3& p) {
                               FieldPoint fp;
                               const Complex phase = amplitude * std::polar(1.0, -k * p.z());
                               fp.e = Vec3c(phase, 0.0, 0.0);
                               fp.h = Vec3c(0.0, phase / kEta0, 0.0);
                               return fp;
                           },
                           {});
                break;
            }
            }
        } else if (const auto* arr = dynamic_cast<const EmArrayComponent*>(object.state.component.get())) {
            if (!arr->settings.active || !sameFrequency(arr->settings.frequencyHz, object.state.name))
                return;
            const ArrayModel model = arrayModelOf(arr->settings);
            if (model.pattern == ElementPattern::Isotropic || model.pattern == ElementPattern::CosineHemisphere) {
                notes.push_back("'" + object.state.name + "' has " + std::string(toString(model.pattern)) +
                                " elements: an element pattern, not a physical source, so it has no near field and is left out. Use "
                                "dipole elements.");
                return;
            }
            ++radiators;
            const double halfWave = 0.5 * wavelength(f);
            for (const ArrayElement& e : model.elements) {
                if (!e.active)
                    continue;
                const Complex current = e.weight * std::polar(1.0, -omega * e.delay);
                const Vec3 offset = e.position;
                if (model.pattern == ElementPattern::HalfWaveDipole)
                    accumulate(place, [&](const Vec3& p) { return thinDipoleField(p - offset, halfWave, current, f); }, {});
                else
                    accumulate(place, [&](const Vec3& p) { return hertzianDipoleField(p - offset, current, f); }, {});
            }
            notes.push_back("'" + object.state.name + "': elements radiate independently (each as an isolated " +
                            std::string(model.pattern == ElementPattern::HalfWaveDipole ? "half-wave dipole with its excitation as feed current"
                                                                                       : "Hertzian dipole with its excitation as current moment") +
                            "); mutual coupling is not included.");
        }
    });
    if (radiators == 0)
        return Failure::failure("no active analytical radiator at " + format("%.6g", f) + " Hz in the scene");
    result->assumptions = {"Analytical fields in free space (vacuum constants), time convention e^{+j omega t}, peak phasors.",
                           "Superposition of independent radiators: no mutual coupling, no scattering by objects in the scene."};
    result->assumptions.insert(result->assumptions.end(), notes.begin(), notes.end());
    result->metadata.domain = PhysicsDomain::Electromagnetic;
    result->metadata.solverId = "em.analytical.fields";
    result->metadata.solverVersion = "1";
    result->metadata.assumptions = result->assumptions;
    return result;
}

}  // namespace resonant::em
