#include "coupling/AccountingAdapters.h"

#include "electromagnetics/RadiationForce.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace resonant::coupling {

using nlohmann::json;
namespace acc = accounting;
using acc::Check;
using acc::CheckStatus;
using acc::Entity;
using acc::EntityKind;
using acc::Exchange;
using acc::Family;
using acc::Holding;
using acc::Ledger;
using acc::Vec3;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kC = 299792458.0;

Entity entity(std::string id, std::string name, EntityKind kind, std::string domain, std::string object = {})
{
    Entity e;
    e.id = std::move(id);
    e.name = std::move(name);
    e.kind = kind;
    e.domain = std::move(domain);
    e.object = std::move(object);
    return e;
}

Exchange scalar(std::string id, std::string from, std::string to, Family f, std::string category, std::string mechanism, double amount)
{
    Exchange x;
    x.id = std::move(id);
    x.from = std::move(from);
    x.to = std::move(to);
    x.family = f;
    x.category = std::move(category);
    x.mechanism = std::move(mechanism);
    x.amount = amount;
    return x;
}

Exchange vectorial(std::string id, std::string from, std::string to, Family f, std::string category, std::string mechanism, const Vec3& v)
{
    Exchange x = scalar(std::move(id), std::move(from), std::move(to), f, std::move(category), std::move(mechanism), 0.0);
    x.isVector = true;
    x.vector = v;
    return x;
}

Holding scalarHolding(std::string id, std::string entity, Family f, std::string form, double initial, double final, bool recoverable = true)
{
    Holding h;
    h.id = std::move(id);
    h.entity = std::move(entity);
    h.family = f;
    h.form = std::move(form);
    h.initial = initial;
    h.final = final;
    h.recoverable = recoverable;
    return h;
}

std::string number(double v)
{
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.4g", v);
    return buf;
}

void judge(Check& c, double relative, double tolerance, double warning)
{
    c.relative = relative;
    c.tolerance = tolerance;
    c.warning = warning;
    c.status = relative <= tolerance ? CheckStatus::Verified : relative <= warning ? CheckStatus::Warning : CheckStatus::Exceeded;
}

const oscillators::NetworkMode* nearestMode(const std::vector<oscillators::NetworkMode>& modes, double f)
{
    const oscillators::NetworkMode* best = nullptr;
    for (const auto& m : modes)
        if (!best || std::abs(m.undampedHz - f) < std::abs(best->undampedHz - f))
            best = &m;
    return best;
}

}  // namespace

// --- lumped networks ----------------------------------------------------------------------

