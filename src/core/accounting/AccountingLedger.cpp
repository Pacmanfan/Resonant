#include "accounting/AccountingLedger.h"

#include <cmath>
#include <set>

namespace resonant::accounting {

using nlohmann::json;

std::string_view toString(Basis b)
{
    return b == Basis::Rate ? "rate" : "interval";
}

namespace {

const std::vector<std::pair<EntityKind, std::string_view>>& entityKinds()
{
    static const std::vector<std::pair<EntityKind, std::string_view>> k = {
        {EntityKind::Body, "body"},       {EntityKind::Region, "region"}, {EntityKind::Field, "field"},
        {EntityKind::Participant, "participant"}, {EntityKind::Element, "element"}, {EntityKind::Source, "source"},
        {EntityKind::Sink, "sink"},       {EntityKind::Environment, "environment"}, {EntityKind::Representation, "representation"}};
    return k;
}

const std::vector<std::pair<CheckStatus, std::string_view>>& statuses()
{
    static const std::vector<std::pair<CheckStatus, std::string_view>> s = {
        {CheckStatus::Verified, "verified"},          {CheckStatus::Warning, "warning"},
        {CheckStatus::Exceeded, "exceeded"},          {CheckStatus::NotEvaluated, "not evaluated"},
        {CheckStatus::InsufficientData, "insufficient data"}, {CheckStatus::NotApplicable, "not applicable"}};
    return s;
}

json vec(const Vec3& v)
{
    return json::array({v[0], v[1], v[2]});
}

Vec3 vecOf(const json& j)
{
    Vec3 v{};
    if (j.is_array())
        for (size_t i = 0; i < 3 && i < j.size(); ++i)
            v[i] = j[i].get<double>();
    return v;
}

std::vector<Vec3> vecs(const json& j)
{
    std::vector<Vec3> out;
    if (j.is_array())
        for (const json& v : j)
            out.push_back(vecOf(v));
    return out;
}

json vecsJson(const std::vector<Vec3>& v)
{
    json out = json::array();
    for (const Vec3& x : v)
        out.push_back(vec(x));
    return out;
}

Result<Family> familyOf(const json& j, const char* key = "family")
{
    auto f = familyFromString(j.value(key, std::string()));
    if (!f)
        return Result<Family>::failure("unknown family '" + j.value(key, std::string()) + "'");
    return *f;
}

}  // namespace

std::string_view toString(EntityKind k)
{
    for (const auto& [kind, name] : entityKinds())
        if (kind == k)
            return name;
    return "body";
}

std::optional<EntityKind> entityKindFromString(std::string_view s)
{
    for (const auto& [kind, name] : entityKinds())
        if (name == s)
            return kind;
    return std::nullopt;
}

std::string_view toString(CheckStatus s)
{
    for (const auto& [status, name] : statuses())
        if (status == s)
            return name;
    return "not evaluated";
}

std::optional<CheckStatus> checkStatusFromString(std::string_view s)
{
    for (const auto& [status, name] : statuses())
        if (name == s)
            return status;
    return std::nullopt;
}

double Check::residualNorm() const
{
    return std::sqrt(residual[0] * residual[0] + residual[1] * residual[1] + residual[2] * residual[2]);
}

json Check::toJson() const
{
    const auto v = [this](const Vec3& x) -> json { return isVector ? vec(x) : json(x[0]); };
    json j{{"model", model},        {"title", title},     {"system", system},        {"family", std::string(accounting::toString(family))},
           {"scope", scope},        {"status", std::string(accounting::toString(status))}, {"unit", unit},
           {"initial", v(initial)}, {"final", v(final)},  {"input", v(input)},       {"output", v(output)},
           {"impulse", v(impulse)}, {"internal", v(internal)}, {"generated", v(generated)}, {"dissipated", v(dissipated)},
           {"delta", v(delta)},     {"residual", v(residual)}, {"residualNorm", residualNorm()}, {"scale", scale},
           {"tolerance", tolerance}, {"warning", warning}, {"numericalError", numericalError}, {"attribution", attribution},
           {"measurementUncertainty", measurementUncertainty},
           {"missing", missing},    {"notes", notes},     {"details", details},      {"vector", isVector}};
    j["relative"] = relative ? json(*relative) : json(nullptr);
    if (!times.empty()) {
        j["times"] = times;
        j["residualHistory"] = residualHistory;
    }
    return j;
}

Result<Check> Check::fromJson(const json& j)
{
    using R = Result<Check>;
    if (!j.is_object())
        return R::failure("a check is an object");
    Check c;
    c.model = j.value("model", std::string());
    c.title = j.value("title", std::string());
    c.system = j.value("system", std::string());
    auto f = familyOf(j);
    if (!f)
        return R::failure(f.message());
    c.family = f.value();
    c.scope = j.value("scope", std::string("global"));
    auto s = checkStatusFromString(j.value("status", std::string()));
    if (!s)
        return R::failure("unknown check status '" + j.value("status", std::string()) + "'");
    c.status = *s;
    c.isVector = j.value("vector", false);
    const auto v = [&](const char* key) {
        const json& x = j.contains(key) ? j[key] : json();
        return x.is_array() ? vecOf(x) : Vec3{x.is_number() ? x.get<double>() : 0.0, 0.0, 0.0};
    };
    c.initial = v("initial");
    c.final = v("final");
    c.input = v("input");
    c.output = v("output");
    c.impulse = v("impulse");
    c.internal = v("internal");
    c.generated = v("generated");
    c.dissipated = v("dissipated");
    c.delta = v("delta");
    c.residual = v("residual");
    if (j.contains("relative") && j["relative"].is_number())
        c.relative = j["relative"].get<double>();
    c.scale = j.value("scale", 0.0);
    c.tolerance = j.value("tolerance", 0.0);
    c.warning = j.value("warning", 0.0);
    c.numericalError = j.value("numericalError", 0.0);
    c.measurementUncertainty = j.value("measurementUncertainty", 0.0);
    c.unit = j.value("unit", std::string());
    c.attribution = j.value("attribution", std::string());
    c.missing = j.value("missing", std::vector<std::string>{});
    c.notes = j.value("notes", std::vector<std::string>{});
    c.details = j.value("details", json::object());
    c.times = j.value("times", std::vector<double>{});
    c.residualHistory = j.value("residualHistory", std::vector<double>{});
    return c;
}

std::string Ledger::unit(Family f) const
{
    const std::string u = basis == Basis::Rate ? rateUnit(f) : info(f).unit;
    return per.empty() ? u : "(" + u + ")/" + per;
}

const Entity* Ledger::entity(const std::string& entityId) const
{
    for (const Entity& e : entities)
        if (e.id == entityId)
            return &e;
    return nullptr;
}

Status Ledger::validate() const
{
    std::set<std::string> ids;
    for (const Entity& e : entities) {
        if (e.id.empty())
            return Status::failure("an entity has no id");
        if (!ids.insert(e.id).second)
            return Status::failure("the entity '" + e.id + "' is defined twice");
    }
    for (const Exchange& x : exchanges) {
        if (!entity(x.from) || !entity(x.to))
            return Status::failure("the exchange '" + x.id + "' names an unknown entity (" + x.from + " -> " + x.to + ")");
        if (x.from == x.to)
            return Status::failure("the exchange '" + x.id + "' goes from an entity to itself");
        if (!info(x.family).conserved && x.family != Family::Entropy)
            return Status::failure("the exchange '" + x.id + "' moves " + info(x.family).name + ", which is not a conserved quantity");
        if (!times.empty() && !x.history.empty() && x.history.size() != times.size())
            return Status::failure("the exchange '" + x.id + "' has a history of a different length than the ledger's times");
        if (!times.empty() && !x.vectorHistory.empty() && x.vectorHistory.size() != times.size())
            return Status::failure("the exchange '" + x.id + "' has a history of a different length than the ledger's times");
    }
    for (const Holding& h : holdings) {
        const Entity* e = entity(h.entity);
        if (!e)
            return Status::failure("the holding '" + h.id + "' names an unknown entity '" + h.entity + "'");
        if (e->external())
            return Status::failure("the holding '" + h.id + "' is held by a source, sink or the environment, which hold nothing");
        if (!times.empty() && !h.history.empty() && h.history.size() != times.size())
            return Status::failure("the holding '" + h.id + "' has a history of a different length than the ledger's times");
    }
    for (const Generation& g : generations)
        if (!entity(g.entity))
            return Status::failure("the generation '" + g.id + "' names an unknown entity");
    for (const ForceRecord& f : forces)
        if (f.family != Family::Force && f.family != Family::Torque)
            return Status::failure("the force record '" + f.id + "' is neither a force nor a torque");
    if (basis == Basis::Interval && !(t1 >= t0))
        return Status::failure("the interval ends before it starts");
    return Status::success();
}

json Ledger::toJson() const
{
    json j{{"format", "resonant.accounting-ledger"}, {"version", 1}, {"id", id}, {"title", title}, {"sourceKind", sourceKind},
           {"solver", solver}, {"solverVersion", solverVersion}, {"basis", std::string(toString(basis))}, {"t0", t0}, {"t1", t1},
           {"per", per}, {"measured", measured}, {"times", times}, {"notes", notes}};
    j["entities"] = json::array();
    for (const Entity& e : entities) {
        json x{{"id", e.id}, {"name", e.name}, {"kind", std::string(toString(e.kind))}, {"domain", e.domain}};
        for (const auto& [k, v] : {std::pair{"object", &e.object}, {"material", &e.material}, {"representation", &e.representation},
                                   {"effectiveOf", &e.effectiveOf}, {"note", &e.note}})
            if (!v->empty())
                x[k] = *v;
        j["entities"].push_back(x);
    }
    j["exchanges"] = json::array();
    for (const Exchange& e : exchanges) {
        json x{{"id", e.id}, {"from", e.from}, {"to", e.to}, {"family", std::string(accounting::toString(e.family))},
               {"category", e.category}, {"mechanism", e.mechanism}, {"fromDomain", e.fromDomain}, {"toDomain", e.toDomain},
               {"region", e.region}, {"method", e.method}, {"provenance", e.provenance}, {"includesDestination", e.includesDestination},
               {"unit", unit(e.family)}};
        if (e.isVector) {
            x["vector"] = vec(e.vector);
            if (!e.vectorHistory.empty())
                x["vectorHistory"] = vecsJson(e.vectorHistory);
        } else {
            x["amount"] = e.amount;
            if (!e.history.empty())
                x["history"] = e.history;
        }
        if (e.sourceAmount)
            x["sourceAmount"] = *e.sourceAmount;
        if (e.uncertainty)
            x["uncertainty"] = *e.uncertainty;
        if (!e.note.empty())
            x["note"] = e.note;
        j["exchanges"].push_back(x);
    }
    j["holdings"] = json::array();
    for (const Holding& h : holdings) {
        json x{{"id", h.id}, {"entity", h.entity}, {"family", std::string(accounting::toString(h.family))}, {"form", h.form},
               {"recoverable", h.recoverable}, {"method", h.method}, {"provenance", h.provenance}, {"unit", unit(h.family)}};
        if (h.isVector) {
            x["initial"] = vec(h.initialVector);
            x["final"] = vec(h.finalVector);
            if (!h.vectorHistory.empty())
                x["vectorHistory"] = vecsJson(h.vectorHistory);
        } else {
            x["initial"] = h.initial;
            x["final"] = h.final;
            if (!h.history.empty())
                x["history"] = h.history;
        }
        if (!h.note.empty())
            x["note"] = h.note;
        if (h.uncertainty)
            x["uncertainty"] = *h.uncertainty;
        j["holdings"].push_back(x);
    }
    j["generations"] = json::array();
    for (const Generation& g : generations)
        j["generations"].push_back({{"id", g.id}, {"entity", g.entity}, {"family", std::string(accounting::toString(g.family))},
                                    {"mechanism", g.mechanism}, {"method", g.method}, {"amount", g.amount}, {"history", g.history}});
    j["forces"] = json::array();
    for (const ForceRecord& f : forces) {
        json x{{"id", f.id}, {"family", std::string(accounting::toString(f.family))}, {"category", f.category}, {"model", f.model},
               {"source", f.source}, {"target", f.target}, {"mechanism", f.mechanism}, {"method", f.method}, {"vector", vec(f.vector)},
               {"application", vec(f.application)}, {"frame", f.frame}, {"time", f.time}, {"distribution", f.distribution},
               {"pair", f.pair}, {"unit", info(f.family).unit}};
        if (f.uncertainty)
            x["uncertainty"] = *f.uncertainty;
        if (!f.history.empty())
            x["history"] = vecsJson(f.history);
        j["forces"].push_back(x);
    }
    j["missing"] = json::array();
    for (const Missing& m : missing)
        j["missing"].push_back({{"family", std::string(accounting::toString(m.family))}, {"entity", m.entity}, {"what", m.what}, {"why", m.why}});
    j["numerical"] = json::array();
    for (const NumericalEstimate& n : numerical)
        j["numerical"].push_back({{"family", std::string(accounting::toString(n.family))}, {"source", n.source}, {"method", n.method}, {"amount", n.amount}});
    j["contracts"] = json::array();
    for (const ScaleContract& c : contracts) {
        json x{{"id", c.id}, {"sourceRepresentation", c.sourceRepresentation}, {"targetEntity", c.targetEntity}, {"quantity", c.quantity},
               {"method", c.method}, {"averaging", c.averaging}, {"resolution", c.resolution}, {"conservation", c.conservation},
               {"sourceRepresented", c.sourceRepresented}};
        if (c.uncertainty)
            x["uncertainty"] = *c.uncertainty;
        j["contracts"].push_back(x);
    }
    j["quantities"] = json::array();
    for (const Quantity& q : quantities)
        j["quantities"].push_back(q.toJson());
    j["checks"] = json::array();
    for (const Check& c : checks)
        j["checks"].push_back(c.toJson());
    return j;
}

Result<Ledger> Ledger::fromJson(const json& j)
{
    using R = Result<Ledger>;
    if (!j.is_object() || j.value("format", std::string()) != "resonant.accounting-ledger")
        return R::failure("not an accounting ledger (format resonant.accounting-ledger)");
    Ledger l;
    l.id = j.value("id", std::string());
    l.title = j.value("title", std::string());
    l.sourceKind = j.value("sourceKind", std::string());
    l.solver = j.value("solver", std::string());
    l.solverVersion = j.value("solverVersion", std::string());
    l.basis = j.value("basis", std::string("interval")) == "rate" ? Basis::Rate : Basis::Interval;
    l.t0 = j.value("t0", 0.0);
    l.t1 = j.value("t1", 0.0);
    l.per = j.value("per", std::string());
    l.measured = j.value("measured", false);
    l.times = j.value("times", std::vector<double>{});
    l.notes = j.value("notes", std::vector<std::string>{});
    for (const json& x : j.value("entities", json::array())) {
        Entity e;
        e.id = x.value("id", std::string());
        e.name = x.value("name", e.id);
        auto k = entityKindFromString(x.value("kind", std::string("body")));
        if (!k)
            return R::failure("unknown entity kind '" + x.value("kind", std::string()) + "'");
        e.kind = *k;
        e.domain = x.value("domain", std::string());
        e.object = x.value("object", std::string());
        e.material = x.value("material", std::string());
        e.representation = x.value("representation", std::string());
        e.effectiveOf = x.value("effectiveOf", std::string());
        e.note = x.value("note", std::string());
        l.entities.push_back(e);
    }
    for (const json& x : j.value("exchanges", json::array())) {
        Exchange e;
        e.id = x.value("id", std::string());
        e.from = x.value("from", std::string());
        e.to = x.value("to", std::string());
        auto f = familyOf(x);
        if (!f)
            return R::failure(f.message());
        e.family = f.value();
        e.category = x.value("category", std::string());
        e.mechanism = x.value("mechanism", std::string());
        e.fromDomain = x.value("fromDomain", std::string());
        e.toDomain = x.value("toDomain", std::string());
        e.region = x.value("region", std::string());
        e.method = x.value("method", std::string());
        e.provenance = x.value("provenance", std::string("computed"));
        e.includesDestination = x.value("includesDestination", false);
        if (x.contains("vector")) {
            e.isVector = true;
            e.vector = vecOf(x["vector"]);
            e.vectorHistory = vecs(x.value("vectorHistory", json::array()));
        } else {
            e.amount = x.value("amount", 0.0);
            e.history = x.value("history", std::vector<double>{});
        }
        if (x.contains("sourceAmount"))
            e.sourceAmount = x["sourceAmount"].get<double>();
        if (x.contains("uncertainty"))
            e.uncertainty = x["uncertainty"].get<double>();
        e.note = x.value("note", std::string());
        l.exchanges.push_back(e);
    }
    for (const json& x : j.value("holdings", json::array())) {
        Holding h;
        h.id = x.value("id", std::string());
        h.entity = x.value("entity", std::string());
        auto f = familyOf(x);
        if (!f)
            return R::failure(f.message());
        h.family = f.value();
        h.form = x.value("form", std::string());
        h.recoverable = x.value("recoverable", true);
        h.method = x.value("method", std::string());
        h.provenance = x.value("provenance", std::string("computed"));
        h.note = x.value("note", std::string());
        if (x.contains("uncertainty"))
            h.uncertainty = x["uncertainty"].get<double>();
        if (x.contains("initial") && x["initial"].is_array()) {
            h.isVector = true;
            h.initialVector = vecOf(x["initial"]);
            h.finalVector = vecOf(x["final"]);
            h.vectorHistory = vecs(x.value("vectorHistory", json::array()));
        } else {
            h.initial = x.value("initial", 0.0);
            h.final = x.value("final", 0.0);
            h.history = x.value("history", std::vector<double>{});
        }
        l.holdings.push_back(h);
    }
    for (const json& x : j.value("generations", json::array())) {
        Generation g;
        g.id = x.value("id", std::string());
        g.entity = x.value("entity", std::string());
        auto f = familyOf(x);
        if (!f)
            return R::failure(f.message());
        g.family = f.value();
        g.mechanism = x.value("mechanism", std::string());
        g.method = x.value("method", std::string());
        g.amount = x.value("amount", 0.0);
        g.history = x.value("history", std::vector<double>{});
        l.generations.push_back(g);
    }
    for (const json& x : j.value("forces", json::array())) {
        ForceRecord f;
        f.id = x.value("id", std::string());
        auto fam = familyOf(x);
        if (!fam)
            return R::failure(fam.message());
        f.family = fam.value();
        f.category = x.value("category", std::string());
        f.model = x.value("model", std::string());
        f.source = x.value("source", std::string());
        f.target = x.value("target", std::string());
        f.mechanism = x.value("mechanism", std::string());
        f.method = x.value("method", std::string());
        f.vector = vecOf(x.value("vector", json::array()));
        f.application = vecOf(x.value("application", json::array()));
        f.frame = x.value("frame", std::string("world"));
        f.time = x.value("time", std::string("final"));
        f.distribution = x.value("distribution", std::string());
        f.pair = x.value("pair", std::string());
        if (x.contains("uncertainty"))
            f.uncertainty = x["uncertainty"].get<double>();
        f.history = vecs(x.value("history", json::array()));
        l.forces.push_back(f);
    }
    for (const json& x : j.value("missing", json::array())) {
        auto f = familyOf(x);
        if (!f)
            return R::failure(f.message());
        l.missing.push_back({f.value(), x.value("entity", std::string()), x.value("what", std::string()), x.value("why", std::string())});
    }
    for (const json& x : j.value("numerical", json::array())) {
        auto f = familyOf(x);
        if (!f)
            return R::failure(f.message());
        l.numerical.push_back({f.value(), x.value("source", std::string()), x.value("method", std::string()), x.value("amount", 0.0)});
    }
    for (const json& x : j.value("contracts", json::array())) {
        ScaleContract c;
        c.id = x.value("id", std::string());
        c.sourceRepresentation = x.value("sourceRepresentation", std::string());
        c.targetEntity = x.value("targetEntity", std::string());
        c.quantity = x.value("quantity", std::string());
        c.method = x.value("method", std::string());
        c.averaging = x.value("averaging", std::string());
        c.resolution = x.value("resolution", std::string());
        c.conservation = x.value("conservation", std::string());
        c.sourceRepresented = x.value("sourceRepresented", false);
        if (x.contains("uncertainty"))
            c.uncertainty = x["uncertainty"].get<double>();
        l.contracts.push_back(c);
    }
    for (const json& x : j.value("quantities", json::array())) {
        auto q = Quantity::fromJson(x);
        if (!q)
            return R::failure(q.message());
        l.quantities.push_back(q.take());
    }
    for (const json& x : j.value("checks", json::array())) {
        auto c = Check::fromJson(x);
        if (!c)
            return R::failure(c.message());
        l.checks.push_back(c.take());
    }
    if (Status v = l.validate(); !v)
        return R::failure(v.message());
    return l;
}

}  // namespace resonant::accounting
