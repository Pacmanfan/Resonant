#include "materials/MaterialField.h"

#include <Eigen/Cholesky>
#include <Eigen/Geometry>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace resonant::materials {
namespace {

using json = nlohmann::json;

std::string num(double v)
{
    char b[64];
    std::snprintf(b, sizeof b, "%.6g", v);
    return b;
}

// The frame's in-plane axes (perpendicular to the axis).
void inPlane(const FieldFrame& f, Vec3& a, Vec3& e1, Vec3& e2)
{
    a = f.axis.normalized();
    e1 = f.reference - f.reference.dot(a) * a;
    if (e1.norm() < 1e-12)
        e1 = a.unitOrthogonal();
    e1.normalize();
    e2 = a.cross(e1);
}

// The world point of frame coordinates.
Vec3 pointOf(const FieldFrame& f, const Vec3& q)
{
    if (f.kind == FieldFrame::Kind::Cartesian)
        return f.origin + q;
    Vec3 a, e1, e2;
    inPlane(f, a, e1, e2);
    if (f.kind == FieldFrame::Kind::Cylindrical)
        return f.origin + q.x() * (std::cos(q.y()) * e1 + std::sin(q.y()) * e2) + q.z() * a;
    return f.origin + q.x() * (std::sin(q.y()) * (std::cos(q.z()) * e1 + std::sin(q.z()) * e2) + std::cos(q.y()) * a);
}

double profileValue(const MaterialField& f, double s)
{
    if (f.profile == "constant")
        return 0.0;
    if (f.profile == "linear")
        return s;
    if (f.profile == "power-law")
        return std::pow(s, f.exponent);
    if (f.profile == "exponential")
        return f.exponent == 0.0 ? s : std::expm1(f.exponent * s) / std::expm1(f.exponent);
    // table
    if (s <= f.table.front().first)
        return f.table.front().second;
    for (size_t i = 1; i < f.table.size(); ++i)
        if (s <= f.table[i].first) {
            const double t = (s - f.table[i - 1].first) / (f.table[i].first - f.table[i - 1].first);
            return f.table[i - 1].second + t * (f.table[i].second - f.table[i - 1].second);
        }
    return f.table.back().second;
}

Status checkTensor(const FieldProperty& p, FieldRank rank, const std::vector<double>& c, const std::string& where)
{
    if (static_cast<int>(c.size()) != componentCount(rank))
        return Status::failure(where + " has " + std::to_string(c.size()) + " components; a " + std::string(toString(rank)) + " takes " +
                               std::to_string(componentCount(rank)));
    for (double v : c)
        if (!std::isfinite(v))
            return Status::failure(where + " has a component that is not finite");
    if (!p.positiveDefinite)
        return Status::success();
    const Eigen::MatrixXd t = tensorOf(rank, Eigen::Map<const Eigen::VectorXd>(c.data(), static_cast<Eigen::Index>(c.size())));
    if (rank == FieldRank::Scalar) {
        if (!(t(0, 0) > 0.0))
            return Status::failure(where + ": " + p.title + " must be positive (refused, not repaired)");
    } else if (Eigen::LLT<Eigen::MatrixXd>(t).info() != Eigen::Success) {
        return Status::failure(where + ": the " + p.title + " tensor is not positive definite (refused, not repaired)");
    }
    return Status::success();
}

json vec(const Vec3& v)
{
    return json::array({v.x(), v.y(), v.z()});
}

Result<Vec3> vecOf(const json& j, const char* what)
{
    if (!j.is_array() || j.size() != 3)
        return Result<Vec3>::failure(std::string(what) + " is three numbers");
    return Vec3(j[0].get<double>(), j[1].get<double>(), j[2].get<double>());
}

}  // namespace

std::string_view toString(FieldRank rank)
{
    switch (rank) {
    case FieldRank::Scalar: return "scalar";
    case FieldRank::Symmetric3: return "symmetric3";
    case FieldRank::Stiffness6: return "stiffness6";
    case FieldRank::Coupling36: return "coupling36";
    }
    return "scalar";
}

