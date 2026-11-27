#pragma once

// The acoustic optimization adapter (RES-OPT-020): what the acoustic domain
// lets the optimization framework change, calculate and evaluate with.
//
// Models, each recorded with every candidate:
//   acoustic.freefield.analytical  Analytical    point-source superposition in a homogeneous
//                                                medium; objects do not affect the field
//   acoustic.reflector.kirchhoff   Reduced       the free field plus one Kirchhoff
//                                                (physical-optics) reflection from every
//                                                reflector, integrated over its surface
//   acoustic.fdtd.staggered        HighFidelity  the full-wave staggered-grid FDTD solver on
//                                                the configured region (2D for a plane
//                                                region: a different, stated problem)
//
// Variables: emitter amplitude, phase, delay and position; array spacing,
// radius, element count, default amplitude and per-element amplitude, phase,
// delay and activation; manipulator geometry parameters, position and
// material; the analysis frequency (every source follows it) and the air
// temperature of an ideal-gas air medium.
//
// Quantities: pressure, phase and gain at the focus target; the error of the
// field at every target against its wanted value (point targets, and line,
// plane, surface and volume targets sampled at half a wavelength); the
// largest pressure at suppression targets; on the configured region the
// focal-to-background contrast, the energy outside the focal radius and the
// distance of the field's actual peak from the focus; on a line through the
// focus the -6 dB focal width and the peak sidelobe level; the drive effort
// of the sources; the clearance between sources and the thickness and size
// of manipulators. Definitions: docs/optimization/OBJECTIVE_FUNCTIONS.md.
#include "optimization/Evaluator.h"

#include <nlohmann/json.hpp>

#include <memory>

namespace resonant::acoustics {

inline constexpr const char* kAcousticAdapterId = "acoustic";

// Configuration (all optional):
//   focus         id of the focus target object (default: the first target that is not a suppression)
//   region        id of a field region (background, energy and peak quantities; required by FDTD)
//   focalRadius   m, radius around the focus counted as focal (default one wavelength)
//   lineHalfLength m, half length of the line through the focus (default four wavelengths)
//   lineSamples   samples on that line (default 241)
//   regionSamples samples per region axis for background quantities (default 41)
//   lineAxis      [x, y, z] direction of the line in world coordinates (default the region's first axis, else X)
//   targetSpacing m, sampling of non-point targets (default half a wavelength)
Result<std::shared_ptr<const optimization::DesignEvaluator>> bindAcousticAdapter(const nlohmann::json& snapshot,
                                                                                const nlohmann::json& configuration);

void registerAcousticOptimization(optimization::AdapterRegistry& registry);

}  // namespace resonant::acoustics
