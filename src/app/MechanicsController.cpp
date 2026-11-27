#include "MechanicsController.h"

#include "AcousticController.h"
#include "DocumentController.h"
#include "acoustics/Components.h"
#include "commands/SceneCommands.h"
#include "coupling/StructuralRadiation.h"
#include "foundation/Transform.h"
#include "mechanics/DisplayMesh.h"
#include "mechanics/Meshing.h"

#include <QFile>
#include <QFileInfo>

#include <nlohmann/json.hpp>

#include <cmath>

using namespace resonant;
using namespace resonant::mechanics;

namespace {

constexpr double pi = 3.14159265358979323846;
constexpr const char* kMeasurementsKey = "mechanicsMeasurements";

}  // namespace

MechanicsController::MechanicsController(DocumentController* document, AcousticController* acoustics, QObject* parent)
    : QObject(parent), document_(document), acoustics_(acoustics)
{
    connect(document_, &DocumentController::jobChanged, this, &MechanicsController::onJobChanged);
    connect(document_, &DocumentController::projectReplaced, this, [this] {
        results_.clear();
        pending_.reset();
        displayed_ = -1;
        rebuildMesh();
        emit settingsChanged();
        emit resultsChanged();
        emit runningChanged();
    });
    timer_.setInterval(33);
    connect(&timer_, &QTimer::timeout, this, [this] { setPhase(std::fmod(phase_ + 2.0 * pi * 0.5 * 0.033, 2.0 * pi)); });
}

MechanicsSettings MechanicsController::settings() const
{
    return mechanicsSettingsOf(document_->project());
}

Status MechanicsController::setSettings(const MechanicsSettings& newSettings)
{
    if (Status status = newSettings.validate(); !status)
        return status;
    if (newSettings == settings())
        return Status::success();
    return document_->pushCommand(std::make_unique<SetProjectExtensionCommand>(
        document_->project(), kMechanicsSettingsKey, toJson(newSettings).dump(), "Change mechanical analysis settings",
        [this] { emit settingsChanged(); }));
}

Status MechanicsController::setComponent(const Id& id, ComponentPtr component, const QString& what)
{
    const SceneObject* object = document_->scene().find(id);
    if (!object)
        return Status::failure("the object no longer exists");
    ObjectState state = object->state;
    state.component = std::move(component);
    return document_->setObjectState(id, std::move(state), what + QLatin1Char(' ') + QString::fromStdString(object->state.name));
}

Status MechanicsController::makeStructural(const Id& id, Idealization idealization)
{
    const SceneObject* object = document_->scene().find(id);
    if (!object)
        return Status::failure("select an object with geometry first");
    if (!object->state.geometry)
        return Status::failure("'" + object->state.name + "' has no geometry to idealize");
    if (object->state.component && !dynamic_cast<const StructureComponent*>(object->state.component.get()))
        return Status::failure("'" + object->state.name + "' already belongs to another domain (" + object->state.component->typeId() + ")");
    StructureSettings s;
    s.idealization = idealization;
    // Sensible starting supports: a cantilever, a simply supported plate, a solid held on one face.
    switch (idealization) {
    case Idealization::Beam: s.supports = {Support::Clamped, Support::Free, Support::Free, Support::Free, Support::Free, Support::Free}; break;
    case Idealization::Plate:
        s.supports = {Support::Pinned, Support::Pinned, Support::Pinned, Support::Pinned, Support::Free, Support::Free};
        break;
    case Idealization::Membrane: break;
    case Idealization::Solid: s.supports = {Support::Clamped, Support::Free, Support::Free, Support::Free, Support::Free, Support::Free}; break;
    }
    if (object->state.geometry->kindId() == "plane")
        s.thickness = 0.002;
    ObjectState state = object->state;
    state.component = std::make_shared<StructureComponent>(s);
    if (state.materialId.isNull())
        state.materialId = Id("mat-aluminum");  // an explicit, visible default; the user can change it
    return document_->setObjectState(id, std::move(state),
                                     QStringLiteral("Make %1 a %2").arg(QString::fromStdString(object->state.name),
                                                                         QString::fromUtf8(toString(idealization).data())));
}

