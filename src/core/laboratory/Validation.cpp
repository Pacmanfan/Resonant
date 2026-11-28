#include "laboratory/Validation.h"

#include "foundation/Dimensions.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace resonant::laboratory {

using nlohmann::json;
namespace ex = experiments;

namespace {

double interpolate(const std::vector<double>& x, const std::vector<double>& y, double at)
{
    const auto it = std::lower_bound(x.begin(), x.end(), at);
    if (it == x.begin())
        return y.front();
    if (it == x.end())
        return y.back();
    const size_t k = static_cast<size_t>(it - x.begin());
    const double t = (at - x[k - 1]) / (x[k] - x[k - 1]);
    return y[k - 1] + t * (y[k] - y[k - 1]);
}

double pearson(const std::vector<double>& a, const std::vector<double>& b)
{
    if (a.size() < 2)
        return 0.0;
    const double ma = std::accumulate(a.begin(), a.end(), 0.0) / a.size(), mb = std::accumulate(b.begin(), b.end(), 0.0) / b.size();
    double sab = 0, saa = 0, sbb = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        sab += (a[i] - ma) * (b[i] - mb);
        saa += (a[i] - ma) * (a[i] - ma);
        sbb += (b[i] - mb) * (b[i] - mb);
    }
    return saa > 0 && sbb > 0 ? sab / std::sqrt(saa * sbb) : 0.0;
}

bool identity(const std::array<double, 16>& t)
{
    for (int i = 0; i < 16; ++i)
        if (t[static_cast<size_t>(i)] != ((i % 5 == 0) ? 1.0 : 0.0))
            return false;
    return true;
}

}  // namespace

double consistencyThreshold(size_t n, double k)
{
    const double p = std::erf(k / std::sqrt(2.0));
    return n == 0 ? p : std::max(0.0, p - 3.0 * std::sqrt(p * (1.0 - p) / static_cast<double>(n)));
}

json ScalarComparison::toJson() const
{
    json j{{"measured", measured}, {"predicted", predicted}, {"difference", difference}, {"expandedMeasured", expandedMeasured},
           {"expandedPredicted", expandedPredicted}, {"verdict", verdict}, {"unit", unit}, {"notes", notes}};
    j["relative"] = relative ? json(*relative) : json(nullptr);
    j["normalizedError"] = normalizedError ? json(*normalizedError) : json(nullptr);
    return j;
}

Result<ScalarComparison> compareScalars(const ScalarValue& m, const ScalarValue& p)
{
    using R = Result<ScalarComparison>;
    ScalarComparison c;
    c.unit = m.unit;
    double pv = p.value, pu = p.standard;
    if (p.unit != m.unit) {
        auto f = convertUnit(1.0, p.unit, m.unit);
        if (!f)
            return R::failure("the prediction (" + p.unit + ") and the measurement (" + m.unit + ") are not comparable: " + f.message());
        pv *= f.value();
        pu *= f.value();
        c.notes.push_back("the prediction was converted from " + p.unit + " to " + m.unit);
    }
    if (!std::isfinite(m.value) || !std::isfinite(pv))
        return R::failure("a value is not finite");
    c.measured = m.value;
    c.predicted = pv;
    c.difference = m.value - pv;
    if (m.value != 0.0)
        c.relative = c.difference / std::abs(m.value);
    c.expandedMeasured = m.coverage * m.standard;
    c.expandedPredicted = p.coverage * pu;
    const double combined = std::hypot(c.expandedMeasured, c.expandedPredicted);
    if (m.standard <= 0.0) {
        c.verdict = "not assessable";
        c.notes.push_back("the measurement has no stated uncertainty: agreement cannot be judged");
    } else {
        c.normalizedError = std::abs(c.difference) / combined;
        c.verdict = *c.normalizedError <= 1.0 ? "consistent" : "inconsistent";
        if (pu <= 0.0)
            c.notes.push_back("the prediction's uncertainty is not stated (taken as zero, which favours inconsistency)");
    }
    return c;
}

json ResponseComparison::toJson() const
{
    return {{"base", ex::toJson(base)},
            {"peakOffsetHz", peakOffsetHz},
            {"peakOffsetRelative", peakOffsetRelative},
            {"peakAmplitudeOffsetDb", peakAmplitudeOffsetDb},
            {"spectralSimilarity", spectralSimilarity},
            {"fractionWithinCombined", fractionWithinCombined},
            {"frequencyHz", frequencyHz},
            {"measuredMagnitude", measuredMagnitude},
            {"predictedMagnitude", predictedMagnitude},
            {"residual", residual},
            {"verdict", verdict}};
}

