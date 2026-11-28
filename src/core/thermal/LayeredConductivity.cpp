#include "thermal/LayeredConductivity.h"

#include "thermal/HeatConduction.h"

#include <algorithm>
#include <cmath>

namespace resonant::thermal {

Result<EffectiveConductivity> layeredConductivity(const std::vector<ConductiveLayer>& cell)
{
    using R = Result<EffectiveConductivity>;
    if (cell.empty())
        return R::failure("the cell needs at least one layer");
    EffectiveConductivity e;
    double sumKd = 0.0, sumDk = 0.0;
    for (const ConductiveLayer& l : cell) {
        if (!(l.thickness > 0.0) || !(l.conductivity > 0.0))
            return R::failure("every layer needs a positive thickness and conductivity");
        e.period += l.thickness;
        sumKd += l.thickness * l.conductivity;
        sumDk += l.thickness / l.conductivity;
    }
    e.parallel = sumKd / e.period;
    e.perpendicular = e.period / sumDk;
    e.anisotropy = e.parallel / e.perpendicular;
    return e;
}

Result<ExplicitConductivity> explicitLayeredConductivity(const std::vector<ConductiveLayer>& cell, int periods, int cellsPerLayer)
{
    using R = Result<ExplicitConductivity>;
    auto effective = layeredConductivity(cell);
    if (!effective)
        return R::failure(effective.message());
    if (periods < 1 || cellsPerLayer < 1)
        return R::failure("at least one period and one cell per layer");
    const double height = effective.value().period * periods;
    // z edges follow the layers; x and y are uniform and as long as the stack is high.
    ConductionProblem p;
    std::vector<double>& z = p.grid.edges[2];
    std::vector<double> kz;
    z.push_back(0.0);
    for (int n = 0; n < periods; ++n)
        for (const ConductiveLayer& l : cell)
            for (int c = 0; c < cellsPerLayer; ++c) {
                z.push_back(z.back() + l.thickness / cellsPerLayer);
                kz.push_back(l.conductivity);
            }
    const int nx = 8;
    for (int a = 0; a < 2; ++a) {
        auto& e = p.grid.edges[static_cast<size_t>(a)];
        for (int i = 0; i <= (a == 0 ? nx : 1); ++i)
            e.push_back(height * i / (a == 0 ? nx : 1));
    }
    p.conductivity.resize(p.grid.cellCount());
    for (size_t k = 0; k < p.grid.count(2); ++k)
        for (size_t j = 0; j < p.grid.count(1); ++j)
            for (size_t i = 0; i < p.grid.count(0); ++i)
                p.conductivity[p.grid.index(i, j, k)] = kz[k];
    const double dT = 10.0;
    ExplicitConductivity out;
    const auto solve = [&](Face hot, Face cold, double length, double area, double& k) -> Status {
        ConductionProblem q = p;
        q.faces[static_cast<size_t>(hot)] = {FaceKind::Temperature, 293.15 + dT, 0.0, 293.15};
        q.faces[static_cast<size_t>(cold)] = {FaceKind::Temperature, 293.15, 0.0, 293.15};
        auto s = solveSteady(q);
        if (!s)
            return Status::failure(s.message());
        const double flow = s.value().faceHeatOut[static_cast<size_t>(cold)];
        k = flow * length / (area * dT);
        out.balanceResidual = std::max(out.balanceResidual, std::abs(s.value().balanceResidual));
        return Status::success();
    };
    const double widthY = p.grid.edges[1].back();
    if (Status s = solve(Face::MinusZ, Face::PlusZ, height, height * widthY, out.perpendicular); !s)
        return R::failure(s.message());
    if (Status s = solve(Face::MinusX, Face::PlusX, height, height * widthY, out.parallel); !s)
        return R::failure(s.message());
    return out;
}

}  // namespace resonant::thermal
