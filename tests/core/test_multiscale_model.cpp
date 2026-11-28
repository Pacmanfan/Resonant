// The multiscale model (RES-MSC-001..004, RES-MSC-010, RES-MSC-011): representations per
// scale, a navigable chain, lineage that survives save and reload, staleness when a source
// changes, model cards for every kind and method, and external engines never claimed.
#include "coupling/MultiscaleDerivations.h"
#include "multiscale/Couplings.h"
#include "multiscale/ExternalSolvers.h"
#include "multiscale/ModelCards.h"
#include "multiscale/Representation.h"
#include "project/Project.h"
#include "project/ProjectSerializer.h"

#include <gtest/gtest.h>

using namespace resonant;
namespace ms = resonant::multiscale;
using nlohmann::json;

namespace {

PropertyValue value(double v, Provenance p = Provenance::Measured, std::string derivation = {})
{
    PropertyValue x;
    x.value = v;
    x.provenance = p;
    x.source = "test";
    x.derivation = std::move(derivation);
    return x;
}

// Crystal X -> (D2) -> constituent C; RVE R of C and matrix B -> (D1) -> material M.
std::unique_ptr<Project> chainProject()
{
    auto p = std::make_unique<Project>();
    Material c(Id("mat-c"), "Constituent");
    EXPECT_TRUE(c.setProperty(PhysicsDomain::Thermal, "thermalConductivity", value(5.0, Provenance::Computed, "drv-2")).ok());
    Material b(Id("mat-b"), "Binder");
    EXPECT_TRUE(b.setProperty(PhysicsDomain::Thermal, "thermalConductivity", value(0.5)).ok());
    Material m(Id("mat-m"), "Composite");
    PropertyValue k = value(1.2, Provenance::Homogenized, "drv-1");
    k.uncertainty = 0.01;
    EXPECT_TRUE(m.setProperty(PhysicsDomain::Thermal, "thermalConductivity", k).ok());
    EXPECT_TRUE(p->materials.add(c).ok());
    EXPECT_TRUE(p->materials.add(b).ok());
    EXPECT_TRUE(p->materials.add(m).ok());
    ms::MultiscaleModel model;
    ms::Representation x;
    x.id = "rep-x";
    x.name = "Crystal";
    x.scale = ms::Scale::Atomistic;
    x.kind = "crystal";
    x.data = {{"structure", {{"atoms", json::array()}}}};
    ms::Representation r;
    r.id = "rep-r";
    r.name = "RVE";
    r.scale = ms::Scale::Microstructure;
    r.kind = "rve.voxel";
    r.data = {{"rve", {{"generator", "homogeneous"}}}};
    r.constituents = {"mat-c", "mat-b"};
    model.representations = {x, r};
    ms::Derivation d1;
    d1.id = "drv-1";
    d1.representation = "rep-r";
    d1.representationRevision = r.revision();
    d1.sourceScale = ms::Scale::Microstructure;
    d1.targetScale = ms::Scale::Effective;
    d1.material = "mat-m";
    d1.properties = {{"thermal", "thermalConductivity", "mean", "W/(m K)", 1.2, 0.01}};
    d1.constituentKeys = {{"mat-c", ms::materialContentKey(*p->materials.find(Id("mat-c")))},
                          {"mat-b", ms::materialContentKey(*p->materials.find(Id("mat-b")))}};
    ms::Derivation d2;
    d2.id = "drv-2";
    d2.representation = "rep-x";
    d2.representationRevision = x.revision();
    d2.sourceScale = ms::Scale::Atomistic;
    d2.material = "mat-c";
    d2.properties = {{"thermal", "thermalConductivity", "isotropic", "W/(m K)", 5.0, {}}};
    model.derivations = {d1, d2};
    model.relationships = {{"material:mat-m", "representation:rep-r", "derived-from"}, {"material:mat-c", "representation:rep-r", "constituent-of"},
                           {"material:mat-c", "representation:rep-x", "derived-from"}};
    ms::storeModel(*p, model);
    return p;
}

}  // namespace

