#include "ExperimentsController.h"

#include "DocumentController.h"
#include "acoustics/AcousticExperiments.h"
#include "commands/SceneCommands.h"
#include "electromagnetics/RfExperiments.h"
#include "experiments/ExperimentArchive.h"
#include "experiments/ExperimentReport.h"
#include "foundation/Id.h"
#include "foundation/Time.h"
#include "geometry/Geometry.h"
#include "hardware/HardwareDrivers.h"
#include "mechanics/MechanicalExperiments.h"
#include "render/FieldLayer.h"

#include <QFile>
#include <QFileInfo>
#include <QMetaObject>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <mutex>

using namespace resonant;
using namespace resonant::experiments;

namespace {

std::mutex& progressMutex()
{
    static std::mutex m;
    return m;
}

std::shared_ptr<const TriangleMesh> markerCube()
{
    static const std::shared_ptr<const TriangleMesh> mesh = [] {
        Geometry box = Geometry::create("box").take();
        for (const char* key : {"width", "depth", "height"})
            [[maybe_unused]] const Status status = box.setParameter(key, 1.0);
        return std::make_shared<const TriangleMesh>(box.tessellate());
    }();
    return mesh;
}

render::DrawItem markerAt(const Pose& p, double size, const Eigen::Vector4f& colour)
{
    render::DrawItem item;
    item.mesh = markerCube();
    item.model = Mat4::Identity();
    item.model.block<3, 3>(0, 0) *= size;
    item.model.block<3, 1>(0, 3) = Vec3(p.x, p.y, p.z);
    item.color = colour;
    return item;
}

struct TemplateEntry {
    const char* id;
    const char* title;
};

constexpr TemplateEntry kTemplates[] = {
    {"acoustic.speaker-characterization", "Acoustic: speaker characterization"},
    {"acoustic.field-mapping", "Acoustic: field mapping (2 kHz)"},
    {"acoustic.focusing-validation", "Acoustic: focusing validation"},
    {"acoustic.ultrasonic-element", "Acoustic: ultrasonic element characterization"},
    {"mechanical.modal-test", "Mechanical: structural modal test"},
    {"mechanical.vibration-characterization", "Mechanical: vibration characterization (120 Hz)"},
    {"mechanical.actuator-calibration", "Mechanical: shaker calibration"},
    {"rf.antenna-impedance", "RF: antenna impedance (VNA)"},
    {"rf.array-calibration", "RF: array channel calibration"},
    {"rf.radiation-pattern", "RF: radiation pattern"},
};

Result<Experiment> fromTemplate(const std::string& templateId, const std::string& id)
{
    if (templateId == "acoustic.speaker-characterization")
        return acoustics::speakerCharacterizationExperiment(id);
    if (templateId == "acoustic.field-mapping")
        return acoustics::fieldMappingExperiment(id, 2000.0);
    if (templateId == "acoustic.focusing-validation")
        return acoustics::focusingValidationExperiment(id, Vec3(0.2, 1.0, 0.0));
    if (templateId == "acoustic.ultrasonic-element")
        return acoustics::ultrasonicElementExperiment(id, "out-a");
    if (templateId == "mechanical.modal-test")
        return mechanics::modalTestExperiment(id);
    if (templateId == "mechanical.vibration-characterization")
        return mechanics::vibrationCharacterizationExperiment(id, 120.0);
    if (templateId == "mechanical.actuator-calibration")
        return mechanics::shakerCalibrationExperiment(id);
    if (templateId == "rf.antenna-impedance")
        return em::antennaImpedanceExperiment(id, 1e9);
    if (templateId == "rf.array-calibration")
        return em::arrayCalibrationExperiment(id, 2.4e9);
    if (templateId == "rf.radiation-pattern")
        return em::radiationPatternExperiment(id, 2.4e9);
    return Result<Experiment>::failure("unknown experiment template '" + templateId + "'");
}

}  // namespace

