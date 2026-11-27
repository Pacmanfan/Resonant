#pragma once

// SoapySDR: software-defined radios. This version discovers devices and
// reports their capabilities (channels, sample rates, tuning range, clock
// sources). Streaming is not implemented: no SDR was available to test it,
// and an untested receive path is not offered as working.
#include "experiments/Hardware.h"

#include <memory>

namespace resonant::hardware {

bool soapyBuilt();
std::shared_ptr<experiments::DeviceDriver> makeSoapyDriver();

}  // namespace resonant::hardware
