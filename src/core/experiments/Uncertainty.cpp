#include "experiments/Uncertainty.h"

#include <array>
#include <cmath>
#include <sstream>
#include <utility>

namespace resonant::experiments {

namespace {

constexpr std::array<std::pair<UncertaintySource, std::string_view>, 13> kSources{{
    {UncertaintySource::SensorCalibration, "sensor-calibration"},
    {UncertaintySource::InstrumentAccuracy, "instrument-accuracy"},
    {UncertaintySource::Quantization, "quantization"},
    {UncertaintySource::Noise, "noise"},
    {UncertaintySource::Position, "position"},
    {UncertaintySource::Orientation, "orientation"},
    {UncertaintySource::Clock, "clock"},
    {UncertaintySource::Temperature, "temperature"},
    {UncertaintySource::Repeatability, "repeatability"},
    {UncertaintySource::Material, "material"},
    {UncertaintySource::Model, "model"},
    {UncertaintySource::NumericalSimulation, "numerical-simulation"},
    {UncertaintySource::Other, "other"},
}};

constexpr std::array<std::pair<UncertaintyType, std::string_view>, 5> kTypes{{
    {UncertaintyType::InstrumentSpecification, "instrument-specification"},
    {UncertaintyType::Calibration, "calibration"},
    {UncertaintyType::Statistical, "statistical"},
    {UncertaintyType::Model, "model"},
    {UncertaintyType::NumericalSimulation, "numerical-simulation"},
}};

template <typename E, size_t N>
std::string_view nameOf(const std::array<std::pair<E, std::string_view>, N>& table, E value)
{
    for (const auto& [v, name] : table)
        if (v == value)
            return name;
    return table.back().second;
}

template <typename E, size_t N>
Result<E> valueOf(const std::array<std::pair<E, std::string_view>, N>& table, std::string_view text, const char* what)
{
    for (const auto& [v, name] : table)
        if (name == text)
            return v;
    return Result<E>::failure(std::string("unknown ") + what + " '" + std::string(text) + "'");
}

}  // namespace

std::string_view toString(UncertaintySource source) { return nameOf(kSources, source); }
std::string_view toString(UncertaintyType type) { return nameOf(kTypes, type); }
Result<UncertaintySource> uncertaintySourceFromString(std::string_view text) { return valueOf(kSources, text, "uncertainty source"); }
Result<UncertaintyType> uncertaintyTypeFromString(std::string_view text) { return valueOf(kTypes, text, "uncertainty type"); }

double UncertaintyBudget::combinedStandard(double value) const
{
    double sum = 0.0;
    for (const auto& c : components) {
        const double r = c.relative * value;
        sum += r * r + c.absolute * c.absolute;
    }
    return std::sqrt(sum);
}

double UncertaintyBudget::combinedStandard(double value, UncertaintyType type) const
{
    double sum = 0.0;
    for (const auto& c : components) {
        if (c.type != type)
            continue;
        const double r = c.relative * value;
        sum += r * r + c.absolute * c.absolute;
    }
    return std::sqrt(sum);
}

double UncertaintyBudget::combinedRelative() const
{
    double sum = 0.0;
    for (const auto& c : components)
        sum += c.relative * c.relative;
    return std::sqrt(sum);
}

std::string UncertaintyBudget::describe(double value, std::string_view unit) const
{
    if (components.empty())
        return "no uncertainty information";
    std::ostringstream out;
    out.precision(3);
    out << "u_c = " << combinedStandard(value) << " " << unit << " (standard), U = " << expanded(value) << " " << unit
        << " (k = " << coverageFactor << "); " << components.size() << " component(s), " << correlation;
    return out.str();
}

nlohmann::json toJson(const UncertaintyBudget& b)
{
    nlohmann::json list = nlohmann::json::array();
    for (const auto& c : b.components)
        list.push_back({{"source", toString(c.source)},
                        {"type", toString(c.type)},
                        {"relative", c.relative},
                        {"absolute", c.absolute},
                        {"distribution", c.distribution},
                        {"degreesOfFreedom", c.degreesOfFreedom},
                        {"description", c.description}});
    return {{"components", list}, {"coverageFactor", b.coverageFactor}, {"correlation", b.correlation}};
}

Result<UncertaintyBudget> uncertaintyBudgetFromJson(const nlohmann::json& v)
{
    UncertaintyBudget b;
    if (v.is_null())
        return b;
    if (!v.is_object())
        return Result<UncertaintyBudget>::failure("an uncertainty budget must be an object");
    b.coverageFactor = v.value("coverageFactor", 2.0);
    b.correlation = v.value("correlation", b.correlation);
    if (!(b.coverageFactor > 0.0))
        return Result<UncertaintyBudget>::failure("the coverage factor must be positive");
    for (const auto& c : v.value("components", nlohmann::json::array())) {
        UncertaintyComponent u;
        auto source = uncertaintySourceFromString(c.value("source", std::string("other")));
        auto type = uncertaintyTypeFromString(c.value("type", std::string("instrument-specification")));
        if (!source)
            return Result<UncertaintyBudget>::failure(source.message());
        if (!type)
            return Result<UncertaintyBudget>::failure(type.message());
        u.source = source.value();
        u.type = type.value();
        u.relative = c.value("relative", 0.0);
        u.absolute = c.value("absolute", 0.0);
        if (!(u.relative >= 0.0) || !(u.absolute >= 0.0))
            return Result<UncertaintyBudget>::failure("standard uncertainties must not be negative");
        u.distribution = c.value("distribution", std::string("normal"));
        u.degreesOfFreedom = c.value("degreesOfFreedom", 0.0);
        u.description = c.value("description", std::string());
        b.components.push_back(std::move(u));
    }
    return b;
}

UncertaintyComponent quantizationComponent(double step)
{
    UncertaintyComponent c;
    c.source = UncertaintySource::Quantization;
    c.type = UncertaintyType::InstrumentSpecification;
    c.absolute = step / std::sqrt(12.0);
    c.distribution = "rectangular";
    c.description = "quantization: step / sqrt(12)";
    return c;
}

UncertaintyComponent statisticalComponent(double standardError, double degreesOfFreedom, std::string description)
{
    UncertaintyComponent c;
    c.source = UncertaintySource::Repeatability;
    c.type = UncertaintyType::Statistical;
    c.absolute = standardError;
    c.degreesOfFreedom = degreesOfFreedom;
    c.description = std::move(description);
    return c;
}

UncertaintyBudget combineForProduct(const UncertaintyBudget& a, const UncertaintyBudget& b)
{
    UncertaintyBudget out;
    out.coverageFactor = std::max(a.coverageFactor, b.coverageFactor);
    for (const auto& c : a.components)
        if (c.relative > 0.0) {
            UncertaintyComponent r = c;
            r.absolute = 0.0;
            out.components.push_back(r);
        }
    for (const auto& c : b.components)
        if (c.relative > 0.0) {
            UncertaintyComponent r = c;
            r.absolute = 0.0;
            out.components.push_back(r);
        }
    out.correlation = "relative components of independent factors, root-sum-square (absolute parts dropped: express them "
                      "relative before combining)";
    return out;
}

}  // namespace resonant::experiments