std::optional<FieldRank> fieldRankFromString(std::string_view text)
{
    for (const FieldRank r : {FieldRank::Scalar, FieldRank::Symmetric3, FieldRank::Stiffness6, FieldRank::Coupling36})
        if (toString(r) == text)
            return r;
    return std::nullopt;
}

int componentCount(FieldRank rank)
{
    switch (rank) {
    case FieldRank::Scalar: return 1;
    case FieldRank::Symmetric3: return 6;
    case FieldRank::Stiffness6: return 21;
    case FieldRank::Coupling36: return 18;
    }
    return 1;
}

const std::vector<FieldProperty>& fieldProperties()
{
    static const std::vector<FieldProperty> list{
        {"mechanical.density", "density", "kg/m^3", {FieldRank::Scalar}, true},
        {"mechanical.stiffness", "elastic stiffness", "Pa", {FieldRank::Stiffness6}, true},
        {"thermal.conductivity", "thermal conductivity", "W/(m K)", {FieldRank::Scalar, FieldRank::Symmetric3}, true},
        {"electromagnetic.conductivity", "electrical conductivity", "S/m", {FieldRank::Scalar, FieldRank::Symmetric3}, true},
        {"electromagnetic.permittivity", "permittivity", "F/m", {FieldRank::Scalar, FieldRank::Symmetric3}, true},
        {"piezoelectric.e", "piezoelectric coupling (stress-charge e)", "C/m^2", {FieldRank::Coupling36}, false},
        {"magnetic.permeability", "magnetic permeability", "H/m", {FieldRank::Scalar, FieldRank::Symmetric3}, true},
    };
    return list;
}

const FieldProperty* findFieldProperty(std::string_view key)
{
    for (const auto& p : fieldProperties())
        if (p.key == key)
            return &p;
    return nullptr;
}

Eigen::MatrixXd tensorOf(FieldRank rank, const Eigen::VectorXd& c)
{
    switch (rank) {
    case FieldRank::Scalar: return Eigen::MatrixXd::Constant(1, 1, c(0));
    case FieldRank::Symmetric3: {
        Eigen::Matrix3d t;
        t << c(0), c(3), c(5), c(3), c(1), c(4), c(5), c(4), c(2);
        return t;
    }
    case FieldRank::Stiffness6: {
        Eigen::MatrixXd t(6, 6);
        int k = 0;
        for (int i = 0; i < 6; ++i)
            for (int j = i; j < 6; ++j) {
                t(i, j) = t(j, i) = c(k);
                ++k;
            }
        return t;
    }
    case FieldRank::Coupling36: {
        Eigen::MatrixXd t(3, 6);
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 6; ++j)
                t(i, j) = c(6 * i + j);
        return t;
    }
    }
    return Eigen::MatrixXd();
}

Eigen::Matrix<double, 6, 6> bondMatrix(const Eigen::Matrix3d& q)
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

Vec3 FieldFrame::coordinates(const Vec3& p) const
{
    const Vec3 d = p - origin;
    if (kind == Kind::Cartesian)
        return d;
    Vec3 a, e1, e2;
    inPlane(*this, a, e1, e2);
    if (kind == Kind::Cylindrical) {
        const double z = d.dot(a);
        const Vec3 rho = d - z * a;
        return Vec3(rho.norm(), std::atan2(rho.dot(e2), rho.dot(e1)), z);
    }
    const double r = d.norm();
    return Vec3(r, r > 0 ? std::acos(std::clamp(d.dot(a) / r, -1.0, 1.0)) : 0.0, std::atan2(d.dot(e2), d.dot(e1)));
}

Eigen::Matrix3d FieldFrame::basis(const Vec3& p) const
{
    if (kind == Kind::Cartesian)
        return Eigen::Matrix3d::Identity();
    Vec3 a, e1, e2;
    inPlane(*this, a, e1, e2);
    const Vec3 q = coordinates(p);
    Eigen::Matrix3d b;
    if (kind == Kind::Cylindrical) {
        const Vec3 er = std::cos(q.y()) * e1 + std::sin(q.y()) * e2;
        b.col(0) = er;
        b.col(1) = a.cross(er);
        b.col(2) = a;
        return b;
    }
    const double th = q.y(), ph = q.z();
    const Vec3 h = std::cos(ph) * e1 + std::sin(ph) * e2;
    b.col(0) = std::sin(th) * h + std::cos(th) * a;
    b.col(1) = std::cos(th) * h - std::sin(th) * a;
    b.col(2) = -std::sin(ph) * e1 + std::cos(ph) * e2;
    return b;
}

