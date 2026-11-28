#include "mechanics/ThermalLoads.h"

#include <algorithm>
#include <cmath>

namespace resonant::mechanics {
namespace {

Status checkInput(const StructuralModel& model, const ThermalLoadInput& input)
{
    if (model.kind != ElementKind::Solid && model.kind != ElementKind::Frame)
        return Status::failure("thermal loads are implemented for solid and frame models, not for " + std::string(toString(model.kind)));
    if (input.elementTemperature.size() != model.elements.size())
        return Status::failure("one temperature per element is needed");
    if (input.expansion.size() != model.materials.size())
        return Status::failure("one expansion coefficient per material is needed");
    for (double t : input.elementTemperature)
        if (!std::isfinite(t) || t <= 0.0)
            return Status::failure("element temperatures must be finite and positive (kelvin)");
    return Status::success();
}

std::array<Vec3, 8> corners(const StructuralModel& model, const Element& e)
{
    std::array<Vec3, 8> x;
    for (size_t q = 0; q < 8; ++q)
        x[q] = model.nodes[static_cast<size_t>(e.nodes[q])];
    return x;
}

Eigen::Matrix<double, 6, 1> freeStrain(double alpha, double deltaT)
{
    Eigen::Matrix<double, 6, 1> eps = Eigen::Matrix<double, 6, 1>::Zero();
    eps.head<3>().setConstant(alpha * deltaT);
    return eps;
}

}  // namespace

Result<Eigen::VectorXd> thermalLoadVector(const StructuralModel& model, const ThermalLoadInput& input)
{
    if (Status s = checkInput(model, input); !s)
        return Result<Eigen::VectorXd>::failure(s.message());
    Eigen::VectorXd f = Eigen::VectorXd::Zero(static_cast<Eigen::Index>(model.dofCount()));
    static const double g = 1.0 / std::sqrt(3.0);
    for (size_t ei = 0; ei < model.elements.size(); ++ei) {
        const Element& e = model.elements[ei];
        const ElasticMaterial& m = model.materials[static_cast<size_t>(e.material)];
        const double alpha = input.expansion[static_cast<size_t>(e.material)];
        const double dT = input.elementTemperature[ei] - input.referenceTemperature;
        if (alpha == 0.0 || dT == 0.0)
            continue;
        if (model.kind == ElementKind::Solid) {
            const auto x = corners(model, e);
            const Eigen::Matrix<double, 6, 1> sigma = solidElasticity(m) * freeStrain(alpha, dT);
            Eigen::Matrix<double, 24, 1> fe = Eigen::Matrix<double, 24, 1>::Zero();
            for (double xi : {-g, g})
                for (double eta : {-g, g})
                    for (double zeta : {-g, g}) {
                        double detJ = 0.0;
                        const Eigen::Matrix<double, 6, 24> b = solidStrainMatrix(x, xi, eta, zeta, detJ);
                        fe += b.transpose() * sigma * detJ;  // Gauss weights are 1
                    }
            for (size_t q = 0; q < 8; ++q)
                for (int d = 0; d < 3; ++d)
                    f(static_cast<Eigen::Index>(model.dof(static_cast<size_t>(e.nodes[q]), d))) += fe(static_cast<Eigen::Index>(3 * q + static_cast<size_t>(d)));
        } else {
            const size_t a = static_cast<size_t>(e.nodes[0]), b = static_cast<size_t>(e.nodes[1]);
            const Vec3 axis = (model.nodes[b] - model.nodes[a]).normalized();
            const double n = m.youngsModulus * model.sections[static_cast<size_t>(e.section)].area * alpha * dT;
            for (int d = 0; d < 3; ++d) {
                f(static_cast<Eigen::Index>(model.dof(a, d))) -= n * axis[d];
                f(static_cast<Eigen::Index>(model.dof(b, d))) += n * axis[d];
            }
        }
    }
    return f;
}

Result<ThermalStressResult> thermalStress(const StructuralModel& model, const ThermalLoadInput& input, const Eigen::VectorXd& u)
{
    if (Status s = checkInput(model, input); !s)
        return Result<ThermalStressResult>::failure(s.message());
    if (u.size() != static_cast<Eigen::Index>(model.dofCount()))
        return Result<ThermalStressResult>::failure("the displacement vector does not match the model");
    ThermalStressResult out;
    out.stress.resize(model.elements.size());
    out.vonMises.resize(model.elements.size());
    for (size_t ei = 0; ei < model.elements.size(); ++ei) {
        const Element& e = model.elements[ei];
        const ElasticMaterial& m = model.materials[static_cast<size_t>(e.material)];
        const double alpha = input.expansion[static_cast<size_t>(e.material)];
        const double dT = input.elementTemperature[ei] - input.referenceTemperature;
        if (model.kind == ElementKind::Solid) {
            const auto x = corners(model, e);
            Eigen::Matrix<double, 24, 1> ue;
            for (size_t q = 0; q < 8; ++q)
                for (int d = 0; d < 3; ++d)
                    ue(static_cast<Eigen::Index>(3 * q + static_cast<size_t>(d))) = u(static_cast<Eigen::Index>(model.dof(static_cast<size_t>(e.nodes[q]), d)));
            double detJ = 0.0;
            const Eigen::Matrix<double, 6, 1> sigma = solidElasticity(m) * (solidStrainMatrix(x, 0, 0, 0, detJ) * ue - freeStrain(alpha, dT));
            for (int c = 0; c < 6; ++c)
                out.stress[ei][static_cast<size_t>(c)] = sigma(c);
            const double sx = sigma(0), sy = sigma(1), sz = sigma(2), txy = sigma(3), tyz = sigma(4), tzx = sigma(5);
            out.vonMises[ei] = std::sqrt(0.5 * ((sx - sy) * (sx - sy) + (sy - sz) * (sy - sz) + (sz - sx) * (sz - sx)) + 3 * (txy * txy + tyz * tyz + tzx * tzx));
        } else {
            const size_t a = static_cast<size_t>(e.nodes[0]), b = static_cast<size_t>(e.nodes[1]);
            const Vec3 d = model.nodes[b] - model.nodes[a];
            const double length = d.norm();
            const Vec3 axis = d / length;
            const double strain = (model.translation(u, b) - model.translation(u, a)).dot(axis) / length;
            const double sigma = m.youngsModulus * (strain - alpha * dT);
            out.stress[ei] = {sigma, 0, 0, 0, 0, 0};
            out.vonMises[ei] = std::abs(sigma);
        }
        out.maxVonMises = std::max(out.maxVonMises, out.vonMises[ei]);
    }
    return out;
}

}  // namespace resonant::mechanics
