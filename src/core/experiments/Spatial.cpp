#include "experiments/Spatial.h"

#include "foundation/Units.h"

#include <array>
#include <cmath>
#include <limits>
#include <sstream>

namespace resonant::experiments {

namespace {

constexpr std::array<std::pair<ScanKind, std::string_view>, 7> kKinds{{
    {ScanKind::Fixed, "fixed"},
    {ScanKind::Imported, "imported"},
    {ScanKind::Linear, "linear"},
    {ScanKind::Planar, "planar"},
    {ScanKind::Volumetric, "volumetric"},
    {ScanKind::Angular, "angular"},
    {ScanKind::ProbeArray, "probe-array"},
}};

nlohmann::json triple(const double v[3]) { return nlohmann::json::array({v[0], v[1], v[2]}); }

bool readTriple(const nlohmann::json& v, const char* key, double out[3])
{
    if (!v.contains(key))
        return true;
    const auto& t = v[key];
    if (!t.is_array() || t.size() != 3)
        return false;
    for (int i = 0; i < 3; ++i) {
        if (!t[static_cast<size_t>(i)].is_number())
            return false;
        out[i] = t[static_cast<size_t>(i)].get<double>();
    }
    return true;
}

double dot(const double a[3], const double b[3]) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

}  // namespace

std::string_view toString(ScanKind kind)
{
    for (const auto& [k, name] : kKinds)
        if (k == kind)
            return name;
    return "fixed";
}

Result<ScanKind> scanKindFromString(std::string_view text)
{
    for (const auto& [k, name] : kKinds)
        if (name == text)
            return k;
    return Result<ScanKind>::failure("unknown scan kind '" + std::string(text) + "'");
}

nlohmann::json toJson(const ScanDefinition& s)
{
    nlohmann::json points = nlohmann::json::array();
    for (const auto& p : s.points)
        points.push_back(toJson(p));
    return {{"kind", toString(s.kind)},   {"points", points},      {"source", s.source},     {"start", triple(s.start)},
            {"end", triple(s.end)},       {"count", s.count},      {"origin", triple(s.origin)}, {"axisU", triple(s.axisU)},
            {"axisV", triple(s.axisV)},   {"axisW", triple(s.axisW)}, {"nu", s.nu},           {"nv", s.nv},
            {"nw", s.nw},                 {"center", triple(s.center)}, {"radius", s.radius},  {"startDeg", s.startDeg},
            {"stopDeg", s.stopDeg},       {"stepDeg", s.stepDeg},  {"plane", s.plane},       {"frame", s.frame},
            {"positionUncertainty", s.positionUncertainty}};
}

Result<ScanDefinition> scanDefinitionFromJson(const nlohmann::json& v)
{
    ScanDefinition s;
    if (v.is_null())
        return s;
    if (!v.is_object())
        return Result<ScanDefinition>::failure("a scan definition must be an object");
    auto kind = scanKindFromString(v.value("kind", std::string("fixed")));
    if (!kind)
        return Result<ScanDefinition>::failure(kind.message());
    s.kind = kind.value();
    for (const auto& p : v.value("points", nlohmann::json::array())) {
        auto pose = poseFromJson(p);
        if (!pose)
            return Result<ScanDefinition>::failure("scan point: " + pose.message());
        s.points.push_back(pose.value());
    }
    s.source = v.value("source", std::string());
    if (!readTriple(v, "start", s.start) || !readTriple(v, "end", s.end) || !readTriple(v, "origin", s.origin) ||
        !readTriple(v, "axisU", s.axisU) || !readTriple(v, "axisV", s.axisV) || !readTriple(v, "axisW", s.axisW) ||
        !readTriple(v, "center", s.center))
        return Result<ScanDefinition>::failure("scan vectors must be three numbers (m)");
    s.count = v.value("count", 0);
    s.nu = v.value("nu", 0);
    s.nv = v.value("nv", 0);
    s.nw = v.value("nw", 1);
    s.radius = v.value("radius", 1.0);
    s.startDeg = v.value("startDeg", 0.0);
    s.stopDeg = v.value("stopDeg", 360.0);
    s.stepDeg = v.value("stepDeg", 10.0);
    s.plane = v.value("plane", std::string("azimuth"));
    s.frame = v.value("frame", std::string("scene"));
    s.positionUncertainty = v.value("positionUncertainty", 0.0);
    return s;
}

Result<std::vector<Pose>> scanPositions(const ScanDefinition& s, std::vector<double>* anglesDeg)
{
    std::vector<Pose> out;
    auto at = [&](double x, double y, double z) {
        Pose p;
        p.x = x;
        p.y = y;
        p.z = z;
        p.frame = s.frame;
        p.positionUncertainty = s.positionUncertainty;
        return p;
    };
    switch (s.kind) {
    case ScanKind::Fixed:
    case ScanKind::Imported:
    case ScanKind::ProbeArray: out = s.points; break;
    case ScanKind::Linear:
        if (s.count < 1)
            return Result<std::vector<Pose>>::failure("a linear scan needs at least one point");
        for (int i = 0; i < s.count; ++i) {
            const double t = s.count == 1 ? 0.0 : static_cast<double>(i) / (s.count - 1);
            out.push_back(at(s.start[0] + t * (s.end[0] - s.start[0]), s.start[1] + t * (s.end[1] - s.start[1]),
                             s.start[2] + t * (s.end[2] - s.start[2])));
        }
        break;
    case ScanKind::Planar:
    case ScanKind::Volumetric: {
        const int nw = s.kind == ScanKind::Planar ? 1 : s.nw;
        if (s.nu < 1 || s.nv < 1 || nw < 1)
            return Result<std::vector<Pose>>::failure("a planar or volumetric scan needs at least one point along each axis");
        if (static_cast<long>(s.nu) * s.nv * nw > 1000000)
            return Result<std::vector<Pose>>::failure("the scan has more than a million points");
        for (int k = 0; k < nw; ++k)
            for (int j = 0; j < s.nv; ++j)
                for (int i = 0; i < s.nu; ++i)
                    out.push_back(at(s.origin[0] + i * s.axisU[0] + j * s.axisV[0] + k * s.axisW[0],
                                     s.origin[1] + i * s.axisU[1] + j * s.axisV[1] + k * s.axisW[1],
                                     s.origin[2] + i * s.axisU[2] + j * s.axisV[2] + k * s.axisW[2]));
        break;
    }
    case ScanKind::Angular: {
        if (!(s.stepDeg > 0.0) || !(s.radius > 0.0))
            return Result<std::vector<Pose>>::failure("an angular scan needs a positive step and radius");
        if (s.plane != "azimuth" && s.plane != "elevation")
            return Result<std::vector<Pose>>::failure("an angular scan's plane is 'azimuth' or 'elevation'");
        const int steps = static_cast<int>(std::floor((s.stopDeg - s.startDeg) / s.stepDeg + 1e-9)) + 1;
        for (int n = 0; n < steps; ++n) {
            const double deg = s.startDeg + n * s.stepDeg;
            if (s.plane == "azimuth" && n > 0 && std::abs(deg - s.startDeg - 360.0) < 1e-9)
                break;  // 360 is 0 again
            const double a = units::degreesToRadians(deg);
            Pose p;
            if (s.plane == "azimuth")
                p = at(s.center[0] + s.radius * std::cos(a), s.center[1] + s.radius * std::sin(a), s.center[2]);
            else
                p = at(s.center[0] + s.radius * std::sin(a), s.center[1], s.center[2] + s.radius * std::cos(a));
            p.dx = (s.center[0] - p.x) / s.radius;
            p.dy = (s.center[1] - p.y) / s.radius;
            p.dz = (s.center[2] - p.z) / s.radius;
            out.push_back(p);
            if (anglesDeg)
                anglesDeg->push_back(deg);
        }
        break;
    }
    }
    if (anglesDeg && s.kind != ScanKind::Angular)
        anglesDeg->assign(out.size(), 0.0);
    return out;
}

Result<ScanDefinition> importPositionsCsv(const std::string& text, const std::string& source)
{
    ScanDefinition s;
    s.kind = ScanKind::Imported;
    s.source = source;
    std::istringstream lines(text);
    std::string line;
    int number = 0;
    while (std::getline(lines, line)) {
        ++number;
        if (line.empty() || line[0] == '#')
            continue;
        for (char& c : line)
            if (c == ',' || c == ';' || c == '\t')
                c = ' ';
        std::istringstream fields(line);
        std::vector<double> values;
        std::string field;
        bool numeric = true;
        while (fields >> field) {
            try {
                size_t used = 0;
                values.push_back(std::stod(field, &used));
                if (used != field.size())
                    numeric = false;
            } catch (...) {
                numeric = false;
            }
        }
        if (!numeric) {
            if (s.points.empty() && number <= 2)
                continue;  // a header line
            return Result<ScanDefinition>::failure(source + ":" + std::to_string(number) + ": not a number");
        }
        if (values.size() != 3 && values.size() != 6)
            return Result<ScanDefinition>::failure(source + ":" + std::to_string(number) + ": expected x y z or x y z dx dy dz");
        Pose p;
        p.x = values[0];
        p.y = values[1];
        p.z = values[2];
        if (values.size() == 6) {
            const double n = std::sqrt(values[3] * values[3] + values[4] * values[4] + values[5] * values[5]);
            if (n < 1e-12)
                return Result<ScanDefinition>::failure(source + ":" + std::to_string(number) + ": zero direction");
            p.dx = values[3] / n;
            p.dy = values[4] / n;
            p.dz = values[5] / n;
        }
        s.points.push_back(p);
    }
    if (s.points.empty())
        return Result<ScanDefinition>::failure(source + ": no positions");
    return s;
}

nlohmann::json toJson(const SpatialDataset& d)
{
    nlohmann::json samples = nlohmann::json::array();
    for (const auto& s : d.samples)
        samples.push_back({{"pose", toJson(s.pose)},
                           {"value", {s.value.real(), s.value.imag()}},
                           {"standardUncertainty", s.standardUncertainty},
                           {"angleDeg", s.angleDeg},
                           {"measurementId", s.measurementId}});
    return {{"id", d.id},
            {"experimentId", d.experimentId},
            {"quantity", toJson(d.quantity)},
            {"frequencyHz", d.frequencyHz},
            {"category", toString(d.category)},
            {"frame", d.frame},
            {"samples", samples},
            {"note", d.note}};
}

Result<SpatialDataset> spatialDatasetFromJson(const nlohmann::json& v)
{
    if (!v.is_object())
        return Result<SpatialDataset>::failure("a spatial data set must be an object");
    SpatialDataset d;
    d.id = v.value("id", std::string());
    d.experimentId = v.value("experimentId", std::string());
    auto q = quantitySpecFromJson(v.value("quantity", nlohmann::json::object()));
    if (!q)
        return Result<SpatialDataset>::failure(q.message());
    d.quantity = q.value();
    d.frequencyHz = v.value("frequencyHz", 0.0);
    auto c = dataCategoryFromString(v.value("category", std::string("processed")));
    if (!c)
        return Result<SpatialDataset>::failure(c.message());
    d.category = c.value();
    d.frame = v.value("frame", std::string("scene"));
    d.note = v.value("note", std::string());
    for (const auto& s : v.value("samples", nlohmann::json::array())) {
        SpatialSample sample;
        auto pose = poseFromJson(s.value("pose", nlohmann::json::object()));
        if (!pose)
            return Result<SpatialDataset>::failure(pose.message());
        sample.pose = pose.value();
        const auto value = s.value("value", nlohmann::json::array({0.0, 0.0}));
        if (!value.is_array() || value.size() != 2)
            return Result<SpatialDataset>::failure("a spatial sample value is [re, im]");
        sample.value = {value[0].get<double>(), value[1].get<double>()};
        sample.standardUncertainty = s.value("standardUncertainty", 0.0);
        sample.angleDeg = s.value("angleDeg", 0.0);
        sample.measurementId = s.value("measurementId", std::string());
        d.samples.push_back(sample);
    }
    return d;
}

Result<PlaneReconstruction> reconstructPlane(const SpatialDataset& dataset, const double origin[3], const double axisU[3],
                                             const double axisV[3], int nu, int nv, double maxDistance, double planeTolerance,
                                             double nodeTolerance)
{
    if (nu < 2 || nv < 2)
        return Result<PlaneReconstruction>::failure("a reconstruction plane needs at least 2 x 2 nodes");
    if (!(maxDistance > 0.0))
        return Result<PlaneReconstruction>::failure("the interpolation radius must be positive");
    // Plane normal.
    const double n[3] = {axisU[1] * axisV[2] - axisU[2] * axisV[1], axisU[2] * axisV[0] - axisU[0] * axisV[2],
                         axisU[0] * axisV[1] - axisU[1] * axisV[0]};
    const double nn = std::sqrt(dot(n, n));
    if (nn < 1e-15)
        return Result<PlaneReconstruction>::failure("the plane axes are parallel");
    struct Point {
        double p[3];
        std::complex<double> v;
    };
    std::vector<Point> points;
    for (const auto& s : dataset.samples) {
        if (s.pose.frame != dataset.frame)
            return Result<PlaneReconstruction>::failure("sample in frame '" + s.pose.frame + "' in a data set of frame '" +
                                                        dataset.frame + "': transform the positions first");
        const double d[3] = {s.pose.x - origin[0], s.pose.y - origin[1], s.pose.z - origin[2]};
        if (std::abs(dot(d, n)) / nn <= planeTolerance)
            points.push_back({{s.pose.x, s.pose.y, s.pose.z}, s.value});
    }
    PlaneReconstruction r;
    for (int i = 0; i < 3; ++i) {
        r.origin[i] = origin[i];
        r.axisU[i] = axisU[i];
        r.axisV[i] = axisV[i];
    }
    r.nu = nu;
    r.nv = nv;
    r.maxDistance = maxDistance;
    r.samplesUsed = points.size();
    r.values.assign(static_cast<size_t>(nu * nv), {0.0, 0.0});
    r.coverage.assign(static_cast<size_t>(nu * nv), NodeCoverage::Empty);
    r.distanceToNearest.assign(static_cast<size_t>(nu * nv), std::numeric_limits<double>::infinity());
    for (int j = 0; j < nv; ++j)
        for (int i = 0; i < nu; ++i) {
            const double node[3] = {origin[0] + i * axisU[0] + j * axisV[0], origin[1] + i * axisU[1] + j * axisV[1],
                                    origin[2] + i * axisU[2] + j * axisV[2]};
            const size_t index = static_cast<size_t>(j * nu + i);
            std::complex<double> sum(0.0, 0.0);
            double weights = 0.0, nearest = std::numeric_limits<double>::infinity();
            const Point* exact = nullptr;
            for (const auto& p : points) {
                const double d[3] = {p.p[0] - node[0], p.p[1] - node[1], p.p[2] - node[2]};
                const double dist = std::sqrt(dot(d, d));
                if (dist < nearest) {
                    nearest = dist;
                    if (dist <= nodeTolerance)
                        exact = &p;
                }
                if (dist <= maxDistance && dist > 0.0) {
                    const double w = 1.0 / (dist * dist);
                    sum += w * p.v;
                    weights += w;
                }
            }
            r.distanceToNearest[index] = nearest;
            if (exact) {
                r.values[index] = exact->v;
                r.coverage[index] = NodeCoverage::Measured;
                ++r.measured;
            } else if (weights > 0.0) {
                r.values[index] = sum / weights;
                r.coverage[index] = NodeCoverage::Interpolated;
                ++r.interpolated;
            } else {
                ++r.empty;
            }
        }
    std::ostringstream method;
    method << "inverse-distance weighting (power 2) of complex values from " << points.size()
           << " sample(s) within " << planeTolerance << " m of the plane, radius " << maxDistance
           << " m; nodes without a sample in that radius are empty (no extrapolation)";
    r.method = method.str();
    return r;
}

}  // namespace resonant::experiments