std::string FieldFrame::describe() const
{
    const std::string k = kind == Kind::Cartesian ? "Cartesian (object axes)" : kind == Kind::Cylindrical ? "cylindrical (r, theta, z)" : "spherical (r, polar, azimuth)";
    return k + " about (" + num(origin.x()) + ", " + num(origin.y()) + ", " + num(origin.z()) + ")";
}

Status MaterialField::validate() const
{
    const std::string who = "the field '" + id + "'";
    const FieldProperty* p = findFieldProperty(property);
    if (!p)
        return Status::failure(who + " names the unknown property '" + property + "'");
    if (std::find(p->ranks.begin(), p->ranks.end(), rank) == p->ranks.end())
        return Status::failure(who + ": " + p->title + " is not a " + std::string(toString(rank)) + " field");
    if (frame.axis.norm() <= 0.0)
        return Status::failure(who + " has a zero frame axis");
    if (frameBasis && frame.kind == FieldFrame::Kind::Cartesian)
        return Status::failure(who + ": a Cartesian frame has no local basis of its own (components are in the object's axes)");
    if (source != "analytic" && source != "measured" && source != "imported" && source != "computed")
        return Status::failure(who + ": the source is analytic, measured, imported or computed");
    if (form != "analytic" && form != "sampled")
        return Status::failure(who + ": the form is analytic or sampled");
    if (form == "analytic") {
        if (Status s = checkTensor(*p, rank, value0, who + " at q0"); !s)
            return s;
        if (profile != "constant")
            if (Status s = checkTensor(*p, rank, value1, who + " at q1"); !s)
                return s;
        if (coordinate < 0 || coordinate > 2 || !(q1 != q0))
            return Status::failure(who + " needs a coordinate 0..2 and q1 != q0");
        if (profile != "constant" && profile != "linear" && profile != "power-law" && profile != "exponential" && profile != "table")
            return Status::failure(who + ": the profile is constant, linear, power-law, exponential or table");
        if (profile == "power-law" && !(exponent > 0.0))
            return Status::failure(who + ": a power law needs a positive exponent");
        if (profile == "table") {
            if (table.size() < 2)
                return Status::failure(who + ": a table needs at least two points");
            for (size_t i = 0; i < table.size(); ++i) {
                if (table[i].second < 0.0 || table[i].second > 1.0)
                    return Status::failure(who + ": a table's blend values lie in [0, 1] (a convex blend keeps the tensors valid)");
                if (i > 0 && !(table[i].first > table[i - 1].first))
                    return Status::failure(who + ": a table's positions must increase");
            }
        }
    } else {
        if (samples.empty())
            return Status::failure(who + " (sampled) has no samples");
        if (interpolation != "nearest" && interpolation != "idw" && interpolation != "linear-1d")
            return Status::failure(who + ": the interpolation is nearest, idw or linear-1d");
        if ((interpolation == "nearest" || interpolation == "idw") && !(support > 0.0))
            return Status::failure(who + ": scattered interpolation needs a support distance (no value is given far from every sample)");
        if (interpolation == "linear-1d" && (coordinate < 0 || coordinate > 2))
            return Status::failure(who + " needs a coordinate 0..2");
        for (size_t i = 0; i < samples.size(); ++i) {
            if (Status s = checkTensor(*p, rank, samples[i].components, who + " sample " + std::to_string(i)); !s)
                return s;
            if (!samples[i].uncertainty.empty() && samples[i].uncertainty.size() != samples[i].components.size())
                return Status::failure(who + " sample " + std::to_string(i) + ": one uncertainty per component");
        }
    }
    return Status::success();
}

