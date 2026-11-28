#include "coupling/EngineAdapters.h"

#include "atomistic/ForceField.h"
#include "atomistic/LatticeStatics.h"
#include "multiscale/ExternalSolvers.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>

namespace resonant::coupling {
namespace {

using json = nlohmann::json;

constexpr double kElectronVolt = 1.602176634e-19;  // J
constexpr double kAvogadro = 6.02214076e23;

std::string num(double v)
{
    char b[64];
    std::snprintf(b, sizeof b, "%.10g", v);
    return b;
}

std::string digest(const std::string& text)
{
    char b[32];
    std::snprintf(b, sizeof b, "%016zx", std::hash<std::string>{}(text));
    return b;
}

// The 21 upper entries of a 6x6 matrix, row by row.
std::vector<double> upper(const Eigen::Matrix<double, 6, 6>& m)
{
    std::vector<double> v;
    for (int i = 0; i < 6; ++i)
        for (int j = i; j < 6; ++j)
            v.push_back(m(i, j));
    return v;
}

// The atomistic module's Voigt order (xx yy zz yz xz xy) to the mechanics order (xx yy zz xy yz zx).
Eigen::Matrix<double, 6, 6> toMechanicsOrder(const Eigen::Matrix<double, 6, 6>& atomistic)
{
    static constexpr int to[6] = {0, 1, 2, 4, 5, 3};
    Eigen::Matrix<double, 6, 6> m;
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j)
            m(to[i], to[j]) = atomistic(i, j);
    return m;
}

const char* kStiffnessRepresentation = "stiffness6: the 21 upper entries row by row of the 6x6 Voigt stiffness in the mechanics order xx yy zz xy yz zx (engineering shear)";

// --- the in-house static lattice ---------------------------------------------------------

class LatticeStaticsAdapter final : public EngineAdapter {
public:
    EngineApplicability applicability() const override
    {
        return {"resonant.lattice-statics",
                "Static lattice (in-house)",
                {"crystalline solids described by the built-in pair potentials (noble-gas solids)"},
                {"elastic-constants", "density", "cohesive-energy"},
                {"finite temperature (the lattice is static, 0 K)", "metals, covalent and ionic solids (two-body potentials do not describe them)",
                 "liquids, diffusion and transport"},
                "relaxes the lattice isotropically and differentiates the energy (atomistic/LatticeStatics); no external program"};
    }
    Status available() const override { return Status::success(); }
    Result<std::string> prepare(const AtomisticTask& task) const override
    {
        using R = Result<std::string>;
        const auto app = applicability();
        if (std::find(app.properties.begin(), app.properties.end(), task.property) == app.properties.end())
            return R::failure("the static lattice computes elastic constants, density and cohesive energy, not '" + task.property + "'");
        if (task.temperature != 0.0)
            return R::failure("the static lattice is at 0 K: a finite temperature needs molecular dynamics (an external engine)");
        const atomistic::ForceField* ff = atomistic::findForceField(task.forceField);
        if (!ff)
            return R::failure("no built-in force field '" + task.forceField + "'");
        if (Status s = task.structure.validate(); !s)
            return R::failure(s.message());
        const auto missing = atomistic::incompatibleSpecies(*ff, task.structure);
        if (!missing.empty())
            return R::failure("the force field '" + ff->id + "' does not describe the species " + missing.front() + ": outside its applicability");
        return json{{"engine", "resonant.lattice-statics"},
                    {"property", task.property},
                    {"structure", task.structure.toJson()},
                    {"forceField", ff->toJson()},
                    {"strain", task.strain},
                    {"pressure", task.pressure}}
            .dump(1);
    }
    Result<std::vector<TransferredProperty>> run(const AtomisticTask& task, const std::string&) const override
    {
        using R = Result<std::vector<TransferredProperty>>;
        auto input = prepare(task);
        if (!input)
            return R::failure(input.message());
        if (task.pressure != 0.0)
            return R::failure("the static lattice relaxes to zero pressure only");
        const atomistic::ForceField& ff = *atomistic::findForceField(task.forceField);
        auto relaxed = atomistic::relaxIsotropic(task.structure, ff);
        if (!relaxed)
            return R::failure(relaxed.message());
        auto c1 = atomistic::elasticConstants(relaxed.value().crystal, ff, {}, task.strain);
        auto c2 = atomistic::elasticConstants(relaxed.value().crystal, ff, {}, 2 * task.strain);
        if (!c1 || !c2)
            return R::failure(!c1 ? c1.message() : c2.message());
        TransferredProperty base;
        base.temperature = 0.0;
        base.pressure = relaxed.value().pressure;
        base.composition = task.structure.composition();
        base.conditions = {{"ensemble", "static lattice (0 K)"},
                           {"relaxation", "isotropic, to zero pressure"},
                           {"strain", task.strain},
                           {"atoms", relaxed.value().crystal.atoms.size()},
                           {"cutoffSigma", ff.cutoffSigma},
                           {"tailCorrection", true}};
        base.engine = "resonant.lattice-statics";
        base.engineVersion = "1";
        base.forceField = ff.id + " v" + ff.version + " (" + ff.source + ")";
        base.inputDigest = digest(input.value());
        base.frame = "the crystal's cell axes";
        std::vector<TransferredProperty> out;
        if (task.property == "elastic-constants") {
            TransferredProperty p = base;
            p.quantity = "elastic-constants";
            p.representation = kStiffnessRepresentation;
            const Eigen::Matrix<double, 6, 6> c = toMechanicsOrder(c1.value().C), cc = toMechanicsOrder(c2.value().C);
            p.values = upper(c);
            for (double d : upper(c - cc))
                p.uncertainty.push_back(std::abs(d));
            p.uncertaintyKind = "numerical (the difference between strains " + num(task.strain) + " and " + num(2 * task.strain) + ")";
            p.unit = "Pa";
            p.method = "finite strain differences of the static energy, Voigt-ordered for the continuum";
            p.warnings = c1.value().warnings;
            if (!c1.value().stable)
                p.warnings.push_back("the lattice is not Born-stable: the constants are not positive definite");
            out.push_back(p);
        } else if (task.property == "density") {
            TransferredProperty p = base;
            p.quantity = "density";
            p.representation = "scalar";
            p.values = {c1.value().density};
            p.unit = "kg/m^3";
            p.method = "the mass of the relaxed cell over its volume";
            out.push_back(p);
        } else {
            TransferredProperty p = base;
            p.quantity = "cohesive-energy";
            p.representation = "scalar (per atom)";
            p.values = {-relaxed.value().energyPerAtom};
            p.unit = "J";
            p.method = "minus the energy per atom of the relaxed lattice";
            out.push_back(p);
        }
        return out;
    }
};

// --- LAMMPS ---------------------------------------------------------------------------------

std::string lammpsExecutable()
{
    for (const char* name : {"lmp", "lmp_serial", "lmp_mpi"}) {
        const std::string p = multiscale::findExecutable(name);
        if (!p.empty())
            return p;
    }
    return {};
}

class LammpsAdapter final : public EngineAdapter {
public:
    EngineApplicability applicability() const override
    {
        return {"lammps",
                "LAMMPS",
                {"crystalline and amorphous solids", "metals (EAM, MEAM)", "ceramics and covalent solids (Tersoff, ReaxFF)", "polymers", "liquids",
                 "interfaces and defects"},
                {"elastic-constants", "density", "thermal-conductivity", "diffusion", "interface-energy", "defect-energy"},
                {"electronic structure, band gaps and optical response (it has no electrons)", "anything outside the chosen potential's fitted range"},
                "the potential decides the validity, not the engine; this adapter prepares elastic constants of orthogonal cells with the built-in "
                "pair potentials and parses their output"};
    }
    Status available() const override
    {
        if (lammpsExecutable().empty())
            return Status::failure("LAMMPS is not installed here: none of lmp, lmp_serial, lmp_mpi is on PATH");
        return Status::success();
    }
    Result<std::string> prepare(const AtomisticTask& task) const override
    {
        using R = Result<std::string>;
        if (task.property != "elastic-constants")
            return R::failure("this adapter prepares elastic constants only; '" + task.property + "' is within LAMMPS's applicability but not prepared here");
        const atomistic::ForceField* ff = atomistic::findForceField(task.forceField);
        if (!ff)
            return R::failure("no built-in force field '" + task.forceField + "'");
        if (Status s = task.structure.validate(); !s)
            return R::failure(s.message());
        if (!atomistic::incompatibleSpecies(*ff, task.structure).empty())
            return R::failure("the force field '" + ff->id + "' does not describe every species of the structure");
        const Eigen::Matrix3d& l = task.structure.lattice;
        if (std::abs(l(1, 0)) + std::abs(l(2, 0)) + std::abs(l(0, 1)) + std::abs(l(2, 1)) + std::abs(l(0, 2)) + std::abs(l(1, 2)) > 1e-12 * l.norm())
            return R::failure("this adapter writes orthogonal cells only; a triclinic cell is not prepared");
        if (task.temperature != 0.0)
            return R::failure("this adapter prepares static (minimized) deformations only; finite temperature is not prepared");
        // Species types and the data file (metal units: A, eV, g/mol).
        std::vector<std::string> species;
        for (const auto& [s, n] : task.structure.composition())
            species.push_back(s);
        std::ostringstream data;
        data << "# Resonant structure '" << task.structure.name << "'\n\n" << task.structure.atoms.size() << " atoms\n" << species.size() << " atom types\n\n";
        data << "0 " << num(l(0, 0) * 1e10) << " xlo xhi\n0 " << num(l(1, 1) * 1e10) << " ylo yhi\n0 " << num(l(2, 2) * 1e10) << " zlo zhi\n\nMasses\n\n";
        for (size_t t = 0; t < species.size(); ++t)
            data << t + 1 << " " << num(ff->species.at(species[t]).mass * kAvogadro * 1e3) << "\n";
        data << "\nAtoms # atomic\n\n";
        const auto cart = task.structure.cartesian();
        for (size_t i = 0; i < cart.size(); ++i) {
            const auto t = std::find(species.begin(), species.end(), task.structure.atoms[i].species) - species.begin() + 1;
            data << i + 1 << " " << t << " " << num(cart[i].x() * 1e10) << " " << num(cart[i].y() * 1e10) << " " << num(cart[i].z() * 1e10) << "\n";
        }
        std::ostringstream in;
        const auto pairLines = [&] {
            std::ostringstream p;
            p << "pair_style lj/cut " << num(ff->cutoffSigma * ff->maxSigma() * 1e10) << "\n";
            for (size_t a = 0; a < species.size(); ++a)
                for (size_t b = a; b < species.size(); ++b) {
                    const auto pp = ff->pair(species[a], species[b]);
                    p << "pair_coeff " << a + 1 << " " << b + 1 << " " << num(pp.epsilon / kElectronVolt) << " " << num(pp.sigma * 1e10) << "\n";
                }
            p << "pair_modify shift no tail yes\n";
            return p.str();
        }();
        in << "# Resonant (coupling/EngineAdapters): elastic constants by +-" << num(task.strain) << " strain deformations, static\n"
           << "# force field " << ff->id << " v" << ff->version << " (" << ff->source << ")\n"
           << "units metal\natom_style atomic\nboundary p p p\nread_data structure.data\n"
           << pairLines << "thermo_style custom step pe pxx pyy pzz pxy pxz pyz\nmin_style cg\n"
           << "fix relax all box/relax iso 0.0\nminimize 1e-15 1e-15 10000 100000\nunfix relax\nwrite_restart relaxed.restart\n";
        // Strains in the mechanics order: xx, yy, zz (scaled box), xy, yz, zx (tilts, engineering shear).
        static const char* dims[3] = {"x", "y", "z"};
        for (int k = 0; k < 6; ++k)
            for (const int sign : {+1, -1}) {
                const double d = sign * task.strain;
                in << "\n# strain " << k << (sign > 0 ? " +" : " -") << "\nclear\nunits metal\natom_style atomic\nboundary p p p\nread_restart relaxed.restart\n"
                   << pairLines << "thermo_style custom step pe pxx pyy pzz pxy pxz pyz\nmin_style cg\n";
                if (k < 3) {
                    in << "change_box all " << dims[k] << " scale " << num(1 + d) << " remap\n";
                } else {
                    in << "change_box all triclinic\n";
                    if (k == 3)
                        in << "change_box all xy final $(" << num(d) << "*ly) remap units box\n";
                    if (k == 4)
                        in << "change_box all yz final $(" << num(d) << "*lz) remap units box\n";
                    if (k == 5)
                        in << "change_box all xz final $(" << num(d) << "*lz) remap units box\n";
                }
                in << "minimize 1e-15 1e-15 10000 100000\nprint \"RESONANT-STRESS " << k << " " << (sign > 0 ? "+" : "-")
                   << " $(pxx) $(pyy) $(pzz) $(pxy) $(pxz) $(pyz)\"\n";
            }
        return in.str() + "\n# ---- structure.data ----\n" + data.str();
    }
    Result<std::vector<TransferredProperty>> run(const AtomisticTask& task, const std::string& workDirectory) const override
    {
        using R = Result<std::vector<TransferredProperty>>;
        auto prepared = prepare(task);
        if (!prepared)
            return R::failure(prepared.message());
        if (Status s = available(); !s)
            return R::failure(s.message() + "; the input is prepared but not run, and no result is invented");
        // Not exercised in this build's tests: LAMMPS is not installed on the development machine.
        const std::string& text = prepared.value();
        const auto split = text.find("\n# ---- structure.data ----\n");
        std::error_code ec;
        std::filesystem::create_directories(workDirectory, ec);
        std::ofstream(std::filesystem::path(workDirectory) / "in.resonant") << text.substr(0, split);
        std::ofstream(std::filesystem::path(workDirectory) / "structure.data") << text.substr(split + 28);
        const std::string command = "cd \"" + workDirectory + "\" && \"" + lammpsExecutable() + "\" -in in.resonant -log log.resonant > /dev/null 2>&1";
        if (std::system(command.c_str()) != 0)
            return R::failure("LAMMPS ended with an error (see log.resonant in the work directory)");
        std::ifstream logFile(std::filesystem::path(workDirectory) / "log.resonant");
        std::stringstream log;
        log << logFile.rdbuf();
        auto c = elasticConstantsFromLammpsLog(log.str(), task.strain);
        if (!c)
            return R::failure(c.message());
        TransferredProperty p;
        p.quantity = "elastic-constants";
        p.representation = kStiffnessRepresentation;
        p.values = upper(c.value());
        p.unit = "Pa";
        p.frame = "the cell axes";
        p.composition = task.structure.composition();
        p.conditions = {{"ensemble", "static minimization (0 K)"}, {"strain", task.strain}, {"relaxation", "box/relax iso 0"}};
        p.engine = "lammps";
        const std::string first = log.str().substr(0, log.str().find('\n'));
        p.engineVersion = first;
        p.forceField = task.forceField;
        p.method = "the stresses of +- strain deformations (central differences), Voigt-ordered for the continuum";
        p.inputDigest = digest(text);
        return std::vector<TransferredProperty>{p};
    }
};

// --- GROMACS --------------------------------------------------------------------------------

class GromacsAdapter final : public EngineAdapter {
public:
    EngineApplicability applicability() const override
    {
        return {"gromacs",
                "GROMACS",
                {"biomolecules", "polymers", "liquids and solutions", "soft matter"},
                {"density", "diffusion", "viscosity"},
                {"elastic constants of crystalline metals and ceramics", "defect and interface energies of solids", "electronic structure"},
                "declared applicability only: topologies and run parameters are not generated in this build"};
    }
    Status available() const override
    {
        if (multiscale::findExecutable("gmx").empty() && multiscale::findExecutable("gmx_mpi").empty())
            return Status::failure("GROMACS is not installed here: neither gmx nor gmx_mpi is on PATH");
        return Status::success();
    }
    Result<std::string> prepare(const AtomisticTask& task) const override
    {
        if (task.property == "elastic-constants" || task.property == "cohesive-energy")
            return Result<std::string>::failure("GROMACS is not the engine for '" + task.property + "' of crystalline solids (outside its applicability)");
        return Result<std::string>::failure("the GROMACS adapter declares its applicability only: input generation is not implemented");
    }
    Result<std::vector<TransferredProperty>> run(const AtomisticTask& task, const std::string&) const override
    {
        auto p = prepare(task);
        return Result<std::vector<TransferredProperty>>::failure(p.message());
    }
};

}  // namespace

