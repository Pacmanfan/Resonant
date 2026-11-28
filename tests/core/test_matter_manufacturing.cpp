// Manufacturing awareness (RES-FAM-015): violations against the stated
// constraints, the residual stress built in at the process temperature, the
// unassessed items, and the idealized architecture never reported feasible.
#include "MatterFixture.h"

#include "coupling/MatterAnalyses.h"
#include "matter/Manufacturing.h"

#include <gtest/gtest.h>

using namespace matter_test;
using namespace resonant::matter;

namespace {

std::vector<ResidualStress> residual(const Architecture& a, const MaterialLibrary& lib)
{
    std::vector<ResidualStress> out;
    for (const json& r : coupling::processResidualStresses(a, lib))
        if (r.contains("stress"))
            out.push_back({r["interface"].get<std::string>(), r["layer"].get<std::string>(), r["stress"].get<double>(), r["temperature"].get<double>(),
                           r["basis"].get<std::string>()});
    return out;
}

bool has(const std::vector<Issue>& issues, const std::string& code, const std::string& element = "")
{
    for (const auto& i : issues)
        if (i.code == code && (element.empty() || i.element == element))
            return true;
    return false;
}

}  // namespace

TEST(MatterManufacturing, AFilmCrystallizedOnSteelDelaminatesAndTheBondedPlateDoesNot)
{
    auto p = example("b-energy-converter.resonant");
    const Architecture film = architecture(*p, "arch-film");
    const auto rs = residual(film, p->materials);
    ASSERT_FALSE(rs.empty());
    double worst = 0.0;
    for (const auto& r : rs)
        if (r.interface == "i-electrode-pzt")
            worst = r.stress;
    // PZT (4e-6 /K) on steel (16e-6 /K) cooled from 923 K: hundreds of MPa.
    EXPECT_GT(worst, 3e8) << worst;
    const ManufacturingCheck f = checkManufacturing(film, rs);
    EXPECT_EQ(f.status, "infeasible");
    EXPECT_TRUE(has(f.violations, "delamination-risk", "i-electrode-pzt"));
    EXPECT_TRUE(has(f.violations, "residual-stress", "i-electrode-pzt"));
    // The bonded plate, cured at 393 K, stays within its limits.
    const Architecture bonded = architecture(*p, "arch-bonded");
    const ManufacturingCheck b = checkManufacturing(bonded, residual(bonded, p->materials));
    for (const auto& v : b.violations)
        ADD_FAILURE() << v.message;
    EXPECT_NE(b.status, "infeasible");
    EXPECT_TRUE(has(b.unassessed, "no-temperature-limit", "electrode-bottom")) << "a missing limit is reported, not assumed";
    // A hot cure at 393 K would build the steel - PZT mismatch into the plate.
    Architecture hot = bonded;
    for (auto& proc : hot.processes)
        if (proc.id == "proc-lam")
            proc.constraints["temperature"] = 393.0;
    const ManufacturingCheck h = checkManufacturing(hot, residual(hot, p->materials));
    EXPECT_EQ(h.status, "infeasible");
    EXPECT_TRUE(has(h.violations, "residual-stress", "i-electrode-pzt"));
}

TEST(MatterManufacturing, ThicknessFeatureMaterialAndSequenceViolations)
{
    auto p = example("b-energy-converter.resonant");
    Architecture a = architecture(*p, "arch-film");
    a.stack("st-beam")->layers[2].thickness = 8e-6;  // above the sol-gel limit of 5 um
    EXPECT_TRUE(has(checkManufacturing(a).violations, "layer-too-thick", "pzt.0"));
    a = architecture(*p, "arch-film");
    a.stack("st-beam")->layers[0].thickness = 2e-4;  // below the printable wall of 0.3 mm
    EXPECT_TRUE(has(checkManufacturing(a).violations, "layer-too-thin", "substrate.0"));
    a = architecture(*p, "arch-film");
    a.stack("st-beam")->layers[3].pattern.gap = 5e-5;  // a gap below the mask's 100 um
    a.stack("st-beam")->layers[3].pattern.segments = 2;
    EXPECT_TRUE(has(checkManufacturing(a).violations, "feature-too-small", "electrode-top.0"));
    a = architecture(*p, "arch-film");
    a.region("electrode-bottom")->materialClass = "ceramic";
    EXPECT_TRUE(has(checkManufacturing(a).violations, "incompatible-material", "electrode-bottom"));
    // A process too hot for a layer made before it.
    a = architecture(*p, "arch-film");
    a.region("substrate")->limits["maxTemperature"] = 800.0;
    const ManufacturingCheck hot = checkManufacturing(a);
    EXPECT_TRUE(has(hot.violations, "process-too-hot", "substrate.0"));
    a = architecture(*p, "arch-film");
    a.processes.push_back({"proc-x", "x", "levitation", json::object(), json::object(), ""});
    EXPECT_TRUE(has(checkManufacturing(a).violations, "unknown-process"));
    // A tolerance tighter than the process repeatability is noted.
    a = architecture(*p, "arch-bonded");
    a.stack("st-beam")->layers[0].thicknessUncertainty = 1e-6;
    EXPECT_TRUE(has(checkManufacturing(a).notes, "tolerance-tighter-than-process"));
    EXPECT_NE(findProcessKind("atomic-layer-deposition"), nullptr);
    EXPECT_EQ(findProcessKind("levitation"), nullptr);
}

TEST(MatterManufacturing, IdealizedArchitecturesAreNeverReportedFeasible)
{
    auto p = example("d-graded-metal-ceramic.resonant");
    const ManufacturingCheck c = checkManufacturing(architecture(*p, "arch-fgm"));
    EXPECT_EQ(c.status, "not-assessed");
    EXPECT_TRUE(has(c.unassessed, "idealized-field", "fld-alumina"));
    EXPECT_TRUE(has(c.unassessed, "no-process", "graded"));
    EXPECT_TRUE(has(c.notes, "idealized"));
}
