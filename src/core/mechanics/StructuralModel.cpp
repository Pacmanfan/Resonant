#include "mechanics/StructuralModel.h"

#include <Eigen/Cholesky>
#include <Eigen/LU>

#include <cmath>
#include <map>
#include <tuple>

namespace resonant::mechanics {
namespace {

// Gauss-Legendre points and weights on [-1, 1].
struct Gauss {
    std::vector<double> x, w;
};
Gauss gauss(int n)
{
    switch (n) {
    case 2: return {{-0.5773502691896257, 0.5773502691896257}, {1.0, 1.0}};
    case 3: return {{-0.7745966692414834, 0.0, 0.7745966692414834}, {0.5555555555555556, 0.8888888888888888, 0.5555555555555556}};
    default:
        return {{-0.8611363115940526, -0.3399810435848563, 0.3399810435848563, 0.8611363115940526},
                {0.3478548451374538, 0.6521451548625461, 0.6521451548625461, 0.3478548451374538}};
    }
}

// ---- frame ------------------------------------------------------------------------

// Rows: local x, y, z axes in world coordinates.
Eigen::Matrix3d frameRotation(const Vec3& a, const Vec3& b, const Vec3& up)
{
    const Vec3 ex = (b - a).normalized();
    Vec3 ez = ex.cross(up);
    if (ez.norm() < 1e-9)  // `up` along the element: pick any perpendicular
        ez = ex.cross(std::abs(ex.x()) < 0.9 ? Vec3::UnitX() : Vec3::UnitY());
    ez.normalize();
    const Vec3 ey = ez.cross(ex);
    Eigen::Matrix3d r;
    r.row(0) = ex;
    r.row(1) = ey;
    r.row(2) = ez;
    return r;
}

void frameLocal(const ElasticMaterial& m, const Section& s, double L, Eigen::Matrix<double, 12, 12>& k, Eigen::Matrix<double, 12, 12>& mm)
{
    k.setZero();
    mm.setZero();
    const double E = m.youngsModulus, G = m.shearModulus(), rho = m.density;
    const auto put = [](Eigen::Matrix<double, 12, 12>& target, const std::array<int, 4>& dofs, const Eigen::Matrix4d& block) {
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                target(dofs[static_cast<size_t>(i)], dofs[static_cast<size_t>(j)]) += block(i, j);
    };
    // Axial and torsion.
    const double ka = E * s.area / L, kt = G * s.torsion / L;
    k(0, 0) = k(6, 6) = ka;
    k(0, 6) = k(6, 0) = -ka;
    k(3, 3) = k(9, 9) = kt;
    k(3, 9) = k(9, 3) = -kt;
    const double ma = rho * s.area * L / 6.0, mt = rho * s.polar * L / 6.0;
    mm(0, 0) = mm(6, 6) = 2.0 * ma;
    mm(0, 6) = mm(6, 0) = ma;
    mm(3, 3) = mm(9, 9) = 2.0 * mt;
    mm(3, 9) = mm(9, 3) = mt;
    // Bending in the x-y plane: v and rz (dofs 1, 5, 7, 11), second moment Iz.
    Eigen::Matrix4d b;
    b << 12, 6 * L, -12, 6 * L, 6 * L, 4 * L * L, -6 * L, 2 * L * L, -12, -6 * L, 12, -6 * L, 6 * L, 2 * L * L, -6 * L, 4 * L * L;
    put(k, {1, 5, 7, 11}, b * (E * s.iz / (L * L * L)));
    // Bending in the x-z plane: w and ry (dofs 2, 4, 8, 10), second moment Iy.
    // A positive ry turns +x towards -z, hence the sign changes.
    Eigen::Matrix4d c;
    c << 12, -6 * L, -12, -6 * L, -6 * L, 4 * L * L, 6 * L, 2 * L * L, -12, 6 * L, 12, 6 * L, -6 * L, 2 * L * L, 6 * L, 4 * L * L;
    put(k, {2, 4, 8, 10}, c * (E * s.iy / (L * L * L)));
    // Consistent mass of the bending dofs (translational inertia; rotary inertia neglected, as in Euler-Bernoulli theory).
    Eigen::Matrix4d mb;
    mb << 156, 22 * L, 54, -13 * L, 22 * L, 4 * L * L, 13 * L, -3 * L * L, 54, 13 * L, 156, -22 * L, -13 * L, -3 * L * L, -22 * L, 4 * L * L;
    Eigen::Matrix4d mc;
    mc << 156, -22 * L, 54, 13 * L, -22 * L, 4 * L * L, -13 * L, -3 * L * L, 54, -13 * L, 156, 22 * L, 13 * L, -3 * L * L, 22 * L, 4 * L * L;
    const double f = rho * s.area * L / 420.0;
    put(mm, {1, 5, 7, 11}, mb * f);
    put(mm, {2, 4, 8, 10}, mc * f);
}

// ---- plate (ACM) ----------------------------------------------------------------------

// Basis of the ACM element: 1, x, y, x^2, xy, y^2, x^3, x^2y, xy^2, y^3, x^3y, xy^3.
struct PlateBasis {
    Eigen::Matrix<double, 12, 1> p, px, py, pxx, pyy, pxy;
};
PlateBasis plateBasis(double x, double y)
{
    PlateBasis b;
    b.p << 1, x, y, x * x, x * y, y * y, x * x * x, x * x * y, x * y * y, y * y * y, x * x * x * y, x * y * y * y;
    b.px << 0, 1, 0, 2 * x, y, 0, 3 * x * x, 2 * x * y, y * y, 0, 3 * x * x * y, y * y * y;
    b.py << 0, 0, 1, 0, x, 2 * y, 0, x * x, 2 * x * y, 3 * y * y, x * x * x, 3 * x * y * y;
    b.pxx << 0, 0, 0, 2, 0, 0, 6 * x, 2 * y, 0, 0, 6 * x * y, 0;
    b.pyy << 0, 0, 0, 0, 0, 2, 0, 0, 2 * x, 6 * y, 0, 6 * x * y;
    b.pxy << 0, 0, 0, 0, 1, 0, 0, 2 * x, 2 * y, 0, 3 * x * x, 3 * y * y;
    return b;
}

// Coefficients of the shape functions for a rectangle of sides a, b centred
// on the origin. Node dofs: w, theta1 = dw/dy, theta2 = -dw/dx.
Eigen::Matrix<double, 12, 12> plateCoefficients(double a, double b)
{
    const double xs[4] = {-a / 2, a / 2, a / 2, -a / 2};
    const double ys[4] = {-b / 2, -b / 2, b / 2, b / 2};
    Eigen::Matrix<double, 12, 12> c;
    for (int n = 0; n < 4; ++n) {
        const PlateBasis basis = plateBasis(xs[n], ys[n]);
        c.row(3 * n) = basis.p.transpose();
        c.row(3 * n + 1) = basis.py.transpose();
        c.row(3 * n + 2) = -basis.px.transpose();
    }
    return c.inverse();
}

void plateMatrices(const ElasticMaterial& m, double h, double a, double b, Eigen::MatrixXd& k, Eigen::MatrixXd& mm)
{
    const Eigen::Matrix<double, 12, 12> c = plateCoefficients(a, b);
    const double D = m.plateBendingStiffness(h), nu = m.poissonsRatio;
    Eigen::Matrix3d d;
    d << 1, nu, 0, nu, 1, 0, 0, 0, (1 - nu) / 2;
    d *= D;
    k = Eigen::MatrixXd::Zero(12, 12);
    mm = Eigen::MatrixXd::Zero(12, 12);
    const Gauss g = gauss(4);
    for (size_t i = 0; i < g.x.size(); ++i)
        for (size_t j = 0; j < g.x.size(); ++j) {
            const double x = 0.5 * a * g.x[i], y = 0.5 * b * g.x[j];
            const double w = g.w[i] * g.w[j] * 0.25 * a * b;
            const PlateBasis basis = plateBasis(x, y);
            Eigen::Matrix<double, 3, 12> bm;
            bm.row(0) = basis.pxx.transpose() * c;
            bm.row(1) = basis.pyy.transpose() * c;
            bm.row(2) = 2.0 * basis.pxy.transpose() * c;
            k += w * bm.transpose() * d * bm;
            const Eigen::Matrix<double, 1, 12> n = basis.p.transpose() * c;
            mm += w * m.density * h * n.transpose() * n;
        }
}

// ---- membrane -------------------------------------------------------------------------

void membraneMatrices(const ElasticMaterial& m, double h, double tension, double a, double b, Eigen::MatrixXd& k, Eigen::MatrixXd& mm)
{
    k = Eigen::MatrixXd::Zero(4, 4);
    mm = Eigen::MatrixXd::Zero(4, 4);
    const double xs[4] = {-1, 1, 1, -1}, ys[4] = {-1, -1, 1, 1};
    const Gauss g = gauss(2);
    for (size_t i = 0; i < 2; ++i)
        for (size_t j = 0; j < 2; ++j) {
            const double xi = g.x[i], eta = g.x[j];
            const double w = g.w[i] * g.w[j] * 0.25 * a * b;
            Eigen::Vector4d n, nx, ny;
            for (int q = 0; q < 4; ++q) {
                n(q) = 0.25 * (1 + xs[q] * xi) * (1 + ys[q] * eta);
                nx(q) = 0.25 * xs[q] * (1 + ys[q] * eta) * 2.0 / a;
                ny(q) = 0.25 * ys[q] * (1 + xs[q] * xi) * 2.0 / b;
            }
            k += w * tension * (nx * nx.transpose() + ny * ny.transpose());
            mm += w * m.density * h * n * n.transpose();
        }
}

// ---- solid ----------------------------------------------------------------------------

constexpr double kHexXi[8][3] = {{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1}, {-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}};

Eigen::Matrix<double, 6, 6> elasticity(const ElasticMaterial& m)
{
    const double l = m.lameLambda(), g = m.shearModulus();
    Eigen::Matrix<double, 6, 6> c = Eigen::Matrix<double, 6, 6>::Zero();
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j)
            c(i, j) = l;
        c(i, i) = l + 2 * g;
        c(i + 3, i + 3) = g;
    }
    return c;
}

