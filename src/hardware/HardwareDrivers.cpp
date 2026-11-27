#include "HardwareDrivers.h"

#include "AlsaDriver.h"
#include "SoapyDriver.h"
#include "experiments/SimulatedHardware.h"

namespace resonant::hardware {

std::vector<DriverBuildInfo> driverBuildInfo()
{
    return {
        {"simulated", true, "part of Resonant",
         "simulated audio interface, vibration DAQ, RF receiver, signal generator, VNA and positioner (generated data)"},
        {"alsa", alsaBuilt(), "ALSA libasound (LGPL-2.1, dynamically linked)",
         "capture and playback on Linux audio interfaces (PCM devices: hw, plughw, default, null, ...)"},
        {"soapysdr", soapyBuilt(), "SoapySDR (BSL-1.0, dynamically linked)",
         "discovery and capability inspection of SDRs; streaming is not implemented in this version"},
    };
}

void registerHardwareDrivers(experiments::DeviceRegistry& registry)
{
    registry.registerDriver(std::make_shared<experiments::SimulatedDriver>());
    if (alsaBuilt())
        registry.registerDriver(makeAlsaDriver());
    if (soapyBuilt())
        registry.registerDriver(makeSoapyDriver());
}

}  // namespace resonant::hardware
