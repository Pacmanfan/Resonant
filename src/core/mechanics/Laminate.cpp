#include "mechanics/Laminate.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace resonant::mechanics {

Ply Ply::isotropic(std::string name, double thickness, double youngs, double poisson, double density, double alpha, double lossFactor)
{
    Ply p;
    p.name = std::move(name);
    p.thickness = thickness;
    p.E1 = p.E2 = youngs;
    p.nu12 = poisson;
    p.G12 = youngs / (2.0 * (1.0 + poisson));
    p.density = density;
    p.alpha1 = p.alpha2 = alpha;
    p.lossFactor = lossFactor;
    return p;
}

Status Ply::validate() const
{
    if (!(thickness > 0.0) || !std::isfinite(thickness))
        return Status::failure("the ply '" + name + "' needs a positive thickness");
    if (!(E1 > 0.0) || !(E2 > 0.0) || !(G12 > 0.0))
        return Status::failure("the ply '" + name + "' needs positive moduli");
    if (!(density >= 0.0))
        return Status::failure("the ply '" + name + "' has a negative density");
    // Positive definiteness of the in-plane compliance: nu12^2 < E1 / E2.
    if (!(nu12 * nu12 < E1 / E2))
        return Status::failure("the ply '" + name + "' has a Poisson's ratio that makes its stiffness indefinite");
    return Status::success();
}

namespace {

Eigen::Matrix3d reducedStiffness(const Ply& p)
{
    const double nu21 = p.nu12 * p.E2 / p.E1;
    const double d = 1.0 - p.nu12 * nu21;
    Eigen::Matrix3d q = Eigen::Matrix3d::Zero();
    q(0, 0) = p.E1 / d;
    q(1, 1) = p.E2 / d;
    q(0, 1) = q(1, 0) = p.nu12 * p.E2 / d;
    q(2, 2) = p.G12;
    return q;
}

Eigen::Matrix3d rotated(const Eigen::Matrix3d& q, double theta)
{
    const double m = std::cos(theta), n = std::sin(theta);
    const double m2 = m * m, n2 = n * n, m4 = m2 * m2, n4 = n2 * n2, mn2 = m2 * n2;
    const double Q11 = q(0, 0), Q22 = q(1, 1), Q12 = q(0, 1), Q66 = q(2, 2);
    Eigen::Matrix3d b;
    b(0, 0) = Q11 * m4 + 2.0 * (Q12 + 2.0 * Q66) * mn2 + Q22 * n4;
    b(1, 1) = Q11 * n4 + 2.0 * (Q12 + 2.0 * Q66) * mn2 + Q22 * m4;
    b(0, 1) = b(1, 0) = (Q11 + Q22 - 4.0 * Q66) * mn2 + Q12 * (m4 + n4);
    b(2, 2) = (Q11 + Q22 - 2.0 * Q12 - 2.0 * Q66) * mn2 + Q66 * (m4 + n4);
    b(0, 2) = b(2, 0) = (Q11 - Q12 - 2.0 * Q66) * m2 * m * n + (Q12 - Q22 + 2.0 * Q66) * m * n2 * n;
    b(1, 2) = b(2, 1) = (Q11 - Q12 - 2.0 * Q66) * m * n2 * n + (Q12 - Q22 + 2.0 * Q66) * m2 * m * n;
    return b;
}

}  // namespace

Eigen::Matrix3d Laminate::Qbar(size_t ply) const
{
    return rotated(reducedStiffness(plies[ply]), plies[ply].angle);
}

Eigen::Vector3d Laminate::alpha(size_t ply) const
{
    const Ply& p = plies[ply];
    const double m = std::cos(p.angle), n = std::sin(p.angle);
    return {p.alpha1 * m * m + p.alpha2 * n * n, p.alpha1 * n * n + p.alpha2 * m * m, 2.0 * (p.alpha1 - p.alpha2) * m * n};
}

Result<Laminate> laminate(const std::vector<Ply>& plies)
{
    if (plies.empty())
        return Result<Laminate>::failure("a laminate needs at least one ply");
    Laminate l;
    l.plies = plies;
    for (const auto& p : plies) {
        if (Status s = p.validate(); !s)
            return Result<Laminate>::failure(s.message());
        l.thickness += p.thickness;
        l.massPerArea += p.density * p.thickness;
    }
    l.z.push_back(-l.thickness / 2.0);
    for (const auto& p : plies)
        l.z.push_back(l.z.back() + p.thickness);
    l.A.setZero();
    l.B.setZero();
    l.D.setZero();
    for (size_t k = 0; k < plies.size(); ++k) {
        const Eigen::Matrix3d q = l.Qbar(k);
        const double z0 = l.z[k], z1 = l.z[k + 1];
        l.A += q * (z1 - z0);
        l.B += q * (z1 * z1 - z0 * z0) / 2.0;
        l.D += q * (z1 * z1 * z1 - z0 * z0 * z0) / 3.0;
    }
    const Eigen::Matrix3d a = l.A.inverse();
    l.Ex = 1.0 / (l.thickness * a(0, 0));
    l.Ey = 1.0 / (l.thickness * a(1, 1));
    l.Gxy = 1.0 / (l.thickness * a(2, 2));
    l.nuxy = -a(0, 1) / a(0, 0);
    l.Dstar = l.D - l.B * a * l.B;
    l.neutralSurface = l.B(0, 0) / l.A(0, 0);
    l.symmetric = l.B.cwiseAbs().maxCoeff() <= 1e-9 * l.A.cwiseAbs().maxCoeff() * l.thickness;
    return l;
}

