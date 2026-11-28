#include "coupling/MatterAnalyses.h"

#include "accounting/AccountingLedger.h"
#include "accounting/Conservation.h"
#include "accounting/ControlVolume.h"
#include "electromagnetics/LayeredMedia.h"
#include "manipulators/Analytics.h"
#include "materials/Material.h"
#include "matter/PropertyResolution.h"
#include "mechanics/Laminate.h"
#include "mechanics/LayeredSolid.h"
#include "mechanics/PiezoelectricBeam.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <numbers>
#include <set>
#include <sstream>

namespace resonant::coupling {

using nlohmann::json;
using namespace resonant::matter;

namespace {

constexpr double kEpsilon0 = 8.8541878128e-12;

std::string number(double v)
{
    std::ostringstream s;
    s.precision(4);
    s << v;
    return s.str();
}

// A (sub)layer as the analyses see it.
struct Sub {
    size_t layer = 0;
    std::string id, region;
    double thickness = 0.0;
    double E1 = 0.0, E2 = 0.0, G12 = 0.0, nu12 = 0.0, rho = 0.0, alpha1 = 0.0, alpha2 = 0.0, loss = 0.0, angle = 0.0;
    double kzz = 0.0, kxx = 0.0;
    double sigma = 0.0, epsr = 1.0, tand = 0.0, mur = 1.0;
    bool piezo = false;
    double d31 = 0.0, d32 = 0.0, eps33T = 0.0;
    int poling = 1;
    std::vector<std::string> origins;  // where the values came from
};

double required(const Architecture& a, const FunctionalRegion& r, const std::string& key, double s, const MaterialLibrary& lib, std::string& error,
                std::vector<std::string>& origins)
{
    const ResolvedProperty p = resolve(a, r, key, s, lib);
    if (!p.ok()) {
        if (error.empty())
            error = p.message;
        return std::nan("");
    }
    if (p.origin != "material")
        origins.push_back(key + ": " + p.origin + (p.source.empty() ? "" : " (" + p.source + ")"));
    return p.value;
}

double optionalValue(const Architecture& a, const FunctionalRegion& r, const std::string& key, double s, const MaterialLibrary& lib, double fallback, bool& found)
{
    const ResolvedProperty p = resolve(a, r, key, s, lib);
    found = p.ok();
    return p.ok() ? p.value : fallback;
}

double setValue(const AnisotropicSet& set, const std::string& key, std::string& error, const std::string& where)
{
    if (const SourcedValue* v = set.value(key))
        return v->value;
    if (error.empty())
        error = where + " lacks '" + key + "'";
    return std::nan("");
}

bool graded(const Architecture& a, const std::string& region)
{
    return std::any_of(a.fields.begin(), a.fields.end(), [&](const PropertyField& f) { return f.region == region; });
}

// The rotation of a region's material axes in the stack frame, including the layer's in-plane angle.
std::array<std::array<double, 3>, 3> frameRotation(const FunctionalRegion& r, double layerAngle)
{
    Orientation o = r.orientation.value_or(Orientation{});
    std::array<std::array<double, 3>, 3> R = o.rotation();
    const double c = std::cos(layerAngle), s = std::sin(layerAngle);
    std::array<std::array<double, 3>, 3> out{};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            const double rz[3][3] = {{c, -s, 0}, {s, c, 0}, {0, 0, 1}};
            for (int k = 0; k < 3; ++k)
                out[i][j] += rz[i][k] * R[k][j];
        }
    return out;
}

double inPlaneAngle(const FunctionalRegion& r, double layerAngle)
{
    if (!r.orientation)
        return layerAngle;
    return layerAngle + r.orientation->phi1 + r.orientation->phi2;  // in-plane orientations (Phi = 0) only reach here
}

Result<std::vector<Sub>> subLayers(const Architecture& a, const Stack& stack, const std::set<std::string>& needs, int sublayers, const MaterialLibrary& lib)
{
    using R = Result<std::vector<Sub>>;
    std::vector<Sub> out;
    for (size_t i = 0; i < stack.layers.size(); ++i) {
        const Layer& l = stack.layers[i];
        const FunctionalRegion* r = a.region(l.region);
        if (!r)
            return R::failure("the layer '" + l.id + "' names no region");
        const int n = graded(a, r->id) ? std::max(1, sublayers) : 1;
        for (int k = 0; k < n; ++k) {
            const double s = (k + 0.5) / n;
            Sub sub;
            sub.layer = i;
            sub.id = n == 1 ? l.id : l.id + "." + std::to_string(k);
            sub.region = r->id;
            sub.thickness = l.thickness / n;
            std::string error;
            const std::string where = "the region '" + r->id + "'";
            if (needs.count("mechanical")) {
                if (auto it = r->anisotropic.find("elastic"); it != r->anisotropic.end()) {
                    sub.E1 = setValue(it->second, "E1", error, where + "'s elastic set");
                    sub.E2 = setValue(it->second, "E2", error, where + "'s elastic set");
                    sub.G12 = setValue(it->second, "G12", error, where + "'s elastic set");
                    sub.nu12 = setValue(it->second, "nu12", error, where + "'s elastic set");
                } else {
                    const double e = required(a, *r, "mechanical.youngsModulus", s, lib, error, sub.origins);
                    const double nu = required(a, *r, "mechanical.poissonsRatio", s, lib, error, sub.origins);
                    sub.E1 = sub.E2 = e;
                    sub.nu12 = nu;
                    sub.G12 = e / (2.0 * (1.0 + nu));
                }
                sub.rho = required(a, *r, "mechanical.density", s, lib, error, sub.origins);
                bool found = false;
                sub.loss = optionalValue(a, *r, "mechanical.lossFactor", s, lib, 0.0, found);
                sub.angle = inPlaneAngle(*r, l.angle);
            }
            if (needs.count("expansion")) {
                if (auto it = r->anisotropic.find("expansion"); it != r->anisotropic.end()) {
                    sub.alpha1 = setValue(it->second, "alpha1", error, where + "'s expansion set");
                    sub.alpha2 = setValue(it->second, "alpha2", error, where + "'s expansion set");
                } else {
                    sub.alpha1 = sub.alpha2 = required(a, *r, "thermal.thermalExpansion", s, lib, error, sub.origins);
                }
            }
            if (needs.count("thermal")) {
                if (auto it = r->anisotropic.find("thermal"); it != r->anisotropic.end()) {
                    const double k1 = setValue(it->second, "k1", error, where + "'s thermal set");
                    const double k2 = setValue(it->second, "k2", error, where + "'s thermal set");
                    const double k3 = setValue(it->second, "k3", error, where + "'s thermal set");
                    const auto rot = frameRotation(*r, l.angle);
                    // k_frame = R diag(k) R^T: the through-thickness (zz) and in-plane (xx) components.
                    sub.kzz = rot[2][0] * rot[2][0] * k1 + rot[2][1] * rot[2][1] * k2 + rot[2][2] * rot[2][2] * k3;
                    sub.kxx = rot[0][0] * rot[0][0] * k1 + rot[0][1] * rot[0][1] * k2 + rot[0][2] * rot[0][2] * k3;
                    sub.origins.push_back("thermal: principal conductivities rotated into the stack frame");
                } else {
                    sub.kzz = sub.kxx = required(a, *r, "thermal.thermalConductivity", s, lib, error, sub.origins);
                }
            }
            if (needs.count("electrical") || needs.count("em")) {
                sub.sigma = required(a, *r, "electromagnetic.conductivity", s, lib, error, sub.origins);
                sub.epsr = required(a, *r, "electromagnetic.relativePermittivity", s, lib, error, sub.origins);
            }
            if (needs.count("em")) {
                sub.tand = required(a, *r, "electromagnetic.lossTangent", s, lib, error, sub.origins);
                sub.mur = required(a, *r, "electromagnetic.relativePermeability", s, lib, error, sub.origins);
            }
            if (needs.count("piezo"))
                if (auto it = r->anisotropic.find("piezoelectric"); it != r->anisotropic.end()) {
                    if (n > 1)
                        return R::failure("the transduction layer '" + l.id + "' is graded: a graded piezoelectric layer is not modeled");
                    sub.piezo = true;
                    sub.d31 = setValue(it->second, "d31", error, where + "'s piezoelectric set");
                    const SourcedValue* d32 = it->second.value("d32");
                    sub.d32 = d32 ? d32->value : sub.d31;
                    sub.eps33T = setValue(it->second, "eps33T", error, where + "'s piezoelectric set");
                    sub.poling = it->second.poling == "-3" ? -1 : 1;
                }
            if (!error.empty())
                return R::failure("the layer '" + l.id + "': " + error);
            out.push_back(std::move(sub));
        }
    }
    return out;
}

std::vector<mechanics::Ply> pliesOf(const std::vector<Sub>& subs)
{
    std::vector<mechanics::Ply> out;
    for (const auto& s : subs) {
        mechanics::Ply p;
        p.name = s.id;
        p.thickness = s.thickness;
        p.E1 = s.E1;
        p.E2 = s.E2;
        p.G12 = s.G12;
        p.nu12 = s.nu12;
        p.density = s.rho;
        p.angle = s.angle;
        p.alpha1 = s.alpha1;
        p.alpha2 = s.alpha2;
        p.lossFactor = s.loss;
        out.push_back(p);
    }
    return out;
}

json matrix(const Eigen::Matrix3d& m)
{
    json out = json::array();
    for (int i = 0; i < 3; ++i)
        out.push_back(json::array({m(i, 0), m(i, 1), m(i, 2)}));
    return out;
}

std::string representationOf(const Architecture& a)
{
    if (a.representation == "as-built")
        return "a computed result for an as-built architecture";
    return "a computed result of an " + std::string(a.representation == "as-designed" ? "as-designed" : "idealized") + " model";
}

// The index ranges of the subs of each layer.
std::vector<std::pair<size_t, size_t>> layerRanges(const std::vector<Sub>& subs, size_t layers)
{
    std::vector<std::pair<size_t, size_t>> out(layers, {0, 0});
    for (size_t i = 0; i < subs.size(); ++i) {
        auto& r = out[subs[i].layer];
        if (r.second == 0)
            r.first = i;
        r.second = i + 1;
    }
    return out;
}

const Interface* interfaceBetweenLayers(const Architecture& a, const Stack& s, size_t i)
{
    return a.interfaceBetween(s.layers[i].region, s.layers[i + 1].region);
}

// --- the analyses -----------------------------------------------------------------------------

Result<json> laminateAnalysis(const Architecture& a, const Stack& s, const json& o, const MaterialLibrary& lib)
{
    const double dT = o.value("deltaT", 0.0);
    std::set<std::string> needs{"mechanical"};
    if (dT != 0.0)
        needs.insert("expansion");
    auto subs = subLayers(a, s, needs, o.value("sublayers", 40), lib);
    if (!subs)
        return Result<json>::failure(subs.message());
    auto l = mechanics::laminate(pliesOf(subs.value()));
    if (!l)
        return Result<json>::failure(l.message());
    const mechanics::Laminate& lam = l.value();
    json metrics{{"Ex", lam.Ex},
                 {"Ey", lam.Ey},
                 {"Gxy", lam.Gxy},
                 {"nuxy", lam.nuxy},
                 {"bendingStiffness", lam.Dstar(0, 0)},
                 {"massPerArea", lam.massPerArea},
                 {"thickness", lam.thickness},
                 {"neutralSurface", lam.neutralSurface}};
    json result{{"A", matrix(lam.A)}, {"B", matrix(lam.B)}, {"D", matrix(lam.D)}, {"Dstar", matrix(lam.Dstar)}, {"symmetric", lam.symmetric}, {"plies", subs.value().size()}};
    json assumptions = json::array({"classical lamination theory: thin plies in plane stress, the Kirchhoff hypothesis (no transverse shear)",
                                    "perfect bonding between the plies (no slip, no peel)", "linear elasticity, small strain"});
    if (dT != 0.0) {
        const mechanics::ThermalResponse t = mechanics::thermalResponse(lam, dT);
        result["thermal"] = {{"deltaT", dT},
                             {"strain", json::array({t.strain(0), t.strain(1), t.strain(2)})},
                             {"curvature", json::array({t.curvature(0), t.curvature(1), t.curvature(2)})}};
        metrics["curvature"] = t.curvature(0);
        const auto ranges = layerRanges(subs.value(), s.layers.size());
        json interfaces = json::array();
        double worst = 0.0;
        for (size_t i = 0; i + 1 < s.layers.size(); ++i) {
            if (s.layers[i].region == s.layers[i + 1].region)
                continue;
            const auto& below = t.stress[ranges[i].second - 1].top;
            const auto& above = t.stress[ranges[i + 1].first].bottom;
            const double mismatch = std::max(std::abs(below(0)), std::abs(above(0)));
            worst = std::max(worst, mismatch);
            const Interface* in = interfaceBetweenLayers(a, s, i);
            interfaces.push_back({{"interface", in ? in->id : std::string()},
                                  {"below", s.layers[i].id},
                                  {"above", s.layers[i + 1].id},
                                  {"stressBelow", json::array({below(0), below(1)})},
                                  {"stressAbove", json::array({above(0), above(1)})},
                                  {"mismatchStress", mismatch}});
        }
        result["thermal"]["interfaces"] = interfaces;
        metrics["maxInterfaceStress"] = worst;
        assumptions.push_back("a uniform temperature change of the free laminate; in-plane ply stresses only - the interfacial shear and peel stresses at free edges are not given");
    }
    return json{{"metrics", metrics}, {"result", result}, {"assumptions", assumptions}};
}

std::vector<size_t> bareLayers(const Architecture& a, const Stack& s, const json& o)
{
    std::vector<size_t> out;
    if (o.contains("bare") && o["bare"].is_array()) {
        for (const json& i : o["bare"])
            if (i.is_number_integer() && i.get<int>() >= 0 && static_cast<size_t>(i.get<int>()) < s.layers.size())
                out.push_back(static_cast<size_t>(i.get<int>()));
        return out;
    }
    for (size_t i = 0; i < s.layers.size(); ++i)
        if (const FunctionalRegion* r = a.region(s.layers[i].region); r && r->role == "substrate")
            out.push_back(i);
    if (out.empty())
        out.push_back(0);
    return out;
}

Result<json> modesAnalysis(const Architecture& a, const Stack& s, const json& o, const MaterialLibrary& lib)
{
    auto subs = subLayers(a, s, {"mechanical"}, o.value("sublayers", 40), lib);
    if (!subs)
        return Result<json>::failure(subs.message());
    const std::string boundary = o.value("boundary", std::string("cantilever"));
    const double length = o.value("length", s.length), width = o.value("width", s.width);
    const int count = std::clamp(o.value("count", 3), 1, 10);
    if (!(length > 0.0) || !(width > 0.0))
        return Result<json>::failure("the modes need the stack's length and width (set them on the stack or pass them)");
    const auto bare = bareLayers(a, s, o);
    std::vector<Sub> bareSubs;
    for (const auto& sub : subs.value())
        if (std::find(bare.begin(), bare.end(), sub.layer) != bare.end())
            bareSubs.push_back(sub);
    auto frequencies = [&](const std::vector<Sub>& list, double& massPerArea) -> Result<std::vector<double>> {
        const auto plies = pliesOf(list);
        auto l = mechanics::laminate(plies);
        if (!l)
            return Result<std::vector<double>>::failure(l.message());
        massPerArea = l.value().massPerArea;
        if (boundary == "cantilever") {
            const double ei = o.value("wide", false) ? l.value().Dstar(0, 0) * width : mechanics::beamSection(plies).bendingStiffness * width;
            return mechanics::cantileverFrequencies(ei, massPerArea * width, length, count);
        }
        if (boundary == "simply-supported-plate") {
            std::vector<double> out;
            for (const auto& m : mechanics::simplySupportedPlateModes(l.value().Dstar, massPerArea, length, width, count))
                out.push_back(m.frequency);
            return out;
        }
        return Result<std::vector<double>>::failure("the boundary is cantilever or simply-supported-plate");
    };
    double coatedMass = 0.0, bareMass = 0.0;
    auto coated = frequencies(subs.value(), coatedMass);
    auto plain = frequencies(bareSubs, bareMass);
    if (!coated || !plain)
        return Result<json>::failure(!coated ? coated.message() : plain.message());
    json bareIds = json::array();
    for (size_t i : bare)
        bareIds.push_back(s.layers[i].id);
    json warnings = json::array();
    if (boundary == "simply-supported-plate") {
        auto l = mechanics::laminate(pliesOf(subs.value()));
        if (l && (std::abs(l.value().D(0, 2)) > 1e-3 * l.value().D(0, 0) || std::abs(l.value().D(1, 2)) > 1e-3 * l.value().D(0, 0)))
            warnings.push_back("the laminate has bending-twisting coupling (D16, D26), which the plate formula neglects");
    }
    const double shift = coated.value()[0] / plain.value()[0] - 1.0;
    json metrics{{"f1", coated.value()[0]}, {"f1Bare", plain.value()[0]}, {"f1Shift", shift}, {"massPerArea", coatedMass}, {"massAdded", coatedMass - bareMass}};
    if (coated.value().size() > 1)
        metrics["f2"] = coated.value()[1];
    json assumptions = json::array({"the coating's stiffness and mass are included (the bare structure is computed for comparison only)",
                                    boundary == "cantilever" ? "a clamped-free Euler-Bernoulli beam, narrow (uniaxial stress) unless 'wide'"
                                                             : "a simply supported, specially orthotropic Kirchhoff plate",
                                    "perfect bonding between the layers; no transverse shear (slender structures, low modes)"});
    return json{{"metrics", metrics},
                {"result", {{"boundary", boundary}, {"length", length}, {"width", width}, {"coated", coated.value()}, {"bare", plain.value()}, {"bareLayers", bareIds}}},
                {"assumptions", assumptions},
                {"warnings", warnings}};
}

// The electrode span of a transduction layer: its own segmented pattern, or a neighbouring
// electrode layer's.
std::pair<double, double> electrodeSpan(const Architecture& a, const Stack& s, size_t layer)
{
    const Pattern& own = s.layers[layer].pattern;
    if (own.kind == "segmented")
        return {own.start, own.end};
    double start = 0.0, end = 1.0;
    for (size_t k : {layer - 1, layer + 1}) {
        if (k >= s.layers.size())
            continue;
        const FunctionalRegion* r = a.region(s.layers[k].region);
        if (r && r->role == "electrode" && s.layers[k].pattern.kind == "segmented") {
            start = std::max(start, s.layers[k].pattern.start);
            end = std::min(end, s.layers[k].pattern.end);
        }
    }
    return {start, end};
}

json harvesterLedger(const mechanics::HarvesterPoint& p, std::string& status)
{
    accounting::Ledger l;
    l.id = "harvester-stage";
    l.title = "Piezoelectric cantilever: time-averaged powers";
    l.sourceKind = "matter.harvester";
    l.solver = "mechanics.piezoelectric-beam";
    l.basis = accounting::Basis::Rate;
    l.entities = {{"base", "Base excitation", accounting::EntityKind::Source, "mechanical", "", "", "", "", ""},
                  {"beam", "Beam", accounting::EntityKind::Body, "mechanical", "", "", "", "", ""},
                  {"load", "Electrical load", accounting::EntityKind::Sink, "electrical", "", "", "", "", ""},
                  {"surroundings", "Surroundings", accounting::EntityKind::Environment, "thermal", "", "", "", "", ""}};
    auto exchange = [](const char* id, const char* from, const char* to, const char* category, const char* mechanism, const char* fromDomain,
                       const char* toDomain, double amount) {
        accounting::Exchange e;
        e.id = id;
        e.from = from;
        e.to = to;
        e.family = accounting::Family::Energy;
        e.category = category;
        e.mechanism = mechanism;
        e.fromDomain = fromDomain;
        e.toDomain = toDomain;
        e.amount = amount;
        e.method = "lumped single-mode model";
        return e;
    };
    l.exchanges = {exchange("base-work", "base", "beam", "work", "base excitation", "mechanical", "mechanical", p.inputPower),
                   exchange("conversion", "beam", "load", "work", "piezoelectric conversion", "mechanical", "electrical", p.electricalPower),
                   exchange("damping", "beam", "surroundings", "dissipation", "mechanical damping", "mechanical", "thermal", p.dissipatedPower)};
    accounting::AccountingSystem system;
    system.id = "beam";
    system.name = "The beam";
    system.kind = accounting::SystemKind::Object;
    system.members = {"beam"};
    const accounting::Report report = accounting::evaluate(l, system, accounting::Tolerances{});
    status = std::string(accounting::toString(accounting::worst(report.checks)));
    return {{"ledger", l.toJson()}, {"report", report.toJson()}};
}

Result<json> harvesterAnalysis(const Architecture& a, const Stack& s, const json& o, const MaterialLibrary& lib)
{
    auto subs = subLayers(a, s, {"mechanical", "piezo"}, o.value("sublayers", 40), lib);
    if (!subs)
        return Result<json>::failure(subs.message());
    mechanics::HarvesterSpec spec;
    spec.plies = pliesOf(subs.value());
    spec.width = o.value("width", s.width);
    spec.length = o.value("length", s.length);
    spec.tipMass = o.value("tipMass", 0.0);
    spec.dampingRatio = o.value("dampingRatio", -1.0);
    json layers = json::array();
    for (size_t i = 0; i < subs.value().size(); ++i) {
        const Sub& sub = subs.value()[i];
        if (!sub.piezo)
            continue;
        // The coefficient along the beam axis for the film's in-plane orientation.
        const double c = std::cos(sub.angle), sn = std::sin(sub.angle);
        const double d = sub.d31 * c * c + sub.d32 * sn * sn;
        const auto [start, end] = electrodeSpan(a, s, sub.layer);
        spec.piezo.push_back({i, d, sub.eps33T, sub.poling, start, end});
        layers.push_back({{"layer", sub.id}, {"region", sub.region}, {"d31AlongBeam", d}, {"angle", sub.angle}, {"electrode", json::array({start, end})},
                          {"poling", sub.poling}});
    }
    if (spec.piezo.empty())
        return Result<json>::failure("the stack has no piezoelectric layer (a region with a piezoelectric set)");
    spec.connection = o.value("connection", std::string(spec.piezo.size() == 1 ? "single" : "parallel"));
    auto model = mechanics::harvesterModel(spec);
    if (!model)
        return Result<json>::failure(model.message());
    const mechanics::HarvesterModel& m = model.value();
    const double frequency = o.contains("frequency") && o["frequency"].is_number() ? o["frequency"].get<double>() : m.shortCircuitHz;
    const double acceleration = o.value("baseAcceleration", 1.0);
    const double load = o.contains("load") && o["load"].is_number() ? o["load"].get<double>() : mechanics::optimalLoad(m, frequency);
    const mechanics::HarvesterPoint p = mechanics::harvesterResponse(m, frequency, load, acceleration);
    std::string balance;
    json ledger = harvesterLedger(p, balance);
    json sweep = json::array();
    if (o.contains("sweep") && o["sweep"].is_object()) {
        const double from = o["sweep"].value("from", 0.8 * m.shortCircuitHz), to = o["sweep"].value("to", 1.2 * m.openCircuitHz);
        const int points = std::clamp(o["sweep"].value("points", 101), 2, 2001);
        for (int k = 0; k < points; ++k) {
            const double f = from + (to - from) * k / (points - 1);
            const mechanics::HarvesterPoint q = mechanics::harvesterResponse(m, f, load, acceleration);
            sweep.push_back({{"frequency", f}, {"power", q.electricalPower}, {"voltage", std::abs(q.voltage)}, {"tipDisplacement", std::abs(q.displacement)}});
        }
    }
    json metrics{{"power", p.electricalPower},
                 {"voltage", std::abs(p.voltage)},
                 {"efficiency", p.efficiency},
                 {"inputPower", p.inputPower},
                 {"dissipatedPower", p.dissipatedPower},
                 {"tipDisplacement", std::abs(p.displacement)},
                 {"shortCircuitHz", m.shortCircuitHz},
                 {"openCircuitHz", m.openCircuitHz},
                 {"couplingSquared", m.couplingSquared},
                 {"load", load},
                 {"massPerArea", mechanics::beamSection(spec.plies).massPerArea}};
    json result{{"model",
                 {{"mass", m.mass},
                  {"forcingMass", m.forcingMass},
                  {"stiffness", m.stiffness},
                  {"damping", m.damping},
                  {"dampingRatio", m.dampingRatio},
                  {"dampingSource", m.dampingSource},
                  {"theta", m.theta},
                  {"capacitance", m.capacitance},
                  {"neutralAxis", m.neutralAxis},
                  {"eulerBernoulliHz", m.eulerBernoulliHz},
                  {"layerTheta", m.layerTheta},
                  {"connection", spec.connection}}},
                {"piezoelectricLayers", layers},
                {"point", {{"frequency", frequency}, {"load", load}, {"baseAcceleration", acceleration}}},
                {"balance", balance},
                {"accounting", ledger},
                {"sweep", sweep}};
    if (o.contains("drive") && o["drive"].is_number()) {
        const double v = o["drive"].get<double>();
        result["actuator"] = {{"voltage", v},
                              {"staticDisplacement", std::abs(mechanics::actuatorDisplacement(m, 0.0, v))},
                              {"displacementAtFrequency", std::abs(mechanics::actuatorDisplacement(m, frequency, v))}};
        metrics["actuatorDisplacement"] = std::abs(mechanics::actuatorDisplacement(m, frequency, v));
    }
    json warnings = json::array();
    for (size_t i = 1; i < m.layerTheta.size(); ++i)
        if (m.layerTheta[i] * m.layerTheta[0] < 0.0 && spec.connection == "parallel")
            warnings.push_back("the piezoelectric layers' contributions have opposite signs in a parallel connection: their charges cancel (opposing polarization)");
    if (balance != "verified")
        warnings.push_back("the energy balance of the stage is " + balance);
    json assumptions = json::array();
    for (const auto& x : m.assumptions)
        assumptions.push_back(x);
    assumptions.push_back("the electrodes, adhesion and encapsulation count with their stiffness and mass");
    return json{{"metrics", metrics}, {"result", result}, {"assumptions", assumptions}, {"warnings", warnings}};
}

Result<json> thermalAnalysis(const Architecture& a, const Stack& s, const json& o, const MaterialLibrary& lib)
{
    const int n = o.value("sublayers", 200);
    auto subs = subLayers(a, s, {"thermal"}, n, lib);
    if (!subs)
        return Result<json>::failure(subs.message());
    auto coarse = subLayers(a, s, {"thermal"}, std::max(1, n / 2), lib);
    json layers = json::array();
    std::vector<double> layerR(s.layers.size(), 0.0), layerRcoarse(s.layers.size(), 0.0), layerKxx(s.layers.size(), 0.0);
    for (const auto& sub : subs.value()) {
        layerR[sub.layer] += sub.thickness / sub.kzz;
        layerKxx[sub.layer] += sub.kxx * sub.thickness;
    }
    if (coarse)
        for (const auto& sub : coarse.value())
            layerRcoarse[sub.layer] += sub.thickness / sub.kzz;
    double total = 0.0, interfaceTotal = 0.0, kParallel = 0.0, discretization = 0.0;
    std::vector<double> interfaceR(s.layers.size(), 0.0);
    json interfaces = json::array();
    for (size_t i = 0; i < s.layers.size(); ++i) {
        const Layer& l = s.layers[i];
        // A field on the conductivity itself is integrated exactly (the series combination of the field).
        const FunctionalRegion* r = a.region(l.region);
        const PropertyField* f = r && !r->anisotropic.count("thermal") ? a.field(r->id, "thermal.thermalConductivity") : nullptr;
        if (f) {
            layerR[i] = l.thickness / f->harmonicMean();
            layerKxx[i] = f->mean() * l.thickness;
        } else if (graded(a, l.region)) {
            discretization = std::max(discretization, std::abs(layerR[i] - layerRcoarse[i]) / layerR[i]);
        }
        total += layerR[i];
        kParallel += layerKxx[i];
        layers.push_back({{"layer", l.id}, {"region", l.region}, {"thickness", l.thickness}, {"resistance", layerR[i]},
                          {"conductivity", l.thickness / layerR[i]}, {"exact", f != nullptr || !graded(a, l.region)}});
        if (i + 1 < s.layers.size() && s.layers[i + 1].region != l.region) {
            const Interface* in = interfaceBetweenLayers(a, s, i);
            const InterfaceModel* m = in ? in->modelFor("thermal") : nullptr;
            double rc = 0.0;
            if (m && m->kind == "thermal-contact-resistance")
                rc = m->parameter("resistance")->value;
            interfaceR[i] = rc;
            total += rc;
            interfaceTotal += rc;
            interfaces.push_back({{"interface", in ? in->id : std::string()}, {"model", m ? m->kind : std::string()}, {"resistance", rc}});
        }
    }
    const double H = s.thickness();
    json metrics{{"resistance", total},
                 {"effectiveConductivity", H / total},
                 {"inPlaneConductivity", kParallel / H},
                 {"interfaceShare", total > 0.0 ? interfaceTotal / total : 0.0}};
    // A temperature profile for a stated heat flux or face temperatures.
    json profile = json::array();
    double q = std::nan("");
    double t0 = o.value("bottomTemperature", 293.15);
    if (o.contains("heatFlux") && o["heatFlux"].is_number())
        q = o["heatFlux"].get<double>();
    if (o.contains("temperatures") && o["temperatures"].is_array() && o["temperatures"].size() == 2) {
        t0 = o["temperatures"][0].get<double>();
        q = (t0 - o["temperatures"][1].get<double>()) / total;
        metrics["heatFlux"] = q;
    }
    if (std::isfinite(q)) {
        metrics["temperatureDrop"] = q * total;
        double z = 0.0, t = t0;
        profile.push_back(json::array({z, t}));
        for (size_t i = 0; i < s.layers.size(); ++i) {
            z += s.layers[i].thickness;
            t -= q * layerR[i];
            profile.push_back(json::array({z, t}));
            if (interfaceR[i] > 0.0) {
                t -= q * interfaceR[i];
                profile.push_back(json::array({z, t}));  // the jump across the interface
            }
        }
    }
    return json{{"metrics", metrics},
                {"result", {{"layers", layers}, {"interfaces", interfaces}, {"profile", profile}, {"discretizationError", discretization}}},
                {"assumptions",
                 json::array({"one-dimensional steady conduction through the thickness (no lateral spreading, no radiation, no sources)",
                              "graded conductivities integrated in series (exactly for a field on the conductivity, by sublayers for a graded composition)",
                              "interfaces as stated: perfect contact or a contact resistance"})}};
}

Result<json> electricalAnalysis(const Architecture& a, const Stack& s, const json& o, const MaterialLibrary& lib)
{
    auto subs = subLayers(a, s, {"electrical"}, o.value("sublayers", 40), lib);
    if (!subs)
        return Result<json>::failure(subs.message());
    double resistance = 0.0, inverseC = 0.0;
    bool insulating = false;
    std::vector<double> layerR(s.layers.size(), 0.0), layerInvC(s.layers.size(), 0.0);
    std::vector<bool> layerInsulating(s.layers.size(), false);
    for (const auto& sub : subs.value()) {
        if (sub.sigma > 0.0)
            layerR[sub.layer] += sub.thickness / sub.sigma;
        else
            layerInsulating[sub.layer] = true;
        layerInvC[sub.layer] += sub.thickness / (kEpsilon0 * sub.epsr);
    }
    json layers = json::array(), interfaces = json::array();
    for (size_t i = 0; i < s.layers.size(); ++i) {
        resistance += layerR[i];
        insulating = insulating || layerInsulating[i];
        inverseC += layerInvC[i];
        layers.push_back({{"layer", s.layers[i].id},
                          {"resistance", layerInsulating[i] ? json(nullptr) : json(layerR[i])},
                          {"capacitance", 1.0 / layerInvC[i]},
                          {"insulating", static_cast<bool>(layerInsulating[i])}});
        if (i + 1 < s.layers.size() && s.layers[i + 1].region != s.layers[i].region) {
            const Interface* in = interfaceBetweenLayers(a, s, i);
            const InterfaceModel* m = in ? in->modelFor("electrical") : nullptr;
            const double rc = m && m->kind == "electrical-contact-resistance" ? m->parameter("resistance")->value : 0.0;
            resistance += rc;
            interfaces.push_back({{"interface", in ? in->id : std::string()}, {"model", m ? m->kind : std::string()}, {"resistance", rc}});
        }
    }
    json metrics{{"capacitancePerArea", 1.0 / inverseC}};
    if (!insulating) {
        metrics["seriesResistance"] = resistance;
        metrics["timeConstant"] = resistance / inverseC;
    }
    if (o.contains("voltage") && o["voltage"].is_number())
        metrics["leakageCurrentDensity"] = insulating ? 0.0 : o["voltage"].get<double>() / resistance;
    return json{{"metrics", metrics},
                {"result", {{"layers", layers}, {"interfaces", interfaces}, {"insulating", insulating}}},
                {"assumptions", json::array({"one-dimensional conduction and displacement through the thickness", "ohmic layers; contact resistances as stated",
                                             "a layer of zero conductivity is an ideal insulator"})}};
}

Result<json> electromagneticAnalysis(const Architecture& a, const Stack& s, const json& o, const MaterialLibrary& lib)
{
    auto subs = subLayers(a, s, {"em"}, o.value("sublayers", 40), lib);
    if (!subs)
        return Result<json>::failure(subs.message());
    std::vector<double> frequencies;
    if (o.contains("frequencies") && o["frequencies"].is_array())
        for (const json& f : o["frequencies"])
            frequencies.push_back(f.get<double>());
    else {
        const json r = o.value("range", json::object());
        const double from = r.value("from", 1e9), to = r.value("to", 10e9);
        const int points = std::clamp(r.value("points", 10), 1, 2000);
        for (int k = 0; k < points; ++k)
            frequencies.push_back(points == 1 ? from : from + (to - from) * k / (points - 1));
    }
    em::LayeredProblem p;
    p.incidentIntensity = o.value("incidentIntensity", 1.0);
    for (const auto& sub : subs.value()) {
        em::MediumLayer m;
        m.name = sub.id;
        m.thickness = sub.thickness;
        m.permittivityReal = sub.epsr;
        m.lossTangent = sub.tand;
        m.conductivity = sub.sigma;
        m.permeability = sub.mur;
        p.layers.push_back(m);
    }
    json assumptions = json::array({"a plane wave at normal incidence on laterally infinite layers", "isotropic layers; lossless half-spaces on both sides"});
    if (o.value("exit", std::string("vacuum")) == "metal") {
        em::MediumLayer metal;
        metal.name = "metal backing";
        metal.thickness = 1e-4;
        metal.conductivity = 5.8e7;
        p.layers.push_back(metal);
        assumptions.push_back("a metal backing of 0.1 mm with the conductivity of copper (5.8e7 S/m, literature)");
    }
    json points = json::array();
    double best = -1.0, bestAt = 0.0;
    json first;
    for (double f : frequencies) {
        p.frequencyHz = f;
        auto sol = em::LayeredSolution::solve(p);
        if (!sol)
            return Result<json>::failure(sol.message());
        const double A = sol.value().absorptance();
        points.push_back({{"frequency", f}, {"reflectance", sol.value().reflectance()}, {"transmittance", sol.value().transmittance()}, {"absorptance", A}});
        if (A > best) {
            best = A;
            bestAt = f;
        }
        if (first.is_null()) {
            json perLayer = json::array();
            double z = 0.0;
            const auto ranges = layerRanges(subs.value(), s.layers.size());
            for (size_t i = 0; i < s.layers.size(); ++i) {
                double z1 = z;
                for (size_t k = ranges[i].first; k < ranges[i].second; ++k)
                    z1 += subs.value()[k].thickness;
                perLayer.push_back({{"layer", s.layers[i].id}, {"absorbed", sol.value().absorbedPower(z, z1) / p.incidentIntensity}});
                z = z1;
            }
            first = {{"frequency", f}, {"reflectance", sol.value().reflectance()}, {"transmittance", sol.value().transmittance()}, {"absorptance", A},
                     {"layers", perLayer}};
        }
    }
    json metrics{{"absorptance", first["absorptance"]}, {"reflectance", first["reflectance"]}, {"transmittance", first["transmittance"]},
                 {"maxAbsorptance", best}, {"frequencyOfMaxAbsorptance", bestAt}};
    return json{{"metrics", metrics}, {"result", {{"points", points}, {"first", first}}}, {"assumptions", assumptions}};
}

Result<json> interfacesAnalysis(const Architecture& a, const Stack& s, const json& o, const MaterialLibrary& lib)
{
    json list = json::array();
    double worstStress = 0.0, minTransmission = 1.0;
    json thermalStress;
    const double dT = o.value("deltaT", 0.0);
    if (dT != 0.0) {
        auto lam = laminateAnalysis(a, s, {{"deltaT", dT}}, lib);
        if (lam)
            thermalStress = lam.value()["result"]["thermal"]["interfaces"];
    }
    auto impedance = [&](const FunctionalRegion& r, std::string& basis) {
        const ResolvedProperty rho = resolve(a, r, "acoustic.density", 0.5, lib), c = resolve(a, r, "acoustic.soundSpeed", 0.5, lib);
        if (rho.ok() && c.ok()) {
            basis = "acoustic density and sound speed";
            return rho.value * c.value;
        }
        const ResolvedProperty e = resolve(a, r, "mechanical.youngsModulus", 0.5, lib), nu = resolve(a, r, "mechanical.poissonsRatio", 0.5, lib),
                               d = resolve(a, r, "mechanical.density", 0.5, lib);
        if (e.ok() && nu.ok() && d.ok()) {
            basis = "the longitudinal wave of an isotropic solid";
            const double cl = std::sqrt(e.value * (1.0 - nu.value) / (d.value * (1.0 + nu.value) * (1.0 - 2.0 * nu.value)));
            return d.value * cl;
        }
        basis = "not available";
        return std::nan("");
    };
    for (size_t i = 0; i + 1 < s.layers.size(); ++i) {
        if (s.layers[i].region == s.layers[i + 1].region)
            continue;
        const FunctionalRegion* r0 = a.region(s.layers[i].region);
        const FunctionalRegion* r1 = a.region(s.layers[i + 1].region);
        if (!r0 || !r1)
            continue;
        const Interface* in = interfaceBetweenLayers(a, s, i);
        json e{{"interface", in ? in->id : std::string()}, {"below", s.layers[i].id}, {"above", s.layers[i + 1].id}};
        json models = json::array();
        if (in)
            for (const auto& m : in->models)
                models.push_back(m.kind);
        e["models"] = models;
        if (!in)
            e["note"] = "no interface is declared: nothing is assumed about it";
        std::string b0, b1;
        const double z0 = impedance(*r0, b0), z1 = impedance(*r1, b1);
        if (std::isfinite(z0) && std::isfinite(z1)) {
            const double tau = manipulators::intensityTransmissionCoefficient(z0, z1);
            minTransmission = std::min(minTransmission, tau);
            e["acoustic"] = {{"impedanceBelow", z0}, {"impedanceAbove", z1}, {"transmission", tau}, {"basis", b0 == b1 ? b0 : b0 + " / " + b1},
                             {"formula", "4 Z1 Z2 / (Z1 + Z2)^2 at normal incidence"}};
        }
        if (thermalStress.is_array())
            for (const json& t : thermalStress)
                if (t.value("below", std::string()) == s.layers[i].id) {
                    e["thermalMismatch"] = {{"deltaT", dT}, {"stress", t["mismatchStress"]}, {"basis", "classical lamination theory, the free laminate"}};
                    worstStress = std::max(worstStress, t["mismatchStress"].get<double>());
                    if (in)
                        if (const InterfaceModel* m = in->model("cohesive-zone"))
                            e["thermalMismatch"]["exceedsStrength"] = t["mismatchStress"].get<double>() > m->parameter("strength")->value;
                }
        if (in)
            if (const InterfaceModel* m = in->model("thermal-contact-resistance"); m && o.contains("heatFlux"))
                e["temperatureJump"] = o["heatFlux"].get<double>() * m->parameter("resistance")->value;
        if (in)
            if (const InterfaceModel* m = in->model("electrical-contact-resistance"); m && o.contains("currentDensity"))
                e["voltageDrop"] = o["currentDensity"].get<double>() * m->parameter("resistance")->value;
        list.push_back(e);
    }
    json metrics{{"minAcousticTransmission", minTransmission}};
    if (dT != 0.0)
        metrics["maxMismatchStress"] = worstStress;
    return json{{"metrics", metrics},
                {"result", {{"interfaces", list}}},
                {"assumptions", json::array({"normal incidence for the acoustic transmission", "the thermal mismatch of the free laminate (lamination theory)"})}};
}


// --- the three-dimensional layered solid (REQ-CHAT-20261128-004, ADR-000232) -------------------

double optionNumber(const json& o, const char* key, double fallback)
{
    return o.contains(key) && o[key].is_number() ? o[key].get<double>() : fallback;
}

Result<mechanics::BodyEdge> edgeOf(const json& v)
{
    const std::string e = v.is_string() ? v.get<std::string>() : std::string();
    if (e == "u0")
        return mechanics::BodyEdge::U0;
    if (e == "u1")
        return mechanics::BodyEdge::U1;
    if (e == "v0")
        return mechanics::BodyEdge::V0;
    if (e == "v1")
        return mechanics::BodyEdge::V1;
    return Result<mechanics::BodyEdge>::failure("an edge is one of u0, u1, v0, v1, not '" + e + "'");
}

Result<int> axisOf(const json& v)
{
    const std::string x = v.is_string() ? v.get<std::string>() : std::string();
    if (x == "x")
        return 0;
    if (x == "y")
        return 1;
    if (x == "z")
        return 2;
    return Result<int>::failure("an axis is one of x, y, z, not '" + x + "'");
}

Result<Vec3> vectorOf(const json& v)
{
    if (!v.is_array() || v.size() != 3 || !v[0].is_number() || !v[1].is_number() || !v[2].is_number())
        return Result<Vec3>::failure("a vector is three numbers");
    return Vec3(v[0].get<double>(), v[1].get<double>(), v[2].get<double>());
}

// The body of a stack: its layers (graded ones divided into sublayers), the
// interfaces it declares, and the reference surface of the options.
Result<mechanics::LayeredBody> bodyOf(const Architecture& a, const Stack& s, const json& o, const MaterialLibrary& lib, bool thermal, json& notes)
{
    using R = Result<mechanics::LayeredBody>;
    auto subs = subLayers(a, s, thermal ? std::set<std::string>{"thermal"} : std::set<std::string>{"mechanical"}, std::max(1, static_cast<int>(optionNumber(o, "sublayers", 4))), lib);
    if (!subs)
        return R::failure(subs.message());
    mechanics::LayeredBody body;
    body.name = s.id;
    const json surface = o.value("surface", json::object());
    const std::string kind = surface.value("kind", std::string("plane"));
    auto& rs = body.surface;
    if (kind == "plane") {
        rs.kind = mechanics::ReferenceSurface::Kind::Plane;
        rs.length = optionNumber(surface, "length", s.length);
        rs.width = optionNumber(surface, "width", s.width);
        if (!(rs.length > 0.0 && rs.width > 0.0))
            return R::failure("the stack '" + s.id + "' has no in-plane extent: give it a length and width, or a surface in the options");
    } else if (kind == "cylinder") {
        rs.kind = mechanics::ReferenceSurface::Kind::Cylinder;
        rs.radius = optionNumber(surface, "radius", 0.0);
        rs.length = optionNumber(surface, "length", s.length);
        rs.sweep = optionNumber(surface, "sweep", 2 * std::numbers::pi);
    } else if (kind == "sphere") {
        rs.kind = mechanics::ReferenceSurface::Kind::Sphere;
        rs.radius = optionNumber(surface, "radius", 0.0);
        rs.polarStart = optionNumber(surface, "polarStart", 0.0);
        rs.polarEnd = optionNumber(surface, "polarEnd", std::numbers::pi / 2);
        rs.sweep = optionNumber(surface, "sweep", 2 * std::numbers::pi);
    } else if (kind == "revolution") {
        rs.kind = mechanics::ReferenceSurface::Kind::Revolution;
        rs.sweep = optionNumber(surface, "sweep", 2 * std::numbers::pi);
        for (const auto& pt : surface.value("profile", json::array()))
            if (pt.is_array() && pt.size() == 2 && pt[0].is_number() && pt[1].is_number())
                rs.profile.push_back({pt[0].get<double>(), pt[1].get<double>()});
    } else {
        return R::failure("a surface is a plane, cylinder, sphere or revolution, not '" + kind + "'");
    }
    if (Status st = rs.validate(); !st)
        return R::failure(st.message());
    const json mesh = o.value("mesh", json::object());
    body.elementsU = static_cast<int>(optionNumber(mesh, "elementsU", 12));
    body.elementsV = static_cast<int>(optionNumber(mesh, "elementsV", 2));
    const int through = static_cast<int>(optionNumber(mesh, "elementsThrough", 2));
    const json debonded = o.value("debonded", json::object());
    const std::vector<Sub>& list = subs.value();
    for (size_t i = 0; i < list.size(); ++i) {
        const Sub& sub = list[i];
        mechanics::BodyLayer l;
        l.name = sub.id;
        l.thickness.start = sub.thickness;
        const bool split = (i > 0 && list[i - 1].layer == sub.layer) || (i + 1 < list.size() && list[i + 1].layer == sub.layer);
        l.elementsThrough = split ? 1 : std::max(1, through);
        l.material.base.name = sub.region;
        if (thermal) {
            if (std::abs(sub.kzz - sub.kxx) > 1e-9 * std::max(sub.kzz, sub.kxx))
                return R::failure("the layer '" + sub.id + "' conducts anisotropically: the three-dimensional conduction takes isotropic layers");
            l.material.conductivity = sub.kzz;
        } else {
            if (std::abs(sub.E1 - sub.E2) > 1e-9 * std::max(sub.E1, sub.E2))
                return R::failure("the layer '" + sub.id + "' is orthotropic in its plane: the three-dimensional solid needs nine orthotropic constants, "
                                  "which the architecture's in-plane elastic set does not give; refused");
            l.material.base.youngsModulus = sub.E1;
            l.material.base.poissonsRatio = sub.nu12;
            l.material.base.density = sub.rho;
            l.material.base.lossFactor = sub.loss;
            l.material.base.lossFactorStated = sub.loss > 0.0;
            l.material.base.provenance = sub.origins.empty() ? "the material library" : sub.origins.front();
        }
        body.layers.push_back(l);
        if (i == 0)
            continue;
        // The interface below this sublayer: the same layer (a graded layer's division),
        // the same region continuing, or the architecture's declared interface.
        mechanics::LayerInterface li;
        const Sub& prev = list[i - 1];
        if (prev.layer == sub.layer || s.layers[prev.layer].region == s.layers[sub.layer].region) {
            li.name = prev.layer == sub.layer ? "within " + s.layers[sub.layer].id : s.layers[prev.layer].id + "/" + s.layers[sub.layer].id;
            li.mechanical = mechanics::InterfaceMechanics::PerfectBonding;
            li.thermal = mechanics::InterfaceThermal::Perfect;
            li.provenance = "continuous material (one region), not an interface";
        } else {
            const Interface* in = interfaceBetweenLayers(a, s, prev.layer);
            if (!in)
                return R::failure("the layers '" + s.layers[prev.layer].id + "' and '" + s.layers[sub.layer].id + "' meet without an interface: nothing is assumed about it");
            li.name = in->id;
            li.provenance = "interface '" + in->id + "'";
            if (const InterfaceModel* m = in->modelFor("mechanical")) {
                const auto param = [&](const char* key) { return m->parameter(key) ? std::optional<double>(m->parameter(key)->value) : std::nullopt; };
                if (m->kind == "perfect" || m->kind == "perfect-bonding") {
                    li.mechanical = mechanics::InterfaceMechanics::PerfectBonding;
                } else if (m->kind == "finite-stiffness") {
                    li.mechanical = mechanics::InterfaceMechanics::FiniteStiffness;
                    li.normalStiffness = param("normalStiffness").value_or(0.0);
                    li.shearStiffness = param("shearStiffness").value_or(0.0);
                } else if (m->kind == "sliding") {
                    li.mechanical = mechanics::InterfaceMechanics::Sliding;
                    li.normalStiffness = param("normalStiffness").value_or(0.0);
                } else if (m->kind == "frictional-contact") {
                    li.mechanical = mechanics::InterfaceMechanics::FrictionalContact;
                    li.frictionCoefficient = param("frictionCoefficient").value_or(0.0);
                    li.normalStiffness = param("normalStiffness").value_or(0.0);
                    li.shearStiffness = param("shearStiffness").value_or(0.0);
                } else if (!thermal) {
                    return R::failure("the " + m->kind + " model of '" + in->id + "' is not represented by the three-dimensional solid");
                }
                li.normalStrength = param("normalStrength");
                li.shearStrength = param("shearStrength");
            }
            if (const InterfaceModel* m = in->modelFor("thermal")) {
                if (m->kind == "perfect")
                    li.thermal = mechanics::InterfaceThermal::Perfect;
                else if (m->kind == "thermal-contact-resistance" && m->parameter("resistance")) {
                    li.thermal = mechanics::InterfaceThermal::Resistance;
                    li.thermalResistance = m->parameter("resistance")->value;
                }
            }
            if (debonded.contains(in->id))
                for (const auto& box : debonded[in->id])
                    if (box.is_array() && box.size() == 4)
                        li.debonded.push_back({box[0].get<double>(), box[1].get<double>(), box[2].get<double>(), box[3].get<double>()});
            if (li.mechanical == mechanics::InterfaceMechanics::PerfectBonding && li.normalStrength)
                notes.push_back("the tractions at the perfectly bonded interface '" + in->id + "' are recovered from the stress of the layer below");
        }
        body.interfaces.push_back(li);
    }
    if (Status st = body.validate(); !st)
        return R::failure(st.message());
    return body;
}

Result<json> layeredSolidAnalysis(const Architecture& a, const Stack& s, const json& o, const MaterialLibrary& lib, bool thermal)
{
    using R = Result<json>;
    json notes = json::array();
    auto made = bodyOf(a, s, o, lib, thermal, notes);
    if (!made)
        return R::failure(made.message());
    const mechanics::LayeredBody& body = made.value();
    json result{{"surface", body.surface.describe()}, {"layers", body.layers.size()}};
    json metrics = json::object();
    json assumptions = json::array(), warnings = json::array();
    if (thermal) {
        const json t = o.value("thermal", json::object());
        mechanics::LayeredThermalBoundary bc;
        if (t.contains("innerTemperature"))
            bc.innerTemperature = t["innerTemperature"].get<double>();
        if (t.contains("outerTemperature"))
            bc.outerTemperature = t["outerTemperature"].get<double>();
        if (t.contains("innerFlux"))
            bc.innerFlux = t["innerFlux"].get<double>();
        if (t.contains("outerConvection") && t["outerConvection"].is_array() && t["outerConvection"].size() == 2)
            bc.outerConvection = std::array<double, 2>{t["outerConvection"][0].get<double>(), t["outerConvection"][1].get<double>()};
        if (!bc.innerTemperature && !bc.outerTemperature && !bc.outerConvection && !bc.innerFlux) {
            bc.innerTemperature = 373.15;  // the default case, stated
            bc.outerTemperature = 293.15;
            assumptions.push_back("no thermal condition given: the inner face at 373.15 K and the outer at 293.15 K");
        }
        auto r = mechanics::solveLayeredConduction(body, bc);
        if (!r)
            return R::failure(r.message());
        const auto& v = r.value();
        result["analysis"] = "steady conduction";
        result["fidelity"] = v.fidelity;
        result["heatIn"] = v.heatIn;
        result["innerArea"] = v.innerArea;
        json jumps = json::array();
        double worst = 0.0;
        for (size_t i = 0; i < body.interfaces.size(); ++i) {
            jumps.push_back({{"interface", body.interfaces[i].name}, {"jump", v.interfaceJump[i]}});
            worst = std::max(worst, std::abs(v.interfaceJump[i]));
        }
        result["interfaceJumps"] = jumps;
        json ranges = json::array();
        for (size_t l = 0; l < body.layers.size(); ++l)
            ranges.push_back({{"layer", body.layers[l].name}, {"min", v.layerRange[l][0]}, {"max", v.layerRange[l][1]}});
        result["layerTemperatures"] = ranges;
        metrics = {{"heatIn", v.heatIn}, {"heatFluxIn", v.innerArea > 0 ? v.heatIn / v.innerArea : 0.0}, {"maxInterfaceJump", worst}};
        for (const auto& x : v.assumptions)
            assumptions.push_back(x);
        for (const auto& x : v.warnings)
            warnings.push_back(x);
    } else {
        mechanics::LayeredSupports supports;
        const json sp = o.value("supports", json{{"clamped", json::array({"u0"})}});
        for (const auto& e : sp.value("clamped", json::array())) {
            auto edge = edgeOf(e);
            if (!edge)
                return R::failure(edge.message());
            supports.clamped.push_back(edge.value());
        }
        for (const auto& p : sp.value("symmetry", json::array())) {
            auto axis = axisOf(p.value("axis", json()));
            if (!axis)
                return R::failure(axis.message());
            supports.symmetry.push_back({axis.value(), p.value("value", 0.0)});
        }
        for (const auto& x : sp.value("heldAxes", json::array())) {
            auto axis = axisOf(x);
            if (!axis)
                return R::failure(axis.message());
            supports.heldAxes.push_back(axis.value());
        }
        supports.innerFaceClamped = sp.value("innerFaceClamped", false);
        const std::string kind = o.value("analysis", std::string("modal"));
        if (kind == "modal") {
            auto r = mechanics::solveLayeredModal(body, supports, static_cast<int>(optionNumber(o, "modes", 4)));
            if (!r)
                return R::failure(r.message());
            const auto& v = r.value();
            result["analysis"] = "free vibration";
            result["fidelity"] = v.fidelity;
            result["frequencies"] = v.frequencies;
            result["mesh"] = {{"elements", v.mesh.structure.elements.size()}, {"nodes", v.mesh.structure.nodes.size()}};
            result["mass"] = v.mesh.structure.totalMass();
            metrics = {{"mass", v.mesh.structure.totalMass()}};
            if (!v.frequencies.empty())
                metrics["f1"] = v.frequencies.front();
            for (const auto& x : v.assumptions)
                assumptions.push_back(x);
            for (const auto& x : v.warnings)
                warnings.push_back(x);
        } else if (kind == "static") {
            mechanics::LayeredLoads loads;
            const json ld = o.value("loads", json::object());
            loads.innerPressure = optionNumber(ld, "innerPressure", 0.0);
            loads.outerPressure = optionNumber(ld, "outerPressure", 0.0);
            if (ld.contains("outerTraction")) {
                auto t = vectorOf(ld["outerTraction"]);
                if (!t)
                    return R::failure("the outer traction: " + t.message());
                loads.outerTraction = t.value();
            }
            for (const auto& ef : ld.value("edgeForces", json::array())) {
                auto edge = edgeOf(ef.value("edge", json()));
                auto force = vectorOf(ef.value("force", json()));
                if (!edge || !force)
                    return R::failure("an edge force needs an edge (u0, u1, v0, v1) and a force (three numbers, N)");
                loads.edgeForces.emplace_back(edge.value(), force.value());
            }
            if (loads.innerPressure == 0.0 && loads.outerPressure == 0.0 && loads.outerTraction.norm() == 0.0 && loads.edgeForces.empty())
                return R::failure("a static analysis needs a load (options.loads: innerPressure, outerPressure, outerTraction or edgeForces)");
            auto r = mechanics::solveLayeredStatic(body, supports, loads);
            if (!r)
                return R::failure(r.message());
            const auto& v = r.value();
            result["analysis"] = "static";
            result["fidelity"] = v.fidelity;
            result["maxDisplacement"] = v.maxDisplacement;
            result["compliance"] = v.compliance;
            result["converged"] = v.converged;
            result["iterations"] = v.iterations;
            result["mesh"] = {{"elements", v.mesh.structure.elements.size()}, {"nodes", v.mesh.structure.nodes.size()}};
            json layers = json::array();
            double worstStress = 0.0;
            for (size_t l = 0; l < body.layers.size(); ++l) {
                layers.push_back({{"layer", body.layers[l].name}, {"maxVonMises", v.maxVonMises(static_cast<int>(l))}});
                worstStress = std::max(worstStress, v.maxVonMises(static_cast<int>(l)));
            }
            result["layerStresses"] = layers;
            json interfaces = json::array();
            std::optional<double> worstIndex;
            for (size_t i = 0; i < body.interfaces.size(); ++i) {
                double tn = 0.0, ts = 0.0;
                std::map<std::string, int> states;
                for (const auto& p : v.interfacePoints)
                    if (p.interface == static_cast<int>(i)) {
                        tn = std::max(tn, p.normalTraction);
                        ts = std::max(ts, p.shearTraction);
                        ++states[p.state];
                    }
                json entry{{"interface", body.interfaces[i].name}, {"maxNormalTraction", tn}, {"maxShearTraction", ts}, {"states", states}};
                if (const auto f = v.maxFailureIndex(static_cast<int>(i))) {
                    entry["maxFailureIndex"] = *f;
                    entry["initiation"] = *f >= 1.0;
                    worstIndex = std::max(worstIndex.value_or(0.0), *f);
                }
                interfaces.push_back(entry);
            }
            result["interfaces"] = interfaces;
            metrics = {{"maxDisplacement", v.maxDisplacement}, {"maxVonMises", worstStress}, {"compliance", v.compliance}};
            if (worstIndex)
                metrics["maxFailureIndex"] = *worstIndex;
            for (const auto& x : v.assumptions)
                assumptions.push_back(x);
            for (const auto& x : v.warnings)
                warnings.push_back(x);
        } else {
            return R::failure("the layered solid's analysis is 'modal' or 'static', not '" + kind + "'");
        }
    }
    for (const auto& n : notes)
        assumptions.push_back(n);
    return json{{"metrics", metrics}, {"result", result}, {"assumptions", assumptions}, {"warnings", warnings}};
}

}  // namespace

