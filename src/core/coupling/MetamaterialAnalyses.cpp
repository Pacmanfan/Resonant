#include "coupling/MetamaterialAnalyses.h"

#include "electromagnetics/Conventions.h"
#include "electromagnetics/EmMaterial.h"
#include "electromagnetics/ObliqueLayers.h"
#include "electromagnetics/Retrieval.h"
#include "electromagnetics/SplitRing.h"
#include "functional/EffectiveMaterial.h"
#include "manipulators/Helmholtz.h"
#include "manipulators/ResonatorDuct.h"
#include "materials/Material.h"
#include "mechanics/LocallyResonantLattice.h"
#include "oscillators/BlochLattice.h"
#include "oscillators/LumpedNetwork.h"
#include "project/Project.h"
#include "thermal/LayeredConductivity.h"
#include "thermal/ThermalMaterial.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace resonant::coupling {

using namespace functional;
using nlohmann::json;
using R = Result<AnalysisOutcome>;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr double kNpToDb = 8.685889638065035;  // 20 log10(e)

const SceneObject* bodyOf(const FunctionalStructure& s, const Project& p, const std::string& role)
{
    const Body* b = s.body(role);
    return b ? p.scene.find(Id(b->objectId)) : nullptr;
}

const Material* materialOf(const FunctionalStructure& s, const Project& p, const std::string& role)
{
    if (const Region* r = s.region(role))
        return p.materials.find(Id(r->materialId));
    if (const SceneObject* o = bodyOf(s, p, role); o && !o->state.materialId.isNull())
        return p.materials.find(o->state.materialId);
    return nullptr;
}

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

int count(const json& where, const char* key, const Values& parameters, int fallback, int lo, int hi)
{
    return std::clamp(static_cast<int>(std::lround(setting(where, key, parameters, fallback))), lo, hi);
}

Curve curve(std::string id, std::string title, std::string xl, std::string xu, std::string yl, std::string yu)
{
    Curve c;
    c.id = std::move(id);
    c.title = std::move(title);
    c.xLabel = std::move(xl);
    c.xUnit = std::move(xu);
    c.yLabel = std::move(yl);
    c.yUnit = std::move(yu);
    return c;
}

std::vector<double> linSpace(double lo, double hi, int n)
{
    std::vector<double> v;
    for (int i = 0; i < n; ++i)
        v.push_back(lo + (hi - lo) * i / std::max(1, n - 1));
    return v;
}

ModelInfo model(std::string id, std::string name, std::string fidelity, std::vector<std::string> assumptions, std::string validity, double seconds)
{
    return {std::move(id), std::move(name), "1", std::move(fidelity), std::move(assumptions), std::move(validity), seconds};
}

void begin(AnalysisOutcome& out, const ModelInfo& mi)
{
    out.solver = mi.id;
    out.assumptions = mi.assumptions;
}

json derivationBase(const FunctionalStructure& s, const std::string& analysis, const Values& v)
{
    json parameters = json::object();
    for (const auto& [k, x] : v)
        parameters[k] = x;
    return {{"structure", {{"id", s.id}, {"name", s.name}}}, {"analysis", analysis}, {"parameters", parameters}};
}

EffectiveProperty property(std::string domain, std::string key, std::string component, double value, std::string note = {})
{
    EffectiveProperty p;
    p.domain = std::move(domain);
    p.key = std::move(key);
    p.component = std::move(component);
    p.value = value;
    p.note = std::move(note);
    return p;
}

// --- mass-in-mass lattice ----------------------------------------------------------------------

Result<AnalysisOutcome> evaluateMassInMass(const FunctionalStructure& s, const Project&, const Values& v, const ModelInfo& mi, JobContext*)
{
    const json& st = s.analysis.settings;
    mechanics::MassInMassCell c;
    c.hostMass = setting(st, "hostMass", v, 0.1);
    c.hostStiffness = setting(st, "hostStiffness", v, 4e6);
    c.resonatorMass = setting(st, "resonatorMass", v, 0.1);
    c.resonatorStiffness = setting(st, "resonatorStiffness", v, 1e5);
    c.spacing = setting(st, "spacing", v, 0.02);
    c.resonatorLossFactor = setting(st, "lossFactor", v, 0.0);
    if (Status ok = c.validate(); !ok)
        return R::failure(ok.message());
    const int cells = count(st, "cells", v, 30, 8, 400);
    AnalysisOutcome out;
    begin(out, mi);
    out.behaviour = "resonant";
    const double f2 = mechanics::localResonanceHz(c);
    const mechanics::LatticeGap gap = mechanics::massInMassGap(c);
    const double centre = 0.5 * (gap.lowerHz + gap.upperHz);
    Values& x = out.metrics;
    x["lattice.localResonance"] = f2;
    x["lattice.hostZoneEdge"] = std::sqrt(4.0 * c.hostStiffness / c.hostMass) / (2 * kPi);
    x["lattice.massRatio"] = c.resonatorMass / c.hostMass;
    x["gap.lower"] = gap.lowerHz;
    x["gap.upper"] = gap.upperHz;
    x["gap.relativeWidth"] = (gap.upperHz - gap.lowerHz) / centre;
    x["gap.negativeMassLower"] = gap.negativeMassLowerHz;
    x["gap.negativeMassUpper"] = gap.negativeMassUpperHz;
    x["gap.attenuationPerCell"] = mechanics::massInMassBlochWavenumber(c, centre).imag() * c.spacing;
    x["lattice.effectiveMassAtGapCentre"] = mechanics::effectiveMass(c, centre).real() / (c.hostMass + c.resonatorMass);
    // Long waves: the lattice moves as one medium of mass m1 + m2 per cell.
    x["lattice.longWaveSpeed"] = c.spacing * std::sqrt(c.hostStiffness / (c.hostMass + c.resonatorMass));
    {
        const double q = 2e-4;  // q a / pi: the resonators' own dispersion is negligible
        x["lattice.longWaveSpeedBloch"] = 2 * kPi * mechanics::massInMassDispersion(c, q)[0] / (q * kPi / c.spacing);
    }
    // The numerical Bloch eigenproblem of the unit cell against the closed form.
    const oscillators::BlochBands bands = oscillators::blochBands(mechanics::massInMassBlochCell(c), 81);
    double deviation = 0.0;
    for (size_t i = 0; i < bands.reducedWavenumber.size(); ++i) {
        const auto exact = mechanics::massInMassDispersion(c, bands.reducedWavenumber[i]);
        for (size_t b = 0; b < 2; ++b)
            if (exact[b] > 1e-9 * f2)
                deviation = std::max(deviation, std::abs(bands.frequencies[b][i] - exact[b]) / exact[b]);
    }
    x["bloch.maxDeviation"] = deviation;
    // A finite chain driven at its first host: the decay between interior hosts against Im(q) a.
    const oscillators::Network chain = mechanics::massInMassChain(c, cells);
    std::vector<std::complex<double>> force(static_cast<size_t>(chain.size()), 0.0);
    force[0] = 1.0;
    const double fmax = s.frequencyHigh > 0.0 ? s.frequencyHigh : 1.6 * gap.upperHz;
    const double fmin = s.frequencyHigh > 0.0 ? std::max(1e-3, s.frequencyLow) : fmax / 300.0;
    std::vector<double> freqs = linSpace(fmin, fmax, 300);
    freqs.push_back(centre);
    auto response = oscillators::drivenResponse(chain, force, freqs);
    if (!response)
        return R::failure(response.message());
    const auto host = [](const oscillators::DrivenPoint& pt, int i) { return std::abs(pt.displacement[static_cast<size_t>(2 * i)]); };
    const oscillators::DrivenPoint& atCentre = response.value().back();
    x["chain.cells"] = cells;
    x["chain.transmissionAtGapCentre"] = host(atCentre, cells - 1) / host(atCentre, 0);
    // Between interior hosts, away from the driven end and from the wave the free far end reflects.
    const int far = cells / 2;
    x["chain.decayPerCell"] = -std::log(host(atCentre, far) / host(atCentre, 2)) / (far - 2);
    Curve d1 = curve("dispersion1", "Acoustic branch", "q a / pi", "", "Frequency", "Hz");
    Curve d2 = curve("dispersion2", "Optical branch", "q a / pi", "", "Frequency", "Hz");
    for (double q : linSpace(0.0, 1.0, 101)) {
        const auto f = mechanics::massInMassDispersion(c, q);
        d1.x.push_back(q);
        d1.y.push_back(f[0]);
        d2.x.push_back(q);
        d2.y.push_back(f[1]);
    }
    Curve mass = curve("effectiveMass", "Effective dynamic mass / static mass", "Frequency", "Hz", "Re m_eff / (m1 + m2)", "");
    Curve att = curve("attenuation", "Attenuation Im(q) a", "Frequency", "Hz", "Im(q) a", "Np");
    Curve tr = curve("chainTransmission", "Transmission of the finite chain |u_last / u_first|", "Frequency", "Hz", "|u_N / u_1|", "dB");
    for (size_t i = 0; i + 1 < freqs.size(); ++i) {
        const double f = freqs[i];
        mass.x.push_back(f);
        mass.y.push_back(std::clamp(mechanics::effectiveMass(c, f).real() / (c.hostMass + c.resonatorMass), -50.0, 50.0));
        att.x.push_back(f);
        att.y.push_back(mechanics::massInMassBlochWavenumber(c, f).imag() * c.spacing);
        tr.x.push_back(f);
        tr.y.push_back(20.0 * std::log10(std::max(1e-300, host(response.value()[i], cells - 1) / host(response.value()[i], 0))));
    }
    out.curves = {std::move(d1), std::move(d2), std::move(mass), std::move(att), std::move(tr)};
    Mode local("local resonance", f2, c.resonatorLossFactor > 0.0 ? 1.0 / c.resonatorLossFactor : kInf,
               {{"gapLowerHz", gap.lowerHz}, {"gapUpperHz", gap.upperHz}});
    if (c.resonatorLossFactor > 0.0)
        local.lossQ["structural"] = 1.0 / c.resonatorLossFactor;
    out.modes.push_back(local);
    if (gap.upperHz > x["lattice.hostZoneEdge"])
        out.warnings.push_back("the local resonance is not far below the host lattice's zone edge: the gap mixes Bragg and local-resonance behaviour");
    out.details = {{"representations",
                    {{"constituents", {{"hostMass", c.hostMass}, {"hostStiffness", c.hostStiffness}, {"resonatorMass", c.resonatorMass},
                                       {"resonatorStiffness", c.resonatorStiffness}, {"lossFactor", c.resonatorLossFactor}}},
                     {"unitCell", "a host mass on the lattice springs carrying an internal resonator"},
                     {"periodic", {{"spacing", c.spacing}, {"finiteCells", cells}}},
                     {"effective", {{"dynamicMass", "m1 + m2 w2^2 / (w2^2 - w^2)"}, {"staticMass", c.hostMass + c.resonatorMass},
                                    {"negativeBetweenHz", {gap.negativeMassLowerHz, gap.negativeMassUpperHz}},
                                    {"note", "a dispersive, in the gap negative, mass per cell: reported as a result, not stored as a library material"}}},
                     {"equivalent", "the lumped lattice itself (no continuum)"}}}};
    out.discretization = {{"model", "closed-form dispersion, numerical Bloch eigenproblem of the cell, driven finite chain"}, {"cells", cells},
                          {"blochWavenumbers", 81}};
    return out;
}

