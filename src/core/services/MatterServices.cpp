// MatterService (REQ-CHAT-20261127-001, RES-FAM-019, ADR-000228).
#include "services/Services.h"

#include "coupling/MatterAnalyses.h"
#include "coupling/MatterExploration.h"
#include "coupling/MatterSystems.h"
#include "foundation/Time.h"
#include "matter/Manufacturing.h"
#include "matter/MatterArchitecture.h"
#include "matter/Mixing.h"
#include "matter/PropertyField.h"
#include "project/ProjectSerializer.h"

#include <cmath>
#include <sstream>

namespace resonant::services {

using nlohmann::json;
namespace fam = matter;

namespace {

Outcome<fam::Workspace> workspace(const Project& p)
{
    auto w = fam::workspaceOf(p);
    if (!w)
        return ServiceError::make(ErrorCode::FailedPrecondition, "the project's architectures cannot be read: " + w.message());
    return w.value();
}

Outcome<fam::Architecture> find(const Project& p, const std::string& id)
{
    auto w = workspace(p);
    if (!w)
        return w.error();
    const fam::Architecture* a = w.value().find(id);
    if (!a)
        return notFound("the architecture '" + id + "'", "matter-architecture", id);
    return *a;
}

json issuesJson(const std::vector<fam::Issue>& issues)
{
    json out = json::array();
    for (const auto& i : issues)
        out.push_back(i.toJson());
    return out;
}

std::vector<fam::ResidualStress> residualOf(const fam::Architecture& a, const MaterialLibrary& lib)
{
    std::vector<fam::ResidualStress> out;
    for (const json& r : coupling::processResidualStresses(a, lib))
        if (r.contains("stress"))
            out.push_back({r["interface"].get<std::string>(), r["layer"].get<std::string>(), r["stress"].get<double>(), r["temperature"].get<double>(),
                           r["basis"].get<std::string>()});
    return out;
}

std::string number(double v)
{
    std::ostringstream s;
    s.precision(4);
    s << v;
    return s.str();
}

json storedSystems(const Project& p)
{
    if (!p.extensions.is_object() || !p.extensions.contains("multiphysics") || !p.extensions["multiphysics"].is_object())
        return json::array();
    return p.extensions["multiphysics"].value("systems", json::array());
}

// The analyses a stack can run, with default options, for reports and comparisons.
json applicableMetrics(const fam::Architecture& a, const fam::Stack& s, const MaterialLibrary& lib)
{
    json out = json::object();
    for (const char* analysis : {"laminate", "modes", "thermal", "harvester", "electromagnetic"}) {
        auto r = coupling::analyzeStack(a, s.id, analysis, json::object(), lib);
        if (r)
            out[analysis] = r.value()["metrics"];
    }
    return out;
}

std::string escape(const std::string& text)
{
    std::string out;
    for (char ch : text) {
        if (ch == '<')
            out += "&lt;";
        else if (ch == '>')
            out += "&gt;";
        else if (ch == '&')
            out += "&amp;";
        else
            out += ch;
    }
    return out;
}

}  // namespace

Outcome<Mutation> MatterService::change(const CallContext& c, const std::string& operation, const std::string& description,
                                        const std::function<Outcome<bool>(Project&)>& apply)
{
    if (auto a = s_.authorize(c, Permission::Modify); !a)
        return a.error();
    auto copy = deserializeProject(serializeProject(s_.project()));
    if (!copy)
        return ServiceError::make(ErrorCode::Internal, "the project could not be copied: " + copy.message());
    Project& changed = *copy.value().project;
    auto ok = apply(changed);
    if (!ok)
        return ok.error();
    return s_.applyCopy(c, operation, changed, description);
}

Outcome<json> MatterService::architectures(const CallContext& c)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    auto w = workspace(s_.project());
    if (!w)
        return w.error();
    json out = json::array();
    for (const auto& a : w.value().architectures) {
        const auto issues = fam::validate(a, &s_.project());
        int errors = 0, warnings = 0;
        for (const auto& i : issues) {
            if (i.severity == "error")
                ++errors;
            else if (i.severity == "warning")
                ++warnings;
        }
        json stacks = json::array();
        for (const auto& s : a.stacks)
            stacks.push_back({{"id", s.id}, {"name", s.name}, {"layers", s.layers.size()}, {"thickness", s.thickness()}});
        out.push_back({{"id", a.id},
                       {"name", a.name},
                       {"description", a.description},
                       {"representation", a.representation},
                       {"parent", a.parent},
                       {"revision", fam::revision(a)},
                       {"objects", a.objects},
                       {"regions", a.regions.size()},
                       {"stacks", stacks},
                       {"interfaces", a.interfaces.size()},
                       {"functions", a.functions.size()},
                       {"errors", errors},
                       {"warnings", warnings}});
    }
    return out;
}

