#include "mechanics/LayeredSolid.h"

#include "mechanics/Analyses.h"

#include <Eigen/Cholesky>
#include <Eigen/LU>
#include <Eigen/SparseCholesky>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <map>
#include <tuple>

namespace resonant::mechanics {
namespace {

constexpr double kPi = 3.14159265358979323846;

std::string num(double v)
{
    char b[64];
    std::snprintf(b, sizeof b, "%.6g", v);
    return b;
}

constexpr double kGauss3[3] = {-0.7745966692414834, 0.0, 0.7745966692414834};
constexpr double kWeight3[3] = {0.5555555555555556, 0.8888888888888888, 0.5555555555555556};

// --- the meridian of a surface of revolution ----------------------------------------------

// A Catmull-Rom curve through the points, each segment's share of s in [0, 1]
// proportional to its chord; returns the point and its derivative along s.
void profileAt(const std::vector<std::array<double, 2>>& p, double s, double& r, double& z, double& dr, double& dz)
{
    const size_t n = p.size();
    std::vector<double> cum(n, 0.0);
    for (size_t i = 1; i < n; ++i)
        cum[i] = cum[i - 1] + std::hypot(p[i][0] - p[i - 1][0], p[i][1] - p[i - 1][1]);
    const double total = cum.back();
    const double at = std::clamp(s, 0.0, 1.0) * total;
    size_t seg = 0;
    while (seg + 2 < n && at > cum[seg + 1])
        ++seg;
    const double length = cum[seg + 1] - cum[seg];
    const double t = length > 0 ? (at - cum[seg]) / length : 0.0;
    const auto point = [&](long i) -> std::array<double, 2> {
        if (i < 0)
            return {2 * p[0][0] - p[1][0], 2 * p[0][1] - p[1][1]};
        if (i >= static_cast<long>(n))
            return {2 * p[n - 1][0] - p[n - 2][0], 2 * p[n - 1][1] - p[n - 2][1]};
        return p[static_cast<size_t>(i)];
    };
    const auto p0 = point(static_cast<long>(seg) - 1), p1 = point(static_cast<long>(seg)), p2 = point(static_cast<long>(seg) + 1),
               p3 = point(static_cast<long>(seg) + 2);
    double c[2], d[2];
    for (int k = 0; k < 2; ++k) {
        const double a0 = 2 * p1[static_cast<size_t>(k)], a1 = -p0[static_cast<size_t>(k)] + p2[static_cast<size_t>(k)],
                     a2 = 2 * p0[static_cast<size_t>(k)] - 5 * p1[static_cast<size_t>(k)] + 4 * p2[static_cast<size_t>(k)] - p3[static_cast<size_t>(k)],
                     a3 = -p0[static_cast<size_t>(k)] + 3 * p1[static_cast<size_t>(k)] - 3 * p2[static_cast<size_t>(k)] + p3[static_cast<size_t>(k)];
        c[k] = 0.5 * (a0 + a1 * t + a2 * t * t + a3 * t * t * t);
        d[k] = 0.5 * (a1 + 2 * a2 * t + 3 * a3 * t * t) / (length > 0 ? length / total : 1.0);
    }
    r = c[0];
    z = c[1];
    dr = d[0];
    dz = d[1];
}

// --- mesh building ---------------------------------------------------------------------

struct Built {
    std::vector<Vec3> nodes;
    std::vector<std::array<int, 20>> elements;
    std::vector<int> elementLayer;
    std::vector<std::array<double, 3>> elementParameter;
    std::vector<std::array<int, 3>> elementCell;
    std::vector<std::vector<int>> lattice;  // per layer: lattice point -> node (-1: not a node)
    struct Face {
        int interface = 0, i = 0, j = 0;
        std::array<int, 8> lower{}, upper{};
    };
    std::vector<Face> faces;  // interfaces that do not share nodes
    int sigma = 1;            // +1: xi along u; -1: xi against u (keeps every element right-handed)
    int nu = 0, nv = 0;
    double scale = 1.0;
};

Vec3 bodyPoint(const LayeredBody& body, int layer, double u, double v, double s)
{
    Vec3 p, n, t;
    body.surface.evaluate(u, v, p, n, t);
    double h = 0.0;
    for (int l = 0; l < layer; ++l)
        h += body.layers[static_cast<size_t>(l)].thickness.at(u, v);
    h += s * body.layers[static_cast<size_t>(layer)].thickness.at(u, v);
    return p + h * n;
}

// A lookup of nodes by position (within a tolerance), for seams and poles.
class NodeIndex {
public:
    explicit NodeIndex(double tolerance) : tol_(tolerance) {}
    int find(const Vec3& x, const std::vector<Vec3>& nodes) const
    {
        const auto c = cell(x);
        for (long dx = -1; dx <= 1; ++dx)
            for (long dy = -1; dy <= 1; ++dy)
                for (long dz = -1; dz <= 1; ++dz) {
                    const auto it = map_.find({std::get<0>(c) + dx, std::get<1>(c) + dy, std::get<2>(c) + dz});
                    if (it == map_.end())
                        continue;
                    for (const int node : it->second)
                        if ((nodes[static_cast<size_t>(node)] - x).norm() <= tol_)
                            return node;
                }
        return -1;
    }
    void add(const Vec3& x, int node)
    {
        map_[cell(x)].push_back(node);
    }

private:
    std::tuple<long, long, long> cell(const Vec3& x) const
    {
        const double h = 4 * tol_;
        return {static_cast<long>(std::floor(x.x() / h)), static_cast<long>(std::floor(x.y() / h)), static_cast<long>(std::floor(x.z() / h))};
    }
    double tol_;
    std::map<std::tuple<long, long, long>, std::vector<int>> map_;
};

// Meshes the body; `shared(i)` tells whether interface i shares its nodes.
Built buildMesh(const LayeredBody& body, const std::function<bool(int)>& shared)
{
    Built b;
    b.nu = body.elementsU;
    b.nv = body.elementsV;
    // The extent of the body, for tolerances.
    {
        Vec3 lo = Vec3::Constant(1e300), hi = Vec3::Constant(-1e300);
        for (int i = 0; i <= 4; ++i)
            for (int j = 0; j <= 4; ++j)
                for (const double s : {0.0, 1.0}) {
                    const Vec3 x = bodyPoint(body, static_cast<int>(body.layers.size()) - 1, i / 4.0, j / 4.0, s);
                    lo = lo.cwiseMin(x);
                    hi = hi.cwiseMax(x);
                }
        b.scale = std::max(1e-12, (hi - lo).norm());
    }
    // Orientation: keep the elements right-handed with zeta along the normal.
    {
        const double e = 1e-4;
        const Vec3 x0 = bodyPoint(body, 0, 0.5, 0.5, 0.0);
        const Vec3 du = bodyPoint(body, 0, 0.5 + e, 0.5, 0.0) - x0, dv = bodyPoint(body, 0, 0.5, 0.5 + e, 0.0) - x0,
                   dn = bodyPoint(body, 0, 0.5, 0.5, 0.5) - x0;
        b.sigma = du.cross(dv).dot(dn) >= 0 ? 1 : -1;
    }
    const int nu = b.nu, nv = b.nv;
    const double tol = 1e-9 * b.scale;
    const auto& natural = solid20NaturalCoordinates();
    b.lattice.resize(body.layers.size());
    for (size_t layer = 0; layer < body.layers.size(); ++layer) {
        const int nt = body.layers[layer].elementsThrough;
        std::vector<int>& lat = b.lattice[layer];
        lat.assign(static_cast<size_t>((2 * nu + 1) * (2 * nv + 1) * (2 * nt + 1)), -1);
        NodeIndex index(tol);
        for (int K = 0; K <= 2 * nt; ++K)
            for (int J = 0; J <= 2 * nv; ++J)
                for (int I = 0; I <= 2 * nu; ++I) {
                    if ((I % 2) + (J % 2) + (K % 2) > 1)
                        continue;  // a face or body centre: not a node of the 20-node element
                    const int at = I + (2 * nu + 1) * (J + (2 * nv + 1) * K);
                    if (K == 0 && layer > 0 && shared(static_cast<int>(layer) - 1)) {
                        const int below = body.layers[layer - 1].elementsThrough;
                        const int node = b.lattice[layer - 1][static_cast<size_t>(I + (2 * nu + 1) * (J + (2 * nv + 1) * (2 * below)))];
                        lat[static_cast<size_t>(at)] = node;
                        if (index.find(b.nodes[static_cast<size_t>(node)], b.nodes) < 0)
                            index.add(b.nodes[static_cast<size_t>(node)], node);
                        continue;
                    }
                    const Vec3 x = bodyPoint(body, static_cast<int>(layer), I / (2.0 * nu), J / (2.0 * nv), K / (2.0 * nt));
                    int node = index.find(x, b.nodes);
                    if (node < 0) {
                        node = static_cast<int>(b.nodes.size());
                        b.nodes.push_back(x);
                        index.add(x, node);
                    }
                    lat[static_cast<size_t>(at)] = node;
                }
        for (int k = 0; k < nt; ++k)
            for (int j = 0; j < nv; ++j)
                for (int i = 0; i < nu; ++i) {
                    std::array<int, 20> e{};
                    for (size_t q = 0; q < 20; ++q) {
                        const int I = 2 * i + 1 + b.sigma * natural[q][0], J = 2 * j + 1 + natural[q][1], K = 2 * k + 1 + natural[q][2];
                        e[q] = lat[static_cast<size_t>(I + (2 * nu + 1) * (J + (2 * nv + 1) * K))];
                    }
                    b.elements.push_back(e);
                    b.elementLayer.push_back(static_cast<int>(layer));
                    b.elementParameter.push_back({(i + 0.5) / nu, (j + 0.5) / nv, (k + 0.5) / nt});
                    b.elementCell.push_back({i, j, k});
                }
    }
    // The faces of the interfaces that do not share nodes.
    for (size_t f = 0; f + 1 < body.layers.size(); ++f) {
        if (shared(static_cast<int>(f)))
            continue;
        const int ntLow = body.layers[f].elementsThrough;
        for (int j = 0; j < nv; ++j)
            for (int i = 0; i < nu; ++i) {
                Built::Face face;
                face.interface = static_cast<int>(f);
                face.i = i;
                face.j = j;
                static constexpr int top[8] = {4, 5, 6, 7, 12, 13, 14, 15}, bottom[8] = {0, 1, 2, 3, 8, 9, 10, 11};
                for (size_t q = 0; q < 8; ++q) {
                    const auto nodeAt = [&](size_t layer, int k, int qq) {
                        const int I = 2 * i + 1 + b.sigma * natural[static_cast<size_t>(qq)][0], J = 2 * j + 1 + natural[static_cast<size_t>(qq)][1],
                                  K = 2 * k + 1 + natural[static_cast<size_t>(qq)][2];
                        return b.lattice[layer][static_cast<size_t>(I + (2 * nu + 1) * (J + (2 * nv + 1) * K))];
                    };
                    face.lower[q] = nodeAt(f, ntLow - 1, top[q]);
                    face.upper[q] = nodeAt(f + 1, 0, bottom[q]);
                }
                b.faces.push_back(face);
            }
    }
    return b;
}

// The isotropic elasticity matrix.
Eigen::Matrix<double, 6, 6> isotropic(double e, double nu)
{
    const double l = e * nu / ((1 + nu) * (1 - 2 * nu)), g = e / (2 * (1 + nu));
    Eigen::Matrix<double, 6, 6> c = Eigen::Matrix<double, 6, 6>::Zero();
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j)
            c(i, j) = l;
        c(i, i) = l + 2 * g;
        c(i + 3, i + 3) = g;
    }
    return c;
}

