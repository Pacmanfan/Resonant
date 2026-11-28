// Atomistic models (RES-MSC-008, RES-MSC-009, docs/physics/ATOMISTIC_MODELS.md):
// Lennard-Jones lattice statics against an independent lattice sum and Kittel's
// tabulated sums, the Cauchy relation and Born stability, cutoff convergence, argon in
// SI units, force-field applicability, XYZ and POSCAR import, and NVE dynamics
// (energy conservation and its time-step scaling, equipartition, the radial distribution).
#include "atomistic/LatticeStatics.h"
#include "atomistic/MolecularDynamics.h"
#include "atomistic/StructureImport.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace resonant;
using namespace resonant::atomistic;

namespace {

const ForceField& reduced()
{
    return *findForceField("lj.reduced");
}

// FCC lattice sums in nearest-neighbour units by brute force (independent of the module).
void fccSums(double& A12, double& A6)
{
    A12 = A6 = 0.0;
    const int n = 40;
    for (int i = -n; i <= n; ++i)
        for (int j = -n; j <= n; ++j)
            for (int k = -n; k <= n; ++k) {
                if ((i + j + k) % 2 != 0 || (i == 0 && j == 0 && k == 0))
                    continue;
                const double r2 = 0.5 * (i * i + j * j + k * k);
                if (r2 > 0.5 * n * n)
                    continue;
                A12 += 1.0 / std::pow(r2, 6);
                A6 += 1.0 / std::pow(r2, 3);
            }
    // The tail beyond R = n / sqrt 2 of a uniform density sqrt 2 (per nearest-neighbour volume).
    const double R = n / std::sqrt(2.0), rho = std::sqrt(2.0), pi = 3.14159265358979323846;
    A6 += 4.0 * pi * rho / (3.0 * std::pow(R, 3));
    A12 += 4.0 * pi * rho / (9.0 * std::pow(R, 9));
}

Crystal fcc(double a)
{
    return cubicCrystal("fcc", a, "LJ").value();
}

}  // namespace

TEST(Atomistic, FccLatticeSumsMatchKittelAndTheIndependentSum)
{
    double A12 = 0.0, A6 = 0.0;
    fccSums(A12, A6);
    EXPECT_NEAR(A12, 12.13188, 1e-5);  // Kittel, Introduction to Solid State Physics, table 3.?
    EXPECT_NEAR(A6, 14.45392, 2e-4);
    const double r0 = std::pow(2.0 * A12 / A6, 1.0 / 6.0), E0 = -A6 * A6 / (2.0 * A12);
    StaticsSettings s;
    s.cutoffSigma = 12.0;
    auto relaxed = relaxIsotropic(fcc(1.55), reduced(), s);
    ASSERT_TRUE(relaxed) << relaxed.message();
    const double a = relaxed.value().crystal.lattice(0, 0);
    EXPECT_NEAR(a / std::sqrt(2.0), r0, 5e-5);
    EXPECT_NEAR(relaxed.value().energyPerAtom, E0, 1e-4);
    EXPECT_NEAR(E0, -8.6102, 1e-4);  // Kittel
    EXPECT_NEAR(relaxed.value().pressure, 0.0, 1e-4);
}

