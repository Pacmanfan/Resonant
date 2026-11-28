#include "mechanics/PiezoelectricSolid.h"

#include <Eigen/Cholesky>
#include <Eigen/LU>
#include <Eigen/SparseCholesky>
#include <Eigen/SparseLU>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>

namespace resonant::mechanics {
namespace {

using Complex = std::complex<double>;
constexpr double kPi = 3.14159265358979323846;
constexpr double kEps0 = 8.8541878128e-12;
constexpr double kGauss3[3] = {-0.7745966692414834, 0.0, 0.7745966692414834};
constexpr double kWeight3[3] = {0.5555555555555556, 0.8888888888888888, 0.5555555555555556};

std::string num(double v)
{
    char b[64];
    std::snprintf(b, sizeof b, "%.6g", v);
    return b;
}

// The Bond stress-transformation matrix of a rotation whose columns are the material axes.
Eigen::Matrix<double, 6, 6> bond(const Eigen::Matrix3d& q)
{
    static constexpr int pair[6][2] = {{0, 0}, {1, 1}, {2, 2}, {0, 1}, {1, 2}, {2, 0}};
    Eigen::Matrix<double, 6, 6> m;
    for (int p = 0; p < 6; ++p)
        for (int k = 0; k < 6; ++k) {
            const int i = pair[p][0], j = pair[p][1], a = pair[k][0], b = pair[k][1];
            m(p, k) = a == b ? q(i, a) * q(j, b) : q(i, a) * q(j, b) + q(i, b) * q(j, a);
        }
    return m;
}

struct WorldPiezo {
    Eigen::Matrix<double, 6, 6> c;
    Eigen::Matrix<double, 3, 6> e;
    Eigen::Matrix3d eps;
};

// The material frame of a piezoelectric layer at (u, v): axis 1 along u in the
// surface, axis 3 along the normal (or against it for reversed poling).
Eigen::Matrix3d poledFrame(const ReferenceSurface& s, double u, double v, int poling)
{
    Vec3 p, n, t;
    s.evaluate(u, v, p, n, t);
    Vec3 e1 = t - t.dot(n) * n;
    if (e1.norm() < 1e-12)
        e1 = n.unitOrthogonal();
    e1.normalize();
    const Vec3 e3 = poling >= 0 ? n : Vec3(-n);
    Eigen::Matrix3d q;
    q.col(0) = e1;
    q.col(2) = e3;
    q.col(1) = e3.cross(e1);
    return q;
}

WorldPiezo rotated(const PiezoelectricConstants& k, const Eigen::Matrix3d& q)
{
    WorldPiezo w;
    w.c = rotateStiffness(k.cE, q);
    w.e = q * k.e * bond(q).transpose();
    w.eps = q * k.epsS * q.transpose();
    w.eps = 0.5 * (w.eps + w.eps.transpose()).eval();
    return w;
}

// Everything the analyses share: the mesh with the piezoelectric elements'
// stiffness, the electric nodes, the coupling matrices over every electric node.
struct Coupled {
    LayeredMesh mesh;
    SystemMatrices system;                      // K_uu, M, loss over the free displacement dofs
    std::vector<int> electricOf;                // per node: its electric index, or -1
    std::vector<int> electricNode;              // per electric index: the node
    Eigen::SparseMatrix<double> kuphi;          // free u dofs x electric nodes
    Eigen::SparseMatrix<double> kphiphi;        // electric nodes x electric nodes
    std::map<std::string, std::vector<int>> terminalNodes;  // electric indices on each terminal's electrodes ("ground" included)
    std::vector<std::string> assumptions;
};

Result<Coupled> build(const PiezoelectricBody& pb, const LayeredSupports& supports)
{
    using R = Result<Coupled>;
    if (Status s = pb.validate(); !s)
        return R::failure(s.message());
    // The piezoelectric layers' nominal isotropic values come from their own constants
    // (1 / s11^E, -s12 / s11), for the mesh's checks only: the element matrices use c^E.
    LayeredBody body = pb.body;
    for (const auto& p : pb.piezo) {
        auto& base = body.layers[static_cast<size_t>(p.layer)].material.base;
        const Eigen::Matrix<double, 6, 6> s = p.constants.cE.inverse();
        base.youngsModulus = 1.0 / s(0, 0);
        base.poissonsRatio = std::clamp(-s(0, 1) / s(0, 0), 0.0, 0.49);
        base.density = p.constants.density;
        if (base.name.empty())
            base.name = p.constants.name;
    }
    auto meshed = meshLayeredBody(body, supports);
    if (!meshed)
        return R::failure(meshed.message());
    Coupled c;
    c.mesh = std::move(meshed.value());
    StructuralModel& m = c.mesh.structure;
    std::map<int, const PiezoLayer3D*> piezoOf;
    for (const auto& p : pb.piezo)
        piezoOf[p.layer] = &p;
    // Element constants in world axes; the mechanical stiffness at constant field.
    std::vector<std::optional<WorldPiezo>> world(m.elements.size());
    for (size_t e = 0; e < m.elements.size(); ++e) {
        const auto it = piezoOf.find(c.mesh.elementLayer[e]);
        if (it == piezoOf.end())
            continue;
        const auto& par = c.mesh.elementParameter[e];
        world[e] = rotated(it->second->constants, poledFrame(body.surface, par[0], par[1], it->second->poling));
        Constitutive con;
        con.stiffness = world[e]->c;
        con.density = it->second->constants.density;
        con.description = "piezoelectric c^E, layer '" + body.layers[static_cast<size_t>(c.mesh.elementLayer[e])].name + "'";
        m.elements[e].constitutive = static_cast<int>(m.constitutive.size());
        m.constitutive.push_back(con);
    }
    if (Status s = m.validate(); !s)
        return R::failure("the piezoelectric mesh is not valid: " + s.message());
    c.system = assemble(m);
    // Electric nodes: the nodes of the piezoelectric elements.
    c.electricOf.assign(m.nodes.size(), -1);
    for (size_t e = 0; e < m.elements.size(); ++e)
        if (world[e])
            for (const int node : m.elements[e].nodes)
                if (c.electricOf[static_cast<size_t>(node)] < 0) {
                    c.electricOf[static_cast<size_t>(node)] = static_cast<int>(c.electricNode.size());
                    c.electricNode.push_back(node);
                }
    const auto ne = static_cast<Eigen::Index>(c.electricNode.size());
    const auto nu = static_cast<Eigen::Index>(c.system.map.freeCount());
    std::vector<Eigen::Triplet<double>> tup, tpp;
    for (size_t e = 0; e < m.elements.size(); ++e) {
        if (!world[e])
            continue;
        const std::array<Vec3, 20> x = solid20Positions(m, m.elements[e]);
        Eigen::Matrix<double, 60, 20> kup = Eigen::Matrix<double, 60, 20>::Zero();
        Eigen::Matrix<double, 20, 20> kpp = Eigen::Matrix<double, 20, 20>::Zero();
        for (int a = 0; a < 3; ++a)
            for (int b = 0; b < 3; ++b)
                for (int g = 0; g < 3; ++g) {
                    double detJ = 0.0;
                    const Eigen::Matrix<double, 20, 3> grad = solid20Gradients(x, kGauss3[a], kGauss3[b], kGauss3[g], detJ);
                    const double w = kWeight3[a] * kWeight3[b] * kWeight3[g] * detJ;
                    const Eigen::Matrix<double, 6, 60> bu = solid20StrainMatrix(grad);
                    kup.noalias() += w * bu.transpose() * world[e]->e.transpose() * grad.transpose();
                    kpp.noalias() += w * grad * world[e]->eps * grad.transpose();
                }
        for (size_t q = 0; q < 20; ++q) {
            const int col = c.electricOf[static_cast<size_t>(m.elements[e].nodes[q])];
            for (size_t r = 0; r < 20; ++r)
                tpp.emplace_back(c.electricOf[static_cast<size_t>(m.elements[e].nodes[r])], col, kpp(static_cast<Eigen::Index>(r), static_cast<Eigen::Index>(q)));
            for (size_t r = 0; r < 20; ++r)
                for (int d = 0; d < 3; ++d) {
                    const int row = c.system.map.free[m.dof(static_cast<size_t>(m.elements[e].nodes[r]), d)];
                    if (row >= 0)
                        tup.emplace_back(row, col, kup(static_cast<Eigen::Index>(3 * r + static_cast<size_t>(d)), static_cast<Eigen::Index>(q)));
                }
        }
    }
    c.kuphi.resize(nu, ne);
    c.kuphi.setFromTriplets(tup.begin(), tup.end());
    c.kphiphi.resize(ne, ne);
    c.kphiphi.setFromTriplets(tpp.begin(), tpp.end());
    // Electrodes: the nodes of a piezoelectric layer's face (or a segment of it).
    std::vector<std::string> assigned(c.electricNode.size());
    for (const auto& el : pb.electrodes) {
        const auto& face = c.mesh.layerFaceNodes[static_cast<size_t>(el.layer)][el.outerFace ? 1 : 0];
        int count = 0;
        for (const int node : face) {
            const auto& uv = c.mesh.nodeParameter[static_cast<size_t>(node)];
            if (el.region && !el.region->contains(uv[0], uv[1]))
                continue;
            const int ei = c.electricOf[static_cast<size_t>(node)];
            if (ei < 0)
                return R::failure("the electrode '" + el.name + "' lies on a face without a piezoelectric layer");
            if (!assigned[static_cast<size_t>(ei)].empty() && assigned[static_cast<size_t>(ei)] != el.terminal)
                return R::failure("the electrode '" + el.name + "' touches another electrode wired to '" + assigned[static_cast<size_t>(ei)] +
                                  "': two terminals would be shorted");
            if (assigned[static_cast<size_t>(ei)].empty())
                c.terminalNodes[el.terminal].push_back(ei);
            assigned[static_cast<size_t>(ei)] = el.terminal;
            ++count;
        }
        if (count == 0)
            return R::failure("the electrode '" + el.name + "' covers no node of the mesh");
    }
    c.assumptions = c.mesh.assumptions;
    c.assumptions.push_back("linear piezoelectricity (stress-charge form), constants rotated with the surface; poling along the layer normal");
    c.assumptions.push_back("electrodes are equipotential surfaces without thickness or stiffness; the outer dielectric (air) is not modelled");
    c.assumptions.push_back("no dielectric or piezoelectric loss; no depolarization or ferroelectric switching");
    for (const auto& p : pb.piezo)
        c.assumptions.push_back("layer '" + body.layers[static_cast<size_t>(p.layer)].name + "': " + p.constants.name + " (" + p.constants.provenance + "), poled " +
                                (p.poling >= 0 ? "outwards" : "inwards"));
    return c;
}

// The electric unknowns of one analysis: each electric node is fixed (a known
// potential) or one of the unknowns (inside the dielectric, or a terminal).
struct Unknowns {
    Eigen::SparseMatrix<double> p;     // electric nodes x unknowns (selection and merging)
    Eigen::VectorXd fixed;             // per electric node: the known potential (0 where unknown)
    std::map<std::string, int> terminalUnknown;  // floating and load terminals
    int count = 0;
};

enum class ElectricMode { Static, Harmonic, ShortCircuit, OpenCircuit };

Result<Unknowns> unknownsFor(const Coupled& c, const PiezoelectricBody& pb, ElectricMode mode)
{
    using R = Result<Unknowns>;
    Unknowns u;
    const auto ne = static_cast<int>(c.electricNode.size());
    std::vector<int> map(static_cast<size_t>(ne), -2);  // -1 fixed, >= 0 the unknown
    u.fixed = Eigen::VectorXd::Zero(ne);
    bool anyFixed = false;
    for (const auto& [name, nodes] : c.terminalNodes) {
        const Terminal* t = nullptr;
        for (const auto& k : pb.terminals)
            if (k.name == name)
                t = &k;
        double value = 0.0;
        bool fixedTerminal = name == "ground";
        if (t) {
            if (t->kind == Terminal::Kind::Driven) {
                fixedTerminal = true;
                value = (mode == ElectricMode::Static || mode == ElectricMode::Harmonic) ? t->voltage : 0.0;
            } else if (t->kind == Terminal::Kind::Load) {
                fixedTerminal = mode == ElectricMode::Static || mode == ElectricMode::ShortCircuit;  // a load discharges statically; shorted in the short-circuit modes
            }
        }
        if (fixedTerminal) {
            anyFixed = true;
            for (const int ei : nodes) {
                map[static_cast<size_t>(ei)] = -1;
                u.fixed(ei) = value;
            }
        } else {
            u.terminalUnknown[name] = u.count;
            for (const int ei : nodes)
                map[static_cast<size_t>(ei)] = u.count;
            ++u.count;
        }
    }
    if (!anyFixed)
        return R::failure("no electrode is grounded or driven: the potential has no reference (wire an electrode to 'ground')");
    for (int ei = 0; ei < ne; ++ei)
        if (map[static_cast<size_t>(ei)] == -2)
            map[static_cast<size_t>(ei)] = u.count++;  // inside the dielectric
    std::vector<Eigen::Triplet<double>> t;
    for (int ei = 0; ei < ne; ++ei)
        if (map[static_cast<size_t>(ei)] >= 0)
            t.emplace_back(ei, map[static_cast<size_t>(ei)], 1.0);
    u.p.resize(ne, u.count);
    u.p.setFromTriplets(t.begin(), t.end());
    return u;
}

template <typename Scalar>
Eigen::SparseMatrix<Scalar> stack(const Eigen::SparseMatrix<Scalar>& a, const Eigen::SparseMatrix<Scalar>& b, const Eigen::SparseMatrix<Scalar>& c,
                                  const Eigen::SparseMatrix<Scalar>& d)
{
    std::vector<Eigen::Triplet<Scalar>> t;
    const auto add = [&](const Eigen::SparseMatrix<Scalar>& m, Eigen::Index r0, Eigen::Index c0) {
        for (int k = 0; k < m.outerSize(); ++k)
            for (typename Eigen::SparseMatrix<Scalar>::InnerIterator it(m, k); it; ++it)
                t.emplace_back(static_cast<int>(it.row() + r0), static_cast<int>(it.col() + c0), it.value());
    };
    add(a, 0, 0);
    add(b, 0, a.cols());
    add(c, a.rows(), 0);
    add(d, a.rows(), a.cols());
    Eigen::SparseMatrix<Scalar> out(a.rows() + c.rows(), a.cols() + b.cols());
    out.setFromTriplets(t.begin(), t.end());
    out.makeCompressed();
    return out;
}

// Solves the coupled system with the electric unknowns scaled to the mechanical
// ones (their diagonals differ by many orders of magnitude) and refined. Returns
// the normwise backward error ||A x - b|| / (||A||_F ||x|| + ||b||) of the unscaled
// system: scale-invariant, and small for a stable solve even at a resonance, where
// ||A|| ||x|| exceeds ||b|| by the amplification.
template <typename Scalar>
Result<Eigen::Matrix<Scalar, Eigen::Dynamic, 1>> solveCoupled(const Eigen::SparseMatrix<Scalar>& a, const Eigen::Matrix<Scalar, Eigen::Dynamic, 1>& b, Eigen::Index nu,
                                                             double& relativeResidual)
{
    using Vector = Eigen::Matrix<Scalar, Eigen::Dynamic, 1>;
    using R = Result<Vector>;
    const Eigen::Index n = a.rows();
    double mech = 0.0, elec = 0.0;
    Eigen::Index nm = 0, nel = 0;
    for (Eigen::Index i = 0; i < n; ++i) {
        const double d = std::abs(a.coeff(i, i));
        if (i < nu)
            mech += d, ++nm;
        else
            elec += d, ++nel;
    }
    const double scale = (nm > 0 && nel > 0 && elec > 0.0) ? std::sqrt((mech / nm) / (elec / nel)) : 1.0;
    Eigen::Matrix<double, Eigen::Dynamic, 1> sd = Eigen::Matrix<double, Eigen::Dynamic, 1>::Ones(n);
    sd.tail(n - nu).setConstant(scale);
    const Vector sdc = sd.template cast<Scalar>();
    const Eigen::SparseMatrix<Scalar> sm = sdc.asDiagonal() * a * sdc.asDiagonal();
    Eigen::SparseLU<Eigen::SparseMatrix<Scalar>> lu;
    lu.compute(sm);
    if (lu.info() != Eigen::Success)
        return R::failure("the coupled system could not be factorized: the body is not held, or an electrode region floats without reference");
    const Vector bs = sdc.cwiseProduct(b);
    Vector y = lu.solve(bs);
    for (int k = 0; k < 3 && y.allFinite(); ++k)
        y += lu.solve(bs - sm * y);
    const Vector x = sdc.cwiseProduct(y);
    double norm = 0.0;
    for (int k = 0; k < a.outerSize(); ++k)
        for (typename Eigen::SparseMatrix<Scalar>::InnerIterator it(a, k); it; ++it)
            norm += std::norm(it.value());
    relativeResidual = x.allFinite() ? (a * x - b).norm() / std::max(1e-300, std::sqrt(norm) * x.norm() + b.norm()) : 1e300;
    return x;
}

std::string fidelity()
{
    return "spatially resolved, three-dimensional linear piezoelectricity (20-node hexahedra with a nodal potential), fully coupled";
}

}  // namespace

// --- constants ---------------------------------------------------------------------------

Status PiezoelectricConstants::validate() const
{
    if (!(density > 0.0))
        return Status::failure("the piezoelectric material '" + name + "' needs a positive density");
    if ((cE - cE.transpose()).cwiseAbs().maxCoeff() > 1e-9 * cE.cwiseAbs().maxCoeff() || Eigen::LLT<Eigen::Matrix<double, 6, 6>>(cE).info() != Eigen::Success)
        return Status::failure("the stiffness c^E of '" + name + "' is not symmetric positive definite: refused, not repaired");
    if ((epsS - epsS.transpose()).cwiseAbs().maxCoeff() > 1e-9 * epsS.cwiseAbs().maxCoeff() || Eigen::LLT<Eigen::Matrix3d>(epsS).info() != Eigen::Success)
        return Status::failure("the permittivity eps^S of '" + name + "' is not symmetric positive definite: refused, not repaired");
    return Status::success();
}

Eigen::Matrix<double, 3, 6> PiezoelectricConstants::d() const
{
    return e * cE.inverse();
}

Eigen::Matrix3d PiezoelectricConstants::epsT() const
{
    return epsS + d() * e.transpose();
}

Result<PiezoelectricConstants> PiezoelectricConstants::fromStrainCharge(const Eigen::Matrix<double, 6, 6>& sE, const Eigen::Matrix<double, 3, 6>& d,
                                                                        const Eigen::Matrix3d& epsT, double density, std::string name, std::string provenance)
{
    PiezoelectricConstants k;
    k.name = std::move(name);
    k.provenance = std::move(provenance);
    k.density = density;
    if (Eigen::LLT<Eigen::Matrix<double, 6, 6>>(sE).info() != Eigen::Success)
        return Result<PiezoelectricConstants>::failure("the compliance s^E of '" + k.name + "' is not positive definite");
    k.cE = sE.inverse();
    k.cE = 0.5 * (k.cE + k.cE.transpose()).eval();
    k.e = d * k.cE;
    k.epsS = epsT - d * k.cE * d.transpose();
    k.epsS = 0.5 * (k.epsS + k.epsS.transpose()).eval();
    if (Status s = k.validate(); !s)
        return Result<PiezoelectricConstants>::failure(s.message() + " (from the strain-charge data: the coupling exceeds the material's stability)");
    return k;
}

PiezoelectricConstants PiezoelectricConstants::pzt5a()
{
    PiezoelectricConstants k;
    k.name = "PZT-5A";
    k.provenance = "commonly tabulated stress-charge constants (Berlincourt et al.); manufacturers' batches differ by 10-20 %";
    const double c11 = 120.3e9, c12 = 75.2e9, c13 = 75.1e9, c33 = 110.9e9, c44 = 21.1e9, c66 = 22.6e9;
    k.cE << c11, c12, c13, 0, 0, 0,  //
        c12, c11, c13, 0, 0, 0,      //
        c13, c13, c33, 0, 0, 0,      //
        0, 0, 0, c66, 0, 0,          // xy
        0, 0, 0, 0, c44, 0,          // yz
        0, 0, 0, 0, 0, c44;          // zx
    // e: rows E1 E2 E3; columns xx yy zz xy yz zx. e15 couples E1 to the zx strain, e24 = e15 E2 to yz.
    k.e.setZero();
    k.e(2, 0) = k.e(2, 1) = -5.4;
    k.e(2, 2) = 15.8;
    k.e(0, 5) = 12.3;
    k.e(1, 4) = 12.3;
    k.epsS = Eigen::Vector3d(916 * kEps0, 916 * kEps0, 830 * kEps0).asDiagonal();
    k.density = 7750.0;
    return k;
}

Status PiezoelectricBody::validate() const
{
    if (Status s = body.validate(); !s)
        return s;
    std::set<int> layers;
    for (const auto& p : piezo) {
        if (p.layer < 0 || static_cast<size_t>(p.layer) >= body.layers.size())
            return Status::failure("a piezoelectric layer index is outside the body");
        if (!layers.insert(p.layer).second)
            return Status::failure("a layer is declared piezoelectric twice");
        if (Status s = p.constants.validate(); !s)
            return s;
    }
    if (piezo.empty())
        return Status::failure("the body has no piezoelectric layer");
    std::set<std::string> names{"ground"};
    for (const auto& t : terminals) {
        if (t.name.empty() || !names.insert(t.name).second)
            return Status::failure("terminal names must be unique and not 'ground'");
        if (t.kind == Terminal::Kind::Load && !(t.resistance > 0.0))
            return Status::failure("the load terminal '" + t.name + "' needs a positive resistance");
    }
    for (const auto& e : electrodes) {
        if (e.layer < 0 || static_cast<size_t>(e.layer) >= body.layers.size() || !layers.count(e.layer))
            return Status::failure("the electrode '" + e.name + "' is not on a piezoelectric layer");
        if (!names.count(e.terminal))
            return Status::failure("the electrode '" + e.name + "' is wired to the unknown terminal '" + e.terminal + "'");
    }
    return Status::success();
}

// --- static ----------------------------------------------------------------------------------

Result<PiezoStaticResult> solvePiezoStatic(const PiezoelectricBody& pb, const LayeredSupports& supports, const LayeredLoads& loads)
{
    using R = Result<PiezoStaticResult>;
    auto built = build(pb, supports);
    if (!built)
        return R::failure(built.message());
    const Coupled& c = built.value();
    auto un = unknownsFor(c, pb, ElectricMode::Static);
    if (!un)
        return R::failure(un.message());
    const Unknowns& z = un.value();
    auto f = layeredLoadVector(pb.body, c.mesh, loads);
    if (!f)
        return R::failure(f.message());
    const Eigen::SparseMatrix<double> cuz = c.kuphi * z.p;
    const Eigen::SparseMatrix<double> dzz = Eigen::SparseMatrix<double>(z.p.transpose()) * c.kphiphi * z.p;
    const Eigen::SparseMatrix<double> a = stack<double>(c.system.stiffness, cuz, Eigen::SparseMatrix<double>(cuz.transpose()), Eigen::SparseMatrix<double>(-dzz));
    const auto nu = c.system.map.freeCount();
    Eigen::VectorXd rhs(static_cast<Eigen::Index>(nu) + z.count);
    rhs.head(static_cast<Eigen::Index>(nu)) = c.system.map.reduce(f.value()) - c.kuphi * z.fixed;
    rhs.tail(z.count) = Eigen::SparseMatrix<double>(z.p.transpose()) * (c.kphiphi * z.fixed);
    double residual = 0.0;
    auto solved = solveCoupled<double>(a, rhs, static_cast<Eigen::Index>(nu), residual);
    if (!solved)
        return R::failure(solved.message());
    if (residual > 1e-10)
        return R::failure("the coupled static solution is not accurate (backward error " + num(residual) + "): the body is not held, or the system is singular");
    const Eigen::VectorXd sol = solved.value();
    PiezoStaticResult out;
    out.mesh = c.mesh;
    out.displacement = c.system.map.expand(Eigen::VectorXd(sol.head(static_cast<Eigen::Index>(nu))));
    const Eigen::VectorXd phi = z.p * sol.tail(z.count) + z.fixed;
    out.potential.assign(c.mesh.structure.nodes.size(), 0.0);
    for (size_t ei = 0; ei < c.electricNode.size(); ++ei)
        out.potential[static_cast<size_t>(c.electricNode[ei])] = phi(static_cast<Eigen::Index>(ei));
    const Eigen::VectorXd q = Eigen::SparseMatrix<double>(c.kuphi.transpose()) * sol.head(static_cast<Eigen::Index>(nu)) - c.kphiphi * phi;  // nodal charge fluxes
    for (const auto& [name, nodes] : c.terminalNodes) {
        double sumQ = 0.0;
        for (const int ei : nodes)
            sumQ += q(ei);
        PiezoTerminalResult r;
        r.voltage = phi(nodes.front());
        r.charge = -sumQ;  // the free charge on the electrode
        out.terminals[name] = r;
    }
    for (size_t n = 0; n < c.mesh.structure.nodes.size(); ++n)
        out.maxDisplacement = std::max(out.maxDisplacement, c.mesh.structure.translation(out.displacement, n).norm());
    // The energy the sources put in, 1/2 (f^T u + sum V Q), stored at equilibrium (linear).
    out.storedEnergy = 0.5 * f.value().dot(out.displacement);
    for (const auto& [name, r] : out.terminals)
        out.storedEnergy += 0.5 * std::real(r.voltage) * std::real(r.charge);
    out.fidelity = fidelity() + "; static";
    out.assumptions = c.assumptions;
    out.assumptions.push_back("static: a resistive load has discharged (grounded); a floating terminal carries no net charge");
    return out;
}

// --- modal -----------------------------------------------------------------------------------

Result<PiezoModalResult> solvePiezoModal(const PiezoelectricBody& pb, const LayeredSupports& supports, int count, bool openCircuit)
{
    using R = Result<PiezoModalResult>;
    auto built = build(pb, supports);
    if (!built)
        return R::failure(built.message());
    const Coupled& c = built.value();
    auto un = unknownsFor(c, pb, openCircuit ? ElectricMode::OpenCircuit : ElectricMode::ShortCircuit);
    if (!un)
        return R::failure(un.message());
    const Unknowns& z = un.value();
    const Eigen::SparseMatrix<double> cuz = c.kuphi * z.p;
    const Eigen::SparseMatrix<double> dzz = Eigen::SparseMatrix<double>(z.p.transpose()) * c.kphiphi * z.p;
    // Condense the electric unknowns: K_eff = K_uu + C D^-1 C^T on the coupled displacement dofs.
    std::vector<int> coupledDofs;
    {
        std::set<int> rows;
        for (int k = 0; k < cuz.outerSize(); ++k)
            for (Eigen::SparseMatrix<double>::InnerIterator it(cuz, k); it; ++it)
                rows.insert(static_cast<int>(it.row()));
        coupledDofs.assign(rows.begin(), rows.end());
    }
    if (coupledDofs.size() > 6000)
        return R::failure("the condensation would couple " + std::to_string(coupledDofs.size()) +
                          " displacement dofs densely: more than the 6000 this modal analysis accepts (coarsen the piezoelectric layers)");
    SystemMatrices sys = c.system;
    if (z.count > 0 && !coupledDofs.empty()) {
        Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> ldlt(dzz);
        if (ldlt.info() != Eigen::Success)
            return R::failure("the dielectric matrix could not be factorized");
        Eigen::MatrixXd ct = Eigen::MatrixXd::Zero(z.count, static_cast<Eigen::Index>(coupledDofs.size()));
        std::map<int, Eigen::Index> position;
        for (size_t i = 0; i < coupledDofs.size(); ++i)
            position[coupledDofs[i]] = static_cast<Eigen::Index>(i);
        for (int k = 0; k < cuz.outerSize(); ++k)
            for (Eigen::SparseMatrix<double>::InnerIterator it(cuz, k); it; ++it)
                ct(it.col(), position[static_cast<int>(it.row())]) = it.value();
        const Eigen::MatrixXd x = ldlt.solve(ct);
        const Eigen::MatrixXd g = ct.transpose() * x;
        std::vector<Eigen::Triplet<double>> t;
        for (int k = 0; k < sys.stiffness.outerSize(); ++k)
            for (Eigen::SparseMatrix<double>::InnerIterator it(sys.stiffness, k); it; ++it)
                t.emplace_back(static_cast<int>(it.row()), static_cast<int>(it.col()), it.value());
        for (size_t i = 0; i < coupledDofs.size(); ++i)
            for (size_t j = 0; j < coupledDofs.size(); ++j)
                t.emplace_back(coupledDofs[i], coupledDofs[j], g(static_cast<Eigen::Index>(i), static_cast<Eigen::Index>(j)));
        sys.stiffness.setFromTriplets(t.begin(), t.end());
        sys.stiffness = 0.5 * (sys.stiffness + Eigen::SparseMatrix<double>(sys.stiffness.transpose()));
    }
    auto modes = solveModal(c.mesh.structure, sys, count);
    if (!modes)
        return R::failure(modes.message());
    PiezoModalResult out;
    out.mesh = c.mesh;
    for (const auto& mode : modes.value().modes)
        out.frequencies.push_back(mode.frequencyHz);
    out.condition = openCircuit ? "open circuit (load terminals floating)" : "short circuit (load terminals grounded)";
    out.fidelity = fidelity() + "; free vibration, " + out.condition;
    out.assumptions = c.assumptions;
    out.assumptions.push_back("the interior and floating potentials condensed exactly (static condensation of the massless electric unknowns)");
    return out;
}

// --- harmonic --------------------------------------------------------------------------------

Result<PiezoHarmonicResult> solvePiezoHarmonic(const PiezoelectricBody& pb, const LayeredSupports& supports, const PiezoHarmonicSettings& settings)
{
    using R = Result<PiezoHarmonicResult>;
    if (!(settings.frequency > 0.0))
        return R::failure("a harmonic analysis needs a positive frequency");
    if (settings.lossFactor < 0.0)
        return R::failure("the loss factor must not be negative");
    auto built = build(pb, supports);
    if (!built)
        return R::failure(built.message());
    const Coupled& c = built.value();
    auto un = unknownsFor(c, pb, ElectricMode::Harmonic);
    if (!un)
        return R::failure(un.message());
    const Unknowns& z = un.value();
    const StructuralModel& m = c.mesh.structure;
    const double w = 2 * kPi * settings.frequency;
    const Complex j(0.0, 1.0);
    const auto nu = static_cast<Eigen::Index>(c.system.map.freeCount());
    // The loss stiffness: the materials' stated loss factors and the settings' one.
    const Eigen::SparseMatrix<double> loss = c.system.lossStiffness + settings.lossFactor * c.system.stiffness;
    const Eigen::SparseMatrix<Complex> kuu =
        (c.system.stiffness.cast<Complex>() + j * loss.cast<Complex>() - Complex(w * w) * c.system.mass.cast<Complex>());
    const Eigen::SparseMatrix<double> cuz = c.kuphi * z.p;
    Eigen::SparseMatrix<double> dzz = Eigen::SparseMatrix<double>(z.p.transpose()) * c.kphiphi * z.p;
    Eigen::SparseMatrix<Complex> dz = (-dzz).cast<Complex>();
    // A load terminal's row: sum of charge fluxes = V / (j omega R).
    for (const auto& t : pb.terminals)
        if (t.kind == Terminal::Kind::Load && z.terminalUnknown.count(t.name)) {
            const int k = z.terminalUnknown.at(t.name);
            dz.coeffRef(k, k) += -1.0 / (j * w * t.resistance);
        }
    const Eigen::SparseMatrix<Complex> a = stack<Complex>(kuu, cuz.cast<Complex>(), Eigen::SparseMatrix<double>(cuz.transpose()).cast<Complex>(), dz);
    // Loads: the base acceleration (inertial, in the base frame) and the applied loads.
    auto applied = layeredLoadVector(pb.body, c.mesh, settings.loads);
    if (!applied)
        return R::failure(applied.message());
    Eigen::VectorXd full = applied.value();
    if (settings.baseAcceleration.norm() > 0.0)
        for (const Element& e : m.elements) {
            const ElementMatrices em = elementMatrices(m, e);
            Eigen::VectorXd r(60);
            for (int q = 0; q < 20; ++q)
                for (int d = 0; d < 3; ++d)
                    r(3 * q + d) = settings.baseAcceleration(d);
            const Eigen::VectorXd fe = -em.mass * r;
            for (int q = 0; q < 20; ++q)
                for (int d = 0; d < 3; ++d)
                    full(static_cast<Eigen::Index>(m.dof(static_cast<size_t>(e.nodes[static_cast<size_t>(q)]), d))) += fe(3 * q + d);
        }
    Eigen::VectorXcd rhs(nu + z.count);
    rhs.head(nu) = (c.system.map.reduce(full) - c.kuphi * z.fixed).cast<Complex>();
    rhs.tail(z.count) = (Eigen::SparseMatrix<double>(z.p.transpose()) * (c.kphiphi * z.fixed)).cast<Complex>();
    double residual = 0.0;
    auto solved = solveCoupled<Complex>(a, rhs, nu, residual);
    if (!solved)
        return R::failure(solved.message() + " (a resonance without damping?)");
    if (residual > 1e-10)
        return R::failure("the coupled harmonic solution is not accurate (backward error " + num(residual) + ")");
    const Eigen::VectorXcd sol = solved.value();
    PiezoHarmonicResult out;
    out.mesh = c.mesh;
    const Eigen::VectorXcd ur = sol.head(nu);
    out.displacement = c.system.map.expand(ur);
    const Eigen::VectorXcd phi = z.p.cast<Complex>() * sol.tail(z.count) + z.fixed.cast<Complex>();
    const Eigen::VectorXcd q = Eigen::SparseMatrix<double>(c.kuphi.transpose()).cast<Complex>() * ur - c.kphiphi.cast<Complex>() * phi;
    // Powers, each from its own terms: the work of the loads on the velocities, the
    // loss stiffness on the displacements, the loads' resistances on their voltages.
    const Eigen::VectorXcd v = j * w * ur;
    const Eigen::VectorXd fr = c.system.map.reduce(full);
    out.inputPower = 0.5 * std::real(fr.cast<Complex>().dot(v));  // dot conjugates the first argument: f is real
    out.dampingPower = 0.5 * w * std::real(ur.dot(loss.cast<Complex>() * ur));
    for (const auto& [name, nodes] : c.terminalNodes) {
        Complex sumQ = 0.0;
        for (const int ei : nodes)
            sumQ += q(ei);
        PiezoTerminalResult r;
        r.voltage = phi(nodes.front());
        r.charge = -sumQ;
        r.current = j * w * sumQ;  // into the external circuit (a load: V / R)
        for (const auto& t : pb.terminals)
            if (t.name == name) {
                if (t.kind == Terminal::Kind::Load)
                    out.loadPower += 0.5 * std::norm(r.voltage) / t.resistance;
                if (t.kind == Terminal::Kind::Driven)
                    out.inputPower += 0.5 * std::real(r.voltage * std::conj(-r.current));  // the source supplies -current
            }
        out.terminals[name] = r;
    }
    out.fidelity = fidelity() + "; steady harmonic at " + num(settings.frequency) + " Hz";
    out.assumptions = c.assumptions;
    out.assumptions.push_back("steady harmonic response in the base frame (the supports move with the base); structural damping as a loss factor");
    return out;
}

}  // namespace resonant::mechanics
