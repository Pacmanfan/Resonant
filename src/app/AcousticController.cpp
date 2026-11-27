#include "AcousticController.h"

#include "DocumentController.h"
#include "commands/SceneCommands.h"
#include "foundation/Log.h"
#include "foundation/Units.h"

#include <QFile>
#include <QFileInfo>

using namespace resonant;
using namespace resonant::acoustics;
using namespace resonant::manipulators;

AcousticController::AcousticController(DocumentController* document, QObject* parent)
    : QObject(parent), document_(document)
{
    connect(document_, &DocumentController::jobChanged, this, &AcousticController::onJobChanged);
    connect(document_, &DocumentController::projectReplaced, this, [this] {
        // Results belong to the project that produced them.
        results_.clear();
        displayed_.reset();
        pending_.clear();
        emit settingsChanged();
        emit resultsChanged();
        emit displayChanged();
    });
    timer_.setInterval(33);
    connect(&timer_, &QTimer::timeout, this, [this] {
        display_.animationPhase =
            std::fmod(display_.animationPhase + 2.0 * units::pi * animationRate_ * 0.033, 2.0 * units::pi);
        emit displayChanged();
    });
}

AcousticSettings AcousticController::settings() const
{
    return settingsOf(document_->project());
}

Status AcousticController::setSettings(const AcousticSettings& newSettings)
{
    if (Status status = newSettings.validate(); !status)
        return status;
    if (newSettings == settings())
        return Status::success();
    // Merge with whatever else is stored under the key, then swap the entry
    // through an undoable command.
    Project scratch;
    scratch.extensions = document_->project().extensions;
    storeSettings(scratch, newSettings);
    return document_->pushCommand(std::make_unique<SetProjectExtensionCommand>(
        document_->project(), kSettingsKey, scratch.extensions[kSettingsKey].dump(), "Change acoustic settings",
        [this] { emit settingsChanged(); }));
}

Result<Id> AcousticController::addEmitter()
{
    EmitterSettings s;
    s.frequencyHz = settings().frequencyHz;
    return document_->insertObject(makeEmitter(document_->scene().uniqueName("Emitter"), s));
}

Result<Id> AcousticController::addArray(ArrayLayout layout)
{
    ArraySettings s;
    s.layout = layout;
    s.element.frequencyHz = settings().frequencyHz;
    // Half-wavelength spacing in the current medium is the usual starting point.
    s.spacingX = s.spacingY = 0.5 * settings().medium.wavelength(s.element.frequencyHz);
    switch (layout) {
    case ArrayLayout::Linear: s.countX = 16; break;
    case ArrayLayout::RectangularPlanar: s.countX = s.countY = 8; break;
    case ArrayLayout::Circular: s.countX = 24; break;
    case ArrayLayout::Cylindrical:
        s.countX = 16;
        s.countY = 4;
        s.facing = ElementFacing::Outward;
        break;
    case ArrayLayout::Spherical:
        s.countX = 64;
        s.facing = ElementFacing::Inward;
        break;
    case ArrayLayout::Custom: s.customPositions = {Vec3(-0.01, 0, 0), Vec3(0.01, 0, 0)}; break;
    }
    const char* names[] = {"Linear array", "Planar array", "Circular array", "Cylindrical array", "Spherical array",
                           "Custom array"};
    return document_->insertObject(makeArray(document_->scene().uniqueName(names[static_cast<int>(layout)]), s));
}

Result<Id> AcousticController::addTarget()
{
    auto target = makeTarget(document_->scene().uniqueName("Target"));
    target->state.local.position = Vec3(0.0, 0.0, 0.1);
    return document_->insertObject(std::move(target));
}

Result<Id> AcousticController::addProbe()
{
    auto probe = makeProbe(document_->scene().uniqueName("Probe"));
    probe->state.local.position = Vec3(0.0, 0.0, 0.1);
    return document_->insertObject(std::move(probe));
}

Result<Id> AcousticController::addRegion(RegionKind kind)
{
    RegionSettings s;
    s.kind = kind;
    if (kind == RegionKind::Volume) {
        s.countU = s.countV = 61;
        s.countW = 61;
    }
    auto region = makeRegion(document_->scene().uniqueName(kind == RegionKind::Volume ? "Field volume" : "Field plane"), s);
    region->state.local.position = Vec3(0.0, 0.0, 0.5 * s.size.z() + 0.005);
    return document_->insertObject(std::move(region));
}