Outcome<json> MatterService::architecture(const CallContext& c, const std::string& id)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    auto a = find(s_.project(), id);
    if (!a)
        return a.error();
    json out = a.value().toJson();
    out["revision"] = fam::revision(a.value());
    out["issues"] = issuesJson(fam::validate(a.value(), &s_.project()));
    return out;
}

Outcome<MatterChange> MatterService::setArchitecture(const CallContext& c, const json& doc)
{
    auto parsed = fam::Architecture::fromJson(doc);
    if (!parsed)
        return invalid(parsed.message(), "architecture");
    const fam::Architecture a = parsed.value();
    const auto issues = fam::validate(a, &s_.project());
    for (const auto& i : issues)
        if (i.severity == "error")
            return invalid("the architecture is not valid: " + i.message, i.element);
    auto m = change(c, "MatterService.SetArchitecture", "Set architecture " + (a.name.empty() ? a.id : a.name), [a](Project& p) -> Outcome<bool> {
        auto w = workspace(p);
        if (!w)
            return w.error();
        fam::Workspace ws = w.value();
        if (fam::Architecture* existing = ws.find(a.id))
            *existing = a;
        else
            ws.architectures.push_back(a);
        fam::storeWorkspace(p, ws);
        return true;
    });
    if (!m)
        return m.error();
    return MatterChange{m.value(), {{"id", a.id}, {"revision", fam::revision(a)}, {"issues", issuesJson(issues)}}};
}

Outcome<Mutation> MatterService::deleteArchitecture(const CallContext& c, const std::string& id)
{
    if (auto a = find(s_.project(), id); !a)
        return a.error();
    return change(c, "MatterService.DeleteArchitecture", "Delete architecture " + id, [id](Project& p) -> Outcome<bool> {
        auto w = workspace(p);
        if (!w)
            return w.error();
        fam::Workspace ws = w.value();
        std::erase_if(ws.architectures, [&](const fam::Architecture& a) { return a.id == id; });
        fam::storeWorkspace(p, ws);
        return true;
    });
}

Outcome<MatterChange> MatterService::duplicateArchitecture(const CallContext& c, const std::string& id, const std::string& newId, const std::string& name)
{
    auto a = find(s_.project(), id);
    if (!a)
        return a.error();
    if (newId.empty())
        return invalid("the alternative needs an id", "newId");
    if (auto w = workspace(s_.project()); w && w.value().find(newId))
        return ServiceError::make(ErrorCode::AlreadyExists, "the architecture '" + newId + "' exists already");
    fam::Architecture copy = a.value();
    copy.id = newId;
    copy.name = name.empty() ? a.value().name + " (alternative)" : name;
    copy.parent = id;
    copy.records.clear();  // manufacturing history belongs to what was made, not to an alternative
    if (copy.representation == "as-built")
        copy.representation = "as-designed";
    auto m = change(c, "MatterService.DuplicateArchitecture", "Duplicate architecture " + id + " as " + newId, [copy](Project& p) -> Outcome<bool> {
        auto w = workspace(p);
        if (!w)
            return w.error();
        fam::Workspace ws = w.value();
        ws.architectures.push_back(copy);
        fam::storeWorkspace(p, ws);
        return true;
    });
    if (!m)
        return m.error();
    return MatterChange{m.value(), {{"id", newId}, {"parent", id}, {"revision", fam::revision(copy)}}};
}

Outcome<json> MatterService::validate(const CallContext& c, const json& spec)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    fam::Architecture a;
    if (spec.contains("architecture") && spec["architecture"].is_string()) {
        auto found = find(s_.project(), spec["architecture"].get<std::string>());
        if (!found)
            return found.error();
        a = found.value();
    } else {
        auto parsed = fam::Architecture::fromJson(spec);
        if (!parsed)
            return json{{"valid", false}, {"issues", json::array({{{"severity", "error"}, {"code", "unreadable"}, {"element", ""}, {"message", parsed.message()}}})}};
        a = parsed.value();
    }
    const auto issues = fam::validate(a, &s_.project());
    return json{{"valid", !fam::hasErrors(issues)}, {"issues", issuesJson(issues)}, {"revision", fam::revision(a)}};
}