// Strain-displacement matrix (engineering shear strains) and the Jacobian
// determinant at a natural point.
Eigen::Matrix<double, 6, 24> hexB(const std::array<Vec3, 8>& x, double xi, double eta, double zeta, double& detJ)
{
    Eigen::Matrix<double, 8, 3> dn;
    for (int q = 0; q < 8; ++q) {
        const double a = kHexXi[q][0], b = kHexXi[q][1], c = kHexXi[q][2];
        dn(q, 0) = 0.125 * a * (1 + b * eta) * (1 + c * zeta);
        dn(q, 1) = 0.125 * b * (1 + a * xi) * (1 + c * zeta);
        dn(q, 2) = 0.125 * c * (1 + a * xi) * (1 + b * eta);
    }
    Eigen::Matrix3d j = Eigen::Matrix3d::Zero();
    for (int q = 0; q < 8; ++q)
        j += dn.row(q).transpose() * x[static_cast<size_t>(q)].transpose();
    detJ = j.determinant();
    const Eigen::Matrix<double, 8, 3> dx = dn * j.inverse().transpose();
    Eigen::Matrix<double, 6, 24> b = Eigen::Matrix<double, 6, 24>::Zero();
    for (int q = 0; q < 8; ++q) {
        const double gx = dx(q, 0), gy = dx(q, 1), gz = dx(q, 2);
        b(0, 3 * q) = gx;
        b(1, 3 * q + 1) = gy;
        b(2, 3 * q + 2) = gz;
        b(3, 3 * q) = gy;
        b(3, 3 * q + 1) = gx;
        b(4, 3 * q + 1) = gz;
        b(4, 3 * q + 2) = gy;
        b(5, 3 * q) = gz;
        b(5, 3 * q + 2) = gx;
    }
    return b;
}

void hexMatrices(const Eigen::Matrix<double, 6, 6>& c, double density, const std::array<Vec3, 8>& x, Eigen::MatrixXd& k, Eigen::MatrixXd& mm)
{
    k = Eigen::MatrixXd::Zero(24, 24);
    mm = Eigen::MatrixXd::Zero(24, 24);
    const Gauss g = gauss(2);
    for (size_t i = 0; i < 2; ++i)
        for (size_t j = 0; j < 2; ++j)
            for (size_t l = 0; l < 2; ++l) {
                double detJ = 0.0;
                const Eigen::Matrix<double, 6, 24> b = hexB(x, g.x[i], g.x[j], g.x[l], detJ);
                const double w = g.w[i] * g.w[j] * g.w[l] * detJ;
                k += w * b.transpose() * c * b;
                Eigen::Matrix<double, 8, 1> n;
                for (int q = 0; q < 8; ++q)
                    n(q) = 0.125 * (1 + kHexXi[q][0] * g.x[i]) * (1 + kHexXi[q][1] * g.x[j]) * (1 + kHexXi[q][2] * g.x[l]);
                // Mass: the same shape function for each direction (3x3 identity blocks).
                const Eigen::Matrix<double, 8, 8> nn = n * n.transpose() * (w * density);
                for (int p = 0; p < 8; ++p)
                    for (int q = 0; q < 8; ++q)
                        for (int d = 0; d < 3; ++d)
                            mm(3 * p + d, 3 * q + d) += nn(p, q);
            }
    // Make the matrices exactly symmetric.
    k = 0.5 * (k + k.transpose()).eval();
}