TEST(MultiscaleModel, RepresentationsAreDistinctPerScale)
{
    auto p = chainProject();
    auto m = ms::modelOf(*p);
    ASSERT_TRUE(m) << m.message();
    EXPECT_TRUE(ms::validateModel(m.value(), *p).empty()) << ms::validateModel(m.value(), *p).front();
    ms::MultiscaleModel bad = m.value();
    bad.representations[0].kind = "rve.voxel";  // not an atomistic kind
    EXPECT_FALSE(ms::validateModel(bad, *p).empty());
    bad = m.value();
    bad.representations[1].constituents.push_back("mat-missing");
    EXPECT_FALSE(ms::validateModel(bad, *p).empty());
    EXPECT_FALSE(ms::Representation::fromJson({{"id", "x"}, {"scale", "galactic"}}));
    // The revision follows the physical content, not the name.
    ms::Representation r = m.value().representations[1];
    const std::string before = r.revision();
    r.name = "renamed";
    EXPECT_EQ(r.revision(), before);
    r.data["rve"]["seed"] = 2;
    EXPECT_NE(r.revision(), before);
    for (ms::Scale s : {ms::Scale::Electronic, ms::Scale::Atomistic, ms::Scale::Molecular, ms::Scale::Microstructure, ms::Scale::Effective, ms::Scale::Continuum})
        EXPECT_EQ(ms::scaleFromString(ms::toString(s)), s);
}

TEST(MultiscaleModel, RelationshipsFormANavigableChain)
{
    auto p = chainProject();
    const auto m = ms::modelOf(*p).value();
    const json l = ms::lineageOf(m, *p, "mat-m");
    ASSERT_EQ(l["properties"].size(), 1u);
    const json& k = l["properties"][0];
    EXPECT_EQ(k["provenance"], "homogenized");
    EXPECT_EQ(k["derivation"], "drv-1");
    EXPECT_DOUBLE_EQ(k["uncertainty"].get<double>(), 0.01);
    EXPECT_EQ(k["representation"]["id"], "rep-r");
    EXPECT_FALSE(k["stale"].get<bool>()) << k["staleReasons"].dump();
    // Down the chain: the RVE's constituent C was itself computed from the crystal.
    bool reachedCrystal = false;
    for (const json& step : k["chain"])
        for (const json& link : step["derivedFrom"])
            reachedCrystal = reachedCrystal || link["representation"]["id"] == "rep-x";
    EXPECT_TRUE(reachedCrystal) << k["chain"].dump();
}

TEST(MultiscaleModel, LineageSurvivesSaveAndReload)
{
    auto p = chainProject();
    auto back = deserializeProject(serializeProject(*p));
    ASSERT_TRUE(back) << back.message();
    const Project& q = *back.value().project;
    const PropertyValue* k = q.materials.find(Id("mat-m"))->property(PhysicsDomain::Thermal, "thermalConductivity");
    ASSERT_NE(k, nullptr);
    EXPECT_EQ(k->derivation, "drv-1");
    ASSERT_TRUE(k->uncertainty);
    EXPECT_DOUBLE_EQ(*k->uncertainty, 0.01);
    EXPECT_EQ(k->provenance, Provenance::Homogenized);
    EXPECT_EQ(ms::modelOf(q).value().toJson(), ms::modelOf(*p).value().toJson());
    for (Provenance x : {Provenance::Computed, Provenance::Database, Provenance::Fitted, Provenance::Surrogate, Provenance::Homogenized})
        EXPECT_EQ(provenanceFromString(toString(x)), x);
    // A property without lineage is written as before.
    const json plain = toJson(*q.materials.find(Id("mat-b")));
    EXPECT_FALSE(plain.dump().find("derivation") != std::string::npos);
}

