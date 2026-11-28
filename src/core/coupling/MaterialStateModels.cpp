#include "coupling/MaterialStateModels.h"

#include "electromagnetics/Hysteresis.h"
#include "foundation/Time.h"
#include "mechanics/Damage.h"
#include "mechanics/DamagedBeam.h"
#include "mechanics/Plasticity.h"
#include "mechanics/Viscoelasticity.h"
#include "simulation/Job.h"
#include "thermal/PhaseChange.h"

#include <algorithm>
#include <cmath>

namespace resonant::coupling {

using nlohmann::json;
using namespace materialstate;
using R = Result<json>;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kMu0 = 1.25663706212e-6;

StateVariable var(std::string key, std::string label, std::string unit, VariableClass c, std::string d = {})
{
    return {std::move(key), std::move(label), std::move(unit), c, std::move(d)};
}

Parameter par(std::string key, std::string label, std::string unit, double v, std::string d = {})
{
    return {std::move(key), std::move(label), std::move(unit), v, std::move(d)};
}

ModelCard notImplemented(std::string id, std::string title, std::string domain, std::string why)
{
    ModelCard c;
    c.id = std::move(id);
    c.title = std::move(title);
    c.domain = std::move(domain);
    c.status = "not implemented";
    c.excluded = {std::move(why)};
    return c;
}

double parameter(const json& study, const ModelCard& card, const std::string& key)
{
    const json p = study.value("parameters", json::object());
    if (p.contains(key)) {
        if (!p[key].is_number())
            throw std::runtime_error("the parameter '" + key + "' must be a number");
        return p[key].get<double>();
    }
    for (const auto& x : card.parameters)
        if (x.key == key)
            return x.value;
    throw std::runtime_error("unknown parameter '" + key + "'");
}

json curve(std::string id, std::string title, std::string xl, std::string xu, std::string yl, std::string yu, const std::vector<double>& x,
           const std::vector<double>& y)
{
    return {{"id", id}, {"title", title}, {"xLabel", xl}, {"xUnit", xu}, {"yLabel", yl}, {"yUnit", yu}, {"x", x}, {"y", y}};
}

// A cyclic path through the given amplitudes: 0 -> a1 -> -a1 -> a2 -> ... (or 0 -> a -> 0 when one-sided).
std::vector<double> cyclicPath(const json& loading, int perHalf)
{
    std::vector<double> path;
    const auto amplitudes = loading.value("amplitudes", std::vector<double>{});
    const bool reversed = loading.value("reversed", true);
    double at = 0.0;
    const auto go = [&](double to) {
        for (int i = 1; i <= perHalf; ++i)
            path.push_back(at + (to - at) * i / perHalf);
        at = to;
    };
    for (double a : amplitudes) {
        go(a);
        go(reversed ? -a : 0.0);
    }
    if (reversed && !amplitudes.empty())
        go(0.0);
    return path;
}

json base(const ModelCard& card, const json& study, std::string kind)
{
    return {{"format", kStateRunFormat}, {"version", 1}, {"kind", std::move(kind)}, {"model", card.id}, {"title", card.title},
            {"parameters", study.value("parameters", json::object())}, {"loading", study.value("loading", json::object())},
            {"created", utcNowIso8601()}, {"history", json::array()}, {"curves", json::array()}, {"profiles", json::array()},
            {"committable", json::array()}, {"reliability", json::array()}, {"modelCard", card.toJson()}};
}

// The error of the trapezoidal work integral of y over x from (0, 0) through the points:
// Richardson's estimate |W_h - W_2h| / 3 (every other point for W_2h).
double workQuadratureError(const std::vector<double>& x, const std::vector<double>& y)
{
    std::vector<double> xs{0.0}, ys{0.0};
    xs.insert(xs.end(), x.begin(), x.end());
    ys.insert(ys.end(), y.begin(), y.end());
    double fine = 0.0, coarse = 0.0;
    for (size_t i = 1; i < xs.size(); ++i)
        fine += 0.5 * (ys[i] + ys[i - 1]) * (xs[i] - xs[i - 1]);
    size_t last = 0;
    for (size_t i = 2; i < xs.size(); i += 2) {
        coarse += 0.5 * (ys[i] + ys[i - 2]) * (xs[i] - xs[i - 2]);
        last = i;
    }
    for (size_t i = last + 1; i < xs.size(); ++i)  // an odd remainder at the fine spacing
        coarse += 0.5 * (ys[i] + ys[i - 1]) * (xs[i] - xs[i - 1]);
    return std::abs(fine - coarse) / 3.0;
}

// --- J2 plasticity, a material point in uniaxial stress ----------------------------------------

mechanics::J2Parameters j2(const json& study, const ModelCard& card)
{
    mechanics::J2Parameters p;
    p.youngsModulus = parameter(study, card, "youngsModulus");
    p.poissonsRatio = parameter(study, card, "poissonsRatio");
    p.yieldStress = parameter(study, card, "yieldStress");
    p.isotropicHardening = parameter(study, card, "isotropicHardening");
    p.kinematicHardening = parameter(study, card, "kinematicHardening");
    return p;
}

R runJ2Point(const ModelCard& card, const json& study)
{
    const mechanics::J2Parameters p = j2(study, card);
    if (Status v = p.validate(); !v)
        return R::failure(v.message());
    const json loading = study.value("loading", json::object());
    const std::vector<double> path = cyclicPath(loading, std::clamp(loading.value("increments", 50), 2, 2000));
    if (path.empty())
        return R::failure("the loading needs strain amplitudes");
    json d = base(card, study, "material-point");
    mechanics::J2State c;
    mechanics::Vec6 guess = mechanics::Vec6::Zero();
    std::vector<double> e, s, ep;
    int maxIterations = 0;
    double work = 0.0, e0 = 0.0, s0 = 0.0;
    mechanics::J2Response last;
    for (size_t i = 0; i < path.size(); ++i) {
        auto u = mechanics::j2Uniaxial(p, c, path[i], guess);
        if (!u) {
            d["reliability"].push_back("step " + std::to_string(i + 1) + ": " + u.message() + " (not committed)");
            break;
        }
        c = u.value().response.state;
        guess = u.value().strain;
        last = u.value().response;
        maxIterations = std::max(maxIterations, u.value().iterations);
        work += 0.5 * (u.value().stress + s0) * (path[i] - e0);
        e0 = path[i];
        s0 = u.value().stress;
        e.push_back(path[i]);
        s.push_back(u.value().stress);
        ep.push_back(c.plasticStrain(0));
        if (i % std::max<size_t>(1, path.size() / 300) == 0 || i + 1 == path.size())
            d["history"].push_back({{"step", i + 1}, {"strain", path[i]}, {"stress", u.value().stress}, {"plasticStrain", c.plasticStrain(0)},
                                    {"equivalentPlasticStrain", c.equivalentPlasticStrain}, {"backStress", c.backStress(0)},
                                    {"plastic", u.value().response.plastic}, {"iterations", u.value().iterations}});
    }
    d["curves"] = json::array({curve("stressStrain", "Stress against strain", "Strain", "", "Stress", "Pa", e, s),
                               curve("plasticStrain", "Plastic strain against strain", "Strain", "", "Plastic strain", "", e, ep)});
    d["final"] = {{"plasticStrain", c.plasticStrain(0)}, {"equivalentPlasticStrain", c.equivalentPlasticStrain}, {"backStress", c.backStress(0)},
                  {"stress", s.empty() ? 0.0 : s.back()}};
    d["energy"] = {{"work", work}, {"elastic", last.elasticEnergy}, {"hardening", last.hardeningEnergy}, {"dissipated", c.dissipated},
                   {"residual", work - last.elasticEnergy - last.hardeningEnergy - c.dissipated}, {"workQuadratureError", workQuadratureError(e, s)},
                   {"unit", "J/m^3"}};
    d["convergence"] = {{"maxIterations", maxIterations}};
    d["summary"] = {{"yieldStrain", p.yieldStress / p.youngsModulus},
                    {"plasticTangent", p.youngsModulus * (p.isotropicHardening + p.kinematicHardening) /
                                           (p.youngsModulus + p.isotropicHardening + p.kinematicHardening)}};
    d["reliable"] = d["reliability"].empty();
    return d;
}

R runJ2Bar(const ModelCard& card, const json& study)
{
    mechanics::BarSpec s;
    s.material = j2(study, card);
    s.length = parameter(study, card, "length");
    s.loadPoint = parameter(study, card, "loadPoint");
    s.area = parameter(study, card, "area");
    const json loading = study.value("loading", json::object());
    const double peak = loading.value("peakLoadFraction", 1.8) * 2.0 * s.material.yieldStress * s.area;  // of the limit load 2 sigma_y A
    const int n = std::clamp(loading.value("increments", 60), 2, 2000);
    for (int i = 1; i <= n; ++i)
        s.loads.push_back(peak * i / n);
    if (loading.value("unload", true))
        for (int i = n - 1; i >= 0; --i)
            s.loads.push_back(peak * i / n);
    auto r = mechanics::j2Bar(s);
    if (!r)
        return R::failure(r.message());
    json d = base(card, study, "structure");
    std::vector<double> u, P, ep1, ep2;
    int maxIt = 0;
    for (size_t i = 0; i < r.value().size(); ++i) {
        const auto& st = r.value()[i];
        if (!st.converged) {
            d["reliability"].push_back("load step " + std::to_string(i + 1) + " did not converge (residual " + std::to_string(st.residual) +
                                       "): the state of the previous step is kept");
            break;
        }
        u.push_back(st.displacement);
        P.push_back(st.load);
        ep1.push_back(st.plasticStrain1);
        ep2.push_back(st.plasticStrain2);
        maxIt = std::max(maxIt, st.iterations);
        d["history"].push_back({{"step", i + 1}, {"load", st.load}, {"displacement", st.displacement}, {"force1", st.force1}, {"force2", st.force2},
                                {"plasticStrain1", st.plasticStrain1}, {"plasticStrain2", st.plasticStrain2}, {"iterations", st.iterations},
                                {"externalWork", st.externalWork}, {"dissipated", st.dissipated}});
    }
    d["curves"] = json::array({curve("loadDisplacement", "Load against displacement at the load point", "Displacement", "m", "Load", "N", u, P),
                               curve("plastic1", "Plastic strain of the short segment", "Displacement", "m", "Plastic strain", "", u, ep1),
                               curve("plastic2", "Plastic strain of the long segment", "Displacement", "m", "Plastic strain", "", u, ep2)});
    const auto& end = r.value().back();
    const double b = s.length - s.loadPoint;
    d["summary"] = {{"firstYieldLoad", s.material.yieldStress * s.area * s.length / b}, {"limitLoad", 2.0 * s.material.yieldStress * s.area},
                    {"residualDisplacement", end.displacement}, {"residualForce", end.force1}};
    d["energy"] = {{"work", end.externalWork}, {"elastic", end.elasticEnergy}, {"hardening", end.hardeningEnergy}, {"dissipated", end.dissipated},
                   {"residual", end.externalWork - end.elasticEnergy - end.hardeningEnergy - end.dissipated}, {"unit", "J"}};
    d["final"] = {{"displacement", end.displacement}, {"force1", end.force1}, {"force2", end.force2}};
    d["convergence"] = {{"maxIterations", maxIt}, {"method", "Newton with the consistent tangent"}};
    d["reliable"] = d["reliability"].empty();
    return d;
}

// --- damage ------------------------------------------------------------------------------------

mechanics::DamageParameters damageOf(const json& study, const ModelCard& card)
{
    mechanics::DamageParameters p;
    p.youngsModulus = parameter(study, card, "youngsModulus");
    p.thresholdStrain = parameter(study, card, "thresholdStrain");
    p.softeningStrain = parameter(study, card, "softeningStrain");
    p.exponent = parameter(study, card, "exponent");
    p.law = study.value("parameters", json::object()).value("law", std::string("power"));
    return p;
}

R runDamagePoint(const ModelCard& card, const json& study)
{
    const mechanics::DamageParameters p = damageOf(study, card);
    if (Status v = p.validate(); !v)
        return R::failure(v.message());
    json loading = study.value("loading", json::object());
    const std::vector<double> path = cyclicPath(loading, std::clamp(loading.value("increments", 100), 2, 5000));
    if (path.empty())
        return R::failure("the loading needs strain amplitudes");
    json d = base(card, study, "material-point");
    mechanics::DamageState c;
    std::vector<double> e, s, D;
    double work = 0.0, e0 = 0.0, s0 = 0.0, stored = 0.0;
    for (size_t i = 0; i < path.size(); ++i) {
        const auto r = mechanics::damageUpdate(p, c, path[i]);
        c = r.state;
        work += 0.5 * (r.stress + s0) * (path[i] - e0);
        e0 = path[i];
        s0 = r.stress;
        stored = r.storedEnergy;
        e.push_back(path[i]);
        s.push_back(r.stress);
        D.push_back(c.damage);
        if (i % std::max<size_t>(1, path.size() / 300) == 0 || i + 1 == path.size())
            d["history"].push_back({{"step", i + 1}, {"strain", path[i]}, {"stress", r.stress}, {"damage", c.damage}, {"kappa", c.kappa}, {"loading", r.loading}});
    }
    d["curves"] = json::array({curve("stressStrain", "Stress against strain", "Strain", "", "Stress", "Pa", e, s),
                               curve("damage", "Damage against strain", "Strain", "", "Damage", "", e, D)});
    d["final"] = {{"damage", c.damage}, {"kappa", c.kappa}, {"secantModulus", (1.0 - c.damage) * p.youngsModulus}};
    d["energy"] = {{"work", work}, {"stored", stored}, {"dissipated", c.dissipated}, {"residual", work - stored - c.dissipated},
                   {"workQuadratureError", workQuadratureError(e, s)}, {"unit", "J/m^3"}};
    d["committable"].push_back({{"domain", "mechanical"}, {"key", "youngsModulus"}, {"value", (1.0 - c.damage) * p.youngsModulus},
                                {"note", "the damaged (secant) modulus (1 - D) E after the loading"}});
    if (p.law == "exponential")
        d["reliability"].push_back("the exponential law softens: in a structure its damage localizes and depends on the mesh");
    d["reliable"] = true;  // a material point is exact for its law
    return d;
}

// --- damaged cantilever -----------------------------------------------------------------------

R runDamagedCantilever(const ModelCard& card, const json& study, JobContext* ctx)
{
    mechanics::DamagedBeamSpec s;
    s.length = parameter(study, card, "length");
    s.width = parameter(study, card, "width");
    s.thickness = parameter(study, card, "thickness");
    s.density = parameter(study, card, "density");
    s.damage = damageOf(study, card);
    s.elements = static_cast<int>(parameter(study, card, "elements"));
    const json loading = study.value("loading", json::object());
    const auto amplitudes = loading.value("amplitudes", std::vector<double>{});
    const int perHalf = std::clamp(loading.value("increments", 40), 2, 1000);
    if (amplitudes.empty())
        return R::failure("the loading needs tip-deflection amplitudes (m)");
    json d = base(card, study, "structure");
    std::vector<double> cycle, f1, deflection, load;
    std::vector<double> path;
    double f0 = 0.0;
    for (size_t k = 0; k < amplitudes.size(); ++k) {
        if (ctx && ctx->cancelRequested())
            return R::failure("cancelled");
        for (int i = 1; i <= perHalf; ++i)
            path.push_back(amplitudes[k] * i / perHalf);
        for (int i = perHalf - 1; i >= 0; --i)
            path.push_back(amplitudes[k] * i / perHalf);
        s.tipDeflections = path;
        auto r = mechanics::damagedCantilever(s);
        if (!r)
            return R::failure(r.message());
        f0 = r.value().undamagedFrequencies[0];
        bool ok = true;
        for (const auto& st : r.value().steps)
            ok = ok && st.converged;
        if (!ok) {
            d["reliability"].push_back("cycle " + std::to_string(k + 1) + " did not converge: the damage and the shape found no fixed point");
            break;
        }
        cycle.push_back(static_cast<double>(k + 1));
        f1.push_back(r.value().frequencies[0]);
        if (k + 1 == amplitudes.size()) {
            for (const auto& st : r.value().steps) {
                deflection.push_back(st.deflection);
                load.push_back(st.load);
            }
            d["profiles"].push_back({{"step", static_cast<int>(path.size())}, {"time", static_cast<double>(k + 1)}, {"title", "Damage along the cantilever"},
                                     {"xLabel", "Position"}, {"xUnit", "m"}, {"yLabel", "Damage"}, {"x", r.value().elementCentres}, {"y", r.value().damage}});
            const auto& end = r.value().steps.back();
            d["final"] = {{"frequencies", r.value().frequencies}, {"undamagedFrequencies", r.value().undamagedFrequencies},
                          {"rootDamage", r.value().damage.front()}, {"maxDamage", *std::max_element(r.value().damage.begin(), r.value().damage.end())},
                          {"tipStiffness", r.value().damagedTipStiffness}, {"undamagedTipStiffness", r.value().undamagedTipStiffness}};
            d["energy"] = {{"work", end.externalWork}, {"stored", end.storedEnergy}, {"dissipated", end.dissipated},
                           {"residual", end.externalWork - end.storedEnergy - end.dissipated}, {"unit", "J"}};
            d["committable"].push_back({{"domain", "mechanical"}, {"key", "youngsModulus"},
                                        {"value", s.damage.youngsModulus * r.value().damagedTipStiffness / r.value().undamagedTipStiffness},
                                        {"note", "the uniform modulus with the damaged beam's tip stiffness (an equivalent, not a local value)"}});
        }
        d["history"].push_back({{"step", k + 1}, {"amplitude", amplitudes[k]}, {"f1", r.value().frequencies[0]}, {"rootDamage", r.value().damage.front()}});
    }
    d["curves"] = json::array({curve("frequency", "First bending frequency after each cycle", "Cycle", "", "f1", "Hz", cycle, f1),
                               curve("loadDeflection", "Tip load against deflection (last cycle path)", "Deflection", "m", "Load", "N", deflection, load)});
    d["summary"] = {{"undamagedF1", f0}, {"damagedF1", f1.empty() ? f0 : f1.back()}, {"relativeShift", f1.empty() ? 0.0 : f1.back() / f0 - 1.0}};
    d["reliable"] = d["reliability"].empty();
    return d;
}

// --- viscoelasticity ------------------------------------------------------------------------

R runZener(const ModelCard& card, const json& study)
{
    mechanics::ZenerParameters p;
    p.relaxedModulus = parameter(study, card, "relaxedModulus");
    p.branchModulus = parameter(study, card, "branchModulus");
    p.relaxationTime = parameter(study, card, "relaxationTime");
    if (Status v = p.validate(); !v)
        return R::failure(v.message());
    const json loading = study.value("loading", json::object());
    const std::string kind = loading.value("kind", "harmonic");
    std::vector<double> t, e;
    if (kind == "relaxation") {
        const double e0 = loading.value("strain", 1e-3), duration = loading.value("duration", 5.0 * p.relaxationTime);
        t = {0.0, 0.0};
        e = {0.0, e0};
        for (int i = 1; i <= 400; ++i) {
            t.push_back(duration * i / 400);
            e.push_back(e0);
        }
    } else if (kind == "harmonic") {
        const double a = loading.value("amplitude", 1e-3), f = loading.value("frequencyHz", 1.0 / (2 * kPi * p.relaxationTime));
        const int cycles = std::clamp(loading.value("cycles", 5), 1, 200);
        const int n = 200 * cycles;
        for (int i = 0; i <= n; ++i) {
            t.push_back(cycles / f * i / n);
            e.push_back(a * std::sin(2 * kPi * f * t.back()));
        }
    } else {
        return R::failure("the loading kind must be relaxation or harmonic");
    }
    auto r = mechanics::zenerResponse(p, t, e);
    if (!r)
        return R::failure(r.message());
    json d = base(card, study, "material-point");
    std::vector<double> tt, ee, ss, qq;
    double work = 0.0;  // the trapezoidal integral of stress over strain (exact on a strain jump)
    for (size_t i = 0; i < r.value().size(); ++i) {
        const auto& z = r.value()[i];
        if (i > 0)
            work += 0.5 * (z.stress + r.value()[i - 1].stress) * (z.strain - r.value()[i - 1].strain);
        tt.push_back(z.time);
        ee.push_back(z.strain);
        ss.push_back(z.stress);
        qq.push_back(z.branchStress);
        if (i % std::max<size_t>(1, r.value().size() / 300) == 0 || i + 1 == r.value().size())
            d["history"].push_back({{"step", i}, {"time", z.time}, {"strain", z.strain}, {"stress", z.stress}, {"branchStress", z.branchStress},
                                    {"dissipated", z.dissipated}});
    }
    d["curves"] = json::array({curve("stressTime", "Stress against time", "Time", "s", "Stress", "Pa", tt, ss),
                               curve("stressStrain", "Stress against strain", "Strain", "", "Stress", "Pa", ee, ss)});
    const auto& end = r.value().back();
    d["energy"] = {{"work", work}, {"stored", end.storedEnergy}, {"dissipated", end.dissipated}, {"residual", work - end.storedEnergy - end.dissipated},
                   {"workQuadratureError", workQuadratureError(ee, ss)}, {"unit", "J/m^3"},
                   {"note", "the work is the trapezoidal integral of the stress over the strain path; its residual is that quadrature's error"}};
    if (kind == "harmonic") {
        const double f = loading.value("frequencyHz", 1.0 / (2 * kPi * p.relaxationTime)), w = 2 * kPi * f, a = loading.value("amplitude", 1e-3);
        const size_t perCycle = 200, last = r.value().size() - 1;
        const double cycleLoss = end.dissipated - r.value()[last - perCycle].dissipated;
        d["summary"] = {{"lossModulus", p.lossModulus(w)}, {"storageModulus", p.storageModulus(w)}, {"lossPerCycle", cycleLoss},
                        {"lossPerCycleSteadyState", kPi * a * a * p.lossModulus(w)}, {"lossFactor", p.lossModulus(w) / p.storageModulus(w)}};
    }
    d["reliable"] = true;
    return d;
}

// --- phase change ------------------------------------------------------------------------

R runPhaseChange(const ModelCard& card, const json& study, JobContext* ctx)
{
    thermal::PhaseChangeProblem p;
    p.length = parameter(study, card, "length");
    p.cells = static_cast<int>(parameter(study, card, "cells"));
    p.material.density = parameter(study, card, "density");
    p.material.solidSpecificHeat = parameter(study, card, "solidSpecificHeat");
    p.material.liquidSpecificHeat = parameter(study, card, "liquidSpecificHeat");
    p.material.solidConductivity = parameter(study, card, "solidConductivity");
    p.material.liquidConductivity = parameter(study, card, "liquidConductivity");
    p.material.meltingTemperature = parameter(study, card, "meltingTemperature");
    p.material.latentHeat = parameter(study, card, "latentHeat");
    p.initialTemperature = parameter(study, card, "initialTemperature");
    p.left = {thermal::SlabFace::Kind::Temperature, parameter(study, card, "wallTemperature")};
    p.right = {thermal::SlabFace::Kind::Adiabatic, 0.0};
    thermal::PhaseChangeSettings st;
    const json loading = study.value("loading", json::object());
    st.duration = loading.value("duration", 3600.0);
    st.snapshots = std::clamp(loading.value("snapshots", 12), 1, 400);
    auto r = solvePhaseChange(p, st, [ctx] { return ctx && ctx->cancelRequested(); });
    if (!r)
        return R::failure(r.message());
    json d = base(card, study, "structure");
    const auto& v = r.value();
    std::vector<double> reference;
    const bool stefan = std::abs(p.initialTemperature - p.material.meltingTemperature) < 1e-9 && p.left.value > p.material.meltingTemperature;
    const double stNumber = p.material.liquidSpecificHeat * (p.left.value - p.material.meltingTemperature) / p.material.latentHeat;
    const double lambda = stefan ? thermal::stefanLambda(stNumber) : 0.0;
    const double alpha = p.material.liquidConductivity / (p.material.density * p.material.liquidSpecificHeat);
    for (size_t i = 0; i < v.times.size(); ++i) {
        reference.push_back(stefan ? 2.0 * lambda * std::sqrt(alpha * v.times[i]) : 0.0);
        d["history"].push_back({{"step", i}, {"time", v.times[i]}, {"meltedThickness", v.front[i]}, {"wallFlux", v.leftFlux[i]}});
        d["profiles"].push_back({{"step", i}, {"time", v.times[i]}, {"title", "Liquid fraction"}, {"xLabel", "Position"}, {"xUnit", "m"},
                                 {"yLabel", "Liquid fraction"}, {"x", v.centres}, {"y", v.liquidFraction[i]}});
    }
    d["curves"] = json::array({curve("front", "Melted thickness against time", "Time", "s", "Melted thickness", "m", v.times, v.front),
                               curve("wallFlux", "Heat flux into the wall face", "Time", "s", "Flux", "W/m^2", v.times, v.leftFlux)});
    if (stefan)
        d["curves"].push_back(curve("stefan", "Stefan (Neumann) solution", "Time", "s", "Melted thickness", "m", v.times, reference));
    d["energy"] = {{"heatIn", v.heatIn}, {"sensible", v.sensibleChange}, {"latent", v.latentChange}, {"residual", v.balanceResidual}, {"unit", "J/m^2"}};
    d["entropy"] = {{"change", v.entropyChange}, {"inflow", v.entropyIn}, {"production", v.entropyProduction},
                    {"residual", v.entropyChange - v.entropyIn - v.entropyProduction}, {"unit", "J/(m^2 K)"},
                    {"note", "the change from the cells' states; inflow q/T at the faces; production by conduction between cell centres"}};
    d["convergence"] = {{"timeStep", v.timeStep}, {"stabilityLimit", v.stabilityLimit}, {"steps", v.steps}};
    d["summary"] = {{"stefanNumber", stNumber}, {"lambda", lambda}, {"finalFront", v.front.back()},
                    {"stefanFront", stefan ? reference.back() : 0.0}};
    d["final"] = {{"temperature", v.temperature.back()}, {"liquidFraction", v.liquidFraction.back()}};
    d["reliable"] = true;
    return d;
}

// --- hysteresis --------------------------------------------------------------------------

R runPreisach(const ModelCard& card, const json& study)
{
    em::PreisachParameters p;
    p.saturationMagnetization = parameter(study, card, "saturationMagnetization");
    p.saturationField = parameter(study, card, "saturationField");
    p.reversibleSusceptibility = parameter(study, card, "reversibleSusceptibility");
    p.levels = static_cast<int>(parameter(study, card, "levels"));
    p.coercivity = parameter(study, card, "coercivity");
    p.coerciveSpread = parameter(study, card, "coerciveSpread");
    p.interactionSpread = parameter(study, card, "interactionSpread");
    p.density = study.value("parameters", json::object()).value("density", std::string("gaussian"));
    auto model = em::PreisachModel::create(p);
    if (!model)
        return R::failure(model.message());
    em::PreisachModel& m = model.value();
    m.reset();
    const json loading = study.value("loading", json::object());
    const auto amplitudes = loading.value("amplitudes", std::vector<double>{});
    const int n = std::clamp(loading.value("increments", 100), 4, 4000);
    if (amplitudes.empty())
        return R::failure("the loading needs field amplitudes (A/m)");
    json d = base(card, study, "material-point");
    std::vector<double> H, M, B;
    json loops = json::array();
    int step = 0;
    const auto apply = [&](double h) {
        const double mm = m.apply(h);
        H.push_back(h);
        M.push_back(mm);
        B.push_back(kMu0 * (h + mm));
        if (step % std::max(1, n / 25) == 0)
            d["history"].push_back({{"step", step}, {"field", h}, {"magnetization", mm}, {"flux", B.back()}, {"memory", m.memory()}});
        ++step;
        return mm;
    };
    // From negative saturation: to each amplitude, one cycle +a -> -a -> +a to close the loop (a minor loop entered
    // from a larger excursion is not closed on its first traversal: relays with alpha > a switched down stay down),
    // then the measured cycle, closed by return-point memory, whose area is the loss.
    double loss = 0.0;
    for (double a : amplitudes) {
        const double from = m.field();
        for (int i = 1; i <= n; ++i)
            apply(from + (a - from) * i / n);
        for (int i = 1; i <= 4 * n; ++i)
            apply(i <= 2 * n ? a - a * i / n : -a + a * (i - 2 * n) / n);
        double area = 0.0, h0 = a, m0 = m.magnetization();
        for (int i = 1; i <= 4 * n; ++i) {
            const double h = i <= 2 * n ? a - a * i / n : -a + a * (i - 2 * n) / n;
            const double mm = apply(h);
            area += kMu0 * 0.5 * (h + h0) * (mm - m0);
            h0 = h;
            m0 = mm;
        }
        loops.push_back({{"amplitude", a}, {"lossPerCycle", area}, {"hysteronLoss", m.hysteronLoopLoss(-a, a)}, {"fieldIncrement", a / n},
                         // Each relay switches inside an increment where the trapezoid takes the increment's mid field: at most
                         // dH / 2 off per switch, two switches per relay and cycle.
                         {"errorBound", 2.0 * kMu0 * p.saturationMagnetization * (a / n)}});
        loss = area;
    }
    d["curves"] = json::array({curve("bh", "Flux density against field", "Field H", "A/m", "B", "T", H, B),
                               curve("mh", "Magnetization against field", "Field H", "A/m", "M", "A/m", H, M)});
    d["energy"] = {{"loops", loops}, {"lossPerCycle", loss}, {"unit", "J/m^3"}, {"note", "the closed integral of H dB over each loop +a -> -a -> +a"}};
    d["final"] = {{"field", m.field()}, {"magnetization", m.magnetization()}, {"memory", m.memory()}};
    // A core: H = N i / (2 pi r); the loss per cycle times the volume and the frequency is the power.
    const json core = loading.value("core", json::object());
    if (!core.empty()) {
        const double volume = 2 * kPi * core.value("meanRadius", 0.02) * core.value("crossSection", 1e-4);
        const double frequency = core.value("frequencyHz", 50.0);
        d["summary"] = {{"coreVolume", volume}, {"lossPerCycle", loss * volume}, {"powerAtFrequency", loss * volume * frequency},
                        {"peakCurrent", amplitudes.empty() ? 0.0 : amplitudes.back() * 2 * kPi * core.value("meanRadius", 0.02) / core.value("turns", 100.0)}};
    }
    d["reliable"] = true;
    return d;
}

}  // namespace