Outcome<json> MatterService::capabilities(const CallContext& c)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    json solvers = json::array(), interfaceModels = json::array(), processes = json::array(), mechanisms = json::array(), mixing = json::object();
    for (const auto& cap : coupling::matterCapabilities()) {
        json j = cap.toJson();
        json metrics = json::array();
        for (const auto& m : coupling::matterMetrics(cap.id))
            metrics.push_back({{"key", m.key}, {"label", m.label}, {"unit", m.unit}, {"definition", m.definition}});
        j["metrics"] = metrics;
        solvers.push_back(j);
    }
    for (const auto& k : fam::interfaceModelKinds()) {
        json params = json::array();
        for (const auto& [key, unit] : k.parameters)
            params.push_back({{"key", key}, {"unit", unit}});
        interfaceModels.push_back({{"id", k.id}, {"title", k.title}, {"physics", k.physics}, {"parameters", params}, {"description", k.description}});
    }
    for (const auto& k : fam::processKinds())
        processes.push_back({{"id", k.id}, {"title", k.title}, {"category", k.category}, {"constraints", k.constraints}, {"description", k.description}});
    for (const auto& m : coupling::mechanisms())
        mechanisms.push_back(m.toJson());
    for (auto kind : {fam::PropertyKind::Density, fam::PropertyKind::SpecificHeat, fam::PropertyKind::Transport, fam::PropertyKind::Elastic, fam::PropertyKind::Expansion})
        mixing[std::string(fam::toString(kind))] = fam::methodsFor(kind);
    return json{{"analyses", solvers},
                {"interfaceModels", interfaceModels},
                {"processes", processes},
                {"mechanisms", mechanisms},
                {"mixingMethods", mixing},
                {"fieldKinds", fam::fieldKinds()},
                {"anisotropicKinds", fam::anisotropicKinds()},
                {"provenance", fam::provenanceKinds()}};
}

Outcome<json> MatterService::mechanisms(const CallContext& c, const std::string& from, const std::string& to, const std::string& architectureId)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    std::optional<fam::Architecture> a;
    if (!architectureId.empty()) {
        auto found = find(s_.project(), architectureId);
        if (!found)
            return found.error();
        a = found.value();
    }
    return coupling::mechanismsFor(from, to, a ? &*a : nullptr, s_.project());
}

Outcome<json> MatterService::check(const CallContext& c, const std::string& architectureId, const std::string& stackId, const std::string& analysis)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    auto a = find(s_.project(), architectureId);
    if (!a)
        return a.error();
    auto r = coupling::checkMatterStack(a.value(), stackId, analysis, s_.project().materials);
    if (!r)
        return invalid(r.message(), "analysis");
    return r.value().toJson();
}

Outcome<json> MatterService::analyze(const CallContext& c, const std::string& architectureId, const std::string& stackId, const std::string& analysis,
                                     const json& options)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    auto a = find(s_.project(), architectureId);
    if (!a)
        return a.error();
    auto r = coupling::analyzeStack(a.value(), stackId, analysis, options, s_.project().materials);
    if (!r)
        return ServiceError::make(ErrorCode::FailedPrecondition, r.message());
    return r.value();
}

Outcome<json> MatterService::effectiveProperties(const CallContext& c, const std::string& architectureId, const std::string& regionId)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    auto a = find(s_.project(), architectureId);
    if (!a)
        return a.error();
    json r = coupling::effectiveRegionProperties(a.value(), regionId, s_.project().materials);
    if (r.is_null())
        return notFound("the region '" + regionId + "'", "matter-region", regionId);
    return r;
}

Outcome<json> MatterService::manufacturability(const CallContext& c, const std::string& architectureId)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    auto a = find(s_.project(), architectureId);
    if (!a)
        return a.error();
    json residual = json::array();
    for (const json& r : coupling::processResidualStresses(a.value(), s_.project().materials))
        residual.push_back(r);
    json out = fam::checkManufacturing(a.value(), residualOf(a.value(), s_.project().materials)).toJson();
    out["residualStresses"] = residual;
    return out;
}

Outcome<json> MatterService::explore(const CallContext& c, const std::string& architectureId)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    auto a = find(s_.project(), architectureId);
    if (!a)
        return a.error();
    return coupling::explore(a.value(), s_.project());
}