Result<Id> MechanicsController::addMarker(const char* type, const QString& name, ComponentPtr component, const Id& structure)
{
    auto object = std::make_unique<SceneObject>(Id::generate(), type);
    object->state.name = document_->scene().uniqueName(name.toStdString());
    object->state.component = std::move(component);
    // On top of the structure's centre, if one is given.
    if (const SceneObject* target = structure.isNull() ? nullptr : document_->scene().find(structure)) {
        const Mat4 world = document_->scene().worldMatrix(*target);
        const Aabb bounds = target->state.geometry ? target->state.geometry->tessellate().bounds() : Aabb();
        Vec3 top = Vec3::Zero();
        if (bounds.isValid())
            top = Vec3(0.5 * (bounds.min.x() + bounds.max.x()), 0.5 * (bounds.min.y() + bounds.max.y()), bounds.max.z());
        object->state.local.position = transformPoint(world, top);
    }
    return document_->insertObject(std::move(object));
}

Result<Id> MechanicsController::addActuator(const Id& structure)
{
    ActuatorSettings s;
    s.structure = structure;
    return addMarker(mechanics::object_types::Actuator, QStringLiteral("Actuator"), std::make_shared<ActuatorComponent>(s), structure);
}

Result<Id> MechanicsController::addSensor(const Id& structure)
{
    SensorSettings s;
    s.structure = structure;
    return addMarker(mechanics::object_types::Sensor, QStringLiteral("Sensor"), std::make_shared<SensorComponent>(s), structure);
}

Result<Id> MechanicsController::addTarget(const Id& structure)
{
    VibrationTargetSettings s;
    s.structure = structure;
    return addMarker(mechanics::object_types::VibrationTarget, QStringLiteral("Vibration target"), std::make_shared<VibrationTargetComponent>(s),
                     structure);
}

Result<Id> MechanicsController::addRod(const char* kind)
{
    auto geometry = Geometry::create(kind);
    if (!geometry)
        return Result<Id>::failure(geometry.message());
    auto object = std::make_unique<SceneObject>(Id::generate(), resonant::object_types::Geometry);
    object->state.name = document_->scene().uniqueName(geometry.value().kind().displayName);
    object->state.geometry = geometry.take();
    object->state.materialId = Id("mat-steel");
    StructureSettings s;
    s.idealization = Idealization::Beam;
    s.supports = {Support::Free, Support::Free, Support::Free, Support::Free, Support::Free, Support::Free};
    object->state.component = std::make_shared<StructureComponent>(s);
    return document_->insertObject(std::move(object));
}

std::vector<std::pair<Id, QString>> MechanicsController::structures() const
{
    std::vector<std::pair<Id, QString>> list;
    for (const Id& id : structureObjects(document_->project()))
        list.emplace_back(id, QString::fromStdString(document_->scene().find(id)->state.name));
    return list;
}

StructureStudy MechanicsController::study(const Id& structure) const
{
    return prepareStudy(document_->project(), structure, settings().memoryLimitBytes);
}

Result<Id> MechanicsController::run(const Id& structure, Analysis analysis)
{
    if (pending_)
        return Result<Id>::failure("a mechanical analysis is already running");
    auto prepared = std::make_shared<const StructureStudy>(study(structure));
    if (!prepared->allowed)
        return Result<Id>::failure(prepared->refusal);
    const MechanicsSettings s = settings();
    if (Status status = s.validate(); !status)
        return Result<Id>::failure(status.message());
    if ((analysis == Analysis::Static || analysis == Analysis::Transient) && prepared->forces.empty())
        return Result<Id>::failure("attach an active actuator to the structure first");
    if (analysis == Analysis::Harmonic && prepared->forces.empty())
        return Result<Id>::failure("attach an active actuator to the structure first");
    const Id projectId = document_->project().id;
    pending_ = std::make_shared<StructuralJob>(prepared->objectName + ", " + std::string(toString(analysis)) + " analysis",
                                               [prepared, s, analysis, projectId](JobContext& context) {
                                                   StructuralOutcome o = StructuralOutcome::failure("unknown analysis");
                                                   switch (analysis) {
                                                   case Analysis::Static: o = runStatic(prepared, &context); break;
                                                   case Analysis::Modal: o = runModal(prepared, s, &context); break;
                                                   case Analysis::Harmonic: o = runHarmonic(prepared, s, &context); break;
                                                   case Analysis::Transient: o = runTransient(prepared, s, &context); break;
                                                   }
                                                   if (o) {
                                                       auto copy = std::make_shared<StructuralResult>(*o.value());
                                                       copy->metadata.projectId = projectId;
                                                       return StructuralOutcome(std::shared_ptr<const StructuralResult>(copy));
                                                   }
                                                   return o;
                                               });
    pendingId_ = document_->submitJob(pending_);
    emit runningChanged();
    return pendingId_;
}

