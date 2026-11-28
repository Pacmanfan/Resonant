#pragma once

// Atomistic structures (RES-MSC-008): a periodic crystal (lattice vectors and a basis
// in fractional coordinates) or a non-periodic set of atoms. SI units (m).

#include "foundation/Result.h"

#include <Eigen/Dense>
#include <nlohmann/json.hpp>

#include <map>
#include <string>
#include <vector>

namespace resonant::atomistic {

struct Atom {
    std::string species;
    Eigen::Vector3d position = Eigen::Vector3d::Zero();  // fractional when periodic, Cartesian (m) otherwise
};

struct Crystal {
    std::string name;
    Eigen::Matrix3d lattice = Eigen::Matrix3d::Identity();  // columns a1, a2, a3 (m)
    std::vector<Atom> atoms;
    bool periodic = true;

    Status validate() const;
    double volume() const;                          // m^3 (periodic)
    std::vector<Eigen::Vector3d> cartesian() const; // m
    Crystal supercell(int n1, int n2, int n3) const;
    std::map<std::string, int> composition() const;
    nlohmann::json toJson() const;
    static Result<Crystal> fromJson(const nlohmann::json& document);
};

// sc, bcc, fcc or diamond in the conventional cubic cell of edge a (m).
Result<Crystal> cubicCrystal(const std::string& structure, double a, const std::string& species);

}  // namespace resonant::atomistic
