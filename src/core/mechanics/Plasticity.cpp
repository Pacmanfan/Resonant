#include "mechanics/Plasticity.h"

#include <cmath>

namespace resonant::mechanics {

namespace {

const double kRoot2 = std::sqrt(2.0);
const double kRoot23 = std::sqrt(2.0 / 3.0);

// Voigt (engineering) strain to Mandel and back; Voigt stress to Mandel and back.
Vec6 strainToMandel(const Vec6& e)
{
    Vec6 m = e;
    m.tail<3>() /= kRoot2;
    return m;
}
Vec6 stressToMandel(const Vec6& s)
{
    Vec6 m = s;
    m.tail<3>() *= kRoot2;
    return m;
}
Vec6 mandelToStress(const Vec6& m)
{
    Vec6 s = m;
    s.tail<3>() /= kRoot2;
    return s;
}
Vec6 mandelToStrain(const Vec6& m)
{
    Vec6 e = m;
    e.tail<3>() *= kRoot2;
    return e;
}

Mat6 deviatoric()
{
    Mat6 P = Mat6::Identity();
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            P(i, j) -= 1.0 / 3.0;
    return P;
}

}  // namespace

Status J2Parameters::validate() const
{
    if (!(youngsModulus > 0.0) || !(poissonsRatio > -1.0 && poissonsRatio < 0.5))
        return Status::failure("the material needs E > 0 and -1 < nu < 0.5");
    if (!(yieldStress > 0.0))
        return Status::failure("the yield stress must be positive");
    if (isotropicHardening < 0.0 || kinematicHardening < 0.0)
        return Status::failure("hardening moduli must not be negative (softening is not modeled)");
    return Status::success();
}

J2Response j2Update(const J2Parameters& p, const J2State& c, const Vec6& strain)
{
    const double E = p.youngsModulus, nu = p.poissonsRatio;
    const double G = E / (2.0 * (1.0 + nu)), K = E / (3.0 * (1.0 - 2.0 * nu));
    const double H = p.isotropicHardening, Hk = p.kinematicHardening;
    const Mat6 P = deviatoric();
    Vec6 m;
    m << 1, 1, 1, 0, 0, 0;
    const Mat6 C = K * m * m.transpose() + 2.0 * G * P;  // Mandel
    const Vec6 epsE = strainToMandel(strain - c.plasticStrain);
    const Vec6 sigmaTrial = C * epsE;
    const Vec6 alpha = stressToMandel(c.backStress);
    const Vec6 xi = P * sigmaTrial - alpha;
    const double norm = xi.norm();
    const double radius = kRoot23 * (p.yieldStress + H * c.equivalentPlasticStrain);
    J2Response r;
    r.state = c;
    if (norm - radius <= 1e-12 * radius) {
        r.stress = mandelToStress(sigmaTrial);
        // Voigt tangent: sigma_V = D^-1 C_M E eps_V with E = diag(1, 1, 1, 1/sqrt2, ...), D^-1 the same.
        Mat6 S = Mat6::Identity();
        S.diagonal().tail<3>().setConstant(1.0 / kRoot2);
        r.tangent = S * C * S;
        r.elasticEnergy = 0.5 * sigmaTrial.dot(epsE);
    } else {
        r.plastic = true;
        const double dgamma = (norm - radius) / (2.0 * G + (2.0 / 3.0) * (H + Hk));
        const Vec6 n = xi / norm;
        const Vec6 sigma = sigmaTrial - 2.0 * G * dgamma * n;
        const Vec6 dEpsP = dgamma * n;  // Mandel
        r.state.plasticStrain = c.plasticStrain + mandelToStrain(dEpsP);
        const Vec6 alphaNew = alpha + (2.0 / 3.0) * Hk * dgamma * n;
        r.state.backStress = mandelToStress(alphaNew);
        r.state.equivalentPlasticStrain = c.equivalentPlasticStrain + kRoot23 * dgamma;
        r.stress = mandelToStress(sigma);
        const double beta = 4.0 * G * G / (2.0 * G + (2.0 / 3.0) * (H + Hk));
        const double theta = 4.0 * G * G * dgamma / norm;
        const Mat6 Cep = C - beta * n * n.transpose() - theta * (P - n * n.transpose());
        Mat6 S = Mat6::Identity();
        S.diagonal().tail<3>().setConstant(1.0 / kRoot2);
        r.tangent = S * Cep * S;
        const Vec6 epsENew = strainToMandel(strain - r.state.plasticStrain);
        r.elasticEnergy = 0.5 * sigma.dot(epsENew);
        // Dissipation: with linear hardening the plastic power minus the stored hardening power is exactly
        // sigma_y de_p (the hardening parts are stored), so the increment is sigma_y de_p.
        r.state.dissipated = c.dissipated + p.yieldStress * kRoot23 * dgamma;
    }
    const Vec6 a = stressToMandel(r.state.backStress);
    r.hardeningEnergy = 0.5 * H * r.state.equivalentPlasticStrain * r.state.equivalentPlasticStrain + (Hk > 0.0 ? 0.75 * a.squaredNorm() / Hk : 0.0);
    return r;
}

Result<UniaxialPoint> j2Uniaxial(const J2Parameters& p, const J2State& c, double axial, const Vec6& guess)
{
    using R = Result<UniaxialPoint>;
    UniaxialPoint u;
    u.strain = guess;
    u.strain(0) = axial;
    if (guess.isZero()) {
        // The elastic guess: lateral contraction.
        u.strain(1) = u.strain(2) = -p.poissonsRatio * axial;
    }
    for (int it = 0; it < 50; ++it) {
        u.response = j2Update(p, c, u.strain);
        const Eigen::Matrix<double, 5, 1> res = u.response.stress.tail<5>();
        u.iterations = it;
        if (res.norm() <= 1e-12 * std::max(1.0, std::abs(u.response.stress(0))) + 1e-9)
            break;
        const Eigen::Matrix<double, 5, 5> Kt = u.response.tangent.bottomRightCorner<5, 5>();
        u.strain.tail<5>() -= Kt.ldlt().solve(res);
        if (it == 49)
            return R::failure("the uniaxial-stress iteration did not converge");
    }
    u.stress = u.response.stress(0);
    // Condensed tangent: d s1 / d e1 with the other stresses held at zero.
    const Mat6& T = u.response.tangent;
    const Eigen::Matrix<double, 5, 5> Kt = T.bottomRightCorner<5, 5>();
    const Eigen::Matrix<double, 5, 1> c1 = T.block<5, 1>(1, 0);
    u.tangent = T(0, 0) - T.block<1, 5>(0, 1) * Kt.ldlt().solve(c1);
    return u;
}

Status BarSpec::validate() const
{
    if (Status v = material.validate(); !v)
        return v;
    if (!(length > 0.0) || !(loadPoint > 0.0 && loadPoint < length) || !(area > 0.0))
        return Status::failure("the bar needs a length, an interior load point and a positive section");
    if (loads.empty())
        return Status::failure("the load path is empty");
    return Status::success();
}

Result<std::vector<BarStep>> j2Bar(const BarSpec& s)
{
    using R = Result<std::vector<BarStep>>;
    if (Status v = s.validate(); !v)
        return R::failure(v.message());
    const double a = s.loadPoint, b = s.length - a, A = s.area;
    J2State c1, c2;  // committed states of the two segments
    Vec6 g1 = Vec6::Zero(), g2 = Vec6::Zero();
    double u = 0.0, work = 0.0, previousLoad = 0.0;
    std::vector<BarStep> out;
    for (double P : s.loads) {
        // Equilibrium at the load point: N1(u / a) - N2(-u / b) = P (segment 2 shortens by u).
        BarStep step;
        step.load = P;
        double x = u;
        Result<UniaxialPoint> p1 = Result<UniaxialPoint>::failure(""), p2 = Result<UniaxialPoint>::failure("");
        bool converged = false;
        for (int it = 0; it < 40; ++it) {
            p1 = j2Uniaxial(s.material, c1, x / a, g1);
            p2 = j2Uniaxial(s.material, c2, -x / b, g2);
            if (!p1 || !p2)
                return R::failure(p1 ? p2.message() : p1.message());
            const double residual = P - (A * p1.value().stress - A * p2.value().stress);
            step.iterations = it + 1;
            step.residual = std::abs(residual);
            if (std::abs(residual) <= s.tolerance * std::max(std::abs(P), s.material.yieldStress * A)) {
                converged = true;
                break;
            }
            const double k = A * p1.value().tangent / a + A * p2.value().tangent / b;
            x += residual / k;
        }
        step.converged = converged;
        if (!converged) {
            out.push_back(step);  // reported, not committed
            break;
        }
        c1 = p1.value().response.state;
        c2 = p2.value().response.state;
        g1 = p1.value().strain;
        g2 = p2.value().strain;
        work += 0.5 * (P + previousLoad) * (x - u);
        u = x;
        previousLoad = P;
        step.displacement = u;
        step.force1 = A * p1.value().stress;
        step.force2 = A * p2.value().stress;
        step.plasticStrain1 = c1.plasticStrain(0);
        step.plasticStrain2 = c2.plasticStrain(0);
        step.externalWork = work;
        step.elasticEnergy = (p1.value().response.elasticEnergy * a + p2.value().response.elasticEnergy * b) * A;
        step.hardeningEnergy = (p1.value().response.hardeningEnergy * a + p2.value().response.hardeningEnergy * b) * A;
        step.dissipated = (c1.dissipated * a + c2.dissipated * b) * A;
        out.push_back(step);
    }
    return out;
}

}  // namespace resonant::mechanics