const std::vector<ModelCard>& materialStateModels()
{
    static const std::vector<ModelCard> cards = [] {
        std::vector<ModelCard> c;
        const std::vector<Parameter> j2p{par("youngsModulus", "Young's modulus", "Pa", 200e9), par("poissonsRatio", "Poisson's ratio", "", 0.3),
                                         par("yieldStress", "Initial yield stress", "Pa", 250e6),
                                         par("isotropicHardening", "Isotropic hardening modulus", "Pa", 2e9),
                                         par("kinematicHardening", "Kinematic hardening modulus", "Pa", 0.0)};
        const std::vector<StateVariable> j2v{var("plasticStrain", "Plastic strain", "", VariableClass::Irreversible),
                                             var("equivalentPlasticStrain", "Equivalent plastic strain", "", VariableClass::Irreversible),
                                             var("backStress", "Back stress", "Pa", VariableClass::History, "the centre of the elastic range"),
                                             var("elasticStrain", "Elastic strain", "", VariableClass::Reversible)};
        ModelCard point{"mechanics.j2.point", "J2 plasticity: a material point in uniaxial stress", "mechanical", "implemented", j2v, j2p,
                        {"small strain", "rate independent", "von Mises yield, linear isotropic and kinematic (Prager) hardening"},
                        {"rate effects", "finite strain", "nonlinear (saturating) hardening", "temperature-dependent yield"},
                        "metals below about a tenth of their melting temperature; small strains", "backward-Euler radial return, consistent tangent",
                        "Simo and Hughes, Computational Inelasticity (1998)"};
        point.loadings = json::array({{{"id", "cyclic"}, {"title", "Cycles to 0.4 % and 0.6 %"}, {"loading", {{"amplitudes", {0.004, 0.006}}, {"increments", 60}}}},
                                      {{"id", "monotonic"}, {"title", "Load to 1 % and unload"}, {"loading", {{"amplitudes", {0.01}}, {"reversed", false}, {"increments", 100}}}}});
        c.push_back(point);
        std::vector<Parameter> barp = j2p;
        barp.push_back(par("length", "Bar length", "m", 1.0));
        barp.push_back(par("loadPoint", "Load point (from the left end)", "m", 0.3));
        barp.push_back(par("area", "Section", "m^2", 1e-4));
        ModelCard bar{"mechanics.j2.bar", "J2 plasticity: a bar fixed at both ends, loaded inside", "mechanical", "implemented", j2v, barp,
                      {"uniaxial stress in each segment", "small strain", "statically indeterminate: the segments share the load by stiffness until they yield"},
                      {"buckling of the compressed segment", "dynamics"}, "as the material point", "global Newton with the consistent tangent",
                      "the analytic solution of the two-segment bar"};
        bar.loadings = json::array({{{"id", "past-limit"}, {"title", "To 95 % of the limit load and back"}, {"loading", {{"peakLoadFraction", 0.95}, {"increments", 60}}}}});
        c.push_back(bar);
        const std::vector<Parameter> dp{par("youngsModulus", "Young's modulus", "Pa", 30e9), par("thresholdStrain", "Damage threshold strain", "", 1e-4),
                                        par("softeningStrain", "Softening strain (exponential law)", "", 1e-3),
                                        par("exponent", "Exponent (power law)", "", 0.6)};
        const std::vector<StateVariable> dv{var("damage", "Damage", "", VariableClass::Irreversible),
                                            var("kappa", "Largest equivalent strain", "", VariableClass::History)};
        ModelCard dmg{"mechanics.damage.point", "Scalar damage: a material point", "mechanical", "implemented", dv, dp,
                      {"isotropic scalar damage", "secant unloading", "equivalent strain |eps|; law power (default) or exponential ('law' parameter)"},
                      {"crack opening and closure (unilateral effects)", "damage-induced anisotropy", "healing"},
                      "quasi-brittle and fibrous materials in the range of their law", "explicit update of the history variable", "Lemaitre, A Course on Damage Mechanics"};
        dmg.loadings = json::array({{{"id", "growing"}, {"title", "Growing cycles"}, {"loading", {{"amplitudes", {0.0005, 0.001, 0.002}}, {"increments", 100}}}}});
        c.push_back(dmg);
        std::vector<Parameter> beamp = dp;
        for (auto x : {par("length", "Length", "m", 0.5), par("width", "Width", "m", 0.03), par("thickness", "Thickness", "m", 0.005),
                       par("density", "Density", "kg/m^3", 2500.0), par("elements", "Elements", "", 40)})
            beamp.push_back(x);
        ModelCard beam{"mechanics.damaged-cantilever", "A cantilever accumulating damage", "mechanical", "implemented", dv, beamp,
                       {"Euler-Bernoulli elements; damage per element from its extreme-fibre strain, uniform over the section",
                        "tip deflection prescribed (displacement control)", "the modes from the damaged stiffness and the consistent mass"},
                       {"damage through the thickness", "crack growth", "fatigue (cycle counting)"},
                       "slender beams; the power law (no softening) for mesh-objective results", "fixed point of damage and shape per step; generalized eigenproblem",
                       "the Euler-Bernoulli modes of the undamaged beam"};
        beam.loadings = json::array({{{"id", "three-cycles"}, {"title", "Three cycles to 5, 10 and 20 mm"}, {"loading", {{"amplitudes", {0.005, 0.01, 0.02}}, {"increments", 40}}}}});
        c.push_back(beam);
        ModelCard zener{"mechanics.viscoelastic.sls", "Standard linear solid (viscoelastic)", "mechanical", "implemented",
                        {var("branchStress", "Maxwell branch stress", "Pa", VariableClass::History, "the internal variable")},
                        {par("relaxedModulus", "Relaxed modulus", "Pa", 1e9), par("branchModulus", "Branch modulus", "Pa", 2e9), par("relaxationTime", "Relaxation time", "s", 0.1)},
                        {"linear viscoelasticity, one relaxation time", "uniaxial"},
                        {"nonlinear viscoelasticity", "a spectrum of relaxation times", "temperature-time superposition"},
                        "polymers in their linear range", "exact exponential integration for strain linear within a step", "Ferry, Viscoelastic Properties of Polymers"};
        zener.loadings = json::array({{{"id", "relaxation"}, {"title", "Relaxation from 0.1 %"}, {"loading", {{"kind", "relaxation"}, {"strain", 0.001}}}},
                                      {{"id", "harmonic"}, {"title", "Harmonic at w tau = 1"}, {"loading", {{"kind", "harmonic"}, {"amplitude", 0.001}, {"cycles", 5}}}}});
        c.push_back(zener);
        ModelCard pc{"thermal.phase-change.slab", "Melting slab (enthalpy method)", "thermal", "implemented",
                     {var("temperature", "Temperature", "K", VariableClass::Reversible), var("liquidFraction", "Liquid fraction", "", VariableClass::Reversible,
                                                                                            "reversible: melting and freezing at the same temperature")},
                     {par("length", "Slab length", "m", 0.05), par("cells", "Cells", "", 400), par("density", "Density", "kg/m^3", 800.0),
                      par("solidSpecificHeat", "Specific heat (solid)", "J/(kg K)", 2000.0), par("liquidSpecificHeat", "Specific heat (liquid)", "J/(kg K)", 2200.0),
                      par("solidConductivity", "Conductivity (solid)", "W/(m K)", 0.25), par("liquidConductivity", "Conductivity (liquid)", "W/(m K)", 0.15),
                      par("meltingTemperature", "Melting temperature", "K", 310.0), par("latentHeat", "Latent heat", "J/kg", 2.0e5),
                      par("initialTemperature", "Initial temperature", "K", 310.0), par("wallTemperature", "Wall temperature (x = 0)", "K", 330.0)},
                     {"conduction only (no convection in the melt)", "a sharp melting point", "equal densities of the phases"},
                     {"undercooling and nucleation kinetics", "volume change", "convection in the liquid"},
                     "pure substances or eutectics; the explicit step below its stability limit", "explicit enthalpy method, conservative",
                     "Carslaw and Jaeger, Conduction of Heat in Solids (the Neumann solution)"};
        pc.loadings = json::array({{{"id", "hour"}, {"title", "Melt for one hour"}, {"loading", {{"duration", 3600.0}, {"snapshots", 12}}}}});
        c.push_back(pc);
        ModelCard pre{"em.preisach", "Preisach hysteresis (magnetization)", "electromagnetic", "implemented",
                      {var("relays", "Hysteron states", "", VariableClass::History, "the memory: alternating extrema of the field history"),
                       var("magnetization", "Magnetization", "A/m", VariableClass::Irreversible)},
                      {par("saturationMagnetization", "Saturation magnetization (irreversible)", "A/m", 1.2e6),
                       par("saturationField", "Saturation field", "A/m", 1000.0), par("reversibleSusceptibility", "Reversible susceptibility", "", 50.0),
                       par("levels", "Relay levels", "", 200), par("coercivity", "Coercivity (gaussian density)", "A/m", 120.0),
                       par("coerciveSpread", "Coercive spread", "A/m", 40.0), par("interactionSpread", "Interaction spread", "A/m", 60.0)},
                      {"scalar (one field direction)", "rate independent (no eddy currents)", "classical Preisach: wiping out, congruent minor loops"},
                      {"vector hysteresis", "eddy-current and excess losses", "temperature dependence"},
                      "soft and hard magnetic materials along one axis", "relay hysterons on a grid; density uniform or gaussian ('density' parameter)",
                      "Mayergoyz, Mathematical Models of Hysteresis (1991)"};
        pre.loadings = json::array({{{"id", "loops"}, {"title", "Major loop, then a minor loop"}, {"loading", {{"amplitudes", {1000.0, 300.0}}, {"increments", 200}}}},
                                    {{"id", "core"}, {"title", "A toroidal core at 50 Hz"},
                                     {"loading", {{"amplitudes", {800.0}}, {"increments", 200}, {"core", {{"turns", 200}, {"meanRadius", 0.02}, {"crossSection", 1e-4}, {"frequencyHz", 50.0}}}}}}});
        c.push_back(pre);
        ModelCard evo{"coupled.evolution", "Time-stepped evolution of a physical system", "multiphysics", "implemented",
                      {var("temperature", "Temperature field", "K", VariableClass::Reversible),
                       var("properties", "Temperature-dependent properties", "", VariableClass::Reversible, "the system's material laws at each step"),
                       var("stress", "Thermal stress", "Pa", VariableClass::Reversible)},
                      {par("duration", "Duration", "s", 60.0), par("steps", "Steps", "", 20), par("substeps", "Thermal substeps per step", "", 4)},
                      {"each step a coupled run continuing from the previous temperature field", "first order in time between steps"},
                      {"state variables of other models (plastic strain, damage) inside the coupled solvers"},
                      "layered-slab systems with a thermal domain", "the Coupled Energy-Matter Engine per step", "ADR-000225"};
        evo.loadings = json::array();
        c.push_back(evo);
        c.push_back(notImplemented("mechanics.fracture.propagation", "Crack propagation", "mechanical",
                                   "not implemented: no crack geometry, energy-release-rate or cohesive-zone solver"));
        c.push_back(notImplemented("mechanics.fatigue", "Fatigue (S-N, cycle counting)", "mechanical", "not implemented: no cycle counting or fatigue damage law"));
        c.push_back(notImplemented("mechanics.finite-strain-plasticity", "Finite-strain plasticity", "mechanical", "not implemented: small strain only"));
        c.push_back(notImplemented("mechanics.hyperelastic", "Hyperelasticity", "mechanical", "not implemented: no finite-strain solid solver"));
        c.push_back(notImplemented("em.nonlinear-dielectric", "Nonlinear dielectrics (ferroelectric)", "electromagnetic", "not implemented"));
        c.push_back(notImplemented("optics.nonlinear", "Nonlinear optics (Kerr, harmonic generation)", "optical", "not implemented: the solvers are linear"));
        c.push_back(notImplemented("acoustics.nonlinear", "Nonlinear acoustics", "acoustic", "not implemented: the acoustic solvers are linear"));
        c.push_back(notImplemented("chemistry.reaction", "Reaction kinetics and composition", "chemical", "not implemented"));
        return c;
    }();
    return cards;
}

