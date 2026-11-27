#include "EmController.h"

#include "DocumentController.h"
#include "acoustics/Components.h"
#include "acoustics/FieldGrid.h"
#include "commands/SceneCommands.h"
#include "foundation/Transform.h"

#include <QFile>
#include <QFileInfo>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>

using namespace resonant;
using namespace resonant::em;

namespace {

constexpr const char* kMeasurementsKey = "electromagneticMeasurements";
constexpr const char* kRunsKey = "electromagneticRuns";

nlohmann::json measurementJson(const QString& name, const OnePortData& d)
{
    nlohmann::json re = nlohmann::json::array(), im = nlohmann::json::array();
    for (const Complex& s : d.reflection) {
        re.push_back(s.real());
        im.push_back(s.imag());
    }
    return {{"name", name.toStdString()}, {"referenceImpedance", d.referenceImpedance}, {"comments", d.comments},
            {"frequencyHz", d.frequencyHz}, {"reflectionReal", re}, {"reflectionImag", im}};
}

}  // namespace

EmController::EmController(DocumentController* document, QObject* parent) : QObject(parent), document_(document)
{
    connect(document_, &DocumentController::jobChanged, this, &EmController::onJobChanged);
    connect(document_, &DocumentController::projectReplaced, this, [this] {
        results_.clear();
        pending_.reset();
        displayed_ = -1;
        rebuildDisplay();
        hidePattern3D();
        emit settingsChanged();
        emit resultsChanged();
        emit runningChanged();
        emit measurementsChanged();
    });
    timer_.setInterval(33);
    connect(&timer_, &QTimer::timeout, this, [this] {
        fieldDisplay_.animationPhase = std::fmod(fieldDisplay_.animationPhase + 2.0 * kPi * 0.5 * 0.033, 2.0 * kPi);
        emit displayChanged();
    });
}

EmSettings EmController::settings() const
{
    return emSettingsOf(document_->project());
}

Status EmController::setSettings(const EmSettings& s)
{
    if (Status status = s.validate(); !status)
        return status;
    if (s == settings())
        return Status::success();
    return document_->pushCommand(std::make_unique<SetProjectExtensionCommand>(document_->project(), kEmSettingsKey, toJson(s).dump(),
                                                                               "Change electromagnetic analysis settings",
                                                                               [this] { emit settingsChanged(); }));
}

EmFullWaveSettings EmController::fullWaveSettings() const
{
    return emFullWaveSettingsOf(document_->project());
}

Status EmController::setFullWaveSettings(const EmFullWaveSettings& s)
{
    if (Status status = s.validate(); !status)
        return status;
    if (s == fullWaveSettings())
        return Status::success();
    return document_->pushCommand(std::make_unique<SetProjectExtensionCommand>(document_->project(), kEmFullWaveKey, toJson(s).dump(),
                                                                               "Change electromagnetic full-wave settings",
                                                                               [this] { emit settingsChanged(); }));
}

// --- objects ------------------------------------------------------------------------------

Result<Id> EmController::insert(const char* type, const QString& name, ComponentPtr component, std::optional<Geometry> geometry, const Id& material)
{
    auto object = std::make_unique<SceneObject>(Id::generate(), type);
    object->state.name = document_->scene().uniqueName(name.toStdString());
    object->state.component = std::move(component);
    object->state.geometry = std::move(geometry);
    object->state.materialId = material;
    return document_->insertObject(std::move(object));
}

Result<Id> EmController::addAntenna(const QString& kind)
{
    auto geometry = Geometry::create(kind.toStdString());
    if (!geometry)
        return Result<Id>::failure(geometry.message());
    Geometry g = geometry.take();
    AntennaSettings s;  // 1 GHz
    const double lambda = wavelength(s.frequencyHz);
    Status sized = Status::success();
    if (kind == QLatin1String("antenna.dipole"))
        sized = g.setParameter("length", 0.5 * lambda);
    else if (kind == QLatin1String("antenna.monopole"))
        sized = g.setParameter("height", 0.25 * lambda);
    if (!sized)
        return Result<Id>::failure(sized.message());
    const bool wire = kind == QLatin1String("antenna.dipole") || kind == QLatin1String("antenna.monopole") || kind == QLatin1String("antenna.loop")
                      || kind == QLatin1String("antenna.helix");
    // An explicit, visible default for wires (the conductor loss uses it); a
    // patch's material is its substrate and is left for the user to choose.
    return insert(em::object_types::Antenna, QString::fromStdString(g.kind().displayName), std::make_shared<AntennaComponent>(s), std::move(g),
                  wire ? Id("mat-copper") : Id());
}