TEST(MultiscaleModel, ChangesToTheSourceMakeDerivedPropertiesStale)
{
    auto p = chainProject();
    EXPECT_TRUE(ms::staleProperties(ms::modelOf(*p).value(), *p).empty());
    // The RVE changes.
    {
        auto q = chainProject();
        auto m = ms::modelOf(*q).value();
        m.representations[1].data["rve"]["seed"] = 9;
        ms::storeModel(*q, m);
        const auto stale = ms::staleProperties(m, *q);
        ASSERT_EQ(stale.size(), 1u);
        EXPECT_EQ(stale[0].material, "mat-m");
        EXPECT_NE(stale[0].reasons[0].find("changed"), std::string::npos);
    }
    // A constituent changes.
    {
        auto q = chainProject();
        Material b = *q->materials.find(Id("mat-b"));
        ASSERT_TRUE(b.setProperty(PhysicsDomain::Thermal, "thermalConductivity", value(0.6)).ok());
        ASSERT_TRUE(q->materials.update(b).ok());
        const auto stale = ms::staleProperties(ms::modelOf(*q).value(), *q);
        ASSERT_EQ(stale.size(), 1u);
        EXPECT_NE(stale[0].reasons[0].find("Binder"), std::string::npos);
    }
    // The value is overridden by hand.
    {
        auto q = chainProject();
        Material m = *q->materials.find(Id("mat-m"));
        PropertyValue k = *m.property(PhysicsDomain::Thermal, "thermalConductivity");
        k.value = 2.0;
        ASSERT_TRUE(m.setProperty(PhysicsDomain::Thermal, "thermalConductivity", k).ok());
        ASSERT_TRUE(q->materials.update(m).ok());
        const auto stale = ms::staleProperties(ms::modelOf(*q).value(), *q);
        ASSERT_EQ(stale.size(), 1u);
        EXPECT_NE(stale[0].reasons.back().find("changed after"), std::string::npos);
    }
}

TEST(MultiscaleModel, EveryMethodHasAModelCard)
{
    for (const auto& m : coupling::derivationMethods()) {
        EXPECT_NE(ms::findModelCard(m.modelCard), nullptr) << m.id;
        EXPECT_FALSE(m.properties.empty() && m.id != "atomistic.md-nve") << m.id;
    }
    for (const char* kind : {"crystal", "atomic-structure", "molecule", "molecular-system", "rve.voxel", "microstructure", "electronic-structure",
                             "effective-material", "continuum-material"})
        EXPECT_NE(ms::findModelCard(std::string("representation.") + kind), nullptr) << kind;
    for (const auto& card : ms::modelCards()) {
        EXPECT_FALSE(card.assumptions.empty() && card.methods.empty()) << card.id;
        EXPECT_FALSE(card.interpretation.empty()) << card.id;
    }
    bool fe2Planned = false;
    for (const auto& k : ms::couplingContracts())
        if (k.id == "fe2")
            fe2Planned = k.status == "planned" && !k.requirements.empty();
    EXPECT_TRUE(fe2Planned);
}

TEST(MultiscaleModel, ExternalSolversAreListedAndNeverClaimed)
{
    const auto list = ms::externalSolvers();
    std::vector<std::string> ids;
    for (const auto& x : list) {
        ids.push_back(x.id);
        EXPECT_NE(x.status.find("not integrated"), std::string::npos) << x.id;
        EXPECT_FALSE(x.toJson().dump().find("foundPath") != std::string::npos);  // no machine paths in reports
    }
    for (const char* id : {"lammps", "openmm", "quantum-espresso", "cp2k", "gpaw", "phonopy"})
        EXPECT_NE(std::find(ids.begin(), ids.end(), id), ids.end()) << id;
    // No method derives from the electronic or molecular scale.
    ms::Representation e;
    e.id = "rep-e";
    e.scale = ms::Scale::Electronic;
    e.kind = "electronic-structure";
    Project p;
    for (const auto& m : coupling::derivationMethods())
        EXPECT_FALSE(coupling::applicability(m, e, p, json::object()).empty()) << m.id;
    auto d = coupling::derive(e, p, "dft.band-structure", json::object(), nullptr);
    ASSERT_FALSE(d);
    EXPECT_NE(d.message().find("no electronic-scale calculation is integrated"), std::string::npos) << d.message();
}