Outcome<json> MatterService::pathway(const CallContext& c, const std::string& architectureId, const std::string& functionId, const json& options)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    auto a = find(s_.project(), architectureId);
    if (!a)
        return a.error();
    auto r = coupling::evaluatePathway(a.value(), functionId, options, s_.project().materials);
    if (!r)
        return invalid(r.message(), "function");
    return r.value();
}

Outcome<MatterChange> MatterService::createPhysicalSystem(const CallContext& c, const std::string& architectureId, const std::string& stackId,
                                                          const json& options)
{
    auto a = find(s_.project(), architectureId);
    if (!a)
        return a.error();
    auto sys = coupling::stackToPhysicalSystem(a.value(), stackId, options, s_.project().materials);
    if (!sys)
        return ServiceError::make(ErrorCode::FailedPrecondition, sys.message());
    const json doc = sys.value().system.toJson();
    const std::string id = sys.value().system.id;
    const bool replace = options.value("replace", false);
    for (const json& existing : storedSystems(s_.project()))
        if (existing.value("id", std::string()) == id && !replace)
            return ServiceError::make(ErrorCode::AlreadyExists, "the physical system '" + id + "' exists already (pass replace to overwrite it)");
    auto m = change(c, "MatterService.CreatePhysicalSystem", "Create physical system " + id + " from " + architectureId, [doc, id](Project& p) -> Outcome<bool> {
        json systems = json::array();
        for (const json& existing : storedSystems(p))
            if (existing.value("id", std::string()) != id)
                systems.push_back(existing);
        systems.push_back(doc);
        p.extensions["multiphysics"] = {{"format", "resonant.multiphysics"}, {"version", 1}, {"systems", systems}};
        return true;
    });
    if (!m)
        return m.error();
    return MatterChange{m.value(), {{"system", doc}, {"notes", sys.value().notes}}};
}

Outcome<MatterChange> MatterService::compare(const CallContext& c, const json& spec)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    const json ids = spec.value("architectures", json::array());
    if (!ids.is_array() || ids.size() < 2)
        return invalid("a comparison needs at least two architectures", "architectures");
    json analyses = spec.value("analyses", json::array());
    if (analyses.empty())
        analyses = json::array({{{"analysis", "laminate"}}, {{"analysis", "modes"}}, {{"analysis", "thermal"}}, {{"analysis", "harvester"}}});
    json columns = json::array(), table = json::object();
    for (const json& id : ids) {
        auto a = find(s_.project(), id.get<std::string>());
        if (!a)
            return a.error();
        const std::string stack = spec.value("stack", a.value().stacks.empty() ? std::string() : a.value().stacks.front().id);
        json column{{"architecture", a.value().id}, {"name", a.value().name}, {"revision", fam::revision(a.value())}, {"representation", a.value().representation}};
        json refusals = json::object();
        for (const json& item : analyses) {
            const std::string analysis = item.value("analysis", std::string());
            auto r = coupling::analyzeStack(a.value(), stack, analysis, item.value("options", json::object()), s_.project().materials);
            if (!r) {
                refusals[analysis] = r.message();
                continue;
            }
            for (const auto& [k, v] : r.value()["metrics"].items())
                table[analysis + "." + k][a.value().id] = v;
        }
        const auto mc = fam::checkManufacturing(a.value(), residualOf(a.value(), s_.project().materials));
        table["manufacturing.status"][a.value().id] = mc.status;
        table["manufacturing.violations"][a.value().id] = mc.violations.size();
        column["refusals"] = refusals;
        columns.push_back(column);
    }
    json result{{"id", spec.value("id", "cmp-" + std::to_string(std::hash<std::string>{}(ids.dump()) % 100000))},
                {"created", utcNowIso8601()},
                {"architectures", columns},
                {"metrics", table},
                {"note", "values side by side: no single score is formed, and a better prediction is not a validated design"}};
    MatterChange out{Mutation{}, result};
    if (spec.value("keep", false)) {
        auto m = change(c, "MatterService.CompareArchitectures", "Keep a comparison of architectures", [result](Project& p) -> Outcome<bool> {
            auto w = workspace(p);
            if (!w)
                return w.error();
            fam::Workspace ws = w.value();
            ws.comparisons.push_back(result);
            fam::storeWorkspace(p, ws);
            return true;
        });
        if (!m)
            return m.error();
        out.mutation = m.value();
        out.result["kept"] = true;
    }
    return out;
}