Result<Id> EmController::addSource(SourceKind kind)
{
    SourceSettings s;
    s.kind = kind;
    if (kind == SourceKind::PlaneWave)
        s.amplitude = 1.0;  // V/m
    return insert(em::object_types::Source, QString::fromUtf8(std::string(toString(kind)).c_str()), std::make_shared<SourceComponent>(s), std::nullopt);
}

Result<Id> EmController::addArray(EmArrayLayout layout)
{
    EmArraySettings s;
    s.layout = layout;
    const double half = 0.5 * wavelength(s.frequencyHz);
    s.spacingX = s.spacingY = half;
    switch (layout) {
    case EmArrayLayout::Linear: s.countX = 8; s.countY = 1; break;
    case EmArrayLayout::Planar: s.countX = s.countY = 4; s.pattern = ElementPattern::CosineHemisphere; break;
    case EmArrayLayout::Circular: s.countX = 12; s.radius = 12 * half / (2.0 * kPi); break;
    case EmArrayLayout::Custom: s.customPositions = {Vec3(-0.5 * half, 0, 0), Vec3(0.5 * half, 0, 0)}; break;
    }
    return insert(em::object_types::Array, QStringLiteral("RF array (%1)").arg(QString::fromUtf8(std::string(toString(layout)).c_str())),
                  std::make_shared<EmArrayComponent>(s), std::nullopt);
}

Result<Id> EmController::addGuide(const QString& kind)
{
    auto geometry = Geometry::create(kind.toStdString());
    if (!geometry)
        return Result<Id>::failure(geometry.message());
    Geometry g = geometry.take();
    return insert(em::object_types::Guide, QString::fromStdString(g.kind().displayName), std::make_shared<GuideComponent>(), std::move(g));
}

Status EmController::setComponent(const Id& id, ComponentPtr component, const QString& what)
{
    const SceneObject* object = document_->scene().find(id);
    if (!object)
        return Status::failure("the object no longer exists");
    ObjectState state = object->state;
    state.component = std::move(component);
    return document_->setObjectState(id, std::move(state), what + QLatin1Char(' ') + QString::fromStdString(object->state.name));
}

EmController::Kind EmController::kindOf(const Id& id) const
{
    const SceneObject* o = id.isNull() ? nullptr : document_->scene().find(id);
    const Component* c = o ? o->state.component.get() : nullptr;
    if (dynamic_cast<const AntennaComponent*>(c))
        return Kind::Antenna;
    if (dynamic_cast<const SourceComponent*>(c))
        return Kind::Source;
    if (dynamic_cast<const EmArrayComponent*>(c))
        return Kind::Array;
    if (dynamic_cast<const GuideComponent*>(c))
        return Kind::Guide;
    return Kind::None;
}

std::vector<std::pair<Id, QString>> EmController::objects() const
{
    std::vector<std::pair<Id, QString>> out;
    document_->scene().forEach([&](const SceneObject& o) {
        if (kindOf(o.id()) != Kind::None)
            out.emplace_back(o.id(), QString::fromStdString(o.state.name));
    });
    return out;
}

std::vector<std::pair<Id, QString>> EmController::regions() const
{
    std::vector<std::pair<Id, QString>> out;
    document_->scene().forEach([&](const SceneObject& o) {
        if (dynamic_cast<const acoustics::RegionComponent*>(o.state.component.get()))
            out.emplace_back(o.id(), QString::fromStdString(o.state.name));
    });
    return out;
}

