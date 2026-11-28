// Importing measurement files (RES-LAB-003, -004): units and metadata from the file,
// conversions recorded, the raw bytes' digest kept, and every problem of an invalid file
// reported with its row and column - nothing discarded silently.
#include "laboratory/LabImport.h"
#include "laboratory/Sha256.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstring>

using namespace resonant;
using namespace resonant::laboratory;
namespace ex = resonant::experiments;

namespace {

bool has(const ImportReport& r, const std::string& code, const std::string& severity)
{
    for (const auto& i : r.issues)
        if (i.code == code && i.severity == severity)
            return true;
    return false;
}

ImportResult csv(const std::string& text, ImportOptions o = {})
{
    if (o.fileName.empty())
        o.fileName = "data.csv";
    return importData(text, o);
}

}  // namespace

TEST(LaboratoryImport, Sha256KnownVectors)
{
    EXPECT_EQ(sha256Hex(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    EXPECT_EQ(sha256Hex("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    EXPECT_EQ(sha256Hex(std::string(1000, 'a')).size(), 64u);
}

TEST(LaboratoryImport, FrequencyResponseWithUnitsAndMetadata)
{
    const std::string text = "# quantity: Displacement\n# instrument: impact hammer and accelerometer (example)\n# provenance: synthetic\n"
                             "# uncertainty: 2 %\nfrequency [kHz], magnitude [mm/N], phase [deg]\n0.1, 1.0, -10\n0.2, 2.0, -90\n0.3, 0.5, -170\n";
    const ImportResult r = csv(text);
    ASSERT_TRUE(r.report.accepted) << r.report.toJson().dump();
    ASSERT_EQ(r.datasets.size(), 1u);
    const auto& d = r.datasets[0];
    EXPECT_EQ(d.record.kind, ex::MeasurementKind::TransferFunction);
    EXPECT_EQ(d.record.quantity.quantity, ex::Quantity::Displacement);
    ASSERT_TRUE(d.record.quantity.per.has_value());
    EXPECT_EQ(*d.record.quantity.per, ex::Quantity::Force);
    EXPECT_DOUBLE_EQ(d.data.axis[1], 200.0);                       // kHz -> Hz
    EXPECT_NEAR(std::abs(d.data.complex[1]), 2.0e-3, 1e-15);       // mm/N -> m/N
    EXPECT_NEAR(std::arg(d.data.complex[1]), -3.14159265358979 / 2, 1e-12);  // deg -> rad
    EXPECT_EQ(d.record.category, ex::DataCategory::Raw);
    EXPECT_TRUE(d.record.simulatedDevice);  // synthetic: generated, not measured
    EXPECT_NE(d.record.note.find("synthetic"), std::string::npos);
    EXPECT_EQ(d.record.acquisition["source"]["sha256"], sha256Hex(text));
    EXPECT_TRUE(d.record.history.empty());  // raw: no processing
    ASSERT_EQ(d.record.acquisition["importSteps"].size(), 1u);
    EXPECT_EQ(d.record.acquisition["importSteps"][0]["operation"], "import");
    EXPECT_NEAR(d.record.uncertainty.combinedRelative(), 0.02, 1e-15);
    EXPECT_EQ(r.report.kind, "frequency-response");
    EXPECT_EQ(r.report.sha256, sha256Hex(text));
}

TEST(LaboratoryImport, TimeSeriesAndConversions)
{
    const ImportResult r = csv("# provenance: measured\n# instrument: thermocouple logger\ntime [ms], temperature [degC]\n0, 20\n100, 21\n200, 22.5\n");
    ASSERT_TRUE(r.report.accepted);
    const auto& d = r.datasets[0];
    EXPECT_EQ(d.record.quantity.quantity, ex::Quantity::Temperature);
    EXPECT_NEAR(d.data.real[0], 293.15, 1e-12);  // degC -> K, recorded
    EXPECT_NEAR(d.record.timeBase.sampleRateHz, 10.0, 1e-12);
    EXPECT_TRUE(has(r.report, "conversion", "info"));
    EXPECT_FALSE(d.record.simulatedDevice);
}

TEST(LaboratoryImport, InvalidDatasetsAreReportedAndNothingIsDiscarded)
{
    // Missing unit: no dataset.
    ImportResult r = csv("# provenance: measured\ntime [s], voltage\n0, 1\n1, 2\n");
    EXPECT_FALSE(r.report.accepted);
    EXPECT_TRUE(r.datasets.empty());
    EXPECT_TRUE(has(r.report, "missing-unit", "error"));
    // A malformed row: an error with its row; with acceptPartial, the row is left out and that is recorded.
    const std::string malformed = "# provenance: measured\n# instrument: daq\ntime [s], voltage [V]\n0, 1\n1, 2, 3\n2, 3\n";
    r = csv(malformed);
    EXPECT_FALSE(r.report.accepted);
    bool rowFound = false;
    for (const auto& i : r.report.issues)
        rowFound = rowFound || (i.code == "malformed-row" && i.row == 5);
    EXPECT_TRUE(rowFound);
    ImportOptions partial;
    partial.acceptPartial = true;
    r = csv(malformed, partial);
    ASSERT_TRUE(r.report.accepted);
    EXPECT_EQ(r.datasets[0].data.real.size(), 2u);
    EXPECT_EQ(r.datasets[0].record.acquisition["importSteps"].back()["operation"], "exclude-rows");
    EXPECT_FALSE(r.datasets[0].record.quality.notes.empty());
    // NaN and infinity: kept as read, each reported.
    r = csv("# provenance: measured\n# instrument: daq\ntime [s], voltage [V]\n0, 1\n1, nan\n2, inf\n3,\n");
    ASSERT_TRUE(r.report.accepted);
    EXPECT_TRUE(std::isnan(r.datasets[0].data.real[1]));
    EXPECT_TRUE(std::isinf(r.datasets[0].data.real[2]));
    EXPECT_TRUE(has(r.report, "not-finite", "warning"));
    EXPECT_TRUE(has(r.report, "missing-value", "warning"));
    // Timestamps.
    EXPECT_TRUE(has(csv("time [s], voltage [V]\n0, 1\n2, 2\n1, 3\n").report, "non-monotonic-time", "error"));
    EXPECT_TRUE(has(csv("time [s], voltage [V]\n0, 1\n1, 2\n1, 3\n").report, "duplicate-time", "error"));
    EXPECT_TRUE(has(csv("time [s], voltage [V]\n0, 1\n1, 2\n3, 3\n4, 3\n").report, "inconsistent-sampling", "warning"));
    EXPECT_TRUE(has(csv("# sample rate: 100\ntime [s], voltage [V]\n0, 1\n1, 2\n2, 3\n").report, "sampling-mismatch", "warning"));
    // A missing channel, an unknown unit, missing metadata, a missing frame.
    ImportOptions expect;
    expect.expectedColumns = {"current"};
    EXPECT_TRUE(has(csv("time [s], voltage [V]\n0, 1\n1, 2\n", expect).report, "missing-column", "error"));
    EXPECT_TRUE(has(csv("time [s], speed [furlong/s]\n0, 1\n1, 2\n").report, "unknown-unit", "error"));
    EXPECT_TRUE(has(csv("time [s], voltage [V]\n0, 1\n1, 2\n").report, "missing-metadata", "warning"));
    EXPECT_TRUE(has(csv("# provenance: measured\nx [m], y [m], z [m], pressure [Pa]\n0,0,0,1\n1,0,0,2\n").report, "missing-frame", "warning"));
    // Empty, malformed JSON, not a WAVE.
    EXPECT_TRUE(has(csv("").report, "empty", "error"));
    ImportOptions js;
    js.fileName = "bad.json";
    EXPECT_TRUE(has(importData("{not json", js).report, "unreadable", "error"));
    ImportOptions wv;
    wv.fileName = "bad.wav";
    EXPECT_TRUE(has(importData("RIFFxxxxNOPE", wv).report, "unreadable", "error"));
}

TEST(LaboratoryImport, JsonScalarsAndWave)
{
    ImportOptions js;
    js.fileName = "q.json";
    const ImportResult s = importData(R"({"format": "resonant.dataset", "metadata": {"provenance": "measured", "instrument": "network analyser"},
        "scalars": [{"name": "resonance", "quantity": "Frequency", "unit": "kHz", "value": 2.5, "uncertainty": 0.01}]})", js);
    ASSERT_TRUE(s.report.accepted) << s.report.toJson().dump();
    EXPECT_EQ(s.datasets[0].record.kind, ex::MeasurementKind::Scalar);
    EXPECT_DOUBLE_EQ(s.datasets[0].data.real[0], 2500.0);
    EXPECT_DOUBLE_EQ(s.datasets[0].record.uncertainty.combinedStandard(2500.0), 10.0);
    // A 16-bit stereo WAVE, written here.
    std::string wav = "RIFF";
    const auto u32 = [&](uint32_t v) {
        for (int i = 0; i < 4; ++i)
            wav.push_back(static_cast<char>((v >> (8 * i)) & 0xff));
    };
    const auto u16 = [&](uint16_t v) {
        wav.push_back(static_cast<char>(v & 0xff));
        wav.push_back(static_cast<char>(v >> 8));
    };
    const int frames = 4;
    u32(36 + frames * 4);
    wav += "WAVEfmt ";
    u32(16);
    u16(1);
    u16(2);
    u32(8000);
    u32(8000 * 4);
    u16(4);
    u16(16);
    wav += "data";
    u32(frames * 4);
    for (int f = 0; f < frames; ++f) {
        u16(static_cast<uint16_t>(16384));                     // +0.5
        u16(static_cast<uint16_t>(static_cast<int16_t>(-16384)));  // -0.5
    }
    ImportOptions wo;
    wo.fileName = "tap.wav";
    wo.provenance = "measured";
    const ImportResult w = importData(wav, wo);
    ASSERT_TRUE(w.report.accepted) << w.report.toJson().dump();
    ASSERT_EQ(w.datasets.size(), 2u);
    EXPECT_EQ(w.datasets[0].record.quantity.quantity, ex::Quantity::DigitalFullScale);
    EXPECT_DOUBLE_EQ(w.datasets[0].record.timeBase.sampleRateHz, 8000.0);
    EXPECT_DOUBLE_EQ(w.datasets[0].data.real[0], 0.5);
    EXPECT_DOUBLE_EQ(w.datasets[1].data.real[0], -0.5);
}
