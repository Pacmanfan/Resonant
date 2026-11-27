#include "electromagnetics/EmFullWave.h"

#include "foundation/Transform.h"
#include "geometry/SolidModel.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdarg>
#include <cstdio>

namespace resonant::em {
namespace {

std::string format(const char* pattern, ...)
{
    char buffer[700];
    va_list arguments;
    va_start(arguments, pattern);
    std::vsnprintf(buffer, sizeof buffer, pattern, arguments);
    va_end(arguments);
    return buffer;
}

bool carrierFromRadiator(dsp::WaveformKind kind)
{
    return kind == dsp::WaveformKind::Continuous || kind == dsp::WaveformKind::ToneBurst || kind == dsp::WaveformKind::GaussianPulse;
}

dsp::Waveform signalFor(const EmFullWaveSettings& s, double frequencyHz, double phase)
{
    dsp::Waveform w = s.waveform;
    if (carrierFromRadiator(w.kind)) {
        w.frequencyHz = frequencyHz;
        w.phase = s.waveform.phase + phase;
    }
    return w;
}

struct Radiator {
    const SceneObject* object;
    enum class Kind { Dipole, Monopole, CurrentElement } kind;
    double frequencyHz, amplitude, phase, delay;
    double length = 0.0;        // dipole total length, monopole height
    double groundRadius = 0.0;  // monopole
    Vec3 centre, axis;          // world
};

}  // namespace

Status EmFullWaveSettings::validate() const
{
    if (!(cellsPerWavelength >= 6.0 && cellsPerWavelength <= 200.0))
        return Status::failure("the resolution must be between 6 and 200 cells per wavelength");
    if (!(courantFraction > 0.0 && courantFraction <= 1.0))
        return Status::failure("the time step must be a fraction of the stability limit in (0, 1]");
    if (!(duration >= 0.0) || absorbingCells < 4 || absorbingCells > 200 || !(harmonicPeriods >= 1.0) || snapshotCount < 0
        || snapshotCount > 2000 || memoryLimitBytes < 1024 * 1024)
        return Status::failure("invalid duration, absorbing layer, analysis window, snapshot count or memory limit");
    if (Status status = compute.validate(); !status)
        return status;
    return waveform.validate();
}

nlohmann::json toJson(const EmFullWaveSettings& s)
{
    nlohmann::json boundary = nlohmann::json::array();
    for (const YeeBoundary b : s.boundary)
        boundary.push_back(std::string(toString(b)));
    return {{"cellsPerWavelength", s.cellsPerWavelength}, {"courantFraction", s.courantFraction}, {"waveform", dsp::toJson(s.waveform)},
            {"duration", s.duration}, {"boundary", boundary}, {"absorbingCells", s.absorbingCells}, {"harmonicPeriods", s.harmonicPeriods},
            {"snapshotCount", s.snapshotCount}, {"memoryLimitBytes", s.memoryLimitBytes}, {"compute", compute::toJson(s.compute)}};
}

Result<EmFullWaveSettings> emFullWaveSettingsFromJson(const nlohmann::json& v)
{
    using Failure = Result<EmFullWaveSettings>;
    if (!v.is_object())
        return Failure::failure("full-wave settings must be an object");
    EmFullWaveSettings s;
    try {
        s.cellsPerWavelength = v.value("cellsPerWavelength", s.cellsPerWavelength);
        s.courantFraction = v.value("courantFraction", s.courantFraction);
        if (v.contains("waveform")) {
            auto w = dsp::waveformFromJson(v["waveform"]);
            if (!w)
                return Failure::failure(w.message());
            s.waveform = w.take();
        }
        s.duration = v.value("duration", s.duration);
        if (v.contains("boundary")) {
            const nlohmann::json b = v["boundary"];
            if (!b.is_array() || b.size() != 6)
                return Failure::failure("the boundary list needs six entries");
            for (size_t i = 0; i < 6; ++i) {
                auto kind = yeeBoundaryFromString(b[i].get<std::string>());
                if (!kind)
                    return Failure::failure(kind.message());
                s.boundary[i] = kind.value();
            }
        }
        s.absorbingCells = v.value("absorbingCells", s.absorbingCells);
        s.harmonicPeriods = v.value("harmonicPeriods", s.harmonicPeriods);
        s.snapshotCount = v.value("snapshotCount", s.snapshotCount);
        s.memoryLimitBytes = v.value("memoryLimitBytes", s.memoryLimitBytes);
        if (v.contains("compute")) {
            auto options = compute::computeOptionsFromJson(v["compute"]);
            if (!options)
                return Failure::failure(options.message());
            s.compute = options.value();
        }
    } catch (const nlohmann::json::exception& e) {
        return Failure::failure(std::string("invalid full-wave settings: ") + e.what());
    }
    if (Status status = s.validate(); !status)
        return Failure::failure(status.message());
    return s;
}

EmFullWaveSettings emFullWaveSettingsOf(const Project& project)
{
    const auto it = project.extensions.find(kEmFullWaveKey);
    if (it == project.extensions.end())
        return EmFullWaveSettings();
    auto parsed = emFullWaveSettingsFromJson(*it);
    return parsed ? parsed.take() : EmFullWaveSettings();
}

void storeEmFullWaveSettings(Project& project, const EmFullWaveSettings& settings)
{
    project.extensions[kEmFullWaveKey] = toJson(settings);
}

std::string EmFullWavePlan::summary() const
{
    return format("%dD Yee grid %d x %d%s cells of %.4g mm at %.6g MHz (%.3g cells per wavelength in the slowest material), %ld steps of "
                  "%.4g ps (%.4g ns), time step %.3g of the stability limit. Memory %.1f MiB; %.3g cell updates, roughly %.3g s on one core.\n"
                  "Runs on: %s.\nDuration: %s",
                  dimensions, nx, ny, dimensions == 3 ? format(" x %d", nz).c_str() : "", cell * 1e3, frequencyHz / 1e6, cellsPerWavelength,
                  steps, dt * 1e12, duration * 1e9, stabilityLimit > 0 ? dt / stabilityLimit : 0.0, stateBytes / 1048576.0, cellUpdates,
                  estimatedSeconds, backend.c_str(), durationBasis.c_str());
}

EmPrepared prepareFullWave(const Project& project, const DomainBox& domain, const EmFullWaveSettings& settings)
{
    EmPrepared prepared;
    EmFullWavePlan& plan = prepared.plan;
    const auto refuse = [&](std::string why) {
        plan.allowed = false;
        plan.refusal = std::move(why);
        return prepared;
    };
    if (Status status = settings.validate(); !status)
        return refuse(status.message());
    plan.dimensions = domain.volume ? 3 : 2;

    // Radiators.
    std::vector<Radiator> radiators;
    project.scene.forEach([&](const SceneObject& o) {
        if (!project.scene.isEffectivelyVisible(o))
            return;
        const Mat4 world = project.scene.worldMatrix(o);
        const Vec3 centre = transformPoint(world, Vec3::Zero()), axis = transformDirection(world, Vec3::UnitZ()).normalized();
        if (const auto* a = dynamic_cast<const AntennaComponent*>(o.state.component.get())) {
            if (!a->settings.active || !o.state.geometry)
                return;
            const std::string& kind = o.state.geometry->kindId();
            const double scale = transformDirection(world, Vec3::UnitZ()).norm();
            if (kind == "antenna.dipole")
                radiators.push_back({&o, Radiator::Kind::Dipole, a->settings.frequencyHz, a->settings.feedCurrent, a->settings.feedPhase,
                                     a->settings.delay, o.state.geometry->parameter("length") * scale, 0.0, centre, axis});
            else if (kind == "antenna.monopole")
                radiators.push_back({&o, Radiator::Kind::Monopole, a->settings.frequencyHz, a->settings.feedCurrent, a->settings.feedPhase,
                                     a->settings.delay, o.state.geometry->parameter("height") * scale,
                                     o.state.geometry->parameter("groundRadius") * transformDirection(world, Vec3::UnitX()).norm(), centre, axis});
            else
                plan.warnings.push_back("'" + o.state.name + "' (" + o.state.geometry->kind().displayName + ") is not represented by the full-wave solver.");
        } else if (const auto* s = dynamic_cast<const SourceComponent*>(o.state.component.get())) {
            if (!s->settings.active)
                return;
            if (s->settings.kind == SourceKind::ElectricDipole)
                radiators.push_back({&o, Radiator::Kind::CurrentElement, s->settings.frequencyHz, s->settings.amplitude, s->settings.phase,
                                     s->settings.delay, 0.0, 0.0, centre, axis});
            else
                plan.warnings.push_back("'" + o.state.name + "': " + std::string(toString(s->settings.kind)) + " sources are not represented by the full-wave solver.");
        } else if (dynamic_cast<const EmArrayComponent*>(o.state.component.get())) {
            plan.warnings.push_back("'" + o.state.name + "': arrays are analysed with the array-factor model and are not represented by the full-wave solver.");
        }
    });
    if (radiators.empty())
        return refuse("there is no dipole or monopole antenna and no electric-dipole source in the scene");
    plan.frequencyHz = radiators.front().frequencyHz;
    for (const Radiator& r : radiators)
        if (std::abs(r.frequencyHz - plan.frequencyHz) > 1e-9 * plan.frequencyHz) {
            plan.warnings.push_back(format("Radiators work at different frequencies; the steady-state field is analysed at %.6g MHz.", plan.frequencyHz / 1e6));
            break;
        }

    // Materials of the bodies.
    struct Body {
        const SceneObject* object;
        Mat4 inverse;
        Vec3 centre;
        double radius;
        SolidModel model;
        std::uint8_t material;
    };
    std::vector<YeeMaterial> materials{YeeMaterial{}};
    std::vector<std::pair<const SceneObject*, YeeMaterial>> candidates;
    double slowest = kSpeedOfLight;
    const double f = plan.frequencyHz, omega = 2.0 * kPi * f;
    project.scene.forEach([&](const SceneObject& o) {
        if (!o.state.geometry || !project.scene.isEffectivelyVisible(o))
            return;
        if (dynamic_cast<const AntennaComponent*>(o.state.component.get()))
            return;  // antennas are represented by their wires
        YeeMaterial m;
        const Material* record = o.state.materialId.isNull() ? nullptr : project.materials.find(o.state.materialId);
        auto em = record ? emMaterialOf(*record, f) : Result<EmMaterial>::failure("none");
        if (!record) {
            m = YeeMaterial{"perfect conductor", 1, 1, 0, true};
            plan.warnings.push_back("'" + o.state.name + "' has no material and is treated as a perfect electric conductor.");
        } else if (!em) {
            m = YeeMaterial{"perfect conductor", 1, 1, 0, true};
            plan.warnings.push_back("'" + o.state.name + "': material '" + record->name() + "' has no electromagnetic data; treated as a perfect electric conductor.");
        } else if (em.value().classify(f) == MaterialClass::GoodConductor) {
            m = YeeMaterial{record->name() + " (as perfect conductor)", 1, 1, 0, true};
            plan.objects.push_back(format("'%s': %s, a good conductor (skin depth %.3g um) represented as a perfect electric conductor",
                                          o.state.name.c_str(), record->name().c_str(), em.value().skinDepth(f) * 1e6));
        } else {
            const EmMaterial& e = em.value();
            const double sigma = e.conductivity + omega * kEpsilon0 * e.relativePermittivity * e.lossTangent;
            m = YeeMaterial{record->name(), std::max(1.0, e.relativePermittivity), e.relativePermeability, sigma, false};
            slowest = std::min(slowest, kSpeedOfLight / std::sqrt(m.relativePermittivity * m.relativePermeability));
            plan.objects.push_back(format("'%s': %s, er = %.4g, mur = %.4g, conductivity %.3g S/m%s", o.state.name.c_str(), record->name().c_str(),
                                          m.relativePermittivity, m.relativePermeability, sigma,
                                          e.lossTangent > 0 ? " (including the loss tangent at this frequency)" : ""));
            if (e.outsideValidity)
                plan.warnings.push_back("'" + o.state.name + "': the material's values are used outside their stated frequency range.");
            if (e.relativePermittivity < 1.0)
                plan.warnings.push_back("'" + o.state.name + "': relative permittivity below 1 is not supported and is raised to 1.");
        }
        candidates.emplace_back(&o, m);
    });

    // Resolution and time step.
    double highest = f;
    for (const Radiator& r : radiators)
        highest = std::max(highest, signalFor(settings, r.frequencyHz, 0.0).highestFrequency());
    plan.cell = slowest / highest / settings.cellsPerWavelength;
    plan.cellsPerWavelength = settings.cellsPerWavelength;
    plan.nx = std::max(2, static_cast<int>(std::lround(domain.size.x() / plan.cell)));
    plan.ny = std::max(2, static_cast<int>(std::lround(domain.size.y() / plan.cell)));
    plan.nz = domain.volume ? std::max(3, static_cast<int>(std::lround(domain.size.z() / plan.cell))) : 1;
    plan.stabilityLimit = yeeStabilityLimit(plan.cell, kSpeedOfLight, plan.dimensions);
    plan.dt = settings.courantFraction * plan.stabilityLimit;
    YeeSpec& spec = prepared.spec;
    spec.nx = plan.nx;
    spec.ny = plan.ny;
    spec.nz = plan.nz;
    spec.cell = plan.cell;
    spec.dt = plan.dt;
    spec.boundary = settings.boundary;
    spec.pmlCells = settings.absorbingCells;
    spec.compute = settings.compute;
    plan.grid.origin = domain.corner;
    plan.grid.axisU = domain.axis[0] * plan.cell * plan.nx;
    plan.grid.axisV = domain.axis[1] * plan.cell * plan.ny;
    plan.grid.axisW = domain.volume ? Vec3(domain.axis[2] * plan.cell * plan.nz) : Vec3::Zero();
    plan.grid.nu = plan.nx + 1;
    plan.grid.nv = plan.ny + 1;
    plan.grid.nw = domain.volume ? plan.nz + 1 : 1;

    // Duration.
    const double diagonal = plan.cell * std::sqrt(double(plan.nx) * plan.nx + double(plan.ny) * plan.ny + (domain.volume ? double(plan.nz) * plan.nz : 0.0));
    const bool continuous = settings.waveform.kind == dsp::WaveformKind::Continuous;
    double longestDelay = 0.0;
    for (const Radiator& r : radiators)
        longestDelay = std::max(longestDelay, r.delay);
    const double signal = continuous ? settings.waveform.rampCycles / f : signalFor(settings, f, 0.0).length();
    const double window = continuous ? settings.harmonicPeriods / f : 0.0;
    const double automatic = longestDelay + signal + 2.0 * diagonal / slowest + window;
    plan.duration = settings.duration > 0.0 ? settings.duration : automatic;
    plan.durationBasis = settings.duration > 0.0 ? format("%.4g ns as set (automatic would be %.4g ns)", settings.duration * 1e9, automatic * 1e9)
                                                 : format("automatic: delay + signal + two crossings of the domain + analysis window = %.4g ns", automatic * 1e9);
    plan.steps = std::max<long>(1, std::lround(std::ceil(plan.duration / plan.dt)));
    plan.stateBytes = spec.stateBytes();
    const double cells = static_cast<double>(plan.stateBytes) / (27.0 * sizeof(double) + 1.0);
    plan.cellUpdates = cells * static_cast<double>(plan.steps);
    plan.estimatedSeconds = plan.cellUpdates / 1.5e7;
    plan.backend = settings.compute.backend == compute::ComputeBackend::Cuda
                       ? "CUDA device " + std::to_string(settings.compute.device) + ", double precision"
                       : "CPU, double precision, " + std::to_string(compute::resolveThreads(settings.compute.threads)) + " thread(s)";
    if (Status status = compute::checkBackend(settings.compute, plan.stateBytes); !status)
        return refuse(status.message() + ". Choose the CPU, or free the device; the backend is never switched automatically.");
    if (plan.stateBytes > settings.memoryLimitBytes)
        return refuse(format("the grid needs %.0f MiB, more than the memory limit of %.0f MiB; reduce the region, the frequency or the "
                             "resolution. The resolution is not lowered automatically.",
                             plan.stateBytes / 1048576.0, settings.memoryLimitBytes / 1048576.0));
    if (plan.cellUpdates > 2e11)
        return refuse(format("the calculation needs %.3g cell updates; reduce the region, the frequency, the resolution or the time", plan.cellUpdates));
    if (settings.cellsPerWavelength < 10.0)
        plan.warnings.push_back("Fewer than 10 cells per wavelength: the Yee scheme's numerical dispersion shifts phases noticeably.");
    if (continuous && plan.duration < automatic * 0.99)
        plan.warnings.push_back("The simulated time is shorter than the automatic choice: the field may not have reached a steady state.");

    // Bodies.
    std::vector<Body> bodies;
    for (const auto& [object, m] : candidates) {
        auto model = solidModelOf(*object->state.geometry, plan.cell);
        if (!model) {
            plan.warnings.push_back("'" + object->state.name + "' has no simulation geometry and is left out.");
            continue;
        }
        if (!model->note.empty())
            plan.warnings.push_back("'" + object->state.name + "': " + model->note + ".");
        std::uint8_t index = 0;
        for (size_t i = 1; i < materials.size(); ++i)
            if (materials[i].name == m.name && materials[i].relativePermittivity == m.relativePermittivity && materials[i].conductivity == m.conductivity
                && materials[i].pec == m.pec)
                index = static_cast<std::uint8_t>(i);
        if (index == 0) {
            if (materials.size() >= 255)
                return refuse("too many different materials");
            materials.push_back(m);
            index = static_cast<std::uint8_t>(materials.size() - 1);
        }
        const Mat4 world = project.scene.worldMatrix(*object);
        const double scale = std::max({transformDirection(world, Vec3::UnitX()).norm(), transformDirection(world, Vec3::UnitY()).norm(),
                                       transformDirection(world, Vec3::UnitZ()).norm()});
        bodies.push_back({object, world.inverse(), transformPoint(world, Vec3::Zero()), model->boundingRadius * scale, *model, index});
    }
    spec.materials = materials;
    if (!bodies.empty()) {
        spec.cellMaterial.assign(static_cast<size_t>(plan.nx) * plan.ny * plan.nz, 0);
        for (int k = 0; k < plan.nz; ++k)
            for (int j = 0; j < plan.ny; ++j)
                for (int i = 0; i < plan.nx; ++i) {
                    const Vec3 p = domain.corner + plan.cell * ((i + 0.5) * domain.axis[0] + (j + 0.5) * domain.axis[1] +
                                                                (domain.volume ? (k + 0.5) : 0.0) * domain.axis[2]);
                    for (const Body& b : bodies)
                        if ((p - b.centre).norm() <= b.radius + plan.cell && b.model.contains(transformPoint(b.inverse, p)))
                            spec.cellMaterial[spec.cellIndex(i, j, k)] = b.material;
                }
        for (const std::uint8_t m : spec.cellMaterial)
            if (m)
                (materials[m].pec ? plan.pecCells : plan.materialCells)++;
    }

    // Sources and wires on the grid.
    const auto gridAxisOf = [&](const Vec3& axis, int& component, double& sign) {
        for (int c = 0; c < (domain.volume ? 3 : 3); ++c) {
            const double d = axis.dot(domain.axis[static_cast<size_t>(c)]);
            if (std::abs(d) > 0.999) {
                component = c;
                sign = d > 0 ? 1.0 : -1.0;
                return true;
            }
        }
        return false;
    };
    const auto local = [&](const Vec3& world) -> Vec3 {
        const Vec3 d = world - domain.corner;
        return Vec3(d.dot(domain.axis[0]), d.dot(domain.axis[1]), d.dot(domain.axis[2])) / plan.cell;
    };
    const int limits[3] = {plan.nx, plan.ny, plan.nz};
    for (const Radiator& r : radiators) {
        int c = 2;
        double sign = 1.0;
        if (!gridAxisOf(r.axis, c, sign)) {
            plan.warnings.push_back("'" + r.object->state.name + "' is not aligned with an axis of the grid and is left out.");
            continue;
        }
        const dsp::Waveform w = signalFor(settings, r.frequencyHz, r.phase);
        const Vec3 q = local(r.centre);
        if (!domain.volume) {
            if (c != 2) {
                plan.warnings.push_back("'" + r.object->state.name + "' lies in the plane of a two-dimensional run; only radiators along "
                                        "the plane's normal are represented (as infinite line currents).");
                continue;
            }
            const int i = static_cast<int>(std::lround(q.x())), j = static_cast<int>(std::lround(q.y()));
            if (i < 0 || j < 0 || i > plan.nx || j > plan.ny || std::abs(q.z()) > 1.0) {
                plan.warnings.push_back("'" + r.object->state.name + "' lies outside the plane of the region.");
                continue;
            }
            const double amplitude = r.kind == Radiator::Kind::CurrentElement ? r.amplitude / plan.cell : r.amplitude;
            prepared.sources.push_back({i, j, 0, 2, sign * amplitude, r.delay, w});
            plan.objects.push_back("'" + r.object->state.name + "': an infinite line current along the normal (two-dimensional run)" +
                                   std::string(r.kind == Radiator::Kind::Monopole ? "; its ground plane is not represented" : ""));
            continue;
        }
        int node[3] = {static_cast<int>(std::lround(q.x())), static_cast<int>(std::lround(q.y())), static_cast<int>(std::lround(q.z()))};
        if (r.kind == Radiator::Kind::Monopole)
            node[c] = static_cast<int>(std::lround(q(c)));  // the ground plane passes through this node layer
        else
            node[c] = static_cast<int>(std::lround(q(c) - 0.5));  // the edge whose midpoint is nearest the feed
        bool inside = true;
        for (int a = 0; a < 3; ++a)
            inside = inside && node[a] >= 0 && node[a] <= limits[a] - (a == c ? 1 : 0);
        if (!inside) {
            plan.warnings.push_back("'" + r.object->state.name + "' lies outside the region.");
            continue;
        }
        const double amplitude = r.kind == Radiator::Kind::CurrentElement ? r.amplitude / plan.cell : r.amplitude;
        prepared.sources.push_back({node[0], node[1], node[2], c, sign * amplitude, r.delay, w});
        if (r.kind == Radiator::Kind::CurrentElement) {
            plan.objects.push_back("'" + r.object->state.name + "': a current element one cell long");
            continue;
        }
        // Arms (dipole) or wire and ground plane (monopole) as thin perfect-conductor edges.
        int below = 0, above = 0;
        if (r.kind == Radiator::Kind::Dipole) {
            const int half = static_cast<int>(std::lround(0.5 * r.length / plan.cell - 0.5));
            below = above = half;
        } else {
            above = static_cast<int>(std::lround(r.length / plan.cell)) - 1;
        }
        for (int s = -below; s <= above; ++s) {
            if (s == 0)
                continue;
            int e[3] = {node[0], node[1], node[2]};
            e[c] += s;
            if (e[c] < 0 || e[c] >= limits[c])
                continue;
            spec.pecEdges.push_back({c, e[0], e[1], e[2]});
            ++plan.wireEdges;
        }
        if (r.kind == Radiator::Kind::Monopole) {
            const int a1 = (c + 1) % 3, a2 = (c + 2) % 3;
            const int reach = static_cast<int>(std::ceil(r.groundRadius / plan.cell));
            for (int u = -reach; u <= reach; ++u)
                for (int v = -reach; v <= reach; ++v) {
                    for (const int comp : {a1, a2}) {
                        int e[3] = {node[0], node[1], node[2]};
                        e[a1] += u;
                        e[a2] += v;
                        const double du = u + (comp == a1 ? 0.5 : 0.0), dv = v + (comp == a2 ? 0.5 : 0.0);
                        if (std::hypot(du, dv) * plan.cell > r.groundRadius || e[a1] < 0 || e[a2] < 0 || e[a1] > limits[a1] || e[a2] > limits[a2])
                            continue;
                        spec.pecEdges.push_back({comp, e[0], e[1], e[2]});
                    }
                }
            plan.objects.push_back(format("'%s': monopole as a feed edge and %d wire edges over a ground disc of thin conducting edges",
                                          r.object->state.name.c_str(), above));
        } else {
            plan.objects.push_back(format("'%s': dipole as a feed edge and %d wire edges per arm (staircase thin wire, effective radius "
                                          "about 0.23 cells)", r.object->state.name.c_str(), below));
            if (below < 2)
                plan.warnings.push_back("'" + r.object->state.name + "' is only a few cells long: the grid barely resolves it.");
        }
    }
    plan.sourceCount = prepared.sources.size();
    if (prepared.sources.empty())
        return refuse("no radiator could be placed on the grid (see the warnings)");
    plan.assumptions = {"Full-wave solution of Maxwell's equations by the Yee finite-difference time-domain method, second order, "
                        "uniform cubic cells; curved surfaces are staircased.",
                        "Good conductors are perfect electric conductors; other materials have frequency-independent permittivity, "
                        "permeability and conductivity (a loss tangent becomes the equivalent conductivity at the run frequency).",
                        "Thin wires are edges held at zero (effective radius about 0.23 cells, independent of the wire's real radius)."};
    if (!domain.volume)
        plan.assumptions.push_back("Two-dimensional TM calculation: nothing varies along the normal of the plane; currents are infinite "
                                   "line currents and objects are infinitely long with the cross-section the plane cuts.");
    if (Status status = spec.validate(); !status)
        return refuse(status.message());
    return prepared;
}

Result<std::shared_ptr<const EmFullWaveResult>> simulateFullWave(const std::string& name, EmPrepared prepared, const EmFullWaveSettings& settings,
                                                                 ResultMetadata metadata, JobContext* context)
{
    using Failure = Result<std::shared_ptr<const EmFullWaveResult>>;
    if (!prepared.plan.allowed)
        return Failure::failure(prepared.plan.refusal);
    auto result = std::make_shared<EmFullWaveResult>();
    YeeRecording rec;
    rec.snapshotCount = settings.snapshotCount;
    rec.snapshotComponent = prepared.sources.empty() ? 2 : prepared.sources.front().component;
    if (settings.waveform.kind == dsp::WaveformKind::Continuous) {
        rec.harmonicFrequencyHz = prepared.plan.frequencyHz;
        rec.harmonicPeriods = settings.harmonicPeriods;
    }
    const YeeRunEnd end = runYee(prepared.spec, prepared.sources, {}, prepared.plan.steps, rec, result->output, context);
    if (end.kind == YeeRunEnd::Kind::Cancelled)
        return Failure::failure("cancelled");
    if (end.kind != YeeRunEnd::Kind::Completed)
        return Failure::failure(end.message);
    const EmFullWavePlan& plan = prepared.plan;
    result->id = Id::generate();
    result->name = name;
    result->plan = plan;
    result->settings = settings;
    metadata.domain = PhysicsDomain::Electromagnetic;
    metadata.solverId = kEmFullWaveSolverId;
    metadata.solverVersion = "1";
    metadata.elapsedSeconds = result->output.elapsedSeconds;
    metadata.assumptions = plan.assumptions;
    metadata.configuration["settings"] = toJson(settings);
    metadata.configuration["grid"] = {{"dimensions", plan.dimensions}, {"nx", plan.nx}, {"ny", plan.ny}, {"nz", plan.nz}, {"cell", plan.cell},
                                      {"timeStep", plan.dt}, {"steps", plan.steps}, {"stabilityLimit", plan.stabilityLimit},
                                      {"cellsPerWavelength", plan.cellsPerWavelength}, {"frequencyHz", plan.frequencyHz},
                                      {"pecCells", plan.pecCells}, {"materialCells", plan.materialCells}, {"wireEdges", plan.wireEdges}};
    metadata.configuration["objects"] = plan.objects;
    metadata.configuration["warnings"] = plan.warnings;
    metadata.configuration["backend"] = result->output.backend;
    metadata.configuration["compute"] = compute::toJson(settings.compute);
    result->metadata = metadata;
    if (!result->output.harmonic[0].empty()) {
        auto field = std::make_shared<EmFieldResult>();
        field->id = Id::generate();
        field->name = name;
        field->grid = plan.grid;
        field->frequencyHz = plan.frequencyHz;
        field->hasH = false;
        field->metadata = metadata;
        const size_t n = plan.grid.count();
        const Vec3 axes[3] = {plan.grid.axisU.normalized(), plan.grid.axisV.normalized(),
                              plan.dimensions == 3 ? Vec3(plan.grid.axisW.normalized()) : Vec3(plan.grid.axisU.normalized().cross(plan.grid.axisV.normalized()))};
        for (int c = 0; c < 3; ++c) {
            field->e[static_cast<size_t>(c)].assign(n, {});
            field->h[static_cast<size_t>(c)].assign(n, {});
        }
        for (size_t i = 0; i < n; ++i) {
            Eigen::Vector3cd world = Eigen::Vector3cd::Zero();
            for (int c = 0; c < 3; ++c)
                world += result->output.harmonic[static_cast<size_t>(c)][i] * axes[c].cast<Complex>();
            for (int c = 0; c < 3; ++c)
                field->e[static_cast<size_t>(c)][i] = std::complex<float>(world(c));
        }
        field->excluded.assign(n, 0);
        for (const YeeSource& s : prepared.sources)
            field->excluded[static_cast<size_t>(s.i) + static_cast<size_t>(plan.grid.nu) * (static_cast<size_t>(s.j) + static_cast<size_t>(plan.grid.nv) * (plan.dimensions == 3 ? s.k : 0))] = 1;
        field->assumptions = plan.assumptions;
        field->assumptions.insert(field->assumptions.begin(),
                                  format("Steady-state electric field at %.6g MHz from the last %.3g periods of the time-domain run; only E is "
                                         "extracted.", plan.frequencyHz / 1e6, settings.harmonicPeriods));
        result->harmonicField = field;
    }
    return std::shared_ptr<const EmFullWaveResult>(result);
}

JobEnd EmFullWaveJob::run(JobContext& context)
{
    auto simulated = simulateFullWave(name_, std::move(prepared_), settings_, metadata_, &context);
    if (!simulated)
        return simulated.message() == "cancelled" ? JobEnd::cancelled() : JobEnd::failed(simulated.message());
    {
        const std::lock_guard lock(mutex_);
        result_ = simulated.take();
    }
    return JobEnd::completed(name_);
}

std::shared_ptr<const EmFullWaveResult> EmFullWaveJob::result() const
{
    const std::lock_guard lock(mutex_);
    return result_;
}

}  // namespace resonant::em
