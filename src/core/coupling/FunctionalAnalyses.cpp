#include "coupling/FunctionalAnalyses.h"

#include "coupling/CoupledRun.h"
#include "coupling/MaterialStateAnalyses.h"
#include "coupling/MetamaterialAnalyses.h"
#include "coupling/ResonanceAnalyses.h"
#include "dsp/Resonance.h"
#include "electromagnetics/CavityResonator.h"
#include "electromagnetics/Conventions.h"
#include "electromagnetics/EmMaterial.h"
#include "electromagnetics/LayeredMedia.h"
#include "electromagnetics/OpticalResonator.h"
#include "electromagnetics/PeriodicLayers.h"
#include "functional/Dependencies.h"
#include "manipulators/Horn.h"
#include "materials/Material.h"
#include "mechanics/Bloch.h"
#include "mechanics/ElasticMaterial.h"
#include "mechanics/MechanicalOptimization.h"
#include "mechanics/Study.h"
#include "multiphysics/PhysicalSystem.h"
#include "optimization/Evaluator.h"
#include "project/Project.h"
#include "simulation/Job.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace resonant::functional {

AnalysisRegistry& AnalysisRegistry::instance()
{
    static AnalysisRegistry registry = [] {
        AnalysisRegistry r;
        coupling::registerFunctionalAnalyses(r);
        return r;
    }();
    return registry;
}

}  // namespace resonant::functional

