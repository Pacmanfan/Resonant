#pragma once

// resonant_hardware: drivers for real instruments, behind the hardware
// abstraction of src/core/experiments (Hardware.h). The core does not depend
// on this library or on any instrument API; the application and the tests
// register the drivers that were built.
//
//   simulated   always (from the core)
//   alsa        Linux audio interfaces through ALSA (when built with ALSA)
//   soapysdr    software-defined radios through SoapySDR (when built with it):
//               discovery and capabilities only
#include "experiments/Hardware.h"

#include <memory>
#include <string>
#include <vector>

namespace resonant::hardware {

struct DriverBuildInfo {
    std::string name;
    bool built = false;
    std::string library;   // and its licence
    std::string scope;     // what the driver does in this version
};

// What this build contains, built or not (for the user interface and the docs).
std::vector<DriverBuildInfo> driverBuildInfo();

// Registers the simulated driver and every built hardware driver.
void registerHardwareDrivers(experiments::DeviceRegistry& registry);

}  // namespace resonant::hardware