// The graded fraction of a layer at an element (constant in it).
double gradedFraction(const LayerMaterial& m, const std::array<double, 3>& p)
{
    const double s = m.grading == LayerMaterial::Grading::Thickness ? p[2] : m.grading == LayerMaterial::Grading::U ? p[0] : p[1];
    return std::pow(std::clamp(s, 0.0, 1.0), m.exponent);
}

// The surface frame at (u, v): columns u tangent, the tangent across it, the normal.
Eigen::Matrix3d surfaceFrame(const ReferenceSurface& s, double u, double v)
{
    Vec3 p, n, t;
    s.evaluate(u, v, p, n, t);
    Vec3 e1 = (t - t.dot(n) * n);
    if (e1.norm() < 1e-12)
        e1 = n.unitOrthogonal();
    e1.normalize();
    Eigen::Matrix3d q;
    q.col(0) = e1;
    q.col(1) = n.cross(e1);
    q.col(2) = n;
    return q;
}

Eigen::Matrix3d tensorOf(const Eigen::Matrix<double, 6, 1>& s)
{
    Eigen::Matrix3d t;
    t << s(0), s(3), s(5), s(3), s(1), s(4), s(5), s(4), s(2);
    return t;
}

Eigen::Matrix<double, 6, 1> voigtOf(const Eigen::Matrix3d& t)
{
    Eigen::Matrix<double, 6, 1> s;
    s << t(0, 0), t(1, 1), t(2, 2), t(0, 1), t(1, 2), t(2, 0);
    return s;
}

double vonMisesOf(const Eigen::Matrix<double, 6, 1>& s)
{
    return std::sqrt(0.5 * ((s(0) - s(1)) * (s(0) - s(1)) + (s(1) - s(2)) * (s(1) - s(2)) + (s(2) - s(0)) * (s(2) - s(0))) +
                     3 * (s(3) * s(3) + s(4) * s(4) + s(5) * s(5)));
}

// Integrates over one face of a 20-node element: axis 0 xi, 1 eta, 2 zeta held at
// `side` (+-1); calls f(n, point, outward normal, dA) at each of 3 x 3 Gauss points.
void forFace(const std::array<Vec3, 20>& x, int axis, int side,
             const std::function<void(const Eigen::Matrix<double, 20, 1>&, const Vec3&, const Vec3&, double)>& f)
{
    for (int a = 0; a < 3; ++a)
        for (int c = 0; c < 3; ++c) {
            double nat[3];
            const int other1 = (axis + 1) % 3, other2 = (axis + 2) % 3;
            nat[axis] = side;
            nat[other1] = kGauss3[a];
            nat[other2] = kGauss3[c];
            Eigen::Matrix<double, 20, 1> n;
            Eigen::Matrix<double, 20, 3> dn;
            solid20Shape(nat[0], nat[1], nat[2], n, dn);
            Vec3 point = Vec3::Zero(), g1 = Vec3::Zero(), g2 = Vec3::Zero();
            for (size_t q = 0; q < 20; ++q) {
                point += n(static_cast<Eigen::Index>(q)) * x[q];
                g1 += dn(static_cast<Eigen::Index>(q), other1) * x[q];
                g2 += dn(static_cast<Eigen::Index>(q), other2) * x[q];
            }
            Vec3 normal = g1.cross(g2) * static_cast<double>(side);  // (other1 x other2) is along +axis for a right-handed element
            const double area = normal.norm();
            if (area <= 0.0)
                continue;  // a collapsed face (a pole)
            normal /= area;
            f(n, point, normal, area * kWeight3[a] * kWeight3[c]);
        }
}

// A penalty for an interface without a stated stiffness: 1000 E / t of the
// stiffer and thinner of the two layers next to it.
double penaltyFor(const LayeredBody& body, size_t interface)
{
    double e = 0.0, t = 1e300;
    for (const size_t l : {interface, interface + 1}) {
        const LayerMaterial& m = body.layers[l].material;
        double el = m.orthotropic ? std::max({m.orthotropic->e1, m.orthotropic->e2, m.orthotropic->e3}) : m.base.youngsModulus;
        if (m.graded)
            el = std::max(el, m.graded->youngsModulus);
        e = std::max(e, el);
        t = std::min({t, body.layers[l].thickness.start, body.layers[l].thickness.along == LayerThickness::Along::Constant
                                                              ? body.layers[l].thickness.start
                                                              : body.layers[l].thickness.end});
    }
    return 1000.0 * e / t;
}

bool sharesNodes(const LayerInterface& i)
{
    return i.mechanical == InterfaceMechanics::PerfectBonding && i.debonded.empty();
}

}  // namespace

// --- the reference surface -----------------------------------------------------------------

Status ReferenceSurface::validate() const
{
    const auto sweepOk = [&] { return sweep > 0.0 && sweep <= 2 * kPi + 1e-12; };
    switch (kind) {
    case Kind::Plane:
        if (!(length > 0.0 && width > 0.0))
            return Status::failure("a plane needs a positive length and width");
        break;
    case Kind::Cylinder:
        if (!(radius > 0.0 && length > 0.0 && sweepOk()))
            return Status::failure("a cylinder needs a positive radius and length and a sweep in (0, 2 pi]");
        break;
    case Kind::Sphere:
        if (!(radius > 0.0 && sweepOk() && polarStart >= 0.0 && polarEnd > polarStart && polarEnd <= kPi + 1e-12))
            return Status::failure("a sphere needs a positive radius, a sweep in (0, 2 pi] and 0 <= polar start < polar end <= pi");
        break;
    case Kind::Revolution:
        if (profile.size() < 2 || !sweepOk())
            return Status::failure("a surface of revolution needs at least two profile points and a sweep in (0, 2 pi]");
        for (size_t i = 0; i < profile.size(); ++i) {
            if (profile[i][0] < 0.0)
                return Status::failure("a profile point has a negative radius");
            if (i > 0 && std::hypot(profile[i][0] - profile[i - 1][0], profile[i][1] - profile[i - 1][1]) <= 0.0)
                return Status::failure("two consecutive profile points coincide");
        }
        break;
    }
    return Status::success();
}

