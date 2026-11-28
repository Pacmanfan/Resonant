#include "atomistic/ForceField.h"

#include <algorithm>
#include <cmath>

namespace resonant::atomistic {

namespace {
constexpr double kBoltzmann = 1.380649e-23;     // J/K (exact)
constexpr double kAtomicMass = 1.66053906660e-27;  // kg (CODATA 2018)
}

SpeciesParameters ForceField::pair(const std::string& a, const std::string& b) const
{
    const SpeciesParameters& x = species.at(a);
    const SpeciesParameters& y = species.at(b);
    return {std::sqrt(x.epsilon * y.epsilon), 0.5 * (x.sigma + y.sigma), 0.0};
}

double ForceField::maxSigma() const
{
    double s = 0.0;
    for (const auto& [speciesName, p] : species)
        s = std::max(s, p.sigma);
    return s;
}

nlohmann::json ForceField::toJson() const
{
    nlohmann::json sp = nlohmann::json::object();
    for (const auto& [speciesName, p] : species)
        sp[speciesName] = {{"epsilonJ", p.epsilon}, {"epsilonOverKB", p.epsilon / kBoltzmann}, {"sigmaM", p.sigma}, {"massKg", p.mass}};
    return {{"id", id},       {"name", name},         {"version", version},  {"form", form},           {"species", sp},
            {"cutoffSigma", cutoffSigma}, {"source", source}, {"validity", validity}, {"limitations", limitations},
            {"mixing", "Lorentz-Berthelot"}};
}

const std::vector<ForceField>& forceFields()
{
    static const std::vector<ForceField> fields{
        {"lj.argon", "Lennard-Jones argon", "1", "lennard-jones-12-6",
         {{"Ar", {119.8 * kBoltzmann, 3.405e-10, 39.948 * kAtomicMass}}},
         10.0,
         "the widely used argon parameters eps/kB = 119.8 K, sigma = 3.405 A (fitted to gas and liquid properties; see Allen and Tildesley, "
         "Computer Simulation of Liquids, 2nd ed., 2017); mass 39.948 u",
         "solid and liquid argon at low pressure; static (0 K) results omit zero-point motion",
         {"a two-body potential: no three-body (Axilrod-Teller) forces, so solid-state properties are approximate",
          "classical: zero-point motion (significant for argon) is absent from lattice statics",
          "argon only"}},
        {"lj.reduced", "Lennard-Jones in reduced units", "1", "lennard-jones-12-6",
         {{"LJ", {1.0, 1.0, 1.0}}},
         10.0,
         "reduced units (eps = 1 J, sigma = 1 m, m = 1 kg): the dimensionless reference model",
         "any state of the model itself",
         {"a model system, not a material"}},
    };
    return fields;
}

const ForceField* findForceField(std::string_view id)
{
    for (const ForceField& f : forceFields())
        if (f.id == id)
            return &f;
    return nullptr;
}

std::vector<std::string> incompatibleSpecies(const ForceField& field, const Crystal& structure)
{
    std::vector<std::string> out;
    for (const auto& [s, count] : structure.composition())
        if (!field.species.count(s))
            out.push_back(s);
    return out;
}

}  // namespace resonant::atomistic