Result<ResponseComparison> compareResponses(const ex::FrequencySeries& m, const ex::FrequencySeries& p, const ex::ComparisonOptions& o)
{
    using R = Result<ResponseComparison>;
    auto base = ex::compareFrequencySeries(m, p, o);
    if (!base)
        return R::failure(base.message());
    ResponseComparison c;
    c.base = base.take();
    // The compared points again, with both magnitudes (the base result keeps only the errors).
    const double lo = std::max(m.frequencyHz.front(), p.frequencyHz.front()), hi = std::min(m.frequencyHz.back(), p.frequencyHz.back());
    std::vector<double> pf = p.frequencyHz, pm;
    for (const auto& v : p.values)
        pm.push_back(std::abs(v));
    double dot = 0, nm = 0, np = 0;
    size_t within = 0, withUncertainty = 0;
    double mPeak = 0, pPeak = 0, mPeakHz = 0, pPeakHz = 0;
    double mScale = 1.0, pScale = 1.0;
    if (o.relative) {
        mScale = 0.0;
        for (const auto& v : m.values)
            mScale = std::max(mScale, std::abs(v));
        pScale = *std::max_element(pm.begin(), pm.end());
        if (!(mScale > 0) || !(pScale > 0))
            return R::failure("a response without magnitude");
    }
    for (size_t k = 0; k < m.frequencyHz.size(); ++k) {
        const double f = m.frequencyHz[k];
        if (f < lo || f > hi)
            continue;
        const double a = std::abs(m.values[k]) / (o.relative ? mScale : 1.0);
        const double b = interpolate(pf, pm, f) / (o.relative ? pScale : 1.0);
        c.frequencyHz.push_back(f);
        c.measuredMagnitude.push_back(a);
        c.predictedMagnitude.push_back(b);
        c.residual.push_back(a - b);
        dot += a * b;
        nm += a * a;
        np += b * b;
        if (a > mPeak) {
            mPeak = a;
            mPeakHz = f;
        }
        if (b > pPeak) {
            pPeak = b;
            pPeakHz = f;
        }
        const double um = k < m.standardUncertainty.size() ? m.standardUncertainty[k] / (o.relative ? mScale : 1.0) : -1.0;
        if (um >= 0.0) {
            const double up = p.standardUncertainty.empty() ? 0.0 : interpolate(pf, p.standardUncertainty, f) / (o.relative ? pScale : 1.0);
            ++withUncertainty;
            if (std::abs(a - b) <= o.coverageFactor * std::hypot(um, up))
                ++within;
        }
    }
    if (c.frequencyHz.empty())
        return R::failure("the responses have no frequency in common");
    c.spectralSimilarity = nm > 0 && np > 0 ? dot / std::sqrt(nm * np) : 0.0;
    c.peakOffsetHz = mPeakHz - pPeakHz;
    c.peakOffsetRelative = pPeakHz > 0 ? c.peakOffsetHz / pPeakHz : 0.0;
    c.peakAmplitudeOffsetDb = mPeak > 0 && pPeak > 0 ? 20.0 * std::log10(mPeak / pPeak) : 0.0;
    c.fractionWithinCombined = withUncertainty > 0 ? static_cast<double>(within) / withUncertainty : -1.0;
    c.verdict = c.fractionWithinCombined < 0.0 ? "not assessable (no measurement uncertainty)"
                : c.fractionWithinCombined >= consistencyThreshold(withUncertainty, o.coverageFactor) ? "consistent within the combined uncertainty"
                                                    : "inconsistent: " + std::to_string(static_cast<int>(100 * (1 - c.fractionWithinCombined))) +
                                                         " % of the points lie outside the combined expanded uncertainty";
    return c;
}

json TimeComparison::toJson() const
{
    return {{"points", points},        {"from", from},           {"to", to},           {"rms", rms},
            {"normalizedRms", normalizedRms}, {"maxDeviation", maxDeviation}, {"maxAt", maxAt}, {"correlation", correlation},
            {"shiftSeconds", shiftSeconds}, {"fractionWithin", fractionWithin}, {"times", times}, {"measured", measured},
            {"predicted", predicted},  {"residual", residual}, {"unit", unit},     {"verdict", verdict}, {"notes", notes}};
}

