#include "experiments/Comparison.h"

#include "foundation/Units.h"

#include <algorithm>
#include <cmath>

namespace resonant::experiments {

namespace {

double wrapDeg(double d)
{
    while (d > 180.0)
        d -= 360.0;
    while (d <= -180.0)
        d += 360.0;
    return d;
}

bool uncalibrated(Quantity q) { return q == Quantity::Voltage || q == Quantity::DigitalFullScale; }

// Linear interpolation in magnitude and unwrapped phase.
std::complex<double> interpolate(const std::vector<double>& f, const std::vector<std::complex<double>>& v, double at)
{
    auto upper = std::lower_bound(f.begin(), f.end(), at);
    if (upper == f.begin())
        return v.front();
    if (upper == f.end())
        return v.back();
    const size_t i = static_cast<size_t>(upper - f.begin());
    if (f[i] == at)
        return v[i];
    const double t = (at - f[i - 1]) / (f[i] - f[i - 1]);
    const double m = std::abs(v[i - 1]) + t * (std::abs(v[i]) - std::abs(v[i - 1]));
    double p0 = std::arg(v[i - 1]), p1 = std::arg(v[i]);
    while (p1 - p0 > units::pi)
        p1 -= 2.0 * units::pi;
    while (p1 - p0 < -units::pi)
        p1 += 2.0 * units::pi;
    return std::polar(m, p0 + t * (p1 - p0));
}

double pearson(const std::vector<double>& a, const std::vector<double>& b)
{
    const double n = static_cast<double>(a.size());
    if (a.size() < 2)
        return 0.0;
    double ma = 0, mb = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        ma += a[i] / n;
        mb += b[i] / n;
    }
    double sab = 0, saa = 0, sbb = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        sab += (a[i] - ma) * (b[i] - mb);
        saa += (a[i] - ma) * (a[i] - ma);
        sbb += (b[i] - mb) * (b[i] - mb);
    }
    return (saa > 0 && sbb > 0) ? sab / std::sqrt(saa * sbb) : 0.0;
}

}  // namespace

Status comparable(const QuantitySpec& m, const QuantitySpec& s, bool relative)
{
    if (m == s)
        return Status::success();
    if (relative) {
        // Shape comparison: the measured side may still be an uncalibrated reading of the same structure of quantity.
        const bool sameShape = m.per.has_value() == s.per.has_value() && m.spectralDensity == s.spectralDensity;
        if (sameShape && (uncalibrated(m.quantity) || m.quantity == s.quantity))
            return Status::success();
    }
    return Status::failure("cannot compare " + m.describe() + " with " + s.describe() +
                           (uncalibrated(m.quantity) && !relative
                                ? ": the measurement is not calibrated; calibrate it, or compare shapes only (relative)"
                                : ": the quantities differ"));
}

nlohmann::json toJson(const ComparisonMetrics& c)
{
    return {{"points", c.points},
            {"rangeHz", {c.fromHz, c.toHz}},
            {"rmsMagnitudeErrorDb", c.rmsMagnitudeErrorDb},
            {"maxMagnitudeErrorDb", c.maxMagnitudeErrorDb},
            {"rmsPhaseErrorDeg", c.rmsPhaseErrorDeg},
            {"normalizedMeanSquareError", c.normalizedMeanSquareError},
            {"magnitudeCorrelation", c.magnitudeCorrelation},
            {"peakMeasuredHz", c.peakMeasuredHz},
            {"peakSimulatedHz", c.peakSimulatedHz},
            {"fractionWithinUncertainty", c.fractionWithinUncertainty},
            {"relative", c.relative},
            {"notes", c.notes}};
}