Result<SampleGrid> EmController::regionGrid(const Id& regionId) const
{
    const SceneObject* o = document_->scene().find(regionId);
    const auto* region = o ? dynamic_cast<const acoustics::RegionComponent*>(o->state.component.get()) : nullptr;
    if (!region)
        return Result<SampleGrid>::failure("choose a field region (Acoustics > Add Field Region places one)");
    const acoustics::GridFrame f = acoustics::gridOf(region->settings, document_->scene().worldMatrix(*o));
    SampleGrid g;
    g.origin = f.origin;
    g.axisU = f.axisU;
    g.axisV = f.axisV;
    g.axisW = f.axisW;
    g.nu = f.nu;
    g.nv = f.nv;
    g.nw = f.nw;
    return g;
}

// --- analysis -----------------------------------------------------------------------------

AntennaReport EmController::antennaReport(const Id& id) const
{
    return analyzeAntenna(document_->project(), id);
}

double EmController::referenceImpedance(const Id& id) const
{
    const SceneObject* o = document_->scene().find(id);
    const auto* antenna = o ? dynamic_cast<const AntennaComponent*>(o->state.component.get()) : nullptr;
    return antenna ? antenna->settings.referenceImpedance : 50.0;
}

GuideReport EmController::guideReport(const Id& id) const
{
    return analyzeGuide(document_->project(), id);
}

std::vector<std::pair<double, AntennaImpedance>> EmController::sweep(const Id& id) const
{
    const EmSettings s = settings();
    return impedanceSweep(document_->project(), id, s.sweepMinHz, s.sweepMaxHz, s.sweepPoints);
}

Result<PatternGrid> EmController::pattern(const Id& id) const
{
    return patternOf(document_->project(), id, settings().patternResolutionDeg);
}

Result<EmController::ArrayAnalysis> EmController::analyzeArray(const Id& id) const
{
    const SceneObject* o = document_->scene().find(id);
    const auto* array = o ? dynamic_cast<const EmArrayComponent*>(o->state.component.get()) : nullptr;
    if (!array)
        return Result<ArrayAnalysis>::failure("select an RF array");
    ArrayAnalysis a;
    const EmArraySettings& s = array->settings;
    a.frequencyHz = s.frequencyHz;
    const ArrayModel model = arrayModelOf(s);
    a.pattern = analyzePattern(model, s.frequencyHz, std::clamp(settings().patternResolutionDeg, 0.25, 0.5));
    std::vector<Vec3> lattice;
    if (s.layout == EmArrayLayout::Linear && s.countX > 1)
        lattice = {Vec3(s.spacingX, 0, 0)};
    else if (s.layout == EmArrayLayout::Planar)
        lattice = {Vec3(s.spacingX, 0, 0), Vec3(0, s.spacingY, 0)};
    if (!lattice.empty()) {
        a.predictedGratingLobes = predictedGratingLobes(lattice, a.pattern.mainLobe, s.frequencyHz);
        for (const Vec3& g : a.predictedGratingLobes)
            a.warnings << QStringLiteral("Grating lobe predicted toward theta = %1 deg, phi = %2 deg (local): the element spacing is too large for this "
                                         "steering at %3 MHz.")
                              .arg(std::acos(std::clamp(g.z(), -1.0, 1.0)) * 180.0 / kPi, 0, 'f', 1)
                              .arg(std::atan2(g.y(), g.x()) * 180.0 / kPi, 0, 'f', 1)
                              .arg(s.frequencyHz / 1e6, 0, 'g', 6);
    } else if (s.layout == EmArrayLayout::Circular || s.layout == EmArrayLayout::Custom) {
        a.warnings << QStringLiteral("Grating lobes are predicted for regular linear and planar lattices; for this layout look at the other maxima "
                                     "within 1 dB of the main lobe below.");
    }
    for (const Vec3& m : a.pattern.equalMaxima)
        a.warnings << QStringLiteral("Another maximum within 1 dB of the main lobe toward theta = %1 deg, phi = %2 deg (a grating lobe, or the "
                                     "mirror image of a planar array without a ground plane).")
                          .arg(std::acos(std::clamp(m.z(), -1.0, 1.0)) * 180.0 / kPi, 0, 'f', 1)
                          .arg(std::atan2(m.y(), m.x()) * 180.0 / kPi, 0, 'f', 1);
    if (s.spacingX < 0.25 * wavelength(s.frequencyHz) && s.layout != EmArrayLayout::Custom)
        a.warnings << QStringLiteral("Elements closer than a quarter wavelength couple strongly; this array-factor model does not include mutual "
                                     "coupling.");
    return a;
}