void ReferenceSurface::evaluate(double u, double v, Vec3& point, Vec3& normal, Vec3& tangentU) const
{
    switch (kind) {
    case Kind::Plane:
        point = Vec3(u * length, v * width, 0.0);
        normal = Vec3::UnitZ();
        tangentU = Vec3::UnitX();
        return;
    case Kind::Cylinder: {
        const double t = u * sweep;
        point = Vec3(radius * std::cos(t), radius * std::sin(t), v * length);
        normal = Vec3(std::cos(t), std::sin(t), 0.0);
        tangentU = Vec3(-std::sin(t), std::cos(t), 0.0);
        return;
    }
    case Kind::Sphere: {
        const double phi = polarStart + u * (polarEnd - polarStart), t = v * sweep;
        normal = Vec3(std::sin(phi) * std::cos(t), std::sin(phi) * std::sin(t), std::cos(phi));
        point = radius * normal;
        tangentU = Vec3(std::cos(phi) * std::cos(t), std::cos(phi) * std::sin(t), -std::sin(phi));
        return;
    }
    case Kind::Revolution: {
        double r = 0, z = 0, dr = 0, dz = 0;
        profileAt(profile, u, r, z, dr, dz);
        const double t = v * sweep, len = std::hypot(dr, dz);
        const double mr = dr / len, mz = dz / len;
        point = Vec3(r * std::cos(t), r * std::sin(t), z);
        normal = Vec3(mz * std::cos(t), mz * std::sin(t), -mr);
        tangentU = Vec3(mr * std::cos(t), mr * std::sin(t), mz);
        return;
    }
    }
}

std::string ReferenceSurface::describe() const
{
    switch (kind) {
    case Kind::Plane: return "a plane " + num(length) + " m x " + num(width) + " m";
    case Kind::Cylinder: return "a cylinder of radius " + num(radius) + " m, length " + num(length) + " m, sweep " + num(sweep * 180 / kPi) + " deg";
    case Kind::Sphere:
        return "a sphere of radius " + num(radius) + " m, polar " + num(polarStart * 180 / kPi) + " to " + num(polarEnd * 180 / kPi) + " deg, sweep " +
               num(sweep * 180 / kPi) + " deg";
    case Kind::Revolution: return "a surface of revolution of " + std::to_string(profile.size()) + " profile points, sweep " + num(sweep * 180 / kPi) + " deg";
    }
    return "";
}

double LayerThickness::at(double u, double v) const
{
    switch (along) {
    case Along::Constant: return start;
    case Along::U: return start + (end - start) * u;
    case Along::V: return start + (end - start) * v;
    }
    return start;
}

std::string_view toString(InterfaceMechanics kind)
{
    switch (kind) {
    case InterfaceMechanics::PerfectBonding: return "perfect-bonding";
    case InterfaceMechanics::FiniteStiffness: return "finite-stiffness";
    case InterfaceMechanics::Sliding: return "sliding";
    case InterfaceMechanics::FrictionalContact: return "frictional-contact";
    }
    return "";
}

double LayeredBody::thicknessAt(double u, double v) const
{
    double t = 0.0;
    for (const auto& l : layers)
        t += l.thickness.at(u, v);
    return t;
}

Status LayeredBody::validate() const
{
    if (Status s = surface.validate(); !s)
        return s;
    if (layers.empty())
        return Status::failure("the body has no layer");
    if (interfaces.size() + 1 != layers.size())
        return Status::failure("a body of " + std::to_string(layers.size()) + " layers needs " + std::to_string(layers.size() - 1) +
                               " interfaces, one between every two consecutive layers (none is assumed)");
    if (elementsU < 1 || elementsV < 1)
        return Status::failure("the body needs at least one element along u and along v");
    long total = 0;
    for (const auto& l : layers) {
        const std::string who = "layer '" + l.name + "'";
        if (l.elementsThrough < 1)
            return Status::failure(who + " needs at least one element through its thickness");
        total += static_cast<long>(elementsU) * elementsV * l.elementsThrough;
        if (!(l.thickness.start > 0.0) || (l.thickness.along != LayerThickness::Along::Constant && !(l.thickness.end > 0.0)))
            return Status::failure(who + " needs a positive thickness everywhere");
    }
    if (total > 60000)
        return Status::failure("the mesh would have " + std::to_string(total) + " elements: more than the 60000 this solver accepts");
    for (const auto& i : interfaces) {
        const std::string who = "interface '" + i.name + "'";
        if (i.mechanical == InterfaceMechanics::FiniteStiffness && !(i.normalStiffness > 0.0 && i.shearStiffness >= 0.0))
            return Status::failure(who + ": finite-stiffness bonding needs a positive normal stiffness and a non-negative shear stiffness");
        if (i.mechanical == InterfaceMechanics::Sliding && !(i.normalStiffness > 0.0))
            return Status::failure(who + ": sliding needs a positive normal stiffness");
        if (i.mechanical == InterfaceMechanics::FrictionalContact && !(i.frictionCoefficient >= 0.0))
            return Status::failure(who + ": frictional contact needs a non-negative friction coefficient");
        for (const auto& box : i.debonded)
            if (!(box.u0 >= 0.0 && box.u1 > box.u0 && box.u1 <= 1.0 && box.v0 >= 0.0 && box.v1 > box.v0 && box.v1 <= 1.0))
                return Status::failure(who + ": a debonded region must be a box inside [0, 1] x [0, 1]");
        if ((i.normalStrength && !(*i.normalStrength > 0.0)) || (i.shearStrength && !(*i.shearStrength > 0.0)))
            return Status::failure(who + ": strengths must be positive");
        if (i.thermal == InterfaceThermal::Resistance && !(i.thermalResistance > 0.0))
            return Status::failure(who + ": a thermal contact resistance must be positive");
    }
    return Status::success();
}

// --- materials ---------------------------------------------------------------------------

Result<Eigen::Matrix<double, 6, 6>> orthotropicStiffness(const OrthotropicConstants& c)
{
    using R = Result<Eigen::Matrix<double, 6, 6>>;
    if (!(c.e1 > 0 && c.e2 > 0 && c.e3 > 0 && c.g12 > 0 && c.g13 > 0 && c.g23 > 0))
        return R::failure("orthotropic moduli must be positive");
    Eigen::Matrix<double, 6, 6> s = Eigen::Matrix<double, 6, 6>::Zero();
    s(0, 0) = 1 / c.e1;
    s(1, 1) = 1 / c.e2;
    s(2, 2) = 1 / c.e3;
    s(0, 1) = s(1, 0) = -c.nu12 / c.e1;
    s(0, 2) = s(2, 0) = -c.nu13 / c.e1;
    s(1, 2) = s(2, 1) = -c.nu23 / c.e2;
    s(3, 3) = 1 / c.g12;  // xy
    s(4, 4) = 1 / c.g23;  // yz
    s(5, 5) = 1 / c.g13;  // zx
    if (Eigen::LLT<Eigen::Matrix<double, 6, 6>>(s).info() != Eigen::Success)
        return R::failure("the orthotropic constants do not give a positive-definite stiffness (the Poisson ratios are too large for the moduli): "
                          "the material would be unstable; refused, not repaired");
    Eigen::Matrix<double, 6, 6> stiffness = s.inverse();
    return R(0.5 * (stiffness + stiffness.transpose()));
}

Eigen::Matrix<double, 6, 6> rotateStiffness(const Eigen::Matrix<double, 6, 6>& material, const Eigen::Matrix3d& q)
{
    // The Bond matrix of the stress transformation sigma_world = M sigma_material.
    static constexpr int pair[6][2] = {{0, 0}, {1, 1}, {2, 2}, {0, 1}, {1, 2}, {2, 0}};
    Eigen::Matrix<double, 6, 6> m;
    for (int p = 0; p < 6; ++p)
        for (int k = 0; k < 6; ++k) {
            const int i = pair[p][0], j = pair[p][1], a = pair[k][0], b = pair[k][1];
            m(p, k) = a == b ? q(i, a) * q(j, b) : q(i, a) * q(j, b) + q(i, b) * q(j, a);
        }
    const Eigen::Matrix<double, 6, 6> c = m * material * m.transpose();
    return 0.5 * (c + c.transpose());
}

// --- the mechanical mesh ------------------------------------------------------------------

namespace {

// The elastic data of every layer (the mechanical analyses need it; conduction does not).
Status validateElasticity(const LayeredBody& body)
{
    for (const auto& l : body.layers) {
        const std::string who = "layer '" + l.name + "'";
        const LayerMaterial& m = l.material;
        if (m.orthotropic) {
            if (m.graded)
                return Status::failure(who + ": a graded orthotropic layer is not supported");
            if (!(m.base.density > 0.0))
                return Status::failure(who + " needs a positive density");
            if (auto c = orthotropicStiffness(*m.orthotropic); !c)
                return Status::failure(who + ": " + c.message());
        } else if (Status s = m.base.validate(); !s) {
            return Status::failure(who + ": " + s.message());
        }
        if (m.graded) {
            if (Status s = m.graded->validate(); !s)
                return Status::failure(who + " (graded material): " + s.message());
            if (!(m.exponent > 0.0))
                return Status::failure(who + ": the grading exponent must be positive");
        }
    }
    return Status::success();
}

}  // namespace