ExperimentsController::ExperimentsController(DocumentController* document, QObject* parent)
    : QObject(parent), document_(document), registry_(std::make_shared<DeviceRegistry>())
{
    hardware::registerHardwareDrivers(*registry_);
    connect(document_, &DocumentController::jobChanged, this, &ExperimentsController::onJobChanged);
    connect(document_, &DocumentController::projectReplaced, this, [this] {
        // A new or opened project starts without any output authorization.
        for (const auto& [id, a] : authorizations_)
            OutputAuthority::instance().release(a);
        authorizations_.clear();
        store_.reset();
        storeProject_.clear();
        reports_.clear();
        emit experimentsChanged();
        emit markersChanged();
    });
    progressTimer_.setInterval(100);
    connect(&progressTimer_, &QTimer::timeout, this, &ExperimentsController::runStateChanged);
}

ExperimentsController::~ExperimentsController()
{
    if (runner_)
        runner_->cancel();
    if (!jobId_.isNull())
        document_->jobs().wait(jobId_, std::chrono::seconds(10));
    for (const auto& [id, a] : authorizations_)
        OutputAuthority::instance().release(a);
    registry_->disconnectAll();
    registry_.reset();  // joins a discovery still running while this object is whole
}

// --- devices -------------------------------------------------------------------------------

void ExperimentsController::discoverDevices()
{
    registry_->discoverAsync([this](const DiscoveryReport& report) {
        QString text = QStringLiteral("%1 device(s) found in %2 s.").arg(report.devices.size()).arg(report.seconds, 0, 'f', 2);
        for (const auto& p : report.problems)
            text += QStringLiteral("\n") + QString::fromStdString(p);
        QMetaObject::invokeMethod(this, [this, text] {
            discoveryReport_ = text;
            emit devicesChanged();
        }, Qt::QueuedConnection);
    });
}

bool ExperimentsController::discovering() const { return registry_->discovering(); }

std::vector<DeviceInfo> ExperimentsController::devices() const { return registry_->knownDevices(); }

QString ExperimentsController::deviceStatus(const std::string& id) const { return QString::fromUtf8(toString(registry_->status(id)).data()); }

Result<DeviceCapabilities> ExperimentsController::capabilities(const std::string& id)
{
    auto device = registry_->connect(id);
    if (!device)
        return Result<DeviceCapabilities>::failure(device.message());
    return device.value()->capabilities();
}

QString ExperimentsController::driverSummary() const
{
    QStringList lines;
    for (const auto& d : hardware::driverBuildInfo())
        lines << QStringLiteral("%1: %2 - %3").arg(QString::fromStdString(d.name), d.built ? QStringLiteral("built") : QStringLiteral("not built"),
                                                   QString::fromStdString(d.scope));
    return lines.join(QStringLiteral("\n"));
}

// --- experiments ---------------------------------------------------------------------------

ExperimentWorkspace ExperimentsController::workspace() const
{
    auto w = loadExperiments(document_->project());
    return w ? w.take() : ExperimentWorkspace{};
}

std::vector<std::pair<QString, QString>> ExperimentsController::templates() const
{
    std::vector<std::pair<QString, QString>> out;
    for (const auto& t : kTemplates)
        out.emplace_back(QString::fromUtf8(t.id), QString::fromUtf8(t.title));
    return out;
}

Status ExperimentsController::storeWorkspace(const ExperimentWorkspace& w, const QString& description)
{
    return document_->pushCommand(std::make_unique<SetProjectExtensionCommand>(document_->project(), kExperimentsKey, toJson(w).dump(),
                                                                               description.toStdString(), [this] {
                                                                                   emit experimentsChanged();
                                                                                   emit markersChanged();
                                                                               }));
}