const std::vector<SolverCapability>& matterCapabilities()
{
    static const std::vector<std::string> mech{"mechanical.youngsModulus", "mechanical.poissonsRatio", "mechanical.density"};
    static const std::vector<SolverCapability> caps{
        {"laminate", "Classical lamination theory", "mechanics.laminate.clt", {"mechanical"},
         {"isotropic", "orthotropic-in-plane", "graded", "composition-effective"}, {"perfect", "perfect-bonding", "cohesive-zone"}, {"segmented"},
         "through the thickness of a plate", {"continuum", "mesoscale (layers)"}, {"thermal expansion (the free thermal response)"}, {},
         {"thin plies in plane stress; no transverse shear", "perfect bonding; no edge (peel, interlaminar shear) stresses", "linear elasticity",
          "a segmented layer is counted over the whole plate (its stiffness and mass are bounded from above)"},
         {"the stack's layers with E, nu, rho (or orthotropic sets) and their angles", "a temperature change (optional)"},
         {"A, B, D", "Ex, Ey, Gxy, nuxy", "the neutral surface and reduced bending stiffness", "the thermal curvature and ply stresses"}, mech},
        {"modes", "Beam and plate bending modes", "mechanics.laminate.modes", {"mechanical"},
         {"isotropic", "orthotropic-in-plane", "graded", "composition-effective"}, {"perfect", "perfect-bonding", "cohesive-zone"}, {"segmented"},
         "a beam or a rectangular plate", {"continuum", "device"}, {}, {},
         {"clamped-free Euler-Bernoulli beams or simply supported specially orthotropic plates", "slender structures, low modes",
          "a segmented layer is counted over the whole length (its stiffness and mass are bounded from above)"},
         {"the stack with its length and width", "the boundary"}, {"the coated and bare frequencies", "the frequency shift and added mass"}, mech},
        {"harvester", "Piezoelectric cantilever (lumped)", "mechanics.piezoelectric-beam", {"electromechanical"},
         {"isotropic", "orthotropic-in-plane", "piezoelectric", "graded", "composition-effective"}, {"perfect", "perfect-bonding", "cohesive-zone"},
         {"segmented"}, "a cantilevered beam (one mode)", {"device"}, {"mechanical to electrical (direct effect)", "electrical to mechanical (converse effect)"}, {},
         {"the first bending mode (Rayleigh, static deflection shape)", "a resistive load; equipotential electrodes", "graded transduction layers are not modeled",
          "a segmented electrode sets the collecting span; its stiffness and mass count over the whole length"},
         {"the stack with its length and width", "the piezoelectric sets (d31, d32, eps33T, poling)", "base acceleration, frequency, load"},
         {"power, voltage, efficiency", "short- and open-circuit frequencies", "the coupling", "the stage's energy balance"}, mech},
        {"thermal", "Through-thickness conduction", "thermal.series", {"thermal"},
         {"isotropic", "principal-conductivity", "graded", "composition-effective"}, {"perfect", "thermal-contact-resistance"}, {},
         "one-dimensional through the thickness", {"continuum", "mesoscale (layers)"}, {}, {},
         {"steady, one-dimensional, no lateral spreading or radiation"}, {"the stack's conductivities", "a heat flux or face temperatures (optional)"},
         {"the thermal resistance and effective conductivities", "the temperature profile with its jumps"}, {"thermal.thermalConductivity"}},
        {"electrical", "Through-thickness electrical", "electrical.series", {"electrical"},
         {"isotropic", "graded", "composition-effective"}, {"perfect", "electrical-contact-resistance"}, {"segmented"},
         "one-dimensional through the thickness", {"continuum"}, {}, {},
         {"ohmic layers; an ideal insulator has zero conductivity", "per unit area where every layer is present (a segmented layer's span)"},
         {"the stack's conductivities and permittivities"}, {"the series resistance and capacitance"}, {"electromagnetic.conductivity", "electromagnetic.relativePermittivity"}},
        {"electromagnetic", "Layered plane-wave solver", "em.layered.transfer-matrix", {"electromagnetic"},
         {"isotropic", "graded", "composition-effective"}, {"perfect"}, {}, "one-dimensional (normal incidence)", {"continuum"}, {}, {},
         {"normal incidence; laterally infinite, isotropic layers"}, {"the stack's permittivities, loss tangents, conductivities, permeabilities", "frequencies"},
         {"reflectance, transmittance, absorptance", "the absorption per layer"},
         {"electromagnetic.relativePermittivity", "electromagnetic.lossTangent", "electromagnetic.conductivity", "electromagnetic.relativePermeability"}},
        {"coupled-engine", "Coupled Energy-Matter Engine (layered slab)", "multiphysics.layered-slab", {"thermal", "electromagnetic"},
         {"isotropic", "principal-conductivity", "graded", "composition-effective"}, {"perfect", "thermal-contact-resistance"}, {},
         "a one-dimensional layered slab", {"continuum", "device"}, {"electromagnetic absorption to heat", "thermal expansion"}, {"temperature-dependent laws"},
         {"the layered-slab geometry of the engine", "electromagnetic interfaces perfect", "anisotropy through the through-thickness conductivity only"},
         {"the stack as regions", "the domains and couplings of the engine"}, {"the coupled result and its ledger"},
         {"thermal.thermalConductivity", "thermal.density", "thermal.specificHeat"}},
        {"layered-solid", "Three-dimensional layered solid", "mechanics.layered-solid.hex20", {"mechanical"},
         {"isotropic", "graded", "composition-effective"}, {"perfect", "perfect-bonding", "finite-stiffness", "sliding", "frictional-contact"}, {},
         "three-dimensional: a flat, cylindrical, spherical or revolved stack", {"continuum", "mesoscale (layers)", "device"}, {},
         {"frictional contact (Coulomb, holonomic)"},
         {"linear elasticity, small displacements (larger ones are flagged)", "isotropic layers: an in-plane orthotropic set does not give the nine 3D constants",
          "a graded layer by sublayers of constant properties", "debonded regions traction-free, without contact; no damage evolution (cohesive zones refused)",
          "the body is built in its own frame on the options' reference surface (default: the stack's length and width)"},
         {"the stack's layers with E, nu, rho", "options: surface, mesh, supports, analysis (modal or static), loads, debonded regions"},
         {"frequencies (modal)", "displacements, layer stresses, interface tractions, states and initiation indices (static)"}, mech},
        {"layered-solid-thermal", "Three-dimensional layered conduction", "thermal.layered-solid.hex20", {"thermal"},
         {"isotropic", "graded", "composition-effective"}, {"perfect", "thermal-contact-resistance"}, {},
         "three-dimensional: a flat, cylindrical, spherical or revolved stack", {"continuum", "mesoscale (layers)", "device"}, {}, {},
         {"steady conduction, isotropic layers, no radiation", "faces without a condition insulated", "debonded regions not represented thermally"},
         {"the stack's conductivities", "options: surface, mesh, thermal (inner and outer temperatures, inner flux, outer convection)"},
         {"the heat flow, interface temperature jumps and layer temperature ranges"}, {"thermal.thermalConductivity"}},
    };
    return caps;
}