void MechanicsController::cancel()
{
    if (pending_)
        document_->cancelJob(pendingId_);
}

void MechanicsController::onJobChanged(const JobInfo& info)
{
    if (!pending_ || info.id != pendingId_ || !isFinished(info.state))
        return;
    const auto job = std::move(pending_);
    pending_.reset();
    emit runningChanged();
    if (info.state != JobState::Completed) {
        emit message(QStringLiteral("Mechanical analysis %1: %2").arg(QString::fromUtf8(toString(info.state).data()),
                                                                       QString::fromStdString(info.message)));
        return;
    }
    const auto result = job->result();
    if (!result)
        return;
    results_.push_back(result);
    nlohmann::json& runs = document_->project().extensions["mechanicsRuns"];
    if (!runs.is_array())
        runs = nlohmann::json::array();
    runs.push_back({{"id", result->id.str()}, {"name", result->name}, {"kind", result->kind()}, {"metadata", toJson(result->metadata)}});
    // Show the new result: a static deformation, the first mode, the
    // frequency of the largest response, the last recorded instant.
    int item = 0;
    if (result->harmonic && !result->harmonic->fields.empty() && !result->harmonic->responses.empty()) {
        const auto& r = result->harmonic->responses.front();
        for (size_t i = 0; i < r.size(); ++i)
            if (std::abs(r[i]) > std::abs(r[static_cast<size_t>(item)]))
                item = static_cast<int>(i);
    } else if (result->transient && !result->transient->snapshots.empty()) {
        item = static_cast<int>(result->transient->snapshots.size()) - 1;
    } else if (result->modal) {
        // Skip rigid-body modes.
        for (size_t i = 0; i < result->modal->modes.size(); ++i)
            if (result->modal->modes[i].frequencyHz > 1e-3 * result->modal->modes.back().frequencyHz) {
                item = static_cast<int>(i);
                break;
            }
    }
    display(static_cast<int>(results_.size()) - 1, item);
    emit resultsChanged();
    emit message(QStringLiteral("Mechanical analysis finished: %1").arg(QString::fromStdString(result->name)));
}

void MechanicsController::removeResult(int index)
{
    if (index < 0 || index >= static_cast<int>(results_.size()))
        return;
    results_.erase(results_.begin() + index);
    if (displayed_ == index)
        displayed_ = -1;
    else if (displayed_ > index)
        --displayed_;
    rebuildMesh();
    emit resultsChanged();
}

// --- display -------------------------------------------------------------------------------

bool MechanicsController::available(const StructuralResult& result, Quantity quantity)
{
    switch (quantity) {
    case Quantity::VonMises: return result.analysis == Analysis::Static && !result.stresses.empty();
    case Quantity::VelocityMagnitude:
    case Quantity::AccelerationMagnitude: return result.analysis == Analysis::Harmonic;
    case Quantity::NormalComponent:
        return result.study->model->kind == ElementKind::Plate || result.study->model->kind == ElementKind::Membrane;
    default: return true;
    }
}

