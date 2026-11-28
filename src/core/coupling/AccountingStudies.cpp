#include "coupling/AccountingStudies.h"

#include "accounting/Conservation.h"
#include "coupling/AccountingAdapters.h"
#include "coupling/CoupledRun.h"
#include "coupling/MaterialStateModels.h"
#include "coupling/StateEvolution.h"
#include "foundation/Time.h"
#include "materialstate/StateModel.h"
#include "multiphysics/PhysicalSystem.h"
#include "multiscale/Representation.h"
#include "project/Project.h"
#include "simulation/Job.h"

#include <cmath>
#include <set>
#include <sstream>

namespace resonant::coupling {

using nlohmann::json;
namespace acc = accounting;

namespace {

constexpr double kPi = 3.14159265358979323846;

Result<oscillators::Network> networkOf(const json& j)
{
    using R = Result<oscillators::Network>;
    if (!j.is_object())
        return R::failure("the source needs a network {names, masses, springs, dampers}");
    oscillators::Network n;
    n.names = j.value("names", std::vector<std::string>{});
    n.masses = j.value("masses", std::vector<double>{});
    for (const json& s : j.value("springs", json::array()))
        n.springs.push_back({s.value("name", std::string()), s.value("i", 0), s.value("j", -1), s.value("k", 0.0)});
    for (const json& d : j.value("dampers", json::array()))
        n.dampers.push_back({d.value("mechanism", std::string("viscous")), d.value("i", 0), d.value("j", -1), d.value("c", 0.0)});
    if (Status v = n.validate(); !v)
        return R::failure(v.message());
    return n;
}

NetworkAccounting accountingOf(const json& j, const std::string& title)
{
    NetworkAccounting a;
    a.rotational = j.value("rotational", false);
    if (j.contains("axis"))
        a.axis = j["axis"].get<acc::Vec3>();
    a.objects = j.value("objects", std::vector<std::string>{});
    a.title = title;
    return a;
}

// A force schedule per coordinate: harmonic, constant or none, active in [from, until).
std::function<std::vector<double>(double)> forceOf(const json& f, int n)
{
    if (!f.is_object() || f.value("kind", std::string("none")) == "none")
        return {};
    const std::string kind = f.value("kind", std::string("harmonic"));
    const int i = f.value("coordinate", 0);
    const double a = f.value("amplitude", 1.0), w = 2.0 * kPi * f.value("frequencyHz", 1.0), phase = f.value("phase", 0.0);
    const double from = f.value("from", 0.0), until = f.value("until", 1e300);
    return [=](double t) {
        std::vector<double> out(static_cast<size_t>(n), 0.0);
        if (i >= 0 && i < n && t >= from && t < until)
            out[static_cast<size_t>(i)] = kind == "constant" ? a : a * std::cos(w * t + phase);
        return out;
    };
}

std::vector<std::complex<double>> phasors(const json& j, int n)
{
    std::vector<std::complex<double>> out(static_cast<size_t>(n), 0.0);
    if (j.is_array())
        for (size_t k = 0; k < j.size() && k < out.size(); ++k)
            out[k] = j[k].is_array() ? std::complex<double>(j[k][0].get<double>(), j[k].size() > 1 ? j[k][1].get<double>() : 0.0)
                                     : std::complex<double>(j[k].get<double>(), 0.0);
    return out;
}

Result<multiphysics::PhysicalSystem> systemOf(const Project& p, const std::string& id)
{
    for (const json& j : p.extensions.value("multiphysics", json::object()).value("systems", json::array()))
        if (j.value("id", std::string()) == id)
            return multiphysics::PhysicalSystem::fromJson(j);
    return Result<multiphysics::PhysicalSystem>::failure("there is no physical system '" + id + "'");
}

// Scale contracts: regions whose materials carry values derived from another representation.
void addContracts(acc::Ledger& l, const multiphysics::PhysicalSystem& sys, const Project& p)
{
    auto model = multiscale::modelOf(p);
    for (const auto& region : sys.regions) {
        const Material* m = p.materials.find(Id(region.materialId));
        if (!m)
            continue;
        std::set<std::string> sources;
        std::string method;
        for (PhysicsDomain d : {PhysicsDomain::Electromagnetic, PhysicsDomain::Thermal, PhysicsDomain::Mechanical, PhysicsDomain::Acoustic})
            if (const PropertySet* set = m->properties(d))
                for (const auto& [key, v] : *set) {
                    if (!v.derivation.empty() && model) {
                        if (const multiscale::Derivation* drv = model.value().derivation(v.derivation)) {
                            sources.insert(drv->representation);
                            method = drv->method;
                        }
                    } else if (v.provenance == Provenance::Homogenized) {
                        sources.insert("homogenized structure");
                        method = "effective-medium analysis";
                    }
                }
        if (sources.empty())
            continue;
        const std::string entityId = "region:" + region.id;
        acc::Entity e;
        e.id = entityId;
        e.name = "Region " + (region.name.empty() ? region.id : region.name);
        e.kind = acc::EntityKind::Region;
        e.material = region.materialId;
        e.effectiveOf = *sources.begin();
        e.note = "uses effective properties; holds nothing in this ledger (its energy is the participants')";
        l.entities.push_back(e);
        for (const std::string& src : sources) {
            acc::ScaleContract c;
            c.id = "contract:" + region.id + ":" + src;
            c.sourceRepresentation = src;
            c.targetEntity = entityId;
            c.quantity = "material properties of " + region.materialId;
            c.method = method;
            c.averaging = "the derivation's homogenization (see its record)";
            c.resolution = "the region's cells";
            c.conservation = "the effective description replaces the source: its energy is not counted again";
            c.sourceRepresented = false;
            l.contracts.push_back(c);
        }
    }
}

Result<acc::Ledger> ledgerFor(const json& source, const json& study, const Project& p, JobContext* ctx, json& extra)
{
    using R = Result<acc::Ledger>;
    const std::string kind = source.value("kind", std::string());
    const std::string title = study.value("name", study.value("id", std::string("Accounting")));
    multiphysics::RunContext run;
    if (ctx) {
        run.cancelled = [ctx] { return ctx->cancelRequested(); };
        run.progress = [ctx](double f, const std::string& m) { ctx->reportProgress(f, m); };
    }
    if (kind == "network") {
        auto net = networkOf(source.value("network", json()));
        if (!net)
            return R::failure(net.message());
        const json t = source.value("transient", json::object());
        oscillators::TransientOptions o;
        o.duration = t.value("duration", 0.0);
        o.dt = t.value("dt", 0.0);
        o.x0 = t.value("x0", std::vector<double>{});
        o.v0 = t.value("v0", std::vector<double>{});
        o.samples = std::clamp(t.value("samples", 400), 10, 100000);
        o.force = forceOf(t.value("force", json()), net.value().size());
        o.elementFlows = true;
        auto rec = oscillators::integrate(net.value(), o, ctx);
        if (!rec)
            return R::failure(rec.message());
        auto l = networkLedger(net.value(), rec.value(), accountingOf(source.value("network", json()), title));
        if (!l)
            return l;
        if (source.contains("resonance")) {
            const json r = source["resonance"];
            const json f = t.value("force", json::object());
            std::vector<std::complex<double>> phasor(static_cast<size_t>(net.value().size()), 0.0);
            phasor[static_cast<size_t>(std::clamp(f.value("coordinate", 0), 0, net.value().size() - 1))] = f.value("amplitude", 1.0);
            l.value().checks.push_back(buildupDecayCheck(net.value(), rec.value(), r.value("frequencyHz", f.value("frequencyHz", 1.0)), phasor,
                                                         r.value("driveOff", f.value("until", o.duration)), r.value("tolerance", 0.02)));
        }
        if (source.value("refine", false)) {
            // The same run at half the step: the residuals' change with refinement.
            o.dt = rec.value().dt / 2.0;
            auto fine = oscillators::integrate(net.value(), o, ctx);
            if (!fine)
                return R::failure(fine.message());
            auto lf = networkLedger(net.value(), fine.value(), accountingOf(source.value("network", json()), title));
            if (!lf)
                return lf;
            extra["refined"] = lf.value().toJson();
            extra["refinedStep"] = o.dt;
            extra["step"] = rec.value().dt;
        }
        return l;
    }
    if (kind == "network-driven") {
        auto net = networkOf(source.value("network", json()));
        if (!net)
            return R::failure(net.message());
        double f = source.value("frequencyHz", 0.0);
        if (source.contains("mode")) {
            auto modes = oscillators::naturalModes(net.value());
            if (!modes)
                return R::failure(modes.message());
            const int m = source.value("mode", 0);
            if (m < 0 || m >= static_cast<int>(modes.value().size()))
                return R::failure("the network has no mode " + std::to_string(m));
            f = modes.value()[static_cast<size_t>(m)].undampedHz;
        }
        if (!(f > 0.0))
            return R::failure("the driven source needs a frequency (frequencyHz or mode)");
        return drivenNetworkLedger(net.value(), phasors(source.value("force", json::array({1.0})), net.value().size()), f,
                                   accountingOf(source.value("network", json()), title));
    }
    if (kind == "physical-system" || kind == "evolution") {
        auto sys = systemOf(p, source.value("system", std::string()));
        if (!sys)
            return R::failure(sys.message());
        if (kind == "physical-system") {
            auto r = runPhysicalSystem(sys.value(), p.materials, run);
            if (!r)
                return R::failure(r.message());
            if (!r.value().ok())
                return R::failure("the coupled run did not complete: " + r.value().message);
            auto l = coupledLedger(r.value().ledger.toJson(), r.value().audit.toJson(), sys.value().name.empty() ? sys.value().id : sys.value().name);
            if (!l)
                return l;
            addContracts(l.value(), sys.value(), p);
            extra["engineAudit"] = r.value().audit.toJson();
            return l;
        }
        EvolutionSettings s;
        s.duration = source.value("duration", 0.0);
        s.steps = source.value("steps", 20);
        s.substeps = source.value("substeps", 4);
        auto doc = evolveSystem(sys.value(), p.materials, s, ctx);
        if (!doc)
            return R::failure(doc.message());
        auto l = evolutionLedger(doc.value());
        if (!l)
            return l;
        l.value().title = "Evolution of " + (sys.value().name.empty() ? sys.value().id : sys.value().name);
        addContracts(l.value(), sys.value(), p);
        return l;
    }
    if (kind == "radiation") {
        em::LayeredProblem prob;
        prob.frequencyHz = source.value("frequencyHz", 1e9);
        prob.incidentIntensity = source.value("intensity", 1.0);
        for (const json& j : source.value("layers", json::array())) {
            em::MediumLayer m;
            m.name = j.value("name", std::string("layer"));
            m.thickness = j.value("thickness", 0.0);
            m.permittivityReal = j.value("permittivity", 1.0);
            m.lossTangent = j.value("lossTangent", 0.0);
            m.conductivity = j.value("conductivity", 0.0);
            m.permeability = j.value("permeability", 1.0);
            prob.layers.push_back(m);
        }
        auto sol = em::LayeredSolution::solve(prob);
        if (!sol)
            return R::failure(sol.message());
        return radiationLedger(sol.value(), source.value("area", 1.0), source.value("points", 2001),
                               study.value("tolerances", json::object()).value("relative", 1e-6));
    }
    if (kind == "material-state") {
        json st = source.value("inline", json());
        if (source.contains("study")) {
            auto w = materialstate::workspaceOf(p);
            if (!w)
                return R::failure(w.message());
            for (const json& s : w.value().studies)
                if (s.value("id", std::string()) == source["study"].get<std::string>())
                    st = s;
            if (st.is_null())
                return R::failure("there is no material-state study '" + source["study"].get<std::string>() + "'");
        }
        if (!st.is_object())
            return R::failure("the material-state source needs a study id or an inline study");
        auto r = runMaterialStateStudy(st, ctx);
        if (!r)
            return R::failure(r.message());
        auto l = materialStateLedger(r.value(), source.value("temperature", 293.15));
        if (l)
            l.value().title = st.value("name", l.value().title);
        return l;
    }
    return R::failure("unknown accounting source '" + kind + "'");
}

json reportJson(const acc::Ledger& l, const acc::AccountingSystem& s, const acc::Tolerances& t)
{
    return acc::evaluate(l, s, t).toJson();
}

Result<std::vector<acc::AccountingSystem>> systemsOf(const json& study, const acc::Ledger& l)
{
    using R = Result<std::vector<acc::AccountingSystem>>;
    std::vector<acc::AccountingSystem> out{acc::wholeSystem(l)};
    for (const json& j : study.value("systems", json::array())) {
        auto s = acc::AccountingSystem::fromJson(j);
        if (!s)
            return R::failure(s.message());
        if (Status v = acc::validateFor(s.value(), l); !v)
            return R::failure(v.message());
        out.push_back(s.take());
    }
    return out;
}

std::string csvField(std::string s)
{
    if (s.find_first_of(",\"\n") == std::string::npos)
        return s;
    std::string out = "\"";
    for (char c : s)
        out += c == '"' ? std::string("\"\"") : std::string(1, c);
    return out + "\"";
}

std::string number(const json& v)
{
    if (v.is_number()) {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%.6g", v.get<double>());
        return buf;
    }
    if (v.is_array()) {
        std::string s = "(";
        for (size_t i = 0; i < v.size(); ++i)
            s += (i ? ", " : "") + number(v[i]);
        return s + ")";
    }
    return v.is_null() ? "-" : v.dump();
}

}  // namespace

json accountingSourceKinds()
{
    return json::array({
        {{"kind", "network"}, {"title", "Lumped network transient"}, {"needs", "network {names, masses, springs, dampers}, transient {duration, dt, x0, v0, force}"},
         {"accounts", "energy, linear or angular momentum, forces or torques, histories"}},
        {{"kind", "network-driven"}, {"title", "Driven steady state of a network"}, {"needs", "network, force phasors, frequencyHz or mode"},
         {"accounts", "power, Q = w E / P by mechanism"}},
        {{"kind", "physical-system"}, {"title", "Coupled run of a physical system"}, {"needs", "the id of a physical system of the project"},
         {"accounts", "energy across domains (momentum not reported by the participants)"}},
        {{"kind", "evolution"}, {"title", "Time-stepped evolution of a physical system"}, {"needs", "system, duration, steps, substeps"},
         {"accounts", "energy per step, histories"}},
        {{"kind", "radiation"}, {"title", "Plane wave on a layered body"}, {"needs", "frequencyHz, intensity, area, layers between vacuum half-spaces"},
         {"accounts", "energy, momentum, the radiation force by two methods, local absorption"}},
        {{"kind", "material-state"}, {"title", "Material-state model"}, {"needs", "a material-state study id or an inline study"},
         {"accounts", "work, stored and dissipated energy, latent heat, entropy, hysteresis loss"}},
    });
}

Status validateAccountingStudy(const json& study)
{
    if (!study.is_object() || study.value("id", std::string()).empty())
        return Status::failure("an accounting study needs an id");
    const std::string kind = study.value("source", json::object()).value("kind", std::string());
    for (const json& k : accountingSourceKinds())
        if (k["kind"] == kind) {
            for (const json& s : study.value("systems", json::array()))
                if (auto v = acc::AccountingSystem::fromJson(s); !v)
                    return Status::failure(v.message());
            return Status::success();
        }
    return Status::failure("unknown accounting source '" + kind + "'");
}

Result<json> runAccountingStudy(const json& study, const Project& p, JobContext* ctx)
{
    using R = Result<json>;
    if (Status v = validateAccountingStudy(study); !v)
        return R::failure(v.message());
    json extra = json::object();
    auto l = ledgerFor(study["source"], study, p, ctx, extra);
    if (!l)
        return R::failure(l.message());
    auto systems = systemsOf(study, l.value());
    if (!systems)
        return R::failure(systems.message());
    const acc::Tolerances t = acc::Tolerances::fromJson(study.value("tolerances", json::object()));
    json d{{"format", kAccountingResultFormat}, {"version", 1}, {"study", study}, {"title", study.value("name", l.value().title)},
           {"created", utcNowIso8601()}, {"tolerances", t.toJson()}, {"ledger", l.value().toJson()}};
    d["systems"] = json::array();
    d["reports"] = json::array();
    std::vector<acc::Check> all;
    for (const acc::AccountingSystem& s : systems.value()) {
        const acc::Report rep = acc::evaluate(l.value(), s, t);
        d["systems"].push_back(s.toJson());
        d["reports"].push_back(rep.toJson());
        all.insert(all.end(), rep.checks.begin(), rep.checks.end());
    }
    d["status"] = std::string(acc::toString(acc::worst(all)));
    // Refinement: each balance of each system at the half step.
    if (extra.contains("refined")) {
        auto fine = acc::Ledger::fromJson(extra["refined"]);
        if (!fine)
            return R::failure(fine.message());
        json conv = json::array();
        for (size_t k = 0; k < systems.value().size(); ++k) {
            const acc::Report coarse = acc::evaluate(l.value(), systems.value()[k], t);
            const acc::Report finer = acc::evaluate(fine.value(), systems.value()[k], t);
            for (size_t c = 0; c < coarse.checks.size() && c < finer.checks.size(); ++c) {
                const acc::Check& a = coarse.checks[c];
                const acc::Check& b = finer.checks[c];
                if (a.scope != "global" || a.status == acc::CheckStatus::NotApplicable)
                    continue;
                const double worstA = a.residualHistory.empty() ? a.residualNorm() : *std::max_element(a.residualHistory.begin(), a.residualHistory.end());
                const double worstB = b.residualHistory.empty() ? b.residualNorm() : *std::max_element(b.residualHistory.begin(), b.residualHistory.end());
                json row{{"system", systems.value()[k].id}, {"model", a.model}, {"unit", a.unit}, {"step", extra["step"]}, {"residual", worstA},
                         {"refinedStep", extra["refinedStep"]}, {"refinedResidual", worstB}};
                row["ratio"] = worstB > 0.0 ? json(worstA / worstB) : json(nullptr);
                row["order"] = worstB > 0.0 && worstA > 0.0 ? json(std::log2(worstA / worstB)) : json(nullptr);
                conv.push_back(row);
            }
        }
        d["convergence"] = conv;
    }
    if (extra.contains("engineAudit"))
        d["engineAudit"] = extra["engineAudit"];
    return d;
}

Result<json> accountingReport(const json& result, const json& system)
{
    using R = Result<json>;
    if (!result.is_object() || result.value("format", std::string()) != kAccountingResultFormat)
        return R::failure("not an accounting result");
    auto l = acc::Ledger::fromJson(result["ledger"]);
    if (!l)
        return R::failure(l.message());
    acc::AccountingSystem s = acc::wholeSystem(l.value());
    if (system.is_object() && !system.empty()) {
        auto parsed = acc::AccountingSystem::fromJson(system);
        if (!parsed)
            return R::failure(parsed.message());
        s = parsed.take();
        if (Status v = acc::validateFor(s, l.value()); !v)
            return R::failure(v.message());
    }
    return reportJson(l.value(), s, acc::Tolerances::fromJson(result.value("tolerances", json::object())));
}

Result<json> compareAccountingSystems(const json& result, const json& systems)
{
    using R = Result<json>;
    if (!systems.is_array() || systems.size() < 2)
        return R::failure("compare at least two systems");
    json reports = json::array();
    std::vector<std::map<std::string, std::string>> roles;
    for (const json& s : systems) {
        auto r = accountingReport(result, s);
        if (!r)
            return r;
        std::map<std::string, std::string> m;
        for (const json& x : r.value()["classified"])
            m[x.value("id", std::string())] = x.value("role", std::string());
        roles.push_back(m);
        reports.push_back(r.take());
    }
    json differences = json::array();
    for (const auto& [id, role] : roles.front()) {
        json row{{"exchange", id}, {"roles", json::array()}};
        bool differs = false;
        for (const auto& m : roles) {
            const std::string r = m.count(id) ? m.at(id) : std::string("outside");
            row["roles"].push_back(r);
            differs = differs || r != role;
        }
        if (differs)
            differences.push_back(row);
    }
    return json{{"reports", reports}, {"differences", differences},
                {"note", "the same exchanges, classified by each system's members: internal to one, a boundary flow of another"}};
}

Result<std::string> exportAccounting(const json& result, const std::string& format)
{
    using R = Result<std::string>;
    if (!result.is_object() || result.value("format", std::string()) != kAccountingResultFormat)
        return R::failure("not an accounting result");
    if (format == "json")
        return result.dump(1);
    std::ostringstream o;
    if (format == "csv") {
        o << "system,check,family,status,residual,relative,tolerance,unit,attribution\n";
        for (const json& r : result.value("reports", json::array()))
            for (const json& c : r.value("checks", json::array()))
                o << csvField(r["system"].value("id", std::string())) << ',' << csvField(c.value("model", std::string())) << ','
                  << c.value("family", std::string()) << ',' << csvField(c.value("status", std::string())) << ',' << number(c.value("residualNorm", json()))
                  << ',' << number(c.value("relative", json())) << ',' << number(c.value("tolerance", json())) << ',' << csvField(c.value("unit", std::string()))
                  << ',' << csvField(c.value("attribution", std::string())) << '\n';
        o << "\nexchange,from,to,family,category,mechanism,amount,unit\n";
        for (const json& x : result["ledger"].value("exchanges", json::array()))
            o << csvField(x.value("id", std::string())) << ',' << csvField(x.value("from", std::string())) << ',' << csvField(x.value("to", std::string()))
              << ',' << x.value("family", std::string()) << ',' << csvField(x.value("category", std::string())) << ','
              << csvField(x.value("mechanism", std::string())) << ',' << number(x.contains("vector") ? x["vector"] : x.value("amount", json())) << ','
              << csvField(x.value("unit", std::string())) << '\n';
        return o.str();
    }
    if (format == "markdown") {
        o << "# " << result.value("title", std::string("Accounting")) << "\n\n";
        const json& l = result["ledger"];
        o << "Source: " << l.value("sourceKind", std::string()) << " (" << l.value("solver", std::string()) << "), basis "
          << l.value("basis", std::string()) << ". Overall status: **" << result.value("status", std::string()) << "**.\n";
        for (const json& r : result.value("reports", json::array())) {
            o << "\n## " << r["system"].value("name", std::string()) << "\n\n| Check | Status | Residual | Relative | Unit | Attribution |\n|---|---|---|---|---|---|\n";
            for (const json& c : r.value("checks", json::array()))
                o << "| " << c.value("title", std::string()) << " | " << c.value("status", std::string()) << " | " << number(c.value("residualNorm", json()))
                  << " | " << number(c.value("relative", json())) << " | " << c.value("unit", std::string()) << " | " << c.value("attribution", std::string())
                  << " |\n";
            for (const json& c : r.value("checks", json::array()))
                for (const json& m : c.value("missing", json::array()))
                    o << "\n- Missing (" << c.value("title", std::string()) << "): " << m.get<std::string>();
            o << "\n";
        }
        o << "\n## Exchanges\n\n| From | To | Category | Mechanism | Amount | Unit |\n|---|---|---|---|---|---|\n";
        for (const json& x : l.value("exchanges", json::array()))
            o << "| " << x.value("from", std::string()) << " | " << x.value("to", std::string()) << " | " << x.value("category", std::string()) << " | "
              << x.value("mechanism", std::string()) << " | " << number(x.contains("vector") ? x["vector"] : x.value("amount", json())) << " | "
              << x.value("unit", std::string()) << " |\n";
        for (const json& n : l.value("notes", json::array()))
            o << "\n- " << n.get<std::string>();
        o << "\n";
        return o.str();
    }
    return R::failure("unknown export format '" + format + "' (json, csv, markdown)");
}

Result<json> queryAccounting(const json& result, const json& system, const std::string& facet)
{
    using R = Result<json>;
    if (!result.is_object() || result.value("format", std::string()) != kAccountingResultFormat)
        return R::failure("not an accounting result");
    auto l = acc::Ledger::fromJson(result["ledger"]);
    if (!l)
        return R::failure(l.message());
    acc::AccountingSystem s = acc::wholeSystem(l.value());
    if (system.is_object() && !system.empty()) {
        auto parsed = acc::AccountingSystem::fromJson(system);
        if (!parsed)
            return R::failure(parsed.message());
        s = parsed.take();
        if (Status v = acc::validateFor(s, l.value()); !v)
            return R::failure(v.message());
    }
    const acc::Ledger& L = l.value();
    const json interval = L.basis == acc::Basis::Rate ? json("steady (rates)") : json::array({L.t0, L.t1});
    json out = json::array();
    const auto familyFacet = [&](acc::Family f) {
        for (const acc::Exchange& x : L.exchanges) {
            if (x.family != f)
                continue;
            const acc::Role role = acc::classify(x, L, s);
            if (role == acc::Role::Outside)
                continue;
            json q{{"id", x.id}, {"kind", "exchange"}, {"family", std::string(acc::toString(x.family))}, {"role", std::string(acc::toString(role))},
                   {"category", x.category}, {"mechanism", x.mechanism}, {"from", x.from}, {"to", x.to}, {"fromDomain", x.fromDomain},
                   {"toDomain", x.toDomain}, {"region", x.region}, {"unit", L.unit(x.family)}, {"interval", interval}, {"method", x.method},
                   {"provenance", x.provenance}, {"source", L.solver}};
            if (x.isVector)
                q["vector"] = x.vector;
            else
                q["value"] = role == acc::Role::Output || role == acc::Role::Dissipated ? x.sourceAmount.value_or(x.amount) : x.amount;
            q["uncertainty"] = x.uncertainty ? json(*x.uncertainty) : json(nullptr);
            out.push_back(q);
        }
        for (const acc::Holding& h : L.holdings) {
            if (h.family != f || !s.contains(h.entity))
                continue;
            json q{{"id", h.id}, {"kind", "holding"}, {"family", std::string(acc::toString(h.family))}, {"entity", h.entity}, {"form", h.form},
                   {"recoverable", h.recoverable}, {"unit", L.unit(h.family)}, {"interval", interval}, {"method", h.method}, {"provenance", h.provenance},
                   {"source", L.solver}};
            if (h.isVector) {
                q["initial"] = h.initialVector;
                q["final"] = h.finalVector;
            } else {
                q["initial"] = h.initial;
                q["final"] = h.final;
            }
            out.push_back(q);
        }
        for (const acc::Generation& g : L.generations)
            if (g.family == f && s.contains(g.entity))
                out.push_back({{"id", g.id}, {"kind", "generation"}, {"family", std::string(acc::toString(g.family))}, {"entity", g.entity},
                               {"mechanism", g.mechanism}, {"value", g.amount}, {"unit", L.unit(g.family)}, {"interval", interval}, {"method", g.method},
                               {"source", L.solver}});
    };
    if (facet == "energy")
        familyFacet(acc::Family::Energy);
    else if (facet == "momentum")
        familyFacet(acc::Family::LinearMomentum);
    else if (facet == "angular-momentum")
        familyFacet(acc::Family::AngularMomentum);
    else if (facet == "entropy")
        familyFacet(acc::Family::Entropy);
    else if (facet == "forces" || facet == "torques") {
        const acc::Family want = facet == "forces" ? acc::Family::Force : acc::Family::Torque;
        for (const acc::ForceRecord& f : L.forces)
            if (f.family == want && (s.contains(f.target) || s.contains(f.source)))
                out.push_back({{"id", f.id}, {"kind", "force"}, {"family", std::string(acc::toString(f.family))}, {"category", f.category},
                               {"model", f.model}, {"source", f.source}, {"target", f.target}, {"vector", f.vector}, {"application", f.application},
                               {"frame", f.frame}, {"time", f.time}, {"mechanism", f.mechanism}, {"method", f.method}, {"pair", f.pair},
                               {"external", s.contains(f.target) && !s.contains(f.source)}, {"unit", acc::info(f.family).unit},
                               {"uncertainty", f.uncertainty ? json(*f.uncertainty) : json(nullptr)}, {"distribution", f.distribution}});
    } else if (facet == "transfers") {
        for (const acc::Exchange& x : L.exchanges)
            if (x.category == "transfer" && acc::classify(x, L, s) != acc::Role::Outside)
                out.push_back({{"id", x.id}, {"kind", "transfer"}, {"family", std::string(acc::toString(x.family))},
                               {"role", std::string(acc::toString(acc::classify(x, L, s)))}, {"from", x.from}, {"to", x.to},
                               {"fromDomain", x.fromDomain}, {"toDomain", x.toDomain}, {"mechanism", x.mechanism}, {"value", x.amount},
                               {"sourceValue", x.sourceAmount.value_or(x.amount)}, {"unit", L.unit(x.family)}, {"method", x.method},
                               {"includesDestination", x.includesDestination}, {"interval", interval}, {"note", x.note}});
    } else if (facet == "missing") {
        for (const acc::Missing& m : L.missing)
            if (m.entity.empty() || s.contains(m.entity))
                out.push_back({{"kind", "missing"}, {"family", std::string(acc::toString(m.family))}, {"entity", m.entity}, {"what", m.what}, {"why", m.why}});
    } else if (facet == "residuals") {
        const acc::Report rep = acc::evaluate(L, s, acc::Tolerances::fromJson(result.value("tolerances", json::object())));
        for (const acc::Check& c : rep.checks)
            out.push_back(c.toJson());
    } else {
        return R::failure("unknown facet '" + facet + "' (energy, momentum, angular-momentum, entropy, forces, torques, transfers, missing, residuals)");
    }
    return json{{"facet", facet}, {"system", s.toJson()}, {"basis", std::string(acc::toString(L.basis))}, {"per", L.per}, {"quantities", out}};
}

json accountingRecord(const json& result, const std::string& id, const std::string& name, int samples)
{
    json l = result.value("ledger", json::object());
    const auto times = l.value("times", std::vector<double>{});
    if (samples >= 2 && static_cast<int>(times.size()) > samples) {
        std::vector<size_t> keep;
        for (int i = 0; i < samples; ++i)
            keep.push_back(static_cast<size_t>(std::llround(static_cast<double>(i) * (times.size() - 1) / (samples - 1))));
        const auto thin = [&](json& arr) {
            if (!arr.is_array() || arr.size() != times.size())
                return;
            json out = json::array();
            for (size_t k : keep)
                out.push_back(arr[k]);
            arr = out;
        };
        thin(l["times"]);
        for (const char* section : {"exchanges", "holdings", "generations", "forces"})
            for (json& x : l[section])
                for (const char* key : {"history", "vectorHistory"})
                    if (x.contains(key))
                        thin(x[key]);
    }
    return {{"format", "resonant.accounting-record"}, {"version", 1}, {"id", id}, {"name", name}, {"created", result.value("created", std::string())},
            {"title", result.value("title", std::string())}, {"study", result.value("study", json::object())}, {"status", result.value("status", std::string())},
            {"tolerances", result.value("tolerances", json::object())}, {"systems", result.value("systems", json::array())}, {"ledger", l},
            {"convergence", result.value("convergence", json::array())}, {"retainedSamples", l.value("times", json::array()).size()}};
}

Result<json> accountingRecordReports(const json& record)
{
    using R = Result<json>;
    auto l = acc::Ledger::fromJson(record.value("ledger", json::object()));
    if (!l)
        return R::failure(l.message());
    const acc::Tolerances t = acc::Tolerances::fromJson(record.value("tolerances", json::object()));
    json out = json::array();
    for (const json& s : record.value("systems", json::array())) {
        auto sys = acc::AccountingSystem::fromJson(s);
        if (!sys)
            return R::failure(sys.message());
        out.push_back(acc::evaluate(l.value(), sys.value(), t).toJson());
    }
    return out;
}

}  // namespace resonant::coupling