const SolverCapability* findMatterCapability(const std::string& id)
{
    for (const auto& c : matterCapabilities())
        if (c.id == id)
            return &c;
    return nullptr;
}

Result<Compatibility> checkMatterStack(const Architecture& a, const std::string& stackId, const std::string& analysis, const MaterialLibrary& library)
{
    const Stack* s = a.stack(stackId);
    if (!s)
        return Result<Compatibility>::failure("the architecture has no stack '" + stackId + "'");
    if (analysis == "interfaces")
        return Compatibility{};
    const SolverCapability* cap = findMatterCapability(analysis);
    if (!cap)
        return Result<Compatibility>::failure("no analysis '" + analysis + "'");
    return checkStack(a, *s, *cap, library);
}

Result<json> analyzeStack(const Architecture& a, const std::string& stackId, const std::string& analysis, const json& options, const MaterialLibrary& library)
{
    const Stack* s = a.stack(stackId);
    if (!s)
        return Result<json>::failure("the architecture has no stack '" + stackId + "'");
    auto compat = checkMatterStack(a, stackId, analysis, library);
    if (!compat)
        return Result<json>::failure(compat.message());
    if (!compat.value().compatible)
        return Result<json>::failure("refused by " + (findMatterCapability(analysis) ? findMatterCapability(analysis)->title : analysis) + ":\n" +
                                     compat.value().summary());
    const json o = options.is_object() ? options : json::object();
    Result<json> r = Result<json>::failure("no analysis '" + analysis + "'");
    if (analysis == "laminate")
        r = laminateAnalysis(a, *s, o, library);
    else if (analysis == "modes")
        r = modesAnalysis(a, *s, o, library);
    else if (analysis == "harvester")
        r = harvesterAnalysis(a, *s, o, library);
    else if (analysis == "thermal")
        r = thermalAnalysis(a, *s, o, library);
    else if (analysis == "electrical")
        r = electricalAnalysis(a, *s, o, library);
    else if (analysis == "electromagnetic")
        r = electromagneticAnalysis(a, *s, o, library);
    else if (analysis == "interfaces")
        r = interfacesAnalysis(a, *s, o, library);
    else if (analysis == "layered-solid")
        r = layeredSolidAnalysis(a, *s, o, library, false);
    else if (analysis == "layered-solid-thermal")
        r = layeredSolidAnalysis(a, *s, o, library, true);
    if (!r)
        return r;
    json out = r.value();
    out["analysis"] = analysis;
    out["architecture"] = a.id;
    out["revision"] = revision(a);
    out["stack"] = stackId;
    out["options"] = o;
    out["representation"] = representationOf(a);
    out["compatibility"] = compat.value().toJson();
    if (!out.contains("warnings"))
        out["warnings"] = json::array();
    for (const auto& i : compat.value().issues)
        if (i.severity == "warning")
            out["warnings"].push_back(i.message);
    if (analysis == "laminate" || analysis == "modes" || analysis == "harvester")
        for (const auto& l : s->layers)
            if (l.pattern.kind == "segmented")
                out["warnings"].push_back("the segmented layer '" + l.id + "' counts with its stiffness and mass over the whole length (" +
                                          number(100.0 * l.pattern.coverage()) + " % covered): an upper bound for this layer");
    if (analysis == "electrical")
        for (const auto& l : s->layers)
            if (l.pattern.kind == "segmented")
                out["warnings"].push_back("per unit area where the segmented layer '" + l.id + "' is present");
    if (a.representation != "as-built")
        out["warnings"].push_back("a prediction of a design, not a validated result: compare it with measurements before relying on it");
    return out;
}