ThermalResponse thermalResponse(const Laminate& l, double deltaT)
{
    ThermalResponse out;
    out.deltaT = deltaT;
    Eigen::Vector3d NT = Eigen::Vector3d::Zero(), MT = Eigen::Vector3d::Zero();
    for (size_t k = 0; k < l.plies.size(); ++k) {
        const Eigen::Vector3d qa = l.Qbar(k) * l.alpha(k) * deltaT;
        const double z0 = l.z[k], z1 = l.z[k + 1];
        NT += qa * (z1 - z0);
        MT += qa * (z1 * z1 - z0 * z0) / 2.0;
    }
    Eigen::Matrix<double, 6, 6> abd;
    abd << l.A, l.B, l.B, l.D;
    Eigen::Matrix<double, 6, 1> load;
    load << NT, MT;
    const Eigen::Matrix<double, 6, 1> def = abd.fullPivLu().solve(load);
    out.strain = def.head<3>();
    out.curvature = def.tail<3>();
    for (size_t k = 0; k < l.plies.size(); ++k) {
        const Eigen::Matrix3d q = l.Qbar(k);
        const Eigen::Vector3d free = l.alpha(k) * deltaT;
        ThermalResponse::PlyStress s;
        s.bottom = q * (out.strain + l.z[k] * out.curvature - free);
        s.top = q * (out.strain + l.z[k + 1] * out.curvature - free);
        out.stress.push_back(s);
    }
    return out;
}

BeamSection beamSection(const std::vector<Ply>& plies)
{
    BeamSection out;
    double z = 0.0, ea = 0.0, eaz = 0.0;
    for (const auto& p : plies) {
        const double m = std::cos(p.angle), n = std::sin(p.angle);
        const double s11 = m * m * m * m / p.E1 + (1.0 / p.G12 - 2.0 * p.nu12 / p.E1) * m * m * n * n + n * n * n * n / p.E2;
        const double ex = 1.0 / s11;
        out.axialModulus.push_back(ex);
        out.centroid.push_back(z + p.thickness / 2.0);
        ea += ex * p.thickness;
        eaz += ex * p.thickness * (z + p.thickness / 2.0);
        out.massPerArea += p.density * p.thickness;
        z += p.thickness;
    }
    out.neutralAxis = eaz / ea;
    double lossWeighted = 0.0;
    for (size_t i = 0; i < plies.size(); ++i) {
        const double h = plies[i].thickness, d = out.centroid[i] - out.neutralAxis;
        const double ei = out.axialModulus[i] * (h * h * h / 12.0 + h * d * d);
        out.bendingStiffness += ei;
        lossWeighted += plies[i].lossFactor * ei;
    }
    out.lossFactor = lossWeighted / out.bendingStiffness;
    return out;
}

double cantileverRoot(int n)
{
    static const double roots[] = {1.875104068711961, 4.694091132974175, 7.854757438237613, 10.995540734875467, 14.137168391046471};
    if (n >= 1 && n <= 5)
        return roots[n - 1];
    return (2.0 * n - 1.0) * std::numbers::pi / 2.0;  // the asymptote, exact to 1e-9 beyond the fifth
}

std::vector<double> cantileverFrequencies(double bendingStiffness, double massPerLength, double length, int count)
{
    std::vector<double> out;
    for (int n = 1; n <= count; ++n) {
        const double b = cantileverRoot(n);
        out.push_back(b * b / (2.0 * std::numbers::pi * length * length) * std::sqrt(bendingStiffness / massPerLength));
    }
    return out;
}

std::vector<PlateMode> simplySupportedPlateModes(const Eigen::Matrix3d& D, double massPerArea, double a, double b, int count)
{
    std::vector<PlateMode> modes;
    const int range = std::max(4, count + 2);
    for (int m = 1; m <= range; ++m)
        for (int n = 1; n <= range; ++n) {
            const double x = m / a, y = n / b;
            const double k4 = D(0, 0) * std::pow(x, 4) + 2.0 * (D(0, 1) + 2.0 * D(2, 2)) * x * x * y * y + D(1, 1) * std::pow(y, 4);
            const double omega = std::numbers::pi * std::numbers::pi * std::sqrt(k4 / massPerArea);
            modes.push_back({m, n, omega / (2.0 * std::numbers::pi)});
        }
    std::sort(modes.begin(), modes.end(), [](const PlateMode& p, const PlateMode& q) { return p.frequency < q.frequency; });
    if (static_cast<int>(modes.size()) > count)
        modes.resize(static_cast<size_t>(count));
    return modes;
}

}  // namespace resonant::mechanics