Result<LayeredMesh> meshLayeredBody(const LayeredBody& body, const LayeredSupports& supports)
{
    using R = Result<LayeredMesh>;
    if (Status s = body.validate(); !s)
        return R::failure(s.message());
    if (Status s = validateElasticity(body); !s)
        return R::failure(s.message());
    for (const auto& i : body.interfaces)
        if (!i.mechanical)
            return R::failure("the interface '" + i.name + "' has no mechanical model: a perfect interface is never assumed (declare perfect bonding or a model)");
    LayeredMesh out;
    const Built b = buildMesh(body, [&](int i) { return sharesNodes(body.interfaces[static_cast<size_t>(i)]); });
    StructuralModel& m = out.structure;
    m.kind = ElementKind::Solid20;
    m.nodes = b.nodes;
    m.fixed.assign(m.nodes.size(), 0);
    out.elementLayer = b.elementLayer;
    out.elementParameter = b.elementParameter;
    out.elementCell = b.elementCell;
    // Materials: one per layer (its base), and an elasticity matrix per element where needed.
    for (const auto& l : body.layers) {
        ElasticMaterial base = l.material.base;
        if (l.material.orthotropic) {  // nominal values; the element's own matrix governs
            base.youngsModulus = l.material.orthotropic->e1;
            base.poissonsRatio = 0.0;
        }
        m.materials.push_back(base);
    }
    bool anyGraded = false, anyOrthotropic = false;
    for (size_t e = 0; e < b.elements.size(); ++e) {
        Element el;
        for (size_t q = 0; q < 20; ++q)
            el.nodes[q] = b.elements[e][q];
        const int layer = b.elementLayer[e];
        el.material = layer;
        const LayerMaterial& lm = body.layers[static_cast<size_t>(layer)].material;
        const auto& p = b.elementParameter[e];
        if (lm.orthotropic) {
            anyOrthotropic = true;
            const auto c = orthotropicStiffness(*lm.orthotropic);
            // Material axes: 1 at the angle from the u direction in the surface, 3 the normal.
            const Eigen::Matrix3d f = surfaceFrame(body.surface, p[0], p[1]);
            Eigen::Matrix3d q;
            q.col(0) = std::cos(lm.angle) * f.col(0) + std::sin(lm.angle) * f.col(1);
            q.col(2) = f.col(2);
            q.col(1) = q.col(2).cross(q.col(0));
            Constitutive con;
            con.stiffness = rotateStiffness(c.value(), q);
            con.density = lm.base.density;
            con.description = "orthotropic, layer '" + body.layers[static_cast<size_t>(layer)].name + "' at " + num(lm.angle * 180 / kPi) + " deg";
            el.constitutive = static_cast<int>(m.constitutive.size());
            m.constitutive.push_back(con);
        } else if (lm.graded) {
            anyGraded = true;
            const double f = gradedFraction(lm, p);
            const double e0 = (1 - f) * lm.base.youngsModulus + f * lm.graded->youngsModulus;
            const double n0 = (1 - f) * lm.base.poissonsRatio + f * lm.graded->poissonsRatio;
            Constitutive con;
            con.stiffness = isotropic(e0, n0);
            con.density = (1 - f) * lm.base.density + f * lm.graded->density;
            con.description = "graded, layer '" + body.layers[static_cast<size_t>(layer)].name + "', fraction " + num(f);
            el.constitutive = static_cast<int>(m.constitutive.size());
            m.constitutive.push_back(con);
        }
        m.elements.push_back(el);
    }
    // Interface elements.
    for (const auto& face : b.faces) {
        const LayerInterface& li = body.interfaces[static_cast<size_t>(face.interface)];
        InterfaceElement ie;
        ie.lower = face.lower;
        ie.upper = face.upper;
        ie.group = face.interface;
        switch (*li.mechanical) {
        case InterfaceMechanics::PerfectBonding:  // with a debonded region: a stated penalty for the bonded part
            ie.normalStiffness = ie.shearStiffness = penaltyFor(body, static_cast<size_t>(face.interface));
            break;
        case InterfaceMechanics::FiniteStiffness:
            ie.normalStiffness = li.normalStiffness;
            ie.shearStiffness = li.shearStiffness;
            break;
        case InterfaceMechanics::Sliding:
            ie.normalStiffness = li.normalStiffness;
            ie.shearStiffness = 0.0;
            break;
        case InterfaceMechanics::FrictionalContact:
            ie.normalStiffness = li.normalStiffness > 0 ? li.normalStiffness : penaltyFor(body, static_cast<size_t>(face.interface));
            ie.shearStiffness = li.shearStiffness > 0 ? li.shearStiffness : ie.normalStiffness;
            break;
        }
        std::array<std::array<double, 2>, 9> uv{};
        std::array<bool, 9> debonded{};
        for (int gi = 0; gi < 3; ++gi)
            for (int gj = 0; gj < 3; ++gj) {
                const size_t gp = static_cast<size_t>(3 * gi + gj);
                uv[gp] = {(face.i + 0.5 + b.sigma * kGauss3[gi] / 2) / b.nu, (face.j + 0.5 + kGauss3[gj] / 2) / b.nv};
                debonded[gp] = std::any_of(li.debonded.begin(), li.debonded.end(), [&](const ParameterBox& box) { return box.contains(uv[gp][0], uv[gp][1]); });
                if (debonded[gp])
                    ie.normalFactor[gp] = ie.shearFactor[gp] = 0.0;
            }
        m.interfaces.push_back(ie);
        out.interfaceParameter.push_back(uv);
        out.interfaceDebonded.push_back(debonded);
    }
    // Supports.
    const double tol = 1e-8 * b.scale;
    const int nu = b.nu, nv = b.nv;
    for (size_t layer = 0; layer < body.layers.size(); ++layer) {
        const int nt = body.layers[layer].elementsThrough;
        for (int K = 0; K <= 2 * nt; ++K)
            for (int J = 0; J <= 2 * nv; ++J)
                for (int I = 0; I <= 2 * nu; ++I) {
                    const int node = b.lattice[layer][static_cast<size_t>(I + (2 * nu + 1) * (J + (2 * nv + 1) * K))];
                    if (node < 0)
                        continue;
                    bool clamp = layer == 0 && K == 0 && supports.innerFaceClamped;
                    for (const BodyEdge edge : supports.clamped)
                        clamp = clamp || (edge == BodyEdge::U0 && I == 0) || (edge == BodyEdge::U1 && I == 2 * nu) || (edge == BodyEdge::V0 && J == 0) ||
                                (edge == BodyEdge::V1 && J == 2 * nv);
                    if (clamp)
                        for (int d = 0; d < 3; ++d)
                            m.fix(static_cast<size_t>(node), d);
                }
    }
    for (size_t n = 0; n < m.nodes.size(); ++n) {
        for (const SymmetryPlane& p : supports.symmetry)
            if (std::abs(m.nodes[n](p.axis) - p.value) <= tol)
                m.fix(n, p.axis);
        for (const int axis : supports.heldAxes)
            m.fix(n, axis);
    }
    out.assumptions.push_back("quadratic 20-node hexahedra (3 x 3 x 3 Gauss points): " + std::to_string(m.elements.size()) + " elements, " +
                              std::to_string(m.nodes.size()) + " nodes; geometry to second order in each element");
    out.assumptions.push_back("the reference surface: " + body.surface.describe());
    if (anyOrthotropic)
        out.assumptions.push_back("orthotropic layers: their axes follow the surface, element by element");
    if (anyGraded)
        out.assumptions.push_back("graded layers: the Voigt (rule-of-mixtures) estimate of E, nu and rho, constant in each element (an upper bound of the stiffness)");
    for (size_t i = 0; i < body.interfaces.size(); ++i) {
        const LayerInterface& li = body.interfaces[i];
        std::string text = "interface '" + li.name + "': " + std::string(toString(*li.mechanical));
        if (*li.mechanical == InterfaceMechanics::PerfectBonding && li.debonded.empty())
            text += " (shared nodes: an idealization, declared)";
        else if (*li.mechanical == InterfaceMechanics::PerfectBonding)
            text += " (bonded part by a penalty of " + num(penaltyFor(body, i)) + " N/m^3, 1000 E / t: its compliance is 0.1 % of the layer's, because of its debonded region)";
        else if (*li.mechanical == InterfaceMechanics::FrictionalContact)
            text += ", mu " + num(li.frictionCoefficient) + (li.normalStiffness > 0 ? "" : ", contact penalty " + num(penaltyFor(body, i)) + " N/m^3 (1000 E / t)");
        else
            text += ", kn " + num(li.normalStiffness) + " N/m^3, ks " + num(*li.mechanical == InterfaceMechanics::Sliding ? 0.0 : li.shearStiffness) + " N/m^3";
        if (!li.debonded.empty())
            text += "; " + std::to_string(li.debonded.size()) + " debonded region(s), traction-free";
        if (!li.provenance.empty())
            text += " [" + li.provenance + "]";
        out.assumptions.push_back(text);
    }
    if (Status s = m.validate(); !s)
        return R::failure("the mesh is not valid: " + s.message());
    return R(std::move(out));
}

// --- static --------------------------------------------------------------------------------