// --- resonator duct ---------------------------------------------------------------------------

Result<AnalysisOutcome> evaluateResonatorDuct(const FunctionalStructure& s, const Project& p, const Values& v, const ModelInfo& mi, JobContext*)
{
    const SceneObject* o = bodyOf(s, p, "resonator");
    if (!o || !o->state.geometry || o->state.geometry->kindId() != "resonator.helmholtz")
        return R::failure("the body 'resonator' must be a Helmholtz resonator (resonator.helmholtz)");
    const json& st = s.analysis.settings;
    manipulators::ResonatorDuctSpec d;
    d.resonator = manipulators::helmholtzSpecOf(*o->state.geometry);
    d.resonator.density = setting(s.environment, "density", v, 1.204);
    d.resonator.soundSpeed = setting(s.environment, "soundSpeed", v, 343.0);
    d.resonator.viscosity = setting(s.environment, "viscosity", v, 1.81e-5);
    d.resonator.viscousLoss = st.value("viscousLoss", true);
    d.ductRadius = setting(st, "ductRadius", v, 0.025);
    d.spacing = setting(st, "spacing", v, 0.1);
    if (Status ok = d.validate(); !ok)
        return R::failure(ok.message());
    const int cells = count(st, "cells", v, 8, 1, 200);
    manipulators::ResonatorDuctSpec lossless = d;
    lossless.resonator.viscousLoss = false;
    manipulators::HelmholtzSpec branch = d.resonator;
    branch.radiationLoss = false;
    auto h = manipulators::analyzeHelmholtz(branch);
    if (!h)
        return R::failure(h.message());
    const double f0 = h.value().frequencyHz, c0 = d.resonator.soundSpeed, rho = d.resonator.density;
    const double ratio = d.resonator.volume() / (d.ductArea() * d.spacing);  // V / (S a)
    const double bragg = c0 / (2.0 * d.spacing);
    AnalysisOutcome out;
    begin(out, mi);
    out.behaviour = "resonant";
    Values& x = out.metrics;
    x["duct.resonatorF0"] = f0;
    x["duct.braggFrequency"] = bragg;
    x["duct.volumeRatio"] = ratio;
    x["duct.negativeModulusLower"] = f0;
    x["duct.negativeModulusUpper"] = f0 * std::sqrt(1.0 + ratio);
    const double fmax = s.frequencyHigh > 0.0 ? s.frequencyHigh : 1.3 * std::max(bragg, f0 * std::sqrt(1.0 + ratio));
    const auto gaps = manipulators::resonatorDuctGaps(lossless, fmax, 6000);
    const manipulators::DuctGap* hybrid = nullptr;
    const manipulators::DuctGap* braggGap = nullptr;
    for (const auto& g : gaps) {
        if (!hybrid && g.upperHz > f0 && g.lowerHz < f0 * std::sqrt(1.0 + ratio) * 1.5)
            hybrid = &g;
        else if (!braggGap && g.lowerHz < bragg * 1.2 && g.upperHz > bragg * 0.8)
            braggGap = &g;
    }
    x["gap.count"] = static_cast<double>(gaps.size());
    double attenuationDb = 0.0;
    if (hybrid) {
        const double centre = 0.5 * (hybrid->lowerHz + hybrid->upperHz);
        x["gap1.lower"] = hybrid->lowerHz;
        x["gap1.upper"] = hybrid->upperHz;
        x["gap1.relativeWidth"] = (hybrid->upperHz - hybrid->lowerHz) / centre;
        const double kappa = manipulators::resonatorDuctBloch(d, centre).imag() * d.spacing;
        attenuationDb = kNpToDb * kappa;
        x["gap1.blochAttenuationDbPerCell"] = attenuationDb;
        const double t1 = manipulators::resonatorDuctTransmission(d, centre, cells), t2 = manipulators::resonatorDuctTransmission(d, centre, cells + 4);
        x["gap1.transmissionDb"] = 10.0 * std::log10(t1);
        // Power falls by 2 kappa per cell: dB(N) - dB(N + 4) = 4 x 2 x 4.343 kappa = 4 x 8.686 kappa.
        x["gap1.measuredAttenuationDbPerCell"] = (10.0 * std::log10(t1) - 10.0 * std::log10(t2)) / 4.0;
        out.modes.push_back(Mode("hybridization gap", centre, 0.0, {{"lowerHz", hybrid->lowerHz}, {"upperHz", hybrid->upperHz}}));
    } else {
        out.warnings.push_back("no stop band was found near the resonators' frequency");
    }
    if (braggGap) {
        x["gap2.lower"] = braggGap->lowerHz;
        x["gap2.upper"] = braggGap->upperHz;
    }
    // Long waves: the resonators' compliance lowers the speed of sound to c / sqrt(1 + V / (S a)).
    x["duct.lowFrequencySpeed"] = c0 / std::sqrt(1.0 + ratio);
    {
        const double f = 0.02 * f0;
        x["duct.lowFrequencySpeedBloch"] = 2 * kPi * f / manipulators::resonatorDuctBloch(lossless, f).real();
    }
    Mode branchMode("side-branch resonance", f0, h.value().qViscous > 0.0 && d.resonator.viscousLoss ? h.value().qViscous : kInf,
                    {{"volumeRatio", ratio}});
    if (d.resonator.viscousLoss)
        branchMode.lossQ["viscous"] = h.value().qViscous;
    out.modes.insert(out.modes.begin(), branchMode);
    Curve re = curve("bloch", "Bloch wavenumber Re(q) a / pi", "Frequency", "Hz", "Re(q) a / pi", "");
    Curve im = curve("attenuation", "Attenuation Im(q) a", "Frequency", "Hz", "Im(q) a", "Np");
    Curve tr = curve("transmission", "Transmission of " + std::to_string(cells) + " cells", "Frequency", "Hz", "T", "dB");
    Curve bulk = curve("bulkModulus", "Effective bulk modulus / B0", "Frequency", "Hz", "Re B_eff / B0", "");
    const double B0 = rho * c0 * c0;
    for (double f : linSpace(fmax / 600.0, fmax, 600)) {
        const Complex q = manipulators::resonatorDuctBloch(d, f);
        re.x.push_back(f);
        re.y.push_back(q.real() * d.spacing / kPi);
        im.x.push_back(f);
        im.y.push_back(q.imag() * d.spacing);
        tr.x.push_back(f);
        tr.y.push_back(10.0 * std::log10(std::max(1e-300, manipulators::resonatorDuctTransmission(d, f, cells))));
        bulk.x.push_back(f);
        bulk.y.push_back(std::clamp(manipulators::resonatorDuctBulkModulus(lossless, f).real() / B0, -50.0, 50.0));
    }
    out.curves = {std::move(re), std::move(im), std::move(tr), std::move(bulk)};
    // The effective medium below the resonance: positive modulus, long waves.
    EffectiveMaterial e;
    e.name = s.name + " (effective)";
    e.kind = "homogenized";
    e.method = std::string(manipulators::kResonatorDuctModelId) + " (long-wavelength limit)";
    e.validToHz = 0.5 * f0;
    e.validFromHz = 0.0;
    const double cEnd = std::sqrt(manipulators::resonatorDuctBulkModulus(lossless, e.validToHz).real() / rho);
    e.scaleRatio = d.spacing * e.validToHz / cEnd;
    e.validity = "plane waves in the duct; frequencies below half the resonators' frequency, where the bulk modulus is positive and the cell is "
                 "small against the wavelength";
    EffectiveProperty speed = property("acoustic", "soundSpeed", "along the duct", c0 / std::sqrt(1.0 + ratio),
                                       "c_eff = sqrt(B_eff / rho0), 1/B_eff = 1/B0 + Y / (j w S a)");
    for (double f : linSpace(0.02 * f0, e.validToHz, 25))
        speed.frequencyTable.emplace_back(f, std::sqrt(manipulators::resonatorDuctBulkModulus(lossless, f).real() / rho));
    e.properties = {property("acoustic", "density", "along the duct", rho, "the resonators add compliance, not mass"), speed,
                    property("acoustic", "referenceTemperature", "isotropic", setting(s.environment, "temperature", v, 293.15))};
    e.limitations = {"one-dimensional: valid for plane waves along the duct", "losses are not included in the effective sound speed",
                     "above half the resonance the modulus disperses strongly and turns negative (reported, not stored)"};
    e.derivation = derivationBase(s, kResonatorDuctAnalysis, v);
    e.derivation["cell"] = {{"ductRadius", d.ductRadius}, {"spacing", d.spacing}, {"resonatorObject", o->id().str()},
                            {"resonatorVolume", d.resonator.volume()}, {"neckRadius", d.resonator.neckRadius}, {"neckLength", d.resonator.neckLength}};
    e.derivation["comparison"] = {{"lowFrequencySpeed", x["duct.lowFrequencySpeed"]}, {"lowFrequencySpeedBloch", x["duct.lowFrequencySpeedBloch"]}};
    out.details = {{"effectiveMaterial", e.toJson()},
                   {"representations",
                    {{"constituents", {{"density", rho}, {"soundSpeed", c0}}},
                     {"unitCell", {{"ductRadius", d.ductRadius}, {"spacing", d.spacing}, {"resonator", o->state.name}}},
                     {"periodic", {{"cells", cells}}},
                     {"effective", {{"bulkModulus", "1/B_eff = 1/B0 + Y/(j w S a)"}, {"negativeBetweenHz", {f0, f0 * std::sqrt(1.0 + ratio)}}}},
                     {"equivalent", "a transmission line with shunt Helmholtz admittances"}}}};
    out.discretization = {{"model", "cell transfer matrices (plane waves)"}, {"cells", cells}};
    return out;
}

