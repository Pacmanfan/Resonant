#pragma once

// Physical-output safety: limits, explicit authorization and the emergency
// stop.
//
//  * Nothing in Resonant drives a speaker, transducer, shaker, piezo
//    actuator or RF transmitter without an OutputAuthorization, and the
//    only way to get one is OutputAuthority::grant: an explicit act of a
//    named operator for one experiment, checked against the experiment's
//    safety limits. Opening a project or loading an experiment never grants
//    one; authorizations are never saved.
//  * An authorization covers one run: unused, it expires (by default after
//    ten minutes); a run consumes it when its outputs start and releases it
//    when it ends, after which it is spent.
//  * The emergency stop is latched: once triggered, every output session
//    stops at its next block and no authorization is granted until an
//    operator resets it.
//  * A software limit is not a hardware interlock, and a warning about
//    exposure or radio regulations does not establish compliance with them.
#include "foundation/Result.h"
#include "foundation/Types.h"

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace resonant::experiments {

struct SafetyLimits {
    double maxOutputAmplitude = 0.0;     // peak command amplitude, in the output unit; 0 = no output permitted
    std::string amplitudeUnit = "V";
    double minFrequencyHz = 0.0;         // permitted stimulus band; 0 / 0 = not restricted
    double maxFrequencyHz = 0.0;
    double maxDutyCycle = 1.0;           // fraction of time an output may be on
    double maxDurationSeconds = 0.0;     // longest permitted output; 0 = not restricted
    std::vector<std::string> allowedOutputChannels;  // binding ids; empty = none
    bool requireInterlock = false;       // the device must report a closed hardware interlock
    double maxTransmitPowerDbm = -1000.0;  // RF: highest permitted transmit power; -1000 = no RF transmission
    double exposureWarningSplDb = 85.0;  // acoustic: warn above this level at 1 m (an estimate, not a dosimeter)
    std::string regulatoryNote;          // e.g. the licence or test-site conditions an RF run relies on

    friend bool operator==(const SafetyLimits&, const SafetyLimits&) = default;
};

nlohmann::json toJson(const SafetyLimits& limits);
Result<SafetyLimits> safetyLimitsFromJson(const nlohmann::json& value);

// What a run intends to output.
struct OutputRequest {
    std::string experimentId;
    PhysicsDomain domain = PhysicsDomain::Acoustic;
    std::vector<std::string> channels;   // binding ids
    double peakAmplitude = 0.0;          // in limits.amplitudeUnit
    double minFrequencyHz = 0.0, maxFrequencyHz = 0.0;
    double dutyCycle = 1.0;
    double durationSeconds = 0.0;
    double transmitPowerDbm = -1000.0;   // RF only
    double estimatedSplDb = 0.0;         // acoustic, at 1 m, if known (0 = unknown)
};

struct SafetyCheck {
    bool allowed = false;
    std::vector<std::string> violations;  // any one forbids the output
    std::vector<std::string> warnings;    // shown to the operator before authorizing
};

SafetyCheck checkOutput(const SafetyLimits& limits, const OutputRequest& request);

// Proof that an operator authorized one run's outputs. Copyable (it is only
// an identifier); validity is decided by the OutputAuthority.
struct OutputAuthorization {
    std::string token;
    std::string experimentId;
    std::string operatorName;
    std::string grantedUtc;
    std::chrono::steady_clock::time_point expires;
    OutputRequest request;
};

struct SafetyEvent {
    std::string timeUtc;
    std::string kind;     // granted, refused, consumed, revoked, emergency-stop, reset, output-stopped, device-error
    std::string message;
};

class OutputAuthority {
public:
    static OutputAuthority& instance();

    // The explicit operator action. Fails when the request violates the
    // limits, when the operator name or acknowledgement is empty, or while
    // the emergency stop is latched.
    Result<OutputAuthorization> grant(const OutputRequest& request, const SafetyLimits& limits, const std::string& operatorName,
                                      const std::string& acknowledgement,
                                      std::chrono::seconds validity = std::chrono::seconds(600));
    // Valid: granted and not yet used (before its expiry), or in use by the
    // run that consumed it; never after release, revocation or an emergency stop.
    // Output devices check this for every block they send.
    bool isValid(const OutputAuthorization& authorization) const;
    // A run calls this once when it starts its outputs; a second call fails.
    Status consume(const OutputAuthorization& authorization);
    // The run has ended: the authorization is spent.
    void release(const OutputAuthorization& authorization);
    void revokeAll(const std::string& reason);

    void emergencyStop(const std::string& reason);
    bool emergencyStopActive() const { return emergencyStop_.load(); }
    // An explicit operator action; clears the latch, grants nothing.
    void resetEmergencyStop(const std::string& operatorName);

    void record(const std::string& kind, const std::string& message);
    std::vector<SafetyEvent> events() const;

private:
    OutputAuthority() = default;
    mutable std::mutex mutex_;
    std::set<std::string> active_;   // granted, not used yet
    std::set<std::string> inUse_;    // consumed by a running run
    std::set<std::string> consumed_; // spent
    std::vector<SafetyEvent> events_;
    std::atomic<bool> emergencyStop_{false};
};

}  // namespace resonant::experiments