Status EmController::steerArray(const Id& id)
{
    const SceneObject* o = document_->scene().find(id);
    const auto* array = o ? dynamic_cast<const EmArrayComponent*>(o->state.component.get()) : nullptr;
    if (!array)
        return Status::failure("select an RF array");
    const EmSettings s = settings();
    ArrayModel model = arrayModelOf(array->settings);
    // Keep the amplitudes (tapers) and steer the phases or delays.
    for (ArrayElement& e : model.elements) {
        e.weight = Complex(std::abs(e.weight), 0.0);
        e.delay = 0.0;
    }
    steer(model, directionOf(s.steerThetaDeg * kPi / 180.0, s.steerPhiDeg * kPi / 180.0), array->settings.frequencyHz,
          s.trueTimeDelay ? SteeringMethod::TimeDelay : SteeringMethod::Phase);
    return setComponent(id, std::make_shared<EmArrayComponent>(withExcitation(array->settings, model)),
                        QStringLiteral("Steer (%1, %2 deg)").arg(s.steerThetaDeg).arg(s.steerPhiDeg));
}

Status EmController::applyChebyshev(const Id& id)
{
    const SceneObject* o = document_->scene().find(id);
    const auto* array = o ? dynamic_cast<const EmArrayComponent*>(o->state.component.get()) : nullptr;
    if (!array)
        return Status::failure("select an RF array");
    if (array->settings.layout != EmArrayLayout::Linear)
        return Status::failure("the Dolph-Chebyshev taper is applied to linear arrays");
    const std::vector<double> w = chebyshevWeights(array->settings.countX, settings().chebyshevSidelobeDb);
    EmArraySettings next = array->settings;
    for (int i = 0; i < next.countX; ++i)
        next.overrides[i].amplitude = w[static_cast<size_t>(i)];
    return setComponent(id, std::make_shared<EmArrayComponent>(next), QStringLiteral("Chebyshev taper on"));
}

Result<BeamSynthesis> EmController::synthesize(const Id& id, bool apply)
{
    const SceneObject* o = document_->scene().find(id);
    const auto* array = o ? dynamic_cast<const EmArrayComponent*>(o->state.component.get()) : nullptr;
    if (!array)
        return Result<BeamSynthesis>::failure("select an RF array");
    const EmSettings s = settings();
    if (s.beams.empty())
        return Result<BeamSynthesis>::failure("add at least one beam or null direction first");
    std::vector<BeamTarget> targets;
    for (const EmSettings::Beam& b : s.beams) {
        BeamTarget t;
        t.direction = directionOf(b.thetaDeg * kPi / 180.0, b.phiDeg * kPi / 180.0);
        t.desired = b.null ? Complex(0.0, 0.0) : Complex(std::pow(10.0, b.levelDb / 20.0), 0.0);
        targets.push_back(t);
    }
    ArrayModel model = arrayModelOf(array->settings);
    auto result = synthesizeBeams(model, targets, array->settings.frequencyHz, s.regularization,
                                  s.amplitudeLimit > 0.0 ? std::optional<double>(s.amplitudeLimit) : std::nullopt);
    if (!result || !apply)
        return result;
    size_t k = 0;
    for (ArrayElement& e : model.elements)
        if (e.active && k < result.value().weights.size()) {
            e.weight = result.value().weights[k++];
            e.delay = 0.0;
        }
    if (Status status = setComponent(id, std::make_shared<EmArrayComponent>(withExcitation(array->settings, model)), QStringLiteral("Synthesize beams for"));
        !status)
        return Result<BeamSynthesis>::failure(status.message());
    return result;
}

// --- fields -------------------------------------------------------------------------------