namespace {

// The nodal loads, full dofs.
Result<Eigen::VectorXd> layeredLoads(const LayeredBody& body, const LayeredMesh& mesh, const LayeredLoads& loads)
{
    using R = Result<Eigen::VectorXd>;
    const StructuralModel& m = mesh.structure;
    Eigen::VectorXd f = Eigen::VectorXd::Zero(static_cast<Eigen::Index>(m.dofCount()));
    const int nu = body.elementsU, nv = body.elementsV, last = static_cast<int>(body.layers.size()) - 1;
    const int sigma = [&] {
        // xi runs along u when node 1 lies at a larger u than node 0 (recorded by the mesh's first element).
        const auto& e = m.elements.front();
        const Vec3 d = m.nodes[static_cast<size_t>(e.nodes[1])] - m.nodes[static_cast<size_t>(e.nodes[0])];
        Vec3 p, n, t;
        body.surface.evaluate(mesh.elementParameter.front()[0], mesh.elementParameter.front()[1], p, n, t);
        return d.dot(t) >= 0 ? 1 : -1;
    }();
    const auto add = [&](size_t e, int axis, int side, const std::function<Vec3(const Vec3&)>& traction) {
        const std::array<Vec3, 20> x = solid20Positions(m, m.elements[e]);
        forFace(x, axis, side, [&](const Eigen::Matrix<double, 20, 1>& n, const Vec3&, const Vec3& normal, double da) {
            const Vec3 t = traction(normal);
            for (size_t q = 0; q < 20; ++q)
                for (int d = 0; d < 3; ++d)
                    f(static_cast<Eigen::Index>(m.dof(static_cast<size_t>(m.elements[e].nodes[q]), d))) += n(static_cast<Eigen::Index>(q)) * t(d) * da;
        });
    };
    for (size_t e = 0; e < m.elements.size(); ++e) {
        const int layer = mesh.elementLayer[e], k = mesh.elementCell[e][2];
        if (layer == 0 && k == 0 && loads.innerPressure != 0.0)
            add(e, 2, -1, [&](const Vec3& normal) { return Vec3(-loads.innerPressure * normal); });
        if (layer == last && k == body.layers.back().elementsThrough - 1 && (loads.outerPressure != 0.0 || loads.outerTraction.norm() > 0.0))
            add(e, 2, +1, [&](const Vec3& normal) { return Vec3(-loads.outerPressure * normal + loads.outerTraction); });
    }
    for (const auto& [edge, force] : loads.edgeForces) {
        // The edge face: U0 / U1 hold xi at -sigma / +sigma, V0 / V1 hold eta at -1 / +1.
        std::vector<std::tuple<size_t, int, int>> faces;
        for (size_t e = 0; e < m.elements.size(); ++e) {
            const auto& c = mesh.elementCell[e];
            if (edge == BodyEdge::U0 && c[0] == 0)
                faces.emplace_back(e, 0, -sigma);
            if (edge == BodyEdge::U1 && c[0] == nu - 1)
                faces.emplace_back(e, 0, sigma);
            if (edge == BodyEdge::V0 && c[1] == 0)
                faces.emplace_back(e, 1, -1);
            if (edge == BodyEdge::V1 && c[1] == nv - 1)
                faces.emplace_back(e, 1, 1);
        }
        double area = 0.0;
        for (const auto& [e, axis, side] : faces)
            forFace(solid20Positions(m, m.elements[e]), axis, side, [&](const auto&, const Vec3&, const Vec3&, double da) { area += da; });
        if (!(area > 0.0))
            return R::failure("an edge loaded with a force has no area (a collapsed edge at a pole)");
        for (const auto& [e, axis, side] : faces)
            add(e, axis, side, [&](const Vec3&) { return Vec3(force / area); });
    }
    return R(f);
}

// Assembles the interface elements' stiffness into triplets.
void addInterfaces(const StructuralModel& m, const DofMap& map, std::vector<Eigen::Triplet<double>>& kt)
{
    for (const InterfaceElement& ie : m.interfaces) {
        const Eigen::MatrixXd local = interfaceStiffness(m, ie);
        std::array<int, 48> index{};
        for (size_t q = 0; q < 8; ++q)
            for (int d = 0; d < 3; ++d) {
                index[3 * q + static_cast<size_t>(d)] = map.free[m.dof(static_cast<size_t>(ie.lower[q]), d)];
                index[24 + 3 * q + static_cast<size_t>(d)] = map.free[m.dof(static_cast<size_t>(ie.upper[q]), d)];
            }
        for (size_t i = 0; i < 48; ++i)
            if (index[i] >= 0)
                for (size_t j = 0; j < 48; ++j)
                    if (index[j] >= 0 && local(static_cast<Eigen::Index>(i), static_cast<Eigen::Index>(j)) != 0.0)
                        kt.emplace_back(index[i], index[j], local(static_cast<Eigen::Index>(i), static_cast<Eigen::Index>(j)));
    }
}

std::string fidelityOf(const LayeredBody& body)
{
    bool nonlinear = false;
    for (const auto& i : body.interfaces)
        nonlinear = nonlinear || i.mechanical == InterfaceMechanics::FrictionalContact;
    return std::string("spatially resolved, three-dimensional linear elasticity (20-node hexahedra) with explicit interface models") +
           (nonlinear ? "; frictional contact solved by Newton's method with Coulomb return mapping (holonomic)" : "");
}

}  // namespace

std::optional<double> LayeredStaticResult::maxFailureIndex(int interface) const
{
    std::optional<double> worst;
    for (const auto& p : interfacePoints)
        if (p.interface == interface && p.failureIndex)
            worst = std::max(worst.value_or(0.0), *p.failureIndex);
    return worst;
}

double LayeredStaticResult::maxVonMises(int layer) const
{
    double worst = 0.0;
    for (const auto& e : elements)
        if (e.layer == layer)
            worst = std::max(worst, e.vonMises);
    return worst;
}