Status AcousticController::setComponent(const Id& id, ComponentPtr component, const QString& what)
{
    const SceneObject* object = document_->scene().find(id);
    if (!object)
        return Status::failure("the object no longer exists");
    ObjectState state = object->state;
    state.component = std::move(component);
    return document_->setObjectState(id, std::move(state), what + QLatin1Char(' ') + QString::fromStdString(object->state.name));
}

std::vector<std::pair<Id, QString>> AcousticController::objectsOfType(const char* type) const
{
    std::vector<std::pair<Id, QString>> list;
    document_->scene().forEach([&](const SceneObject& object) {
        if (object.type() == type)
            list.emplace_back(object.id(), QString::fromStdString(object.state.name));
    });
    return list;
}

Result<FocusSolution> AcousticController::computeFocus(const Id& arrayId, const Vec3& target, FocusControl control,
                                                       Taper taper) const
{
    const SceneObject* object = document_->scene().find(arrayId);
    const auto* array = object ? componentOf<ArrayComponent>(*object) : nullptr;
    if (!array)
        return Result<FocusSolution>::failure("choose an array to focus");
    if (!target.allFinite())
        return Result<FocusSolution>::failure("the target position is not valid");
    const AcousticSettings s = settings();
    if (reflectorModelSelected() && control == FocusControl::Phase) {
        // Hybrid focusing: phases from the model's own responses, so the
        // reflected paths are compensated together with the direct ones.
        FocusSolution solution = computeModelFocus(collectSources(document_->scene(), {arrayId}), target, s.medium, *currentModel());
        const FocusSolution tapered = acoustics::computeFocus(generateElements(array->settings),
                                                               document_->scene().worldMatrix(*object), target, s.medium,
                                                               array->settings.element.frequencyHz, control, taper);
        // Inactive elements are not sources; give each solved element its taper weight by index.
        for (FocusElement& element : solution.elements)
            for (const FocusElement& reference : tapered.elements)
                if (reference.index == element.index)
                    element.weight = reference.weight;
        return solution;
    }
    return acoustics::computeFocus(generateElements(array->settings), document_->scene().worldMatrix(*object), target,
                                   s.medium, array->settings.element.frequencyHz, control, taper);
}

Status AcousticController::applyFocus(const Id& arrayId, const FocusSolution& solution, Taper taper)
{
    const SceneObject* object = document_->scene().find(arrayId);
    const auto* array = object ? componentOf<ArrayComponent>(*object) : nullptr;
    if (!array)
        return Status::failure("the array no longer exists");
    return setComponent(arrayId, std::make_shared<ArrayComponent>(acoustics::applyFocus(array->settings, solution, taper)),
                        QStringLiteral("Focus"));
}

AcousticController::InverseRun AcousticController::solveInverse(const Id& arrayId, const InverseOptions& options) const
{
    InverseRun run;
    const AcousticSettings s = settings();
    run.sources = collectSources(document_->scene(), {arrayId});
    run.targets = collectTargets(document_->scene());
    run.solution = acoustics::solveInverse(run.sources, run.targets.points, s.medium, *currentModel(), options);
    for (const std::string& name : run.targets.unsupported)
        run.solution.notes.push_back("Target \"" + name + "\" is not a point target and was not used: only point "
                                     "targets are solved.");
    return run;
}

Status AcousticController::applyInverse(const Id& arrayId, const InverseRun& run)
{
    if (!run.solution.solved)
        return Status::failure("there is no solution to apply");
    const SceneObject* object = document_->scene().find(arrayId);
    const auto* array = object ? componentOf<ArrayComponent>(*object) : nullptr;
    if (!array)
        return Status::failure("the array no longer exists");
    return setComponent(arrayId,
                        std::make_shared<ArrayComponent>(applyWeights(array->settings, arrayId, run.sources, run.solution.weights)),
                        QStringLiteral("Apply inverse design to"));
}

bool AcousticController::reflectorModelSelected() const
{
    return settings().modelId == kReflectorModelId;
}

namespace {
ReflectorCollection reflectorsOf(const DocumentController& document, const AcousticSettings& s)
{
    ReflectorOptions options;
    options.elementsPerWavelength = s.reflectorElementsPerWavelength;
    return collectReflectors(document.scene(), document.project().materials, s.medium.wavelength(s.frequencyHz), options);
}
}  // namespace

std::shared_ptr<const PropagationModel> AcousticController::currentModel() const
{
    const AcousticSettings s = settings();
    if (s.modelId == kReflectorModelId)
        return std::make_shared<const ReflectorModel>(s.propagation, reflectorsOf(*document_, s).surfaces);
    return std::make_shared<const FreeFieldModel>(s.propagation);
}