Outcome<MatterChange> MatterService::addRecord(const CallContext& c, const std::string& architectureId, const json& record)
{
    auto a = find(s_.project(), architectureId);
    if (!a)
        return a.error();
    json withId = record;
    if (withId.value("id", std::string()).empty())
        withId["id"] = "rec-" + std::to_string(a.value().records.size() + 1);
    if (withId.value("date", std::string()).empty())
        withId["date"] = utcNowIso8601().substr(0, 10);
    json doc = a.value().toJson();
    for (const json& r : doc["records"])
        if (r.value("id", std::string()) == withId["id"].get<std::string>())
            return ServiceError::make(ErrorCode::AlreadyExists, "the record '" + withId["id"].get<std::string>() + "' exists already");
    doc["records"].push_back(withId);
    auto parsed = fam::Architecture::fromJson(doc);
    if (!parsed)
        return invalid(parsed.message(), "record");
    const fam::Architecture updated = parsed.value();
    for (const auto& i : fam::validate(updated, &s_.project()))
        if (i.severity == "error" && i.element == withId["id"].get<std::string>())
            return invalid(i.message, "record");
    auto m = change(c, "MatterService.AddManufacturingRecord", "Add manufacturing record " + withId["id"].get<std::string>(), [updated](Project& p) -> Outcome<bool> {
        auto w = workspace(p);
        if (!w)
            return w.error();
        fam::Workspace ws = w.value();
        *ws.find(updated.id) = updated;
        fam::storeWorkspace(p, ws);
        return true;
    });
    if (!m)
        return m.error();
    return MatterChange{m.value(), withId};
}

Outcome<std::string> MatterService::report(const CallContext& c, const std::string& architectureId, const std::string& format)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    auto found = find(s_.project(), architectureId);
    if (!found)
        return found.error();
    const fam::Architecture& a = found.value();
    const MaterialLibrary& lib = s_.project().materials;
    struct Section {
        std::string heading;
        std::vector<std::string> lines;
    };
    std::vector<Section> sections;
    sections.push_back({"Architecture", {a.description, "Representation: " + a.representation + " (revision " + fam::revision(a) + ")",
                                         a.parent.empty() ? std::string() : "An alternative of '" + a.parent + "'."}});
    Section regions{"Regions", {}};
    for (const auto& r : a.regions) {
        std::string source = !r.materialId.empty() ? r.materialId : r.composition.empty() ? "no material" : "a composition (" + r.composition.method + ")";
        regions.lines.push_back("- " + r.id + " (" + r.role + ", " + r.extent + " of " + r.object + "): " + source +
                                (r.process.empty() ? "" : "; made by " + r.process) + (r.anisotropic.empty() ? "" : "; anisotropic sets stated"));
    }
    sections.push_back(regions);
    for (const auto& s : a.stacks) {
        Section st{"Stack " + s.id, {}};
        for (const auto& l : s.layers)
            st.lines.push_back("- " + l.id + ": " + l.region + ", " + number(l.thickness * 1e6) + " um" + (l.angle != 0.0 ? ", at " + number(l.angle) + " rad" : "") +
                               (l.pattern.kind == "uniform" ? "" : ", " + l.pattern.kind + " (" + number(100.0 * l.pattern.coverage()) + " %)"));
        const json metrics = applicableMetrics(a, s, lib);
        for (const auto& [analysis, m] : metrics.items()) {
            std::string line = "- " + analysis + ":";
            for (const auto& [k, v] : m.items())
                if (v.is_number())
                    line += " " + k + " = " + number(v.get<double>()) + ";";
            st.lines.push_back(line);
        }
        sections.push_back(st);
    }
    Section interfaces{"Interfaces", {}};
    for (const auto& i : a.interfaces) {
        std::string models;
        for (const auto& m : i.models) {
            models += (models.empty() ? "" : ", ") + m.kind;
            for (const auto& [k, v] : m.parameters)
                models += " " + k + " = " + number(v.value) + " " + v.unit + " (" + v.provenance + ")";
        }
        interfaces.lines.push_back("- " + i.id + " between " + i.regions[0] + " and " + i.regions[1] + ": " + (models.empty() ? "no model" : models));
    }
    sections.push_back(interfaces);
    const auto mc = fam::checkManufacturing(a, residualOf(a, lib));
    Section manufacturing{"Manufacturing: " + mc.status, {}};
    for (const auto& v : mc.violations)
        manufacturing.lines.push_back("- violation: " + v.message);
    for (const auto& u : mc.unassessed)
        manufacturing.lines.push_back("- not assessed: " + u.message);
    sections.push_back(manufacturing);
    const json ex = coupling::explore(a, s_.project());
    Section findings{"Findings (from the records and computations)", {}};
    for (const json& f : ex["findings"])
        findings.lines.push_back("- " + f["statement"].get<std::string>() + " [" + f["basis"].get<std::string>() + "]");
    sections.push_back(findings);
    Section hypotheses{"Hypotheses (to test, not results)", {}};
    for (const json& h : ex["hypotheses"])
        hypotheses.lines.push_back("- " + h["statement"].get<std::string>());
    sections.push_back(hypotheses);
    sections.push_back({"Limitations", {"Every value here is a prediction of the architecture's analyses: an optimized or explored architecture is not a validated one.",
                                        ex["disclaimer"].get<std::string>()}});
    std::ostringstream out;
    if (format == "html") {
        out << "<!doctype html><html><head><meta charset=\"utf-8\"><title>" << escape(a.name) << "</title></head><body><h1>" << escape(a.name) << "</h1>\n";
        for (const auto& s : sections) {
            out << "<h2>" << escape(s.heading) << "</h2>\n<ul>\n";
            for (const auto& l : s.lines)
                if (!l.empty())
                    out << "<li>" << escape(l.rfind("- ", 0) == 0 ? l.substr(2) : l) << "</li>\n";
            out << "</ul>\n";
        }
        out << "</body></html>\n";
    } else if (format == "markdown") {
        out << "# " << a.name << "\n";
        for (const auto& s : sections) {
            out << "\n## " << s.heading << "\n\n";
            for (const auto& l : s.lines)
                if (!l.empty())
                    out << l << "\n";
        }
    } else {
        return invalid("the format is markdown or html", "format");
    }
    return out.str();
}

