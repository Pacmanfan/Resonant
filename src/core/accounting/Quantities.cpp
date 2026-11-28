#include "accounting/Quantities.h"

#include <cmath>

namespace resonant::accounting {

namespace {

Dimension d(std::string_view unit)
{
    auto u = parseUnit(unit);
    return u ? u.value().dimension : Dimension::none();
}

}  // namespace

const std::vector<FamilyInfo>& families()
{
    static const std::vector<FamilyInfo> f = {
        {Family::Energy, "energy", "Energy", "J", d("J"), true, false, "stored, transferred, dissipated; same dimension as torque, never combined with it"},
        {Family::Power, "power", "Power", "W", d("W"), false, false, "energy per unit time; a rate, not an amount"},
        {Family::Force, "force", "Force", "N", d("N"), false, true, "an interaction; not conserved"},
        {Family::Torque, "torque", "Torque", "N m", d("N m"), false, true, "a moment of force; not an energy"},
        {Family::LinearMomentum, "linear-momentum", "Linear momentum", "kg m/s", d("kg m/s"), true, true, "an impulse is a momentum transferred by a force"},
        {Family::AngularMomentum, "angular-momentum", "Angular momentum", "kg m^2/s", d("kg m^2/s"), true, true, ""},
        {Family::Mass, "mass", "Mass", "kg", d("kg"), true, false, ""},
        {Family::Charge, "charge", "Electric charge", "C", d("C"), true, false, ""},
        {Family::Entropy, "entropy", "Entropy", "J/K", d("J/K"), false, false, "balanced with its production, which is never negative"},
        {Family::EntropyRate, "entropy-rate", "Entropy rate", "W/K", d("W/K"), false, false, ""},
        {Family::HeatCapacity, "heat-capacity", "Heat capacity", "J/K", d("J/K"), false, false, "same dimension as entropy, not an entropy"},
        {Family::Temperature, "temperature", "Temperature", "K", d("K"), false, false, "intensive"},
        {Family::Pressure, "pressure", "Pressure", "Pa", d("Pa"), false, false, "a scalar normal stress"},
        {Family::Stress, "stress", "Stress", "Pa", d("Pa"), false, false, "a tensor"},
        {Family::MomentumFlux, "momentum-flux", "Momentum flux", "Pa", d("Pa"), false, false, "momentum per area per time; same dimension as pressure"},
        {Family::EnergyDensity, "energy-density", "Energy density", "J/m^3", d("J/m^3"), false, false, "same dimension as pressure"},
        {Family::PowerDensity, "power-density", "Power density", "W/m^3", d("W/m^3"), false, false, ""},
        {Family::EnergyFlux, "energy-flux", "Energy flux", "W/m^2", d("W/m^2"), false, true, "Poynting vector, acoustic intensity, heat flux"},
        {Family::ForceDensity, "force-density", "Force density", "N/m^3", d("N/m^3"), false, true, ""},
        {Family::Velocity, "velocity", "Velocity", "m/s", d("m/s"), false, true, ""},
        {Family::Acceleration, "acceleration", "Acceleration", "m/s^2", d("m/s^2"), false, true, ""},
        {Family::AngularVelocity, "angular-velocity", "Angular velocity", "rad/s", d("rad/s"), false, true, "same dimension as frequency"},
        {Family::MomentOfInertia, "moment-of-inertia", "Moment of inertia", "kg m^2", d("kg m^2"), false, false, ""},
        {Family::Frequency, "frequency", "Frequency", "Hz", d("Hz"), false, false, ""},
        {Family::Length, "length", "Length", "m", d("m"), false, false, ""},
        {Family::Time, "time", "Time", "s", d("s"), false, false, ""},
    };
    return f;
}

const FamilyInfo& info(Family f)
{
    for (const auto& i : families())
        if (i.family == f)
            return i;
    return families().front();
}

std::string_view toString(Family f)
{
    return info(f).id;
}

std::optional<Family> familyFromString(std::string_view id)
{
    for (const auto& i : families())
        if (i.id == id)
            return i.family;
    return std::nullopt;
}

std::string rateUnit(Family f)
{
    switch (f) {
    case Family::Energy:
        return "W";
    case Family::LinearMomentum:
        return "kg m/s^2";
    case Family::AngularMomentum:
        return "kg m^2/s^2";
    case Family::Entropy:
        return "W/K";
    case Family::Mass:
        return "kg/s";
    case Family::Charge:
        return "A";
    default:
        return info(f).unit + "/s";
    }
}

double Quantity::magnitude() const
{
    if (!isVector)
        return std::abs(value);
    return std::sqrt(components[0] * components[0] + components[1] * components[1] + components[2] * components[2]);
}

nlohmann::json Quantity::toJson() const
{
    nlohmann::json j{{"id", id}, {"family", std::string(toString(family))}, {"kind", kind}, {"domain", domain}, {"unit", unit()},
                     {"frame", frame}, {"provenance", provenance}};
    if (isVector)
        j["vector"] = components;
    else
        j["value"] = value;
    for (const auto& [k, v] : {std::pair{"region", &region}, {"object", &object}, {"material", &material}, {"source", &source}, {"validity", &validity}})
        if (!v->empty())
            j[k] = *v;
    if (time)
        j["time"] = *time;
    if (interval)
        j["interval"] = *interval;
    if (uncertainty)
        j["uncertainty"] = *uncertainty;
    return j;
}

Result<Quantity> Quantity::fromJson(const nlohmann::json& j)
{
    using R = Result<Quantity>;
    if (!j.is_object())
        return R::failure("a quantity is an object");
    auto f = familyFromString(j.value("family", std::string()));
    if (!f)
        return R::failure("unknown quantity family '" + j.value("family", std::string()) + "'");
    Quantity q;
    q.family = *f;
    q.id = j.value("id", std::string());
    q.kind = j.value("kind", std::string());
    q.domain = j.value("domain", std::string());
    const std::string unit = j.value("unit", info(*f).unit);
    auto u = parseUnit(unit);
    if (!u)
        return R::failure(u.message());
    if (u.value().dimension != info(*f).dimension)
        return R::failure("the unit '" + unit + "' is not a unit of " + info(*f).name);
    if (j.contains("vector")) {
        const auto v = j["vector"].get<std::vector<double>>();
        if (v.size() != 3)
            return R::failure("a vector has three components");
        q.isVector = true;
        for (size_t i = 0; i < 3; ++i)
            q.components[i] = v[i] * u.value().factor;
    } else {
        q.value = j.value("value", 0.0) * u.value().factor;
    }
    q.frame = j.value("frame", std::string("world"));
    q.region = j.value("region", std::string());
    q.object = j.value("object", std::string());
    q.material = j.value("material", std::string());
    q.source = j.value("source", std::string());
    q.validity = j.value("validity", std::string());
    q.provenance = j.value("provenance", std::string("computed"));
    if (j.contains("time"))
        q.time = j["time"].get<double>();
    if (j.contains("interval"))
        q.interval = j["interval"].get<std::array<double, 2>>();
    if (j.contains("uncertainty"))
        q.uncertainty = j["uncertainty"].get<double>() * u.value().factor;
    return q;
}

Result<Quantity> make(Family f, double value, std::string_view unit, std::string kind)
{
    auto u = parseUnit(unit);
    if (!u)
        return Result<Quantity>::failure(u.message());
    if (u.value().dimension != info(f).dimension)
        return Result<Quantity>::failure("'" + std::string(unit) + "' [" + u.value().dimension.toString() + "] is not a unit of " + info(f).name + " [" +
                                         info(f).dimension.toString() + "]");
    Quantity q;
    q.family = f;
    q.value = value * u.value().factor;
    q.kind = kind.empty() ? std::string(toString(f)) : std::move(kind);
    return q;
}

Result<double> valueIn(const Quantity& q, std::string_view unit)
{
    if (q.isVector)
        return Result<double>::failure("a vector has no single value: convert its components");
    return convertUnit(q.value, q.unit(), unit);
}

namespace {

Result<Quantity> combine(const Quantity& a, const Quantity& b, double sign)
{
    using R = Result<Quantity>;
    if (a.family != b.family) {
        const bool same = info(a.family).dimension == info(b.family).dimension;
        return R::failure("cannot combine " + info(a.family).name + " with " + info(b.family).name +
                          (same ? " (the same dimension, different physical quantities)" : " (different dimensions)"));
    }
    if (a.isVector != b.isVector)
        return R::failure("cannot combine a vector with a scalar " + info(a.family).name);
    if (a.isVector && a.frame != b.frame)
        return R::failure("the vectors are in different frames (" + a.frame + ", " + b.frame + ")");
    Quantity q = a;
    q.id.clear();
    q.kind = a.kind == b.kind ? a.kind : std::string(toString(a.family));
    q.value = a.value + sign * b.value;
    for (size_t i = 0; i < 3; ++i)
        q.components[i] = a.components[i] + sign * b.components[i];
    if (a.uncertainty || b.uncertainty)
        q.uncertainty = std::sqrt(std::pow(a.uncertainty.value_or(0.0), 2) + std::pow(b.uncertainty.value_or(0.0), 2));
    if (a.domain != b.domain)
        q.domain = "general";
    q.source = a.source == b.source ? a.source : std::string();
    return q;
}

}  // namespace

Result<Quantity> add(const Quantity& a, const Quantity& b)
{
    return combine(a, b, 1.0);
}

Result<Quantity> subtract(const Quantity& a, const Quantity& b)
{
    return combine(a, b, -1.0);
}

}  // namespace resonant::accounting