Result<LayeredStaticResult> solveLayeredStatic(const LayeredBody& body, const LayeredSupports& supports, const LayeredLoads& loads)
{
    using R = Result<LayeredStaticResult>;
    auto meshed = meshLayeredBody(body, supports);
    if (!meshed)
        return R::failure(meshed.message());
    LayeredStaticResult out;
    out.mesh = std::move(meshed.value());
    StructuralModel& m = out.mesh.structure;
    out.fidelity = fidelityOf(body);
    out.assumptions = out.mesh.assumptions;
    auto f = layeredLoads(body, out.mesh, loads);
    if (!f)
        return R::failure(f.message());
    // The layers' stiffness once; the interfaces' at every stick/slip iteration.
    StructuralModel solids = m;
    solids.interfaces.clear();
    const SystemMatrices system = assemble(solids);
    if (system.map.freeCount() == 0)
        return R::failure("every degree of freedom is held: nothing can move");
    const Eigen::VectorXd fr = system.map.reduce(f.value());
    std::vector<Eigen::Triplet<double>> base;
    base.reserve(static_cast<size_t>(system.stiffness.nonZeros()));
    for (int k = 0; k < system.stiffness.outerSize(); ++k)
        for (Eigen::SparseMatrix<double>::InnerIterator it(system.stiffness, k); it; ++it)
            base.emplace_back(static_cast<int>(it.row()), static_cast<int>(it.col()), it.value());
    bool frictional = false;
    for (const auto& i : body.interfaces)
        frictional = frictional || i.mechanical == InterfaceMechanics::FrictionalContact;
    // Base stiffnesses of the frictional interface elements (their factors change).
    double bodySize = 0.0;
    {
        Vec3 lo = Vec3::Constant(1e300), hi = Vec3::Constant(-1e300);
        for (const Vec3& x : m.nodes) {
            lo = lo.cwiseMin(x);
            hi = hi.cwiseMax(x);
        }
        bodySize = (hi - lo).norm();
    }
    // Frictional points: 0 stick, 1 slip, 2 open (their final state, for the results).
    std::vector<std::array<char, 9>> fstate(m.interfaces.size());
    std::vector<std::array<double, 9>> fpressure(m.interfaces.size());
    for (auto& v : fstate)
        v.fill(0);
    for (auto& v : fpressure)
        v.fill(0.0);
    const auto isFrictional = [&](const InterfaceElement& ie) {
        return body.interfaces[static_cast<size_t>(ie.group)].mechanical == InterfaceMechanics::FrictionalContact;
    };
    // The linear part: the layers and every interface that is not a frictional contact.
    std::vector<Eigen::Triplet<double>> linear = base;
    {
        StructuralModel lin = m;
        lin.interfaces.clear();
        for (const InterfaceElement& ie : m.interfaces)
            if (!isFrictional(ie))
                lin.interfaces.push_back(ie);
        addInterfaces(lin, system.map, linear);
    }
    const auto freeDofs = static_cast<Eigen::Index>(system.map.freeCount());
    Eigen::SparseMatrix<double> klin(freeDofs, freeDofs);
    klin.setFromTriplets(linear.begin(), linear.end());
    const auto factorize = [&](const Eigen::SparseMatrix<double>& k, Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>>& ldlt) -> Status {
        ldlt.compute(k);
        // A mechanism (an unheld body, or a layer free to slide) shows as a vanishing
        // pivot; a thin, stiffly penalized body is ill-conditioned but well-posed.
        const Eigen::VectorXd pivots = ldlt.vectorD();
        const double largest = pivots.cwiseAbs().maxCoeff(), smallest = pivots.minCoeff();
        if (ldlt.info() != Eigen::Success || !pivots.allFinite() || !(smallest > 1e-12 * largest))
            return Status::failure("the body is not held (or a layer slides freely): its stiffness is singular (smallest pivot " + num(smallest / largest) +
                                   " of the largest), so it can move without deforming; add supports or an interface stiffness");
        return Status::success();
    };
    const auto solve = [&](const Eigen::SparseMatrix<double>& k, const Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>>& ldlt, const Eigen::VectorXd& rhs) {
        Eigen::VectorXd x = ldlt.solve(rhs);
        for (int refine = 0; refine < 3 && x.allFinite(); ++refine)  // iterative refinement: penalties make the system stiff
            x += ldlt.solve(rhs - k * x);
        return x;
    };
    if (!frictional) {
        Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> ldlt;
        if (Status st = factorize(klin, ldlt); !st)
            return R::failure(st.message());
        const Eigen::VectorXd ur = solve(klin, ldlt, fr);
        const double residual = ur.allFinite() ? (klin * ur - fr).norm() / std::max(1e-300, fr.norm()) : 1e300;
        if (residual > 1e-3)
            return R::failure("the static solution is not accurate (relative residual " + num(residual) +
                              " after refinement): the system is too ill-conditioned; reduce the interface penalties or refine the mesh");
        out.displacement = system.map.expand(ur);
    } else {
        // Frictional contact: Newton's method with Coulomb return mapping at every
        // contact point (open: no traction; stick: elastic penalty; slip: mu p along
        // the trial traction) and its consistent tangent, symmetric with the pressure
        // held (a quasi-Newton step), with a backtracking line search.
        struct Evaluation {
            std::vector<Eigen::Triplet<double>> triplets;
            Eigen::VectorXd force;  // reduced: the internal forces of the frictional interfaces
            std::vector<std::array<char, 9>> state;
            std::vector<std::array<double, 9>> pressure;
        };
        const auto evaluate = [&](const Eigen::VectorXd& ufull) {
            Evaluation ev;
            ev.force = Eigen::VectorXd::Zero(freeDofs);
            ev.state.assign(m.interfaces.size(), {});
            ev.pressure.assign(m.interfaces.size(), {});
            for (size_t e = 0; e < m.interfaces.size(); ++e) {
                const InterfaceElement& ie = m.interfaces[e];
                if (!isFrictional(ie))
                    continue;
                const LayerInterface& li = body.interfaces[static_cast<size_t>(ie.group)];
                const auto frames = interfaceFrames(m, ie);
                InterfaceElement unit = ie;
                unit.normalFactor.fill(1.0);
                unit.shearFactor.fill(1.0);
                const auto states = interfaceStates(m, unit, ufull);
                std::array<Eigen::Matrix3d, 9> tangent;
                std::array<Eigen::Vector3d, 9> traction;
                for (size_t gp = 0; gp < 9; ++gp) {
                    tangent[gp].setZero();
                    traction[gp].setZero();
                    ev.state[e][gp] = 2;
                    ev.pressure[e][gp] = 0.0;
                    if (out.mesh.interfaceDebonded[e][gp])
                        continue;
                    const Eigen::Vector3d local = frames[gp] * states[gp].jump;
                    if (local(2) > 0.0)
                        continue;  // open: no tension, no friction
                    const double tn = ie.normalStiffness * local(2), p = -tn, capacity = li.frictionCoefficient * p;
                    const Eigen::Vector2d te = ie.shearStiffness * local.head<2>();
                    tangent[gp](2, 2) = ie.normalStiffness;
                    traction[gp](2) = tn;
                    ev.pressure[e][gp] = p;
                    if (te.norm() <= capacity) {
                        ev.state[e][gp] = 0;
                        tangent[gp].topLeftCorner<2, 2>() = ie.shearStiffness * Eigen::Matrix2d::Identity();
                        traction[gp].head<2>() = te;
                    } else {
                        ev.state[e][gp] = 1;
                        const Eigen::Vector2d sdir = te.normalized();
                        tangent[gp].topLeftCorner<2, 2>() = capacity / te.norm() * ie.shearStiffness * (Eigen::Matrix2d::Identity() - sdir * sdir.transpose());
                        traction[gp].head<2>() = capacity * sdir;
                    }
                }
                const Eigen::MatrixXd ke = interfaceStiffness(m, ie, tangent);
                const Eigen::Matrix<double, 48, 1> fe = interfaceForces(m, ie, traction);
                std::array<int, 48> index{};
                for (size_t q = 0; q < 8; ++q)
                    for (int d = 0; d < 3; ++d) {
                        index[3 * q + static_cast<size_t>(d)] = system.map.free[m.dof(static_cast<size_t>(ie.lower[q]), d)];
                        index[24 + 3 * q + static_cast<size_t>(d)] = system.map.free[m.dof(static_cast<size_t>(ie.upper[q]), d)];
                    }
                for (size_t i = 0; i < 48; ++i) {
                    if (index[i] < 0)
                        continue;
                    ev.force(index[i]) += fe(static_cast<Eigen::Index>(i));
                    for (size_t j = 0; j < 48; ++j)
                        if (index[j] >= 0 && ke(static_cast<Eigen::Index>(i), static_cast<Eigen::Index>(j)) != 0.0)
                            ev.triplets.emplace_back(index[i], index[j], ke(static_cast<Eigen::Index>(i), static_cast<Eigen::Index>(j)));
                }
            }
            return ev;
        };
        Eigen::VectorXd ur = Eigen::VectorXd::Zero(freeDofs);
        const double fnorm = std::max(1e-300, fr.norm());
        Evaluation ev = evaluate(system.map.expand(ur));
        Eigen::VectorXd residual = fr - klin * ur - ev.force;
        const int maxIterations = 60;
        bool converged = false;
        for (int iteration = 1; iteration <= maxIterations; ++iteration) {
            std::vector<Eigen::Triplet<double>> kt = linear;
            kt.insert(kt.end(), ev.triplets.begin(), ev.triplets.end());
            Eigen::SparseMatrix<double> k(freeDofs, freeDofs);
            k.setFromTriplets(kt.begin(), kt.end());
            Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> ldlt;
            if (Status st = factorize(k, ldlt); !st)
                return R::failure(st.message() + (iteration > 1 ? " (a frictional layer slipping or open everywhere: the shear exceeds what friction and the supports hold)" : ""));
            const Eigen::VectorXd du = solve(k, ldlt, residual);
            double alpha = 1.0;
            Eigen::VectorXd trialU;
            Evaluation trialEv;
            Eigen::VectorXd trialResidual;
            for (int search = 0; search < 8; ++search) {
                trialU = ur + alpha * du;
                trialEv = evaluate(system.map.expand(trialU));
                trialResidual = fr - klin * trialU - trialEv.force;
                if (trialResidual.norm() < (1 - 1e-4 * alpha) * residual.norm() || search == 7)
                    break;
                alpha *= 0.5;
            }
            const bool sameStates = trialEv.state == ev.state;
            ur = trialU;
            ev = std::move(trialEv);
            residual = trialResidual;
            out.iterations = iteration;
            if (residual.norm() <= 1e-8 * fnorm && sameStates) {
                converged = true;
                break;
            }
        }
        out.displacement = system.map.expand(ur);
        out.converged = converged;
        if (!converged)
            out.warnings.push_back("the frictional contact iteration did not converge in " + std::to_string(maxIterations) + " Newton iterations (relative residual " +
                                   num(residual.norm() / fnorm) + "): the frictional result is not converged");
        fstate = ev.state;
        fpressure = ev.pressure;
        // The reported states: open points carry nothing; slipping points carry mu p
        // (reported from fpressure); sticking points their elastic traction.
        for (size_t e = 0; e < m.interfaces.size(); ++e) {
            if (!isFrictional(m.interfaces[e]))
                continue;
            for (size_t gp = 0; gp < 9; ++gp) {
                m.interfaces[e].normalFactor[gp] = fstate[e][gp] == 2 ? 0.0 : 1.0;
                m.interfaces[e].shearFactor[gp] = fstate[e][gp] == 0 ? 1.0 : 0.0;
            }
        }
    }
    // Results.
    for (size_t node = 0; node < m.nodes.size(); ++node)
        out.maxDisplacement = std::max(out.maxDisplacement, m.translation(out.displacement, node).norm());
    // A settled slip beyond small displacements: the interface slips through (the shear
    // exceeds what friction and the supports hold), and the model no longer applies.
    if (out.maxDisplacement > 0.01 * bodySize) {
        const std::string text = "displacements reach " + num(out.maxDisplacement) + " m, more than 1 % of the body: small-displacement theory no longer applies";
        if (frictional)  // the contact state depends on it: refused
            return R::failure(text + std::string(out.converged ? "" : " (the contact iteration did not converge)") +
                              "; a frictional interface may slip through (more shear than friction and the supports hold), or an opened layer is held too weakly");
        out.warnings.push_back(text + " (the linear result is reported as computed)");
    }
    out.compliance = f.value().dot(out.displacement);
    out.elements.reserve(m.elements.size());
    for (size_t e = 0; e < m.elements.size(); ++e) {
        LayeredElementResult r;
        r.layer = out.mesh.elementLayer[e];
        r.parameter = out.mesh.elementParameter[e];
        const std::array<Vec3, 20> x = solid20Positions(m, m.elements[e]);
        Eigen::Matrix<double, 20, 1> n;
        Eigen::Matrix<double, 20, 3> dn;
        solid20Shape(0, 0, 0, n, dn);
        for (size_t q = 0; q < 20; ++q)
            r.centre += n(static_cast<Eigen::Index>(q)) * x[q];
        r.stress = solidStress(m, m.elements[e], out.displacement, 0, 0, 0);
        const Eigen::Matrix3d q = surfaceFrame(body.surface, r.parameter[0], r.parameter[1]);
        r.surfaceStress = voigtOf(q.transpose() * tensorOf(r.stress) * q);
        r.vonMises = vonMisesOf(r.stress);
        out.elements.push_back(r);
    }
    // Interface points: interface elements, and the shared-node interfaces from the
    // stress of the layer below at its top face.
    const auto finish = [&](LayeredInterfaceResult& p, const LayerInterface& li) {
        if (li.normalStrength && li.shearStrength)
            p.failureIndex = std::pow(std::max(p.normalTraction, 0.0) / *li.normalStrength, 2) + std::pow(p.shearTraction / *li.shearStrength, 2);
    };
    size_t interpenetrating = 0;
    for (size_t e = 0; e < m.interfaces.size(); ++e) {
        const InterfaceElement& ie = m.interfaces[e];
        const LayerInterface& li = body.interfaces[static_cast<size_t>(ie.group)];
        const auto states = interfaceStates(m, ie, out.displacement);
        for (size_t gp = 0; gp < 9; ++gp) {
            LayeredInterfaceResult p;
            p.interface = ie.group;
            p.parameter = out.mesh.interfaceParameter[e][gp];
            p.position = states[gp].position;
            p.normalTraction = states[gp].normalTraction;
            p.shearTraction = states[gp].shearTraction;
            p.opening = states[gp].opening;
            p.slip = states[gp].slip;
            if (out.mesh.interfaceDebonded[e][gp]) {
                p.state = "debonded";
                if (p.opening < -1e-9 * out.maxDisplacement - 1e-300)
                    ++interpenetrating;
            } else if (li.mechanical == InterfaceMechanics::FrictionalContact) {
                p.contactPressure = std::max(0.0, -p.normalTraction);
                p.state = fstate[e][gp] == 2 ? "open" : fstate[e][gp] == 1 ? "slip" : "stick";
                if (fstate[e][gp] == 1)
                    p.shearTraction = li.frictionCoefficient * fpressure[e][gp];  // the applied friction traction
            } else {
                p.state = "bonded";
            }
            finish(p, li);
            out.interfacePoints.push_back(p);
        }
    }
    for (size_t i = 0; i < body.interfaces.size(); ++i) {
        if (!sharesNodes(body.interfaces[i]))
            continue;
        const int below = static_cast<int>(i), top = body.layers[i].elementsThrough - 1;
        for (size_t e = 0; e < m.elements.size(); ++e) {
            if (out.mesh.elementLayer[e] != below || out.mesh.elementCell[e][2] != top)
                continue;
            const std::array<Vec3, 20> x = solid20Positions(m, m.elements[e]);
            for (int a = 0; a < 3; ++a)
                for (int c = 0; c < 3; ++c) {
                    Eigen::Matrix<double, 20, 1> n;
                    Eigen::Matrix<double, 20, 3> dn;
                    solid20Shape(kGauss3[a], kGauss3[c], 1.0, n, dn);
                    Vec3 point = Vec3::Zero(), g1 = Vec3::Zero(), g2 = Vec3::Zero();
                    for (size_t q = 0; q < 20; ++q) {
                        point += n(static_cast<Eigen::Index>(q)) * x[q];
                        g1 += dn(static_cast<Eigen::Index>(q), 0) * x[q];
                        g2 += dn(static_cast<Eigen::Index>(q), 1) * x[q];
                    }
                    const Vec3 normal = g1.cross(g2).normalized();
                    const Vec3 t = tensorOf(solidStress(m, m.elements[e], out.displacement, kGauss3[a], kGauss3[c], 1.0)) * normal;
                    LayeredInterfaceResult p;
                    p.interface = static_cast<int>(i);
                    const auto& cell = out.mesh.elementCell[e];
                    p.parameter = {(cell[0] + 0.5 + kGauss3[a] / 2) / body.elementsU, (cell[1] + 0.5 + kGauss3[c] / 2) / body.elementsV};
                    p.position = point;
                    p.normalTraction = t.dot(normal);
                    p.shearTraction = (t - p.normalTraction * normal).norm();
                    p.state = "bonded";
                    finish(p, body.interfaces[i]);
                    out.interfacePoints.push_back(p);
                }
        }
    }
    if (interpenetrating > 0)
        out.warnings.push_back(std::to_string(interpenetrating) +
                               " debonded interface point(s) interpenetrate: contact in debonded regions is not modelled, so the result is not physical there");
    if (frictional)
        out.assumptions.push_back("Coulomb friction, holonomic (from the total displacement jump): valid for monotonic, proportional loading; "
                                  "no load history, no dynamic friction");
    out.assumptions.push_back("tractions at shared-node (perfectly bonded) interfaces are recovered from the stress of the layer below at its top face");
    return R(std::move(out));
}

