#include "foundation/Units.h"

namespace resonant::units {

std::string_view symbol(LengthUnit unit)
{
    switch (unit) {
    case LengthUnit::Meter: return "m";
    case LengthUnit::Centimeter: return "cm";
    case LengthUnit::Millimeter: return "mm";
    case LengthUnit::Inch: return "in";
    }
    return "m";
}

std::string_view symbol(FrequencyUnit unit)
{
    switch (unit) {
    case FrequencyUnit::Hertz: return "Hz";
    case FrequencyUnit::Kilohertz: return "kHz";
    case FrequencyUnit::Megahertz: return "MHz";
    }
    return "Hz";
}

std::string_view symbol(TimeUnit unit)
{
    switch (unit) {
    case TimeUnit::Second: return "s";
    case TimeUnit::Millisecond: return "ms";
    case TimeUnit::Microsecond: return "us";
    }
    return "s";
}

}  // namespace resonant::units
