#include "multiphysics/Quantity.h"

#include <algorithm>

namespace resonant::multiphysics {

std::string_view toString(Rank rank)
{
    switch (rank) {
    case Rank::Scalar: return "scalar";
    case Rank::Vector: return "vector";
    case Rank::Tensor: return "tensor";
    }
    return "scalar";
}

std::string_view toString(Representation representation)
{
    return representation == Representation::Complex ? "complex" : "real";
}

std::string_view toString(Meaning meaning)
{
    switch (meaning) {
    case Meaning::Instantaneous: return "instantaneous";
    case Meaning::Peak: return "peak";
    case Meaning::Rms: return "rms";
    case Meaning::TimeAveraged: return "time-averaged";
    case Meaning::Integrated: return "integrated";
    case Meaning::Static: return "static";
    }
    return "static";
}

const std::vector<QuantityDescriptor>& quantityRegistry()
{
    using R = Rank;
    using P = Representation;
    using M = Meaning;
    static const std::vector<QuantityDescriptor> registry = {
        // Energy and power.
        {"energy", "J", R::Scalar, P::Real, M::Integrated, "general", "energy in a region or over an interval", false},
        {"power", "W", R::Scalar, P::Real, M::TimeAveraged, "general", "power through a surface or into a region", false},
        {"energy_density", "J/m^3", R::Scalar, P::Real, M::TimeAveraged, "general", "stored energy per volume", true},
        {"power_density", "W/m^3", R::Scalar, P::Real, M::TimeAveraged, "general", "power per volume", true},
        {"absorbed_power_density", "W/m^3", R::Scalar, P::Real, M::TimeAveraged, "electromagnetic",
         "electromagnetic power converted by dielectric and conduction loss, averaged over a period: 1/2 omega eps0 eps''_eff |E|^2", true},
        {"acoustic_absorbed_power_density", "W/m^3", R::Scalar, P::Real, M::TimeAveraged, "acoustic",
         "acoustic power dissipated by viscous and thermal losses, averaged over a period", true},
        {"heat_source_density", "W/m^3", R::Scalar, P::Real, M::Static, "thermal", "a steady volumetric heat source of the conduction equation", true},
        {"heat_flux", "W/m^2", R::Vector, P::Real, M::Static, "thermal", "conductive heat flux -k grad T", false},
        {"poynting_flux", "W/m^2", R::Vector, P::Real, M::TimeAveraged, "electromagnetic", "time-averaged Poynting vector 1/2 Re(E x H*)", false},
        {"acoustic_intensity", "W/m^2", R::Vector, P::Real, M::TimeAveraged, "acoustic", "time-averaged intensity 1/2 Re(P V*)", false},
        // Fields.
        {"temperature", "K", R::Scalar, P::Real, M::Static, "thermal", "absolute temperature", false},
        {"electric_field", "V/m", R::Vector, P::Complex, M::Peak, "electromagnetic", "electric field phasor, e^{+j omega t}", false},
        {"magnetic_field", "A/m", R::Vector, P::Complex, M::Peak, "electromagnetic", "magnetic field phasor, e^{+j omega t}", false},
        {"electric_flux_density", "C/m^2", R::Vector, P::Complex, M::Peak, "electromagnetic", "electric displacement phasor", false},
        {"magnetic_flux_density", "T", R::Vector, P::Complex, M::Peak, "electromagnetic", "magnetic flux density phasor", false},
        {"current_density", "A/m^2", R::Vector, P::Complex, M::Peak, "electromagnetic", "conduction current density phasor", false},
        {"charge_density", "C/m^3", R::Scalar, P::Real, M::Static, "electromagnetic", "volume charge density", true},
        {"acoustic_pressure", "Pa", R::Scalar, P::Complex, M::Peak, "acoustic", "acoustic pressure phasor, e^{+j omega t}", false},
        {"particle_velocity", "m/s", R::Vector, P::Complex, M::Peak, "acoustic", "acoustic particle velocity phasor", false},
        {"displacement", "m", R::Vector, P::Real, M::Static, "mechanical", "static displacement from the reference configuration", false},
        {"velocity", "m/s", R::Vector, P::Complex, M::Peak, "mechanical", "structural velocity phasor", false},
        {"surface_normal_velocity", "m/s", R::Scalar, P::Complex, M::Peak, "mechanical", "normal velocity phasor of a vibrating surface", false},
        {"strain", "1", R::Tensor, P::Real, M::Static, "mechanical", "small strain (xx, yy, zz, xy, yz, zx; engineering shear)", false},
        {"thermal_strain", "1", R::Scalar, P::Real, M::Static, "mechanical", "free thermal strain alpha (T - T_ref)", false},
        {"stress", "Pa", R::Tensor, P::Real, M::Static, "mechanical", "Cauchy stress (xx, yy, zz, xy, yz, zx)", false},
        {"pressure", "Pa", R::Scalar, P::Real, M::Static, "general", "static pressure", false},
        {"force_density", "N/m^3", R::Vector, P::Real, M::TimeAveraged, "general", "body force per volume (radiation, Lorentz)", true},
        {"surface_traction", "N/m^2", R::Vector, P::Real, M::TimeAveraged, "general", "force per area on a surface", true},
        {"momentum_density", "kg/(m^2 s)", R::Vector, P::Real, M::TimeAveraged, "general", "momentum per volume", true},
        // Material state and properties.
        {"mass_density", "kg/m^3", R::Scalar, P::Real, M::Static, "general", "mass per volume", true},
        {"relative_permittivity", "1", R::Scalar, P::Complex, M::Static, "electromagnetic", "eps' - j eps''", false},
        {"electrical_conductivity", "S/m", R::Scalar, P::Real, M::Static, "electromagnetic", "conductivity", false},
        {"thermal_conductivity", "W/(m K)", R::Scalar, P::Real, M::Static, "thermal", "thermal conductivity", false},
        {"damage", "1", R::Scalar, P::Real, M::Static, "mechanical", "damage variable 0 (intact) .. 1 (failed)", false},
        {"phase_fraction", "1", R::Scalar, P::Real, M::Static, "general", "volume fraction of a phase", false},
    };
    return registry;
}

const QuantityDescriptor* findQuantity(std::string_view id)
{
    const auto& r = quantityRegistry();
    const auto it = std::find_if(r.begin(), r.end(), [id](const QuantityDescriptor& q) { return q.id == id; });
    return it == r.end() ? nullptr : &*it;
}

Status compatible(const QuantityDescriptor& a, const QuantityDescriptor& b)
{
    if (a.id != b.id)
        return Status::failure("different quantities: " + a.id + " and " + b.id);
    if (a.unit != b.unit)
        return Status::failure(a.id + ": units differ (" + a.unit + ", " + b.unit + ")");
    if (a.rank != b.rank)
        return Status::failure(a.id + ": ranks differ");
    if (a.representation != b.representation)
        return Status::failure(a.id + ": one is real, the other complex");
    if (a.meaning != b.meaning)
        return Status::failure(a.id + ": " + std::string(toString(a.meaning)) + " and " + std::string(toString(b.meaning)) + " values differ in meaning");
    return Status::success();
}

}  // namespace resonant::multiphysics