Result<ComparisonMetrics> compareFrequencySeries(const FrequencySeries& measured, const FrequencySeries& simulated,
                                                 const ComparisonOptions& o)
{
    using R = Result<ComparisonMetrics>;
    if (Status ok = comparable(measured.quantity, simulated.quantity, o.relative); !ok)
        return R::failure(ok.message());
    if (measured.frequencyHz.size() != measured.values.size() || simulated.frequencyHz.size() != simulated.values.size())
        return R::failure("frequency and value counts differ");
    if (measured.values.empty() || simulated.values.size() < 2)
        return R::failure("nothing to compare");
    if (!std::is_sorted(simulated.frequencyHz.begin(), simulated.frequencyHz.end()))
        return R::failure("the simulated frequencies are not in increasing order");
    const double lo = std::max(measured.frequencyHz.front(), simulated.frequencyHz.front());
    const double hi = std::min(measured.frequencyHz.back(), simulated.frequencyHz.back());
    ComparisonMetrics c;
    c.relative = o.relative;
    std::vector<std::complex<double>> m, s;
    std::vector<double> u;
    for (size_t i = 0; i < measured.values.size(); ++i) {
        const double f = measured.frequencyHz[i];
        if (f < lo || f > hi)
            continue;
        c.comparedHz.push_back(f);
        m.push_back(measured.values[i]);
        s.push_back(interpolate(simulated.frequencyHz, simulated.values, f));
        u.push_back(measured.standardUncertainty.size() == measured.values.size() ? measured.standardUncertainty[i] : -1.0);
    }
    if (m.empty())
        return R::failure("the measured (" + std::to_string(measured.frequencyHz.front()) + " ... " +
                          std::to_string(measured.frequencyHz.back()) + " Hz) and simulated (" +
                          std::to_string(simulated.frequencyHz.front()) + " ... " + std::to_string(simulated.frequencyHz.back()) +
                          " Hz) ranges do not overlap");
    if (c.comparedHz.size() < measured.values.size())
        c.notes.push_back(std::to_string(measured.values.size() - c.comparedHz.size()) +
                          " measured point(s) outside the simulated range were not compared (no extrapolation)");
    if (o.relative) {
        double mm = 0, sm = 0;
        for (size_t i = 0; i < m.size(); ++i) {
            mm = std::max(mm, std::abs(m[i]));
            sm = std::max(sm, std::abs(s[i]));
        }
        if (mm == 0.0 || sm == 0.0)
            return R::failure("a series is zero everywhere; no shape to compare");
        for (size_t i = 0; i < m.size(); ++i) {
            m[i] /= mm;
            s[i] /= sm;
            if (u[i] >= 0.0)
                u[i] /= mm;
        }
        c.notes.push_back("relative comparison: both normalized to their maximum; absolute levels were not compared");
    }
    double largest = 0.0;
    for (size_t i = 0; i < m.size(); ++i)
        largest = std::max({largest, std::abs(m[i]), std::abs(s[i])});
    double sumDb = 0, sumPhase = 0, sumErr = 0, sumRef = 0;
    size_t dbPoints = 0, phasePoints = 0, withUncertainty = 0, within = 0;
    std::vector<double> am, as;
    for (size_t i = 0; i < m.size(); ++i) {
        const double a = std::abs(m[i]), b = std::abs(s[i]);
        am.push_back(a);
        as.push_back(b);
        sumErr += std::norm(m[i] - s[i]);
        sumRef += std::norm(m[i]);
        if (a > o.minMagnitudeFraction * largest && b > o.minMagnitudeFraction * largest) {
            const double e = 20.0 * std::log10(b / a);
            c.errorDb.push_back(e);
            sumDb += e * e;
            c.maxMagnitudeErrorDb = std::max(c.maxMagnitudeErrorDb, std::abs(e));
            ++dbPoints;
            if (o.comparePhase) {
                const double p = wrapDeg(units::radiansToDegrees(std::arg(s[i]) - std::arg(m[i])));
                sumPhase += p * p;
                ++phasePoints;
            }
        } else {
            c.errorDb.push_back(0.0);
        }
        if (u[i] >= 0.0) {
            ++withUncertainty;
            if (std::abs(a - b) <= o.coverageFactor * u[i])
                ++within;
        }
    }
    c.points = m.size();
    c.fromHz = c.comparedHz.front();
    c.toHz = c.comparedHz.back();
    c.rmsMagnitudeErrorDb = dbPoints ? std::sqrt(sumDb / static_cast<double>(dbPoints)) : 0.0;
    c.rmsPhaseErrorDeg = phasePoints ? std::sqrt(sumPhase / static_cast<double>(phasePoints)) : 0.0;
    c.normalizedMeanSquareError = sumRef > 0.0 ? sumErr / sumRef : 0.0;
    c.magnitudeCorrelation = pearson(am, as);
    c.peakMeasuredHz = c.comparedHz[static_cast<size_t>(std::max_element(am.begin(), am.end()) - am.begin())];
    c.peakSimulatedHz = c.comparedHz[static_cast<size_t>(std::max_element(as.begin(), as.end()) - as.begin())];
    if (withUncertainty == m.size())
        c.fractionWithinUncertainty = static_cast<double>(within) / static_cast<double>(m.size());
    else
        c.notes.push_back("the measurement states no per-point uncertainty; agreement within uncertainty was not assessed");
    if (dbPoints < m.size())
        c.notes.push_back(std::to_string(m.size() - dbPoints) + " point(s) near zero were left out of the dB and phase metrics");
    if (!simulated.model.empty())
        c.notes.push_back("simulated by: " + simulated.model);
    return c;
}

nlohmann::json toJson(const SpatialComparison& c)
{
    return {{"points", c.points},
            {"rmsMagnitudeErrorDb", c.rmsMagnitudeErrorDb},
            {"maxMagnitudeErrorDb", c.maxMagnitudeErrorDb},
            {"rmsPhaseErrorDeg", c.rmsPhaseErrorDeg},
            {"normalizedMeanSquareError", c.normalizedMeanSquareError},
            {"fractionWithinUncertainty", c.fractionWithinUncertainty},
            {"worstPoint", c.worstPoint},
            {"relative", c.relative},
            {"errorDb", c.errorDb},
            {"notes", c.notes}};
}

