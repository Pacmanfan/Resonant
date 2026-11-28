#include "multiscale/ExternalSolvers.h"

#include <cstdlib>
#include <filesystem>
#include <sstream>

namespace resonant::multiscale {

namespace {

std::string findOnPath(const std::string& name)
{
    const char* path = std::getenv("PATH");
    if (!path)
        return {};
    std::stringstream ss(path);
    std::string dir;
    while (std::getline(ss, dir, ':')) {
        if (dir.empty())
            continue;
        std::error_code ec;
        const std::filesystem::path candidate = std::filesystem::path(dir) / name;
        if (std::filesystem::is_regular_file(candidate, ec))
            return candidate.string();
    }
    return {};
}

}  // namespace

std::string findExecutable(const std::string& name)
{
    return findOnPath(name);
}

nlohmann::json ExternalSolver::toJson() const
{
    // The path is reported as found / not found only: a machine path is not part of a report.
    return {{"id", id},           {"name", name},       {"scale", scale},   {"license", license},
            {"executables", executables}, {"pythonModule", pythonModule}, {"provides", provides}, {"install", install},
            {"status", status},   {"found", found}};
}

std::vector<ExternalSolver> externalSolvers()
{
    std::vector<ExternalSolver> list{
        {"lammps", "LAMMPS", "atomistic / molecular", "GPL-2.0", {"lmp", "lmp_serial", "lmp_mpi"}, "lammps",
         {"molecular dynamics", "many force fields", "elastic constants", "thermal conductivity (Green-Kubo, NEMD)"},
         "a distribution package (e.g. apt install lammps; administrator rights) or a conda environment"},
        {"gromacs", "GROMACS", "molecular", "LGPL-2.1", {"gmx", "gmx_mpi"}, "",
         {"molecular dynamics of biomolecules, polymers and liquids", "densities, diffusion and transport of soft matter"},
         "a distribution package (e.g. apt install gromacs; administrator rights)"},
        {"openmm", "OpenMM", "molecular", "MIT / LGPL", {}, "openmm",
         {"molecular dynamics of biomolecular and organic systems", "GPU acceleration"}, "pip or conda (openmm)"},
        {"quantum-espresso", "Quantum ESPRESSO", "electronic", "GPL-2.0", {"pw.x"}, "",
         {"plane-wave DFT", "band structures", "phonons (ph.x)", "dielectric response"}, "a distribution package (apt install quantum-espresso)"},
        {"cp2k", "CP2K", "electronic / atomistic", "GPL-2.0", {"cp2k", "cp2k.psmp", "cp2k.ssmp"}, "",
         {"DFT (Gaussian and plane waves)", "ab initio molecular dynamics"}, "a distribution package (apt install cp2k)"},
        {"gpaw", "GPAW", "electronic", "GPL-3.0", {"gpaw"}, "gpaw", {"real-space and plane-wave DFT through ASE"}, "pip (gpaw) with libxc"},
        {"phonopy", "phonopy", "atomistic", "BSD-3-Clause", {"phonopy"}, "phonopy",
         {"phonon dispersion and density of states", "heat capacity", "quasi-harmonic thermal expansion"}, "pip (phonopy), with a force calculator"},
    };
    // The adapters of coupling/EngineAdapters (REQ-CHAT-20261128-006).
    for (ExternalSolver& s : list) {
        if (s.id == "lammps")
            s.status = "not integrated in the analyses: an adapter (coupling/EngineAdapters) prepares its inputs, parses its output and runs it only when found";
        if (s.id == "gromacs")
            s.status = "not integrated: an adapter (coupling/EngineAdapters) declares its applicability only, without input generation";
    }
    for (ExternalSolver& s : list)
        for (const std::string& exe : s.executables) {
            const std::string where = findOnPath(exe);
            if (!where.empty()) {
                s.found = true;
                s.foundPath = where;
                if (s.id != "lammps" && s.id != "gromacs")
                    s.status = "found on PATH, not integrated (no adapter in this build)";
                break;
            }
        }
    return list;
}

}  // namespace resonant::multiscale