// --- modal ---------------------------------------------------------------------------------

Result<LayeredModalResult> solveLayeredModal(const LayeredBody& body, const LayeredSupports& supports, int count)
{
    using R = Result<LayeredModalResult>;
    for (const auto& i : body.interfaces)
        if (i.mechanical == InterfaceMechanics::FrictionalContact)
            return R::failure("the interface '" + i.name + "' is a frictional contact: whether it sticks or slips depends on the load, so its modes "
                              "are not defined; refused (bound them with sliding and with finite-stiffness bonding)");
    auto meshed = meshLayeredBody(body, supports);
    if (!meshed)
        return R::failure(meshed.message());
    LayeredModalResult out;
    out.mesh = std::move(meshed.value());
    out.fidelity = fidelityOf(body) + "; free vibration";
    out.assumptions = out.mesh.assumptions;
    bool debonded = false;
    for (const auto& i : body.interfaces)
        debonded = debonded || !i.debonded.empty();
    if (debonded)
        out.warnings.push_back("debonded regions are traction-free in the modes: their faces may pass through each other in a mode shape (no contact)");
    const SystemMatrices system = assemble(out.mesh.structure);
    auto modes = solveModal(out.mesh.structure, system, count);
    if (!modes)
        return R::failure(modes.message());
    for (const auto& mode : modes.value().modes) {
        out.frequencies.push_back(mode.frequencyHz);
        out.shapes.push_back(mode.shape);
    }
    return R(std::move(out));
}

// --- steady conduction ------------------------------------------------------------------------

