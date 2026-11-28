// Engine adapters, the atomistic-to-continuum bridge and microstructure statistics
// (REQ-CHAT-20261128-006, RES-MSC-015..017, ADR-000234). No external engine is installed here:
// the in-house static lattice runs through the same interface, the external adapters prepare
// their inputs and refuse to run, and the LAMMPS parser is checked on a synthetic fixture.

#include "atomistic/ForceField.h"
#include "atomistic/LatticeStatics.h"
#include "coupling/EngineAdapters.h"
#include "mechanics/LayeredSolid.h"
#include "multiscale/MicrostructureAnalysis.h"

#include <gtest/gtest.h>

#include <cmath>
#include <sstream>

using namespace resonant;
using namespace resonant::coupling;

namespace {

coupling::AtomisticTask argon(const std::string& property)
{
    AtomisticTask t;
    t.property = property;
    t.structure = atomistic::cubicCrystal("fcc", 5.26e-10, "Ar").value();
    t.forceField = "lj.argon";
    return t;
}

}  // namespace

TEST(EngineAdapters, EachEngineStatesWhatItIsForAndWhetherItCanRun)
{
    const auto ids = engineAdapterIds();
    ASSERT_EQ(ids.size(), 3u);
    for (const auto& id : ids) {
        auto a = engineAdapter(id);
        ASSERT_NE(a, nullptr) << id;
        const auto app = a->applicability();
        EXPECT_FALSE(app.properties.empty()) << id;
        EXPECT_FALSE(app.notFor.empty()) << id;
        const Status here = a->available();
        if (!here.ok())
            EXPECT_NE(here.message().find("not installed"), std::string::npos) << here.message();
    }
    EXPECT_TRUE(engineAdapter("resonant.lattice-statics")->available().ok());
    EXPECT_EQ(engineAdapter("no-such-engine"), nullptr);
}

TEST(EngineAdapters, TheInHouseLatticeCrossesToTheContinuumWithItsProvenance)
{
    auto a = engineAdapter("resonant.lattice-statics");
    auto r = a->run(argon("elastic-constants"), "");
    ASSERT_TRUE(r.ok()) << r.message();
    ASSERT_EQ(r.value().size(), 1u);
    const TransferredProperty& p = r.value().front();
    // The same constants as the engine itself, reordered to the mechanics Voigt order.
    const auto& ff = *atomistic::findForceField("lj.argon");
    auto relaxed = atomistic::relaxIsotropic(argon("elastic-constants").structure, ff);
    auto direct = atomistic::elasticConstants(relaxed.value().crystal, ff, {}, 1e-4);
    ASSERT_TRUE(direct.ok());
    const Eigen::Matrix<double, 6, 6>& c = direct.value().C;  // xx yy zz yz xz xy
    ASSERT_EQ(p.values.size(), 21u);
    EXPECT_NEAR(p.values[0], c(0, 0), 1e-9 * c(0, 0));    // C11
    EXPECT_NEAR(p.values[1], c(0, 1), 1e-9 * c(0, 0));    // C12
    EXPECT_NEAR(p.values[15], c(5, 5), 1e-9 * c(0, 0));   // the xy shear: mechanics index 3
    EXPECT_NEAR(p.values[18], c(3, 3), 1e-9 * c(0, 0));   // the yz shear: mechanics index 4
    // Conditions, uncertainty and provenance travel with it.
    EXPECT_EQ(p.temperature, 0.0);
    EXPECT_EQ(p.composition.at("Ar"), 4);
    EXPECT_NE(p.uncertaintyKind.find("numerical"), std::string::npos);
    EXPECT_LT(p.uncertainty[0], 1e-2 * p.values[0]);
    EXPECT_FALSE(p.inputDigest.empty());
    EXPECT_NE(p.forceField.find("lj.argon"), std::string::npos);
    // As a computed field, it gives a continuum its stiffness: a free cube stretched by a
    // stress strains by the compliance of those constants.
    auto field = p.toMaterialField("c-argon", "crystal");
    ASSERT_TRUE(field.ok()) << field.message();
    EXPECT_EQ(field.value().source, "computed");
    mechanics::LayeredBody body;
    body.surface.length = body.surface.width = 0.001;
    mechanics::BodyLayer l;
    l.name = "argon";
    l.material.base.name = "argon";
    l.material.base.youngsModulus = direct.value().young;
    l.material.base.poissonsRatio = direct.value().poisson;
    l.material.base.density = direct.value().density;
    l.material.fields = {field.value()};
    l.thickness.start = 0.001;
    body.layers = {l};
    body.elementsU = body.elementsV = 1;
    mechanics::LayeredSupports s;
    s.symmetry = {{0, 0.0}, {1, 0.0}, {2, 0.0}};
    mechanics::LayeredLoads loads;
    const double sigma = 1e6;
    loads.edgeForces = {{mechanics::BodyEdge::U1, Vec3(sigma * 1e-6, 0, 0)}};
    auto res = mechanics::solveLayeredStatic(body, s, loads);
    ASSERT_TRUE(res.ok()) << res.message();
    const Eigen::Matrix<double, 6, 6> compliance = materials::tensorOf(materials::FieldRank::Stiffness6,
                                                                       Eigen::Map<const Eigen::VectorXd>(p.values.data(), 21)).inverse();
    double ux = 0;
    int n = 0;
    for (size_t i = 0; i < res.value().mesh.structure.nodes.size(); ++i)
        if (std::abs(res.value().mesh.structure.nodes[i].x() - 0.001) < 1e-15) {
            ux += res.value().mesh.structure.translation(res.value().displacement, i).x();
            ++n;
        }
    EXPECT_NEAR(ux / n, compliance(0, 0) * sigma * 0.001, 1e-9 * compliance(0, 0) * sigma * 0.001);
    // Outside its applicability it refuses.
    AtomisticTask hot = argon("elastic-constants");
    hot.temperature = 80;
    EXPECT_FALSE(a->run(hot, "").ok());
    AtomisticTask copper = argon("elastic-constants");
    copper.structure = atomistic::cubicCrystal("fcc", 3.6e-10, "Cu").value();
    EXPECT_FALSE(a->run(copper, "").ok());
    EXPECT_FALSE(a->run(argon("diffusion"), "").ok());
}