std::shared_ptr<const FieldEvaluator> AcousticController::currentEvaluator() const
{
    const AcousticSettings s = settings();
    const auto sources = collectSources(document_->scene());
    if (s.modelId == kReflectorModelId) {
        auto model = std::make_shared<const ReflectorModel>(s.propagation, reflectorsOf(*document_, s).surfaces);
        return std::make_shared<const ReflectorFieldEvaluator>(sources, s.frequencyHz, s.medium, std::move(model));
    }
    return std::make_shared<const FieldEvaluator>(sources, s.frequencyHz, s.medium,
                                                  std::make_shared<FreeFieldModel>(s.propagation));
}

QStringList AcousticController::modelNotes() const
{
    QStringList notes;
    const AcousticSettings s = settings();
    if (s.modelId == kReflectorModelId) {
        const ReflectorCollection collection = reflectorsOf(*document_, s);
        if (collection.surfaces.empty())
            notes.append(QStringLiteral("There is no reflector in the scene: the result equals the free field."));
        for (const std::string& note : collection.notes)
            notes.append(QString::fromStdString(note));
    } else {
        bool manipulators = false;
        document_->scene().forEach([&](const SceneObject& object) {
            manipulators = manipulators || dynamic_cast<const ManipulatorComponent*>(object.state.component.get()) != nullptr;
        });
        if (manipulators)
            notes.append(QStringLiteral("The scene has wave manipulators, but the free-field model ignores them. Select the "
                                        "reflector model to include reflectors."));
    }
    return notes;
}

std::optional<Vec3> AcousticController::intendedPoint() const
{
    const TargetCollection targets = collectTargets(document_->scene());
    if (!targets.points.empty())
        return targets.points.front().position;
    std::optional<Vec3> focus;
    document_->scene().forEach([&](const SceneObject& object) {
        const auto* manipulator = dynamic_cast<const ManipulatorComponent*>(object.state.component.get());
        if (!focus && manipulator && manipulator->settings.intendedFocus)
            focus = manipulator->settings.intendedFocus;
    });
    return focus;
}

FocusMetrics AcousticController::metricsFor(const FieldResult& result) const
{
    return analyzeFocus(result, intendedPoint());
}

Result<FieldEstimate> AcousticController::estimate(const Id& regionId) const
{
    const SceneObject* object = document_->scene().find(regionId);
    const auto* region = object ? componentOf<RegionComponent>(*object) : nullptr;
    if (!region)
        return Result<FieldEstimate>::failure("choose a field region");
    const AcousticSettings s = settings();
    size_t sourceCount = 0;
    for (const PointSource& source : collectSources(document_->scene()))
        if (std::abs(source.frequencyHz - s.frequencyHz) <= 1e-9 * s.frequencyHz)
            ++sourceCount;
    FieldEstimate estimate = estimateField(gridOf(region->settings, document_->scene().worldMatrix(*object)), sourceCount,
                                           s.medium, s.frequencyHz, s.memoryLimitBytes);
    if (s.modelId == kReflectorModelId) {
        // Every sample also costs one pass over the reflector surface elements.
        ReflectorOptions options;
        options.elementsPerWavelength = s.reflectorElementsPerWavelength;
        const size_t elements = estimateReflectorElements(document_->scene(), s.medium.wavelength(s.frequencyHz), options);
        estimate.evaluations += static_cast<double>(estimate.points) * static_cast<double>(elements);
        estimate.warnings.push_back("Reflector model: " + std::to_string(elements) + " surface element(s) at "
                                    + std::to_string(s.reflectorElementsPerWavelength).substr(0, 4)
                                    + " per wavelength (Kirchhoff approximation, not a full-wave solution).");
        if (estimate.evaluations > 2e11 && estimate.allowed) {
            estimate.allowed = false;
            estimate.refusal = "This calculation needs more than 200 billion element evaluations and would run for "
                               "hours. Reduce the region's resolution, the reflector size or the surface resolution.";
        } else if (estimate.evaluations > 5e9) {
            estimate.warnings.push_back("This is a large calculation and may take minutes.");
        }
    }
    for (const QString& note : modelNotes())
        estimate.warnings.push_back(note.toStdString());
    return estimate;
}