namespace resonant::coupling {

using namespace functional;
using nlohmann::json;
using R = Result<AnalysisOutcome>;

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr double kReferenceTemperature = 293.15;

const SceneObject* bodyObject(const FunctionalStructure& s, const Project& p, const std::string& role)
{
    const Body* b = s.body(role);
    return b ? p.scene.find(Id(b->objectId)) : nullptr;
}

// The material of a region role, or else the material assigned to the body of that role.
const Material* materialFor(const FunctionalStructure& s, const Project& p, const std::string& role)
{
    if (const Region* r = s.region(role))
        return p.materials.find(Id(r->materialId));
    if (const SceneObject* o = bodyObject(s, p, role); o && !o->state.materialId.isNull())
        return p.materials.find(o->state.materialId);
    return nullptr;
}

// A setting given as a number, or as the id of a structure parameter.
double setting(const json& where, const char* key, const Values& parameters, double fallback)
{
    const auto it = where.find(key);
    if (it == where.end() || it->is_null())
        return fallback;
    if (it->is_number())
        return it->get<double>();
    if (it->is_string()) {
        const auto p = parameters.find(it->get<std::string>());
        if (p != parameters.end())
            return p->second;
        throw std::runtime_error(std::string("the setting '") + key + "' names the parameter '" + it->get<std::string>() + "', which does not exist");
    }
    throw std::runtime_error(std::string("the setting '") + key + "' must be a number or a parameter id");
}

std::vector<double> logSpace(double lo, double hi, int n)
{
    std::vector<double> v;
    for (int i = 0; i < n; ++i)
        v.push_back(lo * std::pow(hi / lo, n > 1 ? static_cast<double>(i) / (n - 1) : 0.0));
    return v;
}

std::vector<double> linSpace(double lo, double hi, int n)
{
    std::vector<double> v;
    for (int i = 0; i < n; ++i)
        v.push_back(lo + (hi - lo) * (n > 1 ? static_cast<double>(i) / (n - 1) : 0.0));
    return v;
}

double expansionOf(const Material* m)
{
    if (!m)
        return 0.0;
    const PropertyValue* v = m->property(PhysicsDomain::Thermal, "thermalExpansion");
    return v ? v->value : 0.0;
}

Curve curve(std::string id, std::string title, std::string xLabel, std::string xUnit, std::string yLabel, std::string yUnit)
{
    Curve c;
    c.id = std::move(id);
    c.title = std::move(title);
    c.xLabel = std::move(xLabel);
    c.xUnit = std::move(xUnit);
    c.yLabel = std::move(yLabel);
    c.yUnit = std::move(yUnit);
    return c;
}

ModelInfo model(std::string id, std::string name, std::string version, std::string fidelity, std::vector<std::string> assumptions, std::string validity,
                double seconds)
{
    return {std::move(id), std::move(name), std::move(version), std::move(fidelity), std::move(assumptions), std::move(validity), seconds};
}

// --- microwave cavity ----------------------------------------------------------------------

Result<AnalysisOutcome> evaluateCavity(const FunctionalStructure& s, const Project& p, const Values& parameters, const ModelInfo& m, JobContext* context)
{
    const SceneObject* cavity = bodyObject(s, p, "cavity");
    if (!cavity || !cavity->state.geometry || cavity->state.geometry->kindId() != "cavity.rectangular.em")
        return R::failure("the body 'cavity' must be a rectangular electromagnetic cavity (cavity.rectangular.em)");
    const Geometry& g = *cavity->state.geometry;
    em::CavityResonatorSpec spec;
    spec.a = g.parameter("a");
    spec.b = g.parameter("b");
    spec.d = g.parameter("d");
    AnalysisOutcome out;
    if (cavity->state.local.scale != Vec3::Ones())
        out.warnings.push_back("the cavity object is scaled; the analysis uses its geometry parameters (unscaled)");
    const double guess = em::rectangularCavityModes(spec.a, spec.b, spec.d, 1, 1.0)[0].frequencyHz;
    // Walls: the conductor's conductivity; none: perfect walls.
    const Material* walls = materialFor(s, p, "walls");
    if (!walls)
        return R::failure("the analysis needs the material of the region 'walls'");
    auto wallEm = em::emMaterialOf(*walls, guess);
    if (!wallEm)
        return R::failure("the wall material '" + walls->name() + "': " + wallEm.message());
    spec.wallConductivity = wallEm.value().conductivity;
    spec.wallPermeability = wallEm.value().relativePermeability;
    if (!(spec.wallConductivity > 0.0))
        out.warnings.push_back("the wall material has no conductivity: the walls are treated as perfect conductors (no wall loss)");
    if (const Material* fill = materialFor(s, p, "fill")) {
        auto fe = em::emMaterialOf(*fill, guess / std::sqrt(2.0));
        if (!fe)
            return R::failure("the fill material '" + fill->name() + "': " + fe.message());
        spec.fillPermittivity = std::max(1.0, fe.value().relativePermittivity);
        spec.fillPermeability = std::max(1.0, fe.value().relativePermeability);
        spec.fillLossTangent = fe.value().lossTangent;
        spec.fillConductivity = fe.value().conductivity;
        out.details["fill"] = fill->name();
    }
    out.details["walls"] = walls->name();
    // Operating temperature: the walls expand uniformly (one-way; no feedback modeled).
    const double temperature = setting(s.environment, "temperature", parameters, kReferenceTemperature);
    const double alpha = expansionOf(walls);
    em::CavityResonatorSpec cold = spec;
    if (temperature != kReferenceTemperature) {
        const double scale = 1.0 + alpha * (temperature - kReferenceTemperature);
        spec.a *= scale;
        spec.b *= scale;
        spec.d *= scale;
        if (alpha == 0.0)
            out.warnings.push_back("the wall material has no thermal expansion coefficient: the temperature does not change the dimensions");
    }
    const Boundary* port = s.boundary("port");
    double beta = 0.0;
    if (port) {
        spec.externalQ = setting(port->settings, "externalQ", parameters, 0.0);
        beta = setting(port->settings, "coupling", parameters, 0.0);
    }
    const int count = std::clamp(s.analysis.settings.value("modes", 6), 1, 40);
    auto modes = em::analyzeRectangularCavity(spec, count);
    if (!modes)
        return R::failure(modes.message());
    if (beta > 0.0 && std::isfinite(modes.value()[0].unloadedQ)) {
        spec.externalQ = modes.value()[0].unloadedQ / beta;
        modes = em::analyzeRectangularCavity(spec, count);
    }
    const std::vector<em::CavityResonance>& list = modes.value();
    const em::CavityResonance& first = list[0];
    out.behaviour = "resonant";
    out.solver = m.id;
    out.assumptions = m.assumptions;
    for (const auto& r : list) {
        Mode mode;
        mode.label = r.label;
        mode.frequencyHz = r.frequencyHz;
        mode.quality = r.unloadedQ;
        mode.details = {{"wallQ", std::isfinite(r.wallQ) ? json(r.wallQ) : json("inf")},
                        {"fillQ", std::isfinite(r.fillQ) ? json(r.fillQ) : json("inf")},
                        {"electricEnergyJ", r.electricEnergy},
                        {"magneticEnergyJ", r.magneticEnergy}};
        mode.nature = "natural";
        mode.lossQ = {{"wall conduction", r.wallQ}, {"fill (dielectric and conduction)", r.fillQ}};
        if (r.coupling > 0.0) {
            mode.lossQ["port"] = r.externalQ;
            mode.quality = r.loadedQ;
        }
        mode.bandwidthHz = r.bandwidthHz;
        mode.decayRate = std::isfinite(r.loadedQ) ? 2.0 * em::kPi * r.frequencyHz / (2.0 * r.loadedQ) : 0.0;
        out.modes.push_back(mode);
    }
    Values& x = out.metrics;
    x["cavity.f1"] = first.frequencyHz;
    if (list.size() > 1) {
        x["cavity.f2"] = list[1].frequencyHz;
        x["cavity.modeSeparation"] = list[1].frequencyHz - first.frequencyHz;
    }
    if (list.size() > 2)
        x["cavity.f3"] = list[2].frequencyHz;
    x["cavity.unloadedQ"] = first.unloadedQ;
    x["cavity.wallQ"] = first.wallQ;
    x["cavity.fillQ"] = first.fillQ;
    x["cavity.loadedQ"] = first.loadedQ;
    x["cavity.bandwidth"] = first.bandwidthHz;
    x["cavity.storedEnergy"] = first.storedEnergy;
    x["cavity.dfda"] = em::resonanceSensitivity(spec, first, 0);
    x["cavity.dfdb"] = em::resonanceSensitivity(spec, first, 1);
    x["cavity.dfdd"] = em::resonanceSensitivity(spec, first, 2);
    if (temperature != kReferenceTemperature) {
        auto ref = em::analyzeRectangularCavity(cold, 1);
        x["cavity.thermalShift"] = first.frequencyHz - ref.value()[0].frequencyHz;
    } else {
        x["cavity.thermalShift"] = 0.0;
    }
    if (first.coupling > 0.0) {
        x["cavity.coupling"] = first.coupling;
        // The fraction of the energy lost that leaves through the port: the useful extraction, which
        // grows with the coupling while the loaded Q falls (a tradeoff, not an optimum).
        x["cavity.extractionEfficiency"] = first.coupling / (1.0 + first.coupling);
        x["cavity.portReflection"] = std::abs(em::portReflection(first, first.frequencyHz));
        Curve c = curve("port", "Port reflection near " + first.label, "Frequency", "Hz", "|S11|", "");
        for (double f : linSpace(first.frequencyHz - 5 * first.bandwidthHz, first.frequencyHz + 5 * first.bandwidthHz, 201)) {
            c.x.push_back(f);
            c.y.push_back(std::abs(em::portReflection(first, f)));
        }
        out.curves.push_back(std::move(c));
    }
    // The field of the first mode across the cavity (x at mid y, mid z).
    Curve field = curve("field", "|E| of " + first.label + " along a (normalized to a peak of 1 V/m)", "x", "m", "|E|", "V/m");
    for (double xx : linSpace(0.0, spec.a, 61)) {
        const auto f = em::cavityModeField(spec, first, Vec3(xx, 0.5 * spec.b, 0.5 * spec.d));
        field.x.push_back(xx);
        field.y.push_back(std::sqrt(std::norm(f.e[0]) + std::norm(f.e[1]) + std::norm(f.e[2])));
    }
    out.curves.push_back(std::move(field));
    out.discretization = {{"model", "closed form"}, {"dimensions", {spec.a, spec.b, spec.d}}};
    if (m.id == em::kCavityYeeModelId) {
        em::CavityYeeSettings ys;
        const json yee = s.analysis.settings.value("yee", json::object());
        ys.cellsAcrossSmallest = yee.value("cellsAcrossSmallest", 8);
        ys.durationPeriods = yee.value("durationPeriods", 150.0);
        auto run = em::simulateRectangularCavity(spec, ys, context);
        if (!run)
            return R::failure(run.message());
        const em::CavityYeeResult& y = run.value();
        // Each analytical mode's nearest numerical resonance.
        out.modes.clear();
        int index = 1;
        for (const auto& r : list) {
            double nearest = 0.0;
            for (double f : y.resonancesHz)
                if (nearest == 0.0 || std::abs(f - r.frequencyHz) < std::abs(nearest - r.frequencyHz))
                    nearest = f;
            if (nearest == 0.0 || std::abs(nearest / r.frequencyHz - 1.0) > 0.05)
                continue;
            out.modes.push_back({r.label, nearest, 0.0, {{"analyticalHz", r.frequencyHz}, {"deviation", nearest / r.frequencyHz - 1.0}}});
            if (index <= 3)
                x["cavity.f" + std::to_string(index)] = nearest;
            ++index;
        }
        if (out.modes.empty())
            return R::failure("the full-wave run found no resonance near the analytical ones");
        x["cavity.f1Deviation"] = out.modes[0].frequencyHz / first.frequencyHz - 1.0;
        for (const char* k : {"cavity.unloadedQ", "cavity.wallQ", "cavity.loadedQ", "cavity.bandwidth", "cavity.coupling", "cavity.portReflection"})
            x.erase(k);
        x["cavity.fillQ"] = y.energyDecayRate > 0.0 ? 2.0 * em::kPi * out.modes[0].frequencyHz / y.energyDecayRate : kInf;
        x["cavity.decayRate"] = y.energyDecayRate;
        // Ring-down identification: the complex poles of the probe record after the pulse.
        dsp::PencilOptions po;
        po.minHz = 0.5 * first.frequencyHz;
        po.maxHz = 1.15 * list.back().frequencyHz;
        po.skipSamples = y.pulseSteps;
        po.singularValueThreshold = 1e-4;
        po.maxModes = 40;  // a broadband pulse excites many modes: each needs its pair of poles
        if (auto id = dsp::identifyByMatrixPencil(y.probeSignal, y.dt, po)) {
            for (const auto& r : id.value().resonances) {
                Mode identified;
                identified.nature = "identified";
                identified.frequencyHz = r.frequencyHz;
                identified.quality = r.quality;
                identified.decayRate = r.decayRate;
                identified.bandwidthHz = r.bandwidthHz;
                identified.details = {{"method", r.method}, {"amplitude", r.amplitude}, {"frequencyUncertaintyHz", r.frequencyUncertainty},
                                      {"qualityUncertainty", r.qualityUncertainty}, {"overlapping", r.overlapping}};
                // Label it by the nearest analytical mode within 3 %.
                for (const auto& a : list)
                    if (std::abs(r.frequencyHz / a.frequencyHz - 1.0) < 0.03)
                        identified.label = a.label + " (ring-down)";
                out.modes.push_back(identified);
            }
            const double target = out.modes[0].frequencyHz;
            const auto best = std::min_element(id.value().resonances.begin(), id.value().resonances.end(), [target](const auto& u, const auto& v) {
                return std::abs(u.frequencyHz - target) < std::abs(v.frequencyHz - target);
            });
            if (best != id.value().resonances.end()) {
                x["cavity.ringDownF1"] = best->frequencyHz;
                x["cavity.ringDownQ"] = best->quality;
            }
            for (const std::string& w : id.value().warnings)
                out.warnings.push_back("ring-down: " + w);
        } else {
            out.warnings.push_back("ring-down identification: " + id.message());
        }
        out.curves.erase(std::remove_if(out.curves.begin(), out.curves.end(), [](const Curve& c) { return c.id == "port"; }), out.curves.end());
        out.discretization = {{"model", "Yee grid"}, {"cell", y.cell}, {"cells", y.cells}, {"simulatedSize", y.simulatedSize}, {"steps", y.steps},
                              {"dt", y.dt}, {"backend", y.backend}};
        for (const std::string& n : y.notes)
            out.warnings.push_back(n);
    }
    return out;
}

// --- two-mirror optical resonator ------------------------------------------------------------

Result<AnalysisOutcome> evaluateOptical(const FunctionalStructure& s, const Project& p, const Values& parameters, const ModelInfo& m, JobContext*)
{
    const SceneObject* m1 = bodyObject(s, p, "mirror1");
    const SceneObject* m2 = bodyObject(s, p, "mirror2");
    if (!m1 || !m2)
        return R::failure("the analysis needs the bodies 'mirror1' and 'mirror2'");
    em::OpticalResonatorSpec spec;
    const auto mirror = [](const SceneObject& o, double& radius, double& aperture) -> Status {
        if (!o.state.geometry)
            return Status::failure("'" + o.state.name + "' has no geometry");
        const Geometry& g = *o.state.geometry;
        if (g.kindId() == "reflector.spherical") {
            radius = g.parameter("curvatureRadius");
            aperture = 0.5 * g.parameter("aperture");
        } else if (g.kindId() == "reflector.flat") {
            radius = kInf;
            aperture = 0.5 * std::min(g.parameter("width"), g.parameter("height"));
        } else {
            return Status::failure("'" + o.state.name + "' must be a spherical or flat reflector");
        }
        return Status::success();
    };
    if (Status st = mirror(*m1, spec.radius1, spec.aperture1); !st)
        return R::failure(st.message());
    if (Status st = mirror(*m2, spec.radius2, spec.aperture2); !st)
        return R::failure(st.message());
    const Mat4 w1 = p.scene.worldMatrix(*m1), w2 = p.scene.worldMatrix(*m2);
    spec.length = (w2.block<3, 1>(0, 3) - w1.block<3, 1>(0, 3)).norm();
    spec.wavelength = setting(s.excitation, "wavelength", parameters, 1064e-9);
    spec.refractiveIndex = setting(s.environment, "refractiveIndex", parameters, 1.0);
    spec.reflectivity1 = setting(s.analysis.settings, "reflectivity1", parameters, 0.99);
    spec.reflectivity2 = setting(s.analysis.settings, "reflectivity2", parameters, 0.99);
    spec.extraRoundTripLoss = setting(s.analysis.settings, "extraLoss", parameters, 0.0);
    if (!s.analysis.settings.value("clipping", true))
        spec.aperture1 = spec.aperture2 = 0.0;
    auto r = em::analyzeOpticalResonator(spec);
    if (!r)
        return R::failure(r.message());
    const auto& o = r.value();
    AnalysisOutcome out;
    out.solver = m.id;
    out.assumptions = o.assumptions;
    out.warnings = o.warnings;
    out.assumptions.push_back("the mirrors face each other along the line between the two objects (orientation is not checked)");
    Values& x = out.metrics;
    x["optics.separation"] = spec.length;
    x["optics.g1"] = o.g1;
    x["optics.g2"] = o.g2;
    x["optics.g1g2"] = o.g1 * o.g2;
    x["optics.stable"] = o.stability == em::ResonatorStability::Stable ? 1.0 : 0.0;
    x["optics.fsr"] = o.freeSpectralRange;
    x["optics.finesse"] = o.finesse;
    x["optics.linewidth"] = o.linewidth;
    x["optics.Q"] = o.qualityFactor;
    x["optics.photonLifetime"] = o.photonLifetime;
    x["optics.roundTripLoss"] = o.roundTripLoss;
    x["optics.transmission"] = o.resonantTransmission;
    x["optics.storedEnergyPerWatt"] = o.storedEnergyPerWatt;
    if (o.waist > 0.0) {
        x["optics.waist"] = o.waist;
        x["optics.waistPosition"] = o.waistPosition;
        x["optics.spot1"] = o.spot1;
        x["optics.spot2"] = o.spot2;
        x["optics.rayleighRange"] = o.rayleighRange;
        x["optics.transverseSpacing"] = o.transverseModeSpacing;
        x["optics.clipping1"] = o.clippingLoss1;
        x["optics.clipping2"] = o.clippingLoss2;
        for (long q = o.longitudinalIndex; q <= o.longitudinalIndex + 1; ++q)
            for (int order = 0; order <= 2; ++order)
                out.modes.push_back({"TEM" + std::to_string(order) + "0 q=" + std::to_string(q), em::opticalModeFrequency(spec, o, q, order, 0),
                                     o.qualityFactor, {{"q", q}, {"m", order}, {"n", 0}, {"degeneracy", order + 1}}});
        Curve beam = curve("beam", "Beam radius between the mirrors", "z from mirror 1", "m", "w(z)", "m");
        for (double z : linSpace(0.0, spec.length, 101)) {
            beam.x.push_back(z);
            beam.y.push_back(em::beamRadiusAt(o, z));
        }
        out.curves.push_back(std::move(beam));
        out.behaviour = "resonant";
    } else {
        out.behaviour = o.stability == em::ResonatorStability::Unstable ? "non-resonant" : "resonant";
    }
    out.details = {{"stability", std::string(em::toString(o.stability))}, {"longitudinalIndex", o.longitudinalIndex},
                   {"nearestTem00Hz", o.nearestTem00Frequency}, {"radii", {spec.radius1, spec.radius2}}};
    out.discretization = {{"model", "closed form (ABCD / Gaussian beams)"}};
    return out;
}

// --- acoustic horn ------------------------------------------------------------------------------

Result<AnalysisOutcome> evaluateHorn(const FunctionalStructure& s, const Project& p, const Values& parameters, const ModelInfo& m, JobContext* context)
{
    const SceneObject* horn = bodyObject(s, p, "horn");
    if (!horn || !horn->state.geometry || horn->state.geometry->kindId() != "horn.acoustic")
        return R::failure("the body 'horn' must be an acoustic horn (horn.acoustic)");
    manipulators::HornSpec spec = manipulators::hornSpecOf(*horn->state.geometry);
    spec.density = setting(s.environment, "density", parameters, 1.204);
    spec.soundSpeed = setting(s.environment, "soundSpeed", parameters, 343.0);
    spec.steps = std::clamp(s.analysis.settings.value("steps", 400), 10, 200000);
    const double cutoff = manipulators::hornCutoffHz(spec);
    const double f = setting(s.excitation, "frequencyHz", parameters, 1000.0);
    const double u0 = setting(s.excitation, "throatVelocity", parameters, 0.01);
    auto at = manipulators::analyzeHorn(spec, f, u0, 41);
    if (!at)
        return R::failure(at.message());
    const auto& r = at.value();
    AnalysisOutcome out;
    out.solver = m.id;
    out.assumptions = m.assumptions;
    Values& x = out.metrics;
    x["horn.cutoff"] = cutoff;
    x["horn.throatResistance"] = r.normalizedThroatImpedance.real();
    x["horn.throatReactance"] = r.normalizedThroatImpedance.imag();
    x["horn.reflection"] = std::abs(r.throatReflection);
    x["horn.transmission"] = r.powerTransmission;
    x["horn.radiatedPower"] = r.radiatedPower;
    x["horn.powerBalance"] = r.radiatedPower > 0.0 ? r.mouthPower / r.radiatedPower - 1.0 : 0.0;
    x["horn.onAxisPressure1m"] = r.onAxisPressureAt1m;
    x["horn.directivityIndex"] = r.directivityIndexDb;
    x["horn.beamHalfAngle"] = r.halfAngleMinus6dB;
    x["horn.mouthKa"] = 2.0 * em::kPi * f / spec.soundSpeed * spec.mouthRadius;
    // The response over the frequency range.
    const double lo = s.frequencyHigh > 0.0 ? std::max(1.0, s.frequencyLow) : (cutoff > 0.0 ? 0.5 * cutoff : 50.0);
    const double hi = s.frequencyHigh > 0.0 ? s.frequencyHigh : (cutoff > 0.0 ? 20.0 * cutoff : 10000.0);
    Curve resistance = curve("resistance", "Normalized throat resistance", "Frequency", "Hz", "Re(Z) S/(rho c)", "");
    Curve transmission = curve("transmission", "Power transmission into the horn", "Frequency", "Hz", "1 - |reflection|^2", "");
    double sum = 0.0;
    int above = 0, peaks = 0;
    std::vector<double> res;
    for (double fr : logSpace(lo, hi, 80)) {
        if (context && context->cancelRequested())
            return R::failure("cancelled");
        auto rr = manipulators::analyzeHorn(spec, fr, u0, 2);
        if (!rr)
            return R::failure(rr.message());
        resistance.x.push_back(fr);
        resistance.y.push_back(rr.value().normalizedThroatImpedance.real());
        transmission.x.push_back(fr);
        transmission.y.push_back(rr.value().powerTransmission);
        res.push_back(rr.value().normalizedThroatImpedance.real());
        if (fr >= cutoff) {
            sum += rr.value().powerTransmission;
            ++above;
        }
    }
    for (size_t i = 1; i + 1 < res.size(); ++i)
        if (res[i] > res[i - 1] && res[i] > res[i + 1] && res[i] > 1.3 * std::min(res[i - 1], res[i + 1]))
            ++peaks;
    x["horn.meanTransmission"] = above > 0 ? sum / above : 0.0;
    x["horn.resonancePeaks"] = peaks;
    out.behaviour = peaks > 0 ? "mixed" : "broadband";
    if (peaks > 0)
        out.warnings.push_back("mouth reflections produce " + std::to_string(peaks) + " resonance peak(s) of the throat resistance in the range");
    out.curves.push_back(std::move(resistance));
    out.curves.push_back(std::move(transmission));
    Curve pressure = curve("pressure", "|p| along the axis at " + std::to_string(static_cast<int>(f)) + " Hz", "x from the throat", "m", "|p|", "Pa");
    Curve intensity = curve("intensity", "Intensity along the axis", "x from the throat", "m", "I", "W/m^2");
    for (const auto& sample : r.samples) {
        pressure.x.push_back(sample.x);
        pressure.y.push_back(std::abs(sample.pressure));
        intensity.x.push_back(sample.x);
        intensity.y.push_back(sample.intensity);
    }
    out.curves.push_back(std::move(pressure));
    out.curves.push_back(std::move(intensity));
    const double kaLocal = 2.0 * em::kPi * f / spec.soundSpeed * spec.mouthRadius;
    if (kaLocal > 1.84)
        out.warnings.push_back("ka of the mouth is " + std::to_string(kaLocal).substr(0, 5) +
                               ": above the first cross mode the plane-wave (Webster) model is approximate near the mouth");
    out.details = {{"profile", std::string(manipulators::toString(spec.profile))}, {"frequencyHz", f}, {"throatVelocity", u0}};
    out.discretization = {{"model", "Webster, fourth-order Runge-Kutta"}, {"steps", spec.steps}};
    return out;
}

// --- mechanical resonator -----------------------------------------------------------------------

Result<AnalysisOutcome> evaluateMechanical(const FunctionalStructure& s, const Project& p, const Values&, const ModelInfo& m, JobContext* context)
{
    const SceneObject* body = bodyObject(s, p, "resonator");
    if (!body)
        return R::failure("the analysis needs the body 'resonator' (an object with a structure)");
    const int count = std::clamp(s.analysis.settings.value("modes", 6), 1, 40);
    AnalysisOutcome out;
    out.solver = m.id;
    Values& x = out.metrics;
    auto analytical = mechanics::analyticalFrequencies(p, body->id(), count);
    if (m.id == "mechanics.modal.analytical") {
        if (!analytical)
            return R::failure("no closed form for this structure: " + analytical.message());
        for (size_t i = 0; i < analytical.value().size(); ++i) {
            out.modes.push_back({"", analytical.value()[i], 0.0, {}});
            if (i < 3)
                x["mech.f" + std::to_string(i + 1)] = analytical.value()[i];
        }
        out.behaviour = "resonant";
        out.assumptions = m.assumptions;
        out.discretization = {{"model", "closed form"}};
        return out;
    }
    // Finite elements: the prepared study (mesh) is reused while the object's setup and the settings are unchanged.
    const json key{{"object", dependenciesOf(s, p, {}).objects}, {"material", body->state.materialId.str()}, {"modes", count}};
    const std::string cacheKey = "mechanics.study." + optimization::contentKey(key);
    auto cached = std::static_pointer_cast<const mechanics::StructureStudy>(SimulationGeometryCache::shared().find(cacheKey));
    const bool reused = cached != nullptr;
    if (!cached) {
        auto study = std::make_shared<mechanics::StructureStudy>(mechanics::prepareStudy(p, body->id()));
        if (!study->allowed)
            return R::failure(study->refusal);
        cached = study;
        SimulationGeometryCache::shared().store(cacheKey, cached);
    }
    mechanics::MechanicsSettings settings = mechanics::mechanicsSettingsOf(p);
    settings.modeCount = count;
    auto result = mechanics::runModal(cached, settings, context);
    if (!result)
        return R::failure(result.message());
    const auto& modal = *result.value()->modal;
    int index = 0;
    for (const auto& mode : modal.modes) {
        if (mode.frequencyHz < 1e-3) {
            out.warnings.push_back("a rigid-body mode (free structure) was skipped");
            continue;
        }
        ++index;
        out.modes.push_back({"", mode.frequencyHz, 0.0,
                             {{"effectiveMassFraction", {mode.effectiveMassFraction.x(), mode.effectiveMassFraction.y(), mode.effectiveMassFraction.z()}}}});
        if (index <= 3)
            x["mech.f" + std::to_string(index)] = mode.frequencyHz;
        if (index == 1)
            x["mech.massFractionZ1"] = mode.effectiveMassFraction.z();
    }
    if (index == 0)
        return R::failure("the modal analysis found no elastic mode");
    x["mech.totalMass"] = modal.totalMass.maxCoeff();
    if (analytical && !analytical.value().empty()) {
        x["mech.analyticalF1"] = analytical.value()[0];
        x["mech.f1Deviation"] = x["mech.f1"] / analytical.value()[0] - 1.0;
    }
    out.behaviour = "resonant";
    out.assumptions = cached->assumptions;
    for (const std::string& w : cached->warnings)
        out.warnings.push_back(w);
    for (const std::string& w : result.value()->warnings)
        out.warnings.push_back(w);
    out.discretization = {{"model", "finite elements"}, {"grid", cached->grid}, {"elementSize", cached->elementSize}, {"freeDofs", cached->freeDofs},
                          {"reused", reused}, {"summary", cached->summary()}};
    return out;
}

// --- photonic unit cell -------------------------------------------------------------------------

Result<AnalysisOutcome> evaluatePhotonic(const FunctionalStructure& s, const Project& p, const Values&, const ModelInfo& m, JobContext*)
{
    const SceneObject* l1 = bodyObject(s, p, "layer1");
    const SceneObject* l2 = bodyObject(s, p, "layer2");
    if (!l1 || !l2 || !l1->state.geometry || !l2->state.geometry || l1->state.geometry->kindId() != "box" || l2->state.geometry->kindId() != "box")
        return R::failure("the bodies 'layer1' and 'layer2' must be boxes (their heights are the layer thicknesses)");
    em::PeriodicCell cell;
    cell.thickness1 = l1->state.geometry->parameter("height");
    cell.thickness2 = l2->state.geometry->parameter("height");
    AnalysisOutcome out;
    const double guess = em::kSpeedOfLight / (4.0 * (cell.thickness1 + cell.thickness2));
    for (int layer = 1; layer <= 2; ++layer) {
        const Material* mat = materialFor(s, p, "layer" + std::to_string(layer));
        if (!mat)
            return R::failure("layer" + std::to_string(layer) + " has no material");
        auto e = em::emMaterialOf(*mat, guess);
        if (!e)
            return R::failure("the material '" + mat->name() + "': " + e.message());
        (layer == 1 ? cell.permittivity1 : cell.permittivity2) = std::max(1.0, e.value().relativePermittivity);
        if (e.value().lossTangent > 0.0 || e.value().conductivity > 0.0)
            out.warnings.push_back("the losses of '" + mat->name() + "' are ignored by the lossless Bloch model");
    }
    if (Status st = cell.validate(); !st)
        return R::failure(st.message());
    const int periods = std::clamp(s.analysis.settings.value("periods", 10), 1, 200);
    const double fmax = s.frequencyHigh > 0.0 ? s.frequencyHigh : 2.2 * em::kSpeedOfLight / (2.0 * (std::sqrt(cell.permittivity1) * cell.thickness1 +
                                                                                               std::sqrt(cell.permittivity2) * cell.thickness2));
    const double fmin = s.frequencyHigh > 0.0 ? std::max(1.0, s.frequencyLow) : fmax / 400.0;
    const auto gaps = em::bandGaps(cell, fmax);
    const em::EffectiveMedium eff = em::effectiveMedium(cell);
    out.solver = m.id;
    out.assumptions = m.assumptions;
    Values& x = out.metrics;
    x["cell.period"] = cell.period();
    x["cell.gapCount"] = static_cast<double>(gaps.size());
    x["cell.permittivityParallel"] = eff.permittivityParallel;
    x["cell.permittivityPerpendicular"] = eff.permittivityPerpendicular;
    x["cell.homogenizationLimit"] = eff.validBelowHz;
    if (auto n = em::blochEffectiveIndex(cell, std::min(eff.validBelowHz, 0.5 * fmax)))
        x["cell.effectiveIndex"] = n.value();
    x["stack.periods"] = periods;
    if (!gaps.empty()) {
        const em::BandGap& g = gaps.front();
        x["gap1.lower"] = g.lowerHz;
        x["gap1.upper"] = g.upperHz;
        x["gap1.center"] = g.centerHz;
        x["gap1.relativeWidth"] = g.relativeWidth;
        x["gap1.attenuationPerCell"] = g.peakAttenuation;
        auto t = em::LayeredSolution::solve(em::periodicStack(cell, periods, g.centerHz));
        if (t)
            x["stack.transmissionAtGapCenter"] = t.value().transmittance();
        out.behaviour = "mixed";
    } else {
        out.behaviour = "broadband";
        out.warnings.push_back("no band gap below " + std::to_string(fmax / 1e9).substr(0, 6) + " GHz");
    }
    for (const auto& g : gaps)
        out.modes.push_back({"gap " + std::to_string(out.modes.size() + 1), g.centerHz, 0.0,
                             {{"lowerHz", g.lowerHz}, {"upperHz", g.upperHz}, {"relativeWidth", g.relativeWidth}, {"atZoneEdge", g.atZoneEdge}}});
    Curve t = curve("transmission", "Transmittance of " + std::to_string(periods) + " cells (exact layered solution)", "Frequency", "Hz", "T", "");
    Curve re = curve("bloch", "Bloch wavenumber Re(K) a / pi", "Frequency", "Hz", "Re(K) a / pi", "");
    Curve im = curve("attenuation", "Attenuation Im(K) a (Np per cell)", "Frequency", "Hz", "Im(K) a", "Np");
    for (double f : linSpace(fmin, fmax, 300)) {
        auto sol = em::LayeredSolution::solve(em::periodicStack(cell, periods, f));
        t.x.push_back(f);
        t.y.push_back(sol ? sol.value().transmittance() : 0.0);
        const em::BlochPoint b = em::blochPoint(cell, f);
        re.x.push_back(f);
        re.y.push_back(b.wavenumber.real() * cell.period() / em::kPi);
        im.x.push_back(f);
        im.y.push_back(b.wavenumber.imag() * cell.period());
    }
    out.curves = {std::move(t), std::move(re), std::move(im)};
    out.details = {{"permittivity", {cell.permittivity1, cell.permittivity2}}, {"effectiveModel", eff.model}};
    out.discretization = {{"model", "exact Bloch dispersion and transfer matrices"}, {"periods", periods}};
    return out;
}

// --- periodic elastic rod -----------------------------------------------------------------------

Result<AnalysisOutcome> evaluateRod(const FunctionalStructure& s, const Project& p, const Values&, const ModelInfo& m, JobContext*)
{
    const SceneObject* rod = bodyObject(s, p, "rod");
    if (!rod || !rod->state.geometry || rod->state.geometry->kindId() != "rod.periodic")
        return R::failure("the body 'rod' must be a periodic rod (rod.periodic)");
    const Material* mat = materialFor(s, p, "rod");
    if (!mat)
        return R::failure("the rod has no material");
    auto elastic = mechanics::elasticMaterialOf(*mat);
    if (!elastic)
        return R::failure(elastic.message());
    auto cell = mechanics::periodicRodOf(*rod->state.geometry, elastic.value());
    if (!cell)
        return R::failure(cell.message());
    const double fmax = s.frequencyHigh > 0.0 ? s.frequencyHigh : s.analysis.settings.value("maxFrequencyHz", 50000.0);
    const auto gaps = mechanics::analyticalStopBands(cell.value(), fmax);
    const auto bands = mechanics::rodBandStructure(cell.value(), 4, 41);
    AnalysisOutcome out;
    out.solver = m.id;
    out.assumptions = m.assumptions;
    out.metrics["rod.gapCount"] = static_cast<double>(gaps.size());
    if (!gaps.empty()) {
        out.metrics["gap1.lower"] = gaps[0].first;
        out.metrics["gap1.upper"] = gaps[0].second;
        out.metrics["gap1.center"] = 0.5 * (gaps[0].first + gaps[0].second);
        out.metrics["gap1.relativeWidth"] = (gaps[0].second - gaps[0].first) / (0.5 * (gaps[0].first + gaps[0].second));
    }
    for (const auto& g : gaps)
        out.modes.push_back({"gap " + std::to_string(out.modes.size() + 1), 0.5 * (g.first + g.second), 0.0, {{"lowerHz", g.first}, {"upperHz", g.second}}});
    for (size_t b = 0; b < bands.frequencies.size(); ++b) {
        Curve c = curve("band" + std::to_string(b + 1), "Band " + std::to_string(b + 1), "q a / pi", "", "Frequency", "Hz");
        c.x = bands.reducedWavenumber;
        c.y = bands.frequencies[b];
        out.curves.push_back(std::move(c));
    }
    out.behaviour = gaps.empty() ? "broadband" : "mixed";
    out.discretization = {{"model", "exact longitudinal dispersion (analytical stop bands) and finite-element bands"}};
    return out;
}

// --- coupled slab (the Coupled Energy-Matter Engine) --------------------------------------------

Result<AnalysisOutcome> evaluateCoupled(const FunctionalStructure& s, const Project& p, const Values&, const ModelInfo& m, JobContext* context)
{
    if (s.physicalSystem.empty())
        return R::failure("the structure names no physical system (physicalSystem)");
    const auto ext = p.extensions.find("multiphysics");
    json document;
    if (ext != p.extensions.end())
        for (const json& d : ext->value("systems", json::array()))
            if (d.value("id", "") == s.physicalSystem)
                document = d;
    if (document.is_null())
        return R::failure("the project has no physical system '" + s.physicalSystem + "'");
    auto system = multiphysics::PhysicalSystem::fromJson(document);
    if (!system)
        return R::failure(system.message());
    AnalysisOutcome out;
    // Region thicknesses from the linked scene objects (box heights): the geometry drives the coupled system.
    json linked = json::array();
    for (auto& region : system.value().regions) {
        if (region.objectId.empty())
            continue;
        const SceneObject* o = p.scene.find(Id(region.objectId));
        const std::string kind = o && o->state.geometry ? o->state.geometry->kindId() : "";
        if (kind == "box") {
            region.thickness = o->state.geometry->parameter("height");
            linked.push_back({{"region", region.id}, {"object", o->state.name}, {"thickness", region.thickness}});
        } else if (kind == "cavity.rectangular.em" && !region.dimensions.empty()) {
            for (const char* dim : {"a", "b", "d"})
                region.dimensions[dim] = o->state.geometry->parameter(dim);
            linked.push_back({{"region", region.id}, {"object", o->state.name}, {"dimensions", region.dimensions}});
        } else {
            out.warnings.push_back("region '" + region.id + "' is linked to an object that is not a box or a cavity; its stored size is used");
        }
    }
    multiphysics::RunContext rc;
    if (context) {
        rc.cancelled = [context] { return context->cancelRequested(); };
        rc.progress = [context](double f, const std::string& message) { context->reportProgress(f, message); };
    }
    auto run = runPhysicalSystem(system.value(), p.materials, rc);
    if (!run)
        return R::failure(run.message());
    const multiphysics::CoupledResult& r = run.value();
    if (!r.ok())
        return R::failure("the coupled run ended " + std::string(multiphysics::toString(r.status)) + ": " + r.message);
    Values& x = out.metrics;
    x["coupled.converged"] = r.status == multiphysics::RunStatus::Converged ? 1.0 : 0.0;
    x["coupled.passes"] = r.passes;
    x["coupled.balanced"] = r.audit.balanced ? 1.0 : 0.0;
    x["coupled.relativeResidual"] = r.audit.relativeResidual;
    for (const auto& [name, field] : r.fields)
        if (name.size() > 12 && name.substr(name.size() - 12) == ".temperature") {
            x["coupled.peakTemperature"] = field.maxAbs();
            Curve c = curve("temperature", "Temperature through the slab", "z", "m", "T", "K");
            for (size_t i = 0; i < field.values.size() && i + 1 < field.support.edges[2].size(); ++i) {
                c.x.push_back(0.5 * (field.support.edges[2][i] + field.support.edges[2][i + 1]));
                c.y.push_back(field.values[i]);
            }
            out.curves.push_back(std::move(c));
        }
    if (!r.history.empty())
        for (const auto& [participant, values] : r.history.back().monitored.items()) {
            if (values.contains("absorptance"))
                x["coupled.absorptance"] = values["absorptance"].get<double>();
            if (values.contains("absorbedPowerW"))
                x["coupled.absorbedPower"] = values["absorbedPowerW"].get<double>();
            if (values.contains("maxVonMisesPa"))
                x["coupled.maxVonMises"] = values["maxVonMisesPa"].get<double>();
            if (values.contains("topDisplacementZM"))
                x["coupled.topDisplacement"] = values["topDisplacementZM"].get<double>();
            // A resonant participant: its resonance now and at the first pass (the shift the coupling caused).
            if (values.contains("resonanceHz")) {
                x["coupled.resonanceHz"] = values["resonanceHz"].get<double>();
                x["coupled.loadedQ"] = values.value("loadedQ", 0.0);
                x["coupled.deliveredPower"] = values.value("deliveredW", 0.0);
                const auto& first = r.history.front().monitored;
                if (first.contains(participant) && first[participant].contains("resonanceHz"))
                    x["coupled.resonanceShift"] = values["resonanceHz"].get<double>() - first[participant]["resonanceHz"].get<double>();
            }
            if (values.contains("temperatureK"))
                x["coupled.peakTemperature"] = values["temperatureK"].get<double>();
        }
    out.solver = m.id;
    out.assumptions = m.assumptions;
    out.warnings.insert(out.warnings.end(), r.warnings.begin(), r.warnings.end());
    out.behaviour = "coupled";
    out.details = {{"system", s.physicalSystem}, {"linkedRegions", linked}, {"status", std::string(multiphysics::toString(r.status))},
                   {"audit", r.audit.toJson()}, {"geometry", r.geometry}};
    out.discretization = {{"model", "Coupled Energy-Matter Engine"}, {"scheme", document.value("scheme", json::object())}};
    return out;
}

const std::vector<std::string> kCavityAssumptions{
    "ideal rectangular box; closed-form TE/TM modes (z along d)",
    "wall loss by perturbation: the ideal mode's tangential H on walls of the wall material's surface resistance (good conductor)",
    "fill loss from the fill's loss tangent and conductivity at the frequency of interest; isotropic, homogeneous fill",
    "the port is a lumped coupling of one mode (an external Q), not a modeled aperture or probe"};

}  // namespace