// ---- 20-node hexahedron -------------------------------------------------------------

constexpr std::array<std::array<int, 3>, 20> kHex20Xi{{{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},  //
                                                       {-1, -1, 1},  {1, -1, 1},  {1, 1, 1},  {-1, 1, 1},   //
                                                       {0, -1, -1},  {1, 0, -1},  {0, 1, -1}, {-1, 0, -1},  //
                                                       {0, -1, 1},   {1, 0, 1},   {0, 1, 1},  {-1, 0, 1},   //
                                                       {-1, -1, 0},  {1, -1, 0},  {1, 1, 0},  {-1, 1, 0}}};

Eigen::Matrix<double, 6, 60> hex20B(const Eigen::Matrix<double, 20, 3>& dx)
{
    Eigen::Matrix<double, 6, 60> b = Eigen::Matrix<double, 6, 60>::Zero();
    for (int q = 0; q < 20; ++q) {
        const double gx = dx(q, 0), gy = dx(q, 1), gz = dx(q, 2);
        b(0, 3 * q) = gx;
        b(1, 3 * q + 1) = gy;
        b(2, 3 * q + 2) = gz;
        b(3, 3 * q) = gy;
        b(3, 3 * q + 1) = gx;
        b(4, 3 * q + 1) = gz;
        b(4, 3 * q + 2) = gy;
        b(5, 3 * q) = gz;
        b(5, 3 * q + 2) = gx;
    }
    return b;
}

// Stiffness and consistent mass of the 20-node hexahedron: 3 x 3 x 3 Gauss points.
void hex20Matrices(const Eigen::Matrix<double, 6, 6>& c, double density, const std::array<Vec3, 20>& x, Eigen::MatrixXd& k, Eigen::MatrixXd& mm)
{
    Eigen::Matrix<double, 60, 60> kk = Eigen::Matrix<double, 60, 60>::Zero();
    Eigen::Matrix<double, 20, 20> nn = Eigen::Matrix<double, 20, 20>::Zero();
    const Gauss g = gauss(3);
    for (size_t i = 0; i < 3; ++i)
        for (size_t j = 0; j < 3; ++j)
            for (size_t l = 0; l < 3; ++l) {
                double detJ = 0.0;
                const Eigen::Matrix<double, 20, 3> dx = solid20Gradients(x, g.x[i], g.x[j], g.x[l], detJ);
                const double w = g.w[i] * g.w[j] * g.w[l] * detJ;
                const Eigen::Matrix<double, 6, 60> b = hex20B(dx);
                kk.noalias() += w * (b.transpose() * (c * b));
                Eigen::Matrix<double, 20, 1> n;
                Eigen::Matrix<double, 20, 3> dn;
                solid20Shape(g.x[i], g.x[j], g.x[l], n, dn);
                nn.noalias() += (w * density) * (n * n.transpose());
            }
    k = 0.5 * (kk + kk.transpose());
    mm = Eigen::MatrixXd::Zero(60, 60);
    for (int p = 0; p < 20; ++p)
        for (int q = 0; q < 20; ++q)
            for (int d = 0; d < 3; ++d)
                mm(3 * p + d, 3 * q + d) = nn(p, q);
}

// The 8-node face of an interface element: shape functions and natural derivatives.
void quad8Shape(double xi, double eta, Eigen::Matrix<double, 8, 1>& n, Eigen::Matrix<double, 8, 2>& dn)
{
    static constexpr int c[8][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}, {0, -1}, {1, 0}, {0, 1}, {-1, 0}};
    for (int q = 0; q < 8; ++q) {
        const double a = c[q][0], b = c[q][1];
        if (a != 0 && b != 0) {
            n(q) = 0.25 * (1 + a * xi) * (1 + b * eta) * (a * xi + b * eta - 1);
            dn(q, 0) = 0.25 * a * (1 + b * eta) * (2 * a * xi + b * eta);
            dn(q, 1) = 0.25 * b * (1 + a * xi) * (a * xi + 2 * b * eta);
        } else if (a == 0) {
            n(q) = 0.5 * (1 - xi * xi) * (1 + b * eta);
            dn(q, 0) = -xi * (1 + b * eta);
            dn(q, 1) = 0.5 * b * (1 - xi * xi);
        } else {
            n(q) = 0.5 * (1 + a * xi) * (1 - eta * eta);
            dn(q, 0) = 0.5 * a * (1 - eta * eta);
            dn(q, 1) = -eta * (1 + a * xi);
        }
    }
}

// The face frame at a natural point of an interface element's lower face: rows t1, t2, n.
Eigen::Matrix3d faceFrame(const std::array<Vec3, 8>& x, double xi, double eta, double& area, Vec3& point)
{
    Eigen::Matrix<double, 8, 1> n;
    Eigen::Matrix<double, 8, 2> dn;
    quad8Shape(xi, eta, n, dn);
    Vec3 a1 = Vec3::Zero(), a2 = Vec3::Zero();
    point = Vec3::Zero();
    for (size_t q = 0; q < 8; ++q) {
        a1 += dn(static_cast<Eigen::Index>(q), 0) * x[q];
        a2 += dn(static_cast<Eigen::Index>(q), 1) * x[q];
        point += n(static_cast<Eigen::Index>(q)) * x[q];
    }
    const Vec3 normal = a1.cross(a2);
    area = normal.norm();
    Eigen::Matrix3d r;
    const Vec3 nz = normal / area, t1 = a1.normalized(), t2 = nz.cross(t1);
    r.row(0) = t1;
    r.row(1) = t2;
    r.row(2) = nz;
    return r;
}

std::array<Vec3, 8> lowerFace(const StructuralModel& model, const InterfaceElement& e)
{
    std::array<Vec3, 8> x;
    for (size_t q = 0; q < 8; ++q)
        x[q] = model.nodes[static_cast<size_t>(e.lower[q])];
    return x;
}

int nodeCount(ElementKind kind)
{
    switch (kind) {
    case ElementKind::Frame: return 2;
    case ElementKind::Plate:
    case ElementKind::Membrane: return 4;
    case ElementKind::Solid: return 8;
    case ElementKind::Solid20: return 20;
    }
    return 2;
}