// --- layered electromagnetic medium ---------------------------------------------------------

struct Layer {
    double thickness = 0.0;
    Complex eps{1.0, 0.0};
    double mu = 1.0;
    std::string material, object;
};

Complex complexPermittivity(const em::EmMaterial& m, double f)
{
    return Complex(m.relativePermittivity, -m.relativePermittivity * m.lossTangent) - Complex(0.0, m.conductivity / (2 * kPi * f * em::kEpsilon0));
}

Result<std::vector<Layer>> layersOf(const FunctionalStructure& s, const Project& p, double frequencyHz, AnalysisOutcome* out)
{
    using LR = Result<std::vector<Layer>>;
    std::vector<Layer> layers;
    for (const char* role : {"layer1", "layer2"}) {
        const SceneObject* o = bodyOf(s, p, role);
        if (!o || !o->state.geometry || o->state.geometry->kindId() != "box")
            return LR::failure(std::string("the body '") + role + "' must be a box (its height is the layer's thickness)");
        const Material* m = materialOf(s, p, role);
        if (!m)
            return LR::failure(std::string("the body '") + role + "' has no material");
        Layer l;
        l.thickness = o->state.geometry->parameter("height");
        l.material = m->name();
        l.object = o->id().str();
        if (out) {
            auto e = em::emMaterialOf(*m, frequencyHz);
            if (!e)
                return LR::failure("the material '" + m->name() + "': " + e.message());
            l.eps = complexPermittivity(e.value(), frequencyHz);
            l.mu = e.value().relativePermeability;
            if (std::abs(l.mu - 1.0) > 1e-12)
                out->warnings.push_back("'" + m->name() + "' is magnetic: the uniaxial model takes mu = 1 and the comparison shows the error");
        }
        if (!(l.thickness > 0.0))
            return LR::failure(std::string("the layer '") + role + "' needs a positive height");
        layers.push_back(l);
    }
    return layers;
}

