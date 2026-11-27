#include "mechanics/ElasticMaterial.h"

#include <cmath>

namespace resonant::mechanics {

Status ElasticMaterial::validate() const
{
    if (!(std::isfinite(density) && density > 0.0))
        return Status::failure("material '" + name + "' needs a positive density");
    if (!(std::isfinite(youngsModulus) && youngsModulus > 0.0))
        return Status::failure("material '" + name + "' needs a positive Young's modulus");
    // Thermodynamic limits of an isotropic solid.
    if (!(std::isfinite(poissonsRatio) && poissonsRatio > -1.0 && poissonsRatio < 0.5))
        return Status::failure("material '" + name + "': Poisson's ratio must lie between -1 and 0.5 (exclusive)");
    if (!(std::isfinite(lossFactor) && lossFactor >= 0.0 && lossFactor < 2.0))
        return Status::failure("material '" + name + "': the loss factor must be between 0 and 2");
    return Status::success();
}

double ElasticMaterial::barSpeed() const { return std::sqrt(youngsModulus / density); }
double ElasticMaterial::longitudinalSpeed() const { return std::sqrt((lameLambda() + 2.0 * shearModulus()) / density); }
double ElasticMaterial::shearSpeed() const { return std::sqrt(shearModulus() / density); }

double ElasticMaterial::plateBendingStiffness(double thickness) const
{
    return youngsModulus * thickness * thickness * thickness / (12.0 * (1.0 - poissonsRatio * poissonsRatio));
}

Result<ElasticMaterial> elasticMaterialOf(const Material& material)
{
    ElasticMaterial m;
    m.name = material.name();
    std::string missing;
    const auto read = [&](const char* key, const char* label, double& into) {
        const PropertyValue* value = material.property(PhysicsDomain::Mechanical, key);
        if (!value) {
            missing += missing.empty() ? label : std::string(", ") + label;
            return;
        }
        into = value->value;
        if (!value->source.empty() && m.provenance.find(value->source) == std::string::npos)
            m.provenance += (m.provenance.empty() ? "" : " ") + value->source;
    };
    read("density", "density", m.density);
    read("youngsModulus", "Young's modulus", m.youngsModulus);
    read("poissonsRatio", "Poisson's ratio", m.poissonsRatio);
    if (!missing.empty())
        return Result<ElasticMaterial>::failure("material '" + m.name + "' has no mechanical " + missing);
    if (const PropertyValue* loss = material.property(PhysicsDomain::Mechanical, "lossFactor")) {
        m.lossFactor = loss->value;
        m.lossFactorStated = true;
    }
    if (const Status status = m.validate(); !status)
        return Result<ElasticMaterial>::failure(status.message());
    return m;
}

}  // namespace resonant::mechanics