Result<Ledger> networkLedger(const oscillators::Network& net, const oscillators::TransientRecord& rec, const NetworkAccounting& o)
{
    using R = Result<Ledger>;
    if (!rec.elementFlows)
        return R::failure("the transient was run without element flows (TransientOptions::elementFlows)");
    if (rec.time.empty())
        return R::failure("the transient has no samples");
    const double an = std::sqrt(o.axis[0] * o.axis[0] + o.axis[1] * o.axis[1] + o.axis[2] * o.axis[2]);
    if (!(an > 0.0))
        return R::failure("the axis has no direction");
    const Vec3 axis{o.axis[0] / an, o.axis[1] / an, o.axis[2] / an};
    const auto along = [&](double s) { return Vec3{s * axis[0], s * axis[1], s * axis[2]}; };
    const auto alongAll = [&](const std::vector<double>& s) {
        std::vector<Vec3> out;
        for (double v : s)
            out.push_back(along(v));
        return out;
    };
    const auto negated = [](std::vector<double> v) {
        for (double& x : v)
            x = -x;
        return v;
    };
    const Family P = o.rotational ? Family::AngularMomentum : Family::LinearMomentum;
    const Family F = o.rotational ? Family::Torque : Family::Force;
    const std::string dom = "mechanical";
    Ledger l;
    l.id = "network";
    l.title = o.title.empty() ? std::string("Lumped network") : o.title;
    l.sourceKind = o.rotational ? "rotational network transient" : "lumped network transient";
    l.solver = "oscillators.lumped-network (Runge-Kutta 4, element flows)";
    l.solverVersion = oscillators::kNetworkModelVersion;
    l.basis = acc::Basis::Interval;
    l.t0 = rec.time.front();
    l.t1 = rec.time.back();
    l.times = rec.time;
    const int n = net.size();
    const size_t samples = rec.time.size();
    bool driven = false, grounded = false, damped = false;
    for (int i = 0; i < n; ++i)
        for (size_t k = 0; k < samples; ++k)
            driven = driven || rec.appliedImpulse[static_cast<size_t>(i)][k] != 0.0 || rec.appliedWork[static_cast<size_t>(i)][k] != 0.0;
    for (const auto& e : rec.elements) {
        grounded = grounded || e.j < 0;
        damped = damped || e.kind == "damper";
    }
    std::vector<std::string> body(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        const auto u = static_cast<size_t>(i);
        body[u] = "body:" + net.names[u];
        l.entities.push_back(entity(body[u], net.names[u], EntityKind::Body, dom, u < o.objects.size() ? o.objects[u] : std::string()));
    }
    if (driven)
        l.entities.push_back(entity("driver", o.rotational ? "Applied torque" : "Applied force", EntityKind::Source, dom));
    if (grounded)
        l.entities.push_back(entity("ground", "Ground (fixed support)", EntityKind::Environment, dom));
    if (damped)
        l.entities.push_back(entity("heat", "Heat (dissipation)", EntityKind::Sink, "thermal"));
    std::set<std::string> used;
    std::vector<std::string> element;
    for (const auto& e : rec.elements) {
        std::string id = "element:" + e.name;
        for (int k = 2; used.count(id); ++k)
            id = "element:" + e.name + " (" + std::to_string(k) + ")";
        used.insert(id);
        element.push_back(id);
        Entity en = entity(id, e.name, EntityKind::Element, dom);
        en.note = e.kind == "spring" ? (o.rotational ? "torsional spring, " : "spring, ") + number(e.coefficient) + (o.rotational ? " N m/rad" : " N/m")
                                     : "damper (" + e.mechanism + "), " + number(e.coefficient) + (o.rotational ? " N m s/rad" : " N s/m");
        l.entities.push_back(en);
    }
    // Holdings: each body's kinetic energy and momentum, each spring's elastic energy.
    for (int i = 0; i < n; ++i) {
        const auto u = static_cast<size_t>(i);
        const double m = net.masses[u];
        Holding ke;
        ke.id = "kinetic:" + net.names[u];
        ke.entity = body[u];
        ke.family = Family::Energy;
        ke.form = o.rotational ? "rotational kinetic" : "kinetic";
        ke.method = o.rotational ? "J w^2 / 2" : "m v^2 / 2";
        std::vector<double> p;
        for (double v : rec.velocity[u]) {
            ke.history.push_back(0.5 * m * v * v);
            p.push_back(m * v);
        }
        ke.initial = ke.history.front();
        ke.final = ke.history.back();
        l.holdings.push_back(ke);
        Holding mo;
        mo.id = (o.rotational ? "angular momentum:" : "momentum:") + net.names[u];
        mo.entity = body[u];
        mo.family = P;
        mo.form = o.rotational ? "angular momentum" : "momentum";
        mo.method = o.rotational ? "J w along the axis" : "m v along the axis";
        mo.isVector = true;
        mo.vectorHistory = alongAll(p);
        mo.initialVector = mo.vectorHistory.front();
        mo.finalVector = mo.vectorHistory.back();
        l.holdings.push_back(mo);
    }
    for (size_t e = 0; e < rec.elements.size(); ++e) {
        const auto& ef = rec.elements[e];
        if (ef.kind != "spring")
            continue;
        Holding h;
        h.id = "elastic:" + ef.name;
        h.entity = element[e];
        h.family = Family::Energy;
        h.form = "elastic";
        h.method = o.rotational ? "k (theta_i - theta_j)^2 / 2" : "k (x_i - x_j)^2 / 2";
        h.history = ef.stored;
        h.initial = h.history.front();
        h.final = h.history.back();
        l.holdings.push_back(h);
    }
    // Exchanges, with their cumulative histories.
    if (driven)
        for (int i = 0; i < n; ++i) {
            const auto u = static_cast<size_t>(i);
            Exchange w = scalar("drive work:" + net.names[u], "driver", body[u], Family::Energy, "work", o.rotational ? "applied torque" : "applied force",
                                rec.appliedWork[u].back());
            w.history = rec.appliedWork[u];
            w.method = "integral of f v (the integrator's stages)";
            l.exchanges.push_back(w);
            Exchange imp = vectorial("drive impulse:" + net.names[u], "driver", body[u], P, "impulse", o.rotational ? "applied torque" : "applied force",
                                     along(rec.appliedImpulse[u].back()));
            imp.vectorHistory = alongAll(rec.appliedImpulse[u]);
            imp.method = "integral of f (the integrator's stages)";
            l.exchanges.push_back(imp);
        }
    for (size_t e = 0; e < rec.elements.size(); ++e) {
        const auto& ef = rec.elements[e];
        const std::string& el = element[e];
        const std::string bi = body[static_cast<size_t>(ef.i)];
        const std::string other = ef.j >= 0 ? body[static_cast<size_t>(ef.j)] : std::string("ground");
        const std::string mech = ef.kind == "spring" ? (o.rotational ? "torsional spring" : "elastic") : ef.mechanism;
        // Energy: the work each body does on the element (minus the element's work on it).
        Exchange wi = scalar("work:" + net.names[static_cast<size_t>(ef.i)] + "->" + ef.name, bi, el, Family::Energy, "work", mech, -ef.workOnI.back());
        wi.history = negated(ef.workOnI);
        wi.method = "minus the integral of the element's force times the body's velocity";
        l.exchanges.push_back(wi);
        if (ef.j >= 0) {
            Exchange wj = scalar("work:" + net.names[static_cast<size_t>(ef.j)] + "->" + ef.name, other, el, Family::Energy, "work", mech, -ef.workOnJ.back());
            wj.history = negated(ef.workOnJ);
            wj.method = wi.method;
            l.exchanges.push_back(wj);
        }
        if (ef.kind == "damper") {
            std::vector<double> lost;
            for (size_t k = 0; k < samples; ++k)
                lost.push_back(-(ef.workOnI[k] + ef.workOnJ[k]));
            Exchange d = scalar("dissipation:" + ef.name, el, "heat", Family::Energy, "dissipation", ef.mechanism, lost.back());
            d.history = lost;
            d.method = "the work the damper absorbs from its two ends";
            l.exchanges.push_back(d);
        }
        // Momentum: the impulse of the element's force on each end (opposite on the two ends).
        Exchange ji = vectorial("impulse:" + ef.name + "->" + net.names[static_cast<size_t>(ef.i)], el, bi, P, "impulse", mech, along(ef.impulseOnI.back()));
        ji.vectorHistory = alongAll(ef.impulseOnI);
        ji.method = "integral of the element's force on the body";
        l.exchanges.push_back(ji);
        Exchange jj = vectorial("impulse:" + ef.name + "->" + (ef.j >= 0 ? net.names[static_cast<size_t>(ef.j)] : std::string("ground")), el, other, P,
                                "impulse", mech, along(-ef.impulseOnI.back()));
        jj.vectorHistory = alongAll(negated(ef.impulseOnI));
        jj.method = ji.method + " (the reaction)";
        l.exchanges.push_back(jj);
        // Force records: the pair.
        acc::ForceRecord fi;
        fi.id = "force:" + ef.name + "->" + net.names[static_cast<size_t>(ef.i)];
        fi.family = F;
        fi.category = ef.kind == "spring" ? "elastic" : "viscous";
        fi.model = "oscillators.lumped-network";
        fi.source = el;
        fi.target = bi;
        fi.mechanism = mech;
        fi.method = ef.kind == "spring" ? "-k (x_i - x_j)" : "-c (v_i - v_j)";
        fi.vector = along(ef.force.back());
        fi.history = alongAll(ef.force);
        acc::ForceRecord fj = fi;
        fj.id = "force:" + ef.name + "->" + (ef.j >= 0 ? net.names[static_cast<size_t>(ef.j)] : std::string("ground"));
        fj.target = other;
        fj.vector = along(-ef.force.back());
        fj.history = alongAll(negated(ef.force));
        if (ef.j < 0)
            fj.category = "constraint";
        fi.pair = fj.id;
        fj.pair = fi.id;
        l.forces.push_back(fi);
        l.forces.push_back(fj);
    }
    if (driven)
        for (int i = 0; i < n; ++i) {
            const auto u = static_cast<size_t>(i);
            acc::ForceRecord f;
            f.id = "force:applied->" + net.names[u];
            f.family = F;
            f.category = "applied";
            f.model = "the study's loading";
            f.source = "driver";
            f.target = body[u];
            f.mechanism = o.rotational ? "applied torque" : "applied force";
            f.method = "prescribed";
            f.vector = along(rec.appliedForce[u].back());
            f.history = alongAll(rec.appliedForce[u]);
            l.forces.push_back(f);
        }
    l.numerical.push_back({Family::Energy, "Runge-Kutta integration", "the largest |supplied - stored change - dissipated| of the whole network over the run",
                           rec.residualAbsolute});
    l.notes.push_back("massless elements hold no momentum: the impulses on their two ends are equal and opposite (an action-reaction pair)");
    l.notes.push_back("every flow is integrated with the same Runge-Kutta stages as the state (" + std::to_string(rec.steps) + " steps of " + number(rec.dt) +
                      " s)");
    if (Status v = l.validate(); !v)
        return R::failure(v.message());
    return l;
}

