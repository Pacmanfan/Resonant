#include "accounting/Conservation.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace resonant::accounting {

using nlohmann::json;

namespace {

Vec3 plus(const Vec3& a, const Vec3& b)
{
    return {a[0] + b[0], a[1] + b[1], a[2] + b[2]};
}
Vec3 minus(const Vec3& a, const Vec3& b)
{
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}
double norm(const Vec3& a)
{
    return std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
}
json vj(const Vec3& v, bool vector)
{
    return vector ? json::array({v[0], v[1], v[2]}) : json(v[0]);
}

std::string number(double v)
{
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.4g", v);
    return buf;
}

std::string modelId(Family f)
{
    switch (f) {
    case Family::Energy:
        return "energy.total";
    case Family::LinearMomentum:
        return "momentum.linear";
    case Family::AngularMomentum:
        return "momentum.angular";
    case Family::Entropy:
        return "entropy.second-law";
    case Family::Mass:
        return "mass";
    case Family::Charge:
        return "charge";
    default:
        return std::string(toString(f));
    }
}

bool isVectorFamily(Family f)
{
    return f == Family::LinearMomentum || f == Family::AngularMomentum;
}

int rank(CheckStatus s)
{
    switch (s) {
    case CheckStatus::Exceeded:
        return 5;
    case CheckStatus::InsufficientData:
        return 4;
    case CheckStatus::Warning:
        return 3;
    case CheckStatus::NotEvaluated:
        return 2;
    case CheckStatus::Verified:
        return 1;
    case CheckStatus::NotApplicable:
        return 0;
    }
    return 0;
}

}  // namespace

json CheckModel::toJson() const
{
    return {{"id", id}, {"title", title}, {"kind", kind}, {"family", std::string(accounting::toString(family))}, {"domain", domain},
            {"sources", sources}, {"sinks", sinks}, {"fluxes", fluxes}, {"equation", equation}, {"requiredData", requiredData},
            {"method", method}, {"assumptions", assumptions}};
}

const std::vector<CheckModel>& checkModels()
{
    static const std::vector<CheckModel> m = {
        {"energy.total", "Energy (first law)", "conservation", Family::Energy, "all",
         {"work and heat supplied from outside", "explicit generation inside"}, {"dissipation to heat sinks"},
         {"radiation, transmission and heat leaving the boundary", "transfers to non-members"},
         "dE_stored = E_in - E_out + E_generated - E_dissipated",
         "every holding of the members at both ends of the interval (rate basis: storage rates) and every exchange crossing the boundary",
         "sums of the sources' own integrals", "the members' stored energies are the models' own (kinetic, elastic, thermal, field)"},
        {"momentum.linear", "Linear momentum", "conservation", Family::LinearMomentum, "all",
         {"external impulses (integrals of external forces)"}, {}, {"momentum fluxes through the boundary (radiation, matter)"},
         "dP = P_in - P_out + I_external", "the members' momenta and every impulse and momentum flux crossing the boundary",
         "per component", "massless elements hold no momentum; a steady field's momentum inside the boundary does not change"},
        {"momentum.angular", "Angular momentum", "conservation", Family::AngularMomentum, "all",
         {"external angular impulses (integrals of external torques)"}, {}, {"angular momentum fluxes"},
         "dL = L_in - L_out + H_external", "the members' angular momenta and every angular impulse crossing the boundary", "per component",
         "about the frame's origin or the declared axis"},
        {"entropy.second-law", "Entropy (second law)", "conservation", Family::Entropy, "thermal",
         {"entropy production inside (never negative)"}, {}, {"entropy carried by heat across the boundary (q / T)"},
         "dS = S_in - S_out + S_produced,  S_produced >= 0",
         "the members' entropies and the temperatures at which heat crosses the boundary", "the model's own entropy state",
         "local equilibrium; evaluated only where a model provides entropy"},
        {"mass", "Mass", "conservation", Family::Mass, "all", {}, {}, {"mass flow through the boundary"}, "dm = m_in - m_out",
         "the members' masses and every mass flow", "sums", "no current model transports mass: not applicable"},
        {"charge", "Electric charge", "conservation", Family::Charge, "electromagnetic", {}, {}, {"current through the boundary"},
         "dQ = integral of the current in", "charge densities and currents", "sums", "no current model reports charge: not applicable"},
        {"resonance.quality-factor", "Q = w E / P by mechanism", "consistency", Family::Energy, "all", {}, {}, {},
         "Q_m = w0 <E_stored> / <P_m>  against the modal Q of the same mechanism",
         "a driven steady state at a mode's frequency, its time-averaged stored energy and the loss per mechanism",
         "the driven response against the complex eigenvalues", "light damping (Q >= 5), one dominant mode"},
        {"resonance.buildup-decay", "Buildup and decay of a resonator", "consistency", Family::Energy, "all", {}, {}, {},
         "E(t) = E_ss (1 - e^(-g t))^2 while driven at resonance; E(t) = E(t_off) e^(-2 g (t - t_off)) after",
         "a transient with the drive removed, and the mode's decay rate g", "a fit of ln E over the decay against 2 g",
         "light damping, one dominant mode, drive at the damped frequency"},
        {"radiation.force-methods", "Radiation force by two methods", "consistency", Family::Force, "electromagnetic", {}, {}, {},
         "F = T_zz(0-) - T_zz(d+) = integral of the Lorentz force density = (1 + R - T) I / c",
         "the solver's fields at the control surfaces and inside the body",
         "vacuum Maxwell stress at the surfaces; Lorentz force on the induced currents by Simpson's rule",
         "normal incidence, vacuum on both sides (no momentum-formulation ambiguity), non-magnetic layers"},
        {"absorption.local-global", "Absorbed power: local and global", "consistency", Family::Power, "electromagnetic", {}, {}, {},
         "integral of (1/2) w eps0 eps'' |E|^2 dz = S(0) - S(d)", "the absorbed power density inside and the Poynting flux at the faces",
         "Simpson's rule against the fluxes", "local conservation of the time-averaged Poynting theorem"},
        {"hysteresis.loop-loss", "Hysteresis loss against the hysterons", "consistency", Family::Energy, "electromagnetic", {}, {}, {},
         "mu0 closed integral of H dM = sum over the relays switched in the loop of 2 mu0 w (alpha - beta)",
         "the loop's path and the relays' thresholds and weights", "the trapezoidal loop integral against the relay sum",
         "a closed loop; the error falls with the field increment"},
    };
    return m;
}

