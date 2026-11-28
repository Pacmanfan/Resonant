#include "mechanics/Damage.h"

#include <algorithm>
#include <cmath>

namespace resonant::mechanics {

Status DamageParameters::validate() const
{
    if (!(youngsModulus > 0.0))
        return Status::failure("the damage model needs E > 0");
    if (!(thresholdStrain > 0.0))
        return Status::failure("the damage model needs a positive threshold strain kappa0");
    if (law == "exponential" && !(softeningStrain > thresholdStrain))
        return Status::failure("the exponential damage law needs kappaF > kappa0");
    if (law == "power" && !(exponent > 0.0 && exponent < 1.0))
        return Status::failure("the power damage law needs 0 < n < 1");
    if (law != "exponential" && law != "power")
        return Status::failure("the damage law must be exponential or power");
    return Status::success();
}

double DamageParameters::damage(double k) const
{
    if (k <= thresholdStrain)
        return 0.0;
    if (law == "power")
        return 1.0 - std::pow(thresholdStrain / k, exponent);
    return 1.0 - (thresholdStrain / k) * std::exp(-(k - thresholdStrain) / (softeningStrain - thresholdStrain));
}

double DamageParameters::damageSlope(double k) const
{
    if (k <= thresholdStrain)
        return 0.0;
    if (law == "power")
        return exponent * std::pow(thresholdStrain, exponent) * std::pow(k, -exponent - 1.0);
    const double e = std::exp(-(k - thresholdStrain) / (softeningStrain - thresholdStrain));
    return (thresholdStrain / k) * e * (1.0 / k + 1.0 / (softeningStrain - thresholdStrain));
}

DamageResponse damageUpdate(const DamageParameters& p, const DamageState& c, double strain)
{
    DamageResponse r;
    r.state = c;
    const double e = std::abs(strain);
    const double E = p.youngsModulus;
    if (e > c.kappa && e > p.thresholdStrain) {
        r.loading = true;
        r.state.kappa = e;
        r.state.damage = std::max(c.damage, p.damage(e));
        r.state.dissipated = c.dissipated + 0.5 * E * strain * strain * (r.state.damage - c.damage);
        r.tangent = (1.0 - r.state.damage) * E - E * strain * p.damageSlope(e) * (strain >= 0.0 ? 1.0 : -1.0);
    } else {
        r.state.kappa = std::max(c.kappa, e);
        r.tangent = (1.0 - r.state.damage) * E;
    }
    r.stress = (1.0 - r.state.damage) * E * strain;
    r.storedEnergy = 0.5 * (1.0 - r.state.damage) * E * strain * strain;
    return r;
}

}  // namespace resonant::mechanics