Result<Ledger> drivenNetworkLedger(const oscillators::Network& net, const std::vector<std::complex<double>>& force, double f, const NetworkAccounting& o)
{
    using R = Result<Ledger>;
    auto pts = oscillators::drivenResponse(net, force, {f});
    if (!pts)
        return R::failure(pts.message());
    const auto& p = pts.value().front();
    Ledger l;
    l.id = "driven";
    l.title = o.title.empty() ? std::string("Driven steady state") : o.title;
    l.sourceKind = "lumped network, driven steady state";
    l.solver = "oscillators.lumped-network (harmonic response)";
    l.solverVersion = oscillators::kNetworkModelVersion;
    l.basis = acc::Basis::Rate;
    l.entities.push_back(entity("network", l.title, EntityKind::Body, "mechanical"));
    l.entities.push_back(entity("driver", o.rotational ? "Applied torque" : "Applied force", EntityKind::Source, "mechanical"));
    l.entities.push_back(entity("heat", "Heat (dissipation)", EntityKind::Sink, "thermal"));
    Exchange s = scalar("supplied", "driver", "network", Family::Energy, "work", "harmonic force", p.suppliedPower);
    s.method = "time average of Re(f) Re(v)";
    l.exchanges.push_back(s);
    double lost = 0.0;
    for (const auto& [mech, w] : p.dissipated) {
        Exchange d = scalar("dissipation:" + mech, "network", "heat", Family::Energy, "dissipation", mech, w);
        d.method = "time average of v^T C_m v";
        l.exchanges.push_back(d);
        lost += w;
    }
    l.holdings.push_back(scalarHolding("storage", "network", Family::Energy, "stored (steady: no change)", 0.0, 0.0));
    const double E = p.kineticEnergy + p.potentialEnergy, w = 2.0 * kPi * f;
    for (const auto& [kind, value] : std::vector<std::pair<std::string, double>>{{"stored.time-averaged", E}, {"stored.kinetic", p.kineticEnergy},
                                                                                 {"stored.potential", p.potentialEnergy}}) {
        acc::Quantity q;
        q.id = kind;
        q.family = Family::Energy;
        q.kind = kind;
        q.domain = "mechanical";
        q.value = value;
        q.source = "network";
        q.validity = "time-averaged over a period of the steady state at " + number(f) + " Hz";
        l.quantities.push_back(q);
    }
    // Q = w E / P per mechanism against the modal Q.
    Check c;
    c.model = "resonance.quality-factor";
    c.title = "Q = w E / P by mechanism";
    c.family = Family::Energy;
    c.scope = "relation";
    c.unit = "1";
    auto modes = oscillators::naturalModes(net);
    const oscillators::NetworkMode* mode = modes ? nearestMode(modes.value(), f) : nullptr;
    if (!mode || !(lost > 0.0)) {
        c.status = CheckStatus::NotApplicable;
        c.notes.push_back(!mode ? "the network has no oscillating mode" : "nothing is dissipated: Q is infinite");
    } else if (mode->quality < 5.0) {
        c.status = CheckStatus::NotApplicable;
        c.notes.push_back("Q = " + number(mode->quality) + ": too heavily damped for Q = w E / P");
    } else if (std::abs(f - mode->undampedHz) > 0.05 * mode->undampedHz / mode->quality) {
        c.status = CheckStatus::NotApplicable;
        c.notes.push_back("not driven at the mode's frequency (" + number(mode->undampedHz) + " Hz)");
    } else {
        double worstRel = 0.0;
        json per = json::array();
        for (const auto& [mech, P] : p.dissipated) {
            const double qd = w * E / P;
            const auto it = mode->qualityByMechanism.find(mech);
            const double qm = it == mode->qualityByMechanism.end() ? 0.0 : it->second;
            const double rel = qm > 0.0 ? std::abs(qd - qm) / qm : 1.0;
            worstRel = std::max(worstRel, rel);
            per.push_back({{"mechanism", mech}, {"driven", qd}, {"modal", qm}, {"relative", rel}});
        }
        const double qTotal = w * E / lost;
        const double relTotal = std::abs(qTotal - mode->quality) / mode->quality;
        worstRel = std::max(worstRel, relTotal);
        c.details = {{"frequencyHz", f}, {"modeHz", mode->undampedHz}, {"storedEnergy", E}, {"lossPower", lost}, {"qualityDriven", qTotal},
                     {"qualityModal", mode->quality}, {"byMechanism", per}};
        // Other modes store a little energy too: agreement to about 1/Q^2 of the neighbours is expected.
        const double expected = std::max(1e-9, 2.0 / (mode->quality * mode->quality));
        judge(c, worstRel, std::max(1e-6, expected), 10.0 * std::max(1e-6, expected));
        c.notes.push_back("the driven and modal values agree to O(1/Q^2) when other modes contribute");
    }
    l.checks.push_back(c);
    l.notes.push_back("a steady state: the rates are time averages; the stored energy is a quantity, not a holding");
    if (Status v = l.validate(); !v)
        return R::failure(v.message());
    return l;
}