json EngineApplicability::toJson() const
{
    return {{"engine", engine}, {"title", title}, {"materialClasses", materialClasses}, {"properties", properties}, {"notFor", notFor}, {"note", note}};
}

json TransferredProperty::toJson() const
{
    return {{"quantity", quantity},       {"representation", representation}, {"values", values},           {"uncertainty", uncertainty},
            {"uncertaintyKind", uncertaintyKind}, {"unit", unit},             {"frame", frame},             {"temperature", temperature},
            {"pressure", pressure},       {"composition", composition},        {"conditions", conditions},   {"engine", engine},
            {"engineVersion", engineVersion}, {"forceField", forceField},      {"method", method},           {"inputDigest", inputDigest},
            {"provenance", provenance},   {"warnings", warnings}};
}

Result<materials::MaterialField> TransferredProperty::toMaterialField(const std::string& id, const std::string& region) const
{
    using R = Result<materials::MaterialField>;
    materials::MaterialField f;
    f.id = id;
    f.region = region;
    f.source = "computed";
    f.profile = "constant";
    f.value0 = values;
    f.unit = unit;
    f.provenance = engine + " " + engineVersion + ", " + forceField + ", " + method + ", T = " + num(temperature) + " K, input " + inputDigest +
                   " (the tensor's axes, " + frame + ", taken as the region's axes)";
    if (quantity == "elastic-constants") {
        f.property = "mechanical.stiffness";
        f.rank = materials::FieldRank::Stiffness6;
    } else if (quantity == "density") {
        f.property = "mechanical.density";
        f.rank = materials::FieldRank::Scalar;
    } else {
        return R::failure("a " + quantity + " is not a material field of the continuum");
    }
    if (Status s = f.validate(); !s)
        return R::failure(s.message());
    return f;
}