TEST(Atomistic, ElasticConstantsObeyCauchyAndGiveKittelsBulkModulus)
{
    StaticsSettings s;
    s.cutoffSigma = 12.0;
    auto relaxed = relaxIsotropic(fcc(1.55), reduced(), s);
    ASSERT_TRUE(relaxed);
    auto C = elasticConstants(relaxed.value().crystal, reduced(), s);
    ASSERT_TRUE(C) << C.message();
    const auto& c = C.value();
    EXPECT_TRUE(c.stable);
    EXPECT_TRUE(c.warnings.empty()) << c.warnings.front();
    // Cubic symmetry, central forces at centrosymmetric sites: C12 = C44.
    EXPECT_NEAR(c.C(0, 0), c.C(1, 1), 1e-6 * c.C(0, 0));
    EXPECT_NEAR(c.C(0, 1), c.C(0, 2), 1e-6 * c.C(0, 0));
    EXPECT_NEAR(c.C(3, 3), c.C(5, 5), 1e-6 * c.C(0, 0));
    EXPECT_LT(c.cauchyDeviation, 1e-4);
    EXPECT_NEAR(c.C(0, 3), 0.0, 1e-6 * c.C(0, 0));
    // The bulk modulus from the energy-volume curve of the independent sums (Kittel: about 75 eps / sigma^3).
    double A12 = 0.0, A6 = 0.0;
    fccSums(A12, A6);
    const double r0 = std::pow(2.0 * A12 / A6, 1.0 / 6.0);
    const double upp = 2.0 * (156.0 * A12 * std::pow(r0, -14) - 42.0 * A6 * std::pow(r0, -8));  // d2u/dr2 per atom
    const double B = std::sqrt(2.0) * upp / (9.0 * r0);
    EXPECT_NEAR(B, 75.0, 0.5);
    EXPECT_NEAR(c.bulkVoigt, B, 1e-3 * B);
    EXPECT_NEAR(c.bulkReuss, c.bulkVoigt, 1e-6 * B);  // cubic: B_V = B_R
    EXPECT_GT(c.shearVoigt, c.shearReuss);
    EXPECT_LT(c.maxInternalForce, 1e-8);  // a Bravais lattice: no internal relaxation
}

TEST(Atomistic, TheTailCorrectionMakesShortCutoffsAgree)
{
    const Crystal c = fcc(1.5417);
    StaticsSettings shortTail{2.5, true}, longTail{15.0, true}, shortBare{2.5, false};
    const double a = energyPerAtom(c, reduced(), Eigen::Matrix3d::Identity(), shortTail).value();
    const double b = energyPerAtom(c, reduced(), Eigen::Matrix3d::Identity(), longTail).value();
    const double d = energyPerAtom(c, reduced(), Eigen::Matrix3d::Identity(), shortBare).value();
    EXPECT_NEAR(a, b, 0.01 * std::abs(b));
    EXPECT_GT(std::abs(d - b), 3.0 * std::abs(a - b));
}

TEST(Atomistic, ArgonInSiUnitsAndForceFieldApplicability)
{
    const ForceField& ar = *findForceField("lj.argon");
    auto relaxed = relaxIsotropic(cubicCrystal("fcc", 5.3e-10, "Ar").value(), ar);
    ASSERT_TRUE(relaxed) << relaxed.message();
    const double a = relaxed.value().crystal.lattice(0, 0);
    EXPECT_NEAR(a, std::sqrt(2.0) * 1.09018 * 3.405e-10, 5e-14);  // 5.250 A; measured at 0 K: 5.31 A (zero-point motion is absent)
    auto C = elasticConstants(relaxed.value().crystal, ar);
    ASSERT_TRUE(C);
    const double eps = 119.8 * 1.380649e-23, sigma = 3.405e-10;
    EXPECT_NEAR(C.value().bulk, 75.18 * eps / std::pow(sigma, 3), 0.003 * C.value().bulk);
    EXPECT_NEAR(C.value().density, 4 * 39.948 * 1.66053906660e-27 / std::pow(a, 3), 1e-9);
    EXPECT_GT(C.value().longitudinalSpeed, C.value().transverseSpeed);
    // Copper is outside the argon force field.
    const Crystal cu = cubicCrystal("fcc", 3.6e-10, "Cu").value();
    EXPECT_EQ(incompatibleSpecies(ar, cu), std::vector<std::string>{"Cu"});
    EXPECT_FALSE(energyPerAtom(cu, ar));
}