Status EmController::evaluateRegion(const Id& regionId)
{
    auto grid = regionGrid(regionId);
    if (!grid)
        return Status::failure(grid.message());
    // The frequency of the first active radiator.
    double f = 0.0;
    document_->scene().forEach([&](const SceneObject& o) {
        if (f > 0.0)
            return;
        if (const auto* a = dynamic_cast<const AntennaComponent*>(o.state.component.get()); a && a->settings.active)
            f = a->settings.frequencyHz;
        else if (const auto* s = dynamic_cast<const SourceComponent*>(o.state.component.get()); s && s->settings.active)
            f = s->settings.frequencyHz;
        else if (const auto* r = dynamic_cast<const EmArrayComponent*>(o.state.component.get()); r && r->settings.active)
            f = r->settings.frequencyHz;
    });
    if (f <= 0.0)
        return Status::failure("there is no active antenna, source or array");
    const SceneObject* region = document_->scene().find(regionId);
    auto result = evaluateFields(document_->project(), grid.value(), f, region->state.name + ", analytical field");
    if (!result)
        return Status::failure(result.message());
    auto copy = result.take();
    copy->metadata.projectId = document_->project().id;
    addResult(copy);
    return Status::success();
}

Status EmController::showGuideField(const Id& guideId)
{
    auto result = te10CrossSection(document_->project(), guideId);
    if (!result)
        return Status::failure(result.message());
    auto copy = result.take();
    copy->metadata.projectId = document_->project().id;
    addResult(copy);
    return Status::success();
}

Result<std::shared_ptr<EmFieldResult>> EmController::probe(const Vec3& world) const
{
    double f = 0.0;
    document_->scene().forEach([&](const SceneObject& o) {
        if (f > 0.0)
            return;
        if (const auto* a = dynamic_cast<const AntennaComponent*>(o.state.component.get()); a && a->settings.active)
            f = a->settings.frequencyHz;
        else if (const auto* s = dynamic_cast<const SourceComponent*>(o.state.component.get()); s && s->settings.active)
            f = s->settings.frequencyHz;
        else if (const auto* r = dynamic_cast<const EmArrayComponent*>(o.state.component.get()); r && r->settings.active)
            f = r->settings.frequencyHz;
    });
    if (f <= 0.0)
        return Result<std::shared_ptr<EmFieldResult>>::failure("there is no active antenna, source or array");
    SampleGrid g;
    g.origin = world;
    g.nu = g.nv = g.nw = 1;
    return evaluateFields(document_->project(), g, f, "probe");
}

EmPrepared EmController::planFullWave(const Id& regionId) const
{
    auto grid = regionGrid(regionId);
    if (!grid) {
        EmPrepared refused;
        refused.plan.allowed = false;
        refused.plan.refusal = grid.message();
        return refused;
    }
    const SampleGrid& g = grid.value();
    DomainBox box;
    box.corner = g.origin;
    box.volume = g.nw > 1;
    const Vec3 u = g.axisU.normalized(), v = g.axisV.normalized();
    box.axis = {u, v, box.volume ? Vec3(g.axisW.normalized()) : Vec3(u.cross(v))};
    box.size = Vec3(g.axisU.norm(), g.axisV.norm(), g.axisW.norm());
    return prepareFullWave(document_->project(), box, fullWaveSettings());
}

Result<Id> EmController::runFullWave(const Id& regionId)
{
    if (pending_)
        return Result<Id>::failure("an electromagnetic full-wave run is already in progress");
    EmPrepared prepared = planFullWave(regionId);
    if (!prepared.plan.allowed)
        return Result<Id>::failure(prepared.plan.refusal);
    const SceneObject* region = document_->scene().find(regionId);
    ResultMetadata metadata;
    metadata.projectId = document_->project().id;
    pending_ = std::make_shared<EmFullWaveJob>(region->state.name + ", electromagnetic full-wave (Yee FDTD)", std::move(prepared), fullWaveSettings(),
                                               metadata);
    pendingId_ = document_->submitJob(pending_);
    emit runningChanged();
    return pendingId_;
}

void EmController::cancel()
{
    if (pending_)
        document_->cancelJob(pendingId_);
}