Outcome<std::string> MatterService::exportLayers(const CallContext& c, const std::string& architectureId, const std::string& stackId, const std::string& format)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    auto found = find(s_.project(), architectureId);
    if (!found)
        return found.error();
    const fam::Architecture& a = found.value();
    const fam::Stack* s = a.stack(stackId);
    if (!s)
        return notFound("the stack '" + stackId + "'", "matter-stack", stackId);
    json rows = json::array();
    for (size_t i = 0; i < s->layers.size(); ++i) {
        const fam::Layer& l = s->layers[i];
        const fam::FunctionalRegion* r = a.region(l.region);
        const fam::Interface* above = i + 1 < s->layers.size() ? a.interfaceBetween(l.region, s->layers[i + 1].region) : nullptr;
        rows.push_back({{"layer", l.id},
                        {"region", l.region},
                        {"role", r ? r->role : std::string()},
                        {"material", r ? r->materialId : std::string()},
                        {"thickness_m", l.thickness},
                        {"angle_rad", l.angle},
                        {"pattern", l.pattern.kind},
                        {"coverage", l.pattern.coverage()},
                        {"process", r ? r->process : std::string()},
                        {"interface_above", above ? above->id : std::string()}});
    }
    if (format == "json")
        return json{{"architecture", a.id}, {"revision", fam::revision(a)}, {"stack", s->id}, {"axis", s->axis}, {"layers", rows}}.dump(2);
    if (format != "csv")
        return invalid("the format is csv or json", "format");
    std::ostringstream out;
    out << "# architecture: " << a.id << " (revision " << fam::revision(a) << "), stack " << s->id << ", from the substrate outwards along " << s->axis << "\n";
    out << "layer,region,role,material,thickness_m,angle_rad,pattern,coverage,process,interface_above\n";
    for (const json& r : rows)
        out << r["layer"].get<std::string>() << ',' << r["region"].get<std::string>() << ',' << r["role"].get<std::string>() << ','
            << r["material"].get<std::string>() << ',' << r["thickness_m"].get<double>() << ',' << r["angle_rad"].get<double>() << ','
            << r["pattern"].get<std::string>() << ',' << r["coverage"].get<double>() << ',' << r["process"].get<std::string>() << ','
            << r["interface_above"].get<std::string>() << '\n';
    return out.str();
}

}  // namespace resonant::services
