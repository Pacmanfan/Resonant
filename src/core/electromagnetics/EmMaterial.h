#pragma once

// Electromagnetic material properties at a frequency.
//
// A record's relative permittivity, relative permeability, conductivity
// and dielectric loss tangent (each with provenance and optional frequency
// table) give the complex constitutive parameters
//
//   epsilon_c = epsilon0 epsilon_r (1 - j tan delta) - j sigma / omega
//   mu_c      = mu0 mu_r
//
// (e^{+j omega t}). Magnetic loss and anisotropy are not represented.
#include "electromagnetics/Conventions.h"
#include "foundation/Result.h"
#include "materials/Material.h"

#include <string>

namespace resonant::em {

enum class MaterialClass {
    Dielectric,       // sigma / (omega epsilon) < 0.01
    LossyDielectric,  // in between
    GoodConductor,    // sigma / (omega epsilon) > 100: described by its skin depth
};
std::string_view toString(MaterialClass c);

struct EmMaterial {
    std::string name;
    double relativePermittivity = 1.0;
    double relativePermeability = 1.0;
    double conductivity = 0.0;   // S/m
    double lossTangent = 0.0;
    double frequencyHz = 0.0;    // the frequency the values were taken at
    bool outsideValidity = false;  // the frequency lies outside a stated validity range
    std::string provenance;

    Complex permittivity(double frequencyHz) const;   // F/m, complex
    double permeability() const { return kMu0 * relativePermeability; }
    // sigma / (omega epsilon') + tan delta: the ratio of loss to displacement current.
    double lossRatio(double frequencyHz) const;
    MaterialClass classify(double frequencyHz) const;
    Complex wavenumber(double frequencyHz) const;     // k = omega sqrt(mu epsilon_c), Im <= 0
    Complex impedance(double frequencyHz) const;      // sqrt(mu / epsilon_c)
    double skinDepth(double frequencyHz) const;       // m, exact for any loss: 1 / |Im k|
    double surfaceResistance(double frequencyHz) const;  // ohm per square, good conductor: 1 / (sigma delta)
};

// The electromagnetic properties of a record at a frequency (frequency
// tables interpolated). Missing permittivity or permeability is refused.
Result<EmMaterial> emMaterialOf(const Material& material, double frequencyHz);

// Reflection and transmission of a plane wave at normal incidence from
// medium 1 into medium 2 (field coefficients).
Complex normalReflection(const Complex& eta1, const Complex& eta2);
Complex normalTransmission(const Complex& eta1, const Complex& eta2);

}  // namespace resonant::em