std::vector<std::string> engineAdapterIds()
{
    return {"resonant.lattice-statics", "lammps", "gromacs"};
}

std::unique_ptr<EngineAdapter> engineAdapter(const std::string& id)
{
    if (id == "resonant.lattice-statics")
        return std::make_unique<LatticeStaticsAdapter>();
    if (id == "lammps")
        return std::make_unique<LammpsAdapter>();
    if (id == "gromacs")
        return std::make_unique<GromacsAdapter>();
    return nullptr;
}

Result<Eigen::Matrix<double, 6, 6>> elasticConstantsFromLammpsLog(const std::string& log, double strain)
{
    using R = Result<Eigen::Matrix<double, 6, 6>>;
    if (!(strain > 0.0))
        return R::failure("the strain amplitude must be positive");
    // LAMMPS pressure components pxx pyy pzz pxy pxz pyz to the mechanics order (xx yy zz xy yz zx).
    static constexpr int to[6] = {0, 1, 2, 3, 5, 4};
    Eigen::Matrix<double, 6, 6> plus = Eigen::Matrix<double, 6, 6>::Constant(std::nan("")), minus = plus;
    std::istringstream in(log);
    std::string line;
    while (std::getline(in, line)) {
        const auto at = line.find("RESONANT-STRESS ");
        if (at == std::string::npos || line.find("print") != std::string::npos)
            continue;  // the echoed command, not its output
        std::istringstream ls(line.substr(at + 16));
        int k = -1;
        std::string sign;
        double p[6];
        if (!(ls >> k >> sign >> p[0] >> p[1] >> p[2] >> p[3] >> p[4] >> p[5]) || k < 0 || k > 5 || (sign != "+" && sign != "-"))
            return R::failure("a RESONANT-STRESS line could not be read: " + line);
        for (int i = 0; i < 6; ++i)
            (sign == "+" ? plus : minus)(to[i], k) = -p[i] * 1e5;  // stress = -pressure, bar to Pa
    }
    if (!plus.allFinite() || !minus.allFinite())
        return R::failure("the log does not hold the stresses of all twelve deformations");
    Eigen::Matrix<double, 6, 6> c = (plus - minus) / (2 * strain);
    return Eigen::Matrix<double, 6, 6>(0.5 * (c + c.transpose()));
}

}  // namespace resonant::coupling
