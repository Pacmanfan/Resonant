#include "coupling/MultiscaleCoupling.h"

#include "coupling/CoupledRun.h"
#include "coupling/MultiscaleDerivations.h"
#include "project/Project.h"
#include "simulation/Job.h"

#include <algorithm>
#include <cmath>

namespace resonant::coupling {

using nlohmann::json;
namespace ms = multiscale;

json LocalState::toJson() const
{
    return {{"region", region},          {"meanTemperatureK", meanTemperature}, {"minTemperatureK", minTemperature},
            {"maxTemperatureK", maxTemperature}, {"meanGradientKPerM", meanGradient}, {"zFrom", zFrom}, {"zTo", zTo}, {"cells", cells}};
}

Result<LocalState> regionState(const multiphysics::PhysicalSystem& system, const multiphysics::CoupledResult& result, const std::string& region)
{
    using R = Result<LocalState>;
    if (system.geometryKind != "layered-slab")
        return R::failure("local states are extracted from layered-slab systems");
    // The region's z range: thicknesses in order (current ones when the result has them).
    double z = 0.0, from = -1.0, to = -1.0;
    for (const auto& r : system.regions) {
        double t = r.thickness;
        if (result.geometry.contains("regions") && result.geometry["regions"].contains(r.id) && result.geometry["regions"][r.id].contains("thickness"))
            t = result.geometry["regions"][r.id]["thickness"].value("current", t);
        if (r.id == region) {
            from = z;
            to = z + t;
        }
        z += t;
    }
    if (from < 0.0)
        return R::failure("the system has no region '" + region + "'");
    const multiphysics::FieldData* T = nullptr;
    for (const auto& [name, field] : result.fields)
        if (name.size() > 12 && name.compare(name.size() - 12, 12, ".temperature") == 0)
            T = &field;
    if (!T)
        return R::failure("the result has no temperature field");
    LocalState s;
    s.region = region;
    s.zFrom = from;
    s.zTo = to;
    s.minTemperature = 1e300;
    s.maxTemperature = -1e300;
    double weight = 0.0, sum = 0.0, grad = 0.0;
    std::vector<std::pair<double, double>> inside;
    for (size_t c = 0; c < T->values.size(); ++c) {
        const double zc = T->support.cellCentre(c).z();
        if (zc < from || zc > to)
            continue;
        const double w = T->support.cellVolume(c);
        sum += w * T->values[c];
        weight += w;
        s.minTemperature = std::min(s.minTemperature, T->values[c]);
        s.maxTemperature = std::max(s.maxTemperature, T->values[c]);
        inside.emplace_back(zc, T->values[c]);
        ++s.cells;
    }
    if (s.cells == 0)
        return R::failure("no cell of the temperature field lies in the region '" + region + "'");
    s.meanTemperature = sum / weight;
    std::sort(inside.begin(), inside.end());
    for (size_t i = 1; i < inside.size(); ++i)
        grad += std::abs((inside[i].second - inside[i - 1].second) / (inside[i].first - inside[i - 1].first));
    s.meanGradient = inside.size() > 1 ? grad / (inside.size() - 1) : 0.0;
    return s;
}

Result<LocalState> regionState(const multiphysics::PhysicalSystem& system, const json& doc, const std::string& region)
{
    multiphysics::CoupledResult r;
    r.geometry = doc.value("geometry", json::object());
    const json fields = doc.value("fields", json::object());  // items() must not outlive its object
    for (const auto& [key, f] : fields.items()) {
        if (!f.contains("values") || !f.contains("edges"))
            continue;
        multiphysics::FieldData d;
        const auto edges = f["edges"].get<std::vector<std::vector<double>>>();
        if (edges.size() != 3)
            continue;
        for (size_t a = 0; a < 3; ++a)
            d.support.edges[a] = edges[a];
        d.values = f["values"].get<std::vector<double>>();
        if (d.values.size() != d.support.cellCount())
            continue;  // a vector field: not a temperature
        r.fields[key] = std::move(d);
    }
    return regionState(system, r, region);
}

Result<json> iterateRegion(const multiphysics::PhysicalSystem& system, const Project& project, const IterationSettings& st, JobContext* ctx)
{
    using R = Result<json>;
    auto model = ms::modelOf(project);
    if (!model)
        return R::failure(model.message());
    const ms::Representation* rve = model.value().find(st.representation);
    if (!rve || rve->kind != "rve.voxel")
        return R::failure("'" + st.representation + "' is not an RVE of the project");
    const multiphysics::RegionDef* region = nullptr;
    for (const auto& r : system.regions)
        if (r.id == st.region)
            region = &r;
    if (!region)
        return R::failure("the system has no region '" + st.region + "'");
    if (!(st.tolerance > 0.0) || st.maxIterations < 1 || st.maxIterations > 200)
        return R::failure("a positive tolerance and 1..200 iterations");
    MaterialLibrary library = project.materials;
    Material* target = library.findMutable(Id(region->materialId));
    if (!target)
        return R::failure("the region's material '" + region->materialId + "' is not in the library");
    if (!target->property(PhysicsDomain::Thermal, "thermalConductivity"))
        return R::failure("the region's material has no thermal conductivity to update");
    json history = json::array();
    double previous = target->property(PhysicsDomain::Thermal, "thermalConductivity")->value;
    bool converged = false;
    json lastDerivation;
    LocalState state;
    for (int it = 1; it <= st.maxIterations; ++it) {
        if (ctx && ctx->cancelRequested())
            return R::failure("cancelled");
        auto run = runPhysicalSystem(system, library);
        if (!run)
            return R::failure(run.message());
        if (!run.value().ok())
            return R::failure("the coupled run did not complete: " + run.value().message);
        auto s = regionState(system, run.value(), st.region);
        if (!s)
            return R::failure(s.message());
        state = s.value();
        json settings = st.derive;
        settings["temperatureK"] = state.meanTemperature;
        auto d = derive(*rve, project, "rve.thermal-conductivity", settings, nullptr);
        if (!d)
            return R::failure(d.message());
        double k = 0.0;
        for (const json& p : d.value()["properties"])
            if (p["key"] == "thermalConductivity" && p["component"] == "mean")
                k = p["value"].get<double>();
        history.push_back({{"iteration", it}, {"temperatureK", state.meanTemperature}, {"conductivityUsed", previous}, {"conductivity", k}});
        lastDerivation = d.take();
        PropertyValue v = *target->property(PhysicsDomain::Thermal, "thermalConductivity");
        v.value = k;
        if (Status ok = target->setProperty(PhysicsDomain::Thermal, "thermalConductivity", v); !ok)
            return R::failure(ok.message());
        if (ctx)
            ctx->reportProgress(std::min(1.0, static_cast<double>(it) / st.maxIterations), "iteration " + std::to_string(it));
        if (std::abs(k - previous) <= st.tolerance * std::abs(k)) {
            converged = true;
            break;
        }
        previous = k;
    }
    json coupling = {{"contract", "iterative.region-rve-thermal"}, {"region", st.region}, {"representation", st.representation},
                     {"tolerance", st.tolerance}, {"history", history}, {"converged", converged}, {"iterations", history.size()},
                     {"state", state.toJson()}};
    lastDerivation["coupling"] = coupling;
    if (!converged)
        lastDerivation["reliability"].push_back("the iterative coupling did not converge in " + std::to_string(st.maxIterations) + " iterations");
    lastDerivation["reliable"] = lastDerivation["reliability"].empty();
    return json{{"format", "resonant.multiscale-coupling"}, {"version", 1}, {"kind", "coupling"}, {"history", history}, {"converged", converged},
                {"iterations", history.size()}, {"state", state.toJson()}, {"derivation", lastDerivation}};
}

}  // namespace resonant::coupling