QString MechanicsController::quantityName(Quantity quantity)
{
    switch (quantity) {
    case Quantity::DisplacementMagnitude: return QStringLiteral("Displacement magnitude");
    case Quantity::NormalComponent: return QStringLiteral("Out-of-plane displacement (signed)");
    case Quantity::X: return QStringLiteral("Displacement X (signed)");
    case Quantity::Y: return QStringLiteral("Displacement Y (signed)");
    case Quantity::Z: return QStringLiteral("Displacement Z (signed)");
    case Quantity::VelocityMagnitude: return QStringLiteral("Velocity amplitude");
    case Quantity::AccelerationMagnitude: return QStringLiteral("Acceleration amplitude");
    case Quantity::VonMises: return QStringLiteral("Stress (von Mises / extreme fibre)");
    }
    return {};
}

int MechanicsController::itemCount(int index) const
{
    if (index < 0 || index >= static_cast<int>(results_.size()))
        return 0;
    const StructuralResult& r = *results_[static_cast<size_t>(index)];
    if (r.modal)
        return static_cast<int>(r.modal->modes.size());
    if (r.harmonic)
        return static_cast<int>(r.harmonic->fields.size());
    if (r.transient)
        return static_cast<int>(r.transient->snapshots.size());
    return r.statics ? 1 : 0;
}

QString MechanicsController::itemLabel(int index, int item) const
{
    if (index < 0 || index >= static_cast<int>(results_.size()) || item < 0 || item >= itemCount(index))
        return {};
    const StructuralResult& r = *results_[static_cast<size_t>(index)];
    if (r.modal)
        return QStringLiteral("Mode %1: %2 Hz").arg(item + 1).arg(r.modal->modes[static_cast<size_t>(item)].frequencyHz, 0, 'g', 6);
    if (r.harmonic)
        return QStringLiteral("%1 Hz").arg(r.harmonic->frequenciesHz[static_cast<size_t>(item)], 0, 'g', 6);
    if (r.transient)
        return QStringLiteral("t = %1 ms").arg(r.transient->snapshotTimes[static_cast<size_t>(item)] * 1e3, 0, 'g', 5);
    return QStringLiteral("Static deformation");
}

void MechanicsController::display(int index, int item)
{
    if (index >= static_cast<int>(results_.size()))
        index = -1;
    displayed_ = index;
    item_ = index < 0 ? 0 : std::clamp(item, 0, std::max(0, itemCount(index) - 1));
    if (index >= 0 && !available(*results_[static_cast<size_t>(index)], quantity_))
        quantity_ = Quantity::DisplacementMagnitude;
    rebuildMesh();
}

void MechanicsController::setQuantity(Quantity quantity)
{
    quantity_ = quantity;
    rebuildMesh();
}

void MechanicsController::setScale(bool automatic, double factor)
{
    automaticScale_ = automatic;
    if (factor > 0.0)
        manualScale_ = factor;
    rebuildMesh();
}

void MechanicsController::setAnimating(bool animating)
{
    if (animating)
        timer_.start();
    else
        timer_.stop();
    if (!animating)
        setPhase(0.0);
}

void MechanicsController::setPhase(double radians)
{
    phase_ = radians;
    rebuildMesh();
}

Id MechanicsController::displayedStructure() const
{
    return displayed_ >= 0 ? results_[static_cast<size_t>(displayed_)]->study->objectId : Id();
}

