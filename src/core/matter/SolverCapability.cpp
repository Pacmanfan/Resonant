#include "matter/SolverCapability.h"

#include "matter/PropertyResolution.h"

#include <algorithm>

namespace resonant::matter {

using nlohmann::json;

namespace {

bool has(const std::vector<std::string>& v, const std::string& x)
{
    return std::find(v.begin(), v.end(), x) != v.end();
}

// The anisotropic set that stands for a library property in an analysis that supports it.
std::string anisotropicCover(const std::string& key)
{
    if (key == "mechanical.youngsModulus" || key == "mechanical.poissonsRatio")
        return "elastic";
    if (key == "thermal.thermalConductivity")
        return "thermal";
    if (key == "thermal.thermalExpansion")
        return "expansion";
    return {};
}

std::string modelOfSet(const std::string& kind)
{
    if (kind == "elastic" || kind == "expansion")
        return "orthotropic-in-plane";
    if (kind == "thermal")
        return "principal-conductivity";
    return "piezoelectric";
}

std::string physicsOfSet(const std::string& kind)
{
    if (kind == "elastic" || kind == "expansion")
        return "mechanical";
    if (kind == "thermal")
        return "thermal";
    return "electromechanical";
}

}  // namespace

bool SolverCapability::supports(const std::string& m) const
{
    return has(materialModels, m);
}

bool SolverCapability::acceptsInterface(const std::string& kind) const
{
    return has(interfaceModels, kind);
}

json SolverCapability::toJson() const
{
    return {{"id", id},
            {"title", title},
            {"solver", solver},
            {"physics", physics},
            {"materialModels", materialModels},
            {"interfaceModels", interfaceModels},
            {"patterns", patterns},
            {"dimensionality", dimensionality},
            {"scales", scales},
            {"couplings", couplings},
            {"nonlinearities", nonlinearities},
            {"limitations", limitations},
            {"inputs", inputs},
            {"outputs", outputs},
            {"requiredProperties", requiredProperties}};
}

json Compatibility::toJson() const
{
    json issuesJson = json::array();
    for (const auto& i : issues)
        issuesJson.push_back(i.toJson());
    return {{"compatible", compatible}, {"issues", issuesJson}};
}

std::string Compatibility::summary() const
{
    std::string out;
    for (const auto& i : issues)
        if (i.severity == "refused")
            out += (out.empty() ? "" : "\n") + i.message;
    return out;
}

Compatibility checkStack(const Architecture& a, const Stack& stack, const SolverCapability& cap, const MaterialLibrary& library)
{
    Compatibility out;
    auto refuse = [&](const std::string& code, const std::string& element, const std::string& message) {
        out.compatible = false;
        out.issues.push_back({"refused", code, element, message});
    };
    auto warn = [&](const std::string& code, const std::string& element, const std::string& message) { out.issues.push_back({"warning", code, element, message}); };
    for (size_t i = 0; i < stack.layers.size(); ++i) {
        const Layer& l = stack.layers[i];
        const FunctionalRegion* r = a.region(l.region);
        if (!r) {
            refuse("unknown-region", l.id, "the layer '" + l.id + "' names no region");
            continue;
        }
        // Tensor-valued material fields: read only by the analyses that declare them.
        for (const auto& f : a.materialFields)
            if (f.region == r->id && !cap.supports("tensor-field"))
                refuse("tensor-field-not-supported", l.id, cap.title + " does not read the material field '" + f.id + "' (" + f.property + ") of '" + r->id + "'");
        // Anisotropic sets the analysis's physics reads.
        for (const auto& [kind, set] : r->anisotropic) {
            const std::string physics = physicsOfSet(kind);
            const bool relevant = has(cap.physics, physics) || (physics == "mechanical" && has(cap.physics, "electromechanical")) ||
                                  (kind == "expansion" && has(cap.physics, "mechanical"));
            if (!relevant) {
                if (kind == "piezoelectric" && has(cap.physics, "mechanical"))
                    warn("passive-piezoelectric", l.id,
                         "the piezoelectric coupling of '" + r->id + "' is not modeled by " + cap.title + ": the layer counts with its stiffness and mass only");
                continue;
            }
            if (!cap.supports(modelOfSet(kind)))
                refuse("anisotropy-not-supported", l.id,
                       cap.title + " treats layers as isotropic; the region '" + r->id + "' has an anisotropic " + kind + " set");
            else if (kind != "piezoelectric" && kind != "thermal" && r->orientation && !r->orientation->inPlane())
                refuse("orientation-not-supported", l.id,
                       cap.title + " represents in-plane orientations only; the region '" + r->id + "' is tilted out of the plane");
        }
        if (has(cap.physics, "electromechanical") && !r->anisotropic.count("piezoelectric") && r->role == "transduction")
            refuse("no-piezoelectric-set", l.id, "the transduction region '" + r->id + "' has no piezoelectric coefficients");
        // Required properties.
        for (const auto& key : cap.requiredProperties) {
            const std::string cover = anisotropicCover(key);
            if (!cover.empty() && r->anisotropic.count(cover) && cap.supports(modelOfSet(cover)))
                continue;
            const ResolvedProperty p = resolve(a, *r, key, 0.5, library);
            if (!p.ok()) {
                refuse("missing-property", l.id, "the layer '" + l.id + "' (" + r->id + ") lacks " + key + ": " + p.message);
                continue;
            }
            if (p.origin == "composition" && !cap.supports("composition-effective"))
                refuse("composition-not-supported", l.id, cap.title + " does not take effective properties of compositions (region '" + r->id + "')");
            if (p.origin == "field" && !cap.supports("graded"))
                refuse("graded-not-supported", l.id, cap.title + " cannot represent the graded " + key + " of '" + r->id + "'");
        }
        for (const auto& f : a.fields)
            if (f.region == r->id && f.property.rfind("fraction:", 0) == 0 && !cap.supports("graded"))
                refuse("graded-not-supported", l.id, cap.title + " cannot represent the graded composition of '" + r->id + "'");
        if (l.pattern.kind != "uniform" && !has(cap.patterns, l.pattern.kind))
            refuse("pattern-not-supported", l.id, "the " + l.pattern.kind + " pattern of '" + l.id + "' is not modeled by " + cap.title);
        // The interface with the layer below.
        if (i == 0 || stack.layers[i - 1].region == l.region)
            continue;
        const std::string& below = stack.layers[i - 1].region;
        const Interface* in = a.interfaceBetween(below, l.region);
        std::vector<std::string> needed;
        for (const auto& p : cap.physics) {
            if (p == "mechanical" || p == "thermal" || p == "electrical")
                needed.push_back(p);
            if (p == "electromechanical")
                needed.push_back("mechanical");
        }
        if (!in) {
            if (!needed.empty())
                refuse("undefined-interface", l.id,
                       "the layers '" + stack.layers[i - 1].id + "' and '" + l.id + "' meet without an interface: nothing is assumed about it");
            else
                warn("undefined-interface", l.id, "the layers '" + stack.layers[i - 1].id + "' and '" + l.id + "' meet without an interface record");
            continue;
        }
        for (const auto& physics : needed) {
            const InterfaceModel* m = in->modelFor(physics);
            if (!m)
                refuse("undefined-interface", in->id, "the interface '" + in->id + "' has no " + physics + " model (declare 'perfect' or a model)");
            else if (!cap.acceptsInterface(m->kind))
                refuse("interface-not-supported", in->id, "the " + m->kind + " model of '" + in->id + "' is not represented by " + cap.title);
            else if (m->kind == "cohesive-zone")
                warn("bonded-below-strength", in->id, "the cohesive interface '" + in->id + "' is treated as bonded: valid while its traction stays below the strength");
        }
        // Models of physics the analysis does not read are reported, not used.
        for (const auto& m : in->models) {
            const InterfaceModelKind* k = findInterfaceModelKind(m.kind);
            if (k && k->physics != "all" && !has(needed, k->physics) && !(k->physics == "electrical" && has(cap.physics, "electromagnetic")))
                continue;
            if (k && k->physics == "electrical" && has(cap.physics, "electromagnetic") && !cap.acceptsInterface(m.kind))
                refuse("interface-not-supported", in->id, "the " + m.kind + " model of '" + in->id + "' is not represented by " + cap.title);
        }
    }
    return out;
}

}  // namespace resonant::matter
