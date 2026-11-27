#include "mechanics/Study.h"

#include "foundation/Transform.h"
#include "mechanics/MechanicalGeometry.h"
#include "mechanics/Meshing.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>

namespace resonant::mechanics {
namespace {

constexpr double pi = 3.14159265358979323846;

std::string format(const char* pattern, ...)
{
    char buffer[700];
    va_list arguments;
    va_start(arguments, pattern);
    std::vsnprintf(buffer, sizeof buffer, pattern, arguments);
    va_end(arguments);
    return buffer;
}

const char* axisName(int axis)
{
    return axis == 0 ? "X" : axis == 1 ? "Y" : "Z";
}

constexpr DampingKind kDampingKinds[] = {DampingKind::None, DampingKind::Rayleigh, DampingKind::LossFactor, DampingKind::ModalRatio};

nlohmann::json toJson(const Damping& d)
{
    return {{"kind", std::string(toString(d.kind))}, {"alpha", d.alpha}, {"beta", d.beta}, {"eta", d.eta},
            {"materialLossFactors", d.materialLossFactors}, {"zeta", d.zeta}};
}

Damping dampingFromJson(const nlohmann::json& v, Damping fallback)
{
    if (!v.is_object())
        return fallback;
    Damping d = fallback;
    const std::string kind = v.value("kind", std::string(toString(fallback.kind)));
    for (const DampingKind k : kDampingKinds)
        if (toString(k) == kind)
            d.kind = k;
    d.alpha = v.value("alpha", d.alpha);
    d.beta = v.value("beta", d.beta);
    d.eta = v.value("eta", d.eta);
    d.materialLossFactors = v.value("materialLossFactors", d.materialLossFactors);
    d.zeta = v.value("zeta", d.zeta);
    return d;
}

// Supports of a beam end.
void supportBeamEnd(StructuralModel& model, size_t node, Support support)
{
    if (support == Support::Pinned)
        fixDofs(model, node, {0, 1, 2, 3});  // translations, and the twist about the axis
    else if (support == Support::Clamped)
        fixAll(model, node);
}

// Supports of a plate edge (alongAxis2: the edge runs along axis2, at i = 0 or i = n1).
void supportPlateNode(StructuralModel& model, size_t node, Support support, bool alongAxis2)
{
    if (support == Support::Free)
        return;
    model.fix(node, 0);
    model.fix(node, alongAxis2 ? 1 : 2);  // with w = 0 along the edge its slope along the edge vanishes too
    if (support == Support::Clamped)
        model.fix(node, alongAxis2 ? 2 : 1);
}

// The world frame of an object: unit axes and the scale along each.
struct Frame {
    Vec3 centre;
    std::array<Vec3, 3> axis;
    Vec3 scale;
};
Frame frameOf(const Mat4& world)
{
    Frame f;
    f.centre = transformPoint(world, Vec3::Zero());
    for (int i = 0; i < 3; ++i) {
        const Vec3 d = transformDirection(world, Vec3::Unit(i));
        f.scale(i) = d.norm();
        f.axis[static_cast<size_t>(i)] = d / f.scale(i);
    }
    return f;
}

}  // namespace

// --- settings ------------------------------------------------------------------------------

Status MechanicsSettings::validate() const
{
    if (modeCount < 1 || modeCount > 500)
        return Status::failure("the number of modes must be between 1 and 500");
    if (!(frequencyMin > 0.0 && frequencyMax > frequencyMin && std::isfinite(frequencyMax)))
        return Status::failure("the frequency range must be positive and increasing");
    if (frequencyPoints < 2 || frequencyPoints > 20000)
        return Status::failure("the number of frequencies must be between 2 and 20000");
    if (harmonicModes < 1 || harmonicModes > 1000)
        return Status::failure("the number of modes for superposition must be between 1 and 1000");
    if (Status s = damping.validate(); !s)
        return s;
    if (Status s = transientDamping.validate(); !s)
        return s;
    if (transientDamping.kind == DampingKind::LossFactor || transientDamping.kind == DampingKind::ModalRatio)
        return Status::failure("transient analysis accepts Rayleigh damping or none");
    if (!(transientDuration > 0.0 && std::isfinite(transientDuration)))
        return Status::failure("the simulated time must be positive");
    if (!(stepsPerPeriod >= 4.0 && stepsPerPeriod <= 10000.0))
        return Status::failure("the steps per period must be between 4 and 10000");
    if (!(timeStep >= 0.0 && std::isfinite(timeStep)))
        return Status::failure("the time step must be zero (automatic) or positive");
    if (snapshotCount < 0 || snapshotCount > 5000)
        return Status::failure("the number of snapshots must be between 0 and 5000");
    if (!(targetFrequency > 0.0 && std::isfinite(targetFrequency)))
        return Status::failure("the target frequency must be positive");
    if (!(regularization >= 0.0 && regularization < 1.0))
        return Status::failure("the regularization must be between 0 and 1");
    if (memoryLimitBytes < 1024 * 1024)
        return Status::failure("the memory limit must be at least 1 MiB");
    return Status::success();
}

std::vector<double> MechanicsSettings::frequencies() const
{
    std::vector<double> f(static_cast<size_t>(frequencyPoints));
    for (int i = 0; i < frequencyPoints; ++i) {
        const double t = static_cast<double>(i) / (frequencyPoints - 1);
        f[static_cast<size_t>(i)] = logSpacing ? frequencyMin * std::pow(frequencyMax / frequencyMin, t)
                                               : frequencyMin + (frequencyMax - frequencyMin) * t;
    }
    return f;
}

bool operator==(const MechanicsSettings& a, const MechanicsSettings& b)
{
    return toJson(a) == toJson(b);
}

nlohmann::json toJson(const MechanicsSettings& s)
{
    return {{"modeCount", s.modeCount},
            {"frequencyMin", s.frequencyMin},
            {"frequencyMax", s.frequencyMax},
            {"frequencyPoints", s.frequencyPoints},
            {"logSpacing", s.logSpacing},
            {"harmonicMethod", s.harmonicMethod == HarmonicMethod::Direct ? "direct" : "modal"},
            {"harmonicModes", s.harmonicModes},
            {"damping", toJson(s.damping)},
            {"transientDuration", s.transientDuration},
            {"stepsPerPeriod", s.stepsPerPeriod},
            {"timeStep", s.timeStep},
            {"transientDamping", toJson(s.transientDamping)},
            {"snapshotCount", s.snapshotCount},
            {"targetFrequency", s.targetFrequency},
            {"regularization", s.regularization},
            {"memoryLimitBytes", s.memoryLimitBytes}};
}

Result<MechanicsSettings> mechanicsSettingsFromJson(const nlohmann::json& v)
{
    if (!v.is_object())
        return Result<MechanicsSettings>::failure("mechanical settings must be an object");
    MechanicsSettings s;
    try {
        s.modeCount = v.value("modeCount", s.modeCount);
        s.frequencyMin = v.value("frequencyMin", s.frequencyMin);
        s.frequencyMax = v.value("frequencyMax", s.frequencyMax);
        s.frequencyPoints = v.value("frequencyPoints", s.frequencyPoints);
        s.logSpacing = v.value("logSpacing", s.logSpacing);
        s.harmonicMethod = v.value("harmonicMethod", std::string("direct")) == "modal" ? HarmonicMethod::Modal : HarmonicMethod::Direct;
        s.harmonicModes = v.value("harmonicModes", s.harmonicModes);
        s.damping = dampingFromJson(v.value("damping", nlohmann::json()), s.damping);
        s.transientDuration = v.value("transientDuration", s.transientDuration);
        s.stepsPerPeriod = v.value("stepsPerPeriod", s.stepsPerPeriod);
        s.timeStep = v.value("timeStep", s.timeStep);
        s.transientDamping = dampingFromJson(v.value("transientDamping", nlohmann::json()), s.transientDamping);
        s.snapshotCount = v.value("snapshotCount", s.snapshotCount);
        s.targetFrequency = v.value("targetFrequency", s.targetFrequency);
        s.regularization = v.value("regularization", s.regularization);
        s.memoryLimitBytes = v.value("memoryLimitBytes", s.memoryLimitBytes);
    } catch (const nlohmann::json::exception& error) {
        return Result<MechanicsSettings>::failure(std::string("invalid mechanical settings: ") + error.what());
    }
    if (Status status = s.validate(); !status)
        return Result<MechanicsSettings>::failure(status.message());
    return s;
}

MechanicsSettings mechanicsSettingsOf(const Project& project)
{
    const auto it = project.extensions.find(kMechanicsSettingsKey);
    if (it == project.extensions.end())
        return MechanicsSettings();
    auto parsed = mechanicsSettingsFromJson(*it);
    return parsed ? parsed.take() : MechanicsSettings();
}

void storeMechanicsSettings(Project& project, const MechanicsSettings& settings)
{
    project.extensions[kMechanicsSettingsKey] = toJson(settings);
}

// --- study --------------------------------------------------------------------------------

std::string StructureStudy::summary() const
{
    if (!model)
        return refusal;
    return format("%s of '%s': %zu elements, %zu nodes, %zu degrees of freedom (%zu free); element size %.4g mm; "
                  "estimated memory %.1f MiB.",
                  std::string(toString(model->kind)).c_str(), objectName.c_str(), model->elements.size(), model->nodes.size(),
                  model->dofCount(), freeDofs, elementSize * 1e3, static_cast<double>(estimatedBytes) / 1048576.0);
}

double StructureStudy::elementsPerWavelength(double frequencyHz) const
{
    if (!model || frequencyHz <= 0.0 || elementSize <= 0.0)
        return 0.0;
    const ElasticMaterial& m = model->materials.front();
    const double omega = 2.0 * pi * frequencyHz;
    double wavelength = 0.0;
    switch (model->kind) {
    case ElementKind::Frame: {
        // The section that bends most easily along the beam.
        double ratio = 1e300;
        for (const Section& s : model->sections)
            ratio = std::min(ratio, std::min(s.iy, s.iz) / s.area);
        wavelength = 2.0 * pi * std::pow(m.youngsModulus * ratio / (m.density * omega * omega), 0.25);
        break;
    }
    case ElementKind::Plate: {
        const double h = model->elements.front().thickness;
        wavelength = 2.0 * pi * std::pow(m.plateBendingStiffness(h) / (m.density * h * omega * omega), 0.25);
        break;
    }
    case ElementKind::Membrane: {
        const Element& e = model->elements.front();
        wavelength = std::sqrt(e.tension / (m.density * e.thickness)) / frequencyHz;
        break;
    }
    case ElementKind::Solid: wavelength = m.shearSpeed() / frequencyHz; break;
    }
    return wavelength / elementSize;
}

std::string StructureStudy::waveDescription() const
{
    if (!model)
        return "";
    switch (model->kind) {
    case ElementKind::Frame: return "bending wavelength of the beam";
    case ElementKind::Plate: return "bending wavelength of the plate";
    case ElementKind::Membrane: return "wavelength of the membrane";
    case ElementKind::Solid: return "shear wavelength of the solid";
    }
    return "";
}

std::vector<Id> structureObjects(const Project& project)
{
    std::vector<Id> ids;
    project.scene.forEach([&](const SceneObject& object) {
        if (dynamic_cast<const StructureComponent*>(object.state.component.get()))
            ids.push_back(object.id());
    });
    return ids;
}

StructureStudy prepareStudy(const Project& project, const Id& structureId, size_t memoryLimitBytes)
{
    StructureStudy study;
    const auto refuse = [&](std::string why) {
        study.allowed = false;
        study.refusal = std::move(why);
        return study;
    };
    const SceneObject* object = project.scene.find(structureId);
    const auto* component = object ? dynamic_cast<const StructureComponent*>(object->state.component.get()) : nullptr;
    if (!component)
        return refuse("select a structure: an object with geometry made structural (Mechanics menu)");
    study.objectId = structureId;
    study.objectName = object->state.name;
    study.settings = component->settings;
    if (Status status = component->settings.validate(); !status)
        return refuse(status.message());
    if (!object->state.geometry)
        return refuse("'" + object->state.name + "' has no geometry");
    if (!project.scene.isEffectivelyVisible(*object))
        study.warnings.push_back("'" + object->state.name + "' is hidden; it is analysed all the same.");

    // Material.
    const Material* record = object->state.materialId.isNull() ? nullptr : project.materials.find(object->state.materialId);
    if (!record)
        return refuse("assign a material with mechanical properties (density, Young's modulus, Poisson's ratio) to '" + object->state.name + "'");
    auto material = elasticMaterialOf(*record);
    if (!material)
        return refuse(material.message());
    const ElasticMaterial& m = material.value();
    study.properties.push_back({"Material", m.name + (record->property(PhysicsDomain::Mechanical, "youngsModulus")->provenance == Provenance::Illustrative
                                                          ? " (illustrative values)"
                                                          : "")});
    study.properties.push_back({"Young's modulus", format("%.4g GPa", m.youngsModulus / 1e9)});
    study.properties.push_back({"Poisson's ratio", format("%.4g", m.poissonsRatio)});
    study.properties.push_back({"Density", format("%.5g kg/m^3", m.density)});
    study.properties.push_back({"Shear modulus (derived)", format("%.4g GPa", m.shearModulus() / 1e9)});
    study.properties.push_back({"Bar wave speed sqrt(E/rho)", format("%.5g m/s", m.barSpeed())});
    study.properties.push_back({"Bulk P / S wave speeds", format("%.5g / %.5g m/s", m.longitudinalSpeed(), m.shearSpeed())});
    if (!m.provenance.empty())
        study.notes.push_back("Material values: " + m.provenance);

    const Geometry& g = *object->state.geometry;
    study.world = project.scene.worldMatrix(*object);
    const Frame f = frameOf(study.world);
    const std::string kind = g.kindId();
    const auto size = [&](const char* key, int axis) { return g.parameter(key) * f.scale(axis); };
    const StructureSettings& s = component->settings;
    StructuralModel model;
    study.assumptions = {"Linear elasticity: small displacements and small strains; no contact, plasticity, fracture or prestress "
                         "other than a membrane's tension.",
                         "Isotropic material with the single Young's modulus and Poisson's ratio of its record; anisotropy, "
                         "viscoelasticity and frequency or temperature dependence are not represented."};

    switch (s.idealization) {
    case Idealization::Beam: {
        Vec3 start, end, up;
        std::function<Section(double)> sectionAt;
        double length = 0.0;
        if (kind == "box") {
            const Vec3 dims(size("width", 0), size("depth", 1), size("height", 2));
            int a = s.axis;
            if (a < 0)
                dims.maxCoeff(&a);
            const int yAxis = (a + 1) % 3, zAxis = (a + 2) % 3;
            length = dims(a);
            start = f.centre - 0.5 * length * f.axis[static_cast<size_t>(a)];
            end = f.centre + 0.5 * length * f.axis[static_cast<size_t>(a)];
            up = f.axis[static_cast<size_t>(yAxis)];
            const Section section = rectangularSection(dims(yAxis), dims(zAxis));
            sectionAt = [section](double) { return section; };
            study.characteristicThickness = std::max(dims(yAxis), dims(zAxis));
            study.notes.push_back(format("Beam along local %s, length %.4g mm, %s.", axisName(a), length * 1e3, section.description.c_str()));
        } else if (kind == "cylinder" || isRodGeometry(g)) {
            if (s.axis >= 0 && s.axis != 2)
                study.warnings.push_back("A " + g.kind().displayName + " can only be a beam along its own Z axis; the axis setting is ignored.");
            const double scaleR = std::max(f.scale(0), f.scale(1));
            if (std::abs(f.scale(0) - f.scale(1)) > 1e-9 * scaleR)
                study.warnings.push_back("The object is scaled differently along X and Y; the beam uses a circular section of the larger radius.");
            if (kind == "cylinder") {
                length = size("height", 2);
                const Section section = circularSection(g.parameter("radius") * scaleR);
                sectionAt = [section](double) { return section; };
                study.characteristicThickness = 2.0 * g.parameter("radius") * scaleR;
                study.notes.push_back(format("Beam along local Z, length %.4g mm, %s.", length * 1e3, section.description.c_str()));
            } else {
                const bool periodic = kind == "rod.periodic";
                length = (periodic ? g.parameter("cells") * (g.parameter("lengthA") + g.parameter("lengthB")) : g.parameter("length")) * f.scale(2);
                const Geometry copy = g;
                sectionAt = [copy, scaleR](double t) { return circularSection(rodRadiusAt(copy, t) * scaleR); };
                double largest = 0.0;
                for (int i = 0; i <= 100; ++i)
                    largest = std::max(largest, rodRadiusAt(g, i / 100.0));
                study.characteristicThickness = 2.0 * largest * scaleR;
                study.notes.push_back(format("Beam along local Z, length %.4g mm, circular section varying along the length (each "
                                             "element takes the section at its midpoint).",
                                             length * 1e3));
            }
            start = f.centre - 0.5 * length * f.axis[2];
            end = f.centre + 0.5 * length * f.axis[2];
            up = f.axis[0];
        } else {
            return refuse("a " + g.kind().displayName + " cannot be idealized as a beam; beams are made from boxes, cylinders and rods");
        }
        double elementSize = s.elementSize > 0.0 ? s.elementSize : length / 40.0;
        int n = std::max(4, static_cast<int>(std::lround(length / elementSize)));
        if (isRodGeometry(g) && kind == "rod.periodic")
            n = std::max(n, static_cast<int>(g.parameter("cells")) * 8);  // every segment needs elements of its own
        if (isRodGeometry(g) && std::lround(g.parameter("profile")) == 2 && n % 2)
            ++n;  // a stepped horn changes section exactly at an element boundary
        study.elementSize = length / n;
        study.grid = {n, 0, 0};
        model = frameLine(start, end, n, sectionAt, m, up);
        supportBeamEnd(model, 0, s.supports[0]);
        supportBeamEnd(model, model.nodes.size() - 1, s.supports[1]);
        study.notes.push_back(format("Supports: start (-axis end) %s, end %s.", std::string(toString(s.supports[0])).c_str(),
                                     std::string(toString(s.supports[1])).c_str()));
        const double slenderness = length / study.characteristicThickness;
        study.properties.push_back({"Length", format("%.4g mm", length * 1e3)});
        study.properties.push_back({"Slenderness L / h", format("%.3g", slenderness)});
        if (slenderness < 10.0)
            study.warnings.push_back(format("The beam is stubby (length / section size = %.3g, below 10): Euler-Bernoulli theory neglects "
                                            "shear deformation and rotary inertia and overestimates its frequencies, increasingly for higher "
                                            "modes. A solid idealization is more appropriate.",
                                            slenderness));
        study.assumptions.push_back("Euler-Bernoulli beam: cross-sections stay plane and normal to the axis; shear deformation and the "
                                    "rotary inertia of bending are neglected; Saint-Venant torsion without warping restraint.");
        break;
    }
    case Idealization::Plate:
    case Idealization::Membrane: {
        const bool membrane = s.idealization == Idealization::Membrane;
        int normalAxis = 2;
        double a = 0.0, b = 0.0, thickness = 0.0;
        if (kind == "box") {
            const Vec3 dims(size("width", 0), size("depth", 1), size("height", 2));
            normalAxis = s.axis;
            if (normalAxis < 0)
                dims.minCoeff(&normalAxis);
            a = dims((normalAxis + 1) % 3);
            b = dims((normalAxis + 2) % 3);
            thickness = dims(normalAxis);
            if (s.thickness > 0.0)
                study.warnings.push_back("The thickness setting applies to shapes without thickness; this box's own thickness is used.");
        } else if (kind == "plane") {
            a = size("width", 0);
            b = size("depth", 1);
            thickness = s.thickness;
            if (!(thickness > 0.0))
                return refuse("a plane has no thickness: set the structure's thickness");
            if (s.axis >= 0 && s.axis != 2)
                study.warnings.push_back("A plane is a plate in its own XY plane; the axis setting is ignored.");
        } else {
            return refuse("a " + g.kind().displayName + " cannot be idealized as a " + std::string(membrane ? "membrane" : "plate")
                          + ": the rectangular elements need a box or a plane");
        }
        const Vec3 axis1 = f.axis[static_cast<size_t>((normalAxis + 1) % 3)], axis2 = f.axis[static_cast<size_t>((normalAxis + 2) % 3)];
        const double largest = std::max(a, b);
        const double elementSize = s.elementSize > 0.0 ? s.elementSize : largest / (membrane ? 32.0 : 24.0);
        const int n1 = std::max(4, static_cast<int>(std::lround(a / elementSize)));
        const int n2 = std::max(4, static_cast<int>(std::lround(b / elementSize)));
        if ((n1 + 1.0) * (n2 + 1.0) * (membrane ? 1.0 : 3.0) > 300000.0)
            return refuse("the mesh would have more than the 300 000 degrees of freedom this build accepts; use a larger element size");
        study.elementSize = std::max(a / n1, b / n2);
        study.grid = {n1, n2, 0};
        study.characteristicThickness = thickness;
        model = rectangularGrid(membrane ? ElementKind::Membrane : ElementKind::Plate, f.centre, axis1, axis2, a, b, n1, n2, thickness,
                                s.tension, m);
        for (int j = 0; j <= n2; ++j)
            for (int i = 0; i <= n1; ++i) {
                const size_t node = static_cast<size_t>(i + (n1 + 1) * j);
                if (membrane) {
                    if (i == 0 || i == n1 || j == 0 || j == n2)
                        model.fix(node, 0);
                    continue;
                }
                if (i == 0)
                    supportPlateNode(model, node, s.supports[0], true);
                if (i == n1)
                    supportPlateNode(model, node, s.supports[1], true);
                if (j == 0)
                    supportPlateNode(model, node, s.supports[2], false);
                if (j == n2)
                    supportPlateNode(model, node, s.supports[3], false);
            }
        const int i1 = (normalAxis + 1) % 3, i2 = (normalAxis + 2) % 3;
        study.notes.push_back(format("%s in the local %s-%s plane, %.4g x %.4g mm, thickness %.4g mm.", membrane ? "Membrane" : "Plate",
                                     axisName(i1), axisName(i2), a * 1e3, b * 1e3, thickness * 1e3));
        if (membrane) {
            study.notes.push_back(format("Pre-tension %.4g N/m, held at every edge.", s.tension));
            study.properties.push_back({"Transverse wave speed sqrt(T / rho h)", format("%.5g m/s", std::sqrt(s.tension / (m.density * thickness)))});
            study.assumptions.push_back("Pre-tensioned membrane: bending stiffness neglected; the tension is uniform, constant and set by the "
                                        "user (it is not derived from the geometry or the material). Edges held.");
        } else {
            study.notes.push_back(format("Edges: -%s %s, +%s %s, -%s %s, +%s %s (simply supported = pinned).", axisName(i1),
                                         std::string(toString(s.supports[0])).c_str(), axisName(i1), std::string(toString(s.supports[1])).c_str(),
                                         axisName(i2), std::string(toString(s.supports[2])).c_str(), axisName(i2),
                                         std::string(toString(s.supports[3])).c_str()));
            const double ratio = std::min(a, b) / thickness;
            study.properties.push_back({"Bending stiffness D", format("%.4g N m", m.plateBendingStiffness(thickness))});
            study.properties.push_back({"Side / thickness", format("%.3g", ratio)});
            if (ratio < 20.0)
                study.warnings.push_back(format("The plate is thick (smaller side / thickness = %.3g, below 20): Kirchhoff theory neglects "
                                                "transverse shear and overestimates its frequencies. A solid idealization is more appropriate.",
                                                ratio));
            study.assumptions.push_back("Kirchhoff thin plate: transverse shear deformation and rotary inertia neglected; only out-of-plane "
                                        "bending is modelled (in-plane stretching is not).");
        }
        break;
    }
    case Idealization::Solid: {
        if (kind != "box")
            return refuse("solid elements need a box; a " + g.kind().displayName + " cannot be meshed with hexahedra here");
        const Vec3 dims(size("width", 0), size("depth", 1), size("height", 2));
        const double elementSize = s.elementSize > 0.0 ? s.elementSize : dims.maxCoeff() / 20.0;
        std::array<int, 3> n{};
        for (int i = 0; i < 3; ++i)
            n[static_cast<size_t>(i)] = std::max(2, static_cast<int>(std::lround(dims(i) / elementSize)));
        const double dofs = 3.0 * (n[0] + 1.0) * (n[1] + 1.0) * (n[2] + 1.0);
        if (dofs > 300000.0)
            return refuse(format("the mesh would have about %.0f degrees of freedom, more than the 300 000 this build accepts; use a "
                                 "larger element size",
                                 dofs));
        study.grid = n;
        study.elementSize = std::max({dims(0) / n[0], dims(1) / n[1], dims(2) / n[2]});
        study.characteristicThickness = dims.minCoeff();
        model = hexBox(f.centre, f.axis[0], f.axis[1], f.axis[2], dims, n[0], n[1], n[2], m);
        for (int k = 0; k <= n[2]; ++k)
            for (int j = 0; j <= n[1]; ++j)
                for (int i = 0; i <= n[0]; ++i) {
                    const size_t node = static_cast<size_t>(i + (n[0] + 1) * (j + (n[1] + 1) * k));
                    const bool on[6] = {i == 0, i == n[0], j == 0, j == n[1], k == 0, k == n[2]};
                    for (size_t face = 0; face < 6; ++face)
                        if (on[face] && s.supports[face] != Support::Free)
                            fixAll(model, node);
                }
        study.notes.push_back(format("Solid %.4g x %.4g x %.4g mm, %d x %d x %d hexahedra. A pinned face is held like a clamped one "
                                     "(a solid node has no rotations).",
                                     dims(0) * 1e3, dims(1) * 1e3, dims(2) * 1e3, n[0], n[1], n[2]));
        if (std::min({n[0], n[1], n[2]}) < 4)
            study.warnings.push_back("Fewer than 4 elements span the thinnest dimension: trilinear hexahedra are far too stiff in bending "
                                     "(shear locking). Refine the mesh, or use a plate or beam idealization for thin parts.");
        study.assumptions.push_back("Three-dimensional linear elasticity with trilinear hexahedra (full integration): too stiff in bending "
                                    "unless several elements span the thickness.");
        break;
    }
    }
    if (Status status = model.validate(); !status)
        return refuse(status.message());

    // Cost.
    study.freeDofs = model.dofCount() - model.fixedDofCount();
    const double n = static_cast<double>(study.freeDofs);
    const double perRow = static_cast<double>(model.dofsPerNode()) * (model.kind == ElementKind::Frame ? 3.0 : model.kind == ElementKind::Solid ? 27.0 : 9.0);
    // Sparse matrices (K, M, loss) and a factor whose fill grows like n log n
    // (1D, 2D meshes) or n^(4/3) (3D) under fill-reducing ordering: a rough estimate.
    const double fill = model.kind == ElementKind::Frame ? perRow * n
                        : model.kind == ElementKind::Solid ? 30.0 * std::pow(n, 4.0 / 3.0)
                                                            : 20.0 * n * std::log2(std::max(2.0, n));
    study.estimatedBytes = static_cast<size_t>(3.0 * perRow * n * 16.0 + fill * 16.0 * 2.0);
    study.properties.push_back({"Mesh", format("%zu elements, %zu nodes, %zu free degrees of freedom", model.elements.size(), model.nodes.size(), study.freeDofs)});
    study.properties.push_back({"Mass", format("%.5g kg", model.totalMass())});
    if (study.freeDofs == 0)
        return refuse("every degree of freedom is held: nothing can move");
    if (study.freeDofs > 300000)
        return refuse(format("the mesh has %zu free degrees of freedom, more than the 300 000 this build accepts; use a larger element size",
                             study.freeDofs));
    if (study.estimatedBytes > memoryLimitBytes)
        return refuse(format("the analysis would need about %.0f MiB, more than the memory limit of %.0f MiB; use a larger element size. "
                             "The mesh is not coarsened automatically.",
                             static_cast<double>(study.estimatedBytes) / 1048576.0, static_cast<double>(memoryLimitBytes) / 1048576.0));
    study.model = std::make_shared<const StructuralModel>(std::move(model));
    const StructuralModel& sm = *study.model;

    // Attachments.
    const double tolerance = 2.0 * study.elementSize + 0.5 * study.characteristicThickness;
    const auto attach = [&](const SceneObject& o, const Id& wanted, size_t& node, Vec3& direction) {
        if (!wanted.isNull() && wanted != structureId)
            return false;
        const Mat4 w = project.scene.worldMatrix(o);
        const Vec3 position = transformPoint(w, Vec3::Zero());
        double distance = 0.0;
        node = nearestNode(sm, position, &distance);
        direction = transformDirection(w, Vec3::UnitZ()).normalized();
        if (distance > tolerance) {
            if (!wanted.isNull())
                study.warnings.push_back(format("'%s' is %.3g mm from the structure, more than the %.3g mm allowed, and is not attached.",
                                                o.state.name.c_str(), distance * 1e3, tolerance * 1e3));
            return false;
        }
        return true;
    };
    project.scene.forEach([&](const SceneObject& o) {
        size_t node = 0;
        Vec3 direction;
        if (const auto* actuator = dynamic_cast<const ActuatorComponent*>(o.state.component.get())) {
            if (!actuator->settings.active || !attach(o, actuator->settings.structure, node, direction))
                return;
            ForceInput input;
            input.objectId = o.id();
            input.label = o.state.name;
            double lost = 0.0;
            size_t spread = 1;
            if (actuator->settings.excitation == Excitation::DistributedForce)
                input.pattern = distributedPattern(sm, transformPoint(project.scene.worldMatrix(o), Vec3::Zero()), actuator->settings.radius,
                                                   direction, &lost, &spread);
            else
                input.pattern = forcePattern(sm, node, direction, &lost);
            input.amplitude = actuator->settings.amplitude;
            input.phase = actuator->settings.phase;
            input.delay = actuator->settings.delay;
            input.waveform = actuator->settings.waveform;
            input.position = sm.nodes[node];
            input.direction = direction;
            study.forces.push_back(input);
            study.notes.push_back(format("Actuator '%s': %.4g N %s at node %zu%s.", o.state.name.c_str(), input.amplitude,
                                         actuator->settings.excitation == Excitation::DistributedForce ? format("spread over %zu nodes", spread).c_str() : "point force",
                                         node, actuator->settings.physicalDevice ? " (the named device's response is NOT modelled: the force is applied as set)" : ""));
            if (lost > 0.01)
                study.warnings.push_back(format("%.0f %% of the force of '%s' points along directions this idealization has no stiffness in, "
                                                "and is not applied (a plate or membrane takes only out-of-plane forces).",
                                                100.0 * lost, o.state.name.c_str()));
        } else if (const auto* sensor = dynamic_cast<const SensorComponent*>(o.state.component.get())) {
            if (!attach(o, sensor->settings.structure, node, direction))
                return;
            ResponsePoint p = responseAt(sm, node, direction, o.state.name);
            p.objectId = o.id();
            study.sensors.push_back(p);
        } else if (const auto* target = dynamic_cast<const VibrationTargetComponent*>(o.state.component.get())) {
            if (!attach(o, target->settings.structure, node, direction))
                return;
            TargetPoint t;
            t.objectId = o.id();
            t.label = o.state.name;
            t.point = responseAt(sm, node, direction, o.state.name);
            t.point.objectId = o.id();
            t.desired = target->settings.suppress ? Complex(0.0, 0.0) : std::polar(target->settings.amplitude, target->settings.phase);
            t.weight = target->settings.weight;
            t.suppress = target->settings.suppress;
            study.targets.push_back(t);
        }
    });
    return study;
}

// --- results and runs -----------------------------------------------------------------------

std::string_view toString(Analysis analysis)
{
    switch (analysis) {
    case Analysis::Static: return "static";
    case Analysis::Modal: return "modal";
    case Analysis::Harmonic: return "harmonic";
    case Analysis::Transient: return "transient";
    }
    return "";
}

namespace {

std::shared_ptr<StructuralResult> newResult(const std::shared_ptr<const StructureStudy>& study, Analysis analysis, const std::string& method,
                                            nlohmann::json configuration)
{
    auto r = std::make_shared<StructuralResult>();
    r->id = Id::generate();
    r->analysis = analysis;
    r->study = study;
    r->name = study->objectName + ", " + std::string(toString(analysis)) + " analysis";
    r->metadata.domain = PhysicsDomain::Mechanical;
    r->metadata.solverId = "mechanical.fem." + std::string(toString(analysis));
    r->metadata.solverVersion = kMechanicsSolverVersion;
    r->metadata.assumptions = study->assumptions;
    configuration["structure"] = {{"object", study->objectId.str()},
                                  {"name", study->objectName},
                                  {"component", StructureComponent(study->settings).toJson()},
                                  {"elementFormulation", std::string(toString(study->model->kind))},
                                  {"elements", study->model->elements.size()},
                                  {"nodes", study->model->nodes.size()},
                                  {"freeDofs", study->freeDofs},
                                  {"elementSize", study->elementSize},
                                  {"material",
                                   {{"name", study->model->materials.front().name},
                                    {"density", study->model->materials.front().density},
                                    {"youngsModulus", study->model->materials.front().youngsModulus},
                                    {"poissonsRatio", study->model->materials.front().poissonsRatio},
                                    {"lossFactor", study->model->materials.front().lossFactor},
                                    {"provenance", study->model->materials.front().provenance}}}};
    nlohmann::json forces = nlohmann::json::array();
    for (const ForceInput& f : study->forces)
        forces.push_back({{"object", f.objectId.str()}, {"label", f.label}, {"amplitude", f.amplitude}, {"phase", f.phase}, {"delay", f.delay},
                          {"waveform", dsp::toJson(f.waveform)},
                          {"position", {f.position.x(), f.position.y(), f.position.z()}},
                          {"direction", {f.direction.x(), f.direction.y(), f.direction.z()}}});
    configuration["forces"] = forces;
    configuration["method"] = method;
    configuration["warnings"] = study->warnings;
    configuration["notes"] = study->notes;
    r->metadata.configuration = std::move(configuration);
    r->warnings = study->warnings;
    return r;
}

StructuralOutcome failed(const std::string& message)
{
    return StructuralOutcome::failure(message);
}

}  // namespace

StructuralOutcome runStatic(std::shared_ptr<const StructureStudy> study, JobContext* context)
{
    if (!study || !study->allowed)
        return failed(study ? study->refusal : "no structure");
    if (study->forces.empty())
        return failed("there is no active actuator attached to the structure: a static analysis needs a load");
    if (context && context->cancelRequested())
        return failed("cancelled");
    const auto started = std::chrono::steady_clock::now();
    const SystemMatrices system = assemble(*study->model);
    auto solved = solveStatic(*study->model, system, study->forces);
    if (!solved)
        return failed(solved.message());
    auto r = newResult(study, Analysis::Static, "sparse LDL^T factorization of K", {});
    r->statics = solved.take();
    if (hasStress(study->model->kind))
        for (const Element& e : study->model->elements)
            r->stresses.push_back(elementStress(*study->model, e, r->statics->displacement));
    r->metadata.assumptions.push_back("Static load: each actuator's amplitude times the cosine of its phase, applied as a constant force.");
    r->metadata.elapsedSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    return std::shared_ptr<const StructuralResult>(r);
}

StructuralOutcome runModal(std::shared_ptr<const StructureStudy> study, const MechanicsSettings& settings, JobContext* context)
{
    if (!study || !study->allowed)
        return failed(study ? study->refusal : "no structure");
    const auto started = std::chrono::steady_clock::now();
    const SystemMatrices system = assemble(*study->model);
    auto solved = solveModal(*study->model, system, settings.modeCount, context);
    if (!solved)
        return failed(solved.message());
    auto r = newResult(study, Analysis::Modal, solved.value().method, {{"modeCount", settings.modeCount}});
    r->modal = solved.take();
    r->metadata.assumptions.push_back("Mode shapes are normalized to unit modal mass: their amplitude is arbitrary, not a physical displacement.");
    // Mesh resolution of the highest mode found.
    if (!r->modal->modes.empty()) {
        const double fTop = r->modal->modes.back().frequencyHz;
        const double perWave = study->elementsPerWavelength(fTop);
        if (perWave > 0.0 && perWave < 6.0)
            r->warnings.push_back(format("At the highest mode (%.4g Hz) the mesh has only %.2g elements per %s: the higher modes are "
                                         "inaccurate. Refine the mesh.",
                                         fTop, perWave, study->waveDescription().c_str()));
    }
    r->metadata.elapsedSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    return std::shared_ptr<const StructuralResult>(r);
}

StructuralOutcome runHarmonic(std::shared_ptr<const StructureStudy> study, const MechanicsSettings& settings, JobContext* context)
{
    if (!study || !study->allowed)
        return failed(study ? study->refusal : "no structure");
    if (Status status = settings.validate(); !status)
        return failed(status.message());
    const auto started = std::chrono::steady_clock::now();
    const SystemMatrices system = assemble(*study->model);
    HarmonicSettings h;
    h.frequenciesHz = settings.frequencies();
    h.damping = settings.damping;
    h.method = settings.harmonicMethod;
    h.modeCount = settings.harmonicModes;
    h.fieldMemoryLimit = std::min<size_t>(settings.memoryLimitBytes / 2, 512u * 1024u * 1024u);
    std::vector<ResponsePoint> points = study->sensors;
    for (const TargetPoint& t : study->targets)
        points.push_back(t.point);
    auto solved = solveHarmonic(*study->model, system, study->forces, points, h, context);
    if (!solved)
        return failed(solved.message());
    auto r = newResult(study, Analysis::Harmonic, solved.value().method,
                       {{"frequencyMin", settings.frequencyMin}, {"frequencyMax", settings.frequencyMax}, {"points", settings.frequencyPoints},
                        {"logSpacing", settings.logSpacing}, {"damping", h.damping.describe()}, {"harmonicModes", settings.harmonicModes}});
    r->harmonic = solved.take();
    r->metadata.assumptions.push_back("Steady harmonic response, time convention e^{+j omega t}, peak amplitudes; damping: " + h.damping.describe() + ".");
    if (h.damping.kind == DampingKind::None)
        r->warnings.push_back("Undamped: at a natural frequency the response is unbounded; values near resonances depend on how close a "
                              "frequency point happens to be.");
    const double perWave = study->elementsPerWavelength(settings.frequencyMax);
    if (perWave > 0.0 && perWave < 6.0)
        r->warnings.push_back(format("At %.4g Hz the mesh has only %.2g elements per %s: the response there is inaccurate. Refine the mesh "
                                     "or lower the frequency range.",
                                     settings.frequencyMax, perWave, study->waveDescription().c_str()));
    if (!r->harmonic->fieldsNote.empty())
        r->warnings.push_back(r->harmonic->fieldsNote + ".");
    r->metadata.elapsedSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    return std::shared_ptr<const StructuralResult>(r);
}

double transientTimeStep(const StructureStudy& study, const MechanicsSettings& settings, std::string* basis)
{
    if (settings.timeStep > 0.0) {
        if (basis)
            *basis = format("%.4g us as set", settings.timeStep * 1e6);
        return settings.timeStep;
    }
    double highest = 0.0;
    for (const ForceInput& f : study.forces)
        highest = std::max(highest, f.waveform.highestFrequency());
    if (highest <= 0.0)
        highest = 1000.0;
    const double dt = 1.0 / (settings.stepsPerPeriod * highest);
    if (basis)
        *basis = format("%.4g us: %.3g steps per period of the highest significant frequency of the forces (%.4g Hz)", dt * 1e6,
                        settings.stepsPerPeriod, highest);
    return dt;
}

StructuralOutcome runTransient(std::shared_ptr<const StructureStudy> study, const MechanicsSettings& settings, JobContext* context)
{
    if (!study || !study->allowed)
        return failed(study ? study->refusal : "no structure");
    if (Status status = settings.validate(); !status)
        return failed(status.message());
    if (study->forces.empty())
        return failed("there is no active actuator attached to the structure");
    const SystemMatrices system = assemble(*study->model);
    TransientSettings t;
    std::string basis;
    t.timeStep = transientTimeStep(*study, settings, &basis);
    t.duration = settings.transientDuration;
    t.damping = settings.transientDamping;
    t.snapshotCount = settings.snapshotCount;
    const double steps = t.duration / t.timeStep;
    if (steps > 2e6)
        return failed(format("%.3g time steps: too many. Lengthen the time step or shorten the simulated time.", steps));
    const size_t snapshotBytes = static_cast<size_t>(t.snapshotCount) * study->model->dofCount() * sizeof(double);
    if (snapshotBytes > settings.memoryLimitBytes / 2)
        return failed("the snapshots would need more memory than allowed; record fewer");
    auto solved = solveTransient(*study->model, system, study->forces, study->sensors, t, context);
    if (!solved)
        return failed(solved.message());
    auto r = newResult(study, Analysis::Transient,
                       "implicit Newmark time integration, average acceleration (beta = 1/4, gamma = 1/2): unconditionally stable, "
                       "second order, no numerical damping",
                       {{"duration", t.duration}, {"timeStep", t.timeStep}, {"timeStepBasis", basis}, {"damping", t.damping.describe()},
                        {"steps", static_cast<long>(std::ceil(steps))}});
    r->transient = solved.take();
    double highest = 0.0;
    for (const ForceInput& f : study->forces)
        highest = std::max(highest, f.waveform.highestFrequency());
    const double error = newmarkPeriodError(2.0 * pi * highest, t.timeStep);
    r->metadata.assumptions.push_back(format("Starts at rest. Time step %s; at %.4g Hz the method lengthens the period by %.2g %%. Mesh "
                                             "modes above what the time step resolves are not damped and can add noise.",
                                             basis.c_str(), highest, 100.0 * error));
    const double perWave = study->elementsPerWavelength(highest);
    if (perWave > 0.0 && perWave < 6.0)
        r->warnings.push_back(format("At %.4g Hz the mesh has only %.2g elements per %s: waves at that frequency are badly represented.",
                                     highest, perWave, study->waveDescription().c_str()));
    r->metadata.elapsedSeconds = r->transient->elapsedSeconds;
    return std::shared_ptr<const StructuralResult>(r);
}

JobEnd StructuralJob::run(JobContext& context)
{
    StructuralOutcome outcome = work_(context);
    if (!outcome)
        return outcome.message() == "cancelled" ? JobEnd::cancelled() : JobEnd::failed(outcome.message());
    {
        const std::lock_guard lock(mutex_);
        result_ = outcome.take();
    }
    return JobEnd::completed(result()->name);
}

std::shared_ptr<const StructuralResult> StructuralJob::result() const
{
    const std::lock_guard lock(mutex_);
    return result_;
}

}  // namespace resonant::mechanics
