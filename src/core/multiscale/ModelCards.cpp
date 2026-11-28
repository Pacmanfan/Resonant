#include "multiscale/ModelCards.h"

namespace resonant::multiscale {

nlohmann::json ModelCard::toJson() const
{
    return {{"id", id},         {"title", title},         {"system", system},     {"scale", scale},
            {"assumptions", assumptions}, {"methods", methods}, {"parameters", parameters}, {"excluded", excluded},
            {"validity", validity}, {"interpretation", interpretation}};
}

const std::vector<ModelCard>& modelCards()
{
    static const std::vector<ModelCard> cards{
        {"representation.continuum-material", "Continuum material", "a material as a set of engineering properties", "continuum",
         {"properties are uniform within a region", "the library holds isotropic values with frequency tables"},
         {"the domains' constitutive laws (Hooke, Fourier, Ohm, linear dielectrics)"},
         {"the domain's property set"},
         {"anisotropic tensors in the library", "history dependence"},
         "lengths much larger than the material's internal structure",
         "the values the solvers use; their provenance and derivation say where they came from"},
        {"representation.effective-material", "Effective material", "the averaged behavior of an organized structure", "effective",
         {"scale separation between the structure and the fields"},
         {"homogenization or retrieval (see the derivation's method)"},
         {"the structure, its constituents and the method"},
         {"field gradients on the scale of the structure", "boundary layers of finite samples"},
         "inside the validity range the method states",
         "a description of the structure, not a new substance: its derivation leads back to the structure"},
        {"representation.rve.voxel", "Voxel representative volume element", "a box of material phases on a regular voxel grid", "microstructure",
         {"each voxel holds one phase (and one grain orientation)", "phases are continua with their library properties",
          "perfect contact between phases unless an interface phase is modeled"},
         {"procedural generation (seeded) or imported voxel data"},
         {"grid size and voxel count", "phases and their materials", "for grains: the orientations of a uniaxial constituent"},
         {"interfaces thinner than a voxel", "features smaller than a voxel", "grain-boundary resistance unless modeled as a phase"},
         "features resolved by several voxels; an RVE large against the features (checked by size studies)",
         "synthetic RVEs are not measured microstructures; their statistics are those of the generator"},
        {"representation.microstructure", "Microstructure description", "the organization of a material: grains, phases, inclusions, pores", "microstructure",
         {"a statistical or geometric description"}, {"generator parameters or imported data"}, {"phases, fractions, sizes, orientations"}, {},
         "as stated by its source", "a description; an RVE instantiates it"},
        {"representation.crystal", "Crystal", "a periodic arrangement of atoms: lattice vectors and a basis", "atomistic",
         {"ideal, defect-free unless vacancies or substitutions are listed", "classical point atoms (no zero-point motion)"},
         {"Bravais lattice and basis in fractional coordinates; supercells by repetition"},
         {"lattice vectors (m), species, fractional positions"},
         {"electrons (unless an electronic-structure method is applied)", "temperature (static structures)"},
         "the force field's applicability",
         "a structural model; properties come from a calculation on it"},
        {"representation.atomic-structure", "Atomic structure", "atoms with positions, periodic or not (imported XYZ / POSCAR)", "atomistic",
         {"positions as given by the source"}, {"import with validation"}, {"species, positions, optional cell"},
         {"missing information is not invented: a plain XYZ file has no cell"}, "as stated by its source", "a structural model"},
        {"representation.molecule", "Molecule", "atoms, bonds, angles, dihedrals, charges", "molecular",
         {"a topology and a geometry, kept apart from interaction parameters and simulation conditions"},
         {"none built in: the interface for molecular engines"}, {"topology, geometry, force field"},
         {"no molecular calculation is integrated in this build"}, "-", "a data model for external molecular engines"},
        {"representation.molecular-system", "Molecular system", "molecules in a periodic cell with a force field and conditions", "molecular",
         {"as above"}, {"none built in"}, {"cell, composition, force field, ensemble"}, {"no molecular calculation is integrated"}, "-",
         "a data model for external molecular engines"},
        {"representation.electronic-structure", "Electronic structure", "electrons of an atomic structure: functional, basis, k-points, pseudopotentials",
         "electronic",
         {"density functional theory or another electronic-structure method of an external solver"},
         {"none integrated: no electronic-structure solver is available in this build"},
         {"atomic structure, exchange-correlation functional, cutoff or basis, k-point grid, pseudopotentials, spin, smearing, convergence thresholds"},
         {"every calculation: results exist only from an integrated, validated solver"},
         "-",
         "an interface: Resonant does not compute electronic structure and shows no such result"},
        {"rve.potential-fe", "Computational homogenization of a steady potential problem",
         "a voxel RVE with per-voxel conductivity tensors: heat conduction, electric conduction or electrostatics", "microstructure -> effective",
         {"steady state, linear constituents", "scale separation: the macroscopic gradient is uniform over the RVE",
          "periodic boundaries only for periodic RVEs"},
         {"div(K grad u) = 0 with u = G.x + periodic fluctuation (periodic), u = G.x on the boundary (linear), or u = G.x on the two faces normal to G "
          "and no flux elsewhere (mixed)",
          "trilinear hexahedral finite elements, 2x2x2 Gauss points, conjugate gradients", "K_eff G = <K grad u>; Hill-Mandel: <grad u . K grad u> = G . K_eff G"},
         {"the RVE", "each phase's conductivity (tensor for oriented grains)", "the boundary condition"},
         {"interface resistance unless modeled as a phase", "radiation and convection inside pores", "nonlinear (temperature-dependent) constituents within one solve"},
         "an RVE large against its features (size study) and features resolved by several voxels",
         "periodic results lie between the mixed (lower) and linear (upper) apparent values; their spread over realizations is the uncertainty"},
        {"atomistic.lj.lattice-statics", "Lennard-Jones lattice statics", "a perfect crystal at 0 K with a 12-6 pair potential", "atomistic -> continuum",
         {"classical atoms, no zero-point energy", "a pair potential (central forces): Cauchy relations hold",
          "homogeneous strain without internal relaxation (exact for Bravais lattices)"},
         {"E = sum over pairs 4 eps ((sigma/r)^12 - (sigma/r)^6) within a cutoff plus the analytic tail of a uniform density",
          "equilibrium by minimizing E over the lattice parameter", "C_ij = (1/V) d2E / d(eps_i) d(eps_j) by central differences (Voigt notation)",
          "Voigt-Reuss-Hill averages for an isotropic polycrystal"},
         {"crystal", "force field (eps, sigma, cutoff)"},
         {"temperature (thermal expansion, anharmonic softening)", "zero-point motion", "many-body forces"},
         "temperatures far below melting; the force field's species",
         "a static, classical estimate: compare with measurement before relying on it"},
        {"atomistic.lj.md-nve", "Lennard-Jones molecular dynamics (NVE)", "atoms in an orthorhombic periodic cell", "atomistic",
         {"classical Newtonian dynamics", "minimum-image pair forces within a cutoff (shifted energy)"},
         {"velocity Verlet", "Maxwell-Boltzmann initial velocities (seeded)", "radial distribution and mean squared displacement"},
         {"structure", "force field", "time step, steps, initial temperature"},
         {"thermostats and barostats", "long-range corrections to forces"},
         "time steps small against the fastest vibration",
         "energy drift and its time-step scaling tell whether the run is reliable; one short run is not a converged average"},
        {"coupling.iterative-rve", "Iterative region-RVE coupling", "a macroscopic region whose material is re-homogenized at its local state",
         "continuum <-> microstructure",
         {"the region's state is represented by its mean temperature", "constituent laws linear in temperature"},
         {"fixed point: run the coupled engine, extract the region's temperature, homogenize the RVE with the constituents at that temperature, "
          "update the region's conductivity, repeat until it changes less than the tolerance"},
         {"physical system, region, RVE, constituent laws, tolerance"},
         {"temperature variation inside the region (one state per region)", "FE2 (one RVE per integration point)"},
         "regions whose temperature varies little compared with the constituents' laws",
         "the converged conductivity is consistent at both scales; its history shows the convergence"},
    };
    return cards;
}

const ModelCard* findModelCard(std::string_view id)
{
    for (const ModelCard& c : modelCards())
        if (c.id == id)
            return &c;
    return nullptr;
}

}  // namespace resonant::multiscale