void MechanicsController::rebuildMesh()
{
    mesh_.reset();
    lines_.clear();
    if (displayed_ < 0) {
        emit displayChanged();
        return;
    }
    const StructuralResult& r = *results_[static_cast<size_t>(displayed_)];
    const StructuralModel& model = *r.study->model;
    // The displacement to draw (real), and the amplitude field it comes from.
    Eigen::VectorXd shape, amplitude;
    Eigen::VectorXcd complexField;
    double omega = 0.0;
    if (r.statics) {
        shape = amplitude = r.statics->displacement;
    } else if (r.modal && item_ < static_cast<int>(r.modal->modes.size())) {
        amplitude = r.modal->modes[static_cast<size_t>(item_)].shape;
        shape = amplitude * std::cos(phase_);
    } else if (r.harmonic && item_ < static_cast<int>(r.harmonic->fields.size())) {
        complexField = r.harmonic->fields[static_cast<size_t>(item_)];
        omega = 2.0 * pi * r.harmonic->frequenciesHz[static_cast<size_t>(item_)];
        shape = (complexField * std::polar(1.0, phase_)).real();
        amplitude = complexField.cwiseAbs();
    } else if (r.transient && item_ < static_cast<int>(r.transient->snapshots.size())) {
        shape = amplitude = r.transient->snapshots[static_cast<size_t>(item_)];
    } else {
        emit displayChanged();
        return;
    }
    scale_ = automaticScale_ ? mechanics::automaticScale(model, amplitude) : manualScale_;

    std::vector<double> values(model.nodes.size(), 0.0);
    bool diverging = false;
    for (size_t n = 0; n < model.nodes.size(); ++n) {
        const Vec3 t = model.translation(shape, n);
        switch (quantity_) {
        case Quantity::DisplacementMagnitude:
            values[n] = complexField.size() ? model.translation(complexField, n).norm() : (r.modal ? model.translation(amplitude, n).norm() : t.norm());
            break;
        case Quantity::NormalComponent: values[n] = t.dot(model.normal); diverging = true; break;
        case Quantity::X: values[n] = t.x(); diverging = true; break;
        case Quantity::Y: values[n] = t.y(); diverging = true; break;
        case Quantity::Z: values[n] = t.z(); diverging = true; break;
        case Quantity::VelocityMagnitude: values[n] = omega * model.translation(complexField, n).norm(); break;
        case Quantity::AccelerationMagnitude: values[n] = omega * omega * model.translation(complexField, n).norm(); break;
        case Quantity::VonMises: break;
        }
    }
    if (quantity_ == Quantity::VonMises && !r.stresses.empty()) {
        // Element values averaged at the nodes.
        std::vector<int> count(model.nodes.size(), 0);
        const int perElement = model.kind == ElementKind::Frame ? 2 : model.kind == ElementKind::Solid ? 8 : 4;
        for (size_t e = 0; e < model.elements.size(); ++e)
            for (int q = 0; q < perElement; ++q) {
                const auto node = static_cast<size_t>(model.elements[e].nodes[static_cast<size_t>(q)]);
                values[node] += r.stresses[e].vonMises;
                ++count[node];
            }
        for (size_t n = 0; n < values.size(); ++n)
            if (count[n])
                values[n] /= count[n];
    }
    // The colour scale: the largest value over the whole cycle (fixed while animating).
    double top = 0.0;
    for (size_t n = 0; n < model.nodes.size(); ++n) {
        double v = std::abs(values[n]);
        if (diverging) {
            const Vec3 a = complexField.size() ? Vec3(model.translation(complexField, n).cwiseAbs()) : Vec3(model.translation(amplitude, n).cwiseAbs());
            v = quantity_ == Quantity::NormalComponent ? std::abs(a.dot(model.normal.cwiseAbs())) : a(static_cast<int>(quantity_) - 2);
        }
        top = std::max(top, v);
    }
    const SurfaceMesh surface = deformedSurface(model, r.study->grid, shape, scale_, values);
    auto mesh = std::make_shared<render::ScalarMesh>();
    mesh->positions = surface.positions;
    mesh->normals = surface.normals;
    mesh->values = surface.values;
    mesh->indices = surface.indices;
    mesh->diverging = diverging;
    mesh->scale = static_cast<float>(top > 0.0 ? top : 1.0);
    mesh_ = mesh;
    for (const auto& [a, b] : surface.outline)
        lines_.push_back({a, b, Eigen::Vector4f(0.9f, 0.9f, 0.9f, 1.0f)});
    emit displayChanged();
}

