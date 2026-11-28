#include "atomistic/Crystal.h"

#include <cmath>

namespace resonant::atomistic {

using nlohmann::json;

Status Crystal::validate() const
{
    if (atoms.empty())
        return Status::failure("the structure has no atoms");
    for (const Atom& a : atoms) {
        if (a.species.empty())
            return Status::failure("every atom needs a species");
        if (!a.position.allFinite())
            return Status::failure("every atom needs a finite position");
    }
    if (periodic) {
        if (!lattice.allFinite() || !(std::abs(lattice.determinant()) > 0.0))
            return Status::failure("a periodic structure needs three independent lattice vectors");
        if (lattice.determinant() < 0.0)
            return Status::failure("the lattice vectors must be right-handed");
    }
    return Status::success();
}

double Crystal::volume() const
{
    return periodic ? std::abs(lattice.determinant()) : 0.0;
}

std::vector<Eigen::Vector3d> Crystal::cartesian() const
{
    std::vector<Eigen::Vector3d> x;
    for (const Atom& a : atoms)
        x.push_back(periodic ? Eigen::Vector3d(lattice * a.position) : a.position);
    return x;
}

Crystal Crystal::supercell(int n1, int n2, int n3) const
{
    Crystal s;
    s.name = name + " (" + std::to_string(n1) + "x" + std::to_string(n2) + "x" + std::to_string(n3) + ")";
    s.periodic = periodic;
    s.lattice = lattice;
    s.lattice.col(0) *= n1;
    s.lattice.col(1) *= n2;
    s.lattice.col(2) *= n3;
    for (int i = 0; i < n1; ++i)
        for (int j = 0; j < n2; ++j)
            for (int k = 0; k < n3; ++k)
                for (const Atom& a : atoms)
                    s.atoms.push_back({a.species, Eigen::Vector3d((a.position.x() + i) / n1, (a.position.y() + j) / n2, (a.position.z() + k) / n3)});
    return s;
}

std::map<std::string, int> Crystal::composition() const
{
    std::map<std::string, int> c;
    for (const Atom& a : atoms)
        ++c[a.species];
    return c;
}

json Crystal::toJson() const
{
    json atomsJson = json::array();
    for (const Atom& a : atoms)
        atomsJson.push_back({{"species", a.species}, {"position", {a.position.x(), a.position.y(), a.position.z()}}});
    json lat = json::array();
    for (int c = 0; c < 3; ++c)
        lat.push_back({lattice(0, c), lattice(1, c), lattice(2, c)});
    return {{"name", name}, {"periodic", periodic}, {"lattice", lat}, {"atoms", atomsJson},
            {"units", periodic ? "lattice vectors in m; positions fractional" : "positions in m"}};
}

Result<Crystal> Crystal::fromJson(const json& d)
{
    using R = Result<Crystal>;
    try {
        Crystal c;
        c.name = d.value("name", "");
        c.periodic = d.value("periodic", true);
        if (c.periodic) {
            const auto lat = d.at("lattice").get<std::vector<std::vector<double>>>();
            if (lat.size() != 3)
                return R::failure("a periodic structure needs three lattice vectors");
            for (int k = 0; k < 3; ++k) {
                if (lat[static_cast<size_t>(k)].size() != 3)
                    return R::failure("a lattice vector has three components");
                c.lattice.col(k) = Eigen::Vector3d(lat[static_cast<size_t>(k)][0], lat[static_cast<size_t>(k)][1], lat[static_cast<size_t>(k)][2]);
            }
        }
        for (const json& a : d.at("atoms")) {
            const auto p = a.at("position").get<std::vector<double>>();
            if (p.size() != 3)
                return R::failure("an atom's position has three components");
            c.atoms.push_back({a.at("species").get<std::string>(), Eigen::Vector3d(p[0], p[1], p[2])});
        }
        if (Status v = c.validate(); !v)
            return R::failure(v.message());
        return c;
    } catch (const std::exception& e) {
        return R::failure(std::string("invalid structure: ") + e.what());
    }
}

Result<Crystal> cubicCrystal(const std::string& structure, double a, const std::string& species)
{
    using R = Result<Crystal>;
    if (!(a > 0.0))
        return R::failure("the lattice parameter must be positive");
    std::vector<Eigen::Vector3d> basis;
    if (structure == "sc")
        basis = {{0, 0, 0}};
    else if (structure == "bcc")
        basis = {{0, 0, 0}, {0.5, 0.5, 0.5}};
    else if (structure == "fcc")
        basis = {{0, 0, 0}, {0.5, 0.5, 0}, {0.5, 0, 0.5}, {0, 0.5, 0.5}};
    else if (structure == "diamond")
        basis = {{0, 0, 0}, {0.5, 0.5, 0}, {0.5, 0, 0.5}, {0, 0.5, 0.5}, {0.25, 0.25, 0.25}, {0.75, 0.75, 0.25}, {0.75, 0.25, 0.75}, {0.25, 0.75, 0.75}};
    else
        return R::failure("unknown structure '" + structure + "' (sc, bcc, fcc, diamond)");
    Crystal c;
    c.name = species + " " + structure;
    c.lattice = Eigen::Matrix3d::Identity() * a;
    for (const auto& p : basis)
        c.atoms.push_back({species, p});
    return c;
}

}  // namespace resonant::atomistic