Check buildupDecayCheck(const oscillators::Network& net, const oscillators::TransientRecord& rec, double f, const std::vector<std::complex<double>>& force,
                        double off, double tolerance)
{
    Check c;
    c.model = "resonance.buildup-decay";
    c.title = "Buildup and decay of a resonator";
    c.family = Family::Energy;
    c.scope = "relation";
    c.unit = "1";
    auto modes = oscillators::naturalModes(net);
    auto ss = oscillators::drivenResponse(net, force, {f});
    const oscillators::NetworkMode* mode = modes ? nearestMode(modes.value(), f) : nullptr;
    if (!mode || !ss || !(mode->decayRate > 0.0) || mode->quality < 5.0) {
        c.status = CheckStatus::NotApplicable;
        c.notes.push_back("needs a lightly damped mode (Q >= 5) near the drive frequency");
        return c;
    }
    const double g = mode->decayRate, T = 1.0 / f;
    const double Ess = ss.value().front().kineticEnergy + ss.value().front().potentialEnergy;
    std::vector<double> E;
    for (size_t k = 0; k < rec.time.size(); ++k) {
        double e = rec.couplingEnergy[k];
        for (const auto& coord : rec.energy)
            e += coord[k];
        E.push_back(e);
    }
    // Buildup: the energy averaged over the period ending at tb against E_ss (1 - e^(-g t))^2.
    const double tb = std::min(off, 2.0 / g);
    double sum = 0.0, ref = 0.0;
    int count = 0;
    for (size_t k = 0; k < rec.time.size(); ++k)
        if (rec.time[k] > tb - T && rec.time[k] <= tb) {
            sum += E[k];
            ref += Ess * std::pow(1.0 - std::exp(-g * rec.time[k]), 2);
            ++count;
        }
    // Decay: the slope of ln E after the drive is off (from two periods after).
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    int m = 0;
    for (size_t k = 0; k < rec.time.size(); ++k)
        if (rec.time[k] >= off + 2.0 * T && E[k] > 0.0) {
            const double x = rec.time[k], y = std::log(E[k]);
            sx += x;
            sy += y;
            sxx += x * x;
            sxy += x * y;
            ++m;
        }
    if (count < 3 || m < 10) {
        c.status = CheckStatus::InsufficientData;
        c.notes.push_back("too few samples in the buildup or the decay window");
        return c;
    }
    const double slope = (m * sxy - sx * sy) / (m * sxx - sx * sx);
    const double buildRel = std::abs(sum - ref) / ref;
    const double decayRel = std::abs(-slope - 2.0 * g) / (2.0 * g);
    c.details = {{"decayRate", g}, {"energyDecayRate", -slope}, {"expectedEnergyDecayRate", 2.0 * g}, {"steadyStateEnergy", Ess},
                 {"buildupTime", tb}, {"buildupEnergy", sum / count}, {"buildupExpected", ref / count}, {"buildupRelative", buildRel},
                 {"decayRelative", decayRel}, {"modeHz", mode->undampedHz}, {"quality", mode->quality}};
    judge(c, std::max(buildRel, decayRel), tolerance, 3.0 * tolerance);
    c.notes.push_back("the light-damping envelope (one dominant mode, drive at resonance)");
    return c;
}

// --- the coupled engine -------------------------------------------------------------------------