TEST(EngineAdapters, LammpsInputsArePreparedAndNothingIsInventedWithoutTheEngine)
{
    auto a = engineAdapter("lammps");
    auto deck = a->prepare(argon("elastic-constants"));
    ASSERT_TRUE(deck.ok()) << deck.message();
    const std::string& text = deck.value();
    EXPECT_NE(text.find("pair_style lj/cut"), std::string::npos);
    size_t prints = 0;
    for (size_t at = text.find("print \"RESONANT-STRESS"); at != std::string::npos; at = text.find("print \"RESONANT-STRESS", at + 1))
        ++prints;
    EXPECT_EQ(prints, 12u);  // six strains, both signs
    EXPECT_NE(text.find("4 atoms"), std::string::npos);
    EXPECT_FALSE(a->prepare(argon("thermal-conductivity")).ok());
    if (!a->available().ok()) {
        auto run = a->run(argon("elastic-constants"), "unused");
        ASSERT_FALSE(run.ok());
        EXPECT_NE(run.message().find("no result is invented"), std::string::npos) << run.message();
    }
}

TEST(EngineAdapters, TheLammpsLogParserRecoversTheStiffness)
{
    // A synthetic parser fixture (not a LAMMPS result): the pressures an isotropic
    // stiffness gives under the deck's twelve deformations.
    Eigen::Matrix<double, 6, 6> c = Eigen::Matrix<double, 6, 6>::Zero();
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            c(i, j) = i == j ? 200e9 : 100e9;
    c(3, 3) = c(4, 4) = c(5, 5) = 50e9;
    c(0, 3) = c(3, 0) = 5e9;  // an anisotropic coupling, xx to xy
    const double strain = 1e-4;
    std::ostringstream log;
    log << "LAMMPS (fixture)\nprint \"RESONANT-STRESS 0 + $(pxx) ...\"\n";  // an echoed command: skipped
    static constexpr int lammps[6] = {0, 1, 2, 3, 5, 4};  // pxx pyy pzz pxy pxz pyz from the mechanics order
    for (int k = 0; k < 6; ++k)
        for (const int sign : {+1, -1}) {
            Eigen::Matrix<double, 6, 1> e = Eigen::Matrix<double, 6, 1>::Zero();
            e(k) = sign * strain;
            const Eigen::Matrix<double, 6, 1> s = c * e;
            log << "RESONANT-STRESS " << k << " " << (sign > 0 ? "+" : "-");
            for (int i = 0; i < 6; ++i)
                log << " " << -s(lammps[i]) / 1e5;  // pressure in bar
            log << "\n";
        }
    auto parsed = elasticConstantsFromLammpsLog(log.str(), strain);
    ASSERT_TRUE(parsed.ok()) << parsed.message();
    EXPECT_TRUE(parsed.value().isApprox(c, 1e-6)) << parsed.value();
    EXPECT_FALSE(elasticConstantsFromLammpsLog("RESONANT-STRESS 0 + 1 2 3 4 5 6\n", strain).ok());  // incomplete
}

