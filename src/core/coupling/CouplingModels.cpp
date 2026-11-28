#include "coupling/CouplingModels.h"

namespace resonant::coupling {

const std::vector<multiphysics::CouplingModelDescriptor>& couplingModels()
{
    static const std::vector<multiphysics::CouplingModelDescriptor> models = {
        {"em.absorption-to-heat", "Electromagnetic absorption to heat", "electromagnetic", "thermal", "field-transfer", "absorbed_power_density",
         "heat_source_density", "conservative", "dielectric and conduction loss heats the material",
         "q = 1/2 omega eps0 eps''_eff |E|^2, eps''_eff = eps' tan(delta) + sigma / (omega eps0), averaged over a period, becomes the steady "
         "source of rho c_p dT/dt = div(k grad T) + q (the wave period is far shorter than the thermal time)",
         "validated", "CoupledValidation.MicrowaveAbsorptionHeatsASlab", "assumes the slow thermal time scale; the layered plane-wave model (normal incidence)"},
        {"thermal.temperature-to-em-properties", "Temperature-dependent electromagnetic properties", "thermal", "electromagnetic", "state-update",
         "temperature", "temperature", "interpolating", "temperature changes permittivity, loss tangent and conductivity",
         "eps'(T), tan(delta)(T), sigma(T) from the region's material laws, evaluated at the temperature of each sublayer",
         "validated", "CoupledValidation.TemperatureDependentLossConvergesToTheLumpedFixedPoint", "laws must be supplied; extrapolation beyond their validity is reported"},
        {"thermal.temperature-to-thermal-properties", "Temperature-dependent thermal properties", "thermal", "thermal", "state-update", "temperature",
         "temperature", "interpolating", "temperature changes conductivity and heat capacity",
         "k(T), c_p(T) from the region's material laws, evaluated per cell; the conduction equation is re-solved with them",
         "experimental", "Multiphysics.MaterialStateLaws", "Picard iteration of the nonlinear conduction problem; no Newton acceleration"},
        {"thermal.expansion", "Thermal expansion and stress", "thermal", "mechanical", "field-transfer", "temperature", "temperature", "interpolating",
         "heating strains the material; constrained strain is stress",
         "eps_th = alpha (T - T_ref) I, sigma = D (eps - eps_th), linear thermoelasticity in the reference configuration",
         "validated", "ThermalLoads.*, CoupledValidation.UniformHeatingExpandsAndStressesTheColumn", "small strain; uncoupled (the mechanical work does not change the temperature)"},
        {"thermal.temperature-to-mechanical-properties", "Temperature-dependent elastic properties", "thermal", "mechanical", "state-update",
         "temperature", "temperature", "interpolating", "temperature changes Young's modulus and Poisson's ratio",
         "E(T), nu(T) from the region's material laws, evaluated at each element's temperature",
         "validated", "CoupledValidation.TemperatureDependentModulusScalesTheThermalStress", "laws must be supplied"},
        {"mechanics.deformation-to-geometry", "Deformation changes the geometry", "mechanical", "electromagnetic", "geometry-update", "displacement",
         "displacement", "interpolating", "the deformed layer thicknesses become the current geometry of the wave solution",
         "d_i = d_i,ref + u_z(top of i) - u_z(bottom of i); the electromagnetic solution is rebuilt for the new geometry revision",
         "validated", "CoupledValidation.ExpansionFeedsBackIntoTheElectromagneticSolution", "layer thicknesses only (layered-slab geometry); no remeshing of general shapes"},
        {"mechanics.structure-to-acoustic.rayleigh", "Vibrating plate radiates sound", "mechanical", "acoustic", "field-transfer", "surface_normal_velocity",
         "acoustic_pressure", "interpolating", "a vibrating plate drives the surrounding fluid",
         "p(x) = (j omega rho / 2 pi) sum_e v_e A_e exp(-j k R_e) / R_e (Rayleigh integral, infinite rigid baffle)",
         "external", "StructuralRadiation tests (REQ-CHAT-20261126-006)",
         "implemented one-way in the mechanics panel (coupling/StructuralRadiation); not yet a participant pair of the engine"},
        {"acoustic.pressure-to-structure", "Acoustic pressure loads a structure", "acoustic", "mechanical", "field-transfer", "acoustic_pressure",
         "surface_traction", "conservative", "sound pressure acting on a structure's surface",
         "t = -p n on the wetted surface (harmonic, e^{+j omega t}); two-way with the fluid's added mass and radiation damping",
         "planned", "", "not implemented"},
        {"acoustic.absorption-to-heat", "Acoustic absorption to heat", "acoustic", "thermal", "field-transfer", "acoustic_absorbed_power_density",
         "heat_source_density", "conservative", "viscous and thermal losses heat the medium or an absorber",
         "q = 2 alpha I for a plane wave with amplitude attenuation alpha (Np/m); volumetric loss models in general",
         "planned", "", "not implemented"},
        {"thermal.temperature-to-acoustic-properties", "Temperature-dependent acoustic properties", "thermal", "acoustic", "state-update", "temperature",
         "temperature", "interpolating", "temperature changes the speed of sound and the density",
         "c(T), rho(T) from material laws (ideal gas: c = sqrt(gamma R T / M))", "planned", "", "not implemented"},
        {"em.lorentz-force", "Lorentz force", "electromagnetic", "mechanical", "field-transfer", "current_density", "force_density", "conservative",
         "currents in a magnetic field push the conductor",
         "f = rho E + J x B; time-averaged 1/2 Re(J x B*) for harmonic fields", "planned", "", "not implemented"},
        {"em.radiation-pressure", "Electromagnetic radiation pressure", "electromagnetic", "mechanical", "field-transfer", "poynting_flux",
         "surface_traction", "conservative", "the momentum flux of a wave pushes the surface it meets",
         "F = - closed integral of T . n dA, Maxwell stress T = eps0 E E + mu0^-1 B B - 1/2 (eps0 |E|^2 + mu0^-1 |B|^2) I",
         "planned", "", "not implemented"},
        {"em.magnetic-loss-to-heat", "Magnetic loss to heat", "electromagnetic", "thermal", "field-transfer", "absorbed_power_density",
         "heat_source_density", "conservative", "magnetic hysteresis and relaxation losses",
         "q = 1/2 omega mu0 mu'' |H|^2", "planned", "", "not implemented (the layered model has real permeability)"},
        {"em.piezoelectric", "Piezoelectric coupling", "electromagnetic", "mechanical", "field-transfer", "electric_field", "stress", "interpolating",
         "electric fields strain a piezoelectric material and strain polarizes it",
         "S = s^E T + d^t E, D = d T + eps^T E (strain-charge form)", "planned", "", "not implemented"},
        {"fluid.structure-interaction", "Fluid-structure interaction", "fluid", "mechanical", "field-transfer", "pressure", "surface_traction", "conservative",
         "flow loads a structure whose motion changes the flow",
         "Navier-Stokes with moving boundaries; traction continuity and velocity continuity at the interface", "planned", "",
         "not implemented: Resonant has no fluid-dynamics domain"},
    };
    return models;
}

const std::vector<std::pair<std::string, std::string>>& participantSolvers()
{
    static const std::vector<std::pair<std::string, std::string>> solvers = {
        {"electromagnetic", "em.layered.transfer-matrix"},
        {"thermal", "thermal.conduction.fv"},
        {"mechanical", "mechanics.fe.solid-column"},
    };
    return solvers;
}

}  // namespace resonant::coupling
