#include "experiments/Safety.h"

#include "foundation/Id.h"
#include "foundation/Log.h"
#include "foundation/Time.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace resonant::experiments {

nlohmann::json toJson(const SafetyLimits& l)
{
    return {{"maxOutputAmplitude", l.maxOutputAmplitude},
            {"amplitudeUnit", l.amplitudeUnit},
            {"minFrequencyHz", l.minFrequencyHz},
            {"maxFrequencyHz", l.maxFrequencyHz},
            {"maxDutyCycle", l.maxDutyCycle},
            {"maxDurationSeconds", l.maxDurationSeconds},
            {"allowedOutputChannels", l.allowedOutputChannels},
            {"requireInterlock", l.requireInterlock},
            {"maxTransmitPowerDbm", l.maxTransmitPowerDbm},
            {"exposureWarningSplDb", l.exposureWarningSplDb},
            {"regulatoryNote", l.regulatoryNote}};
}

Result<SafetyLimits> safetyLimitsFromJson(const nlohmann::json& v)
{
    SafetyLimits l;
    if (v.is_null())
        return l;
    if (!v.is_object())
        return Result<SafetyLimits>::failure("safety limits must be an object");
    l.maxOutputAmplitude = v.value("maxOutputAmplitude", 0.0);
    l.amplitudeUnit = v.value("amplitudeUnit", std::string("V"));
    l.minFrequencyHz = v.value("minFrequencyHz", 0.0);
    l.maxFrequencyHz = v.value("maxFrequencyHz", 0.0);
    l.maxDutyCycle = v.value("maxDutyCycle", 1.0);
    l.maxDurationSeconds = v.value("maxDurationSeconds", 0.0);
    l.allowedOutputChannels = v.value("allowedOutputChannels", std::vector<std::string>{});
    l.requireInterlock = v.value("requireInterlock", false);
    l.maxTransmitPowerDbm = v.value("maxTransmitPowerDbm", -1000.0);
    l.exposureWarningSplDb = v.value("exposureWarningSplDb", 85.0);
    l.regulatoryNote = v.value("regulatoryNote", std::string());
    if (!(l.maxOutputAmplitude >= 0.0))
        return Result<SafetyLimits>::failure("the output amplitude limit must not be negative");
    if (!(l.maxDutyCycle >= 0.0 && l.maxDutyCycle <= 1.0))
        return Result<SafetyLimits>::failure("the duty-cycle limit must lie in [0, 1]");
    if (l.maxFrequencyHz > 0.0 && l.maxFrequencyHz < l.minFrequencyHz)
        return Result<SafetyLimits>::failure("the upper frequency limit is below the lower one");
    return l;
}

SafetyCheck checkOutput(const SafetyLimits& l, const OutputRequest& r)
{
    SafetyCheck c;
    std::ostringstream text;
    const bool rfOnly = r.domain == PhysicsDomain::Electromagnetic && r.peakAmplitude == 0.0 && r.transmitPowerDbm > -1000.0;
    if (rfOnly) {
        // An RF source or a network analyser: limited by its transmit power (below), not by a command amplitude.
    } else if (l.maxOutputAmplitude <= 0.0)
        c.violations.push_back("the experiment's safety limits permit no output (maximum amplitude 0)");
    else if (r.peakAmplitude > l.maxOutputAmplitude)
        c.violations.push_back("peak amplitude " + std::to_string(r.peakAmplitude) + " " + l.amplitudeUnit + " exceeds the limit " +
                               std::to_string(l.maxOutputAmplitude) + " " + l.amplitudeUnit);
    if (l.maxFrequencyHz > 0.0 && (r.minFrequencyHz < l.minFrequencyHz || r.maxFrequencyHz > l.maxFrequencyHz))
        c.violations.push_back("the stimulus band " + std::to_string(r.minFrequencyHz) + " ... " + std::to_string(r.maxFrequencyHz) +
                               " Hz lies outside the permitted band " + std::to_string(l.minFrequencyHz) + " ... " +
                               std::to_string(l.maxFrequencyHz) + " Hz");
    if (r.dutyCycle > l.maxDutyCycle)
        c.violations.push_back("duty cycle " + std::to_string(r.dutyCycle) + " exceeds the limit " + std::to_string(l.maxDutyCycle));
    if (l.maxDurationSeconds > 0.0 && r.durationSeconds > l.maxDurationSeconds)
        c.violations.push_back("output duration " + std::to_string(r.durationSeconds) + " s exceeds the limit " +
                               std::to_string(l.maxDurationSeconds) + " s");
    for (const auto& channel : r.channels)
        if (std::find(l.allowedOutputChannels.begin(), l.allowedOutputChannels.end(), channel) == l.allowedOutputChannels.end())
            c.violations.push_back("output channel '" + channel + "' is not in the experiment's permitted output channels");
    if (r.channels.empty())
        c.violations.push_back("the request names no output channel");
    if (r.domain == PhysicsDomain::Electromagnetic) {
        if (l.maxTransmitPowerDbm <= -1000.0)
            c.violations.push_back("the experiment's limits permit no RF transmission (no transmit-power limit set)");
        else if (r.transmitPowerDbm > l.maxTransmitPowerDbm)
            c.violations.push_back("transmit power " + std::to_string(r.transmitPowerDbm) + " dBm exceeds the limit " +
                                   std::to_string(l.maxTransmitPowerDbm) + " dBm");
        c.warnings.push_back("RF transmission: a software power limit does not establish regulatory compliance. Transmit only "
                             "into a load, a shielded enclosure, or under the licence or test-site conditions that apply." +
                             (l.regulatoryNote.empty() ? std::string() : " Stated conditions: " + l.regulatoryNote));
    }
    if (r.domain == PhysicsDomain::Acoustic) {
        if (r.estimatedSplDb > l.exposureWarningSplDb)
            c.warnings.push_back("estimated sound pressure level " + std::to_string(r.estimatedSplDb) + " dB at 1 m exceeds " +
                                 std::to_string(l.exposureWarningSplDb) + " dB: protect hearing; this is an estimate, not a "
                                 "dosimeter reading");
        if (r.maxFrequencyHz > 20000.0)
            c.warnings.push_back("ultrasonic output: inaudible does not mean harmless; high-intensity ultrasound can injure tissue "
                                 "and damage equipment");
        else
            c.warnings.push_back("acoustic output: check the amplifier gain and the transducer rating before running");
    }
    if (r.domain == PhysicsDomain::Mechanical)
        c.warnings.push_back("mechanical excitation: check the shaker or actuator rating, the stinger and the structure's load "
                             "limits; a resonance can amplify the response far above the drive level");
    if (l.requireInterlock)
        c.warnings.push_back("the experiment requires a closed hardware interlock; the device must report it before output starts");
    c.allowed = c.violations.empty();
    return c;
}

