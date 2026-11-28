#include "multiscale/Couplings.h"

namespace resonant::multiscale {

nlohmann::json CouplingContract::toJson() const
{
    return {{"id", id},
            {"name", name},
            {"strategy", strategy},
            {"status", status},
            {"sourceScale", sourceScale},
            {"targetScale", targetScale},
            {"transferred", transferred},
            {"coordinates", coordinates},
            {"assumptions", assumptions},
            {"boundaryConditions", boundaryConditions},
            {"updateFrequency", updateFrequency},
            {"convergence", convergence},
            {"failureHandling", failureHandling},
            {"uncertainty", uncertainty},
            {"provenance", provenance},
            {"requirements", requirements}};
}

const std::vector<CouplingContract>& couplingContracts()
{
    static const std::vector<CouplingContract> contracts{
        {"sequential.atomistic-continuum", "Atomistic to continuum (elastic constants)", "sequential", "implemented", "atomistic", "continuum",
         {"elastic constants C11, C12, C44", "density", "cohesive energy and lattice parameter (reported)"},
         "crystal axes to an isotropic polycrystal (Voigt-Reuss-Hill)", "0 K, pair potential, homogeneous strain", "free crystal (periodic)",
         "once per derivation", "lattice parameter minimized to a relative 1e-10; cutoff sensitivity reported",
         "refused for structures without a cell or species outside the force field's applicability (unless allowed)",
         "the cutoff sensitivity is the stated uncertainty; model error is named in the card", "a derivation record per assignment"},
        {"sequential.rve-effective", "RVE to effective material", "sequential", "implemented", "microstructure", "effective / continuum",
         {"conductivity tensors (thermal, electrical, static permittivity)", "density, specific heat (volume and mass averages)"},
         "RVE axes are the material's axes", "steady, linear constituents, scale separation", "periodic, linear or mixed",
         "once per derivation", "conjugate gradients to a relative residual 1e-10; RVE size and realizations studied",
         "refused when a phase lacks the property; periodic conditions refused for RVEs not marked periodic",
         "standard deviation over realizations", "a derivation record per assignment"},
        {"iterative.region-rve-thermal", "Region temperature and RVE conductivity", "iterative", "implemented", "continuum", "microstructure",
         {"region mean temperature (down)", "effective conductivity at that temperature (up)"},
         "the region's mean over its cells", "one state per region; linear constituent laws", "the coupled engine's own",
         "every iteration", "relative change of the conductivity below the tolerance",
         "stops at the iteration limit with the history, reported as not converged",
         "the RVE's spread carried to the converged value", "the iteration history in the result"},
        {"hierarchical.chain", "Chains of sequential couplings", "hierarchical", "implemented", "any", "any",
         {"each step's derived properties become the next step's constituents"},
         "-", "each step's own", "each step's own", "once per step, in order", "each step's own",
         "a step that is stale marks the steps above it stale", "each step's uncertainty is kept with its value (not yet propagated through the steps)",
         "lineage navigable from the top material to the lowest representation"},
        {"fe2", "FE2 (an RVE at every integration point)", "concurrent", "planned", "continuum", "microstructure",
         {"strain or gradient at each integration point (down)", "stress or flux and tangent (up)"},
         "", "", "", "every Newton iteration", "", "", "", "",
         {"a macroscopic nonlinear finite-element solver with integration-point state", "an RVE solve per point (thousands per step): parallel jobs",
          "consistent tangents from the RVE", "caching of RVE responses"}},
        {"concurrent.atomistic-continuum", "Concurrent atomistic-continuum", "concurrent", "planned", "atomistic", "continuum",
         {"displacements and forces across a handshake region"}, "", "", "", "every time step", "", "", "", "",
         {"an integrated molecular-dynamics engine", "a dynamic continuum solver", "a handshake method (e.g. bridging domain) with spurious-reflection control"}},
    };
    return contracts;
}

}  // namespace resonant::multiscale
