#include "coupling/ResonanceAnalyses.h"

#include "dsp/Resonance.h"
#include "manipulators/Helmholtz.h"
#include "materials/Material.h"
#include "mechanics/ElasticMaterial.h"
#include "mechanics/LumpedResonators.h"
#include "oscillators/LumpedNetwork.h"
#include "project/Project.h"
#include "simulation/Job.h"

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

const SceneObject* body(const FunctionalStructure& s, const Project& p, const std::string& role)
{
    const Body* b = s.body(role);
    return b ? p.scene.find(Id(b->objectId)) : nullptr;
}

const Material* materialOf(const FunctionalStructure& s, const Project& p, const std::string& role)
{
    if (const Region* r = s.region(role))
        return p.materials.find(Id(r->materialId));
    if (const SceneObject* o = body(s, p, role); o && !o->state.materialId.isNull())
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

Result<manipulators::HelmholtzSpec> helmholtzOf(const FunctionalStructure& s, const Project& p, const std::string& role, const Values& v)
{
    using HR = Result<manipulators::HelmholtzSpec>;
    const SceneObject* o = body(s, p, role);
    if (!o || !o->state.geometry || o->state.geometry->kindId() != "resonator.helmholtz")
        return HR::failure("the body '" + role + "' must be a Helmholtz resonator (resonator.helmholtz)");
    manipulators::HelmholtzSpec spec = manipulators::helmholtzSpecOf(*o->state.geometry);
    spec.density = setting(s.environment, "density", v, 1.204);
    spec.soundSpeed = setting(s.environment, "soundSpeed", v, 343.0);
    spec.viscosity = setting(s.environment, "viscosity", v, 1.81e-5);
    spec.flanged = s.analysis.settings.value("flanged", true);
    return spec;
}

Result<mechanics::CantileverSpec> cantileverOf(const FunctionalStructure& s, const Project& p, const std::string& role, const Values& v)
{
    using CR = Result<mechanics::CantileverSpec>;
    const SceneObject* o = body(s, p, role);
    if (!o || !o->state.geometry || o->state.geometry->kindId() != "box")
        return CR::failure("the body '" + role + "' must be a box (length along X = width, thickness along Z = height)");
    const Material* m = materialOf(s, p, role);
    if (!m)
        return CR::failure("the body '" + role + "' has no material");
    auto e = mechanics::elasticMaterialOf(*m);
    if (!e)
        return CR::failure("'" + m->name() + "': " + e.message());
    mechanics::CantileverSpec c;
    c.length = o->state.geometry->parameter("width");
    c.width = o->state.geometry->parameter("depth");
    c.thickness = o->state.geometry->parameter("height");
    c.youngsModulus = e.value().youngsModulus;
    c.density = e.value().density;
    c.lossFactor = setting(s.analysis.settings, "lossFactor", v, e.value().lossFactorStated ? e.value().lossFactor : 0.0);
    return c;
}

Mode modeOf(const oscillators::NetworkMode& n, std::string label)
{
    Mode m(std::move(label), n.frequencyHz, n.quality);
    m.nature = "natural";
    m.decayRate = n.decayRate;
    m.bandwidthHz = n.decayRate > 0.0 ? n.decayRate / kPi : 0.0;
    m.lossQ = n.qualityByMechanism;
    json participation = json::array();
    for (double x : n.participation)
        participation.push_back(x);
    m.details = {{"participation", participation}, {"symmetry", n.symmetry}, {"undampedHz", n.undampedHz}};
    return m;
}

// --- Helmholtz resonator --------------------------------------------------------------------

Result<AnalysisOutcome> evaluateHelmholtz(const FunctionalStructure& s, const Project& p, const Values& v, const ModelInfo& mi, JobContext*)
{
    auto spec = helmholtzOf(s, p, "resonator", v);
    if (!spec)
        return R::failure(spec.message());
    auto r = manipulators::analyzeHelmholtz(spec.value());
    if (!r)
        return R::failure(r.message());
    const auto& h = r.value();
    AnalysisOutcome out;
    out.solver = mi.id;
    out.assumptions = mi.assumptions;
    out.warnings = h.warnings;
    out.behaviour = "resonant";
    Values& x = out.metrics;
    x["helmholtz.volume"] = h.volume;
    x["helmholtz.effectiveLength"] = h.effectiveLength;
    x["helmholtz.outerCorrection"] = h.outerCorrection;
    x["helmholtz.innerCorrection"] = h.innerCorrection;
    x["helmholtz.neckKa"] = h.neckKa;
    x["helmholtz.cavityKSize"] = h.cavityKSize;
    if (mi.id == manipulators::kHelmholtzDuctModelId) {
        manipulators::HelmholtzSpec plain = spec.value();
        plain.innerCorrection = false;
        auto d = manipulators::steppedDuctResonance(plain);
        if (!d)
            return R::failure(d.message());
        x["helmholtz.f0"] = d.value().frequencyHz;
        x["helmholtz.qRadiation"] = d.value().quality;
        x["helmholtz.Q"] = d.value().quality;
        x["helmholtz.bandwidth"] = d.value().frequencyHz / d.value().quality;
        x["helmholtz.lumpedDeviation"] = d.value().frequencyHz / h.frequencyHz - 1.0;
        Mode m("Helmholtz (plane-wave)", d.value().frequencyHz, d.value().quality);
        m.lossQ = {{"radiation", d.value().quality}};
        m.bandwidthHz = d.value().frequencyHz / d.value().quality;
        out.modes.push_back(m);
        out.warnings.push_back("the plane-wave model has no inner end correction and no viscous loss");
        out.discretization = {{"model", "exact transfer matrices of uniform ducts"}};
        return out;
    }
    x["helmholtz.f0"] = h.frequencyHz;
    x["helmholtz.Q"] = h.quality;
    x["helmholtz.qRadiation"] = h.qRadiation;
    x["helmholtz.qViscous"] = h.qViscous;
    x["helmholtz.bandwidth"] = h.bandwidthHz;
    x["helmholtz.pressureGain"] = h.pressureGainAtResonance;
    x["helmholtz.storedEnergyPerPa2"] = h.storedEnergyPerPa2;
    Mode natural("Helmholtz", h.frequencyHz, h.quality);
    natural.lossQ = {{"radiation", h.qRadiation}, {"viscous (neck)", h.qViscous}};
    natural.bandwidthHz = h.bandwidthHz;
    natural.decayRate = kPi * h.bandwidthHz;
    out.modes.push_back(natural);
    // The response: cavity pressure gain and impedance; its peak is a driven peak, not the natural frequency.
    Curve gain = curve("gain", "Cavity pressure / incident pressure", "Frequency", "Hz", "|p_c / p_i|", "");
    Curve impedance = curve("impedance", "|Z| at the mouth (lumped)", "Frequency", "Hz", "|Z|", "Pa s/m^3");
    double bestF = 0.0, best = 0.0;
    for (double f : linSpace(0.5 * h.frequencyHz, 1.5 * h.frequencyHz, 801)) {
        const double g = manipulators::helmholtzPressureGain(spec.value(), f);
        gain.x.push_back(f);
        gain.y.push_back(g);
        impedance.x.push_back(f);
        impedance.y.push_back(std::abs(manipulators::helmholtzImpedance(spec.value(), f)));
        if (g > best) {
            best = g;
            bestF = f;
        }
    }
    Mode peak("pressure-gain peak", bestF, 0.0);
    peak.nature = "driven-peak";
    peak.details = {{"gain", best}, {"offsetFromNaturalHz", bestF - h.frequencyHz}};
    out.modes.push_back(peak);
    out.curves = {gain, impedance};
    out.details = {{"inertance", h.inertance}, {"compliance", h.compliance}, {"radiationResistance", h.radiationResistance},
                   {"viscousResistance", h.viscousResistance}};
    out.discretization = {{"model", "lumped elements"}};
    return out;
}

// --- cantilever as a resonator -------------------------------------------------------------

Result<AnalysisOutcome> evaluateCantilever(const FunctionalStructure& s, const Project& p, const Values& v, const ModelInfo& mi, JobContext* context)
{
    auto c = cantileverOf(s, p, "resonator", v);
    if (!c)
        return R::failure(c.message());
    AnalysisOutcome out;
    out.solver = mi.id;
    out.assumptions = mi.assumptions;
    out.behaviour = "resonant";
    Values& x = out.metrics;
    for (int n = 1; n <= 3; ++n) {
        auto m = mechanics::cantileverMode(c.value(), n);
        if (!m)
            return R::failure(m.message());
        Mode mode("bending " + std::to_string(n), m.value().frequencyHz, m.value().quality);
        mode.lossQ = {{"structural", m.value().quality}};
        mode.bandwidthHz = std::isfinite(m.value().quality) ? m.value().frequencyHz / m.value().quality : 0.0;
        mode.decayRate = kPi * mode.bandwidthHz;
        mode.details = {{"tipMass", m.value().tipMass}, {"tipStiffness", m.value().tipStiffness}};
        out.modes.push_back(mode);
        x["cantilever.f" + std::to_string(n)] = m.value().frequencyHz;
        if (n == 1) {
            x["cantilever.tipMass"] = m.value().tipMass;
            x["cantilever.tipStiffness"] = m.value().tipStiffness;
            x["cantilever.Q"] = m.value().quality;
            x["cantilever.bandwidth"] = mode.bandwidthHz;
            x["cantilever.kineticEnergyPerTip2"] = m.value().kineticEnergyPerTip2;
            x["cantilever.beamMass"] = c.value().density * c.value().length * c.value().width * c.value().thickness;
        }
    }
    // The first mode as a single-degree-of-freedom resonator: frequency response and ring-down.
    auto net = mechanics::coupledCantilevers(c.value(), c.value(), 0.0);
    if (!net)
        return R::failure(net.message());
    oscillators::Network one;
    one.names = {"tip"};
    one.masses = {net.value().masses[0]};
    one.springs = {{"bending", 0, -1, net.value().springs[0].k}};
    if (!net.value().dampers.empty())
        one.dampers = {{"structural", 0, -1, net.value().dampers[0].c}};
    const double f1 = x["cantilever.f1"];
    std::vector<double> freqs = linSpace(0.8 * f1, 1.2 * f1, 401);
    auto resp = oscillators::drivenResponse(one, {{1.0, 0.0}}, freqs);
    if (!resp)
        return R::failure(resp.message());
    Curve fr = curve("response", "Tip displacement per newton at the tip (mode 1)", "Frequency", "Hz", "|x / F|", "m/N");
    for (const auto& pt : resp.value()) {
        fr.x.push_back(pt.frequencyHz);
        fr.y.push_back(std::abs(pt.displacement[0]));
    }
    out.curves.push_back(fr);
    if (!one.dampers.empty()) {
        oscillators::TransientOptions o;
        const double tau = 2.0 * one.masses[0] / one.dampers[0].c;
        o.duration = std::min(3.0 * tau, 4000.0 / f1);
        o.x0 = {1e-3};
        o.samples = 6000;
        auto ring = oscillators::integrate(one, o, context);
        if (!ring)
            return R::failure(ring.message());
        Curve energy = curve("ringdown", "Ring-down energy (1 mm initial tip deflection)", "Time", "s", "E", "J");
        for (size_t i = 0; i < ring.value().time.size(); i += 10) {
            energy.x.push_back(ring.value().time[i]);
            energy.y.push_back(ring.value().energy[0][i]);
        }
        out.curves.push_back(energy);
        dsp::PencilOptions po;
        po.minHz = 0.5 * f1;
        po.maxHz = 2.0 * f1;
        auto id = dsp::identifyByMatrixPencil(ring.value().displacement[0], ring.value().time[1] - ring.value().time[0], po);
        if (id && !id.value().resonances.empty()) {
            x["cantilever.ringDownQ"] = id.value().resonances[0].quality;
            Mode identified("bending 1 (ring-down)", id.value().resonances[0].frequencyHz, id.value().resonances[0].quality);
            identified.nature = "identified";
            identified.decayRate = id.value().resonances[0].decayRate;
            identified.bandwidthHz = id.value().resonances[0].bandwidthHz;
            out.modes.push_back(identified);
        }
        x["cantilever.ledgerResidual"] = ring.value().residual;
    } else {
        out.warnings.push_back("no loss factor: the resonator is lossless (no ring-down)");
    }
    out.discretization = {{"model", "Euler-Bernoulli modes; first mode as a tip-referred oscillator"}};
    return out;
}

// --- coupled resonators ----------------------------------------------------------------------

void reportPair(AnalysisOutcome& out, const oscillators::Network& net, const std::vector<oscillators::NetworkMode>& modes, const std::vector<std::complex<double>>& force,
                const std::vector<std::string>& responseNames, const std::function<std::vector<double>(const oscillators::DrivenPoint&)>& responses,
                const std::string& responseUnit)
{
    Values& x = out.metrics;
    x["coupled.fLower"] = modes[0].frequencyHz;
    x["coupled.fUpper"] = modes[1].frequencyHz;
    x["coupled.splitting"] = modes[1].frequencyHz - modes[0].frequencyHz;
    x["coupled.qLower"] = modes[0].quality;
    x["coupled.qUpper"] = modes[1].quality;
    x["coupled.participationLower1"] = modes[0].participation[0];
    out.modes.push_back(modeOf(modes[0], modes[0].symmetry == "antisymmetric" ? "anti-phase (lower)" : "in-phase"));
    out.modes.push_back(modeOf(modes[1], modes[1].symmetry == "symmetric" ? "in-phase (upper)" : "anti-phase"));
    // The driven response of both resonators to a drive on the first.
    std::vector<double> freqs = linSpace(0.7 * modes[0].frequencyHz, 1.3 * modes[1].frequencyHz, 600);
    auto resp = oscillators::drivenResponse(net, force, freqs);
    if (!resp)
        return;
    std::vector<Curve> curves;
    for (size_t k = 0; k < responseNames.size(); ++k)
        curves.push_back(curve("response" + std::to_string(k + 1), responseNames[k], "Frequency", "Hz", "|response|", responseUnit));
    for (const auto& pt : resp.value()) {
        const auto values = responses(pt);
        for (size_t k = 0; k < curves.size() && k < values.size(); ++k) {
            curves[k].x.push_back(pt.frequencyHz);
            curves[k].y.push_back(values[k]);
        }
    }
    for (auto& c : curves)
        out.curves.push_back(std::move(c));
}

Result<AnalysisOutcome> evaluateCoupledCantilevers(const FunctionalStructure& s, const Project& p, const Values& v, const ModelInfo& mi, JobContext* context)
{
    auto a = cantileverOf(s, p, "beam1", v);
    auto b = cantileverOf(s, p, "beam2", v);
    if (!a || !b)
        return R::failure(a ? b.message() : a.message());
    const double kc = setting(s.analysis.settings, "couplingStiffness", v, 1.0);
    auto net = mechanics::coupledCantilevers(a.value(), b.value(), kc);
    if (!net)
        return R::failure(net.message());
    auto modes = oscillators::naturalModes(net.value());
    if (!modes)
        return R::failure(modes.message());
    if (modes.value().size() < 2)
        return R::failure("the network has fewer than two oscillating modes");
    AnalysisOutcome out;
    out.solver = mi.id;
    out.assumptions = mi.assumptions;
    out.behaviour = "resonant";
    const auto ma = mechanics::cantileverMode(a.value()).value(), mb = mechanics::cantileverMode(b.value()).value();
    Values& x = out.metrics;
    x["coupled.f1individual"] = ma.frequencyHz;
    x["coupled.f2individual"] = mb.frequencyHz;
    x["coupled.detuning"] = mb.frequencyHz - ma.frequencyHz;
    x["coupled.couplingStiffness"] = kc;
    reportPair(out, net.value(), modes.value(), {{1.0, 0.0}, {0.0, 0.0}}, {"Tip of cantilever 1 (drive at tip 1)", "Tip of cantilever 2"},
               [](const oscillators::DrivenPoint& pt) { return std::vector<double>{std::abs(pt.displacement[0]), std::abs(pt.displacement[1])}; }, "m/N");
    const auto cmt = oscillators::coupledModeEstimate(ma.tipMass, ma.tipStiffness, mb.tipMass, mb.tipStiffness, kc);
    x["coupled.cmtSplitting"] = cmt.splittingHz;
    x["coupled.cmtTransfer"] = cmt.maximumTransferFraction;
    x["coupled.couplingRate"] = cmt.couplingRate;
    // Energy exchange after displacing tip 1 alone, over two beat periods.
    const double beat = 1.0 / (modes.value()[1].frequencyHz - modes.value()[0].frequencyHz);
    const double duration = setting(s.analysis.settings, "exchangeDuration", v, 2.0 * beat);
    if (duration * modes.value()[1].frequencyHz > 2e5)
        return R::failure("the exchange run would cover more than 200 000 periods; shorten exchangeDuration or raise the coupling");
    auto exchange = oscillators::energyExchange(net.value(), 0, 1, duration, context);
    if (!exchange)
        return R::failure(exchange.message());
    x["coupled.transferFraction"] = exchange.value().maximumTransferFraction;
    x["coupled.transferTime"] = exchange.value().transferTime;
    x["coupled.beatPeriod"] = exchange.value().beatPeriod;
    oscillators::TransientOptions o;
    o.duration = duration;
    o.x0 = {1e-3, 0.0};
    o.samples = 2000;
    auto run = oscillators::integrate(net.value(), o, context);
    if (!run)
        return R::failure(run.message());
    Curve e1 = curve("energy1", "Energy in cantilever 1 (tip 1 released from 1 mm)", "Time", "s", "E", "J");
    Curve e2 = curve("energy2", "Energy in cantilever 2", "Time", "s", "E", "J");
    for (size_t i = 0; i < run.value().time.size(); ++i) {
        e1.x.push_back(run.value().time[i]);
        e1.y.push_back(run.value().energy[0][i]);
        e2.x.push_back(run.value().time[i]);
        e2.y.push_back(run.value().energy[1][i]);
    }
    out.curves.push_back(e1);
    out.curves.push_back(e2);
    x["coupled.ledgerResidual"] = run.value().residual;
    out.details = {{"couplingModel", cmt.model}, {"network", net.value().unitsNote}};
    out.discretization = {{"model", "two tip-referred oscillators; fourth-order Runge-Kutta"}, {"steps", run.value().steps}, {"dt", run.value().dt}};
    return out;
}

Result<AnalysisOutcome> evaluateCoupledHelmholtz(const FunctionalStructure& s, const Project& p, const Values& v, const ModelInfo& mi, JobContext*)
{
    auto a = helmholtzOf(s, p, "resonator1", v);
    auto b = helmholtzOf(s, p, "resonator2", v);
    if (!a || !b)
        return R::failure(a ? b.message() : a.message());
    const double ac = setting(s.analysis.settings, "connectorRadius", v, 0.003);
    const double lc = setting(s.analysis.settings, "connectorLength", v, 0.01);
    auto net = manipulators::coupledHelmholtz(a.value(), b.value(), ac, lc);
    if (!net)
        return R::failure(net.message());
    auto modes = oscillators::naturalModes(net.value());
    if (!modes)
        return R::failure(modes.message());
    if (modes.value().size() < 2)
        return R::failure("the network has fewer than two oscillating modes");
    AnalysisOutcome out;
    out.solver = mi.id;
    out.assumptions = mi.assumptions;
    out.behaviour = "resonant";
    const double M = net.value().massMatrix(1, 1);
    const double C1 = a.value().volume() / (a.value().density * a.value().soundSpeed * a.value().soundSpeed);
    const double C2 = b.value().volume() / (b.value().density * b.value().soundSpeed * b.value().soundSpeed);
    Values& x = out.metrics;
    x["coupled.f1individual"] = manipulators::analyzeHelmholtz(a.value()).value().frequencyHz;
    x["coupled.f2individual"] = 1.0 / (2 * kPi * std::sqrt(M * C2));  // resonator 2 alone on its connector
    x["coupled.detuning"] = x["coupled.f2individual"] - x["coupled.f1individual"];
    // Drive: the blocked pressure 2 p_i on neck 1; responses: the two cavity pressures per incident pascal.
    reportPair(out, net.value(), modes.value(), {{a.value().flanged ? 2.0 : 1.0, 0.0}, {0.0, 0.0}},
               {"Pressure in cavity 1 per incident Pa", "Pressure in cavity 2 per incident Pa"},
               [C1, C2](const oscillators::DrivenPoint& pt) {
                   return std::vector<double>{std::abs(pt.displacement[0] + pt.displacement[1]) / C1, std::abs(pt.displacement[1]) / C2};
               },
               "");
    out.details = {{"network", net.value().unitsNote}};
    out.discretization = {{"model", "lumped acoustic network (inertances and compliances)"}};
    return out;
}

}  // namespace