void EmController::onJobChanged(const JobInfo& info)
{
    if (!pending_ || info.id != pendingId_ || !isFinished(info.state))
        return;
    const auto job = std::move(pending_);
    pending_.reset();
    emit runningChanged();
    if (info.state != JobState::Completed) {
        emit message(QStringLiteral("Electromagnetic full-wave run %1: %2").arg(QString::fromUtf8(toString(info.state).data()),
                                                                                QString::fromStdString(info.message)));
        return;
    }
    const auto result = job->result();
    if (!result)
        return;
    if (!result->harmonicField) {
        emit message(QStringLiteral("The full-wave run finished; with a non-continuous signal no steady-state field is extracted (the time "
                                    "record is in the run's metadata)."));
        return;
    }
    addResult(result->harmonicField);
}

void EmController::addResult(std::shared_ptr<const EmFieldResult> result)
{
    results_.push_back(result);
    nlohmann::json& runs = document_->project().extensions[kRunsKey];
    if (!runs.is_array())
        runs = nlohmann::json::array();
    runs.push_back({{"id", result->id.str()}, {"name", result->name}, {"kind", result->kind()}, {"frequencyHz", result->frequencyHz},
                    {"metadata", toJson(result->metadata)}, {"assumptions", result->assumptions}});
    emit resultsChanged();
    display(static_cast<int>(results_.size()) - 1);
}

void EmController::removeResult(int index)
{
    if (index < 0 || index >= static_cast<int>(results_.size()))
        return;
    results_.erase(results_.begin() + index);
    if (displayed_ == index)
        displayed_ = -1;
    else if (displayed_ > index)
        --displayed_;
    rebuildDisplay();
    emit resultsChanged();
}

// --- measured data ------------------------------------------------------------------------

Status EmController::importTouchstone(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return Status::failure("cannot read " + path.toStdString());
    auto parsed = parseTouchstone1(file.readAll().toStdString());
    if (!parsed)
        return Status::failure(QFileInfo(path).fileName().toStdString() + ": " + parsed.message());
    nlohmann::json list = document_->project().extensions.value(kMeasurementsKey, nlohmann::json::array());
    if (!list.is_array())
        list = nlohmann::json::array();
    list.push_back(measurementJson(QFileInfo(path).fileName(), parsed.value()));
    return document_->pushCommand(std::make_unique<SetProjectExtensionCommand>(
        document_->project(), kMeasurementsKey, list.dump(), "Import measured one-port data " + QFileInfo(path).fileName().toStdString(),
        [this] { emit measurementsChanged(); }));
}

