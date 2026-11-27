// The example material library.
//
// IMPORTANT: every number in this file is an ILLUSTRATIVE nominal value: a
// rounded, room-temperature figure of the kind quoted in general physics and
// engineering references. Real materials vary with alloy, grade, moisture,
// temperature, frequency and direction (wood and many composites are
// anisotropic). None of these values is authoritative, and each is stored
// with Provenance::Illustrative and a note saying so. Engineering work must
// replace them with measured, manufacturer-supplied or cited values.
#include "materials/Material.h"

namespace resonant {
namespace {

constexpr const char* kNote =
    "Illustrative nominal room-temperature value (about 293 K); not authoritative. "
    "Replace with a measured or cited value before engineering use.";

PropertyValue illustrative(double value, const char* extra = nullptr)
{
    PropertyValue property;
    property.value = value;
    property.provenance = Provenance::Illustrative;
    property.source = kNote;
    if (extra) {
        property.source += ' ';
        property.source += extra;
    }
    return property;
}

struct Acoustic {
    double density;
    double soundSpeed;
    const char* note = nullptr;
};
struct Mechanical {
    double density;
    double youngsModulus;
    double poissonsRatio;
    const char* note = nullptr;
};
struct Electromagnetic {
    double relativePermittivity;
    double relativePermeability;
    double conductivity;
    const char* note = nullptr;
};

// Fixed ids keep references stable between sessions and across projects
// that start from this library.
Material make(const char* id, const char* name, const char* description)
{
    Material material(Id(id), name);
    material.setDescription(description);
    return material;
}

void set(Material& material, PhysicsDomain domain, const char* key, PropertyValue value)
{
    [[maybe_unused]] const Status status = material.setProperty(domain, key, std::move(value));
}

void add(Material& material, const Acoustic& a)
{
    set(material, PhysicsDomain::Acoustic, "density", illustrative(a.density, a.note));
    set(material, PhysicsDomain::Acoustic, "soundSpeed", illustrative(a.soundSpeed, a.note));
    set(material, PhysicsDomain::Acoustic, "referenceTemperature", illustrative(293.15));
}

void add(Material& material, const Mechanical& m)
{
    set(material, PhysicsDomain::Mechanical, "density", illustrative(m.density, m.note));
    set(material, PhysicsDomain::Mechanical, "youngsModulus", illustrative(m.youngsModulus, m.note));
    set(material, PhysicsDomain::Mechanical, "poissonsRatio", illustrative(m.poissonsRatio, m.note));
}

void add(Material& material, const Electromagnetic& e)
{
    set(material, PhysicsDomain::Electromagnetic, "relativePermittivity", illustrative(e.relativePermittivity, e.note));
    set(material, PhysicsDomain::Electromagnetic, "relativePermeability", illustrative(e.relativePermeability, e.note));
    set(material, PhysicsDomain::Electromagnetic, "conductivity", illustrative(e.conductivity, e.note));
}

}  // namespace

MaterialLibrary createExampleMaterialLibrary()
{
    MaterialLibrary library;
    const auto store = [&library](Material material) {
        [[maybe_unused]] const Status status = library.add(std::move(material));
    };

    const char* solidSound = "Speed of sound is the longitudinal bulk-wave speed; shear and plate waves differ.";

    {
        Material m = make("mat-air", "Air", "Dry air near 20 degrees C at sea-level pressure (example).");
        add(m, Acoustic{1.204, 343.0, "Both depend on temperature, pressure and humidity."});
        add(m, Electromagnetic{1.0006, 1.0, 0.0});
        store(std::move(m));
    }
    {
        Material m = make("mat-water", "Water", "Fresh water near 20 degrees C (example).");
        add(m, Acoustic{998.0, 1482.0, "Sound speed depends on temperature, salinity and pressure."});
        add(m, Electromagnetic{80.0, 1.0, 0.005,
                               "Static permittivity; strongly frequency dependent. Conductivity varies by orders of "
                               "magnitude with dissolved ions."});
        store(std::move(m));
    }
    {
        Material m = make("mat-aluminum", "Aluminum", "Generic aluminum (example); alloys differ.");
        add(m, Acoustic{2700.0, 6320.0, solidSound});
        add(m, Mechanical{2700.0, 69e9, 0.33});
        add(m, Electromagnetic{1.0, 1.0, 3.5e7, "Relative permittivity is not a meaningful quantity for a good conductor; 1 is a placeholder."});
        store(std::move(m));
    }
    {
        Material m = make("mat-steel", "Steel", "Generic carbon steel (example); grades differ widely.");
        add(m, Acoustic{7850.0, 5900.0, solidSound});
        add(m, Mechanical{7850.0, 200e9, 0.29});
        add(m, Electromagnetic{1.0, 100.0, 6.0e6,
                               "Permeability of steel varies from about 1 (austenitic stainless) to thousands and is "
                               "nonlinear; 100 is only a placeholder. Relative permittivity is a placeholder."});
        store(std::move(m));
    }
    {
        Material m = make("mat-copper", "Copper", "Generic copper (example).");
        add(m, Acoustic{8960.0, 4760.0, solidSound});
        add(m, Mechanical{8960.0, 117e9, 0.34});
        add(m, Electromagnetic{1.0, 1.0, 5.8e7, "Relative permittivity is not a meaningful quantity for a good conductor; 1 is a placeholder."});
        store(std::move(m));
    }
    {
        Material m = make("mat-glass", "Glass", "Generic soda-lime glass (example).");
        add(m, Acoustic{2500.0, 5600.0, solidSound});
        add(m, Mechanical{2500.0, 70e9, 0.22});
        add(m, Electromagnetic{6.0, 1.0, 1e-12, "Permittivity depends on composition and frequency."});
        store(std::move(m));
    }
    {
        Material m = make("mat-acrylic", "Acrylic", "Generic cast acrylic, PMMA (example).");
        add(m, Acoustic{1180.0, 2730.0, solidSound});
        add(m, Mechanical{1180.0, 3.2e9, 0.37, "Polymers are viscoelastic: stiffness depends on frequency and temperature."});
        add(m, Electromagnetic{3.0, 1.0, 1e-14, "Permittivity depends on frequency."});
        store(std::move(m));
    }
    {
        Material m = make("mat-concrete", "Concrete", "Generic structural concrete (example); mixes differ widely.");
        add(m, Acoustic{2400.0, 3700.0, solidSound});
        add(m, Mechanical{2400.0, 30e9, 0.2});
        store(std::move(m));
    }
    {
        Material m = make("mat-rubber", "Rubber", "Generic soft rubber (example); compounds differ by orders of magnitude.");
        add(m, Acoustic{1100.0, 1600.0, "Longitudinal bulk-wave speed; rubber is strongly lossy and frequency dependent."});
        add(m, Mechanical{1100.0, 5e6, 0.49, "Stiffness varies by orders of magnitude between compounds and with frequency."});
        store(std::move(m));
    }
    {
        Material m = make("mat-wood", "Wood", "Generic softwood along the grain (example); wood is anisotropic.");
        add(m, Acoustic{500.0, 3300.0, "Along the grain; across the grain the speed is several times lower."});
        add(m, Mechanical{500.0, 10e9, 0.3, "Along the grain; wood is orthotropic and a single modulus is a simplification."});
        store(std::move(m));
    }
    {
        Material m = make("mat-stainless-steel", "Stainless steel", "Generic austenitic stainless steel, 304 type (example).");
        add(m, Acoustic{8000.0, 5790.0, solidSound});
        add(m, Mechanical{8000.0, 193e9, 0.29, "Typical handbook value for annealed austenitic grades; cold work and grade change it."});
        add(m, Electromagnetic{1.0, 1.0, 1.4e6, "Austenitic grades are nearly non-magnetic; cold work raises the permeability. Relative permittivity is a placeholder."});
        store(std::move(m));
    }
    {
        Material m = make("mat-brass", "Brass", "Generic yellow brass (example); compositions differ.");
        add(m, Acoustic{8500.0, 4700.0, solidSound});
        add(m, Mechanical{8500.0, 100e9, 0.34, "Varies with zinc content and temper."});
        add(m, Electromagnetic{1.0, 1.0, 1.5e7, "Relative permittivity is not a meaningful quantity for a good conductor; 1 is a placeholder."});
        store(std::move(m));
    }
    {
        Material m = make("mat-titanium", "Titanium alloy", "Generic Ti-6Al-4V titanium alloy (example).");
        add(m, Acoustic{4430.0, 6100.0, solidSound});
        add(m, Mechanical{4430.0, 114e9, 0.34, "Typical value for annealed Ti-6Al-4V; heat treatment and texture change it."});
        add(m, Electromagnetic{1.0, 1.0, 5.8e5, "Relative permittivity is a placeholder."});
        store(std::move(m));
    }
    {
        Material m = make("mat-abs", "ABS", "Generic acrylonitrile butadiene styrene, moulded (example).");
        add(m, Acoustic{1050.0, 2250.0, solidSound});
        add(m, Mechanical{1050.0, 2.2e9, 0.35, "Polymers are viscoelastic: stiffness depends on frequency, temperature and grade."});
        store(std::move(m));
    }
    {
        Material m = make("mat-pla", "PLA", "Generic polylactic acid, bulk (example).");
        add(m, Acoustic{1240.0, 2200.0, solidSound});
        add(m, Mechanical{1240.0, 3.5e9, 0.36, "Bulk value. 3D-printed parts are porous and anisotropic (layer direction, infill) and much "
                                               "softer than this; polymers are viscoelastic."});
        store(std::move(m));
    }
    {
        Material m = make("mat-nylon", "Nylon", "Generic polyamide 6, dry as moulded (example).");
        add(m, Acoustic{1140.0, 2600.0, solidSound});
        add(m, Mechanical{1140.0, 2.7e9, 0.39, "Dry value: absorbed moisture lowers the modulus by up to half. Viscoelastic."});
        store(std::move(m));
    }
    {
        Material m = make("mat-polycarbonate", "Polycarbonate", "Generic polycarbonate (example).");
        add(m, Acoustic{1200.0, 2270.0, solidSound});
        add(m, Mechanical{1200.0, 2.4e9, 0.37, "Polymers are viscoelastic: stiffness depends on frequency and temperature."});
        add(m, Electromagnetic{2.9, 1.0, 1e-14, "Permittivity depends on frequency."});
        store(std::move(m));
    }
    {
        Material m = make("mat-alumina", "Alumina ceramic", "Generic 96 % alumina ceramic (example).");
        add(m, Acoustic{3720.0, 9800.0, solidSound});
        add(m, Mechanical{3720.0, 300e9, 0.21, "Depends strongly on purity and porosity."});
        add(m, Electromagnetic{9.0, 1.0, 1e-12, "Permittivity depends on purity and frequency."});
        store(std::move(m));
    }
    // Fibre-reinforced composites are not included: they are orthotropic and
    // need a constitutive model this library does not have.
    return library;
}

}  // namespace resonant