// In-plane size of a rectangular plate or membrane element: sides along axis1 and axis2.
std::pair<double, double> rectangleSides(const StructuralModel& model, const Element& e)
{
    const Vec3 d1 = model.nodes[static_cast<size_t>(e.nodes[1])] - model.nodes[static_cast<size_t>(e.nodes[0])];
    const Vec3 d2 = model.nodes[static_cast<size_t>(e.nodes[3])] - model.nodes[static_cast<size_t>(e.nodes[0])];
    return {std::abs(d1.dot(model.axis1)), std::abs(d2.dot(model.axis2))};
}

}  // namespace

std::string_view toString(ElementKind kind)
{
    switch (kind) {
    case ElementKind::Frame: return "frame (Euler-Bernoulli beam)";
    case ElementKind::Plate: return "plate (Kirchhoff, ACM rectangle)";
    case ElementKind::Membrane: return "membrane (bilinear)";
    case ElementKind::Solid: return "solid (trilinear hexahedron)";
    case ElementKind::Solid20: return "solid (quadratic 20-node hexahedron)";
    }
    return "";
}

int dofsPerNode(ElementKind kind)
{
    switch (kind) {
    case ElementKind::Frame: return 6;
    case ElementKind::Plate: return 3;
    case ElementKind::Membrane: return 1;
    case ElementKind::Solid:
    case ElementKind::Solid20: return 3;
    }
    return 1;
}

bool hasStress(ElementKind kind)
{
    return kind != ElementKind::Membrane;
}

size_t StructuralModel::fixedDofCount() const
{
    size_t count = 0;
    for (size_t n = 0; n < nodes.size(); ++n)
        for (int d = 0; d < dofsPerNode(); ++d)
            count += isFixed(n, d);
    return count;
}

double StructuralModel::totalMass() const
{
    double mass = 0.0;
    for (const Element& e : elements) {
        const ElasticMaterial& m = materials[static_cast<size_t>(e.material)];
        switch (kind) {
        case ElementKind::Frame:
            mass += m.density * sections[static_cast<size_t>(e.section)].area
                    * (nodes[static_cast<size_t>(e.nodes[1])] - nodes[static_cast<size_t>(e.nodes[0])]).norm();
            break;
        case ElementKind::Plate:
        case ElementKind::Membrane: {
            const auto [a, b] = rectangleSides(*this, e);
            mass += m.density * e.thickness * a * b;
            break;
        }
        case ElementKind::Solid:
        case ElementKind::Solid20: {
            const ElementMatrices em = elementMatrices(*this, e);
            mass += em.mass.sum() / 3.0;
            break;
        }
        }
    }
    return mass;
}

Vec3 StructuralModel::dofDirection(int local) const
{
    switch (kind) {
    case ElementKind::Frame:
    case ElementKind::Solid:
    case ElementKind::Solid20: return local < 3 ? Vec3(Vec3::Unit(local)) : Vec3(Vec3::Zero());
    case ElementKind::Plate:
    case ElementKind::Membrane: return local == 0 ? normal : Vec3(Vec3::Zero());
    }
    return Vec3::Zero();
}

Vec3 StructuralModel::translation(const Eigen::VectorXd& u, size_t node) const
{
    Vec3 t = Vec3::Zero();
    for (int d = 0; d < std::min(3, dofsPerNode()); ++d)
        t += u(static_cast<Eigen::Index>(dof(node, d))) * dofDirection(d);
    return t;
}

Eigen::Vector3cd StructuralModel::translation(const Eigen::VectorXcd& u, size_t node) const
{
    Eigen::Vector3cd t = Eigen::Vector3cd::Zero();
    for (int d = 0; d < std::min(3, dofsPerNode()); ++d)
        t += u(static_cast<Eigen::Index>(dof(node, d))) * dofDirection(d).cast<std::complex<double>>();
    return t;
}

Status StructuralModel::validate() const
{
    if (nodes.empty() || elements.empty())
        return Status::failure("the structural model has no elements");
    if (fixed.size() != nodes.size())
        return Status::failure("the support list does not match the nodes");
    if (materials.empty())
        return Status::failure("the structural model has no material");
    for (const ElasticMaterial& m : materials)
        if (Status status = m.validate(); !status)
            return status;
    const int count = nodeCount(kind);
    for (const Element& e : elements) {
        for (int q = 0; q < count; ++q)
            if (e.nodes[static_cast<size_t>(q)] < 0 || static_cast<size_t>(e.nodes[static_cast<size_t>(q)]) >= nodes.size())
                return Status::failure("an element refers to a node that does not exist");
        if (e.material < 0 || static_cast<size_t>(e.material) >= materials.size())
            return Status::failure("an element refers to a material that does not exist");
        if (kind == ElementKind::Frame) {
            if (e.section < 0 || static_cast<size_t>(e.section) >= sections.size())
                return Status::failure("a frame element refers to a section that does not exist");
            const Section& s = sections[static_cast<size_t>(e.section)];
            if (!(s.area > 0 && s.iy > 0 && s.iz > 0 && s.torsion > 0 && s.polar > 0))
                return Status::failure("a frame section needs a positive area, second moments and torsion constant");
            if ((nodes[static_cast<size_t>(e.nodes[1])] - nodes[static_cast<size_t>(e.nodes[0])]).norm() <= 0.0)
                return Status::failure("a frame element has zero length");
        }
        if ((kind == ElementKind::Plate || kind == ElementKind::Membrane) && !(e.thickness > 0.0))
            return Status::failure("a plate or membrane element needs a positive thickness");
        if (kind == ElementKind::Membrane && !(e.tension > 0.0))
            return Status::failure("a membrane needs a positive tension");
        if (e.constitutive >= 0) {
            if (kind != ElementKind::Solid && kind != ElementKind::Solid20)
                return Status::failure("only solid elements carry their own elasticity matrix");
            if (static_cast<size_t>(e.constitutive) >= constitutive.size())
                return Status::failure("an element refers to an elasticity matrix that does not exist");
        }
        if (kind == ElementKind::Solid20) {
            double detJ = 0.0;
            solid20Gradients(solid20Positions(*this, e), 0, 0, 0, detJ);
            if (!(detJ > 0.0))
                return Status::failure("a 20-node element is inverted or degenerate (non-positive Jacobian at its centre)");
        }
    }
    // An invalid elasticity matrix is refused, never repaired: it must be symmetric
    // and positive definite (a stable material), with a positive density.
    for (size_t i = 0; i < constitutive.size(); ++i) {
        const Constitutive& c = constitutive[i];
        const double scale = c.stiffness.cwiseAbs().maxCoeff();
        if (!(scale > 0.0) || !c.stiffness.allFinite())
            return Status::failure("elasticity matrix " + std::to_string(i) + " is zero or not finite");
        if ((c.stiffness - c.stiffness.transpose()).cwiseAbs().maxCoeff() > 1e-9 * scale)
            return Status::failure("elasticity matrix " + std::to_string(i) + " (" + c.description + ") is not symmetric");
        if (Eigen::LLT<Eigen::Matrix<double, 6, 6>>(c.stiffness).info() != Eigen::Success)
            return Status::failure("elasticity matrix " + std::to_string(i) + " (" + c.description +
                                   ") is not positive definite: the material would be unstable");
        if (!(c.density > 0.0))
            return Status::failure("elasticity matrix " + std::to_string(i) + " (" + c.description + ") needs a positive density");
    }
    if (!interfaces.empty() && kind != ElementKind::Solid20)
        return Status::failure("interface elements join 20-node solid elements only");
    for (const InterfaceElement& ie : interfaces) {
        for (size_t q = 0; q < 8; ++q)
            for (const int node : {ie.lower[q], ie.upper[q]})
                if (node < 0 || static_cast<size_t>(node) >= nodes.size())
                    return Status::failure("an interface element refers to a node that does not exist");
        if (!(ie.normalStiffness >= 0.0) || !(ie.shearStiffness >= 0.0))
            return Status::failure("an interface element needs non-negative normal and shear stiffnesses");
    }
    return Status::success();
}