std::vector<em::IsotropicLayer> symmetricStack(const std::vector<Layer>& l, int periods)
{
    // [layer1 / 2, layer2, layer1 / 2] per cell: a slab symmetric about its middle.
    std::vector<em::IsotropicLayer> stack;
    for (int n = 0; n < periods; ++n) {
        stack.push_back({0.5 * l[0].thickness, l[0].eps, Complex(l[0].mu, 0.0)});
        stack.push_back({l[1].thickness, l[1].eps, Complex(l[1].mu, 0.0)});
        stack.push_back({0.5 * l[0].thickness, l[0].eps, Complex(l[0].mu, 0.0)});
    }
    return stack;
}

Result<AnalysisOutcome> evaluateLayeredMedium(const FunctionalStructure& s, const Project& p, const Values& v, const ModelInfo& mi, JobContext*)
{
    const json& st = s.analysis.settings;
    const int periods = count(st, "periods", v, 10, 1, 400);
    const double angle = setting(st, "angleDeg", v, 45.0) * kPi / 180.0;
    const double maxRatio = setting(st, "maxScaleRatio", v, 0.1);
    if (!(angle >= 0.0) || !(angle < 0.5 * kPi))
        return R::failure("the angle of incidence must lie in [0, 90) degrees");
    if (!(maxRatio > 0.0) || maxRatio > 0.5)
        return R::failure("maxScaleRatio must lie in (0, 0.5]");
    // Geometry first (frequency-independent), then the materials at the middle of the valid band.
    auto geometry = layersOf(s, p, 0.0, nullptr);
    if (!geometry)
        return R::failure(geometry.message());
    AnalysisOutcome out;
    begin(out, mi);
    auto probe = layersOf(s, p, 1e9, &out);
    if (!probe)
        return R::failure(probe.message());
    const double a = probe.value()[0].thickness + probe.value()[1].thickness;
    const auto limitOf = [&](const std::vector<Layer>& l) {
        const double nmax = std::sqrt(std::max({1.0, l[0].eps.real() * l[0].mu, l[1].eps.real() * l[1].mu}));
        return maxRatio * em::kSpeedOfLight / (a * nmax);
    };
    const double reference = 0.5 * limitOf(probe.value());
    out.warnings.clear();
    auto layers = layersOf(s, p, reference, &out);
    if (!layers)
        return R::failure(layers.message());
    const std::vector<Layer>& l = layers.value();
    const double validTo = limitOf(l);
    const double d1 = l[0].thickness, d2 = l[1].thickness;
    const Complex epsPar = (d1 * l[0].eps + d2 * l[1].eps) / a;
    const Complex epsPerp = a / (d1 / l[0].eps + d2 / l[1].eps);
    const double thickness = a * periods;
    const std::vector<em::IsotropicLayer> stack = symmetricStack(l, periods);
    Values& x = out.metrics;
    x["cell.period"] = a;
    x["cell.periods"] = periods;
    x["effective.permittivityParallel"] = epsPar.real();
    x["effective.permittivityPerpendicular"] = epsPerp.real();
    x["effective.anisotropy"] = epsPar.real() / epsPerp.real();
    x["effective.validTo"] = validTo;
    x["effective.angleDeg"] = angle * 180.0 / kPi;
    // Explicit multilayer against the uniaxial slab, TE and TM, inside and beyond the validity.
    Curve teEx = curve("teExplicit", "TE transmittance, explicit layers", "Frequency", "Hz", "T", "");
    Curve teEf = curve("teEffective", "TE transmittance, uniaxial effective slab", "Frequency", "Hz", "T", "");
    Curve tmEx = curve("tmExplicit", "TM transmittance, explicit layers", "Frequency", "Hz", "T", "");
    Curve tmEf = curve("tmEffective", "TM transmittance, uniaxial effective slab", "Frequency", "Hz", "T", "");
    Curve tmIso = curve("tmIsotropic", "TM transmittance, isotropic slab of eps_parallel", "Frequency", "Hz", "T", "");
    Curve diff = curve("difference", "|t_explicit - t_effective| (TM)", "Frequency", "Hz", "|delta t|", "");
    double inTE = 0.0, inTM = 0.0, outTE = 0.0, outTM = 0.0, iso = 0.0;
    for (double f : linSpace(validTo / 40.0, 3.0 * validTo, 160)) {
        const auto a1 = em::obliqueStack(stack, f, angle, em::Polarization::TE);
        const auto b1 = em::uniaxialSlab(thickness, epsPar, epsPerp, f, angle, em::Polarization::TE);
        const auto a2 = em::obliqueStack(stack, f, angle, em::Polarization::TM);
        const auto b2 = em::uniaxialSlab(thickness, epsPar, epsPerp, f, angle, em::Polarization::TM);
        const auto c2 = em::uniaxialSlab(thickness, epsPar, epsPar, f, angle, em::Polarization::TM);
        const double dte = std::abs(a1.t - b1.t), dtm = std::abs(a2.t - b2.t);
        if (f <= validTo) {
            inTE = std::max(inTE, dte);
            inTM = std::max(inTM, dtm);
            iso = std::max(iso, std::abs(a2.t - c2.t));
        } else {
            outTE = std::max(outTE, dte);
            outTM = std::max(outTM, dtm);
        }
        for (auto [cv, y] : {std::pair{&teEx, a1.transmittance}, std::pair{&teEf, b1.transmittance}, std::pair{&tmEx, a2.transmittance},
                             std::pair{&tmEf, b2.transmittance}, std::pair{&tmIso, c2.transmittance}, std::pair{&diff, dtm}}) {
            cv->x.push_back(f);
            cv->y.push_back(y);
        }
    }
    x["compare.maxDifferenceTE"] = inTE;
    x["compare.maxDifferenceTM"] = inTM;
    x["compare.maxDifferenceBeyondTE"] = outTE;
    x["compare.maxDifferenceBeyondTM"] = outTM;
    x["compare.isotropicDifferenceTM"] = iso;
    // Oblique Bloch waves of the cell against the uniaxial dispersion.
    {
        const double f = 0.25 * validTo, k0 = 2 * kPi * f / em::kSpeedOfLight, s2 = std::pow(std::sin(angle), 2);
        const std::vector<em::IsotropicLayer> cell(stack.begin(), stack.begin() + 3);
        const double te = em::obliqueBloch(cell, f, angle, em::Polarization::TE).real();
        const double tm = em::obliqueBloch(cell, f, angle, em::Polarization::TM).real();
        const double teEff = k0 * std::sqrt(epsPar.real() - s2), tmEff = k0 * std::sqrt(epsPar.real() * (1.0 - s2 / epsPerp.real()));
        x["bloch.deviationTE"] = std::abs(te / teEff - 1.0);
        x["bloch.deviationTM"] = std::abs(tm / tmEff - 1.0);
    }
    // Retrieval from the explicit slab's reflection and transmission at normal incidence.
    std::vector<double> fr;
    std::vector<Complex> rr, tt;
    for (double f : linSpace(validTo / 40.0, validTo, 60)) {
        const auto r = em::obliqueStack(stack, f, 0.0, em::Polarization::TE);
        fr.push_back(f);
        rr.push_back(r.r);
        tt.push_back(r.t);
    }
    auto retrieved = em::retrieveSlab(fr, rr, tt, thickness, a);
    if (!retrieved)
        return R::failure(retrieved.message());
    double worst = 0.0, sign = 0.0;
    int flagged = 0;
    Curve reEps = curve("retrievedPermittivity", "Retrieved Re(eps) (normal incidence)", "Frequency", "Hz", "Re eps", "");
    Curve reMu = curve("retrievedPermeability", "Retrieved Re(mu) (normal incidence)", "Frequency", "Hz", "Re mu", "");
    for (const em::RetrievedPoint& pt : retrieved.value().points) {
        worst = std::max(worst, std::abs(pt.permittivity.real() / epsPar.real() - 1.0));
        flagged += (!pt.passive || pt.illConditioned || pt.nonlocal) ? 1 : 0;
        sign = std::max(sign, pt.signViolation);
        reEps.x.push_back(pt.frequencyHz);
        reEps.y.push_back(pt.permittivity.real());
        reMu.x.push_back(pt.frequencyHz);
        reMu.y.push_back(pt.permeability.real());
    }
    for (const std::string& w : retrieved.value().warnings)
        out.warnings.push_back("retrieval: " + w);
    x["retrieval.permittivity"] = retrieved.value().points.front().permittivity.real();
    x["retrieval.permeability"] = retrieved.value().points.front().permeability.real();
    x["retrieval.maxDeviation"] = worst;
    x["retrieval.flagged"] = flagged;
    x["retrieval.antiresonance"] = sign;
    out.curves = {std::move(teEx), std::move(teEf), std::move(tmEx), std::move(tmEf), std::move(tmIso), std::move(diff), std::move(reEps), std::move(reMu)};
    out.behaviour = "broadband";
    // The effective material.
    EffectiveMaterial e;
    e.name = s.name + " (effective)";
    e.kind = "homogenized";
    e.method = "em.layered.uniaxial-mixing";
    e.validToHz = validTo;
    e.scaleRatio = maxRatio;
    e.validity = "fields varying slowly over a period: the period at most " + std::to_string(maxRatio).substr(0, 4) +
                 " of the wavelength in the denser layer; uniaxial, optic axis normal to the layers";
    const auto tangent = [](Complex eps) { return std::max(0.0, -eps.imag() / eps.real()); };
    e.properties = {property("electromagnetic", "relativePermittivity", "parallel", epsPar.real(), "(d1 eps1 + d2 eps2) / a: fields along the layers"),
                    property("electromagnetic", "relativePermittivity", "perpendicular", epsPerp.real(), "a / (d1 / eps1 + d2 / eps2): fields across them"),
                    property("electromagnetic", "relativePermeability", "isotropic", 1.0, "non-magnetic layers")};
    if (tangent(epsPar) > 0.0 || tangent(epsPerp) > 0.0) {
        e.properties.push_back(property("electromagnetic", "lossTangent", "parallel", tangent(epsPar), "of the parallel permittivity, at the reference frequency"));
        e.properties.push_back(property("electromagnetic", "lossTangent", "perpendicular", tangent(epsPerp), ""));
    }
    e.limitations = {"the library holds isotropic values: a saved material carries one principal component",
                     "constituents taken at " + std::to_string(reference / 1e9).substr(0, 6) + " GHz"};
    e.derivation = derivationBase(s, kLayeredEffectiveMediumAnalysis, v);
    e.derivation["cell"] = {{"layers", json::array({{{"role", "layer1"}, {"object", l[0].object}, {"material", l[0].material}, {"thickness", d1},
                                                     {"permittivity", {l[0].eps.real(), l[0].eps.imag()}}},
                                                    {{"role", "layer2"}, {"object", l[1].object}, {"material", l[1].material}, {"thickness", d2},
                                                     {"permittivity", {l[1].eps.real(), l[1].eps.imag()}}}})},
                            {"period", a}};
    e.derivation["comparison"] = {{"maxDifferenceTE", inTE}, {"maxDifferenceTM", inTM}, {"retrievalDeviation", worst}, {"periods", periods}};
    out.details = {{"effectiveMaterial", e.toJson()},
                   {"representations",
                    {{"constituents", e.derivation["cell"]["layers"]},
                     {"unitCell", {{"period", a}, {"symmetric", "layer1 / 2, layer2, layer1 / 2"}}},
                     {"periodic", {{"periods", periods}, {"thickness", thickness}}},
                     {"effective", {{"permittivityParallel", epsPar.real()}, {"permittivityPerpendicular", epsPerp.real()}}},
                     {"equivalent", "a uniaxial slab of the same thickness"}}},
                   {"referenceFrequencyHz", reference}};
    out.discretization = {{"model", "exact transfer matrices (TE, TM) and the uniaxial slab"}, {"periods", periods}, {"frequencies", 160}};
    return out;
}

