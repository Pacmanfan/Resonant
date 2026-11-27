#include "experiments/ExperimentTypes.h"

#include <array>
#include <cmath>
#include <utility>

namespace resonant::experiments {

namespace {

struct QuantityEntry {
    Quantity quantity;
    std::string_view name;
    std::string_view unit;
};

constexpr std::array<QuantityEntry, 19> kQuantities{{
    {Quantity::Voltage, "voltage", "V"},
    {Quantity::DigitalFullScale, "full-scale", "FS"},
    {Quantity::Pressure, "pressure", "Pa"},
    {Quantity::Acceleration, "acceleration", "m/s^2"},
    {Quantity::Velocity, "velocity", "m/s"},
    {Quantity::Displacement, "displacement", "m"},
    {Quantity::Force, "force", "N"},
    {Quantity::Strain, "strain", "m/m"},
    {Quantity::Stress, "stress", "Pa"},
    {Quantity::Power, "power", "W"},
    {Quantity::ElectricField, "electric-field", "V/m"},
    {Quantity::MagneticField, "magnetic-field", "A/m"},
    {Quantity::ScatteringParameter, "s-parameter", "1"},
    {Quantity::Impedance, "impedance", "ohm"},
    {Quantity::Temperature, "temperature", "K"},
    {Quantity::Angle, "angle", "rad"},
    {Quantity::Time, "time", "s"},
    {Quantity::Frequency, "frequency", "Hz"},
    {Quantity::Dimensionless, "dimensionless", "1"},
}};

template <typename E, size_t N>
Result<E> lookup(const std::array<std::pair<E, std::string_view>, N>& table, std::string_view text, std::string_view what)
{
    for (const auto& [value, name] : table)
        if (name == text)
            return value;
    std::string names;
    for (const auto& entry : table)
        names += (names.empty() ? "" : ", ") + std::string(entry.second);
    return Result<E>::failure("unknown " + std::string(what) + " '" + std::string(text) + "' (" + names + ")");
}

constexpr std::array<std::pair<DataCategory, std::string_view>, 6> kCategories{{
    {DataCategory::Raw, "raw"},
    {DataCategory::Calibrated, "calibrated"},
    {DataCategory::Processed, "processed"},
    {DataCategory::Simulated, "simulated"},
    {DataCategory::Reconstructed, "reconstructed"},
    {DataCategory::Command, "command"},
}};

constexpr std::array<std::pair<SampleFormat, std::string_view>, 8> kFormats{{
    {SampleFormat::Int16, "int16"},
    {SampleFormat::Int24, "int24"},
    {SampleFormat::Int32, "int32"},
    {SampleFormat::Float32, "float32"},
    {SampleFormat::Float64, "float64"},
    {SampleFormat::ComplexInt16, "complex-int16"},
    {SampleFormat::ComplexFloat32, "complex-float32"},
    {SampleFormat::ComplexFloat64, "complex-float64"},
}};

}  // namespace

std::string_view toString(Quantity quantity)
{
    for (const auto& e : kQuantities)
        if (e.quantity == quantity)
            return e.name;
    return "dimensionless";
}

Result<Quantity> quantityFromString(std::string_view text)
{
    for (const auto& e : kQuantities)
        if (e.name == text)
            return e.quantity;
    std::string names;
    for (const auto& e : kQuantities)
        names += (names.empty() ? "" : ", ") + std::string(e.name);
    return Result<Quantity>::failure("unknown quantity '" + std::string(text) + "' (" + names + ")");
}

std::string_view siUnit(Quantity quantity)
{
    for (const auto& e : kQuantities)
        if (e.quantity == quantity)
            return e.unit;
    return "1";
}

std::string QuantitySpec::unit() const
{
    std::string base = per ? std::string(siUnit(quantity)) + "/" + std::string(siUnit(*per)) : std::string(siUnit(quantity));
    if (spectralDensity)
        return "(" + base + ")^2/Hz";
    return base;
}

std::string QuantitySpec::describe() const
{
    std::string name = std::string(toString(quantity)) + (per ? " per " + std::string(toString(*per)) : std::string());
    if (spectralDensity)
        name = "power spectral density of " + name;
    return name + " [" + unit() + "]";
}

nlohmann::json toJson(const QuantitySpec& spec)
{
    nlohmann::json v = {{"quantity", toString(spec.quantity)}, {"unit", spec.unit()}};
    if (spec.per)
        v["per"] = toString(*spec.per);
    if (spec.spectralDensity)
        v["spectralDensity"] = true;
    return v;
}

Result<QuantitySpec> quantitySpecFromJson(const nlohmann::json& value)
{
    if (!value.is_object() || !value.contains("quantity"))
        return Result<QuantitySpec>::failure("a quantity needs a \"quantity\" name");
    auto q = quantityFromString(value.value("quantity", std::string()));
    if (!q)
        return Result<QuantitySpec>::failure(q.message());
    QuantitySpec spec;
    spec.quantity = q.value();
    if (value.contains("per")) {
        auto per = quantityFromString(value.value("per", std::string()));
        if (!per)
            return Result<QuantitySpec>::failure(per.message());
        spec.per = per.value();
    }
    spec.spectralDensity = value.value("spectralDensity", false);
    // A stated unit must agree with the quantity's SI unit: the framework does
    // not convert units silently.
    if (value.contains("unit") && value["unit"].is_string() && value["unit"].get<std::string>() != spec.unit())
        return Result<QuantitySpec>::failure("the unit '" + value["unit"].get<std::string>() + "' is not the SI unit of " +
                                             spec.describe() + "; convert the values first");
    return spec;
}

std::string_view toString(DataCategory category)
{
    for (const auto& [c, name] : kCategories)
        if (c == category)
            return name;
    return "raw";
}

Result<DataCategory> dataCategoryFromString(std::string_view text) { return lookup(kCategories, text, "data category"); }

std::string_view toString(SampleFormat format)
{
    for (const auto& [f, name] : kFormats)
        if (f == format)
            return name;
    return "float64";
}

Result<SampleFormat> sampleFormatFromString(std::string_view text) { return lookup(kFormats, text, "sample format"); }

bool isComplex(SampleFormat format)
{
    return format == SampleFormat::ComplexInt16 || format == SampleFormat::ComplexFloat32 || format == SampleFormat::ComplexFloat64;
}

int bitsOf(SampleFormat format)
{
    switch (format) {
    case SampleFormat::Int16:
    case SampleFormat::ComplexInt16: return 16;
    case SampleFormat::Int24: return 24;
    case SampleFormat::Int32:
    case SampleFormat::Float32:
    case SampleFormat::ComplexFloat32: return 32;
    case SampleFormat::Float64:
    case SampleFormat::ComplexFloat64: return 64;
    }
    return 64;
}

double quantizationStep(SampleFormat format, double fullScale)
{
    switch (format) {
    case SampleFormat::Int16:
    case SampleFormat::Int24:
    case SampleFormat::Int32:
    case SampleFormat::ComplexInt16: return 2.0 * fullScale / std::ldexp(1.0, bitsOf(format));
    default: return 0.0;
    }
}

nlohmann::json toJson(const Pose& p)
{
    return {{"position", {p.x, p.y, p.z}},     {"direction", {p.dx, p.dy, p.dz}}, {"frame", p.frame},
            {"positionUncertainty", p.positionUncertainty}, {"orientationUncertaintyDeg", p.orientationUncertaintyDeg}};
}

Result<Pose> poseFromJson(const nlohmann::json& v)
{
    if (!v.is_object())
        return Result<Pose>::failure("a pose must be an object");
    Pose p;
    auto triple = [&](const char* key, double& a, double& b, double& c) -> bool {
        if (!v.contains(key))
            return true;
        const auto& t = v[key];
        if (!t.is_array() || t.size() != 3 || !t[0].is_number() || !t[1].is_number() || !t[2].is_number())
            return false;
        a = t[0].get<double>();
        b = t[1].get<double>();
        c = t[2].get<double>();
        return std::isfinite(a) && std::isfinite(b) && std::isfinite(c);
    };
    if (!triple("position", p.x, p.y, p.z))
        return Result<Pose>::failure("a pose position must be three finite numbers (m)");
    if (!triple("direction", p.dx, p.dy, p.dz))
        return Result<Pose>::failure("a pose direction must be three finite numbers");
    const double norm = std::sqrt(p.dx * p.dx + p.dy * p.dy + p.dz * p.dz);
    if (norm < 1e-12)
        return Result<Pose>::failure("a pose direction must not be zero");
    p.dx /= norm;
    p.dy /= norm;
    p.dz /= norm;
    p.frame = v.value("frame", std::string("scene"));
    p.positionUncertainty = v.value("positionUncertainty", 0.0);
    p.orientationUncertaintyDeg = v.value("orientationUncertaintyDeg", 0.0);
    return p;
}

nlohmann::json toJson(const ProcessingStep& s)
{
    return {{"operation", s.operation}, {"parameters", s.parameters}, {"description", s.description}};
}

Result<ProcessingStep> processingStepFromJson(const nlohmann::json& v)
{
    if (!v.is_object() || !v.contains("operation") || !v["operation"].is_string())
        return Result<ProcessingStep>::failure("a processing step needs an \"operation\"");
    ProcessingStep s;
    s.operation = v["operation"].get<std::string>();
    s.parameters = v.value("parameters", nlohmann::json::object());
    s.description = v.value("description", std::string());
    if (!s.parameters.is_object())
        return Result<ProcessingStep>::failure("processing parameters must be an object");
    return s;
}

}  // namespace resonant::experiments