DofMap makeDofMap(const StructuralModel& model)
{
    DofMap map;
    map.free.assign(model.dofCount(), -1);
    for (size_t n = 0; n < model.nodes.size(); ++n)
        for (int d = 0; d < model.dofsPerNode(); ++d)
            if (!model.isFixed(n, d)) {
                map.free[model.dof(n, d)] = static_cast<int>(map.model.size());
                map.model.push_back(model.dof(n, d));
            }
    return map;
}

Eigen::VectorXd DofMap::expand(const Eigen::VectorXd& reduced) const
{
    Eigen::VectorXd full = Eigen::VectorXd::Zero(static_cast<Eigen::Index>(free.size()));
    for (size_t i = 0; i < model.size(); ++i)
        full(static_cast<Eigen::Index>(model[i])) = reduced(static_cast<Eigen::Index>(i));
    return full;
}

Eigen::VectorXcd DofMap::expand(const Eigen::VectorXcd& reduced) const
{
    Eigen::VectorXcd full = Eigen::VectorXcd::Zero(static_cast<Eigen::Index>(free.size()));
    for (size_t i = 0; i < model.size(); ++i)
        full(static_cast<Eigen::Index>(model[i])) = reduced(static_cast<Eigen::Index>(i));
    return full;
}

Eigen::VectorXd DofMap::reduce(const Eigen::VectorXd& full) const
{
    Eigen::VectorXd reduced(static_cast<Eigen::Index>(model.size()));
    for (size_t i = 0; i < model.size(); ++i)
        reduced(static_cast<Eigen::Index>(i)) = full(static_cast<Eigen::Index>(model[i]));
    return reduced;
}

ElementMatrices elementMatrices(const StructuralModel& model, const Element& e)
{
    ElementMatrices out;
    const ElasticMaterial& m = model.materials[static_cast<size_t>(e.material)];
    switch (model.kind) {
    case ElementKind::Frame: {
        const Vec3& a = model.nodes[static_cast<size_t>(e.nodes[0])];
        const Vec3& b = model.nodes[static_cast<size_t>(e.nodes[1])];
        Eigen::Matrix<double, 12, 12> k, mm;
        frameLocal(m, model.sections[static_cast<size_t>(e.section)], (b - a).norm(), k, mm);
        const Eigen::Matrix3d r = frameRotation(a, b, e.up);
        Eigen::Matrix<double, 12, 12> t = Eigen::Matrix<double, 12, 12>::Zero();
        for (int i = 0; i < 4; ++i)
            t.block<3, 3>(3 * i, 3 * i) = r;
        out.stiffness = t.transpose() * k * t;
        out.mass = t.transpose() * mm * t;
        break;
    }
    case ElementKind::Plate: {
        const auto [a, b] = rectangleSides(model, e);
        plateMatrices(m, e.thickness, a, b, out.stiffness, out.mass);
        break;
    }
    case ElementKind::Membrane: {
        const auto [a, b] = rectangleSides(model, e);
        membraneMatrices(m, e.thickness, e.tension, a, b, out.stiffness, out.mass);
        break;
    }
    case ElementKind::Solid: {
        std::array<Vec3, 8> x;
        for (size_t q = 0; q < 8; ++q)
            x[q] = model.nodes[static_cast<size_t>(e.nodes[q])];
        hexMatrices(elementElasticity(model, e), elementDensity(model, e), x, out.stiffness, out.mass);
        break;
    }
    case ElementKind::Solid20:
        hex20Matrices(elementElasticity(model, e), elementDensity(model, e), solid20Positions(model, e), out.stiffness, out.mass);
        break;
    }
    return out;
}

