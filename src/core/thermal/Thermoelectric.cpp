#include "thermal/Thermoelectric.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace resonant::thermal {
namespace {

std::string num(double v)
{
    char b[64];
    std::snprintf(b, sizeof b, "%.6g", v);
    return b;
}

// The integral of p(T) from a to b.
double integral(const TemperatureFunction& f, double a, double b)
{
    double sum = 0.0;
    for (size_t i = 0; i < f.coefficients.size(); ++i)
        sum += f.coefficients[i] * (std::pow(b, static_cast<double>(i + 1)) - std::pow(a, static_cast<double>(i + 1))) / static_cast<double>(i + 1);
    return sum;
}

struct LegSolution {
    std::vector<double> t;
    double heatIntoCold = 0.0;  // W leaving the leg at x = 0 (into the cold junction)
    double heatFromHot = 0.0;   // W entering the leg at x = L (from the hot junction)
    double resistance = 0.0;    // ohm
    double joule = 0.0, thomson = 0.0;  // W
    int iterations = 0;
};

// One leg, x = 0 cold to x = L hot, current density j along +x.
Result<LegSolution> solveLeg(const ThermoelectricLeg& leg, double j, double cold, double hot, int n, const std::vector<double>& start)
{
    using R = Result<LegSolution>;
    LegSolution out;
    const double h = leg.length / (n - 1);
    std::vector<double>& t = out.t;
    t = start.size() == static_cast<size_t>(n) ? start : std::vector<double>(static_cast<size_t>(n));
    if (start.size() != static_cast<size_t>(n))
        for (int i = 0; i < n; ++i)
            t[static_cast<size_t>(i)] = cold + (hot - cold) * i / (n - 1);
    t.front() = cold;
    t.back() = hot;
    std::vector<double> a(static_cast<size_t>(n)), b(static_cast<size_t>(n)), c(static_cast<size_t>(n)), d(static_cast<size_t>(n));
    for (int it = 1; it <= 200; ++it) {
        out.iterations = it;
        for (int i = 1; i < n - 1; ++i) {
            const auto k = [&](int m) { return leg.conductivity.at(t[static_cast<size_t>(m)]); };
            const double km = 0.5 * (k(i - 1) + k(i)), kp = 0.5 * (k(i) + k(i + 1));
            if (!(km > 0.0 && kp > 0.0))
                return R::failure("the leg '" + leg.name + "' has a non-positive thermal conductivity in its temperature range");
            const double tau = t[static_cast<size_t>(i)] * leg.seebeck.derivative(t[static_cast<size_t>(i)]);  // the Thomson coefficient
            const double rho = leg.resistivity.at(t[static_cast<size_t>(i)]);
            if (!(rho > 0.0))
                return R::failure("the leg '" + leg.name + "' has a non-positive resistivity in its temperature range");
            a[static_cast<size_t>(i)] = km / (h * h) + tau * j / (2 * h);
            c[static_cast<size_t>(i)] = kp / (h * h) - tau * j / (2 * h);
            b[static_cast<size_t>(i)] = -(km + kp) / (h * h);
            d[static_cast<size_t>(i)] = -rho * j * j;
        }
        // Dirichlet ends; the Thomas algorithm on the interior.
        std::vector<double> next = t;
        d[1] -= a[1] * cold;
        d[static_cast<size_t>(n - 2)] -= c[static_cast<size_t>(n - 2)] * hot;
        std::vector<double> cp(static_cast<size_t>(n)), dp(static_cast<size_t>(n));
        cp[1] = c[1] / b[1];
        dp[1] = d[1] / b[1];
        for (int i = 2; i < n - 1; ++i) {
            const double m = b[static_cast<size_t>(i)] - a[static_cast<size_t>(i)] * cp[static_cast<size_t>(i - 1)];
            cp[static_cast<size_t>(i)] = c[static_cast<size_t>(i)] / m;
            dp[static_cast<size_t>(i)] = (d[static_cast<size_t>(i)] - a[static_cast<size_t>(i)] * dp[static_cast<size_t>(i - 1)]) / m;
        }
        next[static_cast<size_t>(n - 2)] = dp[static_cast<size_t>(n - 2)];
        for (int i = n - 3; i >= 1; --i)
            next[static_cast<size_t>(i)] = dp[static_cast<size_t>(i)] - cp[static_cast<size_t>(i)] * next[static_cast<size_t>(i + 1)];
        double change = 0.0;
        for (int i = 0; i < n; ++i)
            change = std::max(change, std::abs(next[static_cast<size_t>(i)] - t[static_cast<size_t>(i)]));
        t = next;
        if (!std::all_of(t.begin(), t.end(), [](double v) { return std::isfinite(v) && v > 0.0; }))
            return R::failure("the temperature of the leg '" + leg.name + "' left the physical range (the current is too large for the legs)");
        if (change <= 1e-12 * hot)
            break;
        if (it == 200)
            return R::failure("the temperature of the leg '" + leg.name + "' did not converge");
    }
    // End fluxes (q = alpha T j - kappa T', second-order one-sided gradients).
    const double g0 = (-3 * t[0] + 4 * t[1] - t[2]) / (2 * h);
    const double gl = (3 * t[static_cast<size_t>(n - 1)] - 4 * t[static_cast<size_t>(n - 2)] + t[static_cast<size_t>(n - 3)]) / (2 * h);
    const double q0 = leg.seebeck.at(cold) * cold * j - leg.conductivity.at(cold) * g0;
    const double ql = leg.seebeck.at(hot) * hot * j - leg.conductivity.at(hot) * gl;
    out.heatIntoCold = -leg.area * q0;
    out.heatFromHot = -leg.area * ql;
    for (int i = 0; i < n; ++i) {
        const double w = (i == 0 || i == n - 1) ? 0.5 : 1.0;
        const double ti = t[static_cast<size_t>(i)];
        const double grad = i == 0 ? g0 : i == n - 1 ? gl : (t[static_cast<size_t>(i + 1)] - t[static_cast<size_t>(i - 1)]) / (2 * h);
        out.resistance += w * h * leg.resistivity.at(ti) / leg.area;
        out.joule += w * h * leg.resistivity.at(ti) * j * j * leg.area;
        out.thomson += w * h * ti * leg.seebeck.derivative(ti) * j * grad * leg.area;
    }
    return R(std::move(out));
}

Status validateLeg(const ThermoelectricLeg& l)
{
    if (!(l.length > 0.0 && l.area > 0.0))
        return Status::failure("the leg '" + l.name + "' needs a positive length and area");
    if (l.seebeck.coefficients.empty() || l.resistivity.coefficients.empty() || l.conductivity.coefficients.empty())
        return Status::failure("the leg '" + l.name + "' needs a Seebeck coefficient, a resistivity and a thermal conductivity");
    return Status::success();
}

}  // namespace