Result<Ledger> coupledLedger(const json& j, const json& audit, const std::string& title)
{
    using R = Result<Ledger>;
    if (!j.is_object() || !j.contains("terms"))
        return R::failure("not a coupled ledger");
    Ledger l;
    l.id = "coupled";
    l.title = title;
    l.sourceKind = "coupled engine run";
    l.solver = "multiphysics.orchestrator";
    l.solverVersion = "1";
    l.basis = j.value("mode", std::string()).rfind("steady", 0) == 0 ? acc::Basis::Rate : acc::Basis::Interval;
    std::map<std::string, std::string> participants;  // id -> domain
    for (const json& t : j["terms"])
        participants[t.value("participant", std::string())] = t.value("domain", std::string());
    for (const json& t : j.value("transfers", json::array())) {
        participants[t.value("sourceParticipant", std::string())] = t.value("sourceDomain", std::string());
        participants[t.value("destinationParticipant", std::string())] = t.value("destinationDomain", std::string());
    }
    participants.erase("");
    for (const auto& [id, domain] : participants)
        l.entities.push_back(entity(id, id + " (" + domain + ")", EntityKind::Participant, domain));
    // The outside: prefixed, since participants may be called "heat" or "environment".
    l.entities.push_back(entity("outside:sources", "External sources", EntityKind::Source, "general"));
    l.entities.push_back(entity("outside:environment", "Environment", EntityKind::Environment, "general"));
    l.entities.push_back(entity("outside:heat", "Heat (dissipation)", EntityKind::Sink, "thermal"));
    int k = 0;
    std::map<std::string, double> stored;
    for (const json& t : j["terms"]) {
        const std::string p = t.value("participant", std::string()), kind = t.value("kind", std::string()), mech = t.value("mechanism", std::string());
        const double v = t.value("value", 0.0);
        const std::string id = "term-" + std::to_string(++k);
        Exchange x;
        if (kind == "input")
            x = scalar(id, "outside:sources", p, Family::Energy, "input", mech, v);
        else if (kind == "boundary-out")
            x = scalar(id, p, "outside:environment", Family::Energy, "boundary", mech, v);
        else if (kind == "dissipated")
            x = scalar(id, p, "outside:heat", Family::Energy, "dissipation", mech, v);
        else if (kind == "stored") {
            stored[p] += v;
            continue;
        } else if (kind == "informational") {
            acc::Quantity q;
            q.id = id;
            q.family = l.basis == acc::Basis::Rate ? Family::Power : Family::Energy;
            q.kind = "informational";
            q.domain = t.value("domain", std::string());
            q.value = v;
            q.source = p;
            q.validity = mech + ": reported, not summed (" + t.value("description", std::string()) + ")";
            l.quantities.push_back(q);
            continue;
        } else {
            continue;  // transfer-out / transfer-in: the transfer records below
        }
        x.fromDomain = x.from == p ? t.value("domain", std::string()) : std::string();
        x.toDomain = x.to == p ? t.value("domain", std::string()) : std::string();
        x.region = t.value("region", std::string());
        x.method = "the participant's own term";
        x.note = t.value("description", std::string());
        l.exchanges.push_back(x);
    }
    for (const auto& [p, v] : stored) {
        Holding h = scalarHolding("stored:" + p, p, Family::Energy,
                                  l.basis == acc::Basis::Rate ? "stored (rate)" : "stored (change over the interval)", 0.0, v);
        h.method = "the participant's own term";
        l.holdings.push_back(h);
    }
    for (const json& t : j.value("transfers", json::array())) {
        const std::string id = "transfer:" + t.value("coupling", std::string());
        if (!t.value("energy", true)) {
            l.notes.push_back(t.value("coupling", std::string()) + ": transfers " + t.value("targetQuantity", std::string()) + " (not an energy)");
            continue;
        }
        Exchange x = scalar(id, t.value("sourceParticipant", std::string()), t.value("destinationParticipant", std::string()), Family::Energy, "transfer",
                            t.value("mechanism", std::string()), t.value("targetIntegral", t.value("amount", 0.0)));
        x.sourceAmount = t.value("sourceIntegral", x.amount);
        x.fromDomain = t.value("sourceDomain", std::string());
        x.toDomain = t.value("destinationDomain", std::string());
        x.region = t.value("region", std::string());
        x.method = t.value("method", std::string()) + " (" + t.value("model", std::string()) + ")";
        x.note = t.value("approximation", std::string());
        l.exchanges.push_back(x);
    }
    for (const json& p : audit.value("participants", json::array()))
        if (p.value("solverResidual", 0.0) != 0.0)
            l.numerical.push_back({Family::Energy, "solver of " + p.value("participant", std::string()), "the participant's own discretized balance",
                                   std::abs(p.value("solverResidual", 0.0))});
    for (Family f : {Family::LinearMomentum, Family::AngularMomentum})
        l.missing.push_back({f, "", "momentum and forces", "the coupled participants report none (radiation pressure, thermal-stress resultants)"});
    if (Status v = l.validate(); !v)
        return R::failure(v.message());
    return l;
}