// --- split-ring array -------------------------------------------------------------------------

Result<AnalysisOutcome> evaluateSplitRing(const FunctionalStructure& s, const Project& p, const Values& v, const ModelInfo& mi, JobContext*)
{
    const json& st = s.analysis.settings;
    em::SplitRingSpec r;
    r.radius = setting(st, "radius", v, 2e-3);
    r.width = setting(st, "width", v, 1e-3);
    r.gap = setting(st, "gap", v, 5e-5);
    r.metalThickness = setting(st, "metalThickness", v, 35e-6);
    r.latticeSpacing = setting(st, "latticeSpacing", v, 5.5e-3);
    r.substratePermittivity = setting(st, "substratePermittivity", v, 10.2);
    r.substrateLossTangent = setting(st, "substrateLossTangent", v, 0.0023);
    r.conductivity = setting(st, "conductivity", v, 5.8e7);
    const double maxRatio = setting(st, "maxScaleRatio", v, 0.2);
    if (const Material* m = materialOf(s, p, "ring")) {
        auto e = em::emMaterialOf(*m, 1e9);
        if (e && e.value().conductivity > 0.0)
            r.conductivity = e.value().conductivity;
    }
    auto model = em::splitRingModel(r);
    if (!model)
        return R::failure(model.message());
    const em::SplitRingModel& m = model.value();
    AnalysisOutcome out;
    begin(out, mi);
    out.behaviour = "resonant";
    Values& x = out.metrics;
    x["srr.resonance"] = m.resonanceHz;
    x["srr.magneticPlasma"] = m.magneticPlasmaHz;
    x["srr.fillingFactor"] = m.fillingFactor;
    x["srr.quality"] = m.quality;
    x["srr.qConductor"] = m.qConductor;
    x["srr.qDielectric"] = m.qDielectric;
    x["srr.inductance"] = m.inductance;
    x["srr.capacitance"] = m.capacitance;
    x["srr.resistance"] = m.resistance;
    x["srr.cellOverWavelength"] = m.cellOverWavelength;
    Curve re = curve("permeabilityReal", "Re mu_eff", "Frequency", "Hz", "Re mu", "");
    Curve im = curve("permeabilityImag", "Im mu_eff", "Frequency", "Hz", "Im mu", "");
    double lowest = kInf, at = 0.0, negLo = 0.0, negHi = 0.0, maxIm = -kInf;
    for (double f : linSpace(0.3 * m.resonanceHz, 1.8 * m.resonanceHz, 3000)) {
        const Complex mu = em::splitRingPermeability(m, f);
        if (mu.real() < lowest) {
            lowest = mu.real();
            at = f;
        }
        if (mu.real() < 0.0) {
            if (negLo == 0.0)
                negLo = f;
            negHi = f;
        }
        maxIm = std::max(maxIm, mu.imag());
        re.x.push_back(f);
        re.y.push_back(mu.real());
        im.x.push_back(f);
        im.y.push_back(mu.imag());
    }
    out.curves = {std::move(re), std::move(im)};
    x["srr.minPermeability"] = lowest;
    x["srr.frequencyOfMinimum"] = at;
    x["srr.negativeBandLower"] = negLo;
    x["srr.negativeBandUpper"] = negHi;
    x["srr.passive"] = maxIm <= 0.0 ? 1.0 : 0.0;
    x["srr.kramersKronigResidual"] =
        em::splitRingKramersKronigResidual(m, {0.5 * m.resonanceHz, 0.9 * m.resonanceHz, 1.1 * m.resonanceHz, 1.5 * m.resonanceHz});
    if (negLo == 0.0)
        out.warnings.push_back("the losses are too high for Re mu to become negative");
    Mode mode("magnetic resonance", m.resonanceHz, m.quality, {{"fillingFactor", m.fillingFactor}, {"magneticPlasmaHz", m.magneticPlasmaHz}});
    mode.lossQ["conductor"] = m.qConductor;
    if (std::isfinite(m.qDielectric))
        mode.lossQ["dielectric"] = m.qDielectric;
    out.modes.push_back(mode);
    out.details = {{"representations",
                    {{"constituents", {{"conductivity", r.conductivity}, {"substratePermittivity", r.substratePermittivity},
                                       {"substrateLossTangent", r.substrateLossTangent}}},
                     {"unitCell", {{"radius", r.radius}, {"width", r.width}, {"gap", r.gap}, {"metalThickness", r.metalThickness}}},
                     {"periodic", {{"latticeSpacing", r.latticeSpacing}}},
                     {"effective", "Lorentzian permeability mu(w) = 1 - F w^2 / (w^2 - w0^2 - j w Gamma)"},
                     {"equivalent", {{"L", m.inductance}, {"C", m.capacitance}, {"R", m.resistance}}}}},
                   {"approximation", "a quasi-static circuit model; not compared with a full-wave calculation"}};
    if (m.cellOverWavelength > maxRatio) {
        out.warnings.push_back("the cell is " + std::to_string(m.cellOverWavelength).substr(0, 5) +
                               " of the wavelength at the resonance: too large for an effective medium; no effective material is reported");
    } else {
        EffectiveMaterial e;
        e.name = s.name + " (effective)";
        e.kind = "equivalent-model";
        e.method = em::kSplitRingModelId;
        e.validToHz = 0.8 * m.resonanceHz;
        e.scaleRatio = m.cellOverWavelength * 0.8;
        e.validity = "below 0.8 of the magnetic resonance (positive, slowly varying permeability); the cell small against the wavelength; "
                     "magnetic field normal to the rings";
        EffectiveProperty mu = property("electromagnetic", "relativePermeability", "normal to the rings", em::splitRingPermeability(m, 0.05 * m.resonanceHz).real(),
                                        "the Lorentzian circuit model, real part");
        for (double f : linSpace(0.05 * m.resonanceHz, e.validToHz, 30))
            mu.frequencyTable.emplace_back(f, em::splitRingPermeability(m, f).real());
        e.properties = {mu, property("electromagnetic", "relativePermittivity", "isotropic", setting(st, "hostPermittivity", v, 1.0),
                                     "the host's: the rings' electric response and the substrate's volume share are not modelled")};
        e.limitations = {"a circuit approximation, not a full-wave result", "the negative band (f0 to f_mp) is reported, not stored: the library holds positive permeability",
                         "the magnetic loss (Im mu) is not stored"};
        e.derivation = derivationBase(s, kSplitRingArrayAnalysis, v);
        e.derivation["cell"] = out.details["representations"]["unitCell"];
        e.derivation["cell"]["latticeSpacing"] = r.latticeSpacing;
        out.details["effectiveMaterial"] = e.toJson();
    }
    out.discretization = {{"model", "quasi-static circuit"}};
    return out;
}