const CheckModel* findCheckModel(std::string_view id)
{
    for (const CheckModel& m : checkModels())
        if (m.id == id)
            return &m;
    return nullptr;
}

json Tolerances::toJson() const
{
    return {{"relative", relative}, {"warning", warning}, {"absolute", absolute}};
}

Tolerances Tolerances::fromJson(const json& j)
{
    Tolerances t;
    if (j.is_object()) {
        t.relative = j.value("relative", t.relative);
        t.warning = std::max(t.relative, j.value("warning", t.warning));
        t.absolute = j.value("absolute", t.absolute);
    }
    return t;
}

CheckStatus worst(const std::vector<Check>& checks)
{
    CheckStatus w = CheckStatus::NotApplicable;
    for (const Check& c : checks)
        if (rank(c.status) > rank(w))
            w = c.status;
    return w;
}

Check balance(const Ledger& l, const AccountingSystem& s, Family family, const Tolerances& tol)
{
    Check c;
    c.model = modelId(family);
    const CheckModel* m = findCheckModel(c.model);
    c.title = m ? m->title : std::string(info(family).name);
    c.system = s.id;
    c.family = family;
    c.unit = l.unit(family);
    c.isVector = isVectorFamily(family);
    c.tolerance = tol.relative;
    c.warning = tol.warning;
    double scale = 0.0;
    bool data = false;
    // Holdings of the members.
    std::vector<const Holding*> held;
    for (const Holding& h : l.holdings)
        if (h.family == family && s.contains(h.entity)) {
            held.push_back(&h);
            data = true;
            const Vec3 i = h.isVector ? h.initialVector : Vec3{h.initial, 0, 0};
            const Vec3 f = h.isVector ? h.finalVector : Vec3{h.final, 0, 0};
            c.initial = plus(c.initial, i);
            c.final = plus(c.final, f);
            c.delta = plus(c.delta, h.change());
            scale = std::max({scale, norm(i), norm(f)});
        }
    // Exchanges, classified for this system.
    double mapping = 0.0;
    std::vector<std::pair<const Exchange*, Role>> roles;
    for (const Exchange& x : l.exchanges) {
        if (x.family != family)
            continue;
        const Role r = classify(x, l, s);
        if (r == Role::Outside)
            continue;
        roles.push_back({&x, r});
        data = true;
        switch (r) {
        case Role::Internal:
            c.internal = plus(c.internal, x.value());
            if (x.sourceAmount)
                mapping += std::abs(x.amount - *x.sourceAmount);
            break;
        case Role::Input:
            if (x.category == "impulse")
                c.impulse = plus(c.impulse, x.value());
            else
                c.input = plus(c.input, x.value());
            break;
        case Role::Output:
            c.output = plus(c.output, x.sourceValue());
            break;
        case Role::Dissipated:
            c.dissipated = plus(c.dissipated, x.sourceValue());
            break;
        case Role::Outside:
            break;
        }
        scale = std::max(scale, std::max(norm(x.value()), norm(x.sourceValue())));
    }
    for (const Generation& g : l.generations)
        if (g.family == family && s.contains(g.entity)) {
            c.generated[0] += g.amount;
            data = true;
            scale = std::max(scale, std::abs(g.amount));
        }
    c.residual = minus(c.delta, minus(plus(plus(c.input, c.impulse), c.generated), plus(c.output, c.dissipated)));
    scale = std::max({scale, norm(c.input), norm(c.impulse), norm(c.output), norm(c.dissipated), norm(c.delta)});
    c.scale = scale;
    // The numerical errors the sources state.
    double numerical = mapping;
    for (const NumericalEstimate& n : l.numerical)
        if (n.family == family)
            numerical += std::abs(n.amount);
    c.numericalError = numerical;
    if (mapping > 0.0)
        c.notes.push_back("transfer mapping (target minus source integrals of internal transfers): " + number(mapping) + " " + c.unit);
    // Declared missing contributions.
    for (const Missing& mi : l.missing)
        if (mi.family == family && (mi.entity.empty() || s.contains(mi.entity)))
            c.missing.push_back((mi.entity.empty() ? std::string() : mi.entity + ": ") + mi.what + (mi.why.empty() ? "" : " (" + mi.why + ")"));
    // Double counting: the same mechanism twice between the same entities, or a destination given a share the source already includes.
    bool doubled = false;
    for (size_t a = 0; a < roles.size(); ++a)
        for (size_t b = a + 1; b < roles.size(); ++b) {
            const Exchange& x = *roles[a].first;
            const Exchange& y = *roles[b].first;
            const bool same = x.from == y.from && x.to == y.to && x.mechanism == y.mechanism && x.category == y.category && !x.mechanism.empty();
            const bool included = (x.includesDestination && y.to == x.to && y.mechanism == x.mechanism) ||
                                  (y.includesDestination && x.to == y.to && x.mechanism == y.mechanism);
            if (same || included) {
                doubled = true;
                c.notes.push_back("possible double counting: '" + x.id + "' and '" + y.id + "' carry " + x.mechanism + " into " + x.to);
            }
        }
    // Effective and explicit representations both holding the quantity.
    std::string replaced;
    for (const std::string& a : s.members) {
        const Entity* ea = l.entity(a);
        if (!ea || ea->effectiveOf.empty())
            continue;
        for (const std::string& b : s.members) {
            const Entity* eb = l.entity(b);
            if (!eb || eb->representation != ea->effectiveOf)
                continue;
            const auto holds = [&](const std::string& id) {
                for (const Holding* h : held)
                    if (h->entity == id)
                        return true;
                return false;
            };
            if (holds(a) && holds(b))
                replaced = "'" + a + "' is an effective description of '" + b + "' (" + ea->effectiveOf + "); both hold " + info(family).name;
        }
    }
    const double r = c.residualNorm();
    if (scale > tol.absolute)
        c.relative = r / scale;
    if (!replaced.empty()) {
        c.status = CheckStatus::NotEvaluated;
        c.notes.push_back("double counting across scales: " + replaced + "; include one of them");
    } else if (!data) {
        c.status = c.missing.empty() ? (family == Family::Energy ? CheckStatus::NotEvaluated : CheckStatus::NotApplicable) : CheckStatus::InsufficientData;
        c.notes.push_back(c.missing.empty() ? "no " + info(family).name + " is reported for these members" : "the contributions are missing");
    } else if (!c.missing.empty()) {
        c.status = CheckStatus::InsufficientData;
        c.notes.push_back("incomplete: the residual leaves out the missing contributions and proves nothing");
    } else {
        const double rel = c.relative.value_or(r <= tol.absolute ? 0.0 : INFINITY);
        c.status = rel <= tol.relative ? CheckStatus::Verified : rel <= tol.warning ? CheckStatus::Warning : CheckStatus::Exceeded;
        if (doubled && c.status == CheckStatus::Verified)
            c.status = CheckStatus::Warning;
    }
    if (family == Family::Entropy && data) {
        const double produced = c.generated[0];
        if (produced < -(tol.relative * scale + tol.absolute)) {
            c.status = CheckStatus::Exceeded;
            c.notes.push_back("negative entropy production (" + number(produced) + " " + c.unit + "): the second law is violated");
        } else {
            c.notes.push_back("entropy production " + number(produced) + " " + c.unit + " (non-negative)");
        }
    }
    // Attribution: numerical or unaccounted; never dissipation.
    if (data && replaced.empty()) {
        if (r <= numerical + tol.relative * scale + tol.absolute)
            c.attribution = numerical > 0.0 ? "within the stated numerical errors (" + number(numerical) + " " + c.unit + ")" : "at rounding level";
        else
            c.attribution = "unaccounted: " + number(r) + " " + c.unit + " exceeds the stated numerical errors (" + number(numerical) + " " + c.unit +
                            ") - a missing contribution, a model error or an unstated numerical error";
    }
    // History of the residual, when every contribution has one.
    if (!l.times.empty() && data) {
        bool complete = true;
        for (const Holding* h : held)
            complete = complete && (h->isVector ? h->vectorHistory.size() == l.times.size() : h->history.size() == l.times.size());
        for (const auto& [x, role] : roles)
            complete = complete && (x->isVector ? x->vectorHistory.size() == l.times.size() : x->history.size() == l.times.size());
        for (const Generation& g : l.generations)
            if (g.family == family && s.contains(g.entity))
                complete = complete && g.history.size() == l.times.size();
        if (complete) {
            for (size_t k = 0; k < l.times.size(); ++k) {
                Vec3 d{}, net{};
                for (const Holding* h : held) {
                    const Vec3 now = h->isVector ? h->vectorHistory[k] : Vec3{h->history[k], 0, 0};
                    const Vec3 first = h->isVector ? h->vectorHistory[0] : Vec3{h->history[0], 0, 0};
                    d = plus(d, minus(now, first));
                }
                for (const auto& [x, role] : roles) {
                    const Vec3 v = x->isVector ? x->vectorHistory[k] : Vec3{x->history[k], 0, 0};
                    if (role == Role::Input)
                        net = plus(net, v);
                    else if (role == Role::Output || role == Role::Dissipated)
                        net = minus(net, v);
                }
                for (const Generation& g : l.generations)
                    if (g.family == family && s.contains(g.entity))
                        net[0] += g.history[k];
                c.times.push_back(l.times[k]);
                c.residualHistory.push_back(norm(minus(d, net)));
            }
        } else {
            c.notes.push_back("no history of the residual: some contributions have no history");
        }
    }
    return c;
}