double TemperatureFunction::at(double t) const
{
    double v = 0.0, p = 1.0;
    for (const double c : coefficients) {
        v += c * p;
        p *= t;
    }
    return v;
}

double TemperatureFunction::derivative(double t) const
{
    double v = 0.0, p = 1.0;
    for (size_t i = 1; i < coefficients.size(); ++i) {
        v += static_cast<double>(i) * coefficients[i] * p;
        p *= t;
    }
    return v;
}

Status ThermoelectricCouple::validate() const
{
    if (Status s = validateLeg(p); !s)
        return s;
    if (Status s = validateLeg(n); !s)
        return s;
    if (couples < 1 || contactResistance < 0.0)
        return Status::failure("a module has at least one couple and a non-negative contact resistance");
    return Status::success();
}

double maximumGeneratorEfficiency(double zt, double hot, double cold)
{
    const double m = std::sqrt(1 + zt);
    return (hot - cold) / hot * (m - 1) / (m + cold / hot);
}

Result<ThermoelectricResult> solveThermoelectric(const ThermoelectricCouple& couple, ThermoelectricMode mode, const ThermoelectricOperation& op)
{
    using R = Result<ThermoelectricResult>;
    if (Status s = couple.validate(); !s)
        return R::failure(s.message());
    const double th = op.hotTemperature, tc = op.coldTemperature;
    if (!(tc > 0.0 && th > tc))
        return R::failure("the junction temperatures need 0 < T_cold < T_hot");
    if (op.nodes < 11)
        return R::failure("a leg needs at least 11 nodes");
    if (mode == ThermoelectricMode::Generator && !op.loadResistance && !op.current)
        return R::failure("a generator needs a load resistance or an imposed current");
    if (mode == ThermoelectricMode::Cooler && !op.current)
        return R::failure("a cooler needs its drive current");
    if (op.loadResistance && !(*op.loadResistance > 0.0))
        return R::failure("the load resistance must be positive");
    const int nc = couple.couples;
    ThermoelectricResult out;
    // The open-circuit voltage is profile-independent: the integral of alpha_p - alpha_n over [T_c, T_h].
    const double vocCouple = integral(couple.p.seebeck, tc, th) - integral(couple.n.seebeck, tc, th);
    out.openCircuitVoltage = nc * vocCouple;
    // The generator convention: current I through the load from the p terminal; the n leg
    // carries +I from cold to hot, the p leg -I. A cooler runs with I = -(drive current).
    double current = mode == ThermoelectricMode::Cooler ? -*op.current : op.current.value_or(0.0);
    std::vector<double> startP, startN;
    LegSolution sp, sn;
    for (int outer = 1; outer <= 200; ++outer) {
        auto p = solveLeg(couple.p, -current / couple.p.area, tc, th, op.nodes, startP);
        auto n = solveLeg(couple.n, current / couple.n.area, tc, th, op.nodes, startN);
        if (!p || !n)
            return R::failure(!p ? p.message() : n.message());
        sp = p.value();
        sn = n.value();
        startP = sp.t;
        startN = sn.t;
        out.iterations += sp.iterations + sn.iterations;
        if (mode == ThermoelectricMode::Cooler || op.current)
            break;
        // A generator with a load: I = N V_oc / (N R_int + R_load), R_int from the current profiles.
        const double rint = sp.resistance + sn.resistance + couple.contactResistance;
        const double next = out.openCircuitVoltage / (nc * rint + *op.loadResistance);
        const bool settled = std::abs(next - current) <= 1e-13 * std::max(1e-300, std::abs(next));
        current = next;
        if (settled)
            break;
        if (outer == 200)
            return R::failure("the generator's current did not converge with its load");
    }
    out.internalResistance = nc * (sp.resistance + sn.resistance + couple.contactResistance);
    const double voltage = out.openCircuitVoltage - current * out.internalResistance;  // generator convention, terminal
    // Heats (generator convention): in at the hot junction, out at the cold junction; the
    // contact resistance sits at the cold terminals, its heat goes to the cold side.
    const double qh = nc * (sp.heatFromHot + sn.heatFromHot);
    const double qc = nc * (sp.heatIntoCold + sn.heatIntoCold) + nc * current * current * couple.contactResistance;
    out.joule = nc * (sp.joule + sn.joule + current * current * couple.contactResistance);
    out.thomson = nc * (sp.thomson + sn.thomson);
    out.energyResidual = qh - qc - voltage * current;
    out.entropyGeneration = qc / tc - qh / th;
    out.x.resize(static_cast<size_t>(op.nodes));
    for (int i = 0; i < op.nodes; ++i)
        out.x[static_cast<size_t>(i)] = static_cast<double>(i) / (op.nodes - 1);  // normalized position along each leg
    out.temperatureP = sp.t;
    out.temperatureN = sn.t;
    conversion::ConversionStage& stage = out.stage;
    stage.model = "thermal.thermoelectric.couple-1d";
    stage.coupling = {{"openCircuitVoltage", out.openCircuitVoltage}, {"internalResistance", out.internalResistance}};
    stage.boundaryConditions = {"junction temperatures " + num(th) + " K and " + num(tc) + " K imposed"};
    stage.assumptions = {"steady, one-dimensional legs; alpha, rho, kappa functions of temperature", "the Thomson heat from T d(alpha)/dT",
                         "no side losses, radiation or junction thermal resistance", "the contact resistance at the cold terminals"};
    stage.validity = {"the stated property functions' temperature range", "the junction temperatures as given (no heat-exchanger model)"};
    stage.missing = {};
    if (mode == ThermoelectricMode::Generator) {
        out.current = current;
        out.voltage = voltage;
        out.heatHot = qh;
        out.heatCold = qc;
        out.electricalPower = voltage * current;
        out.efficiency = qh > 0.0 ? out.electricalPower / qh : 0.0;
        stage.id = "thermoelectric-generator";
        stage.name = "Thermoelectric generator";
        stage.mechanism = "thermoelectric-generator";
        stage.inputForm = conversion::EnergyForm::Thermal;
        stage.outputForm = conversion::EnergyForm::Electrical;
        stage.input = qh;
        stage.output = out.electricalPower;
        stage.otherOutputs = {{"heat rejected at the cold junction", conversion::EnergyForm::Thermal, qc}};
        stage.coupling["efficiency"] = out.efficiency;
    } else {
        out.current = -current;
        out.voltage = -voltage;
        out.heatCold = -qc;  // absorbed from the cold side
        out.heatHot = -qh;   // rejected at the hot side
        out.electricalPower = -voltage * current;
        out.cop = out.electricalPower > 0.0 ? out.heatCold / out.electricalPower : 0.0;
        stage.id = "thermoelectric-cooler";
        stage.name = "Thermoelectric cooler";
        stage.mechanism = "thermoelectric-cooler";
        stage.inputForm = conversion::EnergyForm::Electrical;
        stage.outputForm = conversion::EnergyForm::Thermal;
        stage.input = out.electricalPower;
        stage.otherInputs = {{"heat absorbed at the cold junction", conversion::EnergyForm::Thermal, out.heatCold}};
        stage.output = out.heatHot;
        stage.coupling["cop"] = out.cop;
    }
    stage.coupling["entropyGeneration"] = out.entropyGeneration;
    out.assumptions = stage.assumptions;
    if (out.entropyGeneration < -1e-9 * std::max(std::abs(qh / th), 1e-300))
        return R::failure("the solution generates negative entropy: it is not physical (check the property functions)");
    return R(std::move(out));
}

}  // namespace resonant::thermal
