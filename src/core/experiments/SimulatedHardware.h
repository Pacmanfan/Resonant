#pragma once

// Simulated instruments for development and automated testing without
// hardware. Their data is GENERATED from stated synthetic models, never
// measured: every record from them is marked simulatedDevice, and reports
// say so.
//
// The devices behave like instruments, not like constant sources: they
// stream blocks on a frame clock, need configuring, refuse output without
// an authorization, and inject the faults real equipment has (noise, gain
// mismatch, timing and phase offsets, clipping, dropped samples, timestamp
// glitches, disconnection, acquisition errors) when their options ask for it.
//
//   sim:audio-8x2   8-in / 2-out audio interface (FS units). Output 0 drives a
//                   simulated loudspeaker; inputs 0-6 are microphones in a free
//                   field (1/r spreading, delay r/c, speaker high-pass), input 7
//                   is an electrical loopback of output 0.
//   sim:daq-4x1     vibration DAQ (V). Output 0 drives a shaker on a structure
//                   with modal accelerances; input 0 is a load cell, inputs 1-3
//                   accelerometers.
//   sim:rf-rx-4     4-channel I/Q receiver (FS). Receives the carrier of
//                   sim:siggen through a path loss, with per-channel phase and
//                   gain offsets; coherent LO or independent LOs.
//   sim:siggen      RF signal generator (carrier and power).
//   sim:vna-2       2-port vector network analyser measuring a simulated DUT
//                   (series-RLC antenna on port 1, lossy line port 1 -> 2).
//   sim:positioner  moves the probe (an acoustic microphone or the RF probe
//                   antenna) to scan positions.
//
// Options (DeviceConfiguration.options) are listed in
// docs/experiments/HARDWARE_INTEGRATIONS.md.
#include "experiments/Hardware.h"

#include <map>
#include <memory>
#include <mutex>
#include <optional>

namespace resonant::experiments {

// What the simulated devices share: active RF carriers and the probe pose.
class SimulatedBench {
public:
    struct Carrier {
        double frequencyHz = 0.0;
        double powerDbm = -1000.0;
    };
    // A carrier exists only while its authorization is valid.
    void setCarrier(const std::string& deviceId, std::optional<Carrier> carrier, const OutputAuthorization& authorization);
    std::vector<Carrier> carriers() const;
    void setProbe(const Pose& pose);
    std::optional<Pose> probe() const;

private:
    mutable std::mutex mutex_;
    mutable std::map<std::string, std::pair<Carrier, OutputAuthorization>> carriers_;
    std::optional<Pose> probe_;
};

class SimulatedDriver final : public DeviceDriver {
public:
    SimulatedDriver();
    std::string name() const override { return "simulated"; }
    std::string version() const override { return "1.0"; }
    ConnectionType connection() const override { return ConnectionType::Simulated; }
    DriverAvailability availability() const override;
    Result<std::vector<DeviceInfo>> discover() override;
    Result<std::unique_ptr<HardwareDevice>> open(const DeviceInfo& info) override;
    std::shared_ptr<SimulatedBench> bench() const { return bench_; }

private:
    std::shared_ptr<SimulatedBench> bench_;
};

// The model parameters the simulated devices use by default (so tests can
// compare what a procedure recovers with what was put in).
namespace simulated {
inline constexpr double kSpeedOfSound = 343.0;        // m/s
inline constexpr double kSpeakerPaPerFsAt1m = 2.0;    // Pa at 1 m per full-scale drive (flat band)
inline constexpr double kSpeakerHighPassHz = 80.0;    // second-order, Q 0.707
inline constexpr double kMicFsPerPa = 0.0158;         // -36 dBFS/Pa
inline constexpr double kShakerNewtonPerVolt = 10.0;
inline constexpr double kLoadCellVoltPerNewton = 0.0225;
inline constexpr double kAccelerometerVoltPerMs2 = 0.0102;  // about 100 mV/g
inline constexpr double kRfFullScaleDbm = -10.0;      // input power for |x| = 1 FS at 0 dB gain
inline constexpr double kRfPathGainDb = -40.0;
}  // namespace simulated

}  // namespace resonant::experiments
