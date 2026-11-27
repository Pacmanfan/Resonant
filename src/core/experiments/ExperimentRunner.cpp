#include "experiments/ExperimentRunner.h"

#include "experiments/ExperimentReport.h"
#include "foundation/Id.h"
#include "foundation/Log.h"
#include "foundation/Time.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <set>
#include <sstream>
#include <thread>

namespace resonant::experiments {

namespace {

double peakOf(const Stimulus& s) { return s.kind == StimulusKind::None || s.kind == StimulusKind::RfCarrier ? 0.0 : s.amplitude; }

// Releases the authorization and stops outputs whatever happens.
struct OutputGuard {
    std::vector<std::unique_ptr<OutputSession>> sessions;
    std::optional<OutputAuthorization> authorization;
    bool consumed = false;
    void stopAll()
    {
        for (auto& s : sessions)
            if (s)
                s->stop();
        sessions.clear();
    }
    ~OutputGuard()
    {
        stopAll();
        if (consumed && authorization)
            OutputAuthority::instance().release(*authorization);
    }
};

}  // namespace

OutputRequest outputRequestFor(const Experiment& e)
{
    OutputRequest r;
    r.experimentId = e.id;
    r.domain = e.domain;
    if (e.stimulus.kind != StimulusKind::None) {
        r.channels = e.stimulus.outputBindings;
        r.peakAmplitude = peakOf(e.stimulus);
        r.minFrequencyHz = std::min(e.stimulus.startHz, e.stimulus.endHz);
        r.maxFrequencyHz = std::max(e.stimulus.startHz, e.stimulus.endHz);
        if (e.stimulus.kind == StimulusKind::WhiteNoise) {
            r.minFrequencyHz = 0.0;
            r.maxFrequencyHz = e.acquisition.sampleRateHz / 2.0;
        }
        r.dutyCycle = e.stimulus.dutyCycle;
        size_t positions = 1;
        if (!e.positionerDeviceId.empty())
            if (auto list = scanPositions(e.positions))
                positions = std::max<size_t>(1, list.value().size());
        r.durationSeconds = e.stimulus.durationSeconds * std::max(1, e.acquisition.repetitions) * static_cast<double>(positions);
        if (e.stimulus.kind == StimulusKind::RfCarrier)
            r.transmitPowerDbm = e.stimulus.powerDbm;
    }
    if (!e.sweepDeviceId.empty()) {
        r.channels.push_back(e.sweepDeviceId);
        r.minFrequencyHz = r.channels.size() == 1 ? e.sweep.startHz : std::min(r.minFrequencyHz, e.sweep.startHz);
        r.maxFrequencyHz = std::max(r.maxFrequencyHz, e.sweep.stopHz);
        r.transmitPowerDbm = std::max(r.transmitPowerDbm, e.sweep.powerDbm);
    }
    return r;
}

ExperimentRunner::ExperimentRunner(DeviceRegistry& registry, const CalibrationLibrary& calibrations, MeasurementStore* store)
    : registry_(registry), calibrations_(calibrations), store_(store)
{
}

void ExperimentRunner::cancel() { cancel_.store(true); }

std::shared_ptr<const AcquisitionEngine> ExperimentRunner::activeAcquisition() const
{
    std::lock_guard lock(mutex_);
    return active_;
}

RunResult ExperimentRunner::run(const Experiment& e, const RunOptions& options)
{
    cancel_.store(false);
    RunResult result;
    ExecutionRecord& rec = result.record;
    rec.runId = "run-" + Id::generate().str().substr(0, 8);
    rec.startedUtc = utcNowIso8601();
    rec.operatorName = options.operatorName;
    auto started = std::chrono::steady_clock::now();
    auto stepStart = started;
    int currentStep = 0;
    std::string currentName;

    auto publish = [&](ExperimentState state, double fraction, const std::string& message, AcquisitionStats stats = {}) {
        result.state = state;
        if (options.progress)
            options.progress({state, currentStep, currentName, fraction, message, stats});
    };
    auto begin = [&](int step, const std::string& name, ExperimentState state) {
        currentStep = step;
        currentName = name;
        stepStart = std::chrono::steady_clock::now();
        publish(state, (step - 1) / 13.0, name);
    };
    auto log = [&](const std::string& status, const std::string& message) {
        rec.steps.push_back({currentStep, currentName, status, message,
                             std::chrono::duration<double>(std::chrono::steady_clock::now() - stepStart).count()});
    };
    auto finish = [&](ExperimentState state, const std::string& message) -> RunResult {
        result.state = state;
        rec.finalState = state;
        rec.message = message;
        rec.endedUtc = utcNowIso8601();
        for (const auto& item : result.raw)
            rec.measurementIds.push_back(item.record.id);
        for (const auto& item : result.derived)
            rec.measurementIds.push_back(item.record.id);
        publish(state, 1.0, message);
        if (state == ExperimentState::Failed)
            logWarning("experiments", e.name + ": " + message);
        return std::move(result);
    };
    auto failStep = [&](const std::string& message) -> RunResult {
        log("failed", message);
        return finish(ExperimentState::Failed, "step " + std::to_string(currentStep) + " (" + currentName + ") failed: " + message);
    };

    // 1. Validate.
    begin(1, "validate the experiment", ExperimentState::Validating);
    const ValidationReport validation = validateExperiment(e);
    if (!validation.ok()) {
        std::string all;
        for (const auto& err : validation.errors)
            all += (all.empty() ? "" : "; ") + err;
        return failStep(all);
    }
    for (const auto& w : validation.warnings)
        result.warnings.push_back(w);
    log(validation.warnings.empty() ? "ok" : "warning",
        validation.warnings.empty() ? "definition valid" : std::to_string(validation.warnings.size()) + " warning(s)");

    // 2. Devices.
    begin(2, "verify device availability", ExperimentState::Validating);
    std::set<std::string> deviceIds;
    for (const auto& b : e.channels)
        deviceIds.insert(b.deviceId);
    if (!e.positionerDeviceId.empty())
        deviceIds.insert(e.positionerDeviceId);
    if (!e.sweepDeviceId.empty())
        deviceIds.insert(e.sweepDeviceId);
    {
        auto known = registry_.knownDevices();
        const bool missing = std::any_of(deviceIds.begin(), deviceIds.end(), [&](const std::string& id) {
            return std::none_of(known.begin(), known.end(), [&](const DeviceInfo& d) { return d.id == id; });
        });
        if (missing)
            registry_.discover();
    }
    std::map<std::string, std::shared_ptr<HardwareDevice>> devices;
    bool anySimulated = false;
    for (const auto& id : deviceIds) {
        auto device = registry_.connect(id);
        if (!device)
            return failStep(device.message());
        anySimulated = anySimulated || device.value()->info().simulated;
        devices[id] = device.value();
    }
    log("ok", std::to_string(devices.size()) + " device(s) connected" + (anySimulated ? " (simulated: the data will be generated, not measured)" : ""));

    // 3. Capabilities and configurations.
    begin(3, "verify hardware capabilities", ExperimentState::Validating);
    std::map<std::string, DeviceConfiguration> configs;
    std::map<std::string, std::vector<const ChannelBinding*>> inputsOf, outputsOf;
    for (const auto& b : e.channels)
        (b.direction == ChannelDirection::Input ? inputsOf : outputsOf)[b.deviceId].push_back(&b);
    for (const auto& [id, device] : devices) {
        DeviceConfiguration c = e.devices.count(id) ? e.devices.at(id) : DeviceConfiguration{};
        c.sampleRateHz = e.acquisition.sampleRateHz;
        c.format = e.devices.count(id) ? e.devices.at(id).format : e.acquisition.format;
        c.blockFrames = e.acquisition.blockFrames;
        if (e.acquisition.centerFrequencyHz > 0.0)
            c.centerFrequencyHz = e.acquisition.centerFrequencyHz;
        if (e.acquisition.gainDb != 0.0)
            c.gainDb = e.acquisition.gainDb;
        c.inputChannels.clear();
        c.outputChannels.clear();
        for (const auto* b : inputsOf[id])
            c.inputChannels.push_back(b->channelId);
        for (const auto* b : outputsOf[id])
            c.outputChannels.push_back(b->channelId);
        const DeviceCapabilities caps = device->capabilities();
        for (const auto& ch : c.inputChannels)
            if (!caps.input(ch))
                return failStep(device->info().name + " has no input channel '" + ch + "'");
        for (const auto& ch : c.outputChannels)
            if (!caps.output(ch))
                return failStep(device->info().name + " has no output channel '" + ch + "'");
        if ((!c.inputChannels.empty() || !c.outputChannels.empty()) && !caps.supportsSampleRate(c.sampleRateHz))
            return failStep(device->info().name + " does not support " + std::to_string(c.sampleRateHz) + " Hz");
        if (id == e.positionerDeviceId && !caps.positioner)
            return failStep(device->info().name + " is not a positioner");
        if (id == e.sweepDeviceId && !caps.frequencySweep)
            return failStep(device->info().name + " cannot measure a frequency sweep");
        configs[id] = c;
    }
    log("ok", "channels, sample rate and roles match the devices' capabilities");

    // 4. Calibrations.
    begin(4, "verify calibration status", ExperimentState::Validating);
    const std::string now = utcNowIso8601();
    double bandLo = 0.0, bandHi = e.acquisition.sampleRateHz / 2.0;
    if (e.stimulus.kind != StimulusKind::None && e.stimulus.kind != StimulusKind::WhiteNoise && e.stimulus.kind != StimulusKind::RfCarrier) {
        bandLo = std::min(e.stimulus.startHz, e.stimulus.endHz);
        bandHi = std::max(e.stimulus.startHz, e.stimulus.endHz);
    }
    int calibrated = 0, uncalibrated = 0;
    for (const auto* b : e.inputBindings()) {
        const Sensor* s = e.sensor(b->sensorId);
        for (const std::string& calId : {s ? s->calibrationId : std::string(), b->calibrationId}) {
            if (calId.empty())
                continue;
            const CalibrationRecord* cal = calibrations_.find(calId);
            if (!cal)
                return failStep("calibration '" + calId + "' selected for '" + b->id + "' is not in the calibration library");
            const CalibrationStatus status = checkCalibration(*cal, bandLo, bandHi, now);
            for (const auto& w : status.warnings)
                result.warnings.push_back(w);
            if (!status.usable) {
                if (e.requireCalibration)
                    return failStep(status.problems.front());
                result.warnings.push_back(status.problems.front() + " (the data stays uncalibrated)");
            }
        }
        if (s && !s->calibrationId.empty())
            ++calibrated;
        else
            ++uncalibrated;
    }
    log(uncalibrated ? "warning" : "ok", std::to_string(calibrated) + " calibrated and " + std::to_string(uncalibrated) +
                                             " uncalibrated input(s); raw readings are recorded in instrument units either way");

    // 5. Synchronization.
    begin(5, "verify synchronization", ExperimentState::Validating);
    std::map<std::string, double> timingCorrection;  // binding id -> s
    for (const auto& [binding, delay] : e.synchronization.channelDelayCorrection)
        timingCorrection[binding] = delay;
    if (e.synchronization.strategy == "timing-calibration") {
        for (const auto& id : e.synchronization.timingCalibrationIds) {
            const CalibrationRecord* cal = calibrations_.find(id);
            if (!cal)
                return failStep("timing calibration '" + id + "' is not in the calibration library");
            for (const auto& b : e.channels)
                if (b.deviceId == cal->deviceId && b.channelId == cal->channelId)
                    timingCorrection[b.id] = cal->delaySeconds;
        }
    }
    std::set<std::string> inputDevices;
    for (const auto* b : e.inputBindings())
        inputDevices.insert(b->deviceId);
    std::string syncNote;
    if (e.synchronization.phaseSensitive) {
        if (inputDevices.size() == 1) {
            const auto caps = devices.at(*inputDevices.begin())->capabilities();
            if (!caps.sharedSampleClock)
                return failStep(devices.at(*inputDevices.begin())->info().name +
                                " does not sample its channels on one clock; phase comparison needs a timing calibration");
            syncNote = "single device: its channels share one sample clock (device capability)";
        } else if (e.synchronization.strategy == "shared-clock") {
            for (const auto& id : inputDevices) {
                const auto caps = devices.at(id)->capabilities();
                const std::string& source = configs.at(id).clockSource;
                const bool external = std::any_of(caps.clocks.begin(), caps.clocks.end(),
                                                  [&](const ClockSource& c) { return c.id == source && c.external; });
                if (!external)
                    return failStep("shared-clock strategy, but " + devices.at(id)->info().name + " runs on clock '" + source +
                                    "', not an external reference");
            }
            syncNote = "devices on a shared external reference clock";
        } else {
            syncNote = "timing calibration applied to " + std::to_string(timingCorrection.size()) + " channel(s)";
        }
    } else {
        syncNote = inputDevices.size() > 1 ? "not phase-sensitive: device clocks are independent and were not aligned"
                                           : "not phase-sensitive";
    }
    log("ok", syncNote);

    // 6. Safety.
    begin(6, "check safety limits and output authorization", ExperimentState::Validating);
    OutputGuard guard;
    if (e.hasPhysicalOutput()) {
        const OutputRequest request = outputRequestFor(e);
        const SafetyCheck check = checkOutput(e.safety, request);
        if (!check.allowed) {
            std::string all;
            for (const auto& v : check.violations)
                all += (all.empty() ? "" : "; ") + v;
            return failStep(all);
        }
        if (!options.authorization)
            return failStep("the experiment drives physical outputs (" + std::to_string(request.channels.size()) +
                            " channel(s)); it needs an explicit output authorization by an operator, and none was given. Nothing "
                            "was output.");
        const OutputAuthorization& a = *options.authorization;
        if (a.experimentId != e.id)
            return failStep("the output authorization was granted for another experiment");
        if (!OutputAuthority::instance().isValid(a))
            return failStep("the output authorization is not valid (expired, revoked, used, or the emergency stop is latched)");
        if (a.request.channels != request.channels || request.peakAmplitude > a.request.peakAmplitude + 1e-12 ||
            request.maxFrequencyHz > a.request.maxFrequencyHz || request.minFrequencyHz < a.request.minFrequencyHz ||
            request.transmitPowerDbm > a.request.transmitPowerDbm || request.durationSeconds > a.request.durationSeconds + 1e-9)
            return failStep("the experiment changed after the output was authorized; authorize it again");
        if (e.safety.requireInterlock)
            for (const auto* b : e.outputBindings())
                if (!devices.at(b->deviceId)->interlockClosed())
                    return failStep(devices.at(b->deviceId)->info().name + ": the hardware interlock is open");
        guard.authorization = a;
        rec.outputAuthorized = true;
        for (const auto& w : check.warnings)
            result.warnings.push_back(w);
        log("ok", "output authorized by " + a.operatorName + " at " + a.grantedUtc + "; within the safety limits");
    } else {
        log("skipped", "the experiment has no physical output");
    }
    publish(ExperimentState::Ready, 6.0 / 13.0, "ready");

    // 7. Configure.
    begin(7, "configure instruments", ExperimentState::Ready);
    for (const auto& [id, device] : devices)
        if (Status ok = device->configure(configs.at(id)); !ok)
            return failStep(ok.message());
    log("ok", "configured " + std::to_string(devices.size()) + " device(s)");

    // 8. Arm.
    begin(8, "arm triggers", ExperimentState::Ready);
    if (e.trigger.kind == TriggerKind::Level)
        return failStep("level triggering is not implemented in this version; use immediate, software or external triggering");
    if (e.trigger.kind == TriggerKind::External) {
        for (const auto& id : inputDevices) {
            const auto caps = devices.at(id)->capabilities();
            if (std::none_of(caps.triggers.begin(), caps.triggers.end(), [](const TriggerSource& t) { return t.hardware; }))
                return failStep(devices.at(id)->info().name + " has no hardware trigger input");
        }
    }
    log("ok", std::string("trigger: ") + std::string(toString(e.trigger.kind)));
    publish(ExperimentState::Armed, 8.0 / 13.0, "armed");

    // 9 + 10. Execute and record.
    begin(9, "execute acquisition and authorized outputs", ExperimentState::Running);
    std::vector<std::optional<Pose>> poses{std::nullopt};
    std::vector<double> angles{0.0};
    if (!e.positionerDeviceId.empty()) {
        angles.clear();
        auto positions = scanPositions(e.positions, &angles);
        if (!positions)
            return failStep(positions.message());
        poses.clear();
        for (const auto& p : positions.value())
            poses.push_back(p);
    }
    const double fs = e.acquisition.sampleRateHz;
    const size_t frames = static_cast<size_t>(std::llround(e.acquisition.durationSeconds * fs));
    // The command signal of every driven output binding (with its delay and gain).
    std::map<std::string, std::vector<double>> commands;
    if (e.stimulus.kind != StimulusKind::None && e.stimulus.kind != StimulusKind::RfCarrier) {
        for (const auto& id : e.stimulus.outputBindings) {
            const double delay = e.stimulus.channelDelaySeconds.count(id) ? e.stimulus.channelDelaySeconds.at(id) : 0.0;
            const double gain = e.stimulus.channelGain.count(id) ? e.stimulus.channelGain.at(id) : 1.0;
            auto s = generateStimulus(e.stimulus, fs, delay, gain);
            if (!s)
                return failStep(s.message());
            commands[id] = s.take();
        }
    }
    if (guard.authorization) {
        if (Status ok = OutputAuthority::instance().consume(*guard.authorization); !ok)
            return failStep(ok.message());
        guard.consumed = true;
    }
    bool failed = false, cancelled = false;
    std::string failure;
    DataQuality total;
    const int repetitions = std::max(1, e.acquisition.repetitions);
    const size_t runsTotal = poses.size() * static_cast<size_t>(repetitions);
    size_t runIndex = 0;
    std::vector<std::vector<ProcessingItem>> groups;  // raw items per (pose, repetition)

    // Network-analyser sweep.
    if (!e.sweepDeviceId.empty()) {
        auto* vna = devices.at(e.sweepDeviceId)->sweepInstrument();
        auto data = vna->sweep(e.sweep, *guard.authorization);
        if (!data)
            return failStep(data.message());
        ProcessingItem item;
        MeasurementRecord& r = item.record;
        r.id = "m-" + Id::generate().str();
        r.experimentId = e.id;
        r.runId = rec.runId;
        r.deviceId = e.sweepDeviceId;
        r.channelId = "s-parameters";
        r.domain = e.domain;
        r.kind = MeasurementKind::NetworkParameters;
        r.category = DataCategory::Raw;
        r.quantity = {Quantity::ScatteringParameter, std::nullopt, false};
        r.simulatedDevice = devices.at(e.sweepDeviceId)->info().simulated;
        r.createdUtc = utcNowIso8601();
        r.shape = {data.value().frequencyHz.size(), static_cast<size_t>(data.value().ports), static_cast<size_t>(data.value().ports)};
        r.acquisition = {{"sweep", {{"startHz", e.sweep.startHz}, {"stopHz", e.sweep.stopHz}, {"points", e.sweep.points},
                                    {"powerDbm", e.sweep.powerDbm}, {"ifBandwidthHz", e.sweep.ifBandwidthHz}}},
                         {"referenceImpedance", data.value().referenceImpedance}};
        r.note = data.value().note;
        item.data.axis = data.value().frequencyHz;
        for (const auto& row : data.value().s)
            item.data.complex.insert(item.data.complex.end(), row.begin(), row.end());
        if (store_) {
            if (Status ok = store_->write(r, item.data); !ok)
                return failStep(ok.message());
            r.dataRef = store_->dataRefFor(r);
        }
        groups.push_back({item});
        result.raw.push_back(item);
    }

    for (size_t p = 0; p < poses.size() && !failed && !cancelled && !inputDevices.empty(); ++p) {
        if (poses[p]) {
            if (Status ok = devices.at(e.positionerDeviceId)->positioner()->moveTo(*poses[p]); !ok) {
                failed = true;
                failure = ok.message();
                break;
            }
        }
        for (int rep = 0; rep < repetitions && !failed && !cancelled; ++rep, ++runIndex) {
            // Outputs (authorized above), started before the acquisition so both run on the same frames.
            guard.stopAll();
            for (const auto& [id, bindings] : outputsOf) {
                OutputSignal signal;
                signal.sampleRateHz = fs;
                for (const auto* b : bindings)
                    signal.channels.push_back(commands.count(b->id) ? commands.at(b->id) : std::vector<double>{});
                if (e.stimulus.kind == StimulusKind::RfCarrier) {
                    signal.carrierHz = e.stimulus.startHz;
                    signal.powerDbm = e.stimulus.powerDbm;
                }
                if (e.stimulus.kind == StimulusKind::None)
                    continue;
                auto session = devices.at(id)->startOutput(signal, *guard.authorization);
                if (!session) {
                    failed = true;
                    failure = session.message();
                    break;
                }
                guard.sessions.push_back(session.take());
            }
            if (failed)
                break;
            // One acquisition engine per input device.
            std::vector<std::shared_ptr<AcquisitionEngine>> engines;
            std::vector<std::string> engineDevice;
            for (const auto& id : inputDevices) {
                const auto caps = devices.at(id)->capabilities();
                AcquisitionPlan plan;
                plan.frames = frames;
                plan.queueBlocks = e.acquisition.queueBlocks;
                plan.maxBackpressureSeconds = e.acquisition.maxBackpressureSeconds;
                plan.clipFraction = e.acquisition.clipFraction;
                plan.keepInMemory = true;
                plan.complex = !caps.inputs.empty() && caps.input(inputsOf[id].front()->channelId)->complex;
                for (const auto* b : inputsOf[id]) {
                    const ChannelInfo* ch = caps.input(b->channelId);
                    ChannelPlan cp;
                    cp.rangeMin = ch->rangeMin;
                    cp.rangeMax = ch->rangeMax;
                    MeasurementRecord& r = cp.record;
                    r.id = "m-" + Id::generate().str();
                    r.experimentId = e.id;
                    r.runId = rec.runId;
                    r.bindingId = b->id;
                    r.sensorId = b->sensorId;
                    r.deviceId = id;
                    r.channelId = b->channelId;
                    const Sensor* s = e.sensor(b->sensorId);
                    r.domain = s ? s->domain : e.domain;
                    r.kind = MeasurementKind::TimeSeries;
                    r.category = DataCategory::Raw;
                    r.quantity = {ch->quantity, std::nullopt, false};
                    r.timeBase.sampleRateHz = fs;
                    r.timeBase.clockDomain = id + ":" + configs.at(id).clockSource;
                    r.timeBase.hardwareTimestamps = caps.hardwareTimestamps;
                    r.timeBase.timingCorrectionSeconds = timingCorrection.count(b->id) ? timingCorrection.at(b->id) : 0.0;
                    if (poses[p] && b->id == e.probeBindingId)
                        r.pose = *poses[p];
                    else if (s)
                        r.pose = s->pose;
                    r.nativeFormat = configs.at(id).format;
                    r.simulatedDevice = devices.at(id)->info().simulated;
                    r.uncertainty.add(quantizationComponent(quantizationStep(r.nativeFormat, std::max(std::abs(ch->rangeMin), ch->rangeMax))));
                    r.acquisition = {{"device", toJson(devices.at(id)->info())},
                                     {"configuration", toJson(configs.at(id))},
                                     {"frames", frames},
                                     {"repetition", rep + 1},
                                     {"position", p + 1},
                                     {"trigger", toString(e.trigger.kind)},
                                     {"synchronization", syncNote}};
                    if (poses[p] && e.positions.kind == ScanKind::Angular)
                        r.acquisition["angleDeg"] = angles[p];
                    r.createdUtc = utcNowIso8601();
                    plan.channels.push_back(cp);
                }
                auto engine = std::make_shared<AcquisitionEngine>(devices.at(id), plan, e.acquisition.recordToDisk ? store_ : nullptr);
                if (Status ok = engine->start(); !ok) {
                    failed = true;
                    failure = ok.message();
                    break;
                }
                engines.push_back(engine);
                engineDevice.push_back(id);
            }
            {
                std::lock_guard lock(mutex_);
                active_ = engines.empty() ? nullptr : engines.front();
            }
            // Wait, watching cancellation and the emergency stop.
            while (!engines.empty() && !failed) {
                bool allDone = true;
                for (auto& engine : engines)
                    allDone = allDone && engine->finished();
                if (allDone)
                    break;
                if (cancel_.load() || OutputAuthority::instance().emergencyStopActive()) {
                    guard.stopAll();  // outputs first
                    for (auto& engine : engines)
                        engine->requestStop();
                    cancelled = true;
                }
                const AcquisitionStats stats = engines.front()->stats();
                publish(ExperimentState::Running,
                        (8.0 + (static_cast<double>(runIndex) + (frames ? static_cast<double>(stats.framesRead) / frames : 0.0)) /
                                   static_cast<double>(runsTotal)) / 13.0,
                        "acquiring " + std::to_string(runIndex + 1) + " of " + std::to_string(runsTotal), stats);
                engines.front()->wait(std::chrono::milliseconds(50));
            }
            guard.stopAll();
            std::vector<ProcessingItem> group;
            // The commanded signals, for transfer functions from command to response.
            for (const auto& [bindingId, samples] : commands) {
                const ChannelBinding* b = e.binding(bindingId);
                const Actuator* act = b ? e.actuator(b->actuatorId) : nullptr;
                ProcessingItem item;
                MeasurementRecord& r = item.record;
                r.id = "m-" + Id::generate().str();
                r.experimentId = e.id;
                r.runId = rec.runId;
                r.bindingId = bindingId;
                r.deviceId = b ? b->deviceId : std::string();
                r.channelId = b ? b->channelId : std::string();
                r.domain = act ? act->domain : e.domain;
                r.kind = MeasurementKind::TimeSeries;
                r.category = DataCategory::Command;
                r.quantity = {act ? act->commandQuantity : Quantity::Voltage, std::nullopt, false};
                r.timeBase.sampleRateHz = fs;
                if (act)
                    r.pose = act->pose;
                r.acquisition = {{"repetition", rep + 1}, {"position", p + 1}};
                r.note = "the commanded output signal (what was sent to the channel), not a measurement of the actuator's "
                         "physical output";
                r.createdUtc = utcNowIso8601();
                item.data.real.assign(samples.begin(), samples.begin() + static_cast<long>(std::min(samples.size(), frames)));
                item.data.real.resize(frames, 0.0);
                r.shape = {frames};
                if (store_ && e.acquisition.recordToDisk) {
                    if (Status ok = store_->write(r, item.data); !ok) {
                        failed = true;
                        failure = ok.message();
                    }
                    r.dataRef = store_->dataRefFor(r);
                }
                group.push_back(item);
                result.raw.push_back(item);
            }
            for (auto& engine : engines) {
                AcquisitionOutput out = engine->takeOutput();
                if (out.failed) {
                    failed = true;
                    failure = out.failure;
                }
                if (out.cancelled)
                    cancelled = true;
                total.droppedFrames += out.quality.droppedFrames;
                total.gaps += out.quality.gaps;
                total.clippedSamples += out.quality.clippedSamples;
                total.saturated = total.saturated || out.quality.saturated;
                total.timestampDiscontinuities += out.quality.timestampDiscontinuities;
                total.invalidTimestamps += out.quality.invalidTimestamps;
                total.complete = total.complete && out.quality.complete;
                total.recordedFrames += out.quality.recordedFrames;
                total.expectedFrames += frames * out.records.size();
                for (const auto& err : out.quality.deviceErrors)
                    total.deviceErrors.push_back(err);
                for (const auto& n : out.quality.notes)
                    total.notes.push_back(n);
                for (size_t c = 0; c < out.records.size(); ++c) {
                    ProcessingItem item;
                    item.record = out.records[c];
                    if (!out.iq[c].empty())
                        item.data.complex = std::move(out.iq[c]);
                    else
                        item.data.real = std::move(out.real[c]);
                    group.push_back(item);
                    result.raw.push_back(group.back());
                }
            }
            {
                std::lock_guard lock(mutex_);
                active_.reset();
            }
            groups.push_back(std::move(group));
            if (OutputAuthority::instance().emergencyStopActive()) {
                failure = "emergency stop";
                cancelled = true;
            }
        }
    }
    guard.stopAll();
    result.quality = total;
    if (failed) {
        log("failed", failure);
        return finish(ExperimentState::Failed, "acquisition failed: " + failure + " (the data recorded before the failure is kept, "
                                               "marked incomplete, and was not processed)");
    }
    if (cancelled) {
        log("warning", "stopped by request" + std::string(OutputAuthority::instance().emergencyStopActive() ? " (emergency stop)" : ""));
        publish(ExperimentState::Stopping, 9.0 / 13.0, "stopping");
        return finish(ExperimentState::Cancelled, "cancelled; outputs stopped; the data recorded so far is kept, marked incomplete");
    }
    log("ok", std::to_string(result.raw.size()) + " raw data set(s) in " + std::to_string(groups.size()) + " acquisition(s)");
    begin(10, "record data", ExperimentState::Running);
    log(store_ && e.acquisition.recordToDisk ? "ok" : "skipped",
        store_ && e.acquisition.recordToDisk ? std::string("raw data written read-only to the ") + std::string(toString(store_->format())) +
                                                   " store at " + store_->root().string()
                                             : "not recorded to disk (kept in memory only)");

    // 11. Integrity.
    begin(11, "verify data integrity", ExperimentState::Stopping);
    publish(ExperimentState::Stopping, 11.0 / 13.0, "verifying");
    if (!total.clean()) {
        result.warnings.push_back("data quality: " + total.summary());
        log("warning", total.summary());
    } else {
        log("ok", total.summary());
    }

    // 12. Processing.
    begin(12, "process results", ExperimentState::Stopping);
    if (e.processing.empty()) {
        log("skipped", "the experiment defines no processing");
    } else {
        ProcessingContext ctx;
        ctx.calibrations = &calibrations_;
        for (const auto& s : e.sensors) {
            ctx.sensorQuantity[s.id] = s.measures;
            if (!s.calibrationId.empty())
                ctx.sensorCalibration[s.id] = s.calibrationId;
        }
        for (const auto& b : e.channels)
            ctx.bindingGain[b.id] = b.externalGain;
        for (const auto& group : groups) {
            std::map<std::string, ProcessingItem> byBinding;
            for (const auto& item : group)
                byBinding[item.record.bindingId.empty() ? item.record.channelId : item.record.bindingId] = item;
            auto derived = runChain(e.processing, byBinding, ctx);
            if (!derived) {
                log("failed", derived.message());
                return finish(ExperimentState::Failed, "processing failed: " + derived.message() + " (the raw data is complete and kept)");
            }
            for (auto& item : derived.value()) {
                if (store_) {
                    if (Status ok = store_->write(item.record, item.data); !ok) {
                        log("failed", ok.message());
                        return finish(ExperimentState::Failed, "storing processed data failed: " + ok.message());
                    }
                    item.record.dataRef = store_->dataRefFor(item.record);
                }
                result.derived.push_back(std::move(item));
            }
        }
        // Spatial data sets from single-frequency results at known poses.
        std::map<std::string, SpatialDataset> spatial;
        for (const auto& item : result.derived) {
            if (item.record.kind != MeasurementKind::Spectrum || item.data.complex.size() != 1 || !item.record.pose)
                continue;
            const std::string key = std::to_string(item.data.axis.front()) + "|" + item.record.quantity.unit() + "|" +
                                    item.record.history.back().parameters.dump();
            SpatialDataset& d = spatial[key];
            if (d.samples.empty()) {
                d.id = "spatial-" + Id::generate().str().substr(0, 8);
                d.experimentId = e.id;
                d.quantity = item.record.quantity;
                d.frequencyHz = item.data.axis.front();
                d.category = item.record.category == DataCategory::Calibrated ? DataCategory::Calibrated : DataCategory::Processed;
                d.frame = item.record.pose->frame;
                d.note = "single-frequency amplitudes of run " + rec.runId + (anySimulated ? " (simulated devices)" : "");
            }
            SpatialSample s;
            s.pose = *item.record.pose;
            s.value = item.data.complex.front();
            s.measurementId = item.record.id;
            s.angleDeg = item.record.acquisition.value("angleDeg", 0.0);
            const double u = item.record.uncertainty.combinedStandard(std::abs(s.value));
            s.standardUncertainty = u;
            d.samples.push_back(s);
        }
        for (auto& [key, d] : spatial)
            if (d.samples.size() > 1)
                result.spatial.push_back(std::move(d));
        log("ok", std::to_string(result.derived.size()) + " processed data set(s)" +
                      (result.spatial.empty() ? std::string() : ", " + std::to_string(result.spatial.size()) + " spatial data set(s)"));
    }

    // 13. Report.
    begin(13, "generate the report", ExperimentState::Stopping);
    result.state = ExperimentState::Completed;
    rec.finalState = ExperimentState::Completed;
    result.reportHtml = experimentReportHtml(e, result, calibrations_, {});
    if (store_) {
        const auto path = store_->root() / e.id / ("report-" + rec.runId + ".html");
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        std::ofstream(path) << result.reportHtml;
        rec.reportPath = (std::filesystem::path(e.id) / path.filename()).string();
    }
    log("ok", "report generated");
    return finish(ExperimentState::Completed,
                  result.warnings.empty() ? "completed" : "completed with " + std::to_string(result.warnings.size()) + " warning(s)");
}

ExperimentJob::ExperimentJob(std::shared_ptr<ExperimentRunner> runner, Experiment experiment, RunOptions options)
    : runner_(std::move(runner)), experiment_(std::move(experiment)), options_(std::move(options))
{
}

JobEnd ExperimentJob::run(JobContext& context)
{
    RunOptions options = options_;
    auto user = options.progress;
    options.progress = [&, user](const RunProgress& p) {
        context.reportProgress(p.fraction, p.stepName + (p.message.empty() || p.message == p.stepName ? "" : ": " + p.message));
        if (context.cancelRequested())
            runner_->cancel();
        if (user)
            user(p);
    };
    result_ = runner_->run(experiment_, options);
    switch (result_.state) {
    case ExperimentState::Completed: return JobEnd::completed(result_.record.message);
    case ExperimentState::Cancelled: return JobEnd::cancelled();
    default: return JobEnd::failed(result_.record.message);
    }
}

}  // namespace resonant::experiments