Result<std::string> ExperimentsController::addFromTemplate(const QString& templateId)
{
    ExperimentWorkspace w = workspace();
    const std::string id = "exp-" + Id::generate().str().substr(0, 8);
    auto e = fromTemplate(templateId.toStdString(), id);
    if (!e)
        return Result<std::string>::failure(e.message());
    Experiment experiment = e.take();
    experiment.projectId = document_->project().id.str();
    int n = 1;
    const std::string base = experiment.name;
    while (std::any_of(w.experiments.begin(), w.experiments.end(), [&](const Experiment& x) { return x.name == experiment.name; }))
        experiment.name = base + " " + std::to_string(++n);
    if (Status ok = w.addExperiment(experiment); !ok)
        return Result<std::string>::failure(ok.message());
    if (Status ok = storeWorkspace(w, QStringLiteral("Add experiment")); !ok)
        return Result<std::string>::failure(ok.message());
    return id;
}

Status ExperimentsController::removeExperiment(const std::string& id)
{
    ExperimentWorkspace w = workspace();
    const auto before = w.experiments.size();
    w.experiments.erase(std::remove_if(w.experiments.begin(), w.experiments.end(), [&](const Experiment& e) { return e.id == id; }),
                        w.experiments.end());
    if (w.experiments.size() == before)
        return Status::failure("no experiment " + id);
    // Its measurement records stay: raw data is never discarded with a definition.
    authorizations_.erase(id);
    return storeWorkspace(w, QStringLiteral("Remove experiment"));
}

Status ExperimentsController::setExperiment(const Experiment& experiment, const QString& description)
{
    ExperimentWorkspace w = workspace();
    Experiment* e = w.experiment(experiment.id);
    if (!e)
        return Status::failure("no experiment " + experiment.id);
    *e = experiment;
    e->revision += 1;
    // A changed definition needs a new authorization.
    if (auto it = authorizations_.find(experiment.id); it != authorizations_.end()) {
        OutputAuthority::instance().release(it->second);
        authorizations_.erase(it);
    }
    return storeWorkspace(w, description);
}

ValidationReport ExperimentsController::validate(const std::string& id) const
{
    const ExperimentWorkspace w = workspace();
    const Experiment* e = w.experiment(id);
    if (!e) {
        ValidationReport r;
        r.errors.push_back("no experiment " + id);
        return r;
    }
    ValidationReport r = validateExperiment(*e);
    for (const auto& s : e->sensors)
        if (!s.calibrationId.empty() && !w.calibrations.find(s.calibrationId))
            r.errors.push_back("sensor '" + s.id + "' selects the calibration '" + s.calibrationId + "', which is not in the library");
    return r;
}

// --- calibration ---------------------------------------------------------------------------

Status ExperimentsController::addCalibration(CalibrationRecord record)
{
    ExperimentWorkspace w = workspace();
    auto id = w.calibrations.add(std::move(record));
    if (!id)
        return Status::failure(id.message());
    return storeWorkspace(w, QStringLiteral("Add calibration"));
}

Status ExperimentsController::importCalibration(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return Status::failure("cannot read " + path.toStdString());
    try {
        auto record = calibrationRecordFromJson(nlohmann::json::parse(file.readAll().toStdString()));
        if (!record)
            return Status::failure(record.message());
        CalibrationRecord r = record.take();
        r.files.push_back(QFileInfo(path).fileName().toStdString());
        return addCalibration(r);
    } catch (const std::exception& e) {
        return Status::failure(path.toStdString() + " is not a calibration record (JSON): " + e.what());
    }
}

Status ExperimentsController::selectCalibration(const std::string& experimentId, const std::string& sensorId, const std::string& calibrationId)
{
    ExperimentWorkspace w = workspace();
    Experiment* e = w.experiment(experimentId);
    if (!e)
        return Status::failure("no experiment " + experimentId);
    if (!calibrationId.empty() && !w.calibrations.find(calibrationId))
        return Status::failure("no calibration " + calibrationId);
    Experiment copy = *e;
    bool found = false;
    for (auto& s : copy.sensors)
        if (s.id == sensorId) {
            s.calibrationId = calibrationId;
            found = true;
        }
    if (!found)
        return Status::failure("no sensor " + sensorId);
    return setExperiment(copy, QStringLiteral("Select calibration"));
}

