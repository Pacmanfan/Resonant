#include "electromagnetics/EmComponents.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <stdexcept>

namespace resonant::em {
namespace {

using json = nlohmann::json;

template <typename Build>
Result<ComponentPtr> guarded(const char* what, Build&& build)
{
    try {
        return ComponentPtr(build());
    } catch (const std::exception& error) {
        return Result<ComponentPtr>::failure(std::string("invalid ") + what + ": " + error.what());
    }
}

template <typename Enum, size_t N>
Enum parse(const std::string& text, const Enum (&values)[N], const char* what)
{
    for (const Enum v : values)
        if (toString(v) == text)
            return v;
    throw std::runtime_error(std::string("unknown ") + what + " '" + text + "'");
}

constexpr SourceKind kSourceKinds[] = {SourceKind::ElectricDipole, SourceKind::MagneticDipole, SourceKind::PlaneWave};
constexpr EmArrayLayout kLayouts[] = {EmArrayLayout::Linear, EmArrayLayout::Planar, EmArrayLayout::Circular, EmArrayLayout::Custom};
constexpr ElementPattern kPatterns[] = {ElementPattern::Isotropic, ElementPattern::ShortDipole, ElementPattern::HalfWaveDipole,
                                        ElementPattern::CosineHemisphere};

dsp::Waveform waveformOf(const json& v)
{
    if (!v.contains("waveform"))
        return {};
    auto w = dsp::waveformFromJson(v["waveform"]);
    if (!w)
        throw std::runtime_error(w.message());
    return w.take();
}

}  // namespace

bool isAntennaGeometry(const Geometry& g)
{
    return g.kind().category == "antenna";
}

AntennaModel supportOf(const Geometry& g)
{
    const std::string& k = g.kindId();
    return k == "antenna.dipole" || k == "antenna.monopole" || k == "antenna.loop" ? AntennaModel::Analytical : AntennaModel::Estimate;
}

std::string supportDescription(const Geometry& g)
{
    const std::string& k = g.kindId();
    if (k == "antenna.dipole")
        return "Analytical: thin-wire dipole with the assumed sinusoidal current - exact fields of that current, induced-EMF "
               "impedance. Accurate for thin wires up to about one wavelength long. Not a numerical solution of the antenna.";
    if (k == "antenna.monopole")
        return "Analytical: thin-wire monopole over an INFINITE perfectly conducting ground plane (image theory); the drawn ground "
               "disc is only a picture. Field above the ground plane only.";
    if (k == "antenna.loop")
        return "Analytical: electrically small loop (uniform current, magnetic dipole). Valid while the circumference is below "
               "about 0.3 wavelengths.";
    return "Design estimate only: the geometry and approximate design formulas exist, but no field, pattern or impedance is "
           "calculated for this antenna.";
}

Status AntennaSettings::validate() const
{
    if (!(std::isfinite(frequencyHz) && frequencyHz > 0.0))
        return Status::failure("the frequency must be positive");
    if (!(std::isfinite(feedCurrent) && feedCurrent >= 0.0) || !std::isfinite(feedPhase) || !(std::isfinite(delay) && delay >= 0.0))
        return Status::failure("the feed current must be zero or positive, with a finite phase and a non-negative delay");
    if (!(std::isfinite(referenceImpedance) && referenceImpedance > 0.0))
        return Status::failure("the reference impedance must be positive");
    return waveform.validate();
}

json AntennaComponent::toJson() const
{
    return {{"frequencyHz", settings.frequencyHz}, {"feedCurrent", settings.feedCurrent}, {"feedPhase", settings.feedPhase},
            {"delay", settings.delay}, {"referenceImpedance", settings.referenceImpedance}, {"waveform", dsp::toJson(settings.waveform)},
            {"active", settings.active}, {"measuredData", settings.measuredData}};
}

bool AntennaComponent::equals(const Component& other) const
{
    const auto* o = dynamic_cast<const AntennaComponent*>(&other);
    return o && o->settings == settings;
}

std::string_view toString(SourceKind kind)
{
    switch (kind) {
    case SourceKind::ElectricDipole: return "electricDipole";
    case SourceKind::MagneticDipole: return "magneticDipole";
    case SourceKind::PlaneWave: return "planeWave";
    }
    return "electricDipole";
}

Status SourceSettings::validate() const
{
    if (!(std::isfinite(frequencyHz) && frequencyHz > 0.0))
        return Status::failure("the frequency must be positive");
    if (!(std::isfinite(amplitude) && amplitude >= 0.0) || !std::isfinite(phase) || !(std::isfinite(delay) && delay >= 0.0))
        return Status::failure("the amplitude must be zero or positive, with a finite phase and a non-negative delay");
    return waveform.validate();
}

json SourceComponent::toJson() const
{
    return {{"kind", std::string(toString(settings.kind))}, {"frequencyHz", settings.frequencyHz}, {"amplitude", settings.amplitude},
            {"phase", settings.phase}, {"delay", settings.delay}, {"waveform", dsp::toJson(settings.waveform)}, {"active", settings.active}};
}

bool SourceComponent::equals(const Component& other) const
{
    const auto* o = dynamic_cast<const SourceComponent*>(&other);
    return o && o->settings == settings;
}

std::string_view toString(EmArrayLayout layout)
{
    switch (layout) {
    case EmArrayLayout::Linear: return "linear";
    case EmArrayLayout::Planar: return "planar";
    case EmArrayLayout::Circular: return "circular";
    case EmArrayLayout::Custom: return "custom";
    }
    return "linear";
}

int EmArraySettings::elementCount() const
{
    switch (layout) {
    case EmArrayLayout::Linear: return countX;
    case EmArrayLayout::Planar: return countX * countY;
    case EmArrayLayout::Circular: return countX;
    case EmArrayLayout::Custom: return static_cast<int>(customPositions.size());
    }
    return 0;
}

Status EmArraySettings::validate() const
{
    if (countX < 1 || countY < 1 || elementCount() < 1 || elementCount() > 4096)
        return Status::failure("an array needs between 1 and 4096 elements");
    if (!(spacingX > 0.0 && spacingY > 0.0 && radius > 0.0))
        return Status::failure("spacings and the radius must be positive");
    if (!(std::isfinite(frequencyHz) && frequencyHz > 0.0) || !(std::isfinite(amplitude) && amplitude >= 0.0))
        return Status::failure("the frequency must be positive and the amplitude zero or positive");
    return waveform.validate();
}

ArrayModel arrayModelOf(const EmArraySettings& s)
{
    ArrayModel model;
    model.pattern = s.pattern;
    std::vector<Vec3> positions;
    switch (s.layout) {
    case EmArrayLayout::Linear:
        for (int i = 0; i < s.countX; ++i)
            positions.emplace_back((i - 0.5 * (s.countX - 1)) * s.spacingX, 0.0, 0.0);
        break;
    case EmArrayLayout::Planar:
        for (int j = 0; j < s.countY; ++j)
            for (int i = 0; i < s.countX; ++i)
                positions.emplace_back((i - 0.5 * (s.countX - 1)) * s.spacingX, (j - 0.5 * (s.countY - 1)) * s.spacingY, 0.0);
        break;
    case EmArrayLayout::Circular:
        for (int i = 0; i < s.countX; ++i) {
            const double a = 2.0 * kPi * i / s.countX;
            positions.emplace_back(s.radius * std::cos(a), s.radius * std::sin(a), 0.0);
        }
        break;
    case EmArrayLayout::Custom: positions = s.customPositions; break;
    }
    for (size_t n = 0; n < positions.size(); ++n) {
        ArrayElement e;
        e.position = positions[n];
        e.axis = Vec3::UnitZ();
        double amplitude = s.amplitude, phase = 0.0;
        e.active = s.active;
        if (const auto it = s.overrides.find(static_cast<int>(n)); it != s.overrides.end()) {
            if (it->second.amplitude)
                amplitude = *it->second.amplitude;
            if (it->second.phase)
                phase = *it->second.phase;
            if (it->second.delay)
                e.delay = *it->second.delay;
            if (it->second.active)
                e.active = e.active && *it->second.active;
        }
        e.weight = std::polar(amplitude, phase);
        model.elements.push_back(e);
    }
    return model;
}

EmArraySettings withExcitation(const EmArraySettings& s, const ArrayModel& model)
{
    EmArraySettings out = s;
    for (size_t n = 0; n < model.elements.size(); ++n) {
        ElementExcitation& o = out.overrides[static_cast<int>(n)];
        o.amplitude = std::abs(model.elements[n].weight);
        double phase = std::arg(model.elements[n].weight);
        if (phase < 0.0)
            phase += 2.0 * kPi;
        o.phase = phase;
        o.delay = model.elements[n].delay;
    }
    return out;
}

json EmArrayComponent::toJson() const
{
    json positions = json::array();
    for (const Vec3& p : settings.customPositions)
        positions.push_back({p.x(), p.y(), p.z()});
    json overrides = json::object();
    for (const auto& [index, o] : settings.overrides) {
        if (o.empty())
            continue;
        json entry = json::object();
        if (o.amplitude)
            entry["amplitude"] = *o.amplitude;
        if (o.phase)
            entry["phase"] = *o.phase;
        if (o.delay)
            entry["delay"] = *o.delay;
        if (o.active)
            entry["active"] = *o.active;
        overrides[std::to_string(index)] = entry;
    }
    return {{"layout", std::string(toString(settings.layout))}, {"countX", settings.countX}, {"countY", settings.countY},
            {"spacingX", settings.spacingX}, {"spacingY", settings.spacingY}, {"radius", settings.radius},
            {"customPositions", positions}, {"pattern", std::string(toString(settings.pattern))}, {"frequencyHz", settings.frequencyHz},
            {"amplitude", settings.amplitude}, {"overrides", overrides}, {"waveform", dsp::toJson(settings.waveform)},
            {"active", settings.active}};
}

bool EmArrayComponent::equals(const Component& other) const
{
    const auto* o = dynamic_cast<const EmArrayComponent*>(&other);
    return o && o->settings == settings;
}

Status GuideSettings::validate() const
{
    return std::isfinite(frequencyHz) && frequencyHz > 0.0 ? Status::success() : Status::failure("the frequency must be positive");
}

json GuideComponent::toJson() const
{
    return {{"frequencyHz", settings.frequencyHz}};
}

bool GuideComponent::equals(const Component& other) const
{
    const auto* o = dynamic_cast<const GuideComponent*>(&other);
    return o && o->settings == settings;
}

bool isGuideGeometry(const Geometry& g)
{
    return g.kind().category == "emGuide";
}

void registerElectromagneticComponents(ComponentRegistry& registry)
{
    registry.add(object_types::Guide, [](const json& v) {
        return guarded("waveguide", [&] {
            GuideSettings s;
            s.frequencyHz = v.value("frequencyHz", s.frequencyHz);
            if (Status status = s.validate(); !status)
                throw std::runtime_error(status.message());
            return std::make_shared<GuideComponent>(s);
        });
    });
    registry.add(object_types::Antenna, [](const json& v) {
        return guarded("antenna", [&] {
            AntennaSettings s;
            s.frequencyHz = v.value("frequencyHz", s.frequencyHz);
            s.feedCurrent = v.value("feedCurrent", s.feedCurrent);
            s.feedPhase = v.value("feedPhase", s.feedPhase);
            s.delay = v.value("delay", s.delay);
            s.referenceImpedance = v.value("referenceImpedance", s.referenceImpedance);
            s.waveform = waveformOf(v);
            s.active = v.value("active", true);
            s.measuredData = v.value("measuredData", std::string());
            if (Status status = s.validate(); !status)
                throw std::runtime_error(status.message());
            return std::make_shared<AntennaComponent>(s);
        });
    });
    registry.add(object_types::Source, [](const json& v) {
        return guarded("electromagnetic source", [&] {
            SourceSettings s;
            s.kind = parse(v.value("kind", std::string("electricDipole")), kSourceKinds, "source kind");
            s.frequencyHz = v.value("frequencyHz", s.frequencyHz);
            s.amplitude = v.value("amplitude", s.amplitude);
            s.phase = v.value("phase", s.phase);
            s.delay = v.value("delay", s.delay);
            s.waveform = waveformOf(v);
            s.active = v.value("active", true);
            if (Status status = s.validate(); !status)
                throw std::runtime_error(status.message());
            return std::make_shared<SourceComponent>(s);
        });
    });
    registry.add(object_types::Array, [](const json& v) {
        return guarded("electromagnetic array", [&] {
            EmArraySettings s;
            s.layout = parse(v.value("layout", std::string("linear")), kLayouts, "layout");
            s.countX = v.value("countX", s.countX);
            s.countY = v.value("countY", s.countY);
            s.spacingX = v.value("spacingX", s.spacingX);
            s.spacingY = v.value("spacingY", s.spacingY);
            s.radius = v.value("radius", s.radius);
            for (const json& p : v.value("customPositions", json::array()))
                s.customPositions.emplace_back(p.at(0).get<double>(), p.at(1).get<double>(), p.at(2).get<double>());
            s.pattern = parse(v.value("pattern", std::string("halfWaveDipole")), kPatterns, "element pattern");
            s.frequencyHz = v.value("frequencyHz", s.frequencyHz);
            s.amplitude = v.value("amplitude", s.amplitude);
            const json overrides = v.value("overrides", json::object());  // a named value: items() of a temporary would dangle
            for (const auto& [key, entry] : overrides.items()) {
                ElementExcitation o;
                if (entry.contains("amplitude"))
                    o.amplitude = entry["amplitude"].get<double>();
                if (entry.contains("phase"))
                    o.phase = entry["phase"].get<double>();
                if (entry.contains("delay"))
                    o.delay = entry["delay"].get<double>();
                if (entry.contains("active"))
                    o.active = entry["active"].get<bool>();
                s.overrides[std::stoi(key)] = o;
            }
            s.waveform = waveformOf(v);
            s.active = v.value("active", true);
            if (Status status = s.validate(); !status)
                throw std::runtime_error(status.message());
            return std::make_shared<EmArrayComponent>(s);
        });
    });
}

}  // namespace resonant::em