OutputAuthority& OutputAuthority::instance()
{
    static OutputAuthority authority;
    return authority;
}

Result<OutputAuthorization> OutputAuthority::grant(const OutputRequest& request, const SafetyLimits& limits,
                                                   const std::string& operatorName, const std::string& acknowledgement,
                                                   std::chrono::seconds validity)
{
    if (emergencyStop_.load()) {
        record("refused", "emergency stop is latched; reset it before authorizing output");
        return Result<OutputAuthorization>::failure("the emergency stop is latched; an operator must reset it first");
    }
    if (operatorName.empty() || acknowledgement.empty()) {
        record("refused", "authorization without an operator name or acknowledgement");
        return Result<OutputAuthorization>::failure("output authorization needs the operator's name and an explicit acknowledgement");
    }
    const SafetyCheck check = checkOutput(limits, request);
    if (!check.allowed) {
        std::string all;
        for (const auto& v : check.violations)
            all += (all.empty() ? "" : "; ") + v;
        record("refused", request.experimentId + ": " + all);
        return Result<OutputAuthorization>::failure("output not authorized: " + all);
    }
    OutputAuthorization a;
    a.token = Id::generate().str();
    a.experimentId = request.experimentId;
    a.operatorName = operatorName;
    a.grantedUtc = utcNowIso8601();
    a.expires = std::chrono::steady_clock::now() + validity;
    a.request = request;
    {
        std::lock_guard lock(mutex_);
        active_.insert(a.token);
    }
    record("granted", request.experimentId + " by " + operatorName + " (" + acknowledgement + ")");
    return a;
}

bool OutputAuthority::isValid(const OutputAuthorization& a) const
{
    if (emergencyStop_.load() || a.token.empty())
        return false;
    std::lock_guard lock(mutex_);
    if (inUse_.count(a.token))
        return true;
    return active_.count(a.token) && std::chrono::steady_clock::now() <= a.expires;
}

Status OutputAuthority::consume(const OutputAuthorization& a)
{
    {
        std::lock_guard lock(mutex_);
        const bool usable = !emergencyStop_.load() && active_.count(a.token) && std::chrono::steady_clock::now() <= a.expires;
        if (!usable)
            return Status::failure("the output authorization is not valid (expired, revoked, already used, or the emergency stop "
                                   "is latched)");
        active_.erase(a.token);
        inUse_.insert(a.token);
    }
    record("consumed", a.experimentId + " (authorized by " + a.operatorName + ")");
    return Status::success();
}

void OutputAuthority::release(const OutputAuthorization& a)
{
    {
        std::lock_guard lock(mutex_);
        if (!inUse_.erase(a.token) && !active_.erase(a.token))
            return;
        consumed_.insert(a.token);
    }
    record("released", a.experimentId + ": the run ended; the authorization is spent");
}

void OutputAuthority::revokeAll(const std::string& reason)
{
    {
        std::lock_guard lock(mutex_);
        active_.clear();
        for (const auto& t : inUse_)
            consumed_.insert(t);
        inUse_.clear();
    }
    record("revoked", reason);
}

void OutputAuthority::emergencyStop(const std::string& reason)
{
    emergencyStop_.store(true);
    revokeAll("emergency stop: " + reason);
    record("emergency-stop", reason);
    logWarning("safety", "EMERGENCY STOP: " + reason);
}

void OutputAuthority::resetEmergencyStop(const std::string& operatorName)
{
    emergencyStop_.store(false);
    record("reset", "emergency stop reset by " + operatorName);
}

void OutputAuthority::record(const std::string& kind, const std::string& message)
{
    std::lock_guard lock(mutex_);
    events_.push_back({utcNowIso8601(), kind, message});
}

std::vector<SafetyEvent> OutputAuthority::events() const
{
    std::lock_guard lock(mutex_);
    return events_;
}

}  // namespace resonant::experiments
