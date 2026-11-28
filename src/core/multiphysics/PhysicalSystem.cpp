#include "multiphysics/PhysicalSystem.h"

#include "multiphysics/Quantity.h"
#include "multiphysics/Transfer.h"

#include <algorithm>
#include <cstdio>
#include <set>

namespace resonant::multiphysics {
namespace {

template <typename T>
bool uniqueIds(const std::vector<T>& items)
{
    std::set<std::string> seen;
    for (const T& i : items)
        if (i.id.empty() || !seen.insert(i.id).second)
            return false;
    return true;
}

}  // namespace

const RegionDef* PhysicalSystem::region(const std::string& rid) const
{
    const auto it = std::find_if(regions.begin(), regions.end(), [&](const RegionDef& r) { return r.id == rid; });
    return it == regions.end() ? nullptr : &*it;
}

const DomainDef* PhysicalSystem::domain(const std::string& did) const
{
    const auto it = std::find_if(domains.begin(), domains.end(), [&](const DomainDef& d) { return d.id == did; });
    return it == domains.end() ? nullptr : &*it;
}

double PhysicalSystem::totalThickness() const
{
    double d = 0.0;
    for (const RegionDef& r : regions)
        d += r.thickness;
    return d;
}

nlohmann::json PhysicalSystem::toJson() const
{
    nlohmann::json j;
    j["format"] = "resonant.physical-system";
    j["version"] = 1;
    j["id"] = id;
    j["name"] = name;
    j["description"] = description;
    j["geometry"] = {{"kind", geometryKind}, {"lateralArea", lateralArea}};
    j["environment"] = {{"ambientTemperature", ambientTemperature}, {"incidenceMaterial", incidenceMaterial}, {"exitMaterial", exitMaterial}};
    j["regions"] = nlohmann::json::array();
    for (const RegionDef& r : regions) {
        nlohmann::json rj{{"id", r.id}, {"name", r.name}, {"materialId", r.materialId}, {"thickness", r.thickness}, {"cells", r.cells}};
        if (!r.objectId.empty())
            rj["objectId"] = r.objectId;
        j["regions"].push_back(rj);
    }
    j["domains"] = nlohmann::json::array();
    for (const DomainDef& d : domains)
        j["domains"].push_back({{"id", d.id}, {"kind", d.kind}, {"solver", d.solver}, {"settings", d.settings}});
    j["couplings"] = nlohmann::json::array();
    for (const CouplingDef& c : couplings) {
        nlohmann::json cj{{"id", c.id}, {"model", c.model}, {"from", c.from}, {"to", c.to}};
        if (!c.transfer.empty())
            cj["transfer"] = c.transfer;
        j["couplings"].push_back(cj);
    }
    j["materialLaws"] = nlohmann::json::array();
    for (const PropertyLaw& l : laws)
        j["materialLaws"].push_back(l.toJson());
    j["scheme"] = {{"kind", scheme.kind},
                   {"passes", scheme.passes},
                   {"maxIterations", scheme.maxIterations},
                   {"temperatureTolerance", scheme.temperatureTolerance},
                   {"geometryTolerance", scheme.geometryTolerance},
                   {"relaxation", scheme.relaxation},
                   {"balanceTolerance", scheme.balanceTolerance}};
    j["observations"] = nlohmann::json::array();
    for (const ObservationDef& o : observations)
        j["observations"].push_back({{"id", o.id}, {"point", {o.point.x(), o.point.y(), o.point.z()}}});
    return j;
}

Result<PhysicalSystem> PhysicalSystem::fromJson(const nlohmann::json& j)
{
    using R = Result<PhysicalSystem>;
    if (!j.is_object() || j.value("format", "") != "resonant.physical-system")
        return R::failure("not a physical system (format must be \"resonant.physical-system\")");
    if (j.value("version", 0) != 1)
        return R::failure("unsupported physical-system version " + std::to_string(j.value("version", 0)) + " (this build reads version 1)");
    try {
        PhysicalSystem s;
        s.id = j.value("id", "");
        s.name = j.value("name", "");
        s.description = j.value("description", "");
        if (j.contains("geometry")) {
            s.geometryKind = j["geometry"].value("kind", "layered-slab");
            s.lateralArea = j["geometry"].value("lateralArea", 1e-2);
        }
        if (j.contains("environment")) {
            s.ambientTemperature = j["environment"].value("ambientTemperature", 293.15);
            s.incidenceMaterial = j["environment"].value("incidenceMaterial", "");
            s.exitMaterial = j["environment"].value("exitMaterial", "");
        }
        for (const auto& r : j.value("regions", nlohmann::json::array()))
            s.regions.push_back({r.value("id", ""), r.value("name", ""), r.value("materialId", ""), r.value("thickness", 0.0), r.value("cells", 20),
                                 r.value("objectId", "")});
        for (const auto& d : j.value("domains", nlohmann::json::array()))
            s.domains.push_back({d.value("id", ""), d.value("kind", ""), d.value("solver", ""), d.value("settings", nlohmann::json::object())});
        for (const auto& c : j.value("couplings", nlohmann::json::array()))
            s.couplings.push_back({c.value("id", ""), c.value("model", ""), c.value("from", ""), c.value("to", ""), c.value("transfer", "")});
        for (const auto& l : j.value("materialLaws", nlohmann::json::array())) {
            auto law = PropertyLaw::fromJson(l);
            if (!law)
                return R::failure(law.message());
            s.laws.push_back(law.take());
        }
        if (j.contains("scheme")) {
            const auto& sc = j["scheme"];
            s.scheme.kind = sc.value("kind", "one-way");
            s.scheme.passes = sc.value("passes", 2);
            s.scheme.maxIterations = sc.value("maxIterations", 30);
            s.scheme.temperatureTolerance = sc.value("temperatureTolerance", 1e-3);
            s.scheme.geometryTolerance = sc.value("geometryTolerance", 1e-9);
            s.scheme.relaxation = sc.value("relaxation", 1.0);
            s.scheme.balanceTolerance = sc.value("balanceTolerance", 1e-6);
        }
        for (const auto& o : j.value("observations", nlohmann::json::array())) {
            ObservationDef d;
            d.id = o.value("id", "");
            if (o.contains("point") && o["point"].is_array() && o["point"].size() == 3)
                d.point = Vec3(o["point"][0].get<double>(), o["point"][1].get<double>(), o["point"][2].get<double>());
            s.observations.push_back(d);
        }
        return s;
    } catch (const nlohmann::json::exception& e) {
        return R::failure(std::string("the physical system is malformed: ") + e.what());
    }
}

std::string PhysicalSystem::checksum() const
{
    const std::string text = toJson().dump();
    std::uint64_t h = 1469598103934665603ull;
    for (unsigned char c : text) {
        h ^= c;
        h *= 1099511628211ull;
    }
    char buf[17];
    std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(h));
    return buf;
}