// --- authorization -------------------------------------------------------------------------

OutputRequest ExperimentsController::outputRequest(const std::string& id) const
{
    const ExperimentWorkspace w = workspace();
    const Experiment* e = w.experiment(id);
    return e ? outputRequestFor(*e) : OutputRequest{};
}

SafetyCheck ExperimentsController::safetyCheck(const std::string& id) const
{
    const ExperimentWorkspace w = workspace();
    const Experiment* e = w.experiment(id);
    if (!e)
        return {};
    if (!e->hasPhysicalOutput()) {
        SafetyCheck c;
        c.allowed = true;
        return c;
    }
    return checkOutput(e->safety, outputRequestFor(*e));
}

Status ExperimentsController::authorize(const std::string& id, const QString& operatorName, const QString& acknowledgement)
{
    const ExperimentWorkspace w = workspace();
    const Experiment* e = w.experiment(id);
    if (!e)
        return Status::failure("no experiment " + id);
    if (!e->hasPhysicalOutput())
        return Status::failure("the experiment drives no output; nothing to authorize");
    auto a = OutputAuthority::instance().grant(outputRequestFor(*e), e->safety, operatorName.trimmed().toStdString(),
                                               acknowledgement.trimmed().toStdString());
    if (!a)
        return Status::failure(a.message());
    if (auto it = authorizations_.find(id); it != authorizations_.end())
        OutputAuthority::instance().release(it->second);
    authorizations_[id] = a.value();
    emit runStateChanged();
    return Status::success();
}

bool ExperimentsController::isAuthorized(const std::string& id) const
{
    auto it = authorizations_.find(id);
    return it != authorizations_.end() && OutputAuthority::instance().isValid(it->second);
}

void ExperimentsController::emergencyStop()
{
    OutputAuthority::instance().emergencyStop("operator pressed EMERGENCY STOP");
    if (runner_)
        runner_->cancel();
    authorizations_.clear();
    emit runStateChanged();
}

void ExperimentsController::resetEmergencyStop(const QString& operatorName)
{
    OutputAuthority::instance().resetEmergencyStop(operatorName.isEmpty() ? "operator" : operatorName.toStdString());
    emit runStateChanged();
}

bool ExperimentsController::emergencyStopActive() const { return OutputAuthority::instance().emergencyStopActive(); }

// --- runs ----------------------------------------------------------------------------------

Result<MeasurementStore*> ExperimentsController::store()
{
    const QString path = document_->filePath();
    if (path.isEmpty())
        return Result<MeasurementStore*>::failure("save the project first: measurements are stored next to the project file");
    if (store_ && storeProject_ == path)
        return store_.get();
    auto s = openMeasurementStore(measurementRootFor(path.toStdString()));
    if (!s)
        return Result<MeasurementStore*>::failure(s.message());
    store_ = s.take();
    storeProject_ = path;
    return store_.get();
}