SystemMatrices assemble(const StructuralModel& model)
{
    SystemMatrices s;
    s.map = makeDofMap(model);
    const int count = nodeCount(model.kind), dpn = model.dofsPerNode();
    std::vector<Eigen::Triplet<double>> kt, mt, lt;
    const size_t perElement = static_cast<size_t>(count * dpn) * static_cast<size_t>(count * dpn);
    kt.reserve(model.elements.size() * perElement);
    mt.reserve(model.elements.size() * perElement);
    // Structured meshes repeat the same element: cache the matrices by
    // everything they depend on.
    std::map<std::tuple<int, int, double, double, double, double, double, double>, ElementMatrices> cache;
    for (const Element& e : model.elements) {
        ElementMatrices local;
        if (model.kind == ElementKind::Plate || model.kind == ElementKind::Membrane) {
            const auto [a, b] = rectangleSides(model, e);
            const auto key = std::make_tuple(e.material, 0, a, b, e.thickness, e.tension, 0.0, 0.0);
            auto found = cache.find(key);
            if (found == cache.end())
                found = cache.emplace(key, elementMatrices(model, e)).first;
            local = found->second;
        } else if (model.kind == ElementKind::Solid && e.constitutive < 0) {
            const Vec3 d = model.nodes[static_cast<size_t>(e.nodes[6])] - model.nodes[static_cast<size_t>(e.nodes[0])];
            const Vec3 d1 = model.nodes[static_cast<size_t>(e.nodes[1])] - model.nodes[static_cast<size_t>(e.nodes[0])];
            const auto key = std::make_tuple(e.material, 1, d.x(), d.y(), d.z(), d1.x(), d1.y(), d1.z());
            auto found = cache.find(key);
            if (found == cache.end())
                found = cache.emplace(key, elementMatrices(model, e)).first;
            local = found->second;
        } else {
            local = elementMatrices(model, e);
        }
        const double eta = model.materials[static_cast<size_t>(e.material)].lossFactorStated
                               ? model.materials[static_cast<size_t>(e.material)].lossFactor
                               : 0.0;
        std::vector<int> index(static_cast<size_t>(count * dpn));
        for (int q = 0; q < count; ++q)
            for (int d = 0; d < dpn; ++d)
                index[static_cast<size_t>(q * dpn + d)] = s.map.free[model.dof(static_cast<size_t>(e.nodes[static_cast<size_t>(q)]), d)];
        for (size_t i = 0; i < index.size(); ++i) {
            if (index[i] < 0)
                continue;
            for (size_t j = 0; j < index.size(); ++j) {
                if (index[j] < 0)
                    continue;
                const double k = local.stiffness(static_cast<Eigen::Index>(i), static_cast<Eigen::Index>(j));
                kt.emplace_back(index[i], index[j], k);
                mt.emplace_back(index[i], index[j], local.mass(static_cast<Eigen::Index>(i), static_cast<Eigen::Index>(j)));
                if (eta > 0.0)
                    lt.emplace_back(index[i], index[j], eta * k);
            }
        }
    }
    // Interface elements: stiffness only (a zero-thickness spring layer has no mass).
    for (const InterfaceElement& ie : model.interfaces) {
        const Eigen::MatrixXd local = interfaceStiffness(model, ie);
        std::array<int, 48> index{};
        for (size_t q = 0; q < 8; ++q)
            for (int d = 0; d < 3; ++d) {
                index[3 * q + static_cast<size_t>(d)] = s.map.free[model.dof(static_cast<size_t>(ie.lower[q]), d)];
                index[24 + 3 * q + static_cast<size_t>(d)] = s.map.free[model.dof(static_cast<size_t>(ie.upper[q]), d)];
            }
        for (size_t i = 0; i < 48; ++i) {
            if (index[i] < 0)
                continue;
            for (size_t j = 0; j < 48; ++j)
                if (index[j] >= 0 && local(static_cast<Eigen::Index>(i), static_cast<Eigen::Index>(j)) != 0.0)
                    kt.emplace_back(index[i], index[j], local(static_cast<Eigen::Index>(i), static_cast<Eigen::Index>(j)));
        }
    }
    const auto n = static_cast<Eigen::Index>(s.map.freeCount());
    s.stiffness.resize(n, n);
    s.mass.resize(n, n);
    s.lossStiffness.resize(n, n);
    s.stiffness.setFromTriplets(kt.begin(), kt.end());
    s.mass.setFromTriplets(mt.begin(), mt.end());
    s.lossStiffness.setFromTriplets(lt.begin(), lt.end());
    s.stiffness.makeCompressed();
    s.mass.makeCompressed();
    s.lossStiffness.makeCompressed();
    s.nonZeros = static_cast<size_t>(s.stiffness.nonZeros());
    return s;
}

ElementStress elementStress(const StructuralModel& model, const Element& e, const Eigen::VectorXd& u)
{
    ElementStress out;
    const ElasticMaterial& m = model.materials[static_cast<size_t>(e.material)];
    const int count = nodeCount(model.kind), dpn = model.dofsPerNode();
    Eigen::VectorXd ue(count * dpn);
    for (int q = 0; q < count; ++q)
        for (int d = 0; d < dpn; ++d)
            ue(q * dpn + d) = u(static_cast<Eigen::Index>(model.dof(static_cast<size_t>(e.nodes[static_cast<size_t>(q)]), d)));
    switch (model.kind) {
    case ElementKind::Frame: {
        const Vec3& a = model.nodes[static_cast<size_t>(e.nodes[0])];
        const Vec3& b = model.nodes[static_cast<size_t>(e.nodes[1])];
        const double L = (b - a).norm();
        const Eigen::Matrix3d r = frameRotation(a, b, e.up);
        Eigen::Matrix<double, 12, 1> l;
        for (int i = 0; i < 4; ++i)
            l.segment<3>(3 * i) = r * ue.segment<3>(3 * i);
        const Section& s = model.sections[static_cast<size_t>(e.section)];
        const double E = m.youngsModulus;
        const double strain = (l(6) - l(0)) / L;
        // Curvatures at both ends from the cubic shape functions.
        double worst = 0.0;
        for (const double xi : {0.0, 1.0}) {
            const double h1 = (-6 + 12 * xi) / (L * L), h2 = (-4 + 6 * xi) / L, h3 = (6 - 12 * xi) / (L * L), h4 = (-2 + 6 * xi) / L;
            const double kz = h1 * l(1) + h2 * l(5) + h3 * l(7) + h4 * l(11);   // v''
            const double ky = h1 * l(2) - h2 * l(4) + h3 * l(8) - h4 * l(10);   // w''
            const double bendZ = E * std::abs(kz) * s.cy, bendY = E * std::abs(ky) * s.cz;
            const double bending = s.circular ? std::hypot(bendZ, bendY) : bendZ + bendY;
            worst = std::max(worst, std::abs(E * strain) + bending);
        }
        out.available = true;
        out.maxNormal = worst;
        out.vonMises = worst;  // torsional shear is not included
        out.description = "largest normal stress (axial + bending) at the extreme fibre of either end; torsional shear stress not included";
        break;
    }
    case ElementKind::Plate: {
        const auto [a, b] = rectangleSides(model, e);
        const Eigen::Matrix<double, 12, 12> c = plateCoefficients(a, b);
        const PlateBasis basis = plateBasis(0.0, 0.0);
        const double wxx = basis.pxx.dot(c * ue), wyy = basis.pyy.dot(c * ue), wxy = basis.pxy.dot(c * ue);
        const double nu = m.poissonsRatio, factor = m.youngsModulus * 0.5 * e.thickness / (1 - nu * nu);
        // Surface stresses of the bending field.
        const double sx = -factor * (wxx + nu * wyy), sy = -factor * (wyy + nu * wxx), txy = -factor * (1 - nu) * wxy;
        out.available = true;
        out.maxNormal = std::max(std::abs(sx), std::abs(sy));
        out.vonMises = std::sqrt(sx * sx - sx * sy + sy * sy + 3 * txy * txy);
        out.description = "bending stress at the plate surface, element centre (plane stress, von Mises)";
        break;
    }
    case ElementKind::Membrane:
        out.description = "a membrane carries its pre-tension only; no stress is calculated from its deflection";
        break;
    case ElementKind::Solid:
    case ElementKind::Solid20: {
        const Eigen::Matrix<double, 6, 1> sigma = solidStress(model, e, u, 0, 0, 0);
        const double sx = sigma(0), sy = sigma(1), sz = sigma(2), txy = sigma(3), tyz = sigma(4), tzx = sigma(5);
        out.available = true;
        out.maxNormal = std::max({std::abs(sx), std::abs(sy), std::abs(sz)});
        out.vonMises = std::sqrt(0.5 * ((sx - sy) * (sx - sy) + (sy - sz) * (sy - sz) + (sz - sx) * (sz - sx))
                                 + 3 * (txy * txy + tyz * tyz + tzx * tzx));
        out.description = "stress at the element centre (von Mises)";
        break;
    }
    }
    return out;
}