Result<TimeComparison> compareTransients(const std::vector<double>& mt, const std::vector<double>& mv, const std::vector<double>& mu,
                                         const std::vector<double>& pt, const std::vector<double>& pv, double pu, const std::string& unit,
                                         double coverage)
{
    using R = Result<TimeComparison>;
    if (mt.size() != mv.size() || pt.size() != pv.size() || mt.size() < 2 || pt.size() < 2)
        return R::failure("both responses need times and values of the same length (at least 2)");
    for (size_t k = 1; k < pt.size(); ++k)
        if (!(pt[k] > pt[k - 1]))
            return R::failure("the predicted times do not increase");
    TimeComparison c;
    c.unit = unit;
    c.from = std::max(mt.front(), pt.front());
    c.to = std::min(mt.back(), pt.back());
    if (!(c.to > c.from))
        return R::failure("the time ranges do not overlap");
    if (mt.front() < c.from || mt.back() > c.to)
        c.notes.push_back("compared on the overlap only; nothing is extrapolated");
    size_t within = 0, withU = 0;
    double sse = 0, ssm = 0;
    for (size_t k = 0; k < mt.size(); ++k) {
        if (mt[k] < c.from || mt[k] > c.to || !std::isfinite(mv[k]))
            continue;
        const double p = interpolate(pt, pv, mt[k]);
        c.times.push_back(mt[k]);
        c.measured.push_back(mv[k]);
        c.predicted.push_back(p);
        c.residual.push_back(mv[k] - p);
        sse += (mv[k] - p) * (mv[k] - p);
        ssm += mv[k] * mv[k];
        if (std::abs(mv[k] - p) > c.maxDeviation) {
            c.maxDeviation = std::abs(mv[k] - p);
            c.maxAt = mt[k];
        }
        const double u = mu.size() == mt.size() ? mu[k] : mu.size() == 1 ? mu[0] : -1.0;
        if (u >= 0.0) {
            ++withU;
            if (std::abs(mv[k] - p) <= coverage * std::hypot(u, pu))
                ++within;
        }
    }
    c.points = c.times.size();
    if (c.points < 2)
        return R::failure("fewer than two measured points in the overlap");
    c.rms = std::sqrt(sse / c.points);
    const double range = *std::max_element(c.measured.begin(), c.measured.end()) - *std::min_element(c.measured.begin(), c.measured.end());
    c.normalizedRms = range > 0 ? c.rms / range : 0.0;
    c.correlation = pearson(c.measured, c.predicted);
    c.fractionWithin = withU > 0 ? static_cast<double>(within) / withU : -1.0;
    // The time shift of the prediction that best matches (the largest normalized cross-correlation of the deviations from the mean).
    {
        const double span = c.to - c.from;
        double best = -2.0;
        for (int s = -50; s <= 50; ++s) {
            const double shift = span * s / 500.0;
            std::vector<double> a, b;
            for (size_t k = 0; k < c.times.size(); ++k) {
                const double t = c.times[k] - shift;
                if (t < pt.front() || t > pt.back())
                    continue;
                a.push_back(c.measured[k]);
                b.push_back(interpolate(pt, pv, t));
            }
            const double r = a.size() > 2 ? pearson(a, b) : -2.0;
            if (r > best + 1e-12) {
                best = r;
                c.shiftSeconds = shift;
            }
        }
        if (c.shiftSeconds != 0.0)
            c.notes.push_back("the prediction matches best shifted by " + std::to_string(c.shiftSeconds) + " s (reported, not applied)");
    }
    c.verdict = c.fractionWithin < 0.0 ? "not assessable (no measurement uncertainty)"
                : c.fractionWithin >= consistencyThreshold(withU, coverage) ? "consistent within the combined uncertainty"
                                           : "inconsistent: " + std::to_string(static_cast<int>(100 * (1 - c.fractionWithin))) +
                                                " % of the points lie outside the combined expanded uncertainty";
    return c;
}

std::optional<double> FieldGrid::at(double x, double y, double z) const
{
    const double p[3] = {x, y, z};
    int i0[3];
    double t[3];
    for (int a = 0; a < 3; ++a) {
        const double u = (p[a] - origin[static_cast<size_t>(a)]) / spacing[static_cast<size_t>(a)];
        const int n = size[static_cast<size_t>(a)];
        if (n == 1) {
            if (std::abs(u) > 1e-9)
                return std::nullopt;
            i0[a] = 0;
            t[a] = 0.0;
            continue;
        }
        if (u < -1e-9 || u > n - 1 + 1e-9)
            return std::nullopt;
        i0[a] = std::clamp(static_cast<int>(std::floor(u)), 0, n - 2);
        t[a] = std::clamp(u - i0[a], 0.0, 1.0);
    }
    const auto v = [&](int i, int j, int k) {
        return values[static_cast<size_t>((k * size[1] + j) * size[0] + i)];
    };
    double out = 0.0;
    for (int dk = 0; dk <= (size[2] > 1 ? 1 : 0); ++dk)
        for (int dj = 0; dj <= (size[1] > 1 ? 1 : 0); ++dj)
            for (int di = 0; di <= (size[0] > 1 ? 1 : 0); ++di) {
                const double w = (di ? t[0] : 1 - t[0]) * (dj ? t[1] : 1 - t[1]) * (dk ? t[2] : 1 - t[2]);
                out += w * v(i0[0] + di, i0[1] + dj, i0[2] + dk);
            }
    return out;
}