// --- layered thermal medium -------------------------------------------------------------------

Result<AnalysisOutcome> evaluateLayeredConductivity(const FunctionalStructure& s, const Project& p, const Values& v, const ModelInfo& mi, JobContext*)
{
    const json& st = s.analysis.settings;
    const int periods = count(st, "periods", v, 5, 1, 100);
    const int perLayer = count(st, "cellsPerLayer", v, 2, 1, 20);
    std::vector<thermal::ConductiveLayer> cell;
    std::vector<thermal::ThermalMaterial> mats;
    json layers = json::array();
    for (const char* role : {"layer1", "layer2"}) {
        const SceneObject* o = bodyOf(s, p, role);
        if (!o || !o->state.geometry || o->state.geometry->kindId() != "box")
            return R::failure(std::string("the body '") + role + "' must be a box (its height is the layer's thickness)");
        const Material* m = materialOf(s, p, role);
        if (!m)
            return R::failure(std::string("the body '") + role + "' has no material");
        auto t = thermal::thermalMaterial(*m);
        if (!t)
            return R::failure("the material '" + m->name() + "': " + t.message());
        cell.push_back({o->state.geometry->parameter("height"), t.value().conductivity});
        mats.push_back(t.value());
        layers.push_back({{"role", role}, {"object", o->id().str()}, {"material", m->name()}, {"thickness", cell.back().thickness},
                          {"conductivity", t.value().conductivity}, {"density", t.value().density}, {"specificHeat", t.value().specificHeat}});
    }
    auto eff = thermal::layeredConductivity(cell);
    if (!eff)
        return R::failure(eff.message());
    auto explicitK = thermal::explicitLayeredConductivity(cell, periods, perLayer);
    if (!explicitK)
        return R::failure(explicitK.message());
    const double a = eff.value().period;
    const double rho = (cell[0].thickness * mats[0].density + cell[1].thickness * mats[1].density) / a;
    const double cp = (cell[0].thickness * mats[0].density * mats[0].specificHeat + cell[1].thickness * mats[1].density * mats[1].specificHeat) / (rho * a);
    AnalysisOutcome out;
    begin(out, mi);
    out.behaviour = "non-resonant";
    Values& x = out.metrics;
    x["thermal.parallel"] = eff.value().parallel;
    x["thermal.perpendicular"] = eff.value().perpendicular;
    x["thermal.anisotropy"] = eff.value().anisotropy;
    x["thermal.period"] = a;
    x["explicit.parallel"] = explicitK.value().parallel;
    x["explicit.perpendicular"] = explicitK.value().perpendicular;
    x["explicit.deviationParallel"] = std::abs(explicitK.value().parallel / eff.value().parallel - 1.0);
    x["explicit.deviationPerpendicular"] = std::abs(explicitK.value().perpendicular / eff.value().perpendicular - 1.0);
    x["explicit.balanceResidual"] = explicitK.value().balanceResidual;
    x["effective.density"] = rho;
    x["effective.specificHeat"] = cp;
    x["effective.diffusivityPerpendicular"] = eff.value().perpendicular / (rho * cp);
    EffectiveMaterial e;
    e.name = s.name + " (effective)";
    e.kind = "homogenized";
    e.method = "thermal.layered.series-parallel";
    e.scaleRatio = 1.0 / periods;
    e.validity = "steady conduction, or transients slower than the diffusion time of one period; gradients over lengths much larger than the "
                 "period; no interface resistance";
    e.properties = {property("thermal", "thermalConductivity", "perpendicular", eff.value().perpendicular, "series: a / sum(d_i / k_i)"),
                    property("thermal", "thermalConductivity", "parallel", eff.value().parallel, "parallel: sum(d_i k_i) / a"),
                    property("thermal", "density", "isotropic", rho, "thickness-weighted"),
                    property("thermal", "specificHeat", "isotropic", cp, "mass-weighted")};
    e.limitations = {"the library holds isotropic values: a saved material carries one principal component",
                     "thermal expansion of bonded layers is not homogenized"};
    e.derivation = derivationBase(s, kLayeredConductivityAnalysis, v);
    e.derivation["cell"] = {{"layers", layers}, {"period", a}};
    e.derivation["comparison"] = {{"explicitPerpendicular", explicitK.value().perpendicular}, {"explicitParallel", explicitK.value().parallel},
                                  {"periods", periods}, {"cellsPerLayer", perLayer}, {"solver", "thermal.conduction.fv"}};
    out.details = {{"effectiveMaterial", e.toJson()},
                   {"representations",
                    {{"constituents", layers},
                     {"unitCell", {{"period", a}}},
                     {"periodic", {{"periods", periods}}},
                     {"effective", {{"parallel", eff.value().parallel}, {"perpendicular", eff.value().perpendicular}}},
                     {"equivalent", "series and parallel thermal resistances"}}}};
    out.discretization = {{"model", "closed form and finite volumes on the explicit layers"}, {"periods", periods}, {"cellsPerLayer", perLayer}};
    return out;
}

}  // namespace

