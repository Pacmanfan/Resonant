#pragma once

// Dimensional analysis (REQ-CHAT-20261126-022, RES-ACC-002, ADR-000226): a unit
// expression ("W/m^3", "kg m/s", "N*m", "J/(kg K)", "kWh", "eV", "1") becomes its SI
// dimension (exponents of kg, m, s, K, A, mol, cd) and the factor to SI. Conversions
// check the dimension; affine temperature scales (degC, degF) are refused, since a
// difference and an absolute value convert differently. Dimension alone does not make two
// quantities the same physical quantity: that is the accounting families' job.

#include "foundation/Result.h"

#include <array>
#include <string>
#include <string_view>

namespace resonant {

struct Dimension {
    // kg, m, s, K, A, mol, cd
    std::array<int, 7> e{};

    static Dimension none() { return {}; }
    static Dimension base(int index);
    Dimension operator*(const Dimension& o) const;
    Dimension operator/(const Dimension& o) const;
    Dimension power(int n) const;
    bool operator==(const Dimension& o) const { return e == o.e; }
    bool operator!=(const Dimension& o) const { return e != o.e; }
    bool dimensionless() const { return e == std::array<int, 7>{}; }
    // "kg m^2 s^-3" (SI base units), "1" when dimensionless.
    std::string toString() const;
};

struct UnitValue {
    Dimension dimension;
    double factor = 1.0;  // SI value = factor * value
};

// The expression grammar: factors separated by spaces, '*' or a middle dot, '/' dividing
// the next factor, parentheses, integer exponents (^2, ^-1). Units: the SI base and derived
// units with prefixes (P T G M k c m u n p f; "u" or the micro sign), plus g, Hz, min, h,
// Wh, eV, L, bar, atm, rad and sr (dimensionless), percent.
Result<UnitValue> parseUnit(std::string_view expression);

// value in `from` expressed in `to`; fails when the dimensions differ.
Result<double> convertUnit(double value, std::string_view from, std::string_view to);

// Whether `unit` has the dimension `d`.
bool hasDimension(std::string_view unit, const Dimension& d);

}  // namespace resonant