Eigen::Matrix<double, 6, 24> solidStrainMatrix(const std::array<Vec3, 8>& x, double xi, double eta, double zeta, double& detJ)
{
    return hexB(x, xi, eta, zeta, detJ);
}

Eigen::Matrix<double, 6, 6> solidElasticity(const ElasticMaterial& material)
{
    return elasticity(material);
}

int nodesPerElement(ElementKind kind)
{
    return nodeCount(kind);
}

const std::array<std::array<int, 3>, 20>& solid20NaturalCoordinates()
{
    return kHex20Xi;
}

Eigen::Matrix<double, 6, 6> elementElasticity(const StructuralModel& model, const Element& e)
{
    if (e.constitutive >= 0)
        return model.constitutive[static_cast<size_t>(e.constitutive)].stiffness;
    return elasticity(model.materials[static_cast<size_t>(e.material)]);
}

double elementDensity(const StructuralModel& model, const Element& e)
{
    if (e.constitutive >= 0)
        return model.constitutive[static_cast<size_t>(e.constitutive)].density;
    return model.materials[static_cast<size_t>(e.material)].density;
}

void solid20Shape(double xi, double eta, double zeta, Eigen::Matrix<double, 20, 1>& n, Eigen::Matrix<double, 20, 3>& dn)
{
    for (int q = 0; q < 20; ++q) {
        const double a = kHex20Xi[static_cast<size_t>(q)][0], b = kHex20Xi[static_cast<size_t>(q)][1], c = kHex20Xi[static_cast<size_t>(q)][2];
        if (a != 0 && b != 0 && c != 0) {  // corner
            const double p = 1 + a * xi, r = 1 + b * eta, t = 1 + c * zeta, sum = a * xi + b * eta + c * zeta - 2;
            n(q) = 0.125 * p * r * t * sum;
            dn(q, 0) = 0.125 * a * r * t * (sum + p);
            dn(q, 1) = 0.125 * b * p * t * (sum + r);
            dn(q, 2) = 0.125 * c * p * r * (sum + t);
        } else if (a == 0) {  // midpoint of an edge along xi
            n(q) = 0.25 * (1 - xi * xi) * (1 + b * eta) * (1 + c * zeta);
            dn(q, 0) = -0.5 * xi * (1 + b * eta) * (1 + c * zeta);
            dn(q, 1) = 0.25 * b * (1 - xi * xi) * (1 + c * zeta);
            dn(q, 2) = 0.25 * c * (1 - xi * xi) * (1 + b * eta);
        } else if (b == 0) {
            n(q) = 0.25 * (1 + a * xi) * (1 - eta * eta) * (1 + c * zeta);
            dn(q, 0) = 0.25 * a * (1 - eta * eta) * (1 + c * zeta);
            dn(q, 1) = -0.5 * eta * (1 + a * xi) * (1 + c * zeta);
            dn(q, 2) = 0.25 * c * (1 + a * xi) * (1 - eta * eta);
        } else {
            n(q) = 0.25 * (1 + a * xi) * (1 + b * eta) * (1 - zeta * zeta);
            dn(q, 0) = 0.25 * a * (1 + b * eta) * (1 - zeta * zeta);
            dn(q, 1) = 0.25 * b * (1 + a * xi) * (1 - zeta * zeta);
            dn(q, 2) = -0.5 * zeta * (1 + a * xi) * (1 + b * eta);
        }
    }
}

Eigen::Matrix<double, 20, 3> solid20Gradients(const std::array<Vec3, 20>& x, double xi, double eta, double zeta, double& detJ)
{
    Eigen::Matrix<double, 20, 1> n;
    Eigen::Matrix<double, 20, 3> dn;
    solid20Shape(xi, eta, zeta, n, dn);
    Eigen::Matrix3d j = Eigen::Matrix3d::Zero();
    for (int q = 0; q < 20; ++q)
        j += dn.row(q).transpose() * x[static_cast<size_t>(q)].transpose();
    detJ = j.determinant();
    return dn * j.inverse().transpose();
}

Eigen::Matrix<double, 6, 60> solid20StrainMatrix(const Eigen::Matrix<double, 20, 3>& gradients)
{
    return hex20B(gradients);
}

std::array<Vec3, 20> solid20Positions(const StructuralModel& model, const Element& e)
{
    std::array<Vec3, 20> x;
    for (size_t q = 0; q < 20; ++q)
        x[q] = model.nodes[static_cast<size_t>(e.nodes[q])];
    return x;
}

Eigen::Matrix<double, 6, 1> solidStress(const StructuralModel& model, const Element& e, const Eigen::VectorXd& u, double xi, double eta, double zeta)
{
    const int count = nodeCount(model.kind);
    Eigen::VectorXd ue(3 * count);
    for (int q = 0; q < count; ++q)
        for (int d = 0; d < 3; ++d)
            ue(3 * q + d) = u(static_cast<Eigen::Index>(model.dof(static_cast<size_t>(e.nodes[static_cast<size_t>(q)]), d)));
    double detJ = 0.0;
    if (model.kind == ElementKind::Solid20)
        return elementElasticity(model, e) * (hex20B(solid20Gradients(solid20Positions(model, e), xi, eta, zeta, detJ)) * ue);
    std::array<Vec3, 8> x;
    for (size_t q = 0; q < 8; ++q)
        x[q] = model.nodes[static_cast<size_t>(e.nodes[q])];
    return elementElasticity(model, e) * (hexB(x, xi, eta, zeta, detJ) * ue);
}

