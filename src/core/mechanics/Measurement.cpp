#include "mechanics/Measurement.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace resonant::mechanics {
namespace {

std::string trim(std::string s)
{
    const auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

std::vector<std::string> split(const std::string& line)
{
    std::vector<std::string> out;
    std::string cell;
    std::stringstream stream(line);
    while (std::getline(stream, cell, ','))
        out.push_back(trim(cell));
    return out;
}

std::optional<Vec3> vectorOf(const std::string& text)
{
    std::stringstream s(text);
    double x, y, z;
    if (s >> x >> y >> z)
        return Vec3(x, y, z);
    return std::nullopt;
}

}  // namespace

Result<MeasuredResponse> parseMeasuredResponse(const std::string& text, const std::string& name)
{
    using Failure = Result<MeasuredResponse>;
    MeasuredResponse r;
    r.name = name;
    std::stringstream stream(text);
    std::string line;
    int lineNumber = 0;
    int fColumn = -1, aColumn = -1, bColumn = -1;
    bool polar = true;
    while (std::getline(stream, line)) {
        ++lineNumber;
        line = trim(line);
        if (line.empty())
            continue;
        if (line[0] == '#') {
            const std::string body = trim(line.substr(1));
            const auto colon = body.find(':');
            if (colon == std::string::npos)
                continue;
            const std::string key = trim(body.substr(0, colon)), value = trim(body.substr(colon + 1));
            if (key == "sensor")
                r.sensor = value;
            else if (key == "unit")
                r.unit = value;
            else if (key == "source")
                r.source = value;
            else if (key == "sensitivity")
                r.sensitivity = std::atof(value.c_str());
            else if (key == "position")
                r.position = vectorOf(value);
            else if (key == "direction")
                r.direction = vectorOf(value);
            else if (key == "quantity")
                for (const Quantity q : {Quantity::Displacement, Quantity::Velocity, Quantity::Acceleration})
                    if (toString(q) == value)
                        r.quantity = q;
            continue;
        }
        const std::vector<std::string> cells = split(line);
        if (fColumn < 0) {
            for (size_t i = 0; i < cells.size(); ++i) {
                std::string c = cells[i];
                std::transform(c.begin(), c.end(), c.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
                if (c.rfind("frequency", 0) == 0)
                    fColumn = static_cast<int>(i);
                else if (c.rfind("magnitude", 0) == 0) {
                    aColumn = static_cast<int>(i);
                    polar = true;
                } else if (c.rfind("phase", 0) == 0)
                    bColumn = static_cast<int>(i);
                else if (c.rfind("real", 0) == 0) {
                    aColumn = static_cast<int>(i);
                    polar = false;
                } else if (c.rfind("imag", 0) == 0)
                    bColumn = static_cast<int>(i);
            }
            if (fColumn < 0 || aColumn < 0 || bColumn < 0)
                return Failure::failure("line " + std::to_string(lineNumber)
                                        + ": the header needs a frequency column and magnitude/phase_deg or real/imag columns");
            continue;
        }
        const int needed = std::max({fColumn, aColumn, bColumn});
        if (static_cast<int>(cells.size()) <= needed)
            return Failure::failure("line " + std::to_string(lineNumber) + " has too few values");
        char* end = nullptr;
        const double f = std::strtod(cells[static_cast<size_t>(fColumn)].c_str(), &end);
        const double a = std::strtod(cells[static_cast<size_t>(aColumn)].c_str(), nullptr);
        const double b = std::strtod(cells[static_cast<size_t>(bColumn)].c_str(), nullptr);
        if (!(std::isfinite(f) && f > 0.0) || !std::isfinite(a) || !std::isfinite(b))
            return Failure::failure("line " + std::to_string(lineNumber) + " has a value that is not a finite number (or a frequency <= 0)");
        if (!r.frequencyHz.empty() && f <= r.frequencyHz.back())
            return Failure::failure("line " + std::to_string(lineNumber) + ": frequencies must increase");
        r.frequencyHz.push_back(f);
        r.values.push_back(polar ? std::polar(a, b * 3.14159265358979323846 / 180.0) : std::complex<double>(a, b));
    }
    if (r.frequencyHz.size() < 2)
        return Failure::failure("the file has fewer than two data lines");
    return r;
}

ResponseComparison compareResponses(const std::vector<double>& simulatedHz, const std::vector<std::complex<double>>& simulated,
                                    const MeasuredResponse& measured)
{
    ResponseComparison c;
    if (simulatedHz.size() < 2 || simulated.size() != simulatedHz.size()) {
        c.note = "no simulated response to compare";
        return c;
    }
    double sum = 0.0, peakMeasured = 0.0, peakSimulated = 0.0;
    for (size_t i = 0; i < measured.frequencyHz.size(); ++i) {
        const double f = measured.frequencyHz[i];
        if (f < simulatedHz.front() || f > simulatedHz.back())
            continue;
        const auto upper = std::upper_bound(simulatedHz.begin(), simulatedHz.end(), f);
        const size_t hi = std::min<size_t>(static_cast<size_t>(upper - simulatedHz.begin()), simulatedHz.size() - 1), lo = hi - 1;
        const double t = (f - simulatedHz[lo]) / (simulatedHz[hi] - simulatedHz[lo]);
        const double magnitude = (1 - t) * std::abs(simulated[lo]) + t * std::abs(simulated[hi]);
        const double m = std::abs(measured.values[i]);
        if (magnitude > 0.0 && m > 0.0) {
            const double db = 20.0 * std::log10(magnitude / m);
            sum += db * db;
            ++c.points;
        }
        if (m > peakMeasured) {
            peakMeasured = m;
            c.largestPeakMeasuredHz = f;
        }
    }
    for (size_t i = 0; i < simulated.size(); ++i)
        if (std::abs(simulated[i]) > peakSimulated) {
            peakSimulated = std::abs(simulated[i]);
            c.largestPeakSimulatedHz = simulatedHz[i];
        }
    c.rmsLevelDifferenceDb = c.points ? std::sqrt(sum / static_cast<double>(c.points)) : 0.0;
    c.note = c.points ? "compared at " + std::to_string(c.points) + " measured frequencies inside the simulated range"
                      : "no measured frequency lies inside the simulated range";
    return c;
}

}  // namespace resonant::mechanics