void registerFunctionalAnalyses(AnalysisRegistry& registry)
{
    {
        AnalysisKind k;
        k.id = kCavityAnalysis;
        k.title = "Microwave cavity resonator";
        k.description = "Resonant modes, field distribution, stored energy, wall and fill losses, Q, bandwidth and port response of a rectangular cavity";
        k.domains = {"electromagnetic"};
        k.category = Category::Confinement;
        k.mechanism = "conducting walls confine standing electromagnetic waves: each dimension fixes a wavenumber (m pi / a, ...), so the resonant "
                      "frequency f = c / 2 sqrt((m/a)^2 + (n/b)^2 + (p/d)^2) falls when a dimension grows; Q is the ratio of stored energy to the "
                      "energy lost per radian in the walls (surface resistance) and the fill (loss tangent)";
        k.models = {model(em::kCavityModelId, "Closed-form modes with perturbational losses", em::kCavityModelVersion, "analytical", kCavityAssumptions,
                          "good-conductor walls (skin depth much smaller than the dimensions); losses small (Q >> 1)", 0.0001),
                    model(em::kCavityYeeModelId, "Full-wave Yee solver (perfectly conducting walls)", em::kCavityModelVersion, "full-wave",
                          {"finite differences on a cubic grid; the box rounded to whole cells", "perfectly conducting walls (no wall loss)",
                           "a loss tangent represented by the conductivity it equals at the lowest resonance"},
                          "8 or more cells across the smallest dimension for errors below about 1 %", 5.0)};
        k.metrics = {{"cavity.f1", "Lowest resonant frequency", "Hz", "frequency of the lowest mode"},
                     {"cavity.f2", "Second resonant frequency", "Hz", ""},
                     {"cavity.f3", "Third resonant frequency", "Hz", ""},
                     {"cavity.modeSeparation", "Separation of the two lowest modes", "Hz", "f2 - f1"},
                     {"cavity.unloadedQ", "Unloaded Q of the lowest mode", "", "1/Q0 = 1/Qwall + 1/Qfill"},
                     {"cavity.wallQ", "Wall Q", "", "omega W / P_wall"},
                     {"cavity.fillQ", "Fill Q", "", "omega W / P_fill = eps' / eps''_eff (Yee: omega / decay rate)"},
                     {"cavity.loadedQ", "Loaded Q", "", "1/QL = 1/Q0 + 1/Qe"},
                     {"cavity.bandwidth", "Half-power bandwidth", "Hz", "f1 / QL"},
                     {"cavity.coupling", "Port coupling coefficient", "", "beta = Q0 / Qe"},
                     {"cavity.portReflection", "Port reflection on resonance", "", "|Gamma(f1)| = |beta - 1| / (beta + 1)"},
                     {"cavity.storedEnergy", "Stored energy", "J", "time-averaged W_e + W_m of the lowest mode for a peak |E| of 1 V/m"},
                     {"cavity.dfda", "Sensitivity of f1 to a", "Hz/m", "d f1 / d a (analytical)"},
                     {"cavity.dfdb", "Sensitivity of f1 to b", "Hz/m", "d f1 / d b"},
                     {"cavity.dfdd", "Sensitivity of f1 to d", "Hz/m", "d f1 / d d"},
                     {"cavity.thermalShift", "Shift of f1 at the operating temperature", "Hz",
                      "f1(T) - f1(293.15 K) for walls expanding uniformly with their linear expansion coefficient"},
                     {"cavity.extractionEfficiency", "Extraction efficiency", "", "beta / (1 + beta): the share of the loss leaving through the port"},
                     {"cavity.ringDownF1", "Ring-down frequency of the lowest mode", "Hz", "complex pole of the Yee probe record (matrix pencil)"},
                     {"cavity.ringDownQ", "Ring-down Q of the lowest mode", "", "w / (2 alpha) of that pole"},
                     {"cavity.f1Deviation", "Full-wave deviation of f1", "", "f1(Yee) / f1(closed form) - 1"},
                     {"cavity.decayRate", "Energy decay rate", "1/s", "-d ln W / dt of the full-wave run"}};
        k.roles = {{"cavity", "body", "a cavity.rectangular.em object", true},
                   {"walls", "region", "the conducting wall material", true},
                   {"fill", "region", "the dielectric filling (vacuum when absent)", false},
                   {"port", "boundary", "a coupling port: settings.externalQ or settings.coupling (beta)", false}};
        k.evaluate = evaluateCavity;
        registry.add(std::move(k));
    }
    {
        AnalysisKind k;
        k.id = kOpticalResonatorAnalysis;
        k.title = "Two-mirror optical resonator";
        k.description = "Stability, Gaussian modes, mode spacing, finesse, linewidth and Q of two spherical or plane mirrors";
        k.domains = {"electromagnetic"};
        k.category = Category::Confinement;
        k.mechanism = "curved mirrors refocus a Gaussian beam onto itself when 0 < g1 g2 < 1 (g = 1 - L / R); the round trip fixes the longitudinal "
                      "spacing c / 2nL, the Gouy phase the transverse spacing, and the mirror and clipping losses the finesse and Q";
        k.models = {model(em::kOpticalResonatorModelId, "Paraxial Gaussian-beam resonator (Kogelnik and Li)", em::kOpticalResonatorModelVersion,
                          "analytical", {"paraxial, scalar; Hermite-Gaussian modes", "aligned, thin mirrors"},
                          "waist much larger than the wavelength; stable or confocal resonators for mode results", 0.0001)};
        k.metrics = {{"optics.separation", "Mirror separation", "m", "distance between the mirror objects"},
                     {"optics.g1g2", "Stability product", "", "g1 g2 with g = 1 - L/R: stable for 0 < g1 g2 < 1"},
                     {"optics.g1", "g1", "", ""}, {"optics.g2", "g2", "", ""},
                     {"optics.stable", "Stable", "", "1 when 0 < g1 g2 < 1"},
                     {"optics.waist", "Waist radius", "m", "1/e^2 intensity radius of TEM00"},
                     {"optics.waistPosition", "Waist position", "m", "from mirror 1"},
                     {"optics.spot1", "Beam radius on mirror 1", "m", ""}, {"optics.spot2", "Beam radius on mirror 2", "m", ""},
                     {"optics.rayleighRange", "Rayleigh range", "m", "pi w0^2 / lambda_medium"},
                     {"optics.fsr", "Free spectral range", "Hz", "c / (2 n L)"},
                     {"optics.transverseSpacing", "Transverse mode spacing", "Hz", "FSR arccos(+-sqrt(g1 g2)) / pi"},
                     {"optics.finesse", "Finesse", "", "pi S^(1/4) / (1 - sqrt(S)), S = round-trip survival"},
                     {"optics.linewidth", "Linewidth", "Hz", "FSR / finesse"},
                     {"optics.Q", "Quality factor", "", "optical frequency / linewidth"},
                     {"optics.photonLifetime", "Photon lifetime", "s", "1 / (2 pi linewidth)"},
                     {"optics.roundTripLoss", "Round-trip loss", "", "1 - R1' R2' (1 - extra loss)"},
                     {"optics.clipping1", "Clipping loss at mirror 1", "", "exp(-2 a^2 / w^2)"},
                     {"optics.clipping2", "Clipping loss at mirror 2", "", ""},
                     {"optics.transmission", "Transmission on resonance", "", "T1 T2 / (1 - sqrt(S))^2"},
                     {"optics.storedEnergyPerWatt", "Stored energy per watt circulating", "J/W", "2 n L / c"}};
        k.roles = {{"mirror1", "body", "a reflector.spherical or reflector.flat object", true},
                   {"mirror2", "body", "the second mirror", true}};
        k.evaluate = evaluateOptical;
        registry.add(std::move(k));
    }
    {
        AnalysisKind k;
        k.id = kHornAnalysis;
        k.title = "Acoustic horn";
        k.description = "Throat impedance, impedance transformation, reflection, transmission, radiated power and mouth directivity of an axisymmetric horn";
        k.domains = {"acoustic"};
        k.category = Category::Propagation;
        k.mechanism = "a gradually flaring duct transforms the small throat's high acoustic impedance to the mouth's radiation load: above the flare "
                      "cut-off the throat sees a nearly resistive load (rho c / S_t), so a source couples its power into sound; reflections at a small "
                      "mouth cause the resistance ripple";
        k.models = {model(manipulators::kHornModelId, "Webster horn equation with a baffled-piston mouth", manipulators::kHornModelVersion, "reduced",
                          {"plane (one-parameter) waves along the axis; lossless walls", "mouth radiation of a circular piston in an infinite rigid baffle",
                           "linear acoustics in a homogeneous gas"},
                          "local ka below about 1.8 (no cross modes) and a gradual flare", 0.002)};
        k.metrics = {{"horn.cutoff", "Flare cut-off", "Hz", "m c / (4 pi) of exponential and hyperbolic horns"},
                     {"horn.throatResistance", "Normalized throat resistance", "", "Re(Z_t) S_t / (rho c)"},
                     {"horn.throatReactance", "Normalized throat reactance", "", "Im(Z_t) S_t / (rho c)"},
                     {"horn.reflection", "Throat reflection", "", "|Z_t - rho c/S_t| / |Z_t + rho c/S_t|"},
                     {"horn.transmission", "Power transmission", "", "1 - |reflection|^2"},
                     {"horn.radiatedPower", "Radiated power", "W", "1/2 Re(Z_t) |U_t|^2 for the throat velocity"},
                     {"horn.powerBalance", "Power balance error", "", "P_mouth / P_throat - 1 (lossless: 0)"},
                     {"horn.onAxisPressure1m", "On-axis pressure at 1 m", "Pa", "baffled-piston far field of the mouth volume velocity"},
                     {"horn.directivityIndex", "Directivity index", "dB", "10 log10((ka)^2 / (1 - J1(2ka)/(ka)))"},
                     {"horn.beamHalfAngle", "-6 dB half angle", "rad", "0 when the main lobe fills the half-space"},
                     {"horn.mouthKa", "ka of the mouth", "", ""},
                     {"horn.meanTransmission", "Mean transmission above cut-off", "", "over the frequency range"},
                     {"horn.resonancePeaks", "Resonance peaks", "", "peaks of the throat resistance in the range"}};
        k.roles = {{"horn", "body", "a horn.acoustic object", true}};
        k.evaluate = evaluateHorn;
        registry.add(std::move(k));
    }
    {
        AnalysisKind k;
        k.id = kMechanicalModalAnalysis;
        k.title = "Mechanical resonator";
        k.description = "Natural frequencies, modal masses and mode shapes of a structure (beam, plate, membrane, solid)";
        k.domains = {"mechanical"};
        k.category = Category::Confinement;
        k.mechanism = "stiffness and mass distribution fix the natural modes: a longer or thinner beam is more compliant and resonates lower "
                      "(f ~ t / L^2 for bending)";
        k.models = {model("mechanics.modal.fe", "Finite elements (the structure's mesh)", "1", "finite-element",
                          {"linear elasticity, small displacements", "the structure's idealization, supports and element size"},
                          "elements resolve the mode shapes (see the study's elements per wavelength)", 0.2),
                    model("mechanics.modal.analytical", "Closed-form beam and plate formulas", "1", "analytical",
                          {"Euler-Bernoulli beams or simply supported Kirchhoff plates"}, "slender beams, thin plates", 0.0001)};
        k.metrics = {{"mech.f1", "First natural frequency", "Hz", ""},
                     {"mech.f2", "Second natural frequency", "Hz", ""},
                     {"mech.f3", "Third natural frequency", "Hz", ""},
                     {"mech.massFractionZ1", "Effective mass fraction (Z) of mode 1", "", ""},
                     {"mech.totalMass", "Mass", "kg", "of the free degrees of freedom"},
                     {"mech.analyticalF1", "Closed-form first frequency", "Hz", ""},
                     {"mech.f1Deviation", "Deviation from the closed form", "", "f1(FE) / f1(analytical) - 1"}};
        k.roles = {{"resonator", "body", "an object with a structure component", true}};
        k.evaluate = evaluateMechanical;
        registry.add(std::move(k));
    }
    {
        AnalysisKind k;
        k.id = kPhotonicCellAnalysis;
        k.title = "Photonic unit cell (two layers)";
        k.description = "Bloch dispersion, band gaps, finite-stack transmission and homogenized properties of a periodic two-layer cell";
        k.domains = {"electromagnetic"};
        k.category = Category::Reflection;
        k.mechanism = "partial reflections at every interface add in phase when the round trip through a cell is a multiple of half a wavelength: "
                      "the waves become evanescent (a band gap) and the stack reflects; the index contrast sets the gap width";
        k.models = {model(em::kPeriodicLayersModelId, "Exact Bloch dispersion and transfer matrices", em::kPeriodicLayersModelVersion, "analytical",
                          {"normal incidence", "lossless, isotropic, non-magnetic layers", "infinite lateral extent"},
                          "homogenized values only below the reported limit (a / lambda < 0.1)", 0.01)};
        k.metrics = {{"cell.period", "Period", "m", "d1 + d2"},
                     {"cell.gapCount", "Band gaps in the range", "", ""},
                     {"gap1.lower", "First gap: lower edge", "Hz", ""},
                     {"gap1.upper", "First gap: upper edge", "Hz", ""},
                     {"gap1.center", "First gap: centre", "Hz", ""},
                     {"gap1.relativeWidth", "First gap: relative width", "", "(upper - lower) / centre"},
                     {"gap1.attenuationPerCell", "First gap: peak attenuation", "Np", "max Im(K) a"},
                     {"stack.transmissionAtGapCenter", "Stack transmission at the gap centre", "", "exact layered solution"},
                     {"stack.periods", "Cells in the finite stack", "", ""},
                     {"cell.effectiveIndex", "Effective index (long wavelength)", "", "K c / omega below the homogenization limit"},
                     {"cell.homogenizationLimit", "Homogenization limit", "Hz", "a / lambda_eff = 0.1"},
                     {"cell.permittivityParallel", "Homogenized permittivity (E along the layers)", "", "(e1 d1 + e2 d2) / a"},
                     {"cell.permittivityPerpendicular", "Homogenized permittivity (E across the layers)", "", "a / (d1/e1 + d2/e2)"}};
        k.roles = {{"layer1", "body", "a box whose height is the first layer's thickness", true},
                   {"layer2", "body", "a box whose height is the second layer's thickness", true}};
        k.evaluate = evaluatePhotonic;
        registry.add(std::move(k));
    }
    {
        AnalysisKind k;
        k.id = kPeriodicRodAnalysis;
        k.title = "Periodic elastic rod (phononic)";
        k.description = "Longitudinal stop bands and band structure of a rod of alternating sections";
        k.domains = {"mechanical"};
        k.category = Category::Reflection;
        k.mechanism = "impedance steps between sections reflect longitudinal waves; where the reflections add in phase the waves cannot propagate";
        k.models = {model("mechanics.bloch.rod", "Exact longitudinal dispersion of a periodic rod", "1", "analytical",
                          {"longitudinal waves, uniform stress over each section (elementary rod theory)"}, "wavelength much longer than the radius", 0.01)};
        k.metrics = {{"rod.gapCount", "Stop bands in the range", "", ""},
                     {"gap1.lower", "First stop band: lower edge", "Hz", ""},
                     {"gap1.upper", "First stop band: upper edge", "Hz", ""},
                     {"gap1.center", "First stop band: centre", "Hz", ""},
                     {"gap1.relativeWidth", "First stop band: relative width", "", ""}};
        k.roles = {{"rod", "body", "a rod.periodic object with an elastic material", true}};
        k.evaluate = evaluateRod;
        registry.add(std::move(k));
    }
    {
        AnalysisKind k;
        k.id = kCoupledSlabAnalysis;
        k.title = "Coupled energy-matter system";
        k.description = "Runs the structure's physical system in the Coupled Energy-Matter Engine with region sizes from the linked objects "
                        "(slab thicknesses from boxes, cavity dimensions from cavities)";
        k.domains = {"electromagnetic", "thermal", "mechanical"};
        k.category = Category::Coupled;
        k.mechanism = "the slab's thickness sets its electromagnetic standing-wave pattern and so the absorbed power; the heat raises the "
                      "temperature, which changes the loss and expands the slab, feeding back into the wave solution";
        k.models = {model("multiphysics.engine", "Coupled Energy-Matter Engine (the system's scheme)", "1", "coupled",
                          {"the physical system's participants, couplings and scheme"}, "as the engine's models (layered systems)", 0.05)};
        k.metrics = {{"coupled.converged", "Converged", "", "1 when the scheme converged"},
                     {"coupled.passes", "Passes", "", ""},
                     {"coupled.balanced", "Energy balanced", "", "1 when the ledger residual is within its tolerance"},
                     {"coupled.relativeResidual", "Ledger residual", "", "relative to the energy that flowed"},
                     {"coupled.peakTemperature", "Peak temperature", "K", ""},
                     {"coupled.absorptance", "Absorptance", "", "absorbed / incident power"},
                     {"coupled.absorbedPower", "Absorbed power", "W", ""},
                     {"coupled.maxVonMises", "Largest von Mises stress", "Pa", ""},
                     {"coupled.topDisplacement", "Expansion of the slab", "m", "displacement of its top face"},
                     {"coupled.resonanceHz", "Resonance of the coupled state", "Hz", "a resonant participant's frequency at the converged state"},
                     {"coupled.resonanceShift", "Resonance shift by the coupling", "Hz", "converged minus first-pass resonance"},
                     {"coupled.loadedQ", "Loaded Q of the coupled state", "", ""},
                     {"coupled.deliveredPower", "Power delivered into the resonator", "W", "incident minus reflected"}};
        k.roles = {};
        k.evaluate = evaluateCoupled;
        registry.add(std::move(k));
    }
    registerResonanceAnalyses(registry);
    registerMetamaterialAnalyses(registry);
    registerMaterialStateAnalyses(registry);
    // Every evaluation turns exceptions (an invalid setting) into a failure.
    for (const AnalysisKind& k : registry.kinds()) {
        AnalysisKind copy = k;
        const Evaluate inner = k.evaluate;
        copy.evaluate = [inner](const FunctionalStructure& s, const Project& p, const Values& v, const ModelInfo& m, JobContext* c) -> R {
            try {
                return inner(s, p, v, m, c);
            } catch (const std::exception& e) {
                return R::failure(e.what());
            }
        };
        registry.add(std::move(copy));
    }
}

}  // namespace resonant::coupling
