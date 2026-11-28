#include "conversion/ConversionStage.h"

#include "accounting/ControlVolume.h"

#include <algorithm>
#include <cmath>

namespace resonant::conversion {
namespace {

using json = nlohmann::json;

std::string domainOf(EnergyForm f)
{
    switch (f) {
    case EnergyForm::Mechanical:
    case EnergyForm::Elastic: return "mechanical";
    case EnergyForm::Acoustic: return "acoustic";
    case EnergyForm::Thermal:
    case EnergyForm::Dissipative: return "thermal";
    case EnergyForm::Electrical: return "electrical";
    case EnergyForm::Electromagnetic: return "electromagnetic";
    case EnergyForm::Optical: return "optical";
    }
    return "mechanical";
}

std::string categoryOf(EnergyForm f)
{
    switch (f) {
    case EnergyForm::Thermal:
    case EnergyForm::Dissipative: return "heat";
    case EnergyForm::Acoustic:
    case EnergyForm::Electromagnetic:
    case EnergyForm::Optical: return "radiation";
    default: return "work";
    }
}

accounting::Exchange exchange(const std::string& id, const std::string& from, const std::string& to, const std::string& category,
                              const std::string& mechanism, EnergyForm fromForm, EnergyForm toForm, double amount, const std::string& method)
{
    accounting::Exchange e;
    e.id = id;
    e.from = from;
    e.to = to;
    e.family = accounting::Family::Energy;
    e.category = category;
    e.mechanism = mechanism;
    e.fromDomain = domainOf(fromForm);
    e.toDomain = domainOf(toForm);
    e.amount = amount;
    e.method = method;
    return e;
}

// The framework's order: exceeded > insufficient data > warning > not evaluated > verified > not applicable.
accounting::CheckStatus worstOf(const std::vector<accounting::CheckStatus>& all)
{
    const auto rank = [](accounting::CheckStatus s) {
        switch (s) {
        case accounting::CheckStatus::Exceeded: return 5;
        case accounting::CheckStatus::InsufficientData: return 4;
        case accounting::CheckStatus::Warning: return 3;
        case accounting::CheckStatus::NotEvaluated: return 2;
        case accounting::CheckStatus::Verified: return 1;
        case accounting::CheckStatus::NotApplicable: return 0;
        }
        return 2;
    };
    accounting::CheckStatus w = accounting::CheckStatus::NotApplicable;
    for (const auto s : all)
        if (rank(s) > rank(w))
            w = s;
    return w;
}

}  // namespace

std::string_view toString(EnergyForm form)
{
    switch (form) {
    case EnergyForm::Mechanical: return "mechanical";
    case EnergyForm::Acoustic: return "acoustic";
    case EnergyForm::Thermal: return "thermal";
    case EnergyForm::Electrical: return "electrical";
    case EnergyForm::Electromagnetic: return "electromagnetic";
    case EnergyForm::Optical: return "optical";
    case EnergyForm::Elastic: return "elastic";
    case EnergyForm::Dissipative: return "dissipative";
    }
    return "mechanical";
}

std::optional<EnergyForm> energyFormFromString(std::string_view text)
{
    for (const EnergyForm f : {EnergyForm::Mechanical, EnergyForm::Acoustic, EnergyForm::Thermal, EnergyForm::Electrical, EnergyForm::Electromagnetic,
                               EnergyForm::Optical, EnergyForm::Elastic, EnergyForm::Dissipative})
        if (toString(f) == text)
            return f;
    return std::nullopt;
}

double ConversionStage::otherInput() const
{
    double sum = 0.0;
    for (const auto& o : otherInputs)
        sum += o.amount;
    return sum;
}

double ConversionStage::otherOutput() const
{
    double sum = 0.0;
    for (const auto& o : otherOutputs)
        sum += o.amount;
    return sum;
}

double ConversionStage::residual() const
{
    return input + otherInput() - output - otherOutput() - dissipated - storedChange;
}

double ConversionStage::efficiency() const
{
    return input != 0.0 ? output / input : 0.0;
}

json ConversionStage::toJson() const
{
    json others = json::array(), otherIn = json::array();
    for (const auto& o : otherOutputs)
        others.push_back({{"name", o.name}, {"form", std::string(toString(o.form))}, {"amount", o.amount}});
    for (const auto& o : otherInputs)
        otherIn.push_back({{"name", o.name}, {"form", std::string(toString(o.form))}, {"amount", o.amount}});
    json out{{"id", id},
             {"name", name},
             {"mechanism", mechanism},
             {"model", model},
             {"inputForm", std::string(toString(inputForm))},
             {"outputForm", std::string(toString(outputForm))},
             {"input", input},
             {"output", output},
             {"otherInputs", otherIn},
             {"otherOutputs", others},
             {"storedChange", storedChange},
             {"storedPeak", storedPeak},
             {"dissipated", dissipated},
             {"residual", residual()},
             {"efficiency", efficiency()},
             {"coupling", coupling},
             {"boundaryConditions", boundaryConditions},
             {"assumptions", assumptions},
             {"validity", validity},
             {"missing", missing}};
    if (uncertainty)
        out["uncertainty"] = *uncertainty;
    return out;
}

accounting::Ledger chainLedger(const ConversionChain& chain)
{
    accounting::Ledger l;
    l.id = chain.id.empty() ? "conversion-chain" : chain.id;
    l.title = chain.title.empty() ? "Energy-conversion chain" : chain.title;
    l.sourceKind = "conversion.chain";
    l.solver = "conversion stages";
    l.basis = chain.rate ? accounting::Basis::Rate : accounting::Basis::Interval;
    if (!chain.rate)
        l.t1 = 1.0;
    if (chain.stages.empty())
        return l;
    const ConversionStage& first = chain.stages.front();
    const ConversionStage& last = chain.stages.back();
    l.entities.push_back({"source", "Source", accounting::EntityKind::Source, domainOf(first.inputForm), "", "", "", "", ""});
    for (const auto& s : chain.stages)
        l.entities.push_back({s.id, s.name.empty() ? s.id : s.name, accounting::EntityKind::Body, domainOf(s.inputForm), "", "", "", "", s.model});
    l.entities.push_back({"load", "Load", accounting::EntityKind::Sink, domainOf(last.outputForm), "", "", "", "", ""});
    l.entities.push_back({"surroundings", "Surroundings", accounting::EntityKind::Environment, "thermal", "", "", "", "", ""});
    l.exchanges.push_back(exchange("in", "source", first.id, categoryOf(first.inputForm), "supplied", first.inputForm, first.inputForm, first.input, first.model));
    for (size_t i = 0; i < chain.stages.size(); ++i) {
        const ConversionStage& s = chain.stages[i];
        if (i + 1 < chain.stages.size()) {
            const ConversionStage& next = chain.stages[i + 1];
            accounting::Exchange e = exchange(s.id + "->" + next.id, s.id, next.id, categoryOf(s.outputForm), s.mechanism, s.inputForm, s.outputForm, next.input, s.model);
            if (std::abs(next.input - s.output) > 0.0)
                e.sourceAmount = s.output;  // what the sending stage gives up, when it differs from what the next receives
            l.exchanges.push_back(e);
        } else {
            l.exchanges.push_back(exchange(s.id + "->load", s.id, "load", categoryOf(s.outputForm), s.mechanism, s.inputForm, s.outputForm, s.output, s.model));
        }
        if (s.dissipated != 0.0)
            l.exchanges.push_back(exchange(s.id + ".dissipation", s.id, "surroundings", "dissipation", "dissipation", s.inputForm, EnergyForm::Thermal, s.dissipated, s.model));
        for (size_t k = 0; k < s.otherInputs.size(); ++k) {
            const std::string src = "source:" + s.id + "." + std::to_string(k);
            l.entities.push_back({src, s.otherInputs[k].name, accounting::EntityKind::Source, domainOf(s.otherInputs[k].form), "", "", "", "", ""});
            l.exchanges.push_back(exchange(s.id + ".in" + std::to_string(k), src, s.id, categoryOf(s.otherInputs[k].form), s.otherInputs[k].name,
                                           s.otherInputs[k].form, s.otherInputs[k].form, s.otherInputs[k].amount, s.model));
        }
        for (size_t k = 0; k < s.otherOutputs.size(); ++k)
            l.exchanges.push_back(exchange(s.id + ".out" + std::to_string(k), s.id, "surroundings", categoryOf(s.otherOutputs[k].form), s.otherOutputs[k].name,
                                           s.inputForm, s.otherOutputs[k].form, s.otherOutputs[k].amount, s.model));
        if (s.storedChange != 0.0) {
            accounting::Holding h;
            h.id = s.id + ".stored";
            h.entity = s.id;
            h.family = accounting::Family::Energy;
            h.form = "elastic";
            h.final = s.storedChange;
            h.method = s.model;
            l.holdings.push_back(h);
        }
        for (const auto& m : s.missing)
            l.missing.push_back({accounting::Family::Energy, s.id, m, "not computed by " + (s.model.empty() ? std::string("the stage's model") : s.model)});
    }
    return l;
}

ChainReport evaluateChain(const ConversionChain& chain, const accounting::Tolerances& tolerances)
{
    ChainReport out;
    out.ledger = chainLedger(chain);
    std::vector<accounting::CheckStatus> all;
    for (const auto& s : chain.stages) {
        accounting::AccountingSystem system;
        system.id = s.id;
        system.name = s.name.empty() ? s.id : s.name;
        system.kind = accounting::SystemKind::Object;
        system.members = {s.id};
        const accounting::Report report = accounting::evaluate(out.ledger, system, tolerances);
        StageCheck c;
        c.stage = s.id;
        c.missing = s.missing;
        for (const auto& check : report.checks)
            if (check.family == accounting::Family::Energy && check.scope == "global") {
                c.status = check.status;
                c.residual = check.residual[0];
                c.relative = check.relative;
                break;
            }
        all.push_back(c.status);
        out.stages.push_back(c);
    }
    for (size_t i = 0; i + 1 < chain.stages.size(); ++i) {
        LinkCheck link;
        link.from = chain.stages[i].id;
        link.to = chain.stages[i + 1].id;
        link.leaving = chain.stages[i].output;
        link.entering = chain.stages[i + 1].input;
        const double scale = std::max({std::abs(link.leaving), std::abs(link.entering), tolerances.absolute});
        link.joined = std::abs(link.leaving - link.entering) <= tolerances.relative * scale;
        if (!link.joined)
            link.note = "the stages do not join: " + std::to_string(link.leaving) + " leave '" + link.from + "', " + std::to_string(link.entering) +
                        " enter '" + link.to + "'";
        all.push_back(link.joined ? accounting::CheckStatus::Verified : accounting::CheckStatus::Exceeded);
        out.links.push_back(link);
    }
    out.status = all.empty() ? accounting::CheckStatus::NotEvaluated : worstOf(all);
    if (!chain.stages.empty() && chain.stages.front().input != 0.0)
        out.overallEfficiency = chain.stages.back().output / chain.stages.front().input;
    return out;
}

json ChainReport::toJson() const
{
    json st = json::array(), ln = json::array();
    for (const auto& s : stages) {
        json j{{"stage", s.stage}, {"status", std::string(accounting::toString(s.status))}, {"residual", s.residual}, {"missing", s.missing}};
        if (s.relative)
            j["relative"] = *s.relative;
        st.push_back(j);
    }
    for (const auto& l : links)
        ln.push_back({{"from", l.from}, {"to", l.to}, {"leaving", l.leaving}, {"entering", l.entering}, {"joined", l.joined}, {"note", l.note}});
    return {{"stages", st}, {"links", ln}, {"status", std::string(accounting::toString(status))}, {"overallEfficiency", overallEfficiency},
            {"ledger", ledger.toJson()}};
}

}  // namespace resonant::conversion