Eigen::MatrixXd interfaceStiffness(const StructuralModel& model, const InterfaceElement& ie)
{
    Eigen::Matrix<double, 48, 48> k = Eigen::Matrix<double, 48, 48>::Zero();
    const std::array<Vec3, 8> x = lowerFace(model, ie);
    const Gauss g = gauss(3);
    for (size_t i = 0; i < 3; ++i)
        for (size_t j = 0; j < 3; ++j) {
            double area = 0.0;
            Vec3 point;
            const Eigen::Matrix3d r = faceFrame(x, g.x[i], g.x[j], area, point);
            const size_t gp = 3 * i + j;
            const Eigen::Vector3d dvec(ie.shearStiffness * ie.shearFactor[gp], ie.shearStiffness * ie.shearFactor[gp],
                                       ie.normalStiffness * ie.normalFactor[gp]);
            const Eigen::Matrix3d d = r.transpose() * dvec.asDiagonal() * r;  // world frame
            Eigen::Matrix<double, 8, 1> n;
            Eigen::Matrix<double, 8, 2> dn;
            quad8Shape(g.x[i], g.x[j], n, dn);
            // The jump operator: u_upper - u_lower = [-N  N] (u_lower; u_upper).
            Eigen::Matrix<double, 3, 48> b = Eigen::Matrix<double, 3, 48>::Zero();
            for (int q = 0; q < 8; ++q) {
                b.block<3, 3>(0, 3 * q) = -n(q) * Eigen::Matrix3d::Identity();
                b.block<3, 3>(0, 24 + 3 * q) = n(q) * Eigen::Matrix3d::Identity();
            }
            k.noalias() += (g.w[i] * g.w[j] * area) * (b.transpose() * d * b);
        }
    return 0.5 * (k + k.transpose());
}

std::array<InterfacePointState, 9> interfaceStates(const StructuralModel& model, const InterfaceElement& ie, const Eigen::VectorXd& u)
{
    std::array<InterfacePointState, 9> out;
    const std::array<Vec3, 8> x = lowerFace(model, ie);
    const Gauss g = gauss(3);
    for (size_t i = 0; i < 3; ++i)
        for (size_t j = 0; j < 3; ++j) {
            InterfacePointState& p = out[3 * i + j];
            double area = 0.0;
            const Eigen::Matrix3d r = faceFrame(x, g.x[i], g.x[j], area, p.position);
            Eigen::Matrix<double, 8, 1> n;
            Eigen::Matrix<double, 8, 2> dn;
            quad8Shape(g.x[i], g.x[j], n, dn);
            p.jump = Vec3::Zero();
            for (size_t q = 0; q < 8; ++q)
                p.jump += n(static_cast<Eigen::Index>(q)) * (model.translation(u, static_cast<size_t>(ie.upper[q])) -
                                                             model.translation(u, static_cast<size_t>(ie.lower[q])));
            p.normal = r.row(2).transpose();
            const Eigen::Vector3d local = r * p.jump;
            p.opening = local(2);
            p.slip = std::hypot(local(0), local(1));
            const size_t gp = 3 * i + j;
            p.normalTraction = ie.normalStiffness * ie.normalFactor[gp] * p.opening;
            p.shearTraction = ie.shearStiffness * ie.shearFactor[gp] * p.slip;
            p.xi = g.x[i];
            p.eta = g.x[j];
        }
    return out;
}

std::array<Eigen::Matrix3d, 9> interfaceFrames(const StructuralModel& model, const InterfaceElement& ie)
{
    std::array<Eigen::Matrix3d, 9> out;
    const std::array<Vec3, 8> x = lowerFace(model, ie);
    const Gauss g = gauss(3);
    for (size_t i = 0; i < 3; ++i)
        for (size_t j = 0; j < 3; ++j) {
            double area = 0.0;
            Vec3 point;
            out[3 * i + j] = faceFrame(x, g.x[i], g.x[j], area, point);
        }
    return out;
}

Eigen::MatrixXd interfaceStiffness(const StructuralModel& model, const InterfaceElement& ie, const std::array<Eigen::Matrix3d, 9>& localTangent)
{
    Eigen::Matrix<double, 48, 48> k = Eigen::Matrix<double, 48, 48>::Zero();
    const std::array<Vec3, 8> x = lowerFace(model, ie);
    const Gauss g = gauss(3);
    for (size_t i = 0; i < 3; ++i)
        for (size_t j = 0; j < 3; ++j) {
            double area = 0.0;
            Vec3 point;
            const Eigen::Matrix3d r = faceFrame(x, g.x[i], g.x[j], area, point);
            const Eigen::Matrix3d d = r.transpose() * localTangent[3 * i + j] * r;
            Eigen::Matrix<double, 8, 1> n;
            Eigen::Matrix<double, 8, 2> dn;
            quad8Shape(g.x[i], g.x[j], n, dn);
            Eigen::Matrix<double, 3, 48> b = Eigen::Matrix<double, 3, 48>::Zero();
            for (int q = 0; q < 8; ++q) {
                b.block<3, 3>(0, 3 * q) = -n(q) * Eigen::Matrix3d::Identity();
                b.block<3, 3>(0, 24 + 3 * q) = n(q) * Eigen::Matrix3d::Identity();
            }
            k.noalias() += (g.w[i] * g.w[j] * area) * (b.transpose() * d * b);
        }
    return 0.5 * (k + k.transpose());
}

Eigen::Matrix<double, 48, 1> interfaceForces(const StructuralModel& model, const InterfaceElement& ie, const std::array<Eigen::Vector3d, 9>& localTraction)
{
    Eigen::Matrix<double, 48, 1> f = Eigen::Matrix<double, 48, 1>::Zero();
    const std::array<Vec3, 8> x = lowerFace(model, ie);
    const Gauss g = gauss(3);
    for (size_t i = 0; i < 3; ++i)
        for (size_t j = 0; j < 3; ++j) {
            double area = 0.0;
            Vec3 point;
            const Eigen::Matrix3d r = faceFrame(x, g.x[i], g.x[j], area, point);
            const Eigen::Vector3d t = r.transpose() * localTraction[3 * i + j];  // world
            Eigen::Matrix<double, 8, 1> n;
            Eigen::Matrix<double, 8, 2> dn;
            quad8Shape(g.x[i], g.x[j], n, dn);
            const double w = g.w[i] * g.w[j] * area;
            for (int q = 0; q < 8; ++q) {
                f.segment<3>(3 * q) -= w * n(q) * t;
                f.segment<3>(24 + 3 * q) += w * n(q) * t;
            }
        }
    return f;
}

}  // namespace resonant::mechanics
