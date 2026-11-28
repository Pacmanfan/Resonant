#include "laboratory/ExperimentalEnergy.h"

#include <set>

namespace resonant::laboratory {

using nlohmann::json;
namespace acc = accounting;

json ExperimentalBalance::toJson() const
{
    json j{{"title", title}, {"basis", basis}, {"members", members}, {"coverage", coverage}};
    j["terms"] = json::array();
    for (const MeasuredTerm& t : terms)
        j["terms"].push_back({{"id", t.id}, {"role", t.role}, {"from", t.from}, {"to", t.to}, {"mechanism", t.mechanism}, {"value", t.value},
                              {"standard", t.standard}, {"direct", t.direct}, {"derivation", t.derivation}, {"datasets", t.datasets}});
    j["unmeasured"] = json::array();
    for (const auto& [what, why] : unmeasured)
        j["unmeasured"].push_back({{"what", what}, {"why", why}});
    return j;
}

Result<ExperimentalBalance> ExperimentalBalance::fromJson(const json& j)
{
    using R = Result<ExperimentalBalance>;
    if (!j.is_object())
        return R::failure("a balance is an object");
    ExperimentalBalance b;
    b.title = j.value("title", std::string("Experimental energy balance"));
    b.basis = j.value("basis", std::string("interval"));
    if (b.basis != "interval" && b.basis != "rate")
        return R::failure("the basis is interval (J) or rate (W)");
    b.members = j.value("members", std::vector<std::string>{});
    b.coverage = j.value("coverage", 2.0);
    for (const json& t : j.value("terms", json::array()))
        b.terms.push_back({t.value("id", std::string()), t.value("role", std::string()), t.value("from", std::string()), t.value("to", std::string()),
                           t.value("mechanism", std::string()), t.value("value", 0.0), t.value("standard", 0.0), t.value("direct", false),
                           t.value("derivation", std::string()), t.value("datasets", std::vector<std::string>{})});
    for (const json& u : j.value("unmeasured", json::array()))
        b.unmeasured.push_back({u.value("what", std::string()), u.value("why", std::string())});
    return b;
}

namespace {

acc::Entity entity(const std::string& id, acc::EntityKind kind)
{
    acc::Entity e;
    e.id = id;
    e.name = id;
    e.kind = kind;
    e.domain = "general";
    return e;
}

}  // namespace

Result<acc::Ledger> experimentalLedger(const ExperimentalBalance& b)
{
    using R = Result<acc::Ledger>;
    if (b.members.empty())
        return R::failure("the balance needs the members of its system");
    acc::Ledger l;
    l.id = "experimental";
    l.title = b.title;
    l.sourceKind = "measurements";
    l.solver = "laboratory";
    l.solverVersion = "1";
    l.basis = b.basis == "rate" ? acc::Basis::Rate : acc::Basis::Interval;
    l.measured = true;
    std::set<std::string> members(b.members.begin(), b.members.end()), outside;
    for (const std::string& m : b.members)
        l.entities.push_back(entity(m, acc::EntityKind::Body));
    const auto ensureOutside = [&](const std::string& id, acc::EntityKind kind) {
        if (members.count(id) || outside.count(id))
            return;
        outside.insert(id);
        l.entities.push_back(entity(id, kind));
    };
    for (const MeasuredTerm& t : b.terms) {
        if (t.id.empty())
            return R::failure("every term needs an id");
        if (!(t.standard >= 0.0))
            return R::failure("the term '" + t.id + "' has a negative uncertainty");
        if (!t.direct && t.derivation.empty())
            return R::failure("the term '" + t.id + "' is not measured directly: name its derivation");
        const std::string how = t.direct ? std::string("measured directly") : "derived: " + t.derivation;
        if (t.role == "stored") {
            if (!members.count(t.to))
                return R::failure("the stored term '" + t.id + "' is held by '" + t.to + "', not a member");
            acc::Holding h;
            h.id = t.id;
            h.entity = t.to;
            h.form = t.mechanism.empty() ? std::string("stored") : t.mechanism;
            h.initial = 0.0;
            h.final = t.value;
            h.uncertainty = t.standard;
            h.method = how;
            h.provenance = "measured";
            l.holdings.push_back(h);
            continue;
        }
        acc::Exchange x;
        x.id = t.id;
        x.family = acc::Family::Energy;
        x.mechanism = t.mechanism;
        x.amount = t.value;
        x.uncertainty = t.standard;
        x.method = how;
        x.provenance = "measured";
        if (t.role == "input") {
            if (!members.count(t.to))
                return R::failure("the input '" + t.id + "' goes to '" + t.to + "', not a member");
            x.from = t.from.empty() ? std::string("supply") : t.from;
            x.to = t.to;
            x.category = "input";
            ensureOutside(x.from, acc::EntityKind::Source);
        } else if (t.role == "output" || t.role == "dissipated") {
            if (!members.count(t.from))
                return R::failure("the term '" + t.id + "' comes from '" + t.from + "', not a member");
            x.from = t.from;
            x.to = t.to.empty() ? std::string(t.role == "dissipated" ? "heat" : "surroundings") : t.to;
            x.category = t.role == "dissipated" ? "dissipation" : "output";
            ensureOutside(x.to, t.role == "dissipated" ? acc::EntityKind::Sink : acc::EntityKind::Environment);
        } else {
            return R::failure("the term '" + t.id + "' has the role '" + t.role + "' (input, output, dissipated, stored)");
        }
        if (!t.datasets.empty())
            x.note = "from the datasets " + nlohmann::json(t.datasets).dump();
        l.exchanges.push_back(x);
    }
    for (const auto& [what, why] : b.unmeasured)
        l.missing.push_back({acc::Family::Energy, "", what, why.empty() ? std::string("not measured") : why});
    l.notes.push_back("a measured balance: judged against the expanded measurement uncertainty (k = " + std::to_string(b.coverage).substr(0, 4) +
                      "), the terms' uncertainties combined as uncorrelated");
    if (Status v = l.validate(); !v)
        return R::failure(v.message());
    return l;
}

}  // namespace resonant::laboratory
