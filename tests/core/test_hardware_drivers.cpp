// Real-hardware drivers (REQ-CHAT-20261126-008, RES-EXP-016): ALSA capture
// and playback against the ALSA "null" device (no microphone is opened and
// no sound is played: null discards playback and captures silence), and
// SoapySDR discovery. Skipped when the build has no such driver.
#include "experiments/ExperimentRunner.h"
#include "hardware/HardwareDrivers.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <thread>

using namespace resonant;
using namespace resonant::experiments;

namespace {

bool built(const std::string& name)
{
    for (const auto& d : hardware::driverBuildInfo())
        if (d.name == name)
            return d.built;
    return false;
}

}  // namespace

TEST(HardwareDrivers, BuildInformationNamesEveryDriver)
{
    const auto info = hardware::driverBuildInfo();
    ASSERT_EQ(info.size(), 3u);
    EXPECT_TRUE(info[0].built);  // simulated
    DeviceRegistry registry;
    hardware::registerHardwareDrivers(registry);
    EXPECT_GE(registry.drivers().size(), 1u);
}

TEST(HardwareDrivers, AlsaNullDeviceCapturesAndPlaysBackOnlyWithAuthorization)
{
    if (!built("alsa"))
        GTEST_SKIP() << "built without ALSA";
    DeviceRegistry registry;
    hardware::registerHardwareDrivers(registry);
    const auto report = registry.discover();
    const auto devices = registry.knownDevices();
    const bool hasNull = std::any_of(devices.begin(), devices.end(), [](const DeviceInfo& d) { return d.id == "alsa:null"; });
    if (!hasNull)
        GTEST_SKIP() << "ALSA lists no null device here";
    auto device = registry.connect("alsa:null");
    ASSERT_TRUE(device.ok()) << device.message();
    const auto caps = device.value()->capabilities();
    ASSERT_FALSE(caps.inputs.empty());
    ASSERT_FALSE(caps.outputs.empty());
    EXPECT_TRUE(caps.supportsSampleRate(48000.0));
    DeviceConfiguration c;
    c.sampleRateHz = 48000.0;
    c.format = SampleFormat::Int16;
    c.inputChannels = {"in0", "in1"};
    c.outputChannels = {"out0"};
    c.blockFrames = 1024;
    ASSERT_TRUE(device.value()->configure(c).ok());
    // Capture: the null device delivers silence, in full-scale units.
    auto session = device.value()->startAcquisition(4800);
    ASSERT_TRUE(session.ok()) << session.message();
    size_t frames = 0;
    bool end = false;
    double peak = 0.0;
    while (!end) {
        auto block = session.value()->read(std::chrono::milliseconds(2000));
        ASSERT_TRUE(block.ok()) << block.message();
        ASSERT_EQ(block.value().channels, 2u);
        frames += block.value().frames;
        for (double v : block.value().samples)
            peak = std::max(peak, std::abs(v));
        end = block.value().endOfStream;
    }
    EXPECT_EQ(frames, 4800u);
    EXPECT_EQ(peak, 0.0);
    session.value()->stop();
    // Playback: refused without an authorization, accepted with one.
    OutputSignal signal;
    signal.sampleRateHz = 48000.0;
    signal.channels = {std::vector<double>(4800, 0.0)};
    EXPECT_FALSE(device.value()->startOutput(signal, OutputAuthorization{}).ok());
    OutputRequest request;
    request.experimentId = "alsa-null";
    request.channels = {"ch-out"};
    request.peakAmplitude = 0.0;
    request.domain = PhysicsDomain::Acoustic;
    SafetyLimits limits;
    limits.maxOutputAmplitude = 0.1;
    limits.allowedOutputChannels = {"ch-out"};
    auto a = OutputAuthority::instance().grant(request, limits, "test", "null device: nothing is played");
    ASSERT_TRUE(a.ok()) << a.message();
    auto playback = device.value()->startOutput(signal, a.value());
    ASSERT_TRUE(playback.ok()) << playback.message();
    for (int i = 0; i < 200 && playback.value()->running(); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    playback.value()->stop();
    EXPECT_GE(playback.value()->framesWritten(), 4096u);
    OutputAuthority::instance().release(a.value());
}

TEST(HardwareDrivers, SoapySdrDiscoveryReportsWithoutClaimingStreaming)
{
    if (!built("soapysdr"))
        GTEST_SKIP() << "built without SoapySDR";
    DeviceRegistry registry;
    hardware::registerHardwareDrivers(registry);
    const auto report = registry.discover();  // no SDR needs to be attached; discovery must not fail the registry
    for (const auto& d : registry.knownDevices())
        if (d.driver == "soapysdr") {
            auto device = registry.connect(d.id);
            ASSERT_TRUE(device.ok()) << device.message();
            EXPECT_FALSE(device.value()->startAcquisition(1000).ok());
        }
    SUCCEED() << report.devices.size() << " device(s) found";
}