const std::vector<MatterMetric>& matterMetrics(const std::string& analysis)
{
    static const std::map<std::string, std::vector<MatterMetric>> metrics{
        {"layered-solid",
         {{"f1", "First frequency (3D)", "Hz", "the lowest mode of the 20-node model with the stated supports (modal)"},
          {"mass", "Mass", "kg", "the integral of rho over the meshed body"},
          {"maxDisplacement", "Largest displacement", "m", "max |u| over the nodes (static)"},
          {"maxVonMises", "Largest von Mises stress", "Pa", "at element centres, over all layers (static)"},
          {"compliance", "Compliance", "J", "f^T u of the applied loads (static)"},
          {"maxFailureIndex", "Largest interface initiation index", "1", "(max(t_n, 0) / N)^2 + (t_s / S)^2 at interface points with both strengths (static)"}}},
        {"layered-solid-thermal",
         {{"heatIn", "Heat flow in", "W", "through the inner face"},
          {"heatFluxIn", "Mean heat flux in", "W/m^2", "the heat flow over the inner face's area"},
          {"maxInterfaceJump", "Largest interface temperature jump", "K", "the mean jump across each interface, largest in magnitude"}}},
        {"laminate",
         {{"Ex", "In-plane modulus Ex", "Pa", "1 / (h a11)"},
          {"Ey", "In-plane modulus Ey", "Pa", "1 / (h a22)"},
          {"Gxy", "In-plane shear modulus", "Pa", "1 / (h a66)"},
          {"nuxy", "Poisson's ratio nu_xy", "1", "-a12 / a11"},
          {"bendingStiffness", "Bending stiffness D*11", "N m", "(D - B A^-1 B)11"},
          {"massPerArea", "Mass per area", "kg/m^2", "sum rho h"},
          {"thickness", "Thickness", "m", "sum h"},
          {"curvature", "Thermal curvature", "1/m", "kappa_x of the free laminate"},
          {"maxInterfaceStress", "Largest mismatch stress at an interface", "Pa", "in-plane ply stress next to an interface"}}},
        {"modes",
         {{"f1", "First frequency", "Hz", "coated"},
          {"f1Bare", "First frequency of the bare structure", "Hz", "the substrate only"},
          {"f1Shift", "Relative frequency shift", "1", "f1 / f1Bare - 1"},
          {"f2", "Second frequency", "Hz", "coated"},
          {"massPerArea", "Mass per area", "kg/m^2", "coated"},
          {"massAdded", "Added mass per area", "kg/m^2", "coated - bare"}}},
        {"harvester",
         {{"power", "Electrical power", "W", "|V|^2 / (2 R)"},
          {"voltage", "Voltage amplitude", "V", "|V|"},
          {"efficiency", "Conversion efficiency", "1", "electrical / input power"},
          {"inputPower", "Input power", "W", "from the base"},
          {"dissipatedPower", "Dissipated power", "W", "mechanical damping"},
          {"tipDisplacement", "Tip displacement", "m", "relative to the base"},
          {"shortCircuitHz", "Short-circuit frequency", "Hz", "sqrt(k / m) / 2 pi"},
          {"openCircuitHz", "Open-circuit frequency", "Hz", "sqrt((k + theta^2 / C) / m) / 2 pi"},
          {"couplingSquared", "Coupling theta^2 / (k C)", "1", ""},
          {"load", "Load", "ohm", ""},
          {"massPerArea", "Mass per area", "kg/m^2", "sum rho h"},
          {"actuatorDisplacement", "Actuated tip displacement", "m", "converse effect"}}},
        {"thermal",
         {{"resistance", "Thermal resistance", "m^2 K/W", "sum h / k + sum R_c"},
          {"effectiveConductivity", "Effective through-thickness conductivity", "W/(m K)", "H / R"},
          {"inPlaneConductivity", "In-plane conductivity", "W/(m K)", "sum k h / H"},
          {"interfaceShare", "Share of the interfaces", "1", "sum R_c / R"},
          {"temperatureDrop", "Temperature drop", "K", "q R"},
          {"heatFlux", "Heat flux", "W/m^2", "(T_bottom - T_top) / R"}}},
        {"electrical",
         {{"seriesResistance", "Series resistance", "ohm m^2", "sum h / sigma + sum R_c"},
          {"capacitancePerArea", "Capacitance per area", "F/m^2", "1 / sum (h / eps)"},
          {"timeConstant", "Time constant", "s", "R C"},
          {"leakageCurrentDensity", "Leakage current density", "A/m^2", "V / R"}}},
        {"electromagnetic",
         {{"absorptance", "Absorptance", "1", "at the first frequency"},
          {"reflectance", "Reflectance", "1", "at the first frequency"},
          {"transmittance", "Transmittance", "1", "at the first frequency"},
          {"maxAbsorptance", "Largest absorptance", "1", "over the frequencies"},
          {"frequencyOfMaxAbsorptance", "Frequency of the largest absorptance", "Hz", ""}}},
        {"interfaces",
         {{"minAcousticTransmission", "Smallest acoustic transmission", "1", "4 Z1 Z2 / (Z1 + Z2)^2"},
          {"maxMismatchStress", "Largest thermal mismatch stress", "Pa", "lamination theory"}}},
    };
    static const std::vector<MatterMetric> none;
    auto it = metrics.find(analysis);
    return it == metrics.end() ? none : it->second;
}

