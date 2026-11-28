#pragma once

// Layered media at oblique incidence (RES-META-003, RES-META-005): isotropic
// layers (complex permittivity and permeability, e^{+jwt}: losses have
// negative imaginary parts) between vacuum half-spaces, TE (E along the
// layers' y) and TM (H along y); a uniaxial homogenized slab (optic axis
// normal to the layers: eps_xx = eps_yy = eps_par, eps_zz = eps_perp); and the
// Bloch wavenumber of a periodic cell at a fixed transverse wavenumber.
// Each layer's matrix [cos d, j z sin d; j sin d / z, cos d] acts on the
// tangential (E, eta0 H); at normal incidence it is the layered-media one.

#include "foundation/Types.h"

#include <vector>

namespace resonant::em {

enum class Polarization { TE, TM };

struct IsotropicLayer {
    double thickness = 0.0;
    Complex permittivity{1.0, 0.0};
    Complex permeability{1.0, 0.0};
};

struct ObliqueResponse {
    Complex r, t;
    double reflectance = 0.0, transmittance = 0.0, absorptance = 0.0;
};

ObliqueResponse obliqueStack(const std::vector<IsotropicLayer>& layers, double frequencyHz, double angleRad, Polarization polarization);
ObliqueResponse uniaxialSlab(double thickness, Complex epsParallel, Complex epsPerpendicular, double frequencyHz, double angleRad,
                             Polarization polarization);
// cos(K a) = (M11 + M22) / 2 of the cell at the transverse wavenumber of the angle in vacuum; K (1/m),
// Re K a in [0, pi] (the phase per cell), Im K a >= 0 (the attenuation per cell, Np).
Complex obliqueBloch(const std::vector<IsotropicLayer>& cell, double frequencyHz, double angleRad, Polarization polarization);

}  // namespace resonant::em