Result<Id> AcousticController::runField(const Id& regionId)
{
    auto estimated = estimate(regionId);
    if (!estimated)
        return Result<Id>::failure(estimated.message());
    if (!estimated.value().allowed)
        return Result<Id>::failure(estimated.value().refusal);

    const SceneObject* object = document_->scene().find(regionId);
    const auto* region = componentOf<RegionComponent>(*object);
    const AcousticSettings s = settings();
    const auto evaluator = currentEvaluator();

    // Everything needed to reproduce the run, as it is right now.
    ResultMetadata metadata;
    metadata.projectId = document_->project().id;
    nlohmann::json sources = nlohmann::json::array();
    for (const PointSource& source : evaluator->sources())
        sources.push_back({{"label", source.label},
                           {"object", source.objectId.str()},
                           {"element", source.elementIndex},
                           {"position", {source.position.x(), source.position.y(), source.position.z()}},
                           {"forward", {source.forward.x(), source.forward.y(), source.forward.z()}},
                           {"frequencyHz", source.frequencyHz},
                           {"amplitude", {source.amplitude.real(), source.amplitude.imag()}},
                           {"directivity", std::string(toString(source.directivity.model))},
                           {"radius", source.radius}});
    nlohmann::json targets = nlohmann::json::array();
    for (const TargetPoint& target : collectTargets(document_->scene()).points)
        targets.push_back({{"label", target.label},
                           {"position", {target.position.x(), target.position.y(), target.position.z()}},
                           {"desired", {target.desired.real(), target.desired.imag()}},
                           {"weight", target.weight}});
    nlohmann::json reflectors = nlohmann::json::array();
    // Keep the model alive while its surfaces are read.
    const std::shared_ptr<const PropagationModel> propagationModel = currentModel();
    if (const auto* model = dynamic_cast<const ReflectorModel*>(propagationModel.get()))
        for (const ReflectingSurface& surface : model->surfaces())
            reflectors.push_back({{"object", surface.objectId.str()},
                                  {"name", surface.label},
                                  {"boundary", std::string(manipulators::toString(surface.boundary))},
                                  {"impedance", surface.impedance},
                                  {"surfaceElements", surface.elements.size()}});
    metadata.configuration = {{"settings", toJson(s)},
                              {"reflectors", std::move(reflectors)},
                              {"region", {{"object", regionId.str()}, {"name", object->state.name}, {"component", region->toJson()}}},
                              {"sources", std::move(sources)},
                              {"targets", std::move(targets)}};

    const std::string name = object->state.name + " @ " + std::to_string(static_cast<long long>(s.frequencyHz)) + " Hz, run "
                             + std::to_string(results_.size() + pending_.size() + 1);
    auto job = std::make_shared<FieldJob>(name, evaluator, gridOf(region->settings, document_->scene().worldMatrix(*object)),
                                          std::move(metadata));
    const Id jobId = document_->submitJob(job);
    pending_.emplace_back(jobId, job);
    return jobId;
}

void AcousticController::onJobChanged(const JobInfo& info)
{
    const auto it = std::find_if(pending_.begin(), pending_.end(), [&](const auto& entry) { return entry.first == info.id; });
    if (it == pending_.end() || !isFinished(info.state))
        return;
    const auto job = it->second;
    pending_.erase(it);
    if (info.state != JobState::Completed) {
        emit message(QStringLiteral("Field simulation %1: %2").arg(QString::fromUtf8(toString(info.state).data()),
                                                                    QString::fromStdString(info.message)));
        return;
    }
    const auto result = job->result();
    if (!result)
        return;
    results_.push_back(result);
    // Keep the history of runs in the project (the samples are not stored).
    // The history is kept apart from the settings so that undoing a settings
    // change never removes a run record.
    nlohmann::json& runs = document_->project().extensions["acousticRuns"];
    if (!runs.is_array())
        runs = nlohmann::json::array();
    runs.push_back(runRecord(*result));
    displayed_ = result;
    updateScale();
    emit resultsChanged();
    emit displayChanged();
    emit message(QStringLiteral("Field simulation finished: %1").arg(QString::fromStdString(result->name)));
}

void AcousticController::addResult(std::shared_ptr<const FieldResult> result)
{
    if (!result)
        return;
    results_.push_back(result);
    nlohmann::json& runs = document_->project().extensions["acousticRuns"];
    if (!runs.is_array())
        runs = nlohmann::json::array();
    runs.push_back(runRecord(*result));
    displayed_ = std::move(result);
    updateScale();
    emit resultsChanged();
    emit displayChanged();
}

void AcousticController::displayTransient(std::shared_ptr<const FieldResult> field)
{
    displayed_ = std::move(field);
    updateScale();
    emit displayChanged();
}

void AcousticController::displayResult(const Id& resultId)
{
    displayed_.reset();
    for (const auto& result : results_)
        if (result->id == resultId)
            displayed_ = result;
    updateScale();
    emit displayChanged();
}