TEST(Atomistic, StructuresImportFromXyzAndPoscarWithoutInventedData)
{
    const Crystal c = cubicCrystal("fcc", 5.25e-10, "Ar").value();
    auto back = parseXyz(writeXyz(c));
    ASSERT_TRUE(back) << back.message();
    EXPECT_TRUE(back.value().periodic);
    EXPECT_EQ(back.value().atoms.size(), 4u);
    EXPECT_NEAR(back.value().lattice(0, 0), 5.25e-10, 1e-19);
    EXPECT_NEAR(back.value().atoms[1].position.x(), 0.5, 1e-9);
    // A plain XYZ file has no cell: not periodic, nothing invented.
    auto plain = parseXyz("2\nargon dimer\nAr 0 0 0\nAr 0 0 3.8\n");
    ASSERT_TRUE(plain);
    EXPECT_FALSE(plain.value().periodic);
    EXPECT_NEAR(plain.value().atoms[1].position.z(), 3.8e-10, 1e-20);
    EXPECT_FALSE(relaxIsotropic(plain.value(), *findForceField("lj.argon")));
    EXPECT_FALSE(parseXyz("3\nshort\nAr 0 0 0\n"));
    const std::string poscar = "Ar fcc\n5.25\n1 0 0\n0 1 0\n0 0 1\nAr\n4\nDirect\n0 0 0\n0.5 0.5 0\n0.5 0 0.5\n0 0.5 0.5\n";
    auto p = parsePoscar(poscar);
    ASSERT_TRUE(p) << p.message();
    EXPECT_NEAR(p.value().lattice(1, 1), 5.25e-10, 1e-19);
    EXPECT_EQ(p.value().composition().at("Ar"), 4);
    EXPECT_FALSE(parsePoscar("old\n5.25\n1 0 0\n0 1 0\n0 0 1\n4\nDirect\n0 0 0\n0.5 0.5 0\n0.5 0 0.5\n0 0.5 0.5\n"));  // VASP 4: no species
    EXPECT_FALSE(parseStructure(poscar, "cif"));
}

TEST(Atomistic, NveDynamicsConservesEnergyWithTheTimeStepSquared)
{
    // A reduced-unit FCC solid near equilibrium (4 x 4 x 4 cells, 256 atoms) started at T* = 0.1.
    const Crystal solid = fcc(1.5496).supercell(4, 4, 4);
    MdSettings s;
    s.temperature = 0.1 / 1.380649e-23;  // T* = kT / eps with eps = 1 J
    s.cutoffSigma = 2.5;
    s.sampleEvery = 10;
    s.timeStep = 0.004;
    s.steps = 500;
    auto coarse = runNve(solid, reduced(), s);
    ASSERT_TRUE(coarse) << coarse.message();
    s.timeStep = 0.002;
    s.steps = 1000;
    auto fine = runNve(solid, reduced(), s);
    ASSERT_TRUE(fine) << fine.message();
    EXPECT_LT(fine.value().energyDrift, 1e-2);
    const double ratio = coarse.value().energyDrift / fine.value().energyDrift;
    EXPECT_GT(ratio, 2.5);
    EXPECT_LT(ratio, 6.0);
    // A harmonic solid started from its lattice shares the energy: the mean temperature falls to about half.
    EXPECT_NEAR(fine.value().meanTemperature * 1.380649e-23, 0.05, 0.015);
    // g(r) peaks at the nearest-neighbour distance; the atoms stay near their sites.
    const auto& g = fine.value().rdf;
    const size_t peak = static_cast<size_t>(std::max_element(g.begin(), g.end()) - g.begin());
    EXPECT_NEAR(fine.value().rdfRadius[peak], 1.5496 / std::sqrt(2.0), 0.05);
    EXPECT_LT(std::sqrt(fine.value().msd.back()) / (1.5496 / std::sqrt(2.0)), 0.15);
    // Too small a cell for the cutoff is refused.
    EXPECT_FALSE(runNve(fcc(1.5496).supercell(2, 2, 2), reduced(), s));
}