std::vector<SystemIssue> validateSystem(const PhysicalSystem& s, const std::vector<CouplingModelDescriptor>& models,
                                        const std::function<bool(const std::string&)>& materialExists,
                                        const std::vector<std::pair<std::string, std::string>>& solvers)
{
    std::vector<SystemIssue> out;
    const auto error = [&out](std::string field, std::string message) { out.push_back({3, std::move(field), std::move(message)}); };
    const auto warn = [&out](std::string field, std::string message) { out.push_back({2, std::move(field), std::move(message)}); };
    if (s.id.empty())
        error("id", "the system needs an id");
    if (s.geometryKind != "layered-slab")
        error("geometry.kind", "unsupported geometry kind '" + s.geometryKind + "' (this build supports \"layered-slab\")");
    if (!(s.lateralArea > 0.0))
        error("geometry.lateralArea", "the lateral area must be positive");
    if (!(s.ambientTemperature > 0.0))
        error("environment.ambientTemperature", "the ambient temperature must be positive (kelvin)");
    for (const std::string* m : {&s.incidenceMaterial, &s.exitMaterial})
        if (!m->empty() && !materialExists(*m))
            error("environment", "the material '" + *m + "' is not in the project's library");
    if (s.regions.empty())
        error("regions", "the system needs at least one region");
    if (!uniqueIds(s.regions))
        error("regions", "region ids must be present and unique");
    for (const RegionDef& r : s.regions) {
        const std::string f = "regions." + r.id;
        if (!(r.thickness > 0.0))
            error(f + ".thickness", "the region needs a positive thickness");
        if (r.cells < 1 || r.cells > 20000)
            error(f + ".cells", "cells must be between 1 and 20000");
        if (r.materialId.empty() || !materialExists(r.materialId))
            error(f + ".materialId", "the material '" + r.materialId + "' is not in the project's library");
    }
    if (!uniqueIds(s.domains))
        error("domains", "domain ids must be present and unique");
    for (const DomainDef& d : s.domains) {
        const bool known = std::any_of(solvers.begin(), solvers.end(), [&d](const auto& p) { return p.first == d.kind && p.second == d.solver; });
        if (!known)
            error("domains." + d.id, "no participant for domain kind '" + d.kind + "' with solver '" + d.solver + "'");
    }
    if (!uniqueIds(s.couplings))
        error("couplings", "coupling ids must be present and unique");
    bool feedback = false;
    for (const CouplingDef& c : s.couplings) {
        const std::string f = "couplings." + c.id;
        const auto m = std::find_if(models.begin(), models.end(), [&c](const CouplingModelDescriptor& d) { return d.id == c.model; });
        if (m == models.end()) {
            error(f + ".model", "unknown coupling model '" + c.model + "'");
            continue;
        }
        if (m->status == "planned")
            error(f + ".model", "the coupling model '" + c.model + "' is planned, not implemented: " + m->limitations);
        else if (m->status != "validated" && m->status != "experimental")
            error(f + ".model", "the coupling model '" + c.model + "' is not available to the engine (" + m->status + "): " + m->limitations);
        else if (m->status == "experimental")
            warn(f + ".model", "the coupling model '" + c.model + "' is experimental: " + m->limitations);
        const DomainDef* from = s.domain(c.from);
        const DomainDef* to = s.domain(c.to);
        if (!from || !to) {
            error(f, "the coupling refers to a domain that does not exist");
            continue;
        }
        if (from->kind != m->sourceDomain || to->kind != m->targetDomain)
            error(f, "the model '" + m->id + "' couples " + m->sourceDomain + " to " + m->targetDomain + ", not " + from->kind + " to " + to->kind);
        if (!c.transfer.empty()) {
            const auto method = transferMethodFromString(c.transfer);
            if (!method)
                error(f + ".transfer", "unknown transfer method '" + c.transfer + "'");
            else if (*method == TransferMethod::Conservative) {
                const QuantityDescriptor* q = findQuantity(m->sourceQuantity);
                if (q && !q->conservative)
                    error(f + ".transfer", m->sourceQuantity + " is not a conserved density: use interpolating");
            }
        }
        if (m->kind != "field-transfer")
            feedback = true;
    }
    if (s.scheme.kind != "one-way" && s.scheme.kind != "sequential" && s.scheme.kind != "iterative")
        error("scheme.kind", "the scheme must be one-way, sequential or iterative");
    if (s.scheme.kind == "one-way" && feedback)
        error("scheme.kind", "a one-way scheme cannot use state or geometry updates (feedback): use sequential or iterative");
    if (s.scheme.kind == "iterative" && (s.scheme.maxIterations < 2 || !(s.scheme.temperatureTolerance > 0.0) || !(s.scheme.geometryTolerance > 0.0)))
        error("scheme", "an iterative scheme needs at least 2 iterations and positive tolerances");
    if (s.scheme.kind == "sequential" && s.scheme.passes < 1)
        error("scheme.passes", "a sequential scheme needs at least one pass");
    if (!(s.scheme.relaxation > 0.0 && s.scheme.relaxation <= 1.0))
        error("scheme.relaxation", "the relaxation factor must be in (0, 1]");
    for (const PropertyLaw& l : s.laws)
        if (!s.region(l.region))
            error("materialLaws", "a law refers to the region '" + l.region + "', which does not exist");
    return out;
}

}  // namespace resonant::multiphysics
