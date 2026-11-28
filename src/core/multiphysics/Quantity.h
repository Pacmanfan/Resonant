#pragma once

// Typed physical quantities of the Coupled Energy-Matter Engine
// (REQ-CHAT-20261126-016, RES-CEME-002, docs/physics/PHYSICAL_QUANTITY_SYSTEM.md).
//
// Every quantity a participant provides or accepts has a registered
// descriptor: identifier, SI unit, rank, real or complex representation,
// meaning (instantaneous, peak, RMS, time-averaged, integrated or static),
// the domain that defines it, and whether its transfer must conserve the
// integral (densities of power, heat, force, mass). Identical descriptors are
// compatible; any other pair needs a conversion that a coupling model declares
// explicitly (for example a time-averaged absorbed power density becoming a
// steady heat source).
#include "foundation/Result.h"

#include <string>
#include <string_view>
#include <vector>

namespace resonant::multiphysics {

enum class Rank { Scalar, Vector, Tensor };
enum class Representation { Real, Complex };
enum class Meaning { Instantaneous, Peak, Rms, TimeAveraged, Integrated, Static };

std::string_view toString(Rank rank);
std::string_view toString(Representation representation);
std::string_view toString(Meaning meaning);

struct QuantityDescriptor {
    std::string id;           // "absorbed_power_density"
    std::string unit;         // "W/m^3" (SI)
    Rank rank = Rank::Scalar;
    Representation representation = Representation::Real;
    Meaning meaning = Meaning::Static;
    std::string domain;       // "electromagnetic", "thermal", "mechanical", "acoustic", "general"
    std::string description;
    bool conservative = false;  // a density whose integral transfers must preserve

    int components() const { return rank == Rank::Scalar ? 1 : rank == Rank::Vector ? 3 : 6; }
};

// The registry of known quantities (SI units only).
const std::vector<QuantityDescriptor>& quantityRegistry();
const QuantityDescriptor* findQuantity(std::string_view id);

// Success when a and b describe the same physical quantity in the same
// unit, rank, representation and meaning; otherwise the first difference.
Status compatible(const QuantityDescriptor& a, const QuantityDescriptor& b);

}  // namespace resonant::multiphysics