Result<Ledger> evolutionLedger(const json& doc)
{
    using R = Result<Ledger>;
    const json history = doc.value("history", json::array());
    if (history.empty())
        return R::failure("the evolution has no steps");
    Ledger l;
    l.id = "evolution";
    l.title = "Evolution";
    l.sourceKind = "time-stepped evolution (coupled engine per step)";
    l.solver = "coupling.state-evolution";
    l.solverVersion = "1";
    l.basis = acc::Basis::Interval;
    l.times.push_back(0.0);
    std::map<std::string, size_t> exchangeIndex, holdingIndex;
    std::map<std::string, double> numerical;
    size_t step = 0;
    for (const json& s : history) {
        if (!s.contains("ledger"))
            return R::failure("an evolution step has no ledger");
        auto lk = coupledLedger(s["ledger"], s.value("energy", json::object()), "step");
        if (!lk)
            return R::failure(lk.message());
        if (lk.value().basis != acc::Basis::Interval)
            return R::failure("an evolution step is not accounted over its interval");
        ++step;
        l.times.push_back(s.value("time", 0.0));
        for (const Entity& e : lk.value().entities)
            if (!l.entity(e.id))
                l.entities.push_back(e);
        // Grow every history by the step (zero for what the step does not have).
        for (Exchange& x : l.exchanges)
            x.history.push_back(x.history.back());
        for (Holding& h : l.holdings)
            h.history.push_back(h.history.back());
        for (const Exchange& x : lk.value().exchanges) {
            const std::string key = x.from + "|" + x.to + "|" + x.category + "|" + x.mechanism;
            auto it = exchangeIndex.find(key);
            if (it == exchangeIndex.end()) {
                Exchange y = x;
                y.id = "x" + std::to_string(l.exchanges.size() + 1) + ":" + x.mechanism;
                y.amount = 0.0;
                y.sourceAmount = x.sourceAmount ? std::optional<double>(0.0) : std::nullopt;
                y.history.assign(step + 1, 0.0);
                it = exchangeIndex.emplace(key, l.exchanges.size()).first;
                l.exchanges.push_back(y);
            }
            Exchange& y = l.exchanges[it->second];
            y.amount += x.amount;
            if (y.sourceAmount)
                *y.sourceAmount += x.sourceAmount.value_or(x.amount);
            y.history.back() += x.amount;
        }
        for (const Holding& h : lk.value().holdings) {
            auto it = holdingIndex.find(h.entity);
            if (it == holdingIndex.end()) {
                Holding g = h;
                g.id = "stored:" + h.entity;
                g.form = "stored (change since the start)";
                g.initial = g.final = 0.0;
                g.history.assign(step + 1, 0.0);
                it = holdingIndex.emplace(h.entity, l.holdings.size()).first;
                l.holdings.push_back(g);
            }
            Holding& g = l.holdings[it->second];
            g.final += h.change()[0];
            g.history.back() += h.change()[0];
        }
        for (const auto& n : lk.value().numerical)
            numerical[n.source] += n.amount;
    }
    for (const auto& [source, amount] : numerical)
        l.numerical.push_back({Family::Energy, source, "summed over the steps", amount});
    l.t0 = 0.0;
    l.t1 = l.times.back();
    for (Family f : {Family::LinearMomentum, Family::AngularMomentum})
        l.missing.push_back({f, "", "momentum and forces", "the coupled participants report none"});
    l.notes.push_back(std::to_string(step) + " steps, each a coupled run continuing from the previous temperature field");
    if (Status v = l.validate(); !v)
        return R::failure(v.message());
    return l;
}

// --- radiation on a layered body ------------------------------------------------------------------

Result<Ledger> radiationLedger(const em::LayeredSolution& s, double area, int points, double tolerance)
{
    using R = Result<Ledger>;
    if (!(area > 0.0))
        return R::failure("the illuminated area must be positive");
    auto fine = em::radiationForce(s, points);
    if (!fine)
        return R::failure(fine.message());
    auto coarse = em::radiationForce(s, std::max(9, points / 2));
    if (!coarse)
        return R::failure(coarse.message());
    const auto& f = fine.value();
    const double I = f.intensity, A = area;
    Ledger l;
    l.id = "radiation";
    l.title = "Plane wave on a layered body";
    l.sourceKind = "layered electromagnetic solution";
    l.solver = em::kLayeredSolverId;
    l.solverVersion = em::kLayeredSolverVersion;
    l.basis = acc::Basis::Rate;
    l.entities.push_back(entity("wave.incident", "Incident wave", EntityKind::Source, "electromagnetic"));
    l.entities.push_back(entity("wave.reflected", "Reflected wave", EntityKind::Environment, "electromagnetic"));
    l.entities.push_back(entity("wave.transmitted", "Transmitted wave", EntityKind::Environment, "electromagnetic"));
    l.entities.push_back(entity("body", "Layered body", EntityKind::Body, "electromagnetic"));
    l.entities.push_back(entity("heat", "Heat (absorption)", EntityKind::Sink, "thermal"));
    const auto zv = [&](double v) { return Vec3{0.0, 0.0, v}; };
    Exchange in = scalar("incident", "wave.incident", "body", Family::Energy, "radiation", "incident plane wave", I * A);
    in.method = "the incident intensity times the area";
    Exchange re = scalar("reflected", "body", "wave.reflected", Family::Energy, "radiation", "reflection", f.reflectance * I * A);
    re.method = "|r|^2 from the transfer-matrix solution";
    Exchange tr = scalar("transmitted", "body", "wave.transmitted", Family::Energy, "radiation", "transmission", f.transmittance * I * A);
    tr.method = "the transmitted Poynting flux";
    Exchange ab = scalar("absorbed", "body", "heat", Family::Energy, "dissipation", "dielectric and conduction loss", f.absorbedLocal * A);
    ab.method = "the volume integral of (1/2) w eps0 eps'' |E|^2 (Simpson)";
    for (Exchange* x : {&in, &re, &tr, &ab})
        l.exchanges.push_back(*x);
    Holding e = scalarHolding("field energy", "body", Family::Energy, "field (steady: no change)", 0.0, 0.0);
    e.note = "the time-averaged field energy inside does not change in the steady state";
    l.holdings.push_back(e);
    // Momentum: the waves' momentum fluxes; the body's momentum changes at the rate of the Lorentz force on its currents.
    Exchange pin = vectorial("momentum incident", "wave.incident", "body", Family::LinearMomentum, "flux", "incident plane wave", zv(I * A / kC));
    pin.method = "I A / c along the propagation";
    Exchange pre = vectorial("momentum reflected", "body", "wave.reflected", Family::LinearMomentum, "flux", "reflection",
                             zv(-f.reflectance * I * A / kC));
    pre.method = "R I A / c, carried along -z";
    Exchange ptr = vectorial("momentum transmitted", "body", "wave.transmitted", Family::LinearMomentum, "flux", "transmission",
                             zv(f.transmittance * I * A / kC));
    ptr.method = "T I A / c along +z";
    for (Exchange* x : {&pin, &pre, &ptr})
        l.exchanges.push_back(*x);
    Holding mom;
    mom.id = "mechanical momentum";
    mom.entity = "body";
    mom.family = Family::LinearMomentum;
    mom.form = "mechanical momentum (rate: the Lorentz force on the induced currents)";
    mom.isVector = true;
    mom.finalVector = zv(f.pressureVolume * A);
    mom.method = "the volume integral of (1/2) Re(J conj(mu0 H)) (Simpson)";
    mom.note = "the body is free at the instant considered; a held body passes the force to its support";
    l.holdings.push_back(mom);
    acc::ForceRecord force;
    force.id = "radiation force";
    force.category = "radiation-pressure";
    force.model = em::kLayeredSolverId;
    force.source = "wave.incident";
    force.target = "body";
    force.mechanism = "the time-averaged Lorentz force on the polarization and conduction currents";
    force.method = "volume integral of the force density (" + std::to_string(f.points) + " points)";
    force.vector = zv(f.pressureVolume * A);
    force.time = "time-averaged";
    force.distribution = "over the depth (the force-density profile); uniform over " + number(A) + " m^2";
    l.forces.push_back(force);
    l.numerical.push_back({Family::Energy, "Simpson's rule", "the change of the absorbed power when the points are halved",
                           std::abs(f.absorbedLocal - coarse.value().absorbedLocal) * A});
    l.numerical.push_back({Family::LinearMomentum, "Simpson's rule", "the change of the volume force when the points are halved",
                           std::abs(f.pressureVolume - coarse.value().pressureVolume) * A});
    // The force by three methods.
    Check c;
    c.model = "radiation.force-methods";
    c.title = "Radiation force by two methods";
    c.family = Family::Force;
    c.scope = "relation";
    c.unit = "N";
    const double ref = f.pressureFluxes;
    const double rel = std::max(std::abs(f.pressureSurfaces - ref), std::abs(f.pressureVolume - ref)) / std::max(std::abs(ref), 1e-300);
    c.details = {{"surfaces", f.pressureSurfaces * A}, {"volume", f.pressureVolume * A}, {"fluxes", ref * A}, {"pressureFluxes", ref},
                 {"reflectance", f.reflectance}, {"transmittance", f.transmittance}, {"absorptance", f.absorptance}, {"formulation", f.formulation},
                 {"incidentSideMomentumFlux", f.fluxIncidentSide}, {"exitSideMomentumFlux", f.fluxExitSide}};
    judge(c, rel, tolerance, 100.0 * tolerance);
    c.notes.push_back("vacuum on both sides: the total force is the same for every momentum formulation inside matter");
    l.checks.push_back(c);
    Check a;
    a.model = "absorption.local-global";
    a.title = "Absorbed power: local and global";
    a.family = Family::Power;
    a.scope = "local";
    a.unit = "W";
    const double relA = std::abs(f.absorbedLocal - f.absorbedGlobal) / std::max(std::abs(f.absorbedGlobal), 1e-300);
    a.details = {{"local", f.absorbedLocal * A}, {"global", f.absorbedGlobal * A}, {"points", f.points}};
    if (f.absorbedGlobal <= 1e-12 * I) {
        a.status = CheckStatus::NotApplicable;
        a.notes.push_back("the body absorbs nothing");
    } else {
        judge(a, relA, tolerance, 100.0 * tolerance);
    }
    l.checks.push_back(a);
    l.notes.push_back("R = " + number(f.reflectance) + ", T = " + number(f.transmittance) + ", A = " + number(f.absorptance));
    if (Status v = l.validate(); !v)
        return R::failure(v.message());
    return l;
}