void registerMetamaterialAnalyses(AnalysisRegistry& registry)
{
    {
        AnalysisKind k;
        k.id = kMassInMassAnalysis;
        k.title = "Locally resonant lattice (mass-in-mass)";
        k.description = "Bloch dispersion, the locally resonant band gap, the negative effective dynamic mass and a finite chain's attenuation";
        k.domains = {"mechanical"};
        k.category = Category::EffectiveMedium;
        k.mechanism = "each cell carries an internal resonator: near its frequency the resonator moves against the host and the cell's effective "
                      "mass m1 + m2 w2^2 / (w2^2 - w^2) turns negative, so waves cannot propagate - a gap set by the resonator, not by the spacing";
        k.models = {model("mechanics.mass-in-mass.bloch", "Mass-in-mass lattice: closed form, cell Bloch eigenproblem, finite chain", "analytical",
                          {"lumped masses and springs; one dimension", "structural loss of the resonator spring (if stated)"},
                          "a lumped lattice: the parts much smaller than the elastic wavelengths inside them", 0.01)};
        k.metrics = {{"lattice.localResonance", "Local resonance", "Hz", "sqrt(k2 / m2) / 2 pi"},
                     {"lattice.hostZoneEdge", "Host lattice zone edge", "Hz", "sqrt(4 k1 / m1) / 2 pi"},
                     {"lattice.massRatio", "Mass ratio", "", "m2 / m1"},
                     {"gap.lower", "Gap lower edge", "Hz", "acoustic branch at q a = pi"},
                     {"gap.upper", "Gap upper edge", "Hz", "optical branch at q = 0"},
                     {"gap.relativeWidth", "Relative gap width", "", ""},
                     {"gap.negativeMassLower", "Negative mass from", "Hz", "effective-medium prediction"},
                     {"gap.negativeMassUpper", "Negative mass to", "Hz", "w2 sqrt(1 + m2 / m1)"},
                     {"gap.attenuationPerCell", "Attenuation at the gap centre", "Np", "Im(q) a"},
                     {"lattice.effectiveMassAtGapCentre", "Effective mass at the gap centre", "", "Re m_eff / (m1 + m2)"},
                     {"lattice.longWaveSpeed", "Long-wave speed (effective medium)", "m/s", "a sqrt(k1 / (m1 + m2))"},
                     {"lattice.longWaveSpeedBloch", "Long-wave speed (dispersion)", "m/s", ""},
                     {"bloch.maxDeviation", "Numerical Bloch vs closed form", "", "largest relative difference"},
                     {"chain.cells", "Cells of the finite chain", "", ""},
                     {"chain.transmissionAtGapCentre", "Chain transmission at the gap centre", "", "|u_N / u_1|"},
                     {"chain.decayPerCell", "Chain decay per cell", "Np", "between host 2 and the middle host"}};
        k.evaluate = evaluateMassInMass;
        registry.add(std::move(k));
    }
    {
        AnalysisKind k;
        k.id = kResonatorDuctAnalysis;
        k.title = "Duct with Helmholtz resonators";
        k.description = "Bloch dispersion, the hybridization and Bragg gaps, the effective bulk modulus and a finite array's attenuation";
        k.domains = {"acoustic"};
        k.category = Category::EffectiveMedium;
        k.mechanism = "side-branch resonators add compliance to the duct: below their frequency they slow sound down, just above it the "
                      "effective bulk modulus is negative and the array reflects; the spacing adds a Bragg gap at c / 2a";
        k.models = {model(manipulators::kResonatorDuctModelId, "Plane-wave duct with lumped shunt Helmholtz resonators", "reduced",
                          {"plane waves in the duct", "lumped Helmholtz resonators with end corrections and viscous loss", "no radiation into the duct beyond the end correction"},
                          "below the duct's first cross mode; resonators small against the wavelength", 0.05)};
        k.metrics = {{"duct.resonatorF0", "Resonator frequency", "Hz", "lumped, without radiation"},
                     {"duct.braggFrequency", "Bragg frequency", "Hz", "c / 2a"},
                     {"duct.volumeRatio", "Volume ratio", "", "V / (S a)"},
                     {"duct.negativeModulusLower", "Negative modulus from", "Hz", ""},
                     {"duct.negativeModulusUpper", "Negative modulus to", "Hz", "f0 sqrt(1 + V / (S a))"},
                     {"gap.count", "Stop bands", "", ""},
                     {"gap1.lower", "Hybridization gap lower edge", "Hz", ""},
                     {"gap1.upper", "Hybridization gap upper edge", "Hz", ""},
                     {"gap1.relativeWidth", "Hybridization gap relative width", "", ""},
                     {"gap1.blochAttenuationDbPerCell", "Bloch attenuation at the gap centre", "dB", "per cell, 8.686 Im(q) a"},
                     {"gap1.measuredAttenuationDbPerCell", "Finite-array attenuation", "dB", "per cell, from N and N + 4 cells"},
                     {"gap1.transmissionDb", "Transmission at the gap centre", "dB", ""},
                     {"gap2.lower", "Bragg gap lower edge", "Hz", ""},
                     {"gap2.upper", "Bragg gap upper edge", "Hz", ""},
                     {"duct.lowFrequencySpeed", "Low-frequency speed (effective medium)", "m/s", "c / sqrt(1 + V / (S a))"},
                     {"duct.lowFrequencySpeedBloch", "Low-frequency speed (Bloch)", "m/s", "w / Re q at 0.02 f0"}};
        k.roles = {{"resonator", "body", "the Helmholtz resonator of each cell (resonator.helmholtz)", true}};
        k.evaluate = evaluateResonatorDuct;
        registry.add(std::move(k));
    }
    {
        AnalysisKind k;
        k.id = kLayeredEffectiveMediumAnalysis;
        k.title = "Layered effective medium (anisotropic)";
        k.description = "Uniaxial homogenization of a two-layer cell compared with the explicit multilayer at oblique incidence (TE and TM), "
                        "oblique Bloch waves, and parameter retrieval";
        k.domains = {"electromagnetic"};
        k.category = Category::EffectiveMedium;
        k.mechanism = "fields along the layers see the layers in parallel (eps_par, arithmetic mean), fields across them in series (eps_perp, "
                      "harmonic mean): the organization makes an isotropic pair of materials anisotropic; TM waves at an angle feel both";
        k.models = {model("em.layered.uniaxial-mixing", "Quasi-static uniaxial mixing against exact transfer matrices", "analytical",
                          {"isotropic layers; non-magnetic for the effective model", "plane waves between vacuum half-spaces",
                           "the explicit slab symmetric: layer1 / 2, layer2, layer1 / 2 per period"},
                          "period below maxScaleRatio of the wavelength in the denser layer", 0.05)};
        k.metrics = {{"cell.period", "Period", "m", ""},
                     {"cell.periods", "Periods", "", ""},
                     {"effective.permittivityParallel", "eps parallel", "", "(d1 eps1 + d2 eps2) / a"},
                     {"effective.permittivityPerpendicular", "eps perpendicular", "", "a / (d1 / eps1 + d2 / eps2)"},
                     {"effective.anisotropy", "Anisotropy", "", "eps_par / eps_perp"},
                     {"effective.validTo", "Valid up to", "Hz", ""},
                     {"effective.angleDeg", "Angle of incidence", "deg", ""},
                     {"compare.maxDifferenceTE", "Explicit vs effective (TE)", "", "max |delta t| inside the validity"},
                     {"compare.maxDifferenceTM", "Explicit vs effective (TM)", "", "max |delta t| inside the validity"},
                     {"compare.maxDifferenceBeyondTE", "Explicit vs effective beyond (TE)", "", "up to 3 x the limit"},
                     {"compare.maxDifferenceBeyondTM", "Explicit vs effective beyond (TM)", "", ""},
                     {"compare.isotropicDifferenceTM", "Explicit vs isotropic eps_par (TM)", "", "what ignoring the anisotropy costs"},
                     {"bloch.deviationTE", "Oblique Bloch vs uniaxial (TE)", "", "at a quarter of the limit"},
                     {"bloch.deviationTM", "Oblique Bloch vs uniaxial (TM)", "", ""},
                     {"retrieval.permittivity", "Retrieved eps (lowest frequency)", "", "normal incidence"},
                     {"retrieval.permeability", "Retrieved mu (lowest frequency)", "", ""},
                     {"retrieval.maxDeviation", "Retrieved vs eps parallel", "", "largest relative difference inside the validity"},
                     {"retrieval.flagged", "Flagged retrieval points", "", "non-passive, ill-conditioned or non-local"},
                     {"retrieval.antiresonance", "Largest Im eps or Im mu > 0", "", "the antiresonance of a periodic cell: spatial dispersion"}};
        k.roles = {{"layer1", "body", "a box whose height is the first layer's thickness", true},
                   {"layer2", "body", "a box whose height is the second layer's thickness", true}};
        k.evaluate = evaluateLayeredMedium;
        registry.add(std::move(k));
    }
    {
        AnalysisKind k;
        k.id = kSplitRingArrayAnalysis;
        k.title = "Split-ring array (circuit model)";
        k.description = "Magnetic resonance, Lorentzian effective permeability, its negative band, causality and passivity of a split-ring array";
        k.domains = {"electromagnetic"};
        k.category = Category::EffectiveMedium;
        k.mechanism = "a magnetic field through the ring drives a current around it; the gap's capacitance and the ring's inductance resonate, "
                      "and above the resonance the induced moment opposes the field: Re mu < 0 up to f0 / sqrt(1 - F)";
        k.models = {model(em::kSplitRingModelId, "Quasi-static LC circuit of the ring (Pendry)", "analytical",
                          {"ring inductance of an equivalent round wire", "gap and fringing capacitance; substrate as a mean permittivity",
                           "skin-effect conductor loss and the substrate's dielectric loss", "an approximation: not compared with a full-wave calculation"},
                          "cell below maxScaleRatio of the wavelength at the resonance", 0.001)};
        k.metrics = {{"srr.resonance", "Magnetic resonance", "Hz", "1 / (2 pi sqrt(L C))"},
                     {"srr.magneticPlasma", "Magnetic plasma frequency", "Hz", "f0 / sqrt(1 - F)"},
                     {"srr.fillingFactor", "Filling factor", "", "pi r^2 / a^2"},
                     {"srr.quality", "Quality factor", "", ""},
                     {"srr.qConductor", "Conductor Q", "", "w0 L / R"},
                     {"srr.qDielectric", "Dielectric Q", "", ""},
                     {"srr.inductance", "Ring inductance", "H", ""},
                     {"srr.capacitance", "Gap capacitance", "F", ""},
                     {"srr.resistance", "Ring resistance", "Ohm", "at the resonance"},
                     {"srr.cellOverWavelength", "Cell / wavelength", "", "at the resonance"},
                     {"srr.minPermeability", "Minimum Re mu", "", ""},
                     {"srr.frequencyOfMinimum", "Frequency of the minimum", "Hz", ""},
                     {"srr.negativeBandLower", "Re mu < 0 from", "Hz", ""},
                     {"srr.negativeBandUpper", "Re mu < 0 to", "Hz", ""},
                     {"srr.passive", "Passive", "", "1 when Im mu <= 0 everywhere"},
                     {"srr.kramersKronigResidual", "Kramers-Kronig residual", "", "largest |Re mu (KK) - Re mu|"}};
        k.roles = {{"ring", "body", "optional: an object whose material gives the ring's conductivity", false}};
        k.evaluate = evaluateSplitRing;
        registry.add(std::move(k));
    }
    {
        AnalysisKind k;
        k.id = kLayeredConductivityAnalysis;
        k.title = "Layered thermal medium";
        k.description = "Effective conductivity tensor of a two-layer cell, checked by finite volumes on the explicit layers";
        k.domains = {"thermal"};
        k.category = Category::EffectiveMedium;
        k.mechanism = "heat along the layers flows through them in parallel, across them in series: the effective conductivity is anisotropic, "
                      "dominated by the better conductor along the layers and by the worse one across them";
        k.models = {model("thermal.layered.series-parallel", "Series and parallel conductances against finite volumes", "analytical",
                          {"steady conduction", "perfect thermal contact between layers", "isotropic constituents"},
                          "lengths much larger than the period", 0.05)};
        k.metrics = {{"thermal.parallel", "k parallel", "W/(m K)", "sum(d_i k_i) / a"},
                     {"thermal.perpendicular", "k perpendicular", "W/(m K)", "a / sum(d_i / k_i)"},
                     {"thermal.anisotropy", "Anisotropy", "", "k_par / k_perp"},
                     {"thermal.period", "Period", "m", ""},
                     {"explicit.parallel", "Finite-volume k parallel", "W/(m K)", ""},
                     {"explicit.perpendicular", "Finite-volume k perpendicular", "W/(m K)", ""},
                     {"explicit.deviationParallel", "Closed form vs finite volumes (parallel)", "", "relative"},
                     {"explicit.deviationPerpendicular", "Closed form vs finite volumes (perpendicular)", "", "relative"},
                     {"explicit.balanceResidual", "Energy balance residual", "W", ""},
                     {"effective.density", "Effective density", "kg/m^3", "thickness-weighted"},
                     {"effective.specificHeat", "Effective specific heat", "J/(kg K)", "mass-weighted"},
                     {"effective.diffusivityPerpendicular", "Diffusivity across the layers", "m^2/s", ""}};
        k.roles = {{"layer1", "body", "a box whose height is the first layer's thickness, with a thermal material", true},
                   {"layer2", "body", "a box whose height is the second layer's thickness, with a thermal material", true}};
        k.evaluate = evaluateLayeredConductivity;
        registry.add(std::move(k));
    }
}

}  // namespace resonant::coupling