void AcousticController::removeResult(const Id& resultId)
{
    std::erase_if(results_, [&](const auto& result) { return result->id == resultId; });
    if (displayed_ && displayed_->id == resultId)
        displayed_.reset();
    emit resultsChanged();
    emit displayChanged();
}

std::vector<PointReport> AcousticController::pointReports(bool includeTargets) const
{
    return reportPoints(document_->scene(), *currentEvaluator(), includeTargets);
}

Status AcousticController::exportPointReports(const QString& path) const
{
    const auto evaluator = currentEvaluator();
    const auto reports = reportPoints(document_->scene(), *evaluator, true);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return Status::failure("could not write to " + path.toStdString());
    const std::string text = QFileInfo(path).suffix().compare(QLatin1String("json"), Qt::CaseInsensitive) == 0
                                 ? pointReportsToJson(reports, *evaluator).dump(2) + "\n"
                                 : pointReportsToCsv(reports);
    if (file.write(text.data(), static_cast<qint64>(text.size())) != static_cast<qint64>(text.size()))
        return Status::failure("could not write all of " + path.toStdString());
    logInfo("acoustics", "exported " + std::to_string(reports.size()) + " point report(s) to " + path.toStdString());
    return Status::success();
}

void AcousticController::updateScale()
{
    const double peak = displayed_ ? displayed_->statistics.maxMagnitude : 1.0;
    display_.scale = normalization_ == Normalization::Normalized ? (peak > 0.0 ? peak : 1.0) : absoluteScale_;
}

void AcousticController::setQuantity(render::FieldQuantity quantity)
{
    display_.quantity = quantity;
    emit displayChanged();
}

void AcousticController::setNormalization(Normalization normalization, double absoluteScalePa)
{
    normalization_ = normalization;
    if (absoluteScalePa > 0.0)
        absoluteScale_ = absoluteScalePa;
    updateScale();
    emit displayChanged();
}

void AcousticController::setSlices(double u, double v, double w, bool showU, bool showV, bool showW)
{
    display_.sliceU = u;
    display_.sliceV = v;
    display_.sliceW = w;
    display_.showSliceU = showU;
    display_.showSliceV = showV;
    display_.showSliceW = showW;
    emit displayChanged();
}

void AcousticController::setFieldVisible(bool visible)
{
    display_.visible = visible;
    emit displayChanged();
}

void AcousticController::setAnimating(bool animating)
{
    if (animating)
        timer_.start();
    else
        timer_.stop();
    emit displayChanged();
}

void AcousticController::setAnimationRate(double cyclesPerSecond)
{
    animationRate_ = std::clamp(cyclesPerSecond, 0.01, 10.0);
}

QString AcousticController::displayDescription() const
{
    if (!displayed_)
        return QStringLiteral("No field is displayed.");
    QString quantity;
    switch (display_.quantity) {
    case render::FieldQuantity::Magnitude: quantity = QStringLiteral("Pressure magnitude |P| (peak)"); break;
    case render::FieldQuantity::Phase: return QStringLiteral("Pressure phase, -180 to +180 degrees (cyclic colours)");
    case render::FieldQuantity::Instantaneous: quantity = QStringLiteral("Instantaneous pressure Re{P exp(j wt)}"); break;
    case render::FieldQuantity::RelativeIntensity:
        return QStringLiteral("Relative intensity |P|^2 / |P|max^2 (normalized; not an absolute intensity)")
               + (normalization_ == Normalization::Absolute
                      ? QStringLiteral(", reference %1 Pa").arg(display_.scale, 0, 'g', 4)
                      : QString());
    }
    return normalization_ == Normalization::Normalized
               ? QStringLiteral("%1, NORMALIZED to this result's maximum (%2 Pa)").arg(quantity).arg(display_.scale, 0, 'g', 4)
               : QStringLiteral("%1, ABSOLUTE, colour scale top = %2 Pa").arg(quantity).arg(display_.scale, 0, 'g', 4);
}

render::ComplexFieldGrid AcousticController::displayGrid() const
{
    render::ComplexFieldGrid grid;
    if (!displayed_)
        return grid;
    grid.origin = displayed_->frame.origin;
    grid.axisU = displayed_->frame.axisU;
    grid.axisV = displayed_->frame.axisV;
    grid.axisW = displayed_->frame.axisW;
    grid.nu = displayed_->frame.nu;
    grid.nv = displayed_->frame.nv;
    grid.nw = displayed_->frame.nw;
    grid.owner = displayed_;
    grid.samples = displayed_->pressure.data();
    return grid;
}