namespace resonant {

namespace {

ParameterDef lengthDef(std::string key, std::string label, double value)
{
    ParameterDef d;
    d.key = std::move(key);
    d.label = std::move(label);
    d.unit = "m";
    d.defaultValue = value;
    d.minValue = 0.0;
    d.maxValue = 1e6;
    return d;
}

void addCylinder(TriangleMesh& mesh, double radius, double z0, double z1, int segments)
{
    revolveProfile(mesh, {{radius, z0, 1.0, 0.0}, {radius, z1, 1.0, 0.0}}, segments);
    revolveProfile(mesh, {{0.0, z0, 0.0, -1.0}, {radius, z0, 0.0, -1.0}}, segments);
    revolveProfile(mesh, {{0.0, z1, 0.0, 1.0}, {radius, z1, 0.0, 1.0}}, segments);
}

void addBox(TriangleMesh& mesh, const Vec3& lo, const Vec3& hi)
{
    Geometry box = Geometry::create("box").take();
    const Vec3 size = hi - lo;
    [[maybe_unused]] Status s1 = box.setParameter("width", std::max(size.x(), 1e-9));
    [[maybe_unused]] Status s2 = box.setParameter("depth", std::max(size.y(), 1e-9));
    [[maybe_unused]] Status s3 = box.setParameter("height", std::max(size.z(), 1e-9));
    TriangleMesh piece = box.tessellate();
    const Vec3 centre = 0.5 * (lo + hi);
    for (Eigen::Vector3f& p : piece.positions)
        p += centre.cast<float>();
    mesh.append(piece);
}

GeometryKind kind(std::string id, std::string name, std::vector<ParameterDef> parameters,
                  std::function<TriangleMesh(const Geometry&, const TessellationOptions&)> tessellate,
                  std::function<Status(const Geometry&)> validate = {})
{
    GeometryKind k;
    k.id = std::move(id);
    k.displayName = std::move(name);
    k.category = "antenna";
    k.parameters = std::move(parameters);
    k.tessellate = std::move(tessellate);
    k.validate = std::move(validate);
    return k;
}

}  // namespace

void registerAntennaGeometry(GeometryRegistry& registry)
{
    // Waveguides along local Z (inner dimensions; walls drawn 2 % of the size thick).
    const auto guideKind = [](std::string id, std::string name, std::vector<ParameterDef> parameters,
                              std::function<TriangleMesh(const Geometry&, const TessellationOptions&)> tessellate,
                              std::function<Status(const Geometry&)> validate = {}) {
        GeometryKind k = kind(std::move(id), std::move(name), std::move(parameters), std::move(tessellate), std::move(validate));
        k.category = "emGuide";
        return k;
    };
    const auto rectangularWalls = [](double a, double b, double z0, double z1, TriangleMesh& mesh) {
        const double t = 0.02 * std::max(a, b);
        addBox(mesh, Vec3(-0.5 * a - t, -0.5 * b - t, z0), Vec3(-0.5 * a, 0.5 * b + t, z1));
        addBox(mesh, Vec3(0.5 * a, -0.5 * b - t, z0), Vec3(0.5 * a + t, 0.5 * b + t, z1));
        addBox(mesh, Vec3(-0.5 * a, -0.5 * b - t, z0), Vec3(0.5 * a, -0.5 * b, z1));
        addBox(mesh, Vec3(-0.5 * a, 0.5 * b, z0), Vec3(0.5 * a, 0.5 * b + t, z1));
    };
    registry.add(guideKind("guide.rectangular", "Rectangular waveguide (EM)",
                           {lengthDef("a", "Inner width a (X)", 0.02286), lengthDef("b", "Inner height b (Y)", 0.01016),
                            lengthDef("length", "Length (Z)", 0.1)},
                           [rectangularWalls](const Geometry& g, const TessellationOptions&) {
                               TriangleMesh mesh;
                               rectangularWalls(g.parameter("a"), g.parameter("b"), -0.5 * g.parameter("length"), 0.5 * g.parameter("length"), mesh);
                               return mesh;
                           },
                           [](const Geometry& g) {
                               return g.parameter("a") >= g.parameter("b") ? Status::success()
                                                                          : Status::failure("the width a must not be smaller than the height b");
                           }));
    registry.add(guideKind("guide.circular", "Circular waveguide (EM)", {lengthDef("radius", "Inner radius", 0.01), lengthDef("length", "Length (Z)", 0.1)},
                           [](const Geometry& g, const TessellationOptions& o) {
                               TriangleMesh mesh;
                               const double r = g.parameter("radius"), h = 0.5 * g.parameter("length");
                               revolveProfile(mesh, {{r, -h, -1.0, 0.0}, {r, h, -1.0, 0.0}}, o.segments);
                               revolveProfile(mesh, {{1.04 * r, -h, 1.0, 0.0}, {1.04 * r, h, 1.0, 0.0}}, o.segments);
                               return mesh;
                           }));
    registry.add(guideKind("guide.coaxial", "Coaxial line (EM)",
                           {lengthDef("innerRadius", "Inner conductor radius", 0.00045), lengthDef("outerRadius", "Outer conductor inner radius", 0.00147),
                            lengthDef("length", "Length (Z)", 0.05)},
                           [](const Geometry& g, const TessellationOptions& o) {
                               TriangleMesh mesh;
                               const double h = 0.5 * g.parameter("length");
                               addCylinder(mesh, g.parameter("innerRadius"), -h, h, o.segments / 2);
                               revolveProfile(mesh, {{g.parameter("outerRadius"), -h, -1.0, 0.0}, {g.parameter("outerRadius"), h, -1.0, 0.0}}, o.segments);
                               return mesh;
                           },
                           [](const Geometry& g) {
                               return g.parameter("innerRadius") < g.parameter("outerRadius")
                                          ? Status::success()
                                          : Status::failure("the inner conductor must be smaller than the outer one");
                           }));
    registry.add(guideKind("cavity.rectangular.em", "Rectangular cavity (EM)",
                           {lengthDef("a", "Inner size a (X)", 0.03), lengthDef("b", "Inner size b (Y)", 0.018), lengthDef("d", "Inner size d (Z)", 0.042)},
                           [rectangularWalls](const Geometry& g, const TessellationOptions&) {
                               TriangleMesh mesh;
                               const double a = g.parameter("a"), b = g.parameter("b"), d = g.parameter("d"), t = 0.02 * std::max(a, b);
                               rectangularWalls(a, b, -0.5 * d, 0.5 * d, mesh);
                               addBox(mesh, Vec3(-0.5 * a - t, -0.5 * b - t, -0.5 * d - t), Vec3(0.5 * a + t, 0.5 * b + t, -0.5 * d));
                               addBox(mesh, Vec3(-0.5 * a - t, -0.5 * b - t, 0.5 * d), Vec3(0.5 * a + t, 0.5 * b + t, 0.5 * d + t));
                               return mesh;
                           }));

    // Dipole along local Z, centred, feed gap at the origin.
    registry.add(kind("antenna.dipole", "Dipole antenna",
                      {lengthDef("length", "Total length", 0.15), lengthDef("wireRadius", "Wire radius", 0.001)},
                      [](const Geometry& g, const TessellationOptions& o) {
                          TriangleMesh mesh;
                          const double h = 0.5 * g.parameter("length"), a = g.parameter("wireRadius"), gap = std::min(0.5 * a, 0.05 * h);
                          addCylinder(mesh, a, gap, h, o.segments / 2);
                          addCylinder(mesh, a, -h, -gap, o.segments / 2);
                          return mesh;
                      },
                      [](const Geometry& g) {
                          return g.parameter("wireRadius") < 0.05 * g.parameter("length")
                                     ? Status::success()
                                     : Status::failure("the wire radius must be less than 5 % of the length (a thin-wire antenna)");
                      }));
    // Monopole from the origin along +Z over a ground disc in the XY plane.
    registry.add(kind("antenna.monopole", "Monopole antenna",
                      {lengthDef("height", "Height", 0.075), lengthDef("wireRadius", "Wire radius", 0.001),
                       lengthDef("groundRadius", "Ground plane radius (drawn)", 0.2)},
                      [](const Geometry& g, const TessellationOptions& o) {
                          TriangleMesh mesh;
                          addCylinder(mesh, g.parameter("wireRadius"), 0.0, g.parameter("height"), o.segments / 2);
                          addCylinder(mesh, g.parameter("groundRadius"), -0.001 * g.parameter("groundRadius"), 0.0, o.segments);
                          return mesh;
                      },
                      [](const Geometry& g) {
                          return g.parameter("wireRadius") < 0.05 * g.parameter("height")
                                     ? Status::success()
                                     : Status::failure("the wire radius must be less than 5 % of the height (a thin-wire antenna)");
                      }));
    // Loop in the XY plane, axis along Z.
    registry.add(kind("antenna.loop", "Loop antenna",
                      {lengthDef("loopRadius", "Loop radius", 0.01), lengthDef("wireRadius", "Wire radius", 0.0005)},
                      [](const Geometry& g, const TessellationOptions& o) {
                          TriangleMesh mesh;
                          const double r = g.parameter("loopRadius"), a = g.parameter("wireRadius");
                          const int n = std::max(16, o.segments);
                          // A torus from rings of quads.
                          for (int i = 0; i < n; ++i)
                              for (int j = 0; j < 8; ++j) {
                                  const auto point = [&](int ii, int jj) {
                                      const double u = 2 * em::kPi * ii / n, v = 2 * em::kPi * jj / 8;
                                      return Vec3((r + a * std::cos(v)) * std::cos(u), (r + a * std::cos(v)) * std::sin(u), a * std::sin(v));
                                  };
                                  const auto normal = [&](int ii, int jj) {
                                      const double u = 2 * em::kPi * ii / n, v = 2 * em::kPi * jj / 8;
                                      return Vec3(std::cos(v) * std::cos(u), std::cos(v) * std::sin(u), std::sin(v));
                                  };
                                  const auto a0 = mesh.addVertex(point(i, j), normal(i, j));
                                  const auto a1 = mesh.addVertex(point(i + 1, j), normal(i + 1, j));
                                  const auto a2 = mesh.addVertex(point(i + 1, j + 1), normal(i + 1, j + 1));
                                  const auto a3 = mesh.addVertex(point(i, j + 1), normal(i, j + 1));
                                  mesh.addTriangle(a0, a1, a2);
                                  mesh.addTriangle(a0, a2, a3);
                              }
                          return mesh;
                      },
                      [](const Geometry& g) {
                          return g.parameter("wireRadius") < 0.5 * g.parameter("loopRadius") ? Status::success()
                                                                                              : Status::failure("the wire radius must be less than half the loop radius");
                      }));
    // Rectangular patch on a substrate over ground, radiating along +Z.
    registry.add(kind("antenna.patch", "Patch antenna",
                      {lengthDef("patchLength", "Patch length (X, resonant)", 0.029), lengthDef("patchWidth", "Patch width (Y)", 0.038),
                       lengthDef("substrateThickness", "Substrate thickness", 0.0016), lengthDef("substrateSize", "Substrate side", 0.06)},
                      [](const Geometry& g, const TessellationOptions&) {
                          TriangleMesh mesh;
                          const double s = 0.5 * g.parameter("substrateSize"), t = g.parameter("substrateThickness");
                          addBox(mesh, Vec3(-s, -s, -t), Vec3(s, s, 0.0));
                          addBox(mesh, Vec3(-0.5 * g.parameter("patchLength"), -0.5 * g.parameter("patchWidth"), 0.0),
                                 Vec3(0.5 * g.parameter("patchLength"), 0.5 * g.parameter("patchWidth"), 0.02 * t));
                          return mesh;
                      }));
    // Pyramidal horn fed by a rectangular guide, aperture at +Z.
    registry.add(kind("antenna.horn", "Horn antenna",
                      {lengthDef("apertureA", "Aperture width (X)", 0.1), lengthDef("apertureB", "Aperture height (Y)", 0.08),
                       lengthDef("guideA", "Guide width a (X)", 0.02286), lengthDef("guideB", "Guide height b (Y)", 0.01016),
                       lengthDef("length", "Flare length (Z)", 0.12)},
                      [](const Geometry& g, const TessellationOptions&) {
                          TriangleMesh mesh;
                          const double A = 0.5 * g.parameter("apertureA"), B = 0.5 * g.parameter("apertureB");
                          const double a = 0.5 * g.parameter("guideA"), b = 0.5 * g.parameter("guideB"), L = g.parameter("length");
                          const Vec3 back[4] = {{-a, -b, 0}, {a, -b, 0}, {a, b, 0}, {-a, b, 0}};
                          const Vec3 front[4] = {{-A, -B, L}, {A, -B, L}, {A, B, L}, {-A, B, L}};
                          for (int i = 0; i < 4; ++i) {
                              const int j = (i + 1) % 4;
                              const Vec3 n = (back[j] - back[i]).cross(front[i] - back[i]).normalized();
                              const auto v0 = mesh.addVertex(back[i], -n), v1 = mesh.addVertex(back[j], -n);
                              const auto v2 = mesh.addVertex(front[j], -n), v3 = mesh.addVertex(front[i], -n);
                              mesh.addTriangle(v0, v1, v2);
                              mesh.addTriangle(v0, v2, v3);
                          }
                          addBox(mesh, Vec3(-a, -b, -0.5 * L), Vec3(a, b, 0.0));
                          return mesh;
                      }));
    // Axial-mode helix along +Z over a ground disc.
    registry.add(kind("antenna.helix", "Helical antenna",
                      {lengthDef("diameter", "Helix diameter", 0.04), lengthDef("pitch", "Pitch (spacing per turn)", 0.03),
                       ParameterDef{"turns", "Turns", "", 8.0, 1.0, 100.0, false, true}, lengthDef("wireRadius", "Wire radius", 0.001)},
                      [](const Geometry& g, const TessellationOptions&) {
                          TriangleMesh mesh;
                          const double r = 0.5 * g.parameter("diameter"), p = g.parameter("pitch"), a = g.parameter("wireRadius");
                          const int steps = static_cast<int>(g.parameter("turns") * 24);
                          for (int i = 0; i < steps; ++i) {
                              const double t0 = 2 * em::kPi * i / 24, t1 = 2 * em::kPi * (i + 1) / 24;
                              const Vec3 c0(r * std::cos(t0), r * std::sin(t0), p * t0 / (2 * em::kPi));
                              const Vec3 c1(r * std::cos(t1), r * std::sin(t1), p * t1 / (2 * em::kPi));
                              const Vec3 axis = (c1 - c0).normalized(), side = axis.unitOrthogonal(), up = axis.cross(side);
                              for (int j = 0; j < 6; ++j) {
                                  const double v0 = 2 * em::kPi * j / 6, v1 = 2 * em::kPi * (j + 1) / 6;
                                  const Vec3 n0 = std::cos(v0) * side + std::sin(v0) * up, n1 = std::cos(v1) * side + std::sin(v1) * up;
                                  const auto q0 = mesh.addVertex(c0 + a * n0, n0), q1 = mesh.addVertex(c1 + a * n0, n0);
                                  const auto q2 = mesh.addVertex(c1 + a * n1, n1), q3 = mesh.addVertex(c0 + a * n1, n1);
                                  mesh.addTriangle(q0, q1, q2);
                                  mesh.addTriangle(q0, q2, q3);
                              }
                          }
                          addCylinder(mesh, 0.75 * r * 2.0, -0.001, 0.0, 32);
                          return mesh;
                      }));
}

}  // namespace resonant