Result<SpatialComparison> compareSpatial(const SpatialDataset& measured, const FieldSampler& simulated,
                                         const QuantitySpec& simulatedQuantity, const SpatialComparisonOptions& o)
{
    using R = Result<SpatialComparison>;
    if (Status ok = comparable(measured.quantity, simulatedQuantity, o.relative); !ok)
        return R::failure(ok.message());
    if (measured.samples.empty())
        return R::failure("the spatial data set has no samples");
    if (!simulated)
        return R::failure("no simulated field to compare with");
    bool identity = true;
    for (int i = 0; i < 16; ++i)
        identity = identity && o.transform[i] == ((i % 5 == 0) ? 1.0 : 0.0);
    if (measured.frame != o.simulationFrame && identity)
        return R::failure("the measurements are in frame '" + measured.frame + "', the simulation in '" + o.simulationFrame +
                          "': give the transform between them");
    SpatialComparison c;
    c.relative = o.relative;
    std::vector<std::complex<double>> m;
    std::vector<double> u;
    for (const auto& s : measured.samples) {
        const double* t = o.transform;
        const double x = t[0] * s.pose.x + t[1] * s.pose.y + t[2] * s.pose.z + t[3];
        const double y = t[4] * s.pose.x + t[5] * s.pose.y + t[6] * s.pose.z + t[7];
        const double z = t[8] * s.pose.x + t[9] * s.pose.y + t[10] * s.pose.z + t[11];
        auto v = simulated(x, y, z);
        if (!v)
            return R::failure("simulated field at (" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) +
                              "): " + v.message());
        c.simulated.push_back(v.value());
        m.push_back(s.value);
        u.push_back(s.standardUncertainty);
    }
    std::vector<std::complex<double>> sim = c.simulated;
    if (o.relative) {
        double mm = 0, sm = 0;
        for (size_t i = 0; i < m.size(); ++i) {
            mm = std::max(mm, std::abs(m[i]));
            sm = std::max(sm, std::abs(sim[i]));
        }
        if (mm == 0.0 || sm == 0.0)
            return R::failure("a field is zero at every point; no shape to compare");
        // Phases are compared relative to the strongest measured point.
        const size_t ref = static_cast<size_t>(std::max_element(m.begin(), m.end(), [](auto a, auto b) { return std::abs(a) < std::abs(b); }) -
                                               m.begin());
        const std::complex<double> pm = std::polar(1.0, -std::arg(m[ref])), ps = std::polar(1.0, -std::arg(sim[ref]));
        for (size_t i = 0; i < m.size(); ++i) {
            m[i] = m[i] / mm * pm;
            sim[i] = sim[i] / sm * ps;
            u[i] /= mm;
        }
        c.notes.push_back("relative comparison: both normalized to their maximum and phase-referenced to the strongest measured "
                          "point; absolute levels and absolute phase were not compared");
    }
    double sumDb = 0, sumPhase = 0, sumErr = 0, sumRef = 0, worst = -1;
    size_t within = 0, withU = 0;
    for (size_t i = 0; i < m.size(); ++i) {
        const double a = std::abs(m[i]), b = std::abs(sim[i]);
        const double e = (a > 0 && b > 0) ? 20.0 * std::log10(b / a) : 0.0;
        c.errorDb.push_back(e);
        sumDb += e * e;
        if (std::abs(e) > worst) {
            worst = std::abs(e);
            c.worstPoint = i;
        }
        c.maxMagnitudeErrorDb = std::max(c.maxMagnitudeErrorDb, std::abs(e));
        if (o.comparePhase) {
            const double p = wrapDeg(units::radiansToDegrees(std::arg(sim[i]) - std::arg(m[i])));
            sumPhase += p * p;
        }
        sumErr += std::norm(m[i] - sim[i]);
        sumRef += std::norm(m[i]);
        if (u[i] > 0.0) {
            ++withU;
            if (std::abs(a - b) <= o.coverageFactor * u[i])
                ++within;
        }
    }
    const double n = static_cast<double>(m.size());
    c.points = m.size();
    c.rmsMagnitudeErrorDb = std::sqrt(sumDb / n);
    c.rmsPhaseErrorDeg = o.comparePhase ? std::sqrt(sumPhase / n) : 0.0;
    c.normalizedMeanSquareError = sumRef > 0 ? sumErr / sumRef : 0.0;
    if (withU == m.size())
        c.fractionWithinUncertainty = static_cast<double>(within) / n;
    else
        c.notes.push_back("not every sample states an uncertainty; agreement within uncertainty was not assessed");
    c.notes.push_back("compared at the " + std::to_string(m.size()) + " measured positions only; this says nothing about the field "
                      "between them");
    return c;
}

}  // namespace resonant::experiments