QString MechanicsController::displayDescription() const
{
    if (displayed_ < 0)
        return QStringLiteral("No mechanical result is shown.");
    const StructuralResult& r = *results_[static_cast<size_t>(displayed_)];
    const StructuralModel& model = *r.study->model;
    const QString drawn = QStringLiteral("Deformation drawn magnified x%1 (a visual scale only; the undeformed shape is outlined).")
                              .arg(scale_, 0, 'g', 4);
    QString physical;
    if (r.modal) {
        physical = QStringLiteral("Mode shape: its amplitude is arbitrary (normalized to unit modal mass), so no physical displacement exists. ");
    } else {
        double largest = 0.0;
        if (r.statics)
            largest = r.statics->maxTranslation;
        else if (r.harmonic && item_ < static_cast<int>(r.harmonic->fields.size()))
            for (size_t n = 0; n < model.nodes.size(); ++n)
                largest = std::max(largest, model.translation(r.harmonic->fields[static_cast<size_t>(item_)], n).norm());
        else if (r.transient && item_ < static_cast<int>(r.transient->snapshots.size()))
            for (size_t n = 0; n < model.nodes.size(); ++n)
                largest = std::max(largest, model.translation(r.transient->snapshots[static_cast<size_t>(item_)], n).norm());
        physical = QStringLiteral("Physical: largest displacement %1 m%2. ")
                       .arg(largest, 0, 'g', 4)
                       .arg(r.harmonic ? QStringLiteral(" (peak amplitude)") : QString());
    }
    return QStringLiteral("%1 - %2\n%3%4\nColour: %5.")
        .arg(QString::fromStdString(r.name), itemLabel(displayed_, item_), physical, drawn, quantityName(quantity_));
}

// --- targeting, bands, radiation, measurements -------------------------------------------------

Result<TargetingSolution> MechanicsController::solveTargets(const Id& structure) const
{
    const StructureStudy s = study(structure);
    if (!s.allowed)
        return Result<TargetingSolution>::failure(s.refusal);
    std::vector<TargetRequest> requests;
    for (const TargetPoint& t : s.targets)
        requests.push_back({t.label, t.point, t.desired, t.weight});
    const MechanicsSettings settings = this->settings();
    return mechanics::solveTargets(*s.model, assemble(*s.model), s.forces, requests, settings.targetFrequency, settings.damping,
                                   settings.regularization);
}

Status MechanicsController::applyTargets(const Id& structure, const TargetingSolution& solution)
{
    const StructureStudy s = study(structure);
    if (!s.allowed)
        return Status::failure(s.refusal);
    if (s.forces.size() != solution.forces.size())
        return Status::failure("the actuators changed since the solution was calculated; solve again");
    // One undo step for all actuators.
    auto composite = std::make_unique<CompositeCommand>("Apply actuator inputs");
    for (size_t i = 0; i < s.forces.size(); ++i) {
        const SceneObject* object = document_->scene().find(s.forces[i].objectId);
        const auto* actuator = object ? dynamic_cast<const ActuatorComponent*>(object->state.component.get()) : nullptr;
        if (!actuator)
            continue;
        ActuatorSettings a = actuator->settings;
        a.amplitude = std::abs(solution.forces[i]);
        a.phase = std::arg(solution.forces[i]);
        if (a.phase < 0.0)
            a.phase += 2.0 * pi;
        a.delay = 0.0;
        ObjectState state = object->state;
        state.component = std::make_shared<ActuatorComponent>(a);
        composite->add(std::make_unique<SetObjectStateCommand>(document_->project().scene, object->id(), std::move(state), "Set actuator input"));
    }
    return document_->pushCommand(std::move(composite));
}

Result<BandStructure> MechanicsController::bandStructure(const Id& structure, int bands) const
{
    const SceneObject* object = document_->scene().find(structure);
    if (!object || !object->state.geometry)
        return Result<BandStructure>::failure("select a periodic rod");
    const Material* material = object->state.materialId.isNull() ? nullptr : document_->project().materials.find(object->state.materialId);
    if (!material)
        return Result<BandStructure>::failure("assign a material to the rod");
    auto elastic = elasticMaterialOf(*material);
    if (!elastic)
        return Result<BandStructure>::failure(elastic.message());
    const Mat4 world = document_->scene().worldMatrix(*object);
    auto rod = periodicRodOf(*object->state.geometry, elastic.value(), transformDirection(world, Vec3::UnitZ()).norm(),
                             transformDirection(world, Vec3::UnitX()).norm());
    if (!rod)
        return Result<BandStructure>::failure(rod.message());
    return rodBandStructure(rod.value(), bands, 61);
}

