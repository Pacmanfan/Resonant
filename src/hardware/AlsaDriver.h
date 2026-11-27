#pragma once

// ALSA: capture and playback on Linux audio interfaces.
//
// Discovery lists the PCM device names ALSA's configuration provides
// ("hw:...", "plughw:...", "default", "null", ...) without opening them;
// capabilities are probed when a device is opened. Samples are read as
// 16-bit, 32-bit or float interleaved frames and delivered as readings in
// full-scale units (FS, -1 ... 1): ALSA does not know volts or pascals.
// An overrun is recovered and the lost frames are estimated from the elapsed
// time (ALSA does not count them); the gap is reported, not hidden.
// Playback needs a valid output authorization and stops at once when it ends.
#include "experiments/Hardware.h"

#include <memory>

namespace resonant::hardware {

bool alsaBuilt();
std::shared_ptr<experiments::DeviceDriver> makeAlsaDriver();

}  // namespace resonant::hardware
