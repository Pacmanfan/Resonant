#include "matter/PropertyField.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>

namespace resonant::matter {

using nlohmann::json;

const std::vector<std::string>& fieldKinds()
{
    static const std::vector<std::string> kinds{"constant", "linear", "power-law", "exponential", "table"};
    return kinds;
}

double PropertyField::at(double s) const
{
    s = std::clamp(s, 0.0, 1.0);
    if (kind == "constant")
        return from;
    if (kind == "linear")
        return from + (to - from) * s;
    if (kind == "power-law")
        return from + (to - from) * std::pow(s, exponent);
    if (kind == "exponential") {
        // from * (to / from)^s: a constant relative gradient (from and to of one sign).
        return from * std::pow(to / from, s);
    }
    if (kind == "table") {
        if (table.empty())
            return std::nan("");
        if (s <= table.front().first)
            return table.front().second;
        for (size_t i = 1; i < table.size(); ++i)
            if (s <= table[i].first) {
                const auto& [s0, v0] = table[i - 1];
                const auto& [s1, v1] = table[i];
                return s1 > s0 ? v0 + (v1 - v0) * (s - s0) / (s1 - s0) : v1;
            }
        return table.back().second;
    }
    return std::nan("");
}

namespace {

double simpson(const std::function<double(double)>& f, double a, double b, double fa, double fm, double fb, double whole, double tol, int depth)
{
    const double m = 0.5 * (a + b);
    const double lm = 0.5 * (a + m), rm = 0.5 * (m + b);
    const double flm = f(lm), frm = f(rm);
    const double left = (m - a) / 6.0 * (fa + 4.0 * flm + fm);
    const double right = (b - m) / 6.0 * (fm + 4.0 * frm + fb);
    if (depth <= 0 || std::abs(left + right - whole) <= 15.0 * tol)
        return left + right + (left + right - whole) / 15.0;
    return simpson(f, a, m, fa, flm, fm, left, tol / 2.0, depth - 1) + simpson(f, m, b, fm, frm, fb, right, tol / 2.0, depth - 1);
}

double integrate(const std::function<double(double)>& f)
{
    // Split at the table's knots (kinks) so that each piece is smooth.
    const double fa = f(0.0), fm = f(0.5), fb = f(1.0);
    const double whole = (fa + 4.0 * fm + fb) / 6.0;
    const double scale = std::max({std::abs(fa), std::abs(fm), std::abs(fb), 1e-300});
    return simpson(f, 0.0, 1.0, fa, fm, fb, whole, 1e-13 * scale, 40);
}

}  // namespace

double PropertyField::mean() const
{
    if (kind == "constant")
        return from;
    if (kind == "table") {
        double sum = 0.0;
        double prevS = 0.0, prevV = at(0.0);
        for (const auto& [s, v] : table) {
            if (s <= prevS)
                continue;
            sum += 0.5 * (prevV + v) * (s - prevS);
            prevS = s;
            prevV = v;
        }
        sum += prevV * (1.0 - prevS);
        return sum;
    }
    return integrate([this](double s) { return at(s); });
}

double PropertyField::harmonicMean() const
{
    if (kind == "constant")
        return from;
    if (kind == "table") {
        // Piecewise linear: the exact integral of 1/v on each piece.
        double sum = 0.0;
        double prevS = 0.0, prevV = at(0.0);
        auto piece = [](double s0, double v0, double s1, double v1) {
            if (std::abs(v1 - v0) <= 1e-14 * std::abs(v0))
                return (s1 - s0) / v0;
            return (s1 - s0) * std::log(v1 / v0) / (v1 - v0);
        };
        for (const auto& [s, v] : table) {
            if (s <= prevS)
                continue;
            sum += piece(prevS, prevV, s, v);
            prevS = s;
            prevV = v;
        }
        if (prevS < 1.0)
            sum += (1.0 - prevS) / prevV;
        return 1.0 / sum;
    }
    if (kind == "linear" && std::abs(to - from) > 1e-14 * std::abs(from))
        return (to - from) / std::log(to / from);  // the exact series combination of a linear profile
    return 1.0 / integrate([this](double s) { return 1.0 / at(s); });
}

double PropertyField::minimum() const
{
    double lo = std::numeric_limits<double>::infinity();
    if (kind == "table") {
        for (const auto& [s, v] : table)
            lo = std::min(lo, v);
        return lo;
    }
    return std::min(at(0.0), at(1.0));  // the analytic kinds are monotonic
}

double PropertyField::maximum() const
{
    double hi = -std::numeric_limits<double>::infinity();
    if (kind == "table") {
        for (const auto& [s, v] : table)
            hi = std::max(hi, v);
        return hi;
    }
    return std::max(at(0.0), at(1.0));
}

std::vector<double> PropertyField::discretize(int n) const
{
    std::vector<double> out;
    for (int i = 0; i < n; ++i)
        out.push_back(at((i + 0.5) / n));
    return out;
}

double PropertyField::discretizationError(int n) const
{
    auto series = [this](int m) {
        double sum = 0.0;
        for (double v : discretize(m))
            sum += 1.0 / (m * v);
        return 1.0 / sum;
    };
    const double a = series(n), b = series(2 * n);
    return std::abs(a - b) / std::abs(b);
}

Status PropertyField::validate() const
{
    if (id.empty())
        return Status::failure("a property field needs an id");
    if (region.empty())
        return Status::failure("the field '" + id + "' names no region");
    if (property.empty())
        return Status::failure("the field '" + id + "' names no property");
    if (std::find(fieldKinds().begin(), fieldKinds().end(), kind) == fieldKinds().end())
        return Status::failure("the field '" + id + "' has an unknown kind '" + kind + "'");
    if (kind == "table") {
        if (table.size() < 2)
            return Status::failure("the tabulated field '" + id + "' needs at least two points");
        for (size_t i = 0; i < table.size(); ++i) {
            if (table[i].first < 0.0 || table[i].first > 1.0)
                return Status::failure("the field '" + id + "' has a position outside [0, 1]");
            if (i > 0 && table[i].first <= table[i - 1].first)
                return Status::failure("the field '" + id + "' has positions that do not increase");
            if (!std::isfinite(table[i].second))
                return Status::failure("the field '" + id + "' has a value that is not finite");
        }
    } else if (!std::isfinite(from) || !std::isfinite(to)) {
        return Status::failure("the field '" + id + "' has values that are not finite");
    }
    if (kind == "exponential" && (from == 0.0 || from * to <= 0.0))
        return Status::failure("the exponential field '" + id + "' needs end values of one sign");
    if (kind == "power-law" && !(exponent > 0.0))
        return Status::failure("the power-law field '" + id + "' needs a positive exponent");
    if (realizable && process.empty())
        return Status::failure("the field '" + id + "' is marked realizable but names no process");
    return Status::success();
}

json PropertyField::toJson() const
{
    json j{{"id", id}, {"region", region}, {"property", property}, {"axis", axis}, {"kind", kind}, {"from", from}, {"to", to},
           {"exponent", exponent}, {"unit", unit}, {"realizable", realizable}, {"process", process}, {"provenance", provenance}, {"note", note}};
    json t = json::array();
    for (const auto& [s, v] : table)
        t.push_back(json::array({s, v}));
    j["table"] = t;
    return j;
}

Result<PropertyField> PropertyField::fromJson(const json& j)
{
    if (!j.is_object())
        return Result<PropertyField>::failure("a property field is an object");
    PropertyField f;
    f.id = j.value("id", std::string());
    f.region = j.value("region", std::string());
    f.property = j.value("property", std::string());
    f.axis = j.value("axis", std::string("stack"));
    f.kind = j.value("kind", std::string("linear"));
    f.from = j.value("from", 0.0);
    f.to = j.value("to", f.from);
    f.exponent = j.value("exponent", 1.0);
    f.unit = j.value("unit", std::string());
    f.realizable = j.value("realizable", false);
    f.process = j.value("process", std::string());
    f.provenance = j.value("provenance", std::string("unstated"));
    f.note = j.value("note", std::string());
    const json table = j.value("table", json::array());
    for (const json& p : table) {
        if (!p.is_array() || p.size() != 2 || !p[0].is_number() || !p[1].is_number())
            return Result<PropertyField>::failure("the field '" + f.id + "' has a table entry that is not [position, value]");
        f.table.emplace_back(p[0].get<double>(), p[1].get<double>());
    }
    if (Status s = f.validate(); !s)
        return Result<PropertyField>::failure(s.message());
    return f;
}

}  // namespace resonant::matter