const ModelCard* findMaterialStateModel(std::string_view id)
{
    for (const ModelCard& c : materialStateModels())
        if (c.id == id)
            return &c;
    return nullptr;
}

R runMaterialStateStudy(const json& study, JobContext* ctx)
{
    if (!study.is_object())
        return R::failure("a study is an object {model, parameters, loading}");
    const std::string id = study.value("model", "");
    const ModelCard* card = findMaterialStateModel(id);
    if (!card)
        return R::failure("unknown material-state model '" + id + "'");
    if (card->status != "implemented")
        return R::failure("the model '" + card->title + "' is not implemented: " + (card->excluded.empty() ? std::string() : card->excluded.front()));
    try {
        if (id == "mechanics.j2.point")
            return runJ2Point(*card, study);
        if (id == "mechanics.j2.bar")
            return runJ2Bar(*card, study);
        if (id == "mechanics.damage.point")
            return runDamagePoint(*card, study);
        if (id == "mechanics.damaged-cantilever")
            return runDamagedCantilever(*card, study, ctx);
        if (id == "mechanics.viscoelastic.sls")
            return runZener(*card, study);
        if (id == "thermal.phase-change.slab")
            return runPhaseChange(*card, study, ctx);
        if (id == "em.preisach")
            return runPreisach(*card, study);
        if (id == "coupled.evolution")
            return R::failure("an evolution runs a physical system of a project: MaterialStateService.Evolve (or a study naming \"system\")");
    } catch (const std::exception& e) {
        return R::failure(std::string("invalid study: ") + e.what());
    }
    return R::failure("no runner for '" + id + "'");
}

}  // namespace resonant::coupling
