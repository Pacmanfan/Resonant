#include "fdtd/FdtdSetup.h"

#include "acoustics/Components.h"
#include "acoustics/Propagation.h"
#include "geometry/SolidModel.h"
#include "manipulators/Manipulator.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <map>

namespace resonant::fdtd {
namespace {

constexpr double pi = 3.14159265358979323846;

std::string format(const char* pattern, ...)
{
    char buffer[600];
    va_list arguments;
    va_start(arguments, pattern);
    std::vsnprintf(buffer, sizeof buffer, pattern, arguments);
    va_end(arguments);
    return buffer;
}

bool carrierFromEmitter(WaveformKind kind)
{
    return kind == WaveformKind::Continuous || kind == WaveformKind::ToneBurst || kind == WaveformKind::GaussianPulse;
}

Waveform waveformFor(const FdtdSettings& settings, const acoustics::PointSource& source)
{
    Waveform w = settings.waveform;
    if (carrierFromEmitter(w.kind)) {
        w.frequencyHz = source.frequencyHz;
        w.phase = settings.waveform.phase + source.phaseRad;
    }
    return w;
}

// How an object's cells behave.
struct Representation {
    enum class Kind { Rigid, PressureRelease, Fluid } kind = Kind::Rigid;
    CellMaterial material;
    std::string description;
    std::string warning;
};

Representation representationOf(const SceneObject& object, const MaterialLibrary& materials)
{
    Representation r;
    const Material* material = !object.state.materialId.isNull() ? materials.find(object.state.materialId) : nullptr;
    const PropertyValue* density = material ? material->property(PhysicsDomain::Acoustic, "density") : nullptr;
    const PropertyValue* speed = material ? material->property(PhysicsDomain::Acoustic, "soundSpeed") : nullptr;
    const PropertyValue* attenuation = material ? material->property(PhysicsDomain::Acoustic, "attenuation") : nullptr;
    const bool usable = density && speed && density->value > 0.0 && speed->value > 0.0;
    const auto fluid = [&] {
        r.kind = Representation::Kind::Fluid;
        r.material = CellMaterial{material->name(), density->value, speed->value, attenuation ? attenuation->value : 0.0, false, false};
        r.description = format("material '%s' as a fluid: density %.6g kg/m^3, speed of sound %.6g m/s%s (no shear waves)",
                               material->name().c_str(), density->value, speed->value,
                               attenuation && attenuation->value > 0.0 ? format(", attenuation %.4g Np/m", attenuation->value).c_str() : "");
    };
    const auto rigid = [&](const char* why) {
        r.kind = Representation::Kind::Rigid;
        r.material = CellMaterial{"rigid", 0.0, 0.0, 0.0, true, false};
        r.description = std::string("rigid body (") + why + ")";
    };

    const auto* manipulator = dynamic_cast<const manipulators::ManipulatorComponent*>(object.state.component.get());
    if (!manipulator) {
        if (usable)
            fluid();
        else
            rigid(material ? "its material has no acoustic density and speed of sound" : "no material assigned");
        return r;
    }
    using manipulators::BoundaryCondition;
    using manipulators::Category;
    if (manipulator->settings.category == Category::Lens) {
        // A lens works by transmission: it has to be penetrable.
        if (usable) {
            fluid();
        } else {
            rigid("a lens needs a material with an acoustic density and speed of sound");
            r.warning = "has no usable material, so it is simulated as a rigid body and does not act as a lens";
        }
        return r;
    }
    switch (manipulator->settings.boundary) {
    case BoundaryCondition::Rigid:
        rigid("rigid boundary");
        break;
    case BoundaryCondition::PressureRelease:
        r.kind = Representation::Kind::PressureRelease;
        r.material = CellMaterial{"pressure release", 0.0, 0.0, 0.0, false, true};
        r.description = "pressure-release body (pressure held at zero)";
        break;
    case BoundaryCondition::Impedance:
        if (usable) {
            fluid();
            r.description += "; the wave enters the material, which is more than a locally reacting impedance surface";
        } else {
            rigid("impedance boundary without a material");
            r.warning = "has an impedance boundary but no usable material, and is simulated as rigid";
        }
        break;
    case BoundaryCondition::Absorbing:
        rigid("an ideal absorbing surface cannot be represented on the grid");
        r.warning = "is set to an ideal absorbing boundary, which this solver cannot represent; it is simulated as RIGID";
        break;
    }
    return r;
}

struct Domain {
    Vec3 origin;           // world position of the corner of cell (0, 0, 0)
    Vec3 eu, ev, ew;       // unit vectors of the grid axes
    Vec3 extent;           // region size along them, m
    bool volume = false;
};

Domain domainOf(const acoustics::RegionSettings& region, const Mat4& world)
{
    Domain d;
    const Vec3 u = transformDirection(world, region.localAxisU());
    const Vec3 v = transformDirection(world, region.localAxisV());
    const Vec3 w = transformDirection(world, region.localAxisW());
    d.volume = region.isVolume();
    d.extent = Vec3(u.norm(), v.norm(), d.volume ? w.norm() : 0.0);
    d.eu = u.normalized();
    d.ev = v.normalized();
    d.ew = d.volume ? w.normalized() : d.eu.cross(d.ev).normalized();
    d.origin = transformPoint(world, region.localOrigin());
    return d;
}

}  // namespace

// --- settings ---------------------------------------------------------------------------

Status FdtdSettings::validate() const
{
    if (!(std::isfinite(pointsPerWavelength) && pointsPerWavelength >= 4.0 && pointsPerWavelength <= 200.0))
        return Status::failure("the resolution must be between 4 and 200 cells per wavelength");
    if (!(std::isfinite(courantFraction) && courantFraction > 0.0 && courantFraction <= 1.0))
        return Status::failure("the time step must be a fraction of the stability limit between 0 (exclusive) and 1");
    if (Status status = waveform.validate(); !status)
        return status;
    if (!(std::isfinite(duration) && duration >= 0.0))
        return Status::failure("the duration must be zero (automatic) or positive");
    if (!(std::isfinite(boundaryImpedance) && boundaryImpedance > 0.0))
        return Status::failure("the boundary impedance must be positive");
    if (absorbingCells < 4 || absorbingCells > 200)
        return Status::failure("an absorbing layer must be between 4 and 200 cells thick");
    if (snapshotCount < 0 || snapshotCount > 2000)
        return Status::failure("the number of snapshots must be between 0 and 2000");
    if (!(std::isfinite(harmonicPeriods) && harmonicPeriods >= 1.0 && harmonicPeriods <= 1000.0))
        return Status::failure("the analysis window must be between 1 and 1000 periods");
    if (memoryLimitBytes < 1024 * 1024)
        return Status::failure("the memory limit must be at least 1 MiB");
    return Status::success();
}

nlohmann::json toJson(const FdtdSettings& s)
{
    nlohmann::json boundary = nlohmann::json::array();
    for (const BoundaryKind kind : s.boundary)
        boundary.push_back(std::string(toString(kind)));
    return {{"pointsPerWavelength", s.pointsPerWavelength},
            {"courantFraction", s.courantFraction},
            {"waveform", toJson(s.waveform)},
            {"duration", s.duration},
            {"boundary", boundary},
            {"boundaryImpedance", s.boundaryImpedance},
            {"absorbingCells", s.absorbingCells},
            {"snapshotCount", s.snapshotCount},
            {"harmonicPeriods", s.harmonicPeriods},
            {"memoryLimitBytes", s.memoryLimitBytes}};
}

Result<FdtdSettings> fdtdSettingsFromJson(const nlohmann::json& value)
{
    if (!value.is_object())
        return Result<FdtdSettings>::failure("solver settings must be an object");
    FdtdSettings s;
    try {
        s.pointsPerWavelength = value.value("pointsPerWavelength", s.pointsPerWavelength);
        s.courantFraction = value.value("courantFraction", s.courantFraction);
        if (value.contains("waveform")) {
            auto waveform = waveformFromJson(value["waveform"]);
            if (!waveform)
                return Result<FdtdSettings>::failure(waveform.message());
            s.waveform = waveform.take();
        }
        s.duration = value.value("duration", s.duration);
        if (value.contains("boundary")) {
            const nlohmann::json boundary = value["boundary"];
            if (!boundary.is_array() || boundary.size() != 6)
                return Result<FdtdSettings>::failure("the boundary list needs six entries");
            for (size_t i = 0; i < 6; ++i) {
                auto kind = boundaryKindFromString(boundary[i].get<std::string>());
                if (!kind)
                    return Result<FdtdSettings>::failure(kind.message());
                s.boundary[i] = kind.value();
            }
        }
        s.boundaryImpedance = value.value("boundaryImpedance", s.boundaryImpedance);
        s.absorbingCells = value.value("absorbingCells", s.absorbingCells);
        s.snapshotCount = value.value("snapshotCount", s.snapshotCount);
        s.harmonicPeriods = value.value("harmonicPeriods", s.harmonicPeriods);
        s.memoryLimitBytes = value.value("memoryLimitBytes", s.memoryLimitBytes);
    } catch (const nlohmann::json::exception& error) {
        return Result<FdtdSettings>::failure(std::string("invalid solver settings: ") + error.what());
    }
    if (Status status = s.validate(); !status)
        return Result<FdtdSettings>::failure(status.message());
    return s;
}

FdtdSettings fdtdSettingsOf(const Project& project)
{
    const auto it = project.extensions.find(kFdtdSettingsKey);
    if (it == project.extensions.end())
        return FdtdSettings();
    auto parsed = fdtdSettingsFromJson(*it);
    return parsed ? parsed.take() : FdtdSettings();
}

void storeFdtdSettings(Project& project, const FdtdSettings& settings)
{
    project.extensions[kFdtdSettingsKey] = toJson(settings);
}

// --- plan ---------------------------------------------------------------------------------

std::string FdtdPlan::summary() const
{
    std::string text = format(
        "%dD grid %d x %d%s cells of %.4g mm (%zu cells with absorbing layers), %ld time steps of %.4g us (%.4g ms simulated).\n"
        "Resolution: %.3g cells per wavelength at %.6g Hz in the slowest medium (%.6g m/s); numerical phase velocity error up to %.2g %%.\n"
        "Time step: %.3g of the stability limit (%.4g us); Courant number c_max dt / dx = %.3f.\n"
        "Memory: %.1f MiB solver state + %.1f MiB recordings. Work: %.3g cell updates, roughly %.3g s on one core.",
        dimensions, nx, ny, dimensions == 3 ? format(" x %d", nz).c_str() : "", dx * 1e3, cellCount, steps, dt * 1e6, duration * 1e3,
        pointsPerWavelength, highestFrequency, slowestSpeed, 100.0 * phaseVelocityError, stabilityLimit > 0.0 ? dt / stabilityLimit : 0.0,
        stabilityLimit * 1e6, courantNumber, static_cast<double>(stateBytes) / 1048576.0, static_cast<double>(recordingBytes) / 1048576.0,
        cellUpdates, estimatedSeconds);
    text += "\nDuration: " + durationBasis;
    return text;
}

Prepared prepare(const Project& project, const Id& regionId, const acoustics::Medium& medium, const FdtdSettings& settings)
{
    Prepared prepared;
    FdtdPlan& plan = prepared.plan;
    const auto refuse = [&](std::string why) {
        plan.allowed = false;
        plan.refusal = std::move(why);
        return prepared;
    };

    if (Status status = settings.validate(); !status)
        return refuse(status.message());
    if (Status status = medium.validate(); !status)
        return refuse(status.message());
    const SceneObject* regionObject = project.scene.find(regionId);
    const auto* regionComponent = regionObject ? dynamic_cast<const acoustics::RegionComponent*>(regionObject->state.component.get()) : nullptr;
    if (!regionComponent)
        return refuse("select a field region: it defines the computational domain");
    const acoustics::RegionSettings& region = regionComponent->settings;
    const Domain domain = domainOf(region, project.scene.worldMatrix(*regionObject));
    plan.dimensions = domain.volume ? 3 : 2;

    // --- sources ---
    const std::vector<acoustics::PointSource> allSources = acoustics::collectSources(project.scene);
    if (allSources.empty())
        return refuse("there is no active emitter in the scene");
    double highest = 0.0;
    bool directional = false;
    for (const acoustics::PointSource& source : allSources) {
        highest = std::max(highest, waveformFor(settings, source).highestFrequency());
        directional = directional || source.directivity.model != acoustics::DirectivityModel::Omnidirectional;
    }
    plan.highestFrequency = highest;

    // --- objects and materials ---
    struct Body {
        const SceneObject* object;
        Mat4 inverseWorld;
        Vec3 centre;
        double radius;
        SolidModel model;
        std::uint8_t material;
    };
    std::vector<CellMaterial> materials{CellMaterial{medium.name, medium.density, medium.soundSpeed, medium.attenuation, false, false}};
    prepared.materialNames = {medium.name};
    std::vector<std::pair<const SceneObject*, Representation>> candidates;
    project.scene.forEach([&](const SceneObject& object) {
        if (!object.state.geometry)
            return;
        if (!project.scene.isEffectivelyVisible(object)) {
            plan.objects.push_back("'" + object.state.name + "' is hidden and was left out");
            return;
        }
        candidates.emplace_back(&object, representationOf(object, project.materials));
    });
    double slowest = medium.soundSpeed, fastest = medium.soundSpeed;
    for (const auto& [object, representation] : candidates)
        if (representation.kind == Representation::Kind::Fluid) {
            slowest = std::min(slowest, representation.material.soundSpeed);
            fastest = std::max(fastest, representation.material.soundSpeed);
        }
    // Resolution and time step are set for every material present in the
    // scene, whether or not it ends up inside the domain: the plan must not
    // depend on rasterization.
    plan.slowestSpeed = slowest;
    plan.fastestSpeed = fastest;
    plan.shortestWavelength = slowest / highest;
    plan.dx = plan.shortestWavelength / settings.pointsPerWavelength;
    plan.nx = std::max(2, static_cast<int>(std::lround(domain.extent.x() / plan.dx)));
    plan.ny = std::max(2, static_cast<int>(std::lround(domain.extent.y() / plan.dx)));
    plan.nz = domain.volume ? std::max(3, static_cast<int>(std::lround(domain.extent.z() / plan.dx))) : 1;
    plan.pointsPerWavelength = settings.pointsPerWavelength;
    plan.stabilityLimit = stabilityLimit(plan.dx, fastest, plan.dimensions);
    plan.dt = settings.courantFraction * plan.stabilityLimit;
    plan.courantNumber = fastest * plan.dt / plan.dx;
    plan.phaseVelocityError = 1.0 - axialPhaseVelocityRatio(settings.pointsPerWavelength, slowest * plan.dt / plan.dx);

    GridSpec grid;
    grid.nx = plan.nx;
    grid.ny = plan.ny;
    grid.nz = plan.nz;
    grid.dx = plan.dx;
    grid.dt = plan.dt;
    grid.boundary = settings.boundary;
    grid.impedance.fill(settings.boundaryImpedance);
    grid.pmlCells = settings.absorbingCells;
    plan.totalCells = grid.totalCells();
    plan.cellCount = static_cast<size_t>(plan.totalCells[0]) * static_cast<size_t>(plan.totalCells[1]) * static_cast<size_t>(plan.totalCells[2]);

    // Frame of the cell centres.
    const Vec3 corner = domain.origin + 0.5 * plan.dx * (domain.eu + domain.ev + (domain.volume ? domain.ew : Vec3::Zero().eval()));
    plan.frame.origin = corner;
    plan.frame.axisU = domain.eu * plan.dx * (plan.nx - 1);
    plan.frame.axisV = domain.ev * plan.dx * (plan.ny - 1);
    plan.frame.axisW = domain.volume ? (domain.ew * plan.dx * (plan.nz - 1)).eval() : Vec3::Zero().eval();
    plan.frame.nu = plan.nx;
    plan.frame.nv = plan.ny;
    plan.frame.nw = plan.nz;
    const auto cellOf = [&](const Vec3& world, int& i, int& j, int& k, double& outOfPlane) {
        const Vec3 d = world - domain.origin;
        i = static_cast<int>(std::floor(d.dot(domain.eu) / plan.dx));
        j = static_cast<int>(std::floor(d.dot(domain.ev) / plan.dx));
        k = domain.volume ? static_cast<int>(std::floor(d.dot(domain.ew) / plan.dx)) : 0;
        outOfPlane = domain.volume ? 0.0 : std::abs(d.dot(domain.ew));
        return i >= 0 && i < plan.nx && j >= 0 && j < plan.ny && k >= 0 && k < plan.nz;
    };

    // --- duration ---
    const double diagonal = plan.dx * std::sqrt(static_cast<double>(plan.nx) * plan.nx + static_cast<double>(plan.ny) * plan.ny
                                                + (domain.volume ? static_cast<double>(plan.nz) * plan.nz : 0.0));
    const double crossing = diagonal / slowest;
    double longestDelay = 0.0;
    for (const acoustics::PointSource& source : allSources)
        longestDelay = std::max(longestDelay, source.delaySec);
    const bool continuous = settings.waveform.kind == WaveformKind::Continuous;
    const double analysisFrequency = continuous ? allSources.front().frequencyHz : 0.0;
    double signalTime = 0.0;  // time until the signal is fully on (continuous) or over (finite)
    if (continuous)
        signalTime = settings.waveform.rampCycles / analysisFrequency;
    else
        signalTime = settings.waveform.repetitionPeriod > 0.0 ? settings.waveform.repetitionPeriod : settings.waveform.length();
    const double window = continuous ? settings.harmonicPeriods / analysisFrequency : 0.0;
    const double automatic = longestDelay + signalTime + 2.0 * crossing + window;
    if (settings.duration > 0.0) {
        plan.duration = settings.duration;
        plan.durationBasis = format("%.4g ms as set (the automatic choice would be %.4g ms).", settings.duration * 1e3, automatic * 1e3);
    } else {
        plan.duration = automatic;
        plan.durationBasis = format("automatic: longest delay %.4g ms + signal %.4g ms + two crossings of the domain diagonal %.4g ms%s.",
                                    longestDelay * 1e3, signalTime * 1e3, 2.0 * crossing * 1e3,
                                    continuous ? format(" + analysis window %.4g ms", window * 1e3).c_str() : "");
    }
    plan.steps = std::max<long>(1, std::lround(std::ceil(plan.duration / plan.dt)));
    plan.analysisFrequency = analysisFrequency;

    // --- cost ---
    const size_t interior = grid.interiorCells();
    plan.stateBytes = grid.stateBytes();
    plan.cellUpdates = static_cast<double>(plan.cellCount) * static_cast<double>(plan.steps);
    plan.estimatedSeconds = plan.cellUpdates / kAssumedUpdateRate;
    plan.recordingBytes = static_cast<size_t>(settings.snapshotCount) * interior * sizeof(float)
                          + (continuous ? interior * (2 * sizeof(double) + 2 * sizeof(float)) : 0) + interior;

    // --- warnings that do not depend on the grid contents ---
    if (settings.pointsPerWavelength < 10.0)
        plan.warnings.push_back(format("%.3g cells per wavelength is coarse: the numerical phase velocity is up to %.2g %% low, which "
                                       "shifts interference patterns and resonances. 10 to 20 cells per wavelength are usual.",
                                       settings.pointsPerWavelength, 100.0 * plan.phaseVelocityError));
    if (plan.duration < longestDelay + crossing)
        plan.warnings.push_back(format("The simulated time (%.4g ms) is shorter than one crossing of the domain (%.4g ms): parts of the "
                                       "domain are not reached by the wave.", plan.duration * 1e3, (longestDelay + crossing) * 1e3));
    bool closed = false;
    for (int face = 0; face < (domain.volume ? 6 : 4); ++face)
        closed = closed || settings.boundary[static_cast<size_t>(face)] != BoundaryKind::Absorbing;
    if (continuous) {
        if (plan.duration < longestDelay + signalTime + crossing + window)
            plan.warnings.push_back("The steady-state analysis window starts before the wave has crossed the domain: the harmonic field is "
                                    "not a steady state. Lengthen the simulated time.");
        if (closed)
            plan.warnings.push_back("At least one face of the domain reflects. With a continuous signal the field keeps building up, and "
                                    "the harmonic amplitudes depend on the simulated time unless losses bring it to a steady state.");
        for (const acoustics::PointSource& source : allSources)
            if (std::abs(source.frequencyHz - analysisFrequency) > 1e-9 * analysisFrequency) {
                plan.warnings.push_back(format("Emitters radiate at different frequencies; the steady-state field is analysed at %.6g Hz only.",
                                               analysisFrequency));
                break;
            }
    }
    plan.assumptions = {
        "Full-wave solution of the linear acoustic equations for a fluid at rest (no flow, no nonlinearity, no thermoviscous boundary layers).",
        "Finite-difference time-domain method on a uniform staggered grid, second-order accurate; curved and inclined surfaces are "
        "represented in steps of one cell.",
        format("Emitters are point (monopole) volume-velocity sources placed in the nearest cell, up to %.3g mm from their position.",
               0.87 * plan.dx * 1e3),
        "Solid materials are treated as fluids with their density and speed of sound: no shear waves, no structural vibration."};
    if (!domain.volume)
        plan.assumptions.push_back(
            "Two-dimensional calculation: nothing varies perpendicular to the plane. Every emitter is a LINE source and every object an "
            "infinitely long cylinder with the cross-section the plane cuts from it. Amplitudes decay with the square root of distance, "
            "not as for a point source; each emitter's strength is set so that it gives its amplitude at 1 m at its own frequency.");
    if (directional)
        plan.assumptions.push_back("Emitter directivity models are NOT applied: every emitter radiates as a monopole.");
    if (!carrierFromEmitter(settings.waveform.kind))
        plan.assumptions.push_back("The signal is used as defined for every emitter: emitter frequency and phase settings do not apply, "
                                   "amplitude and time delay do.");

    // --- limits ---
    if (plan.stateBytes + plan.recordingBytes > settings.memoryLimitBytes)
        return refuse(format("The calculation needs %.0f MiB (%zu cells), more than the memory limit of %.0f MiB. Reduce the region, the "
                             "frequency, the cells per wavelength or the number of snapshots, or raise the limit. The resolution is not "
                             "lowered automatically.",
                             static_cast<double>(plan.stateBytes + plan.recordingBytes) / 1048576.0, plan.cellCount,
                             static_cast<double>(settings.memoryLimitBytes) / 1048576.0));
    if (plan.cellUpdates > 5e11)
        return refuse(format("The calculation needs %.3g cell updates (about %.0f hours on one core). Reduce the region, the frequency, "
                             "the resolution or the simulated time.", plan.cellUpdates, plan.estimatedSeconds / 3600.0));
    if (plan.estimatedSeconds > 600.0)
        plan.warnings.push_back(format("This is a long calculation: roughly %.0f minutes on one core.", plan.estimatedSeconds / 60.0));

    // --- rasterize the bodies ---
    const double minThickness = 2.0 * plan.dx;
    std::vector<Body> bodies;
    std::map<std::string, std::uint8_t> materialIndex;
    for (const auto& [object, representation] : candidates) {
        auto model = solidModelOf(*object->state.geometry, minThickness);
        if (!model) {
            plan.objects.push_back("'" + object->state.name + "' has no simulation geometry for this solver and was left out");
            plan.warnings.push_back("'" + object->state.name + "' (" + object->state.geometry->kind().displayName
                                    + ") cannot be represented by this solver and does NOT affect the result.");
            continue;
        }
        const std::string key = representation.kind == Representation::Kind::Fluid
                                    ? format("fluid:%s:%.17g:%.17g:%.17g", representation.material.name.c_str(), representation.material.density,
                                             representation.material.soundSpeed, representation.material.attenuation)
                                    : (representation.kind == Representation::Kind::Rigid ? std::string("rigid") : std::string("release"));
        auto found = materialIndex.find(key);
        if (found == materialIndex.end()) {
            if (materials.size() >= 255)
                return refuse("the scene uses more than 254 different materials, more than the grid can index");
            found = materialIndex.emplace(key, static_cast<std::uint8_t>(materials.size())).first;
            materials.push_back(representation.material);
            prepared.materialNames.push_back(representation.material.name);
        }
        const Mat4 world = project.scene.worldMatrix(*object);
        // Uniform scale is the only scale a bounding radius survives; use the largest axis scale.
        const double scale = std::max({transformDirection(world, Vec3::UnitX()).norm(), transformDirection(world, Vec3::UnitY()).norm(),
                                       transformDirection(world, Vec3::UnitZ()).norm()});
        bodies.push_back({object, world.inverse(), transformPoint(world, Vec3::Zero()), model->boundingRadius * scale, *model, found->second});
        std::string line = "'" + object->state.name + "': " + representation.description;
        if (!model->note.empty()) {
            line += "; " + model->note;
            plan.warnings.push_back("'" + object->state.name + "': " + model->note + ".");
        }
        if (!representation.warning.empty())
            plan.warnings.push_back("'" + object->state.name + "' " + representation.warning + ".");
        plan.objects.push_back(std::move(line));
    }

    grid.materials = materials;
    if (!bodies.empty()) {
        grid.cellMaterial.assign(interior, 0);
        for (const Body& body : bodies) {
            size_t filled = 0;
            for (int k = 0; k < plan.nz; ++k)
                for (int j = 0; j < plan.ny; ++j) {
                    const Vec3 row = corner + plan.dx * (j * domain.ev + (domain.volume ? (k * domain.ew).eval() : Vec3::Zero().eval()));
                    // Skip rows that cannot reach the body.
                    const Vec3 toCentre = body.centre - row;
                    const double along = toCentre.dot(domain.eu);
                    const double off2 = toCentre.squaredNorm() - along * along;
                    if (off2 > body.radius * body.radius)
                        continue;
                    const double half = std::sqrt(body.radius * body.radius - off2);
                    const int first = std::max(0, static_cast<int>(std::floor((along - half) / plan.dx)));
                    const int last = std::min(plan.nx - 1, static_cast<int>(std::ceil((along + half) / plan.dx)));
                    for (int i = first; i <= last; ++i)
                        if (body.model.contains(transformPoint(body.inverseWorld, row + plan.dx * i * domain.eu))) {
                            grid.cellMaterial[grid.interiorIndex(i, j, k)] = body.material;
                            ++filled;
                        }
                }
            if (filled == 0)
                plan.objects.push_back("'" + body.object->state.name + "' does not intersect the domain");
        }
        for (const std::uint8_t m : grid.cellMaterial) {
            if (m == 0)
                continue;
            if (materials[m].rigid)
                ++plan.rigidCells;
            else if (materials[m].pressureRelease)
                ++plan.pressureReleaseCells;
            else
                ++plan.materialCells;
        }
    }
    prepared.materialMap = grid.cellMaterial;

    // --- sources on the grid ---
    Problem& problem = prepared.problem;
    for (const acoustics::PointSource& source : allSources) {
        int i, j, k;
        double outOfPlane;
        if (!cellOf(source.position, i, j, k, outOfPlane) || outOfPlane > plan.dx) {
            ++plan.excludedSources;
            continue;
        }
        const size_t cell = grid.interiorIndex(i, j, k);
        const CellMaterial& host = materials[grid.cellMaterial.empty() ? 0 : grid.cellMaterial[cell]];
        if (!host.isFluid()) {
            ++plan.excludedSources;
            plan.warnings.push_back("'" + source.label + "' lies inside a rigid or pressure-release body and radiates nothing.");
            continue;
        }
        const Waveform waveform = waveformFor(settings, source);
        double strength;
        if (domain.volume) {
            strength = 4.0 * pi * source.amplitudePa / host.density;
        } else {
            // |p(r)| = (rho S / 4) |H0(2)(k r)|: choose S for the amplitude at 1 m.
            const double kr = 2.0 * pi * waveform.frequencyHz / host.soundSpeed;
            const double hankel = std::hypot(std::cyl_bessel_j(0.0, kr), std::cyl_neumann(0.0, kr));
            strength = 4.0 * source.amplitudePa / (host.density * hankel);
        }
        problem.sources.push_back({i, j, k, strength, source.delaySec, waveform});
        prepared.sources.push_back(source);
    }
    plan.sourceCount = problem.sources.size();
    if (plan.excludedSources > 0)
        plan.warnings.push_back(format("%zu emitter(s) lie outside the domain%s and are NOT simulated.", plan.excludedSources,
                                       domain.volume ? "" : " or off its plane"));
    if (problem.sources.empty())
        return refuse(domain.volume ? "no active emitter lies inside the region"
                                    : "no active emitter lies in the plane of the region (within one cell of it)");

    // --- probes and targets ---
    project.scene.forEach([&](const SceneObject& object) {
        const bool probe = dynamic_cast<const acoustics::ProbeComponent*>(object.state.component.get()) != nullptr;
        const bool target = dynamic_cast<const acoustics::TargetComponent*>(object.state.component.get()) != nullptr;
        if (!probe && !target)
            return;
        const Vec3 position = transformPoint(project.scene.worldMatrix(object), Vec3::Zero());
        int i, j, k;
        double outOfPlane;
        if (!cellOf(position, i, j, k, outOfPlane) || outOfPlane > plan.dx)
            return;
        problem.probes.push_back({object.id(), object.state.name, plan.frame.point(i, j, k), i, j, k});
    });
    plan.probeCount = problem.probes.size();
    plan.recordingBytes += plan.probeCount * static_cast<size_t>(plan.steps) * sizeof(double);

    problem.grid = std::move(grid);
    problem.steps = plan.steps;
    problem.recording.snapshotCount = settings.snapshotCount;
    problem.recording.harmonicFrequencyHz = plan.analysisFrequency;
    problem.recording.harmonicPeriods = settings.harmonicPeriods;
    if (Status status = problem.grid.validate(); !status)
        return refuse(status.message());
    return prepared;
}

solvers::ProblemTraits traitsOf(const Project& project, const Id& regionId, const acoustics::Medium& medium,
                                const FdtdSettings& settings, const FdtdPlan& plan)
{
    solvers::ProblemTraits traits;
    const SceneObject* regionObject = project.scene.find(regionId);
    const auto* region = regionObject ? dynamic_cast<const acoustics::RegionComponent*>(regionObject->state.component.get()) : nullptr;
    traits.dimensions = region && !region->settings.isVolume() ? 2 : 3;
    project.scene.forEach([&](const SceneObject& object) {
        if (!object.state.geometry || !project.scene.isEffectivelyVisible(object))
            return;
        const auto* manipulator = dynamic_cast<const manipulators::ManipulatorComponent*>(object.state.component.get());
        if (manipulator && manipulator->settings.category == manipulators::Category::Reflector)
            ++traits.reflectors;
        else if (manipulator)
            ++traits.otherManipulators;
        else
            ++traits.plainObjects;
        if (representationOf(object, project.materials).kind == Representation::Kind::Fluid)
            ++traits.penetrableObjects;
    });
    traits.transientSignal = settings.waveform.kind != WaveformKind::Continuous;
    for (int face = 0; face < (traits.dimensions == 3 ? 6 : 4); ++face)
        traits.closedDomain = traits.closedDomain || settings.boundary[static_cast<size_t>(face)] != BoundaryKind::Absorbing;
    traits.mediumAttenuation = medium.attenuation > 0.0;
    for (const acoustics::PointSource& source : acoustics::collectSources(project.scene))
        traits.directionalSources = traits.directionalSources || source.directivity.model != acoustics::DirectivityModel::Omnidirectional;
    if (plan.dx > 0.0 && plan.shortestWavelength > 0.0)
        traits.domainWavelengths = plan.dx * std::max({plan.nx, plan.ny, plan.nz}) / plan.shortestWavelength;
    traits.gridCellUpdates = plan.cellUpdates;
    return traits;
}

// --- result ---------------------------------------------------------------------------------

std::shared_ptr<const acoustics::FieldResult> FdtdResult::snapshotField(size_t index) const
{
    if (index >= output.snapshots.size())
        return nullptr;
    auto field = std::make_shared<acoustics::FieldResult>();
    field->id = Id::generate();
    field->name = name + format(" @ t = %.4g ms", output.snapshotTimes[index] * 1e3);
    field->metadata = metadata;
    field->frame = plan.frame;
    field->frequencyHz = plan.analysisFrequency > 0.0 ? plan.analysisFrequency : settings.waveform.frequencyHz;
    field->medium = medium;
    const std::vector<float>& snapshot = output.snapshots[index];
    field->pressure.resize(snapshot.size());
    field->nearSource.assign(snapshot.size(), 0);
    double sum = 0.0;
    for (size_t c = 0; c < snapshot.size(); ++c) {
        field->pressure[c] = {snapshot[c], 0.0f};
        const double magnitude = std::abs(snapshot[c]);
        sum += magnitude;
        if (magnitude > field->statistics.maxMagnitude) {
            field->statistics.maxMagnitude = magnitude;
            const int i = static_cast<int>(c % static_cast<size_t>(plan.nx));
            const int j = static_cast<int>((c / static_cast<size_t>(plan.nx)) % static_cast<size_t>(plan.ny));
            const int k = static_cast<int>(c / (static_cast<size_t>(plan.nx) * static_cast<size_t>(plan.ny)));
            field->statistics.maxPoint = plan.frame.point(i, j, k);
        }
    }
    field->statistics.meanMagnitude = snapshot.empty() ? 0.0 : sum / static_cast<double>(snapshot.size());
    field->assumptions = plan.assumptions;
    field->assumptions.insert(field->assumptions.begin(),
                              format("INSTANTANEOUS pressure at t = %.6g ms (a transient snapshot, not a harmonic amplitude): only the "
                                     "real part is meaningful.", output.snapshotTimes[index] * 1e3));
    return field;
}

Spectrum FdtdResult::probeSpectrum(size_t probe, Window window) const
{
    if (probe >= output.probes.size())
        return {};
    return computeSpectrum(output.probes[probe].pressure, output.dt, window);
}

double FdtdResult::finalEnergyFraction() const
{
    double largest = 0.0;
    for (const double e : output.energy)
        largest = std::max(largest, e);
    return largest > 0.0 && !output.energy.empty() ? output.energy.back() / largest : 0.0;
}

FdtdJob::FdtdJob(std::string name, Prepared prepared, FdtdSettings settings, acoustics::Medium medium, ResultMetadata metadata)
    : name_(std::move(name)), prepared_(std::move(prepared)), settings_(std::move(settings)), medium_(std::move(medium)),
      metadata_(std::move(metadata))
{
}

std::shared_ptr<const FdtdResult> FdtdJob::result() const
{
    const std::lock_guard lock(mutex_);
    return result_;
}

JobEnd FdtdJob::run(JobContext& context)
{
    auto simulated = simulate(name_, std::move(prepared_), settings_, medium_, metadata_, &context);
    if (!simulated)
        return simulated.message() == "cancelled" ? JobEnd::cancelled() : JobEnd::failed(simulated.message());
    {
        const std::lock_guard lock(mutex_);
        result_ = simulated.take();
    }
    return JobEnd::completed(format("%ld steps, peak %.6g Pa", result()->output.steps, result()->output.peakPressure));
}

Result<std::shared_ptr<const FdtdResult>> simulate(const std::string& name, Prepared prepared, const FdtdSettings& settings,
                                                   const acoustics::Medium& medium, ResultMetadata metadata, JobContext* context)
{
    using Failure = Result<std::shared_ptr<const FdtdResult>>;
    if (!prepared.plan.allowed)
        return Failure::failure(prepared.plan.refusal);
    auto result = std::make_shared<FdtdResult>();
    const RunEnd end = fdtd::run(prepared.problem, result->output, context);
    if (end.kind == RunEnd::Kind::Cancelled)
        return Failure::failure("cancelled");
    if (end.kind != RunEnd::Kind::Completed)
        return Failure::failure(end.message);

    const FdtdPlan& plan = prepared.plan;
    result->id = Id::generate();
    result->name = name;
    result->plan = plan;
    result->settings = settings;
    result->medium = medium;
    result->materialMap = std::move(prepared.materialMap);
    result->materialNames = std::move(prepared.materialNames);
    metadata.domain = PhysicsDomain::Acoustic;
    metadata.solverId = kFdtdSolverId;
    metadata.solverVersion = kFdtdSolverVersion;
    metadata.elapsedSeconds = result->output.elapsedSeconds;
    metadata.assumptions = plan.assumptions;
    nlohmann::json& configuration = metadata.configuration;
    configuration["settings"] = toJson(settings);
    configuration["medium"] = acoustics::toJson(medium);
    configuration["grid"] = {{"dimensions", plan.dimensions}, {"nx", plan.nx}, {"ny", plan.ny}, {"nz", plan.nz},
                             {"cellSize", plan.dx}, {"timeStep", plan.dt}, {"steps", plan.steps},
                             {"courantNumber", plan.courantNumber}, {"stabilityLimit", plan.stabilityLimit},
                             {"pointsPerWavelength", plan.pointsPerWavelength}, {"highestFrequencyHz", plan.highestFrequency},
                             {"phaseVelocityError", plan.phaseVelocityError}, {"totalCells", plan.cellCount},
                             {"rigidCells", plan.rigidCells}, {"pressureReleaseCells", plan.pressureReleaseCells},
                             {"materialCells", plan.materialCells}};
    configuration["objects"] = plan.objects;
    configuration["warnings"] = plan.warnings;
    configuration["sources"] = plan.sourceCount;
    configuration["excludedSources"] = plan.excludedSources;
    configuration["cellUpdatesPerSecond"] = result->output.cellUpdatesPerSecond;
    result->metadata = metadata;

    if (!result->output.harmonic.empty()) {
        auto field = std::make_shared<acoustics::FieldResult>();
        field->id = Id::generate();
        field->name = name;
        field->metadata = metadata;
        field->frame = plan.frame;
        field->frequencyHz = plan.analysisFrequency;
        field->medium = medium;
        field->sources = prepared.sources;
        field->excludedSources = plan.excludedSources;
        field->pressure.resize(result->output.harmonic.size());
        field->nearSource.assign(result->output.harmonic.size(), 0);
        // The source cells themselves are not field values to rank or scale by.
        for (const GridSource& s : prepared.problem.sources)
            field->nearSource[prepared.problem.grid.interiorIndex(s.i, s.j, s.k)] = 1;
        double sum = 0.0;
        size_t counted = 0;
        for (size_t c = 0; c < field->pressure.size(); ++c) {
            const std::complex<double> p = result->output.harmonic[c];
            field->pressure[c] = {static_cast<float>(p.real()), static_cast<float>(p.imag())};
            if (field->nearSource[c]) {
                ++field->statistics.nearSourceSamples;
                continue;
            }
            const double magnitude = std::abs(p);
            sum += magnitude;
            ++counted;
            if (magnitude > field->statistics.maxMagnitude) {
                field->statistics.maxMagnitude = magnitude;
                const int i = static_cast<int>(c % static_cast<size_t>(plan.nx));
                const int j = static_cast<int>((c / static_cast<size_t>(plan.nx)) % static_cast<size_t>(plan.ny));
                const int k = static_cast<int>(c / (static_cast<size_t>(plan.nx) * static_cast<size_t>(plan.ny)));
                field->statistics.maxPoint = plan.frame.point(i, j, k);
            }
        }
        field->statistics.meanMagnitude = counted ? sum / static_cast<double>(counted) : 0.0;
        field->assumptions = plan.assumptions;
        field->assumptions.insert(field->assumptions.begin(),
                                  format("Harmonic amplitude at %.6g Hz extracted from the time-domain run over its last %.4g ms "
                                         "(%.3g periods); it is a steady state only if the field had settled by then.",
                                         plan.analysisFrequency, (result->output.harmonicEnd - result->output.harmonicStart) * 1e3,
                                         settings.harmonicPeriods));
        result->harmonicField = field;
    }
    return std::shared_ptr<const FdtdResult>(result);
}

}  // namespace resonant::fdtd