json Report::toJson() const
{
    json j{{"system", system.toJson()}, {"basis", std::string(toString(basis))}, {"breakdown", breakdown}, {"flows", flows},
           {"forces", forces}, {"series", series}, {"classified", classified}};
    j["checks"] = json::array();
    for (const Check& c : checks)
        j["checks"].push_back(c.toJson());
    j["status"] = std::string(toString(worst(checks)));
    return j;
}

Report evaluate(const Ledger& l, const AccountingSystem& s, const Tolerances& tol)
{
    Report rep;
    rep.system = s;
    rep.basis = l.basis;
    // Balances: energy, both momenta and entropy always (so that "not applicable" is said), mass and charge when reported.
    std::set<Family> families{Family::Energy, Family::LinearMomentum, Family::AngularMomentum, Family::Entropy};
    for (const Exchange& x : l.exchanges)
        families.insert(x.family);
    for (const Holding& h : l.holdings)
        families.insert(h.family);
    for (const Missing& m : l.missing)
        families.insert(m.family);
    for (Family f : {Family::Energy, Family::LinearMomentum, Family::AngularMomentum, Family::Entropy, Family::Mass, Family::Charge})
        if (families.count(f))
            rep.checks.push_back(balance(l, s, f, tol));
    for (const Check& c : l.checks) {
        Check copy = c;
        copy.system = s.id;
        rep.checks.push_back(copy);
    }
    // Classified exchanges and the breakdown per family.
    std::map<Family, json> breakdown;
    for (const Exchange& x : l.exchanges) {
        const Role r = classify(x, l, s);
        json e{{"id", x.id}, {"from", x.from}, {"to", x.to}, {"family", std::string(toString(x.family))}, {"role", std::string(toString(r))},
               {"category", x.category}, {"mechanism", x.mechanism}, {"unit", l.unit(x.family)}};
        e["amount"] = vj(r == Role::Output || r == Role::Dissipated ? x.sourceValue() : x.value(), x.isVector);
        rep.classified.push_back(e);
        if (r != Role::Outside) {
            json& b = breakdown[x.family];
            if (b.is_null())
                b = {{"family", std::string(toString(x.family))}, {"unit", l.unit(x.family)}, {"contributions", json::array()}, {"holdings", json::array()}};
            b["contributions"].push_back(e);
        }
    }
    for (const Holding& h : l.holdings)
        if (s.contains(h.entity)) {
            json& b = breakdown[h.family];
            if (b.is_null())
                b = {{"family", std::string(toString(h.family))}, {"unit", l.unit(h.family)}, {"contributions", json::array()}, {"holdings", json::array()}};
            b["holdings"].push_back({{"id", h.id}, {"entity", h.entity}, {"form", h.form}, {"recoverable", h.recoverable},
                                     {"initial", vj(h.isVector ? h.initialVector : Vec3{h.initial, 0, 0}, h.isVector)},
                                     {"final", vj(h.isVector ? h.finalVector : Vec3{h.final, 0, 0}, h.isVector)},
                                     {"change", vj(h.change(), h.isVector)}});
        }
    for (const Check& c : rep.checks) {
        if (!breakdown.count(c.family) || c.model != modelId(c.family))
            continue;
        json& b = breakdown[c.family];
        for (const auto& [k, v] : std::vector<std::pair<const char*, Vec3>>{{"input", c.input}, {"impulse", c.impulse}, {"output", c.output},
                                                                            {"dissipated", c.dissipated}, {"internal", c.internal},
                                                                            {"generated", c.generated}, {"stored", c.delta}, {"residual", c.residual}})
            b[k] = vj(v, c.isVector);
        b["status"] = std::string(toString(c.status));
    }
    for (auto& [f, b] : breakdown)
        rep.breakdown.push_back(b);
    // Flow graph: the members, and every outside entity an exchange of theirs touches.
    json nodes = json::array(), edges = json::array();
    std::set<std::string> touched(s.members.begin(), s.members.end());
    std::map<std::tuple<std::string, std::string, std::string, std::string>, std::pair<Vec3, int>> agg;  // from, to, family, role
    std::map<std::tuple<std::string, std::string, std::string, std::string>, bool> vectorEdge;
    for (const Exchange& x : l.exchanges) {
        const Role r = classify(x, l, s);
        if (r == Role::Outside)
            continue;
        touched.insert(x.from);
        touched.insert(x.to);
        auto key = std::make_tuple(x.from, x.to, std::string(toString(x.family)), std::string(toString(r)));
        auto& [sum, count] = agg[key];
        sum = plus(sum, r == Role::Output || r == Role::Dissipated ? x.sourceValue() : x.value());
        ++count;
        vectorEdge[key] = x.isVector;
    }
    for (const std::string& id : touched) {
        const Entity* e = l.entity(id);
        if (!e)
            continue;
        double stored = 0.0;
        for (const Holding& h : l.holdings)
            if (h.entity == id && h.family == Family::Energy)
                stored += h.change()[0];
        nodes.push_back({{"id", e->id}, {"name", e->name}, {"kind", std::string(toString(e->kind))}, {"domain", e->domain},
                         {"member", s.contains(e->id)}, {"storedChange", stored}});
    }
    for (const auto& [key, value] : agg) {
        const auto& [from, to, family, role] = key;
        edges.push_back({{"from", from}, {"to", to}, {"family", family}, {"role", role}, {"amount", vj(value.first, vectorEdge[key])},
                         {"magnitude", norm(value.first)}, {"exchanges", value.second}});
    }
    rep.flows = {{"nodes", nodes}, {"edges", edges}};
    // Forces: per target the resultant and contributions; for the system the external resultant and the internal pairs.
    json targets = json::object();
    Vec3 externalForce{}, externalTorque{};
    json internalPairs = json::array();
    for (const ForceRecord& f : l.forces) {
        json& t = targets[f.target];
        if (t.is_null())
            t = {{"force", json::array({0.0, 0.0, 0.0})}, {"torque", json::array({0.0, 0.0, 0.0})}, {"contributions", json::array()}};
        const char* key = f.family == Family::Torque ? "torque" : "force";
        for (size_t i = 0; i < 3; ++i)
            t[key][i] = t[key][i].get<double>() + f.vector[i];
        t["contributions"].push_back({{"id", f.id}, {"category", f.category}, {"source", f.source}, {"family", std::string(toString(f.family))},
                                      {"vector", json::array({f.vector[0], f.vector[1], f.vector[2]})}, {"mechanism", f.mechanism},
                                      {"method", f.method}, {"pair", f.pair}, {"time", f.time}});
        const bool onMember = s.contains(f.target);
        const bool fromMember = s.contains(f.source);
        if (onMember && !fromMember) {
            Vec3& acc = f.family == Family::Torque ? externalTorque : externalForce;
            acc = plus(acc, f.vector);
        } else if (onMember && fromMember) {
            internalPairs.push_back({{"id", f.id}, {"pair", f.pair}});
        }
    }
    rep.forces = {{"targets", targets},
                  {"externalForce", json::array({externalForce[0], externalForce[1], externalForce[2]})},
                  {"externalTorque", json::array({externalTorque[0], externalTorque[1], externalTorque[2]})},
                  {"internal", internalPairs},
                  {"note", "force is not conserved; internal pairs act between members and cancel in the system's resultant"}};
    // Histories of the scalar balances.
    if (!l.times.empty())
        for (Family f : {Family::Energy, Family::Entropy}) {
            json series{{"family", std::string(toString(f))}, {"unit", l.unit(f)}, {"times", l.times}};
            std::vector<double> in(l.times.size(), 0.0), out(l.times.size(), 0.0), diss(l.times.size(), 0.0), stored(l.times.size(), 0.0),
                internal(l.times.size(), 0.0);
            bool any = false, complete = true;
            for (const Exchange& x : l.exchanges) {
                if (x.family != f || x.isVector)
                    continue;
                const Role r = classify(x, l, s);
                if (r == Role::Outside)
                    continue;
                any = true;
                if (x.history.size() != l.times.size()) {
                    complete = false;
                    continue;
                }
                std::vector<double>& target = r == Role::Input ? in : r == Role::Output ? out : r == Role::Dissipated ? diss : internal;
                for (size_t k = 0; k < l.times.size(); ++k)
                    target[k] += x.history[k];
            }
            json byHolding = json::array();
            for (const Holding& h : l.holdings) {
                if (h.family != f || !s.contains(h.entity) || h.isVector)
                    continue;
                any = true;
                if (h.history.size() != l.times.size()) {
                    complete = false;
                    continue;
                }
                for (size_t k = 0; k < l.times.size(); ++k)
                    stored[k] += h.history[k];
                byHolding.push_back({{"id", h.id}, {"entity", h.entity}, {"form", h.form}, {"values", h.history}});
            }
            if (!any)
                continue;
            series["input"] = in;
            series["output"] = out;
            series["dissipated"] = diss;
            series["internal"] = internal;
            series["stored"] = stored;
            series["holdings"] = byHolding;
            series["complete"] = complete;
            std::vector<double> residual(l.times.size(), 0.0);
            for (size_t k = 0; k < l.times.size(); ++k)
                residual[k] = (stored[k] - stored[0]) - (in[k] - out[k] - diss[k]);
            series["residual"] = residual;
            rep.series.push_back(series);
        }
    return rep;
}

}  // namespace resonant::accounting
