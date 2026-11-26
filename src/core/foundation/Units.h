// Unit conventions. Everything inside the core is SI: metres, seconds,
// hertz, kilograms, pascals, radians. Display units exist only at the user
// interface boundary and are converted with the functions below; nothing in
// the core converts units implicitly.
#pragma once

#include <numbers>
#include <string_view>

namespace resonant::units {

inline constexpr double pi = std::numbers::pi;

constexpr double degreesToRadians(double degrees) { return degrees * pi / 180.0; }
constexpr double radiansToDegrees(double radians) { return radians * 180.0 / pi; }

enum class LengthUnit { Meter, Centimeter, Millimeter, Inch };
enum class FrequencyUnit { Hertz, Kilohertz, Megahertz };
enum class TimeUnit { Second, Millisecond, Microsecond };

// Metres per one display unit.
constexpr double metersPer(LengthUnit unit)
{
    switch (unit) {
    case LengthUnit::Meter: return 1.0;
    case LengthUnit::Centimeter: return 1e-2;
    case LengthUnit::Millimeter: return 1e-3;
    case LengthUnit::Inch: return 0.0254;
    }
    return 1.0;
}
constexpr double hertzPer(FrequencyUnit unit)
{
    switch (unit) {
    case FrequencyUnit::Hertz: return 1.0;
    case FrequencyUnit::Kilohertz: return 1e3;
    case FrequencyUnit::Megahertz: return 1e6;
    }
    return 1.0;
}
constexpr double secondsPer(TimeUnit unit)
{
    switch (unit) {
    case TimeUnit::Second: return 1.0;
    case TimeUnit::Millisecond: return 1e-3;
    case TimeUnit::Microsecond: return 1e-6;
    }
    return 1.0;
}

constexpr double toSI(double value, LengthUnit unit) { return value * metersPer(unit); }
constexpr double fromSI(double meters, LengthUnit unit) { return meters / metersPer(unit); }
constexpr double toSI(double value, FrequencyUnit unit) { return value * hertzPer(unit); }
constexpr double fromSI(double hertz, FrequencyUnit unit) { return hertz / hertzPer(unit); }
constexpr double toSI(double value, TimeUnit unit) { return value * secondsPer(unit); }
constexpr double fromSI(double seconds, TimeUnit unit) { return seconds / secondsPer(unit); }

std::string_view symbol(LengthUnit unit);
std::string_view symbol(FrequencyUnit unit);
std::string_view symbol(TimeUnit unit);

// The conventions a project file records, so a reader never has to guess.
struct Conventions {
    static constexpr std::string_view length = "m";
    static constexpr std::string_view time = "s";
    static constexpr std::string_view frequency = "Hz";
    static constexpr std::string_view mass = "kg";
    static constexpr std::string_view angle = "rad";
    static constexpr std::string_view rotation = "intrinsic Z-Y-X Euler angles stored as [x, y, z]";
    static constexpr std::string_view axes = "right-handed; X horizontal, Y depth, Z vertical (up)";
};

}  // namespace resonant::units