Status ExperimentsController::run(const std::string& id)
{
    if (runner_)
        return Status::failure("an experiment is running");
    const ExperimentWorkspace w = workspace();
    const Experiment* e = w.experiment(id);
    if (!e)
        return Status::failure("no experiment " + id);
    auto s = store();
    if (!s)
        return Status::failure(s.message());
    RunOptions options;
    options.operatorName = "operator";
    if (e->hasPhysicalOutput()) {
        if (!isAuthorized(id))
            return Status::failure("the experiment drives physical outputs: authorize the output first (nothing was output)");
        options.authorization = authorizations_.at(id);
        options.operatorName = options.authorization->operatorName;
        authorizations_.erase(id);  // spent by this run
    }
    options.progress = [this](const RunProgress& p) {
        std::lock_guard lock(progressMutex());
        progress_ = p;
    };
    runCalibrations_ = std::make_unique<CalibrationLibrary>(w.calibrations);
    runner_ = std::make_shared<ExperimentRunner>(*registry_, *runCalibrations_, s.value());
    job_ = std::make_shared<ExperimentJob>(runner_, *e, options);
    runningExperiment_ = id;
    {
        std::lock_guard lock(progressMutex());
        progress_ = {};
    }
    jobId_ = document_->submitJob(job_);
    progressTimer_.start();
    emit runStateChanged();
    return Status::success();
}

void ExperimentsController::stop()
{
    if (runner_)
        runner_->cancel();
    if (!jobId_.isNull())
        document_->cancelJob(jobId_);
}

std::optional<LiveView::Snapshot> ExperimentsController::liveSnapshot() const
{
    if (!runner_)
        return std::nullopt;
    auto engine = runner_->activeAcquisition();
    if (!engine)
        return std::nullopt;
    return engine->live().snapshot();
}

void ExperimentsController::onJobChanged(const JobInfo& info)
{
    if (jobId_.isNull() || info.id != jobId_ || !isFinished(info.state))
        return;
    finishRun();
}

void ExperimentsController::finishRun()
{
    if (!job_)
        return;
    RunResult result = job_->result();
    ExperimentWorkspace w = workspace();
    if (Experiment* e = w.experiment(runningExperiment_)) {
        e->history.push_back(result.record);
        e->state = result.state;
        for (const auto& item : result.raw) {
            (void)w.addMeasurement(item.record);
            e->resultIds.push_back(item.record.id);
        }
        for (const auto& item : result.derived) {
            (void)w.addMeasurement(item.record);
            e->resultIds.push_back(item.record.id);
        }
        for (const auto& s : result.spatial)
            w.spatial.push_back(s);
        (void)storeWorkspace(w, QStringLiteral("Record experiment run"));
    }
    reports_[runningExperiment_] = QString::fromStdString(result.reportHtml);
    const QString message = QString::fromStdString(result.record.message);
    lastResult_ = std::move(result);
    runner_.reset();
    job_.reset();
    jobId_ = Id();
    runCalibrations_.reset();
    progressTimer_.stop();
    emit runStateChanged();
    emit markersChanged();
    emit devicesChanged();  // the run connected its devices
    emit runFinished(message);
}

// --- measurements --------------------------------------------------------------------------

std::vector<MeasurementRecord> ExperimentsController::measurements(const std::string& experimentId) const
{
    std::vector<MeasurementRecord> out;
    const ExperimentWorkspace w = workspace();  // the pointers below point into it
    for (const auto* m : w.measurementsOf(experimentId))
        out.push_back(*m);
    return out;
}

Result<MeasurementData> ExperimentsController::data(const std::string& measurementId)
{
    const ExperimentWorkspace w = workspace();
    const MeasurementRecord* r = w.measurement(measurementId);
    if (!r)
        return Result<MeasurementData>::failure("no measurement " + measurementId);
    if (r->dataRef.empty())
        return Result<MeasurementData>::failure("measurement " + measurementId + " was not recorded to disk");
    auto s = store();
    if (!s)
        return Result<MeasurementData>::failure(s.message());
    return s.value()->read(r->dataRef);
}

Status ExperimentsController::importTouchstone(const QString& path, const std::string& experimentId)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return Status::failure("cannot read " + path.toStdString());
    auto item = em::importTouchstoneMeasurement(file.readAll().toStdString(), QFileInfo(path).fileName().toStdString(), experimentId);
    if (!item)
        return Status::failure(item.message());
    auto s = store();
    if (!s)
        return Status::failure(s.message());
    ProcessingItem i = item.take();
    if (Status ok = s.value()->write(i.record, i.data); !ok)
        return ok;
    i.record.dataRef = s.value()->dataRefFor(i.record);
    ExperimentWorkspace w = workspace();
    if (Status ok = w.addMeasurement(i.record); !ok)
        return ok;
    return storeWorkspace(w, QStringLiteral("Import Touchstone measurement"));
}