Result<Eigen::VectorXd> MaterialField::componentsAt(const Vec3& point) const
{
    using R = Result<Eigen::VectorXd>;
    const auto map = [](const std::vector<double>& v) { return Eigen::Map<const Eigen::VectorXd>(v.data(), static_cast<Eigen::Index>(v.size())); };
    const Vec3 q = frame.coordinates(point);
    if (form == "analytic") {
        if (profile == "constant")
            return Eigen::VectorXd(map(value0));
        const double s = (q(coordinate) - q0) / (q1 - q0);
        if (s < -1e-9 || s > 1 + 1e-9)
            return R::failure("the field '" + id + "' is defined for coordinate " + std::to_string(coordinate) + " in [" + num(std::min(q0, q1)) + ", " +
                              num(std::max(q0, q1)) + "]; " + num(q(coordinate)) + " is outside (not extrapolated)");
        const double f = profileValue(*this, std::clamp(s, 0.0, 1.0));
        return Eigen::VectorXd(map(value0) + f * (map(value1) - map(value0)));
    }
    if (interpolation == "linear-1d") {
        std::vector<const FieldSample*> sorted;
        for (const auto& s : samples)
            sorted.push_back(&s);
        std::sort(sorted.begin(), sorted.end(), [&](const FieldSample* a, const FieldSample* b) { return a->at(coordinate) < b->at(coordinate); });
        const double x = q(coordinate), lo = sorted.front()->at(coordinate), hi = sorted.back()->at(coordinate);
        const double tol = 1e-9 * std::max(1.0, std::abs(hi - lo));
        if (x < lo - tol || x > hi + tol)
            return R::failure("the field '" + id + "' has samples for coordinate " + std::to_string(coordinate) + " in [" + num(lo) + ", " + num(hi) + "]; " +
                              num(x) + " is outside (not extrapolated)");
        if (sorted.size() == 1)
            return Eigen::VectorXd(map(sorted.front()->components));
        for (size_t i = 1; i < sorted.size(); ++i)
            if (x <= sorted[i]->at(coordinate) + tol || i + 1 == sorted.size()) {
                const double a = sorted[i - 1]->at(coordinate), b = sorted[i]->at(coordinate);
                const double t = b > a ? std::clamp((x - a) / (b - a), 0.0, 1.0) : 0.0;
                return Eigen::VectorXd(map(sorted[i - 1]->components) + t * (map(sorted[i]->components) - map(sorted[i - 1]->components)));
            }
    }
    // nearest / idw, by the world distance to each sample.
    double best = 1e300;
    const FieldSample* nearest = nullptr;
    Eigen::VectorXd sum = Eigen::VectorXd::Zero(componentCount(rank));
    double weights = 0.0;
    for (const auto& s : samples) {
        const double d = (pointOf(frame, s.at) - point).norm();
        if (d < best) {
            best = d;
            nearest = &s;
        }
        if (d <= support) {
            const double w = 1.0 / std::max(d * d, 1e-300);
            sum += w * map(s.components);
            weights += w;
        }
    }
    if (best > support)
        return R::failure("the field '" + id + "' has no sample within " + num(support) + " m of the point (the nearest is " + num(best) + " m away): not extrapolated");
    if (interpolation == "nearest" || best <= 1e-12)
        return Eigen::VectorXd(map(nearest->components));
    return Eigen::VectorXd(sum / weights);
}

Result<Eigen::MatrixXd> MaterialField::tensorAt(const Vec3& point) const
{
    auto c = componentsAt(point);
    if (!c)
        return Result<Eigen::MatrixXd>::failure(c.message());
    const Eigen::MatrixXd local = tensorOf(rank, c.value());
    if (!frameBasis || rank == FieldRank::Scalar)
        return local;
    const Eigen::Matrix3d q = frame.basis(point);  // columns: the local axes in the object's axes
    switch (rank) {
    case FieldRank::Symmetric3: return Eigen::MatrixXd(q * local * q.transpose());
    case FieldRank::Stiffness6: {
        const Eigen::Matrix<double, 6, 6> m = bondMatrix(q);
        return Eigen::MatrixXd(m * local * m.transpose());
    }
    case FieldRank::Coupling36: return Eigen::MatrixXd(q * local * bondMatrix(q).transpose());
    default: return local;
    }
}

