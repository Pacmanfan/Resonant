// Active focusing of an array on a point: the per-element phase or time
// delay that makes every element's wave arrive at the target together.
//
// Two different controls, never confused with each other:
//
//   Phase       narrowband. Element i gets the phase  phi_i = k r_i  (mod
//               2 pi), which cancels its propagation phase -k r_i AT ONE
//               FREQUENCY. At another frequency the compensation is wrong.
//
//   TimeDelay   broadband. Element i is delayed by  tau_i = (r_max - r_i)/c,
//               so every wavefront arrives at the same instant whatever the
//               frequency. Delays are non-negative; the farthest element
//               has none.
//
// Neither guarantees a focus: the field that results must be calculated
// with the propagation model, and that is what the application shows.
#pragma once

#include "acoustics/Propagation.h"

namespace resonant::acoustics {

enum class FocusControl { Phase, TimeDelay };

// Amplitude weighting across the aperture.
enum class Taper {
    Uniform,   // weight 1
    Hann,      // 0.5 (1 + cos(pi d / d_max'))
    Hamming,   // 0.54 + 0.46 cos(pi d / d_max')
    Cosine,    // cos(pi d / (2 d_max'))
};

std::string_view toString(FocusControl control);
std::string_view toString(Taper taper);

struct FocusElement {
    int index = 0;
    double distance = 0.0;   // m, element to target
    double phaseRad = 0.0;   // Phase control: phase to set, in [0, 2 pi)
    double delaySec = 0.0;   // TimeDelay control: delay to set, >= 0
    double weight = 1.0;     // amplitude taper weight in (0, 1]
};

struct FocusSolution {
    FocusControl control = FocusControl::Phase;
    double frequencyHz = 0.0;     // the frequency the phases are valid for
    double soundSpeed = 0.0;
    std::vector<FocusElement> elements;
};

// Focus settings for the elements of an array placed in the world by
// `arrayWorld`, for a target point in world coordinates. The taper weight of
// an element depends on its distance d from the array's centroid; d_max' is
// slightly larger than the largest d so that edge elements keep a small
// non-zero weight.
FocusSolution computeFocus(const std::vector<ArrayElement>& elements, const Mat4& arrayWorld, const Vec3& target,
                           const Medium& medium, double frequencyHz, FocusControl control,
                           Taper taper = Taper::Uniform);

// The array settings with a focus solution applied as per-element
// overrides. Phase control sets each element's phase and clears its delay
// override; TimeDelay control sets the delay and clears the phase override.
// A non-uniform taper sets amplitude = default amplitude * weight; a
// uniform taper leaves amplitude overrides alone.
ArraySettings applyFocus(const ArraySettings& settings, const FocusSolution& solution, Taper taper);

}  // namespace resonant::acoustics
