#include "materials/Material.h"

#include <gtest/gtest.h>

using namespace resonant;

namespace {
PropertyValue measured(double value)
{
    PropertyValue property;
    property.value = value;
    property.provenance = Provenance::Measured;
    property.source = "test";
    return property;
}
}  // namespace

TEST(Material, HoldsSeparatePropertySetsPerDomain)
{
    Material material(Id::generate(), "Test");
    EXPECT_TRUE(material.domains().empty());

    ASSERT_TRUE(material.setProperty(PhysicsDomain::Acoustic, "soundSpeed", measured(1500.0)).ok());
    ASSERT_TRUE(material.setProperty(PhysicsDomain::Electromagnetic, "conductivity", measured(1e7)).ok());

    EXPECT_TRUE(material.appliesTo(PhysicsDomain::Acoustic));
    EXPECT_TRUE(material.appliesTo(PhysicsDomain::Electromagnetic));
    EXPECT_FALSE(material.appliesTo(PhysicsDomain::Mechanical));
    EXPECT_EQ(material.property(PhysicsDomain::Acoustic, "soundSpeed")->value, 1500.0);
    // A property of one domain does not exist in another.
    EXPECT_EQ(material.property(PhysicsDomain::Acoustic, "conductivity"), nullptr);
    EXPECT_FALSE(material.setProperty(PhysicsDomain::Acoustic, "conductivity", measured(1.0)).ok());
    EXPECT_FALSE(material.setProperty(PhysicsDomain::Mechanical, "soundSpeed", measured(1.0)).ok());
}

TEST(Material, RejectsUnphysicalValues)
{
    Material material(Id::generate(), "Test");
    EXPECT_FALSE(material.setProperty(PhysicsDomain::Acoustic, "density", measured(0.0)).ok());
    EXPECT_FALSE(material.setProperty(PhysicsDomain::Acoustic, "density", measured(-5.0)).ok());
    EXPECT_FALSE(material.setProperty(PhysicsDomain::Acoustic, "soundSpeed", measured(std::nan(""))).ok());
    EXPECT_TRUE(material.setProperty(PhysicsDomain::Acoustic, "attenuation", measured(0.0)).ok());
    EXPECT_FALSE(material.setProperty(PhysicsDomain::Acoustic, "attenuation", measured(-0.1)).ok());
    EXPECT_FALSE(material.setProperty(PhysicsDomain::Mechanical, "poissonsRatio", measured(0.6)).ok());
    EXPECT_TRUE(material.setProperty(PhysicsDomain::Mechanical, "poissonsRatio", measured(0.3)).ok());
}

TEST(Material, ValidationRequiresIdAndName)
{
    EXPECT_FALSE(Material(Id(), "x").validate().ok());
    EXPECT_FALSE(Material(Id::generate(), "").validate().ok());
    EXPECT_TRUE(Material(Id::generate(), "x").validate().ok());
}

TEST(Material, FrequencyTableInterpolatesAndClamps)
{
    PropertyValue property = measured(1.0);
    property.frequencyTable = {{100.0, 1.0}, {200.0, 3.0}, {400.0, 4.0}};
    EXPECT_DOUBLE_EQ(property.valueAt(50.0), 1.0);
    EXPECT_DOUBLE_EQ(property.valueAt(150.0), 2.0);
    EXPECT_DOUBLE_EQ(property.valueAt(300.0), 3.5);
    EXPECT_DOUBLE_EQ(property.valueAt(1000.0), 4.0);
    EXPECT_DOUBLE_EQ(measured(7.0).valueAt(123.0), 7.0);

    Material material(Id::generate(), "Test");
    PropertyValue unordered = measured(1.0);
    unordered.frequencyTable = {{200.0, 1.0}, {100.0, 2.0}};
    EXPECT_FALSE(material.setProperty(PhysicsDomain::Acoustic, "attenuation", unordered).ok());
}

TEST(Material, ValidityRangeMustNotBeInverted)
{
    Material material(Id::generate(), "Test");
    PropertyValue property = measured(343.0);
    property.validity.minFrequencyHz = 20000.0;
    property.validity.maxFrequencyHz = 20.0;
    EXPECT_FALSE(material.setProperty(PhysicsDomain::Acoustic, "soundSpeed", property).ok());
    property.validity.minFrequencyHz = 20.0;
    property.validity.maxFrequencyHz = 20000.0;
    EXPECT_TRUE(material.setProperty(PhysicsDomain::Acoustic, "soundSpeed", property).ok());
}

TEST(MaterialLibrary, AddFindUpdateRemove)
{
    MaterialLibrary library;
    Material material(Id("m1"), "First");
    ASSERT_TRUE(library.add(material).ok());
    EXPECT_FALSE(library.add(material).ok());  // duplicate id
    ASSERT_NE(library.find(Id("m1")), nullptr);
    EXPECT_EQ(library.findByName("First")->id(), Id("m1"));

    material.setName("Renamed");
    ASSERT_TRUE(library.update(material).ok());
    EXPECT_EQ(library.find(Id("m1"))->name(), "Renamed");
    EXPECT_FALSE(library.update(Material(Id("missing"), "x")).ok());

    EXPECT_TRUE(library.remove(Id("m1")).has_value());
    EXPECT_FALSE(library.remove(Id("m1")).has_value());
    EXPECT_TRUE(library.empty());
}

TEST(MaterialLibrary, ExampleLibraryHasTheTenMaterialsMarkedIllustrative)
{
    const MaterialLibrary library = createExampleMaterialLibrary();
    for (const char* name :
         {"Air", "Water", "Aluminum", "Steel", "Copper", "Glass", "Acrylic", "Concrete", "Rubber", "Wood"}) {
        const Material* material = library.findByName(name);
        ASSERT_NE(material, nullptr) << name;
        EXPECT_TRUE(material->validate().ok()) << name;
        EXPECT_TRUE(material->appliesTo(PhysicsDomain::Acoustic)) << name;
    }
    EXPECT_EQ(library.materials().size(), 18u);

    // No example value may pass as authoritative.
    for (const Material& material : library.materials()) {
        for (const PhysicsDomain domain : material.domains()) {
            for (const auto& [key, property] : *material.properties(domain)) {
                EXPECT_EQ(property.provenance, Provenance::Illustrative) << material.name() << " " << key;
                EXPECT_NE(property.source.find("not authoritative"), std::string::npos) << material.name() << " " << key;
            }
        }
    }
    // Fluids have no mechanical (solid) property set.
    EXPECT_FALSE(library.findByName("Air")->appliesTo(PhysicsDomain::Mechanical));
    EXPECT_TRUE(library.findByName("Steel")->appliesTo(PhysicsDomain::Mechanical));
}