json MaterialField::toJson() const
{
    json s = json::array();
    for (const auto& x : samples) {
        json j{{"at", vec(x.at)}, {"components", x.components}};
        if (!x.uncertainty.empty())
            j["uncertainty"] = x.uncertainty;
        s.push_back(j);
    }
    json t = json::array();
    for (const auto& [a, b] : table)
        t.push_back(json::array({a, b}));
    const std::string kind = frame.kind == FieldFrame::Kind::Cartesian ? "cartesian" : frame.kind == FieldFrame::Kind::Cylindrical ? "cylindrical" : "spherical";
    return {{"id", id},
            {"region", region},
            {"property", property},
            {"rank", std::string(toString(rank))},
            {"unit", unit},
            {"frame", {{"kind", kind}, {"origin", vec(frame.origin)}, {"axis", vec(frame.axis)}, {"reference", vec(frame.reference)}}},
            {"frameBasis", frameBasis},
            {"form", form},
            {"source", source},
            {"provenance", provenance},
            {"value0", value0},
            {"value1", value1},
            {"coordinate", coordinate},
            {"q0", q0},
            {"q1", q1},
            {"profile", profile},
            {"exponent", exponent},
            {"table", t},
            {"samples", s},
            {"interpolation", interpolation},
            {"support", support}};
}

Result<MaterialField> MaterialField::fromJson(const json& j)
{
    using R = Result<MaterialField>;
    if (!j.is_object())
        return R::failure("a material field is an object");
    MaterialField f;
    try {
        f.id = j.value("id", std::string());
        f.region = j.value("region", std::string());
        f.property = j.value("property", std::string());
        const auto rank = fieldRankFromString(j.value("rank", std::string("scalar")));
        if (!rank)
            return R::failure("the field '" + f.id + "' has an unknown rank");
        f.rank = *rank;
        f.unit = j.value("unit", std::string());
        if (j.contains("frame")) {
            const json& fr = j["frame"];
            const std::string kind = fr.value("kind", std::string("cartesian"));
            f.frame.kind = kind == "cylindrical" ? FieldFrame::Kind::Cylindrical : kind == "spherical" ? FieldFrame::Kind::Spherical : FieldFrame::Kind::Cartesian;
            if (kind != "cartesian" && kind != "cylindrical" && kind != "spherical")
                return R::failure("the field '" + f.id + "': a frame is cartesian, cylindrical or spherical");
            for (const auto& [key, target] : std::vector<std::pair<const char*, Vec3*>>{{"origin", &f.frame.origin}, {"axis", &f.frame.axis}, {"reference", &f.frame.reference}})
                if (fr.contains(key)) {
                    auto v = vecOf(fr[key], key);
                    if (!v)
                        return R::failure(v.message());
                    *target = v.value();
                }
        }
        f.frameBasis = j.value("frameBasis", false);
        f.source = j.value("source", std::string("analytic"));
        f.form = j.value("form", std::string(j.contains("samples") && !j["samples"].empty() ? "sampled" : "analytic"));
        f.provenance = j.value("provenance", std::string("unstated"));
        f.value0 = j.value("value0", std::vector<double>{});
        f.value1 = j.value("value1", std::vector<double>{});
        f.coordinate = j.value("coordinate", 2);
        f.q0 = j.value("q0", 0.0);
        f.q1 = j.value("q1", 1.0);
        f.profile = j.value("profile", std::string("linear"));
        f.exponent = j.value("exponent", 1.0);
        for (const auto& p : j.value("table", json::array()))
            f.table.emplace_back(p.at(0).get<double>(), p.at(1).get<double>());
        for (const auto& s : j.value("samples", json::array())) {
            FieldSample x;
            auto at = vecOf(s.value("at", json()), "a sample's position");
            if (!at)
                return R::failure(at.message());
            x.at = at.value();
            x.components = s.value("components", std::vector<double>{});
            x.uncertainty = s.value("uncertainty", std::vector<double>{});
            f.samples.push_back(x);
        }
        f.interpolation = j.value("interpolation", std::string("linear-1d"));
        f.support = j.value("support", 0.0);
    } catch (const std::exception& e) {
        return R::failure("the material field could not be read: " + std::string(e.what()));
    }
    return f;
}

}  // namespace resonant::materials