Result<LayeredThermalResult> solveLayeredConduction(const LayeredBody& body, const LayeredThermalBoundary& boundary)
{
    using R = Result<LayeredThermalResult>;
    if (Status s = body.validate(); !s)
        return R::failure(s.message());
    for (const auto& i : body.interfaces)
        if (!i.thermal)
            return R::failure("the interface '" + i.name + "' has no thermal model: perfect contact is never assumed (declare it, or a contact resistance)");
    for (const auto& l : body.layers) {
        if (!l.material.conductivity)
            return R::failure("the layer '" + l.name + "' has no thermal conductivity");
        if (l.material.graded && !l.material.gradedConductivity)
            return R::failure("the graded layer '" + l.name + "' has no conductivity for its graded material");
    }
    if (!boundary.innerTemperature && !boundary.outerTemperature && !boundary.outerConvection)
        return R::failure("the temperature is undetermined: give an inner or outer temperature, or an outer convection");
    if (boundary.innerTemperature && boundary.innerFlux)
        return R::failure("the inner face has both a temperature and a heat flux");
    const Built b = buildMesh(body, [&](int i) { return body.interfaces[static_cast<size_t>(i)].thermal == InterfaceThermal::Perfect; });
    const size_t n = b.nodes.size();
    std::vector<Eigen::Triplet<double>> kt;
    Eigen::VectorXd f = Eigen::VectorXd::Zero(static_cast<Eigen::Index>(n));
    std::vector<char> fixed(n, 0);
    Eigen::VectorXd t = Eigen::VectorXd::Zero(static_cast<Eigen::Index>(n));
    const int last = static_cast<int>(body.layers.size()) - 1;
    LayeredThermalResult out;
    const auto positions = [&](size_t e) {
        std::array<Vec3, 20> x;
        for (size_t q = 0; q < 20; ++q)
            x[q] = b.nodes[static_cast<size_t>(b.elements[e][q])];
        return x;
    };
    std::vector<int> innerNodes;
    for (size_t e = 0; e < b.elements.size(); ++e) {
        const int layer = b.elementLayer[e];
        const LayerMaterial& lm = body.layers[static_cast<size_t>(layer)].material;
        double k = *lm.conductivity;
        if (lm.graded) {
            const double g = gradedFraction(lm, b.elementParameter[e]);
            k = (1 - g) * k + g * *lm.gradedConductivity;
        }
        const std::array<Vec3, 20> x = positions(e);
        Eigen::Matrix<double, 20, 20> ke = Eigen::Matrix<double, 20, 20>::Zero();
        for (int a = 0; a < 3; ++a)
            for (int c = 0; c < 3; ++c)
                for (int d = 0; d < 3; ++d) {
                    double detJ = 0.0;
                    const Eigen::Matrix<double, 20, 3> g = solid20Gradients(x, kGauss3[a], kGauss3[c], kGauss3[d], detJ);
                    ke.noalias() += (k * kWeight3[a] * kWeight3[c] * kWeight3[d] * detJ) * (g * g.transpose());
                }
        for (size_t p = 0; p < 20; ++p)
            for (size_t q = 0; q < 20; ++q)
                kt.emplace_back(b.elements[e][p], b.elements[e][q], ke(static_cast<Eigen::Index>(p), static_cast<Eigen::Index>(q)));
        const int kk = b.elementCell[e][2];
        if (layer == 0 && kk == 0)
            forFace(x, 2, -1, [&](const Eigen::Matrix<double, 20, 1>& nn, const Vec3&, const Vec3&, double da) {
                out.innerArea += da;
                if (boundary.innerFlux)
                    for (size_t q = 0; q < 20; ++q)
                        f(b.elements[e][q]) += *boundary.innerFlux * nn(static_cast<Eigen::Index>(q)) * da;
            });
        if (layer == last && kk == body.layers.back().elementsThrough - 1 && boundary.outerConvection) {
            const double h = (*boundary.outerConvection)[0], ambient = (*boundary.outerConvection)[1];
            forFace(x, 2, +1, [&](const Eigen::Matrix<double, 20, 1>& nn, const Vec3&, const Vec3&, double da) {
                for (size_t p = 0; p < 20; ++p) {
                    f(b.elements[e][p]) += h * ambient * nn(static_cast<Eigen::Index>(p)) * da;
                    for (size_t q = 0; q < 20; ++q)
                        kt.emplace_back(b.elements[e][p], b.elements[e][q], h * nn(static_cast<Eigen::Index>(p)) * nn(static_cast<Eigen::Index>(q)) * da);
                }
            });
        }
    }
    // Contact resistances: a conductance 1/R between the faces.
    for (const auto& face : b.faces) {
        const double h = 1.0 / body.interfaces[static_cast<size_t>(face.interface)].thermalResistance;
        std::array<Vec3, 8> x;
        for (size_t q = 0; q < 8; ++q)
            x[q] = b.nodes[static_cast<size_t>(face.lower[q])];
        for (int a = 0; a < 3; ++a)
            for (int c = 0; c < 3; ++c) {
                // The 8-node face's shape functions, from the 20-node element's top face.
                Eigen::Matrix<double, 20, 1> n20;
                Eigen::Matrix<double, 20, 3> dn20;
                solid20Shape(kGauss3[a], kGauss3[c], 1.0, n20, dn20);
                static constexpr int top[8] = {4, 5, 6, 7, 12, 13, 14, 15};
                Eigen::Matrix<double, 8, 1> nn;
                Vec3 g1 = Vec3::Zero(), g2 = Vec3::Zero();
                for (size_t q = 0; q < 8; ++q) {
                    nn(static_cast<Eigen::Index>(q)) = n20(top[q]);
                    g1 += dn20(top[q], 0) * x[q];
                    g2 += dn20(top[q], 1) * x[q];
                }
                const double da = g1.cross(g2).norm() * kWeight3[a] * kWeight3[c];
                for (size_t p = 0; p < 8; ++p)
                    for (size_t q = 0; q < 8; ++q) {
                        const double v = h * nn(static_cast<Eigen::Index>(p)) * nn(static_cast<Eigen::Index>(q)) * da;
                        kt.emplace_back(face.lower[p], face.lower[q], v);
                        kt.emplace_back(face.upper[p], face.upper[q], v);
                        kt.emplace_back(face.lower[p], face.upper[q], -v);
                        kt.emplace_back(face.upper[p], face.lower[q], -v);
                    }
            }
    }
    // Fixed temperatures.
    const int nu = b.nu, nv = b.nv;
    for (size_t layer = 0; layer < body.layers.size(); ++layer) {
        const int nt = body.layers[layer].elementsThrough;
        for (int K : {0, 2 * nt})
            for (int J = 0; J <= 2 * nv; ++J)
                for (int I = 0; I <= 2 * nu; ++I) {
                    const int node = b.lattice[layer][static_cast<size_t>(I + (2 * nu + 1) * (J + (2 * nv + 1) * K))];
                    if (node < 0)
                        continue;
                    if (layer == 0 && K == 0) {
                        innerNodes.push_back(node);
                        if (boundary.innerTemperature) {
                            fixed[static_cast<size_t>(node)] = 1;
                            t(node) = *boundary.innerTemperature;
                        }
                    }
                    if (static_cast<int>(layer) == last && K == 2 * nt && boundary.outerTemperature) {
                        fixed[static_cast<size_t>(node)] = 1;
                        t(node) = *boundary.outerTemperature;
                    }
                }
    }
    Eigen::SparseMatrix<double> kfull(static_cast<Eigen::Index>(n), static_cast<Eigen::Index>(n));
    kfull.setFromTriplets(kt.begin(), kt.end());
    std::vector<int> freeIndex(n, -1);
    int freeCount = 0;
    for (size_t i = 0; i < n; ++i)
        if (!fixed[i])
            freeIndex[i] = freeCount++;
    if (freeCount > 0) {
        std::vector<Eigen::Triplet<double>> kr;
        Eigen::VectorXd fr = Eigen::VectorXd::Zero(freeCount);
        for (int col = 0; col < kfull.outerSize(); ++col)
            for (Eigen::SparseMatrix<double>::InnerIterator it(kfull, col); it; ++it) {
                const int r = freeIndex[static_cast<size_t>(it.row())], c = freeIndex[static_cast<size_t>(it.col())];
                if (r >= 0 && c >= 0)
                    kr.emplace_back(r, c, it.value());
                else if (r >= 0)
                    fr(r) -= it.value() * t(it.col());
            }
        for (size_t i = 0; i < n; ++i)
            if (freeIndex[i] >= 0)
                fr(freeIndex[i]) += f(static_cast<Eigen::Index>(i));
        Eigen::SparseMatrix<double> k(freeCount, freeCount);
        k.setFromTriplets(kr.begin(), kr.end());
        Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> ldlt(k);
        if (ldlt.info() != Eigen::Success)
            return R::failure("the conduction matrix could not be factorized: the temperature is not determined everywhere");
        const Eigen::VectorXd tr = ldlt.solve(fr);
        for (size_t i = 0; i < n; ++i)
            if (freeIndex[i] >= 0)
                t(static_cast<Eigen::Index>(i)) = tr(freeIndex[i]);
    }
    // Heat into the body through the inner face: the reaction at fixed inner nodes, or the flux.
    if (boundary.innerTemperature) {
        const Eigen::VectorXd reaction = kfull * t - f;
        std::sort(innerNodes.begin(), innerNodes.end());
        innerNodes.erase(std::unique(innerNodes.begin(), innerNodes.end()), innerNodes.end());
        for (const int node : innerNodes)
            out.heatIn += reaction(node);
    } else if (boundary.innerFlux) {
        out.heatIn = *boundary.innerFlux * out.innerArea;
    }
    out.nodes = b.nodes;
    out.temperature.assign(t.data(), t.data() + t.size());
    out.interfaceJump.assign(body.interfaces.size(), 0.0);
    std::vector<double> area(body.interfaces.size(), 0.0);
    for (const auto& face : b.faces) {
        std::array<Vec3, 8> x;
        for (size_t q = 0; q < 8; ++q)
            x[q] = b.nodes[static_cast<size_t>(face.lower[q])];
        for (int a = 0; a < 3; ++a)
            for (int c = 0; c < 3; ++c) {
                Eigen::Matrix<double, 20, 1> n20;
                Eigen::Matrix<double, 20, 3> dn20;
                solid20Shape(kGauss3[a], kGauss3[c], 1.0, n20, dn20);
                static constexpr int top[8] = {4, 5, 6, 7, 12, 13, 14, 15};
                double jump = 0.0;
                Vec3 g1 = Vec3::Zero(), g2 = Vec3::Zero();
                for (size_t q = 0; q < 8; ++q) {
                    jump += n20(top[q]) * (t(face.upper[q]) - t(face.lower[q]));
                    g1 += dn20(top[q], 0) * x[q];
                    g2 += dn20(top[q], 1) * x[q];
                }
                const double da = g1.cross(g2).norm() * kWeight3[a] * kWeight3[c];
                out.interfaceJump[static_cast<size_t>(face.interface)] += jump * da;
                area[static_cast<size_t>(face.interface)] += da;
            }
    }
    for (size_t i = 0; i < area.size(); ++i)
        if (area[i] > 0)
            out.interfaceJump[i] /= area[i];
    out.layerRange.assign(body.layers.size(), {1e300, -1e300});
    for (size_t e = 0; e < b.elements.size(); ++e)
        for (const int node : b.elements[e]) {
            auto& r = out.layerRange[static_cast<size_t>(b.elementLayer[e])];
            r[0] = std::min(r[0], t(node));
            r[1] = std::max(r[1], t(node));
        }
    out.fidelity = "spatially resolved, three-dimensional steady conduction (20-node hexahedra) with explicit interface contact resistances";
    out.assumptions.push_back("steady conduction, isotropic conductivity per element; no radiation; faces without a condition are insulated");
    out.assumptions.push_back("the reference surface: " + body.surface.describe());
    for (const auto& i : body.interfaces) {
        out.assumptions.push_back("interface '" + i.name + "': " + (*i.thermal == InterfaceThermal::Perfect ? std::string("perfect contact (declared)")
                                                                                                          : "contact resistance " + num(i.thermalResistance) + " m^2 K/W"));
        if (!i.debonded.empty())
            out.warnings.push_back("the debonded regions of '" + i.name + "' are not represented thermally: its declared thermal model applies over the whole interface");
    }
    return R(std::move(out));
}

}  // namespace resonant::mechanics