Result<ComparisonRecord> ExperimentsController::compareWithSimulation(const std::string& experimentId, const std::string& measurementId)
{
    using R = Result<ComparisonRecord>;
    ExperimentWorkspace w = workspace();
    const Experiment* e = w.experiment(experimentId);
    if (!e)
        return R::failure("no experiment " + experimentId);
    ComparisonRecord c;
    c.id = "cmp-" + Id::generate().str().substr(0, 8);
    c.experimentId = experimentId;
    c.measurementId = measurementId;
    c.createdUtc = utcNowIso8601();
    const MeasurementRecord* m = w.measurement(measurementId);
    if (m && m->kind == MeasurementKind::NetworkParameters) {
        // S11 against the first antenna of the project.
        Id antenna;
        document_->project().scene.forEach([&](const SceneObject& o) {
            if (antenna.isNull() && o.state.component && o.state.component->typeId() == em::object_types::Antenna)
                antenna = o.id();
        });
        if (antenna.isNull())
            return R::failure("the project has no antenna whose simulated impedance could be compared with this S11");
        auto data = this->data(measurementId);
        if (!data)
            return R::failure(data.message());
        auto network = em::networkOf(ProcessingItem{*m, data.take()});
        if (!network)
            return R::failure(network.message());
        const auto sweep = em::impedanceSweep(document_->project(), antenna, network.value().frequencyHz.front(),
                                              network.value().frequencyHz.back(), 201);
        auto metrics = em::compareReflection(network.value(), sweep, "analytical antenna impedance of the project's antenna");
        if (!metrics)
            return R::failure(metrics.message());
        c.kind = "s-parameter";
        c.simulatedLabel = "analytical impedance of antenna " + antenna.str();
        c.metrics = toJson(metrics.value());
    } else if (e->domain == PhysicsDomain::Acoustic) {
        // The free field of the experiment's driven actuators as point sources.
        const SpatialDataset* d = nullptr;
        for (const auto& s : w.spatial)
            if (s.experimentId == experimentId)
                d = &s;
        if (!d)
            return R::failure("the experiment has no spatial data set to compare; run it first");
        acoustics::Medium air;
        std::vector<acoustics::PointSource> sources;
        const double omega = 2.0 * 3.14159265358979323846 * d->frequencyHz;
        for (const auto& id : e->stimulus.outputBindings) {
            const ChannelBinding* b = e->binding(id);
            const Actuator* a = b ? e->actuator(b->actuatorId) : nullptr;
            if (!a)
                continue;
            acoustics::PointSource p;
            p.position = Vec3(a->pose.x, a->pose.y, a->pose.z);
            p.forward = Vec3(a->pose.dx, a->pose.dy, a->pose.dz);
            p.frequencyHz = d->frequencyHz;
            const double delay = e->stimulus.channelDelaySeconds.count(id) ? e->stimulus.channelDelaySeconds.at(id) : 0.0;
            const double gain = e->stimulus.channelGain.count(id) ? e->stimulus.channelGain.at(id) : 1.0;
            p.amplitude = std::polar(gain, -omega * delay);
            sources.push_back(p);
        }
        if (sources.empty())
            return R::failure("the experiment drives no acoustic source to predict a field from");
        auto evaluator = std::make_shared<const acoustics::FieldEvaluator>(sources, d->frequencyHz, air,
                                                                           std::make_shared<const acoustics::FreeFieldModel>());
        SpatialComparisonOptions o;
        o.relative = true;  // the sources' absolute levels are not modelled
        auto metrics = compareSpatial(*d, acoustics::fieldSampler(evaluator), {Quantity::Pressure, std::nullopt, false}, o);
        if (!metrics)
            return R::failure(metrics.message());
        c.kind = "spatial";
        c.measurementId = d->id;
        c.simulatedLabel = "free-field point sources at the experiment's actuators (relative)";
        c.metrics = toJson(metrics.value());
    } else {
        return R::failure("there is no simulation in the project to compare this measurement with");
    }
    w.comparisons.push_back(c);
    if (Status ok = storeWorkspace(w, QStringLiteral("Compare with simulation")); !ok)
        return R::failure(ok.message());
    return c;
}

