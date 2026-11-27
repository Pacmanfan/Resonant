#pragma once

// One-way coupling: sound radiated by a vibrating plate or membrane.
//
// Assumptions, stated in every result: the plate is set in an infinite rigid
// baffle; the surrounding fluid does not load the plate (its vibration is
// calculated in vacuo and then taken as given); linear acoustics in a
// homogeneous medium at rest. The pressure in front of the baffle is the
// Rayleigh integral
//
//   p(x) = (j omega rho / 2 pi) sum_e v_e A_e exp(-j k R_e) / R_e,      v_e = j omega w_e,
//
// over the elements (normal velocity v_e at the element centre, area A_e),
// convention e^{+j omega t}. Behind the baffle the field is not calculated.
// Two-way fluid-structure interaction is not implemented.
#include "acoustics/FieldGrid.h"
#include "mechanics/StructuralModel.h"

namespace resonant::coupling {

Result<std::shared_ptr<acoustics::FieldResult>> radiateFromPlate(const mechanics::StructuralModel& model, const Eigen::VectorXcd& displacement,
                                                                 double frequencyHz, const acoustics::Medium& medium,
                                                                 const acoustics::GridFrame& frame, const std::string& name);

}  // namespace resonant::coupling
