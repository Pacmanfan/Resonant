#include "matter/Mixing.h"

#include <algorithm>
#include <cmath>

namespace resonant::matter {

using nlohmann::json;

namespace {

constexpr double kInfinity = std::numeric_limits<double>::infinity();

// 1 / sum f_i / (v_i + shift) - shift, with a zero-valued phase giving 0.
double harmonic(const std::vector<PhaseProperty>& phases, double shift, double PhaseProperty::*field)
{
    double sum = 0.0;
    for (const auto& p : phases) {
        if (p.fraction <= 0.0)
            continue;
        const double d = p.*field + shift;
        if (d <= 0.0)
            return 0.0 - shift;  // an infinite sum: the reciprocal vanishes
        sum += p.fraction / d;
    }
    return 1.0 / sum - shift;
}

std::vector<PhaseProperty> withModuli(const std::vector<PhaseProperty>& phases)
{
    // value -> bulk modulus in .value, shear modulus in .poisson (internal use only).
    std::vector<PhaseProperty> out = phases;
    for (auto& p : out) {
        const ElasticModuli m = elasticFrom(p.value, p.poisson);
        p.value = m.bulk;
        p.poisson = m.shear;
    }
    return out;
}

ElasticModuli walpole(const std::vector<PhaseProperty>& phases, bool upper)
{
    const auto km = withModuli(phases);
    double kStar = upper ? 0.0 : kInfinity, gStar = upper ? 0.0 : kInfinity;
    for (const auto& p : km) {
        if (p.fraction <= 0.0)
            continue;
        kStar = upper ? std::max(kStar, p.value) : std::min(kStar, p.value);
        gStar = upper ? std::max(gStar, p.poisson) : std::min(gStar, p.poisson);
    }
    ElasticModuli out;
    out.bulk = harmonic(km, 4.0 * gStar / 3.0, &PhaseProperty::value);
    const double zeta = kStar + 2.0 * gStar > 0.0 ? gStar / 6.0 * (9.0 * kStar + 8.0 * gStar) / (kStar + 2.0 * gStar) : 0.0;
    out.shear = harmonic(km, zeta, &PhaseProperty::poisson);
    return out;
}

}  // namespace

PropertyKind propertyKindOf(std::string_view key)
{
    if (key == "mechanical.density" || key == "thermal.density" || key == "acoustic.density")
        return PropertyKind::Density;
    if (key == "thermal.specificHeat")
        return PropertyKind::SpecificHeat;
    if (key == "thermal.thermalConductivity" || key == "electromagnetic.conductivity" || key == "electromagnetic.relativePermittivity" ||
        key == "electromagnetic.relativePermeability")
        return PropertyKind::Transport;
    if (key == "mechanical.youngsModulus" || key == "mechanical.poissonsRatio")
        return PropertyKind::Elastic;
    if (key == "thermal.thermalExpansion")
        return PropertyKind::Expansion;
    return PropertyKind::Other;
}

std::string_view toString(PropertyKind kind)
{
    switch (kind) {
    case PropertyKind::Density: return "density";
    case PropertyKind::SpecificHeat: return "specific-heat";
    case PropertyKind::Transport: return "transport";
    case PropertyKind::Elastic: return "elastic";
    case PropertyKind::Expansion: return "expansion";
    case PropertyKind::Other: return "other";
    }
    return "other";
}

std::vector<std::string> methodsFor(PropertyKind kind)
{
    switch (kind) {
    case PropertyKind::Density: return {"volume-average"};
    case PropertyKind::SpecificHeat: return {"mass-average"};
    case PropertyKind::Transport: return {"hashin-shtrikman-lower", "hashin-shtrikman-upper", "parallel", "series", "maxwell-garnett", "bruggeman"};
    case PropertyKind::Elastic: return {"hashin-shtrikman-lower", "hashin-shtrikman-upper", "voigt", "reuss", "gibson-ashby-open-cell"};
    case PropertyKind::Expansion: return {"turner"};
    case PropertyKind::Other: return {};
    }
    return {};
}

double ElasticModuli::youngs() const
{
    return 3.0 * bulk + shear > 0.0 ? 9.0 * bulk * shear / (3.0 * bulk + shear) : 0.0;
}

double ElasticModuli::poisson() const
{
    return 3.0 * bulk + shear > 0.0 ? (3.0 * bulk - 2.0 * shear) / (2.0 * (3.0 * bulk + shear)) : 0.0;
}

ElasticModuli elasticFrom(double youngs, double poisson)
{
    if (youngs <= 0.0)
        return {0.0, 0.0};
    return {youngs / (3.0 * (1.0 - 2.0 * poisson)), youngs / (2.0 * (1.0 + poisson))};
}

std::pair<double, double> wienerBounds(const std::vector<PhaseProperty>& phases)
{
    double parallel = 0.0;
    for (const auto& p : phases)
        parallel += p.fraction * p.value;
    return {harmonic(phases, 0.0, &PhaseProperty::value), parallel};
}

std::pair<double, double> hashinShtrikmanBounds(const std::vector<PhaseProperty>& phases)
{
    double lo = kInfinity, hi = 0.0;
    for (const auto& p : phases)
        if (p.fraction > 0.0) {
            lo = std::min(lo, p.value);
            hi = std::max(hi, p.value);
        }
    return {harmonic(phases, 2.0 * lo, &PhaseProperty::value), harmonic(phases, 2.0 * hi, &PhaseProperty::value)};
}

double maxwellGarnett(const std::vector<PhaseProperty>& phases, int matrix)
{
    const double em = phases[static_cast<size_t>(matrix)].value;
    double s = 0.0;
    for (size_t i = 0; i < phases.size(); ++i)
        if (static_cast<int>(i) != matrix)
            s += phases[i].fraction * (phases[i].value - em) / (phases[i].value + 2.0 * em);
    return em * (1.0 + 2.0 * s) / (1.0 - s);
}

double bruggeman(const std::vector<PhaseProperty>& phases)
{
    // sum f_i (v_i - v) / (v_i + 2 v) = 0: decreasing in v on (0, inf); the root lies in the Wiener bounds.
    auto g = [&](double v) {
        double s = 0.0;
        for (const auto& p : phases)
            s += p.fraction * (p.value - v) / (p.value + 2.0 * v);
        return s;
    };
    const auto [lo0, hi0] = wienerBounds(phases);
    double lo = std::max(lo0, 1e-300), hi = std::max(hi0, lo * 2.0);
    if (g(lo) <= 0.0)
        return lo0;
    for (int i = 0; i < 200 && hi - lo > 1e-15 * hi; ++i) {
        const double mid = std::sqrt(lo * hi) > 0.0 ? 0.5 * (lo + hi) : 0.5 * (lo + hi);
        (g(mid) > 0.0 ? lo : hi) = mid;
    }
    return 0.5 * (lo + hi);
}

std::pair<ElasticModuli, ElasticModuli> walpoleBounds(const std::vector<PhaseProperty>& phases)
{
    return {walpole(phases, false), walpole(phases, true)};
}

json EffectiveEstimate::toJson() const
{
    auto num = [](double v) { return std::isfinite(v) ? json(v) : json(nullptr); };
    return {{"key", key},
            {"method", method},
            {"value", num(value)},
            {"poisson", num(poisson)},
            {"lower", num(lower)},
            {"upper", num(upper)},
            {"bounds", bounds},
            {"absoluteLower", num(absoluteLower)},
            {"absoluteUpper", num(absoluteUpper)},
            {"absoluteBounds", absoluteBounds},
            {"exact", exact},
            {"valid", valid},
            {"validity", validity},
            {"assumptions", assumptions}};
}

Result<EffectiveEstimate> combine(std::string_view key, const std::vector<PhaseProperty>& phases, std::string_view method, int matrix)
{
    using R = Result<EffectiveEstimate>;
    const PropertyKind kind = propertyKindOf(key);
    if (phases.empty())
        return R::failure("a combination needs at least one phase");
    double sum = 0.0;
    for (const auto& p : phases) {
        if (!(p.fraction >= 0.0) || !std::isfinite(p.value) || p.value < 0.0)
            return R::failure("the phase '" + p.id + "' has an invalid fraction or value");
        sum += p.fraction;
    }
    if (std::abs(sum - 1.0) > 1e-6)
        return R::failure("the volume fractions sum to " + std::to_string(sum) + ", not 1");
    const auto methods = methodsFor(kind);
    if (methods.empty())
        return R::failure("no estimator combines '" + std::string(key) +
                          "': it depends on the microstructure, so it must be measured or derived (multiscale) for this composition");
    if (std::find(methods.begin(), methods.end(), method) == methods.end()) {
        std::string list;
        for (const auto& m : methods)
            list += (list.empty() ? "" : ", ") + m;
        return R::failure("the method '" + std::string(method) + "' does not apply to " + std::string(toString(kind)) + " properties (" + list + ")");
    }
    if ((method == "maxwell-garnett" || method == "gibson-ashby-open-cell") && (matrix < 0 || matrix >= static_cast<int>(phases.size())))
        return R::failure("the method '" + std::string(method) + "' needs the matrix (the continuous phase)");
    EffectiveEstimate e;
    e.key = std::string(key);
    e.method = std::string(method);
    if (kind == PropertyKind::Density) {
        double v = 0.0;
        for (const auto& p : phases)
            v += p.fraction * p.value;
        e.value = e.lower = e.upper = v;
        e.exact = true;
        e.bounds = "exact";
        e.validity = "exact for any arrangement (volume fractions)";
        return e;
    }
    if (kind == PropertyKind::SpecificHeat) {
        double m = 0.0, c = 0.0;
        for (const auto& p : phases) {
            if (!(p.density > 0.0) && p.fraction > 0.0)
                return R::failure("the mass-weighted specific heat needs the density of '" + p.id + "'");
            m += p.fraction * p.density;
            c += p.fraction * p.density * p.value;
        }
        e.value = e.lower = e.upper = c / m;
        e.exact = true;
        e.bounds = "exact";
        e.validity = "exact for a mechanical mixture (no excess heat capacity of mixing)";
        return e;
    }
    if (kind == PropertyKind::Expansion) {
        double num = 0.0, den = 0.0, lo = kInfinity, hi = -kInfinity;
        for (const auto& p : phases) {
            if (!(p.bulkModulus > 0.0) && p.fraction > 0.0)
                return R::failure("the Turner estimate needs the bulk modulus of '" + p.id + "'");
            num += p.fraction * p.bulkModulus * p.value;
            den += p.fraction * p.bulkModulus;
            if (p.fraction > 0.0) {
                lo = std::min(lo, p.value);
                hi = std::max(hi, p.value);
            }
        }
        e.value = num / den;
        e.lower = lo;
        e.upper = hi;
        e.bounds = "the constituents' range";
        e.validity = "an estimate: every phase under the same hydrostatic stress";
        e.assumptions = {"no shear coupling between the phases (Kerner and Schapery refine it)"};
        return e;
    }
    if (kind == PropertyKind::Transport) {
        const auto [wl, wu] = wienerBounds(phases);
        const auto [hl, hu] = hashinShtrikmanBounds(phases);
        e.lower = hl;
        e.upper = hu;
        e.bounds = "Hashin-Shtrikman (a statistically isotropic mixture)";
        e.absoluteLower = wl;
        e.absoluteUpper = wu;
        e.absoluteBounds = "Wiener (series and parallel: any microstructure)";
        if (method == "hashin-shtrikman-lower") {
            e.value = hl;
            e.validity = "a lower bound for an isotropic mixture, attained when the least conducting phase coats the others";
        } else if (method == "hashin-shtrikman-upper") {
            e.value = hu;
            e.validity = "an upper bound for an isotropic mixture, attained when the most conducting phase coats the others";
        } else if (method == "parallel") {
            e.value = wu;
            e.validity = "exact for phases continuous along the flux (layers parallel to it); an upper bound otherwise";
        } else if (method == "series") {
            e.value = wl;
            e.validity = "exact for phases in series along the flux (layers across it); a lower bound otherwise";
        } else if (method == "maxwell-garnett") {
            e.value = maxwellGarnett(phases, matrix);
            const double inclusions = 1.0 - phases[static_cast<size_t>(matrix)].fraction;
            e.valid = inclusions <= 0.3;
            e.validity = std::string("dilute, well-separated spherical inclusions in a continuous matrix (inclusions ") + std::to_string(inclusions) +
                         (e.valid ? ", within the dilute range up to 0.3)" : ", beyond the dilute range of 0.3: not valid)");
            e.assumptions = {"spherical inclusions", "no interaction between inclusions", "perfect interfaces"};
        } else {
            e.value = bruggeman(phases);
            e.validity = "a random mixture in which no phase is the matrix; it has a percolation threshold at a fraction of 1/3";
            e.assumptions = {"spherical grains", "perfect interfaces"};
        }
        if (method != "maxwell-garnett" && method != "bruggeman")
            e.assumptions = {"perfect interfaces (no contact resistance between the phases)"};
        return e;
    }
    // Elastic: Young's modulus and Poisson's ratio through the bulk and shear moduli.
    const auto km = withModuli(phases);
    ElasticModuli voigt, reuss;
    for (const auto& p : km) {
        voigt.bulk += p.fraction * p.value;
        voigt.shear += p.fraction * p.poisson;
    }
    reuss.bulk = harmonic(km, 0.0, &PhaseProperty::value);
    reuss.shear = harmonic(km, 0.0, &PhaseProperty::poisson);
    const auto [hsl, hsu] = walpoleBounds(phases);
    e.lower = hsl.youngs();
    e.upper = hsu.youngs();
    e.bounds = "Hashin-Shtrikman-Walpole (a statistically isotropic mixture)";
    e.absoluteLower = reuss.youngs();
    e.absoluteUpper = voigt.youngs();
    e.absoluteBounds = "Reuss and Voigt (uniform stress and uniform strain)";
    ElasticModuli chosen;
    if (method == "hashin-shtrikman-lower") {
        chosen = hsl;
        e.validity = "a lower bound for an isotropic mixture";
    } else if (method == "hashin-shtrikman-upper") {
        chosen = hsu;
        e.validity = "an upper bound for an isotropic mixture";
    } else if (method == "voigt") {
        chosen = voigt;
        e.validity = "the uniform-strain bound: exact along continuous fibres or layers, an upper bound otherwise";
    } else if (method == "reuss") {
        chosen = reuss;
        e.validity = "the uniform-stress bound: exact across layers, a lower bound otherwise";
    } else {
        // Gibson-Ashby, open cells: E* = C (rho*/rho_s)^2 E_s with C = 1, nu* = 1/3
        // (Gibson & Ashby, Cellular Solids, 2nd ed., 1997: an empirical scaling of bending-dominated foams).
        const PhaseProperty& solid = phases[static_cast<size_t>(matrix)];
        for (size_t i = 0; i < phases.size(); ++i)
            if (static_cast<int>(i) != matrix && phases[i].value > 0.0 && phases[i].fraction > 0.0)
                return R::failure("the Gibson-Ashby scaling describes a solid with pores: every other constituent must be void");
        const double rel = solid.fraction;
        e.value = solid.value * rel * rel;
        e.poisson = 1.0 / 3.0;
        e.valid = rel <= 0.3;
        e.validity = std::string("open-cell, bending-dominated foams at a relative density ") + std::to_string(rel) +
                     (e.valid ? " (within the range up to 0.3)" : " (beyond 0.3: the scaling does not apply)");
        e.assumptions = {"an empirical scaling with C = 1 and nu = 1/3 (Gibson and Ashby 1997, literature)", "isotropic cells", "no strut buckling"};
        return e;
    }
    e.value = chosen.youngs();
    e.poisson = chosen.poisson();
    e.assumptions = {"perfect interfaces between the phases"};
    return e;
}

}  // namespace resonant::matter
