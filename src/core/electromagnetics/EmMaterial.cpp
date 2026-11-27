#include "electromagnetics/EmMaterial.h"

#include <cmath>
#include <limits>

namespace resonant::em {

std::string_view toString(MaterialClass c)
{
    switch (c) {
    case MaterialClass::Dielectric: return "dielectric";
    case MaterialClass::LossyDielectric: return "lossy dielectric";
    case MaterialClass::GoodConductor: return "good conductor";
    }
    return "";
}

Complex EmMaterial::permittivity(double f) const
{
    const double omega = 2.0 * kPi * f;
    return Complex(kEpsilon0 * relativePermittivity, -kEpsilon0 * relativePermittivity * lossTangent - conductivity / omega);
}

double EmMaterial::lossRatio(double f) const
{
    return conductivity / (2.0 * kPi * f * kEpsilon0 * relativePermittivity) + lossTangent;
}

MaterialClass EmMaterial::classify(double f) const
{
    const double r = lossRatio(f);
    return r > 100.0 ? MaterialClass::GoodConductor : r < 0.01 ? MaterialClass::Dielectric : MaterialClass::LossyDielectric;
}

Complex EmMaterial::wavenumber(double f) const
{
    const double omega = 2.0 * kPi * f;
    Complex k = omega * std::sqrt(permeability() * permittivity(f));
    if (k.imag() > 0.0)
        k = -k;
    if (k.real() < 0.0)
        k = Complex(-k.real(), k.imag());
    return k;
}

Complex EmMaterial::impedance(double f) const
{
    Complex z = std::sqrt(permeability() / permittivity(f));
    if (z.real() < 0.0)
        z = -z;
    return z;
}

double EmMaterial::skinDepth(double f) const
{
    const double attenuation = -wavenumber(f).imag();
    return attenuation > 0.0 ? 1.0 / attenuation : std::numeric_limits<double>::infinity();
}

double EmMaterial::surfaceResistance(double f) const
{
    return conductivity > 0.0 ? 1.0 / (conductivity * skinDepth(f)) : std::numeric_limits<double>::infinity();
}

Result<EmMaterial> emMaterialOf(const Material& material, double f)
{
    EmMaterial m;
    m.name = material.name();
    m.frequencyHz = f;
    const PropertyValue* er = material.property(PhysicsDomain::Electromagnetic, "relativePermittivity");
    const PropertyValue* mr = material.property(PhysicsDomain::Electromagnetic, "relativePermeability");
    if (!er || !mr)
        return Result<EmMaterial>::failure("material '" + m.name + "' has no electromagnetic permittivity and permeability");
    const auto read = [&](const PropertyValue* v, double& into) {
        into = v->valueAt(f);
        if (v->validity.minFrequencyHz && f < *v->validity.minFrequencyHz)
            m.outsideValidity = true;
        if (v->validity.maxFrequencyHz && f > *v->validity.maxFrequencyHz)
            m.outsideValidity = true;
        if (!v->source.empty() && m.provenance.find(v->source) == std::string::npos)
            m.provenance += (m.provenance.empty() ? "" : " ") + v->source;
    };
    read(er, m.relativePermittivity);
    read(mr, m.relativePermeability);
    if (const PropertyValue* s = material.property(PhysicsDomain::Electromagnetic, "conductivity"))
        read(s, m.conductivity);
    if (const PropertyValue* t = material.property(PhysicsDomain::Electromagnetic, "lossTangent"))
        read(t, m.lossTangent);
    if (!(m.relativePermittivity > 0.0 && m.relativePermeability > 0.0 && m.conductivity >= 0.0 && m.lossTangent >= 0.0))
        return Result<EmMaterial>::failure("material '" + m.name + "' has invalid electromagnetic properties");
    return m;
}

Complex normalReflection(const Complex& eta1, const Complex& eta2)
{
    return (eta2 - eta1) / (eta2 + eta1);
}

Complex normalTransmission(const Complex& eta1, const Complex& eta2)
{
    return 2.0 * eta2 / (eta2 + eta1);
}

}  // namespace resonant::em