json FieldComparison::toJson() const
{
    json pos = json::array();
    for (const auto& p : positions)
        pos.push_back({p[0], p[1], p[2]});
    return {{"points", points},     {"outside", outside},     {"worst", worst},       {"rms", rms},           {"normalizedRms", normalizedRms},
            {"maxDeviation", maxDeviation}, {"fractionWithin", fractionWithin}, {"positions", pos}, {"measured", measured},
            {"predicted", predicted}, {"residual", residual}, {"frame", frame},       {"unit", unit},         {"verdict", verdict},
            {"notes", notes}};
}

Result<FieldComparison> compareFields(const std::vector<std::array<double, 3>>& pos, const std::vector<double>& mv, const std::vector<double>& mu,
                                      const std::string& measuredFrame, const FieldGrid& g, const std::array<double, 16>& t, double coverage)
{
    using R = Result<FieldComparison>;
    if (pos.size() != mv.size() || pos.empty())
        return R::failure("every measured value needs a position");
    if (g.values.size() != static_cast<size_t>(g.size[0]) * g.size[1] * g.size[2])
        return R::failure("the predicted grid's values do not match its size");
    if (measuredFrame.empty())
        return R::failure("the measurement's frame is not stated: give it (and a transform to the prediction's frame)");
    if (measuredFrame != g.frame && identity(t))
        return R::failure("the measurement is in the frame '" + measuredFrame + "' and the prediction in '" + g.frame +
                          "': give the transform between them");
    FieldComparison c;
    c.frame = g.frame;
    c.unit = g.unit;
    if (!identity(t))
        c.notes.push_back("measured positions transformed from '" + measuredFrame + "' to '" + g.frame + "'");
    c.notes.push_back("the prediction is interpolated trilinearly on its grid at the measured positions");
    double sse = 0;
    size_t within = 0, withU = 0;
    for (size_t k = 0; k < pos.size(); ++k) {
        const auto& p = pos[k];
        const std::array<double, 3> q{t[0] * p[0] + t[1] * p[1] + t[2] * p[2] + t[3], t[4] * p[0] + t[5] * p[1] + t[6] * p[2] + t[7],
                                      t[8] * p[0] + t[9] * p[1] + t[10] * p[2] + t[11]};
        const auto v = g.at(q[0], q[1], q[2]);
        if (!v || !std::isfinite(mv[k])) {
            ++c.outside;
            continue;
        }
        c.positions.push_back(q);
        c.measured.push_back(mv[k]);
        c.predicted.push_back(*v);
        c.residual.push_back(mv[k] - *v);
        sse += (mv[k] - *v) * (mv[k] - *v);
        if (std::abs(mv[k] - *v) > c.maxDeviation) {
            c.maxDeviation = std::abs(mv[k] - *v);
            c.worst = c.measured.size() - 1;
        }
        const double u = mu.size() == pos.size() ? mu[k] : mu.size() == 1 ? mu[0] : -1.0;
        if (u >= 0.0) {
            ++withU;
            if (std::abs(mv[k] - *v) <= coverage * u)
                ++within;
        }
    }
    c.points = c.measured.size();
    if (c.points == 0)
        return R::failure("no measured point lies inside the predicted field");
    if (c.outside > 0)
        c.notes.push_back(std::to_string(c.outside) + " measured points lie outside the predicted field and were not compared");
    c.rms = std::sqrt(sse / c.points);
    const double range = *std::max_element(c.measured.begin(), c.measured.end()) - *std::min_element(c.measured.begin(), c.measured.end());
    c.normalizedRms = range > 0 ? c.rms / range : 0.0;
    c.fractionWithin = withU > 0 ? static_cast<double>(within) / withU : -1.0;
    c.verdict = c.fractionWithin < 0.0 ? "not assessable (no measurement uncertainty)"
                : c.fractionWithin >= consistencyThreshold(withU, coverage) ? "consistent within the measurement uncertainty"
                                           : "inconsistent at " + std::to_string(static_cast<int>(100 * (1 - c.fractionWithin))) + " % of the points";
    return c;
}

}  // namespace resonant::laboratory