Status EmController::exportTouchstone(const Id& antennaId, const QString& path) const
{
    const SceneObject* o = document_->scene().find(antennaId);
    const auto* antenna = o ? dynamic_cast<const AntennaComponent*>(o->state.component.get()) : nullptr;
    if (!antenna)
        return Status::failure("select an antenna");
    const auto points = sweep(antennaId);
    if (points.empty())
        return Status::failure("no impedance is calculated for this antenna kind (design estimates only)");
    OnePortData d;
    d.referenceImpedance = antenna->settings.referenceImpedance;
    d.comments = "Calculated by Resonant for '" + o->state.name + "': " + antennaReport(antennaId).model +
                 "\nAnalytical model, not a measurement. Time convention exp(+j omega t).";
    for (const auto& [f, z] : points) {
        d.frequencyHz.push_back(f);
        d.reflection.push_back(match(z.input, d.referenceImpedance).reflection);
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return Status::failure("cannot write " + path.toStdString());
    const std::string text = writeTouchstone1(d);
    file.write(text.data(), static_cast<qint64>(text.size()));
    return Status::success();
}

std::vector<std::pair<QString, OnePortData>> EmController::measurements() const
{
    std::vector<std::pair<QString, OnePortData>> out;
    const auto it = document_->project().extensions.find(kMeasurementsKey);
    if (it == document_->project().extensions.end() || !it->is_array())
        return out;
    for (const nlohmann::json& m : *it) {
        try {
            OnePortData d;
            d.referenceImpedance = m.value("referenceImpedance", 50.0);
            d.comments = m.value("comments", std::string());
            d.frequencyHz = m.at("frequencyHz").get<std::vector<double>>();
            const auto re = m.at("reflectionReal").get<std::vector<double>>(), im = m.at("reflectionImag").get<std::vector<double>>();
            for (size_t i = 0; i < re.size() && i < im.size(); ++i)
                d.reflection.emplace_back(re[i], im[i]);
            if (d.reflection.size() == d.frequencyHz.size())
                out.emplace_back(QString::fromStdString(m.value("name", std::string("measurement"))), std::move(d));
        } catch (const nlohmann::json::exception&) {
            // a damaged record is skipped; the rest is shown
        }
    }
    return out;
}

// --- display ------------------------------------------------------------------------------

void EmController::display(int index)
{
    displayed_ = index >= 0 && index < static_cast<int>(results_.size()) ? index : -1;
    if (displayed_ >= 0 && !results_[static_cast<size_t>(displayed_)]->hasH && component_ >= FieldComponent::HMagnitude)
        component_ = FieldComponent::EMagnitude;
    rebuildDisplay();
}

void EmController::setComponentShown(FieldComponent component)
{
    component_ = component;
    rebuildDisplay();
}

void EmController::setQuantity(Quantity quantity)
{
    quantity_ = quantity;
    rebuildDisplay();
}

void EmController::setNormalized(bool normalized, double absoluteScale)
{
    normalized_ = normalized;
    absoluteScale_ = absoluteScale > 0.0 ? absoluteScale : 1.0;
    rebuildDisplay();
}

void EmController::setAnimating(bool animating)
{
    if (animating)
        timer_.start();
    else
        timer_.stop();
    emit displayChanged();
}

void EmController::rebuildDisplay()
{
    samples_.reset();
    if (displayed_ >= 0) {
        const EmFieldResult& r = *results_[static_cast<size_t>(displayed_)];
        auto samples = std::make_shared<std::vector<std::complex<float>>>(r.grid.count());
        for (size_t i = 0; i < samples->size(); ++i)
            (*samples)[i] = r.excluded.empty() || !r.excluded[i] ? r.component(component_, i) : std::complex<float>(0.0f, 0.0f);
        samples_ = samples;
        const bool real = component_ == FieldComponent::EMagnitude || component_ == FieldComponent::HMagnitude || component_ == FieldComponent::PowerDensity;
        if (real && (quantity_ == Quantity::Phase || quantity_ == Quantity::Instantaneous))
            quantity_ = Quantity::Magnitude;  // magnitudes and power density have no phase
        fieldDisplay_.quantity = quantity_ == Quantity::Phase           ? render::FieldQuantity::Phase
                                 : quantity_ == Quantity::Instantaneous ? render::FieldQuantity::Instantaneous
                                 : quantity_ == Quantity::Decibels      ? render::FieldQuantity::Decibels
                                                                        : render::FieldQuantity::Magnitude;
        fieldDisplay_.decibelRange = 40.0;
        const double largest = r.largest(component_);
        fieldDisplay_.scale = normalized_ ? (largest > 0.0 ? largest : 1.0) : absoluteScale_;
    }
    emit displayChanged();
}

render::ComplexFieldGrid EmController::displayGrid() const
{
    render::ComplexFieldGrid grid;
    if (displayed_ < 0 || !samples_)
        return grid;
    const EmFieldResult& r = *results_[static_cast<size_t>(displayed_)];
    grid.origin = r.grid.origin;
    grid.axisU = r.grid.axisU;
    grid.axisV = r.grid.axisV;
    grid.axisW = r.grid.axisW;
    grid.nu = r.grid.nu;
    grid.nv = r.grid.nv;
    grid.nw = r.grid.nw;
    grid.owner = samples_;
    grid.samples = samples_->data();
    return grid;
}

QString EmController::displayDescription() const
{
    if (displayed_ < 0)
        return QStringLiteral("No electromagnetic field is displayed.");
    const EmFieldResult& r = *results_[static_cast<size_t>(displayed_)];
    const bool magnetic = component_ >= FieldComponent::HMagnitude && component_ <= FieldComponent::Hz;
    const QString unit = component_ == FieldComponent::PowerDensity ? QStringLiteral("W/m^2") : magnetic ? QStringLiteral("A/m") : QStringLiteral("V/m");
    QString what = QString::fromUtf8(std::string(toString(component_)).c_str());
    if (quantity_ == Quantity::Phase)
        what += QStringLiteral(", phase -180 to +180 degrees (cyclic colours)");
    else if (quantity_ == Quantity::Instantaneous)
        what += QStringLiteral(", instantaneous Re{X exp(j wt)}");
    else if (quantity_ == Quantity::Decibels)
        what += QStringLiteral(", level in dB: colours span the 40 dB below the colour-scale top");
    QString scale = normalized_ ? QStringLiteral("NORMALIZED to this result's maximum (%1 %2)").arg(fieldDisplay_.scale, 0, 'g', 4).arg(unit)
                                : QStringLiteral("ABSOLUTE, colour scale top = %1 %2").arg(fieldDisplay_.scale, 0, 'g', 4).arg(unit);
    if (quantity_ == Quantity::Phase)
        scale.clear();
    const QString source = QString::fromStdString(r.metadata.solverId) == QLatin1String(kEmFullWaveSolverId)
                               ? QStringLiteral("full-wave numerical (Yee FDTD)")
                               : QStringLiteral("analytical");
    return QStringLiteral("%1 — %2 at %3 MHz, peak phasors, %4. %5").arg(QString::fromStdString(r.name), what).arg(r.frequencyHz / 1e6, 0, 'g', 7)
        .arg(source, scale);
}

Status EmController::showPattern3D(const Id& id)
{
    auto grid = pattern(id);
    if (!grid)
        return Status::failure(grid.message());
    const SceneObject* o = document_->scene().find(id);
    const Mat4 world = document_->scene().worldMatrix(*o);
    const Vec3 centre = transformPoint(world, Vec3::Zero());
    Mat3 rotation = world.block<3, 3>(0, 0);
    for (int c = 0; c < 3; ++c)
        rotation.col(c).normalize();
    // Size: a little larger than the object, at least a quarter wavelength.
    double size = 0.1;
    if (o->state.geometry) {
        const Aabb b = o->state.geometry->tessellate().bounds();
        if (b.isValid())
            size = std::max(size, 0.75 * (b.max - b.min).norm());
    }
    if (const auto* array = dynamic_cast<const EmArrayComponent*>(o->state.component.get())) {
        const ArrayModel model = arrayModelOf(array->settings);
        double extent = 0.0;
        for (const ArrayElement& e : model.elements)
            extent = std::max(extent, e.position.norm());
        size = std::max(size, 1.2 * extent);
    }
    const PatternGrid& p = grid.value();
    auto mesh = std::make_shared<render::ScalarMesh>();
    mesh->scale = 1.0f;
    for (int i = 0; i < p.thetaCount; ++i)
        for (int j = 0; j < p.phiCount; ++j) {
            const double t = i * p.stepRad, ph = j * p.stepRad, r = p.value(i, j);
            const Vec3 local = size * r * directionOf(t, ph);
            mesh->positions.push_back((centre + rotation * local).cast<float>());
            mesh->normals.push_back((rotation * directionOf(t, ph)).cast<float>());
            mesh->values.push_back(static_cast<float>(r));
        }
    const auto at = [&](int i, int j) { return static_cast<std::uint32_t>(i * p.phiCount + (j % p.phiCount)); };
    for (int i = 0; i + 1 < p.thetaCount; ++i)
        for (int j = 0; j < p.phiCount; ++j) {
            mesh->indices.insert(mesh->indices.end(), {at(i, j), at(i + 1, j), at(i + 1, j + 1)});
            mesh->indices.insert(mesh->indices.end(), {at(i, j), at(i + 1, j + 1), at(i, j + 1)});
        }
    patternMesh_ = mesh;
    // The local axes of the pattern frame.
    patternLines_.clear();
    const Eigen::Vector4f colours[3] = {{0.9f, 0.3f, 0.3f, 1.0f}, {0.3f, 0.8f, 0.3f, 1.0f}, {0.3f, 0.5f, 1.0f, 1.0f}};
    for (int c = 0; c < 3; ++c) {
        render::LineItem line;
        line.from = centre;
        line.to = centre + 1.1 * size * rotation.col(c);
        line.color = colours[c];
        patternLines_.push_back(line);
    }
    emit patternChanged();
    return Status::success();
}

void EmController::hidePattern3D()
{
    patternMesh_.reset();
    patternLines_.clear();
    emit patternChanged();
}