json effectiveRegionProperties(const Architecture& a, const std::string& regionId, const MaterialLibrary& library)
{
    const FunctionalRegion* r = a.region(regionId);
    if (!r)
        return json(nullptr);
    json props = json::array();
    for (PhysicsDomain d : {PhysicsDomain::Mechanical, PhysicsDomain::Thermal, PhysicsDomain::Electromagnetic, PhysicsDomain::Acoustic})
        for (const auto& def : propertyDefinitions(d)) {
            const std::string key = std::string(toString(d)) + "." + def.key;
            props.push_back(resolve(a, *r, key, 0.5, library).toJson());
        }
    json aniso = json::object();
    for (const auto& [kind, set] : r->anisotropic) {
        json values = json::object();
        for (const auto& [k, v] : set.values)
            values[k] = v.toJson();
        aniso[kind] = {{"values", values}, {"poling", set.poling}};
    }
    // Every method for a composition, so that the choice is visible.
    json methods = json::array();
    if (r->composition.constituents.size() > 1)
        for (const char* key : {"thermal.thermalConductivity", "mechanical.youngsModulus", "electromagnetic.relativePermittivity"}) {
            for (const auto& m : methodsFor(propertyKindOf(key))) {
                FunctionalRegion copy = *r;
                copy.properties.clear();
                copy.composition.method = m;
                const ResolvedProperty p = resolve(a, copy, key, 0.5, library);
                if (p.effective)
                    methods.push_back(p.effective->toJson());
            }
        }
    return {{"region", regionId}, {"properties", props}, {"anisotropic", aniso}, {"methods", methods}};
}