Status MechanicsController::radiate(int resultIndex, int frequencyIndex, const Id& regionId)
{
    if (resultIndex < 0 || resultIndex >= static_cast<int>(results_.size()))
        return Status::failure("select a harmonic result");
    const StructuralResult& r = *results_[static_cast<size_t>(resultIndex)];
    if (!r.harmonic || frequencyIndex < 0 || frequencyIndex >= static_cast<int>(r.harmonic->fields.size()))
        return Status::failure("radiation needs a harmonic result with its full fields kept");
    const SceneObject* region = document_->scene().find(regionId);
    const auto* component = region ? dynamic_cast<const acoustics::RegionComponent*>(region->state.component.get()) : nullptr;
    if (!component)
        return Status::failure("select an acoustic field region to calculate the sound on");
    const acoustics::GridFrame frame = acoustics::gridOf(component->settings, document_->scene().worldMatrix(*region));
    const double f = r.harmonic->frequenciesHz[static_cast<size_t>(frequencyIndex)];
    auto field = coupling::radiateFromPlate(*r.study->model, r.harmonic->fields[static_cast<size_t>(frequencyIndex)], f,
                                            acoustics_->settings().medium, frame,
                                            r.study->objectName + " radiating at " + std::to_string(static_cast<long long>(std::lround(f))) + " Hz");
    if (!field)
        return Status::failure(field.message());
    field.value()->metadata.projectId = document_->project().id;
    field.value()->metadata.configuration = {{"structuralResult", r.id.str()}, {"frequencyHz", f}, {"region", regionId.str()}};
    acoustics_->addResult(field.take());
    return Status::success();
}

Status MechanicsController::importMeasurement(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return Status::failure("could not read " + path.toStdString());
    auto parsed = parseMeasuredResponse(file.readAll().toStdString(), QFileInfo(path).completeBaseName().toStdString());
    if (!parsed)
        return Status::failure(parsed.message());
    const MeasuredResponse& m = parsed.value();
    nlohmann::json entry = {{"name", m.name}, {"sensor", m.sensor}, {"quantity", std::string(toString(m.quantity))}, {"unit", m.unit},
                            {"source", m.source.empty() ? path.toStdString() : m.source}, {"sensitivity", m.sensitivity},
                            {"frequencyHz", m.frequencyHz}};
    std::vector<double> re, im;
    for (const auto& v : m.values) {
        re.push_back(v.real());
        im.push_back(v.imag());
    }
    entry["real"] = re;
    entry["imag"] = im;
    if (m.position)
        entry["position"] = {m.position->x(), m.position->y(), m.position->z()};
    nlohmann::json all = document_->project().extensions.value(kMeasurementsKey, nlohmann::json::array());
    all.push_back(entry);
    return document_->pushCommand(std::make_unique<SetProjectExtensionCommand>(document_->project(), kMeasurementsKey, all.dump(),
                                                                               "Import measured response", [this] { emit resultsChanged(); }));
}

std::vector<MeasuredResponse> MechanicsController::measurements() const
{
    std::vector<MeasuredResponse> list;
    const nlohmann::json all = document_->project().extensions.value(kMeasurementsKey, nlohmann::json::array());
    for (const nlohmann::json& e : all) {
        MeasuredResponse m;
        m.name = e.value("name", std::string());
        m.sensor = e.value("sensor", std::string());
        m.unit = e.value("unit", std::string());
        m.source = e.value("source", std::string());
        const std::string quantity = e.value("quantity", std::string("acceleration"));
        for (const auto q : {mechanics::Quantity::Displacement, mechanics::Quantity::Velocity, mechanics::Quantity::Acceleration})
            if (toString(q) == quantity)
                m.quantity = q;
        m.frequencyHz = e.value("frequencyHz", std::vector<double>{});
        const auto re = e.value("real", std::vector<double>{}), im = e.value("imag", std::vector<double>{});
        for (size_t i = 0; i < std::min(re.size(), im.size()); ++i)
            m.values.emplace_back(re[i], im[i]);
        list.push_back(std::move(m));
    }
    return list;
}
