#pragma once

// Spatial transfer between cell grids (RES-CEME-008,
// docs/physics/SPATIAL_AND_TEMPORAL_TRANSFER.md).
//
//   Conservative    overlap-weighted remapping of densities: a target cell
//                   receives sum_s v_s |overlap(t, s)| / |t|. The integral over
//                   the overlap of the two grids is preserved to rounding; a
//                   target that covers the whole source preserves the whole
//                   integral. Only for quantities whose descriptor says
//                   conservative (densities of power, heat, force, mass).
//   Interpolating   linear interpolation between cell centres along each axis
//                   (tensor product), clamped to the outermost centres: no
//                   extrapolation, not conservative. For intensive quantities
//                   (temperature, displacement).
//
// Both grids must be in the same frame (world) and axis aligned.
#include "foundation/Result.h"
#include "multiphysics/FieldData.h"

#include <optional>
#include <string>
#include <string_view>

namespace resonant::multiphysics {

enum class TransferMethod { Conservative, Interpolating };
std::string_view toString(TransferMethod method);
std::optional<TransferMethod> transferMethodFromString(std::string_view text);

struct TransferReport {
    TransferMethod method = TransferMethod::Conservative;
    double sourceIntegral = 0.0;     // of component 0 over the source grid
    double targetIntegral = 0.0;     // over the target grid
    double relativeDifference = 0.0; // |target - source| / |source| (0 when the source integral is 0)
    double coveredFraction = 1.0;    // of the source volume inside the target grid
    std::string description;
};

// The source field on the target grid (same quantity and time character).
Result<FieldData> transfer(const FieldData& source, const GridSupport& target, TransferMethod method, TransferReport* report = nullptr);

}  // namespace resonant::multiphysics