TEST(EngineAdapters, GromacsIsDeclaredForSoftMatterOnly)
{
    auto a = engineAdapter("gromacs");
    auto elastic = a->prepare(argon("elastic-constants"));
    ASSERT_FALSE(elastic.ok());
    EXPECT_NE(elastic.message().find("applicability"), std::string::npos);
    auto density = a->prepare(argon("density"));
    ASSERT_FALSE(density.ok());
    EXPECT_NE(density.message().find("not implemented"), std::string::npos);
}

// --- microstructure -------------------------------------------------------------------------

TEST(Microstructure, GrainBoundariesAreCountedAndResistInSeries)
{
    // A bicrystal: two grains stacked along x, periodic (two boundaries per period).
    const int nx = 20, ny = 2, nz = 2;
    const double lx = 1e-4, ly = 1e-5, lz = 1e-5;
    nlohmann::json spec{{"generator", "voxels"}, {"voxels", {nx, ny, nz}}, {"size", {lx, ly, lz}}, {"phases", {"alumina"}}, {"periodic", true}};
    std::vector<int> data, grain;
    for (int k = 0; k < nz; ++k)
        for (int j = 0; j < ny; ++j)
            for (int i = 0; i < nx; ++i) {
                data.push_back(0);
                grain.push_back(i < nx / 2 ? 0 : 1);
            }
    spec["data"] = data;
    spec["grain"] = grain;
    spec["axes"] = {{0, 0, 1}, {0, 0, 1}};
    auto rve = multiscale::generateRve(spec);
    ASSERT_TRUE(rve.ok()) << rve.message();
    const auto st = multiscale::microstructureStatistics(rve.value());
    EXPECT_EQ(st.grains, 2);
    EXPECT_NEAR(st.grainBoundaryDensity, 2.0 / lx, 1e-9 * 2.0 / lx);
    EXPECT_NEAR(st.phaseBoundaryDensity, 0.0, 1e-30);
    // Conduction: across the boundaries the resistance adds in series, along them nothing changes.
    const double k = 30.0, resistance = 1e-7;
    auto plain = multiscale::conductionProblem(rve.value(), {Eigen::Matrix3d::Identity() * k}, 0.0);
    auto resisted = multiscale::conductionProblem(rve.value(), {Eigen::Matrix3d::Identity() * k}, resistance);
    ASSERT_TRUE(plain.ok() && resisted.ok());
    auto h0 = multiscale::homogenize(plain.value(), multiscale::Boundary::Periodic);
    auto h1 = multiscale::homogenize(resisted.value(), multiscale::Boundary::Periodic);
    ASSERT_TRUE(h0.ok() && h1.ok());
    EXPECT_NEAR(h0.value().effective(0, 0), k, 1e-8 * k);
    EXPECT_NEAR(h1.value().effective(0, 0), lx / (lx / k + 2 * resistance), 1e-8 * k);
    EXPECT_NEAR(h1.value().effective(1, 1), k, 1e-8 * k);
}

TEST(Microstructure, PorosityInclusionsAndDefectsAreReported)
{
    const int n = 6;
    nlohmann::json spec{{"generator", "voxels"}, {"voxels", {n, n, 1}}, {"size", {6e-6, 6e-6, 1e-6}}, {"phases", {"matrix", "pore", "carbide"}}, {"periodic", false}};
    std::vector<int> data(n * n, 0);
    data[0] = data[1] = 1;            // a pore of two voxels
    data[3 * n + 3] = 2;              // two separate carbides
    data[5 * n + 0] = 2;
    spec["data"] = data;
    auto rve = multiscale::generateRve(spec);
    ASSERT_TRUE(rve.ok()) << rve.message();
    multiscale::DefectRecord d;
    d.kind = "dislocation";
    d.where = "matrix";
    d.density = 1e14;
    d.unit = "1/m^2";
    d.provenance = "a stated estimate";
    const auto st = multiscale::microstructureStatistics(rve.value(), {"pore"}, {"carbide"}, {d});
    EXPECT_NEAR(st.porosity, 2.0 / 36.0, 1e-15);
    EXPECT_EQ(st.inclusions, 2);
    ASSERT_EQ(st.defects.size(), 1u);
    bool stated = false;
    for (const auto& note : st.notes)
        stated = stated || note.find("not computed") != std::string::npos;
    EXPECT_TRUE(stated);
    // An invalid phase conductivity is refused.
    EXPECT_FALSE(multiscale::conductionProblem(rve.value(), {Eigen::Matrix3d::Identity(), -Eigen::Matrix3d::Identity(), Eigen::Matrix3d::Identity()}, 0.0).ok());
}