QString ExperimentsController::reportHtml(const std::string& experimentId) const
{
    if (auto it = reports_.find(experimentId); it != reports_.end())
        return it->second;
    const ExperimentWorkspace w = workspace();
    const Experiment* e = w.experiment(experimentId);
    if (!e || e->history.empty() || e->history.back().reportPath.empty() || document_->filePath().isEmpty())
        return {};
    const auto path = measurementRootFor(document_->filePath().toStdString()) / e->history.back().reportPath;
    std::ifstream in(path);
    return QString::fromStdString(std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>()));
}

Status ExperimentsController::exportArchive(const std::string& experimentId, const QString& directory)
{
    auto s = store();
    if (!s)
        return Status::failure(s.message());
    auto r = exportExperiment(workspace(), experimentId, *s.value(), directory.toStdString());
    if (!r)
        return Status::failure(r.message());
    if (!r.value().missing.empty())
        return Status::failure(std::to_string(r.value().missing.size()) + " data set(s) were missing from the store; the archive "
                               "is incomplete");
    return Status::success();
}

Status ExperimentsController::importArchive(const QString& directory)
{
    auto archive = importExperiment(directory.toStdString());
    if (!archive)
        return Status::failure(archive.message());
    auto s = store();
    if (!s)
        return Status::failure(s.message());
    ExperimentWorkspace w = workspace();
    if (Status ok = mergeArchive(w, *s.value(), archive.value()); !ok)
        return ok;
    return storeWorkspace(w, QStringLiteral("Import experiment archive"));
}

// --- viewport ------------------------------------------------------------------------------

std::vector<render::DrawItem> ExperimentsController::markers(const std::string& experimentId) const
{
    std::vector<render::DrawItem> items;
    const ExperimentWorkspace w = workspace();
    const Experiment* e = w.experiment(experimentId);
    if (!e)
        return items;
    for (const auto& s : e->sensors)
        if (s.pose.frame == "scene")
            items.push_back(markerAt(s.pose, 0.02, Eigen::Vector4f(0.25f, 0.85f, 0.35f, 1.0f)));
    for (const auto& a : e->actuators)
        if (a.pose.frame == "scene")
            items.push_back(markerAt(a.pose, 0.04, Eigen::Vector4f(0.95f, 0.55f, 0.15f, 1.0f)));
    if (!e->positions.empty())
        if (auto positions = scanPositions(e->positions))
            for (const auto& p : positions.value())
                items.push_back(markerAt(p, 0.008, Eigen::Vector4f(0.6f, 0.6f, 0.65f, 1.0f)));
    // Measured points, coloured by magnitude (sequential scale); nothing is drawn between them.
    for (const auto& d : w.spatial) {
        if (d.experimentId != experimentId || d.samples.empty())
            continue;
        double largest = 0.0;
        for (const auto& s : d.samples)
            largest = std::max(largest, std::abs(s.value));
        for (const auto& s : d.samples) {
            const auto c = render::sequentialColor(largest > 0 ? static_cast<float>(std::abs(s.value) / largest) : 0.0f);
            items.push_back(markerAt(s.pose, 0.03, Eigen::Vector4f(c.x(), c.y(), c.z(), 1.0f)));
        }
    }
    return items;
}
