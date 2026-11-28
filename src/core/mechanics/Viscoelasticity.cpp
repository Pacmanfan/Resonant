#include "mechanics/Viscoelasticity.h"

#include <cmath>

namespace resonant::mechanics {

Status ZenerParameters::validate() const
{
    if (!(relaxedModulus > 0.0) || !(branchModulus > 0.0) || !(relaxationTime > 0.0))
        return Status::failure("the standard linear solid needs E_inf > 0, E1 > 0 and tau > 0");
    return Status::success();
}

double ZenerParameters::lossModulus(double w) const
{
    return branchModulus * w * relaxationTime / (1.0 + w * w * relaxationTime * relaxationTime);
}

double ZenerParameters::storageModulus(double w) const
{
    const double x = w * relaxationTime;
    return relaxedModulus + branchModulus * x * x / (1.0 + x * x);
}

Result<std::vector<ZenerPoint>> zenerResponse(const ZenerParameters& p, const std::vector<double>& t, const std::vector<double>& eps)
{
    using R = Result<std::vector<ZenerPoint>>;
    if (Status v = p.validate(); !v)
        return R::failure(v.message());
    if (t.empty() || t.size() != eps.size())
        return R::failure("one strain per time");
    std::vector<ZenerPoint> out;
    ZenerPoint z;
    z.time = t[0];
    z.strain = eps[0];
    z.branchStress = p.branchModulus * eps[0];  // an instantaneous strain loads the branch fully
    z.stress = p.relaxedModulus * eps[0] + z.branchStress;
    z.storedEnergy = 0.5 * p.relaxedModulus * eps[0] * eps[0] + 0.5 * z.branchStress * z.branchStress / p.branchModulus;
    out.push_back(z);
    const double tau = p.relaxationTime, eta = p.branchModulus * tau;
    for (size_t i = 1; i < t.size(); ++i) {
        const double dt = t[i] - t[i - 1];
        if (!(dt >= 0.0))
            return R::failure("times must ascend");
        const double de = eps[i] - eps[i - 1];
        ZenerPoint n;
        n.time = t[i];
        n.strain = eps[i];
        const double q0 = z.branchStress;
        if (dt == 0.0) {
            n.branchStress = q0 + p.branchModulus * de;  // a jump
            n.dissipated = z.dissipated;
        } else {
            const double a = std::exp(-dt / tau), r = de / dt;
            n.branchStress = a * q0 + p.branchModulus * tau * r * (1.0 - a);
            // Exact integral of q^2 / eta over the step: q(s) = c + (q0 - c) e^{-s/tau}, c = E1 tau r.
            const double c = p.branchModulus * tau * r, d = q0 - c;
            const double integral = c * c * dt + 2.0 * c * d * tau * (1.0 - a) + d * d * 0.5 * tau * (1.0 - a * a);
            n.dissipated = z.dissipated + integral / eta;
        }
        n.stress = p.relaxedModulus * eps[i] + n.branchStress;
        n.storedEnergy = 0.5 * p.relaxedModulus * eps[i] * eps[i] + 0.5 * n.branchStress * n.branchStress / p.branchModulus;
        out.push_back(n);
        z = n;
    }
    return out;
}

}  // namespace resonant::mechanics