// --- material state ------------------------------------------------------------------------------

Result<Ledger> materialStateLedger(const json& run, double T0)
{
    using R = Result<Ledger>;
    if (!run.is_object() || run.value("format", std::string()) != "resonant.material-state-run")
        return R::failure("not a material-state run");
    const std::string model = run.value("model", std::string());
    const json energy = run.value("energy", json::object());
    const std::string unit = energy.value("unit", std::string("J"));
    Ledger l;
    l.id = "material-state";
    l.title = run.value("title", model);
    l.sourceKind = "material-state run (" + model + ")";
    l.solver = model;
    l.solverVersion = "1";
    l.basis = acc::Basis::Interval;
    l.per = unit == "J/m^3" ? "m^3" : unit == "J/m^2" ? "m^2" : "";
    const auto value = [&](const char* key) { return energy.value(key, 0.0); };
    const auto isothermal = [&](const std::string& body, double dissipated) {
        // An isothermal material point: the dissipated work leaves as heat at T0, carrying D / T0 of entropy, all produced inside.
        acc::Generation g;
        g.id = "entropy production";
        g.entity = body;
        g.family = Family::Entropy;
        g.mechanism = "dissipation at " + number(T0) + " K";
        g.method = "D / T (isothermal)";
        g.amount = dissipated / T0;
        l.generations.push_back(g);
        Exchange s = scalar("entropy to heat", body, "heat", Family::Entropy, "heat", "heat to the surroundings", dissipated / T0);
        s.method = "D / T (isothermal)";
        l.exchanges.push_back(s);
        l.holdings.push_back(scalarHolding("entropy", body, Family::Entropy, "entropy (isothermal: unchanged)", 0.0, 0.0));
        l.notes.push_back("isothermal at " + number(T0) + " K: the entropy produced is the dissipation over the temperature");
    };
    if (model == "mechanics.j2.point" || model == "mechanics.j2.bar" || model == "mechanics.damage.point" || model == "mechanics.damaged-cantilever" ||
        model == "mechanics.viscoelastic.sls") {
        l.entities.push_back(entity("loading", "Loading device", EntityKind::Source, "mechanical"));
        l.entities.push_back(entity("material", model == "mechanics.j2.bar" ? "Bar" : model == "mechanics.damaged-cantilever" ? "Cantilever" : "Material",
                                    EntityKind::Body, "mechanical"));
        l.entities.push_back(entity("heat", "Heat (dissipation)", EntityKind::Sink, "thermal"));
        if (!energy.contains("work"))
            return R::failure("the run reports no work");
        Exchange w = scalar("work", "loading", "material", Family::Energy, "work", "the loading path", value("work"));
        w.method = "the trapezoidal integral of the load over the deformation (the model's own)";
        l.exchanges.push_back(w);
        if (energy.contains("elastic")) {
            Holding e = scalarHolding("elastic", "material", Family::Energy, "elastic", 0.0, value("elastic"), true);
            e.method = "sigma : eps_e / 2";
            l.holdings.push_back(e);
            Holding h = scalarHolding("hardening", "material", Family::Energy, "hardening (stored, not mechanically recoverable)", 0.0, value("hardening"),
                                      false);
            h.method = "H e_p^2 / 2 + 3/4 |alpha|^2 / Hk";
            l.holdings.push_back(h);
        } else {
            Holding e = scalarHolding("stored", "material", Family::Energy, "elastic", 0.0, value("stored"), true);
            e.method = "the model's stored energy";
            l.holdings.push_back(e);
        }
        Exchange d = scalar("dissipated", "material", "heat", Family::Energy, "dissipation",
                            model.find("damage") != std::string::npos ? "damage" : model.find("sls") != std::string::npos ? "viscous flow" : "plastic flow",
                            value("dissipated"));
        d.method = "the model's dissipation";
        l.exchanges.push_back(d);
        if (energy.contains("workQuadratureError"))
            l.numerical.push_back({Family::Energy, "work quadrature", "Richardson's estimate of the trapezoidal work integral", value("workQuadratureError")});
        else
            l.notes.push_back("the work quadrature's error is not estimated for this model");
        isothermal("material", value("dissipated"));
    } else if (model == "thermal.phase-change.slab") {
        l.entities.push_back(entity("wall", "Heated wall", EntityKind::Source, "thermal"));
        l.entities.push_back(entity("slab", "Slab", EntityKind::Body, "thermal"));
        Exchange q = scalar("heat in", "wall", "slab", Family::Energy, "heat", "conduction through the faces", value("heatIn"));
        q.method = "the face fluxes integrated over the steps";
        l.exchanges.push_back(q);
        l.holdings.push_back(scalarHolding("sensible", "slab", Family::Energy, "thermal (sensible)", 0.0, value("sensible")));
        l.holdings.push_back(scalarHolding("latent", "slab", Family::Energy, "latent", 0.0, value("latent")));
        const json s = run.value("entropy", json::object());
        if (s.is_object() && s.contains("change")) {
            Exchange si = scalar("entropy in", "wall", "slab", Family::Entropy, "heat", "q / T at the faces", s.value("inflow", 0.0));
            si.method = "the face fluxes over the face temperatures, integrated over the steps";
            l.exchanges.push_back(si);
            Holding sh = scalarHolding("entropy", "slab", Family::Entropy, "entropy", 0.0, s.value("change", 0.0));
            sh.method = "from the cells' states (rho c ln(T/Tm), rho L f / Tm)";
            l.holdings.push_back(sh);
            acc::Generation g;
            g.id = "entropy production";
            g.entity = "slab";
            g.family = Family::Entropy;
            g.mechanism = "conduction";
            g.method = "q (1/T_cold - 1/T_hot) over the faces and steps";
            g.amount = s.value("production", 0.0);
            l.generations.push_back(g);
            l.notes.push_back("the entropy residual is the explicit time step's error (first order)");
        } else {
            l.missing.push_back({Family::Entropy, "slab", "entropy", "the run does not report it"});
        }
    } else if (model == "em.preisach") {
        l.entities.push_back(entity("source", "Field source", EntityKind::Source, "electromagnetic"));
        l.entities.push_back(entity("core", "Core", EntityKind::Body, "electromagnetic"));
        l.entities.push_back(entity("heat", "Heat (hysteresis loss)", EntityKind::Sink, "thermal"));
        double area = 0.0, relays = 0.0, bound = 0.0;
        json loops = json::array();
        for (const json& loop : energy.value("loops", json::array())) {
            area += loop.value("lossPerCycle", 0.0);
            relays += loop.value("hysteronLoss", 0.0);
            bound += loop.value("errorBound", 0.0);
            loops.push_back(loop);
        }
        Exchange w = scalar("work", "source", "core", Family::Energy, "work", "the closed field loops", area);
        w.method = "mu0 closed integral of H dM (trapezoidal), over the loops";
        l.exchanges.push_back(w);
        Exchange d = scalar("hysteresis", "core", "heat", Family::Energy, "dissipation", "relay switching", relays);
        d.method = "the sum over the switched relays of 2 mu0 Ms w (alpha - beta)";
        l.exchanges.push_back(d);
        l.holdings.push_back(scalarHolding("magnetization", "core", Family::Energy, "magnetic (closed loops: unchanged)", 0.0, 0.0));
        l.numerical.push_back({Family::Energy, "field increments", "2 mu0 Ms dH per loop (each switch is inside one increment)", bound});
        Check c;
        c.model = "hysteresis.loop-loss";
        c.title = "Hysteresis loss against the hysterons";
        c.family = Family::Energy;
        c.scope = "relation";
        c.unit = l.unit(Family::Energy);
        c.details = {{"loops", loops}, {"loopIntegral", area}, {"hysteronSum", relays}, {"errorBound", bound}};
        if (!(relays > 0.0)) {
            c.status = CheckStatus::NotApplicable;
            c.notes.push_back("no relay switches in the loops");
        } else {
            const double diff = std::abs(area - relays);
            c.relative = diff / relays;
            c.numericalError = bound;
            c.status = diff <= bound ? CheckStatus::Verified : CheckStatus::Exceeded;
            c.attribution = diff <= bound ? "within the field-increment bound" : "outside the field-increment bound";
        }
        l.checks.push_back(c);
    } else {
        return R::failure("no accounting for the model '" + model + "'");
    }
    if (Status v = l.validate(); !v)
        return R::failure(v.message());
    return l;
}

}  // namespace resonant::coupling