void registerResonanceAnalyses(AnalysisRegistry& registry)
{
    const std::vector<MetricInfo> pairMetrics{
        {"coupled.f1individual", "Resonator 1 alone", "Hz", "its frequency without the coupling element's partner"},
        {"coupled.f2individual", "Resonator 2 alone", "Hz", ""},
        {"coupled.detuning", "Detuning", "Hz", "f2 alone - f1 alone"},
        {"coupled.fLower", "Lower coupled mode", "Hz", "natural frequency of the network"},
        {"coupled.fUpper", "Upper coupled mode", "Hz", ""},
        {"coupled.splitting", "Mode splitting", "Hz", "f_upper - f_lower"},
        {"coupled.qLower", "Q of the lower mode", "", "|s| / (2 decay rate)"},
        {"coupled.qUpper", "Q of the upper mode", "", ""},
        {"coupled.participationLower1", "Share of the lower mode in resonator 1", "", "kinetic-energy fraction (localization with detuning)"}};
    {
        AnalysisKind k;
        k.id = kHelmholtzAnalysis;
        k.title = "Helmholtz resonator";
        k.description = "Resonance, end corrections, radiation and viscous Q, bandwidth, impedance and cavity pressure gain of a Helmholtz resonator";
        k.domains = {"acoustic"};
        k.category = Category::Confinement;
        k.mechanism = "the air in the neck is a mass on the spring of the compressible air in the cavity: f0 = (c / 2 pi) sqrt(S / (V L_eff)); a "
                      "larger cavity or a longer, narrower neck lowers it; the neck radiates (radiation loss) and rubs on its wall (viscous loss)";
        k.models = {model(manipulators::kHelmholtzModelId, "Lumped Helmholtz resonator with end corrections", "analytical",
                          {"lumped elements: the neck an inertance, the cavity a compliance", "outer end correction 8a/3pi (flanged) or 0.6133a, inner 0.8488a(1 - 1.25a/R)",
                           "radiation of a baffled (or unbaffled) piston; viscous boundary layer of the neck; no thermal loss in the cavity"},
                          "k times the cavity size and k L_eff below about 0.5", 0.0001),
                    model(manipulators::kHelmholtzDuctModelId, "Plane-wave stepped duct with a baffled-piston load", "reduced",
                          {"plane waves in the neck and the cavity", "no inner end correction, no viscous loss"},
                          "below the cavity's first cross mode", 0.001)};
        k.metrics = {{"helmholtz.f0", "Resonant frequency", "Hz", ""},
                     {"helmholtz.Q", "Quality factor", "", "1/Q = 1/Q_rad + 1/Q_visc"},
                     {"helmholtz.qRadiation", "Radiation Q", "", "w0 M / R_rad"},
                     {"helmholtz.qViscous", "Viscous Q", "", "w0 M / R_visc"},
                     {"helmholtz.bandwidth", "Bandwidth", "Hz", "f0 / Q"},
                     {"helmholtz.pressureGain", "Cavity pressure gain at resonance", "", "|p_cavity / p_incident|"},
                     {"helmholtz.storedEnergyPerPa2", "Stored energy per incident pascal squared", "J/Pa^2", "time-averaged, at resonance"},
                     {"helmholtz.volume", "Cavity volume", "m^3", ""},
                     {"helmholtz.effectiveLength", "Effective neck length", "m", "L plus the end corrections"},
                     {"helmholtz.outerCorrection", "Outer end correction", "m", ""},
                     {"helmholtz.innerCorrection", "Inner end correction", "m", ""},
                     {"helmholtz.neckKa", "ka of the neck", "", ""},
                     {"helmholtz.cavityKSize", "k times the cavity size", "", "validity of the lumped model"},
                     {"helmholtz.lumpedDeviation", "Plane-wave deviation from the lumped model", "", "f0(duct) / f0(lumped) - 1"}};
        k.roles = {{"resonator", "body", "a resonator.helmholtz object", true}};
        k.evaluate = evaluateHelmholtz;
        registry.add(std::move(k));
    }
    {
        AnalysisKind k;
        k.id = kCantileverResonatorAnalysis;
        k.title = "Cantilever resonator";
        k.description = "Bending modes, tip-referred modal mass and stiffness, Q, frequency response and ring-down of a cantilever";
        k.domains = {"mechanical"};
        k.category = Category::Confinement;
        k.mechanism = "the beam's bending stiffness against its distributed mass: f1 = (1.8751^2 / 2 pi) sqrt(E I / (rho A L^4)); the structural loss "
                      "factor sets Q = 1 / eta";
        k.models = {model("mechanics.cantilever.euler-bernoulli", "Euler-Bernoulli cantilever; first mode as an oscillator", "analytical",
                          {"slender beam (no shear or rotary inertia); clamped at x = 0, free at the tip; bending across the thickness",
                           "structural damping as the equivalent viscous damper at each mode's frequency"},
                          "length much larger than the thickness", 0.01)};
        k.metrics = {{"cantilever.f1", "First bending frequency", "Hz", ""},
                     {"cantilever.f2", "Second bending frequency", "Hz", ""},
                     {"cantilever.f3", "Third bending frequency", "Hz", ""},
                     {"cantilever.tipMass", "Tip-referred modal mass", "kg", "0.25 of the beam for mode 1"},
                     {"cantilever.tipStiffness", "Tip-referred modal stiffness", "N/m", "w1^2 times the tip mass"},
                     {"cantilever.beamMass", "Beam mass", "kg", ""},
                     {"cantilever.Q", "Quality factor", "", "1 / eta"},
                     {"cantilever.bandwidth", "Bandwidth", "Hz", "f1 / Q"},
                     {"cantilever.kineticEnergyPerTip2", "Peak kinetic energy per tip amplitude squared", "J/m^2", ""},
                     {"cantilever.ringDownQ", "Ring-down Q", "", "complex pole of the simulated ring-down"},
                     {"cantilever.ledgerResidual", "Ring-down energy-ledger residual", "", "relative"}};
        k.roles = {{"resonator", "body", "a box: length along X, thickness along Z, with an elastic material", true}};
        k.evaluate = evaluateCantilever;
        registry.add(std::move(k));
    }
    {
        AnalysisKind k;
        k.id = kCoupledCantileversAnalysis;
        k.title = "Coupled cantilevers";
        k.description = "Two cantilevers joined at their tips by a spring: mode splitting, hybridized modes, energy exchange and detuning";
        k.domains = {"mechanical"};
        k.category = Category::Confinement;
        k.mechanism = "the coupling spring makes the two resonators exchange energy: the in-phase mode leaves it unstretched, the anti-phase mode "
                      "stretches it and lies higher; detuning localizes the modes and limits the transfer";
        k.models = {model("mechanics.coupled-cantilevers.lumped", "Two tip-referred oscillators and a coupling spring", "reduced",
                          {"each cantilever's first bending mode (Euler-Bernoulli) as a tip-referred oscillator", "a massless coupling spring at the tips",
                           "structural damping as equivalent viscous dampers"},
                          "coupling weak enough not to excite the second bending modes", 0.05)};
        k.metrics = pairMetrics;
        for (MetricInfo m : std::vector<MetricInfo>{
                 {"coupled.couplingStiffness", "Coupling stiffness", "N/m", ""},
                 {"coupled.cmtSplitting", "Splitting by coupled-mode theory", "Hz", "an approximation"},
                 {"coupled.cmtTransfer", "Maximum transfer by coupled-mode theory", "", "4 kappa^2 / (4 kappa^2 + detuning^2)"},
                 {"coupled.couplingRate", "Coupling rate", "rad/s", "kappa"},
                 {"coupled.transferFraction", "Maximum energy transferred", "", "max E2 / E_total after releasing tip 1 (time integration)"},
                 {"coupled.transferTime", "Time of the first transfer maximum", "s", ""},
                 {"coupled.beatPeriod", "Beat period", "s", "1 / splitting"},
                 {"coupled.ledgerResidual", "Energy-ledger residual", "", "relative"}})
            k.metrics.push_back(m);
        k.roles = {{"beam1", "body", "the first cantilever (a box)", true}, {"beam2", "body", "the second cantilever (a box)", true}};
        k.evaluate = evaluateCoupledCantilevers;
        registry.add(std::move(k));
    }
    {
        AnalysisKind k;
        k.id = kCoupledHelmholtzAnalysis;
        k.title = "Coupled Helmholtz resonators";
        k.description = "An open Helmholtz resonator joined through a connecting neck to a closed one: coupled modes, splitting and cavity pressures";
        k.domains = {"acoustic"};
        k.category = Category::Confinement;
        k.mechanism = "the connecting neck's air mass couples the two cavities' compliances: the coupled modes either move air through the "
                      "connector in phase with the outer neck or against it";
        k.models = {model("acoustic.coupled-helmholtz.lumped", "Lumped acoustic network", "analytical",
                          {"lumped inertances and compliances with end corrections", "losses evaluated at resonator 1's frequency"},
                          "every resonator small against the wavelength", 0.001)};
        k.metrics = pairMetrics;
        k.roles = {{"resonator1", "body", "the open resonator (resonator.helmholtz)", true},
                   {"resonator2", "body", "the closed resonator (resonator.helmholtz; its own neck is not used)", true}};
        k.evaluate = evaluateCoupledHelmholtz;
        registry.add(std::move(k));
    }
}

}  // namespace resonant::coupling