std::vector<json> processResidualStresses(const Architecture& a, const MaterialLibrary& library, double reference)
{
    std::vector<json> out;
    for (const auto& s : a.stacks)
        for (size_t i = 1; i < s.layers.size(); ++i) {
            const FunctionalRegion* r = a.region(s.layers[i].region);
            if (!r || r->process.empty() || s.layers[i - 1].region == r->id)
                continue;
            const Process* p = a.process(r->process);
            if (!p || !p->constraints.contains("temperature") || !p->constraints["temperature"].is_number())
                continue;
            const double t = p->constraints["temperature"].get<double>();
            // The layers made so far, stress-free at the process temperature, cooled to the reference.
            Stack made = s;
            made.layers.resize(i + 1);
            const Interface* in = a.interfaceBetween(s.layers[i - 1].region, r->id);
            auto subs = subLayers(a, made, {"mechanical", "expansion"}, 20, library);
            if (!subs) {
                out.push_back({{"interface", in ? in->id : std::string()}, {"layer", s.layers[i].id}, {"note", "not computed: " + subs.message()}});
                continue;
            }
            auto l = mechanics::laminate(pliesOf(subs.value()));
            if (!l)
                continue;
            const mechanics::ThermalResponse tr = mechanics::thermalResponse(l.value(), reference - t);
            const auto ranges = layerRanges(subs.value(), made.layers.size());
            const double stress = std::max(std::abs(tr.stress[ranges[i - 1].second - 1].top(0)), std::abs(tr.stress[ranges[i].first].bottom(0)));
            out.push_back({{"interface", in ? in->id : std::string()},
                           {"layer", s.layers[i].id},
                           {"stress", stress},
                           {"temperature", t},
                           {"basis", "lamination theory: the layers made so far stress-free at " + number(t) + " K, cooled to " + number(reference) + " K"}});
        }
    return out;
}

}  // namespace resonant::coupling
