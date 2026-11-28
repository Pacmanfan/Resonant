#include "laboratory/ExperimentalResonance.h"

#include "dsp/Resonance.h"

#include <algorithm>
#include <cmath>

namespace resonant::laboratory {

using nlohmann::json;

namespace {

constexpr double kPi = 3.14159265358979323846;

std::string number(double v)
{
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.4g", v);
    return buf;
}

double unwrappedPhaseAt(const std::vector<double>& f, const std::vector<double>& phase, double at)
{
    const auto it = std::lower_bound(f.begin(), f.end(), at);
    if (it == f.begin())
        return phase.front();
    if (it == f.end())
        return phase.back();
    const size_t k = static_cast<size_t>(it - f.begin());
    const double t = (at - f[k - 1]) / (f[k] - f[k - 1]);
    return phase[k - 1] + t * (phase[k] - phase[k - 1]);
}

}  // namespace

json ResonanceCandidate::toJson() const
{
    return {{"frequencyHz", frequencyHz}, {"frequencyUncertainty", frequencyUncertainty}, {"quality", quality}, {"qualityUncertainty", qualityUncertainty},
            {"dampingRatio", dampingRatio}, {"bandwidthHz", bandwidthHz}, {"amplitude", amplitude}, {"snrDb", snrDb},
            {"phaseChangeDeg", phaseKnown ? json(phaseChangeDeg) : json(nullptr)}, {"classification", classification}, {"method", method},
            {"evidence", evidence}, {"predictedIndex", predictedIndex}, {"predictedHz", predictedHz}};
}

Result<std::vector<ResonanceCandidate>> resonancesFromResponse(const std::vector<double>& f, const std::vector<std::complex<double>>& H, bool phaseMeasured,
                                                               const ResonanceOptions& o)
{
    using R = Result<std::vector<ResonanceCandidate>>;
    if (f.size() != H.size() || f.size() < 8)
        return R::failure("a frequency response of at least 8 points");
    std::vector<double> power, mag, phase;
    for (const auto& h : H) {
        power.push_back(std::norm(h));
        mag.push_back(std::abs(h));
        phase.push_back(std::arg(h));
    }
    for (size_t k = 1; k < phase.size(); ++k)  // unwrap
        while (phase[k] - phase[k - 1] > kPi)
            phase[k] -= 2 * kPi;
    for (size_t k = 1; k < phase.size(); ++k)
        while (phase[k] - phase[k - 1] < -kPi)
            phase[k] += 2 * kPi;
    std::vector<double> sorted = mag;
    std::nth_element(sorted.begin(), sorted.begin() + sorted.size() / 2, sorted.end());
    const double floor = std::max(sorted[sorted.size() / 2], 1e-300);
    std::vector<std::string> warnings;
    const auto fits = dsp::fitResonancePeaks(f, power, o.maxPeaks, &warnings);
    if (fits.empty())
        return R::failure("no peak could be fitted" + (warnings.empty() ? std::string() : ": " + warnings.front()));
    std::vector<ResonanceCandidate> out;
    const double resolution = (f.back() - f.front()) / (f.size() - 1);
    for (const dsp::LorentzianFit& fit : fits) {
        ResonanceCandidate c;
        c.method = "Lorentzian fit of |H|^2";
        c.frequencyHz = fit.centerHz;
        c.frequencyUncertainty = fit.centerUncertainty;
        c.quality = fit.quality;
        c.qualityUncertainty = fit.qualityUncertainty;
        c.dampingRatio = fit.quality > 0 ? 1.0 / (2.0 * fit.quality) : 0.0;
        c.bandwidthHz = 2.0 * fit.halfWidthHz;
        c.amplitude = std::sqrt(std::max(fit.amplitude + fit.baseline, 0.0));
        c.snrDb = 20.0 * std::log10(std::max(c.amplitude, 1e-300) / floor);
        c.evidence.push_back("peak at " + number(c.frequencyHz) + " Hz, Q " + number(c.quality) + ", " + number(c.snrDb) + " dB above the median level");
        if (fit.overlapping)
            c.evidence.push_back("another peak lies within a few bandwidths (the fit may be biased)");
        if (fit.pointsInLinewidth < 8)
            c.evidence.push_back("only " + std::to_string(fit.pointsInLinewidth) + " points within the linewidth");
        if (phaseMeasured) {
            c.phaseKnown = true;
            c.phaseChangeDeg = std::abs(unwrappedPhaseAt(f, phase, c.frequencyHz + 2 * fit.halfWidthHz) -
                                        unwrappedPhaseAt(f, phase, c.frequencyHz - 2 * fit.halfWidthHz)) *
                               180.0 / kPi;
        }
        // Classification, the most specific evidence first.
        bool reference = false;
        for (double r : o.referencePeaksHz)
            reference = reference || std::abs(r - c.frequencyHz) <= std::max(2.0 * c.bandwidthHz, 2.0 * resolution);
        bool mains = false;
        if (o.mainsHz > 0.0) {
            const double m = std::round(c.frequencyHz / o.mainsHz);
            mains = m >= 1 && std::abs(c.frequencyHz - m * o.mainsHz) <= std::max(0.5, c.bandwidthHz) && c.quality > 100.0;
        }
        if (c.snrDb < o.minSnrDb) {
            c.classification = "artifact";
            c.evidence.push_back("below the " + number(o.minSnrDb) + " dB signal-to-noise margin");
        } else if (reference) {
            c.classification = "excitation or instrument peak";
            c.evidence.push_back("the reference channel peaks at the same frequency: the excitation or the instrument, not the structure");
        } else if (mains) {
            c.classification = "possible interference";
            c.evidence.push_back("narrow, at a multiple of the " + number(o.mainsHz) + " Hz mains frequency");
        } else if (c.phaseKnown && c.phaseChangeDeg >= 90.0) {
            c.classification = "natural mode";
            c.evidence.push_back("the phase turns by " + number(c.phaseChangeDeg) + " degrees across the peak, as a resonance does");
        } else if (c.phaseKnown) {
            c.classification = "driven-response peak";
            c.evidence.push_back("the phase turns by only " + number(c.phaseChangeDeg) + " degrees: not a resonance of the structure");
        } else {
            c.classification = "unclassified";
            c.evidence.push_back("the phase is not measured: a resonance cannot be told from a forced peak");
        }
        for (size_t k = 0; k < o.predictedHz.size(); ++k)
            if (std::abs(o.predictedHz[k] - c.frequencyHz) <= o.matchTolerance * o.predictedHz[k] &&
                (c.predictedIndex < 0 || std::abs(o.predictedHz[k] - c.frequencyHz) < std::abs(c.predictedHz - c.frequencyHz))) {
                c.predictedIndex = static_cast<int>(k);
                c.predictedHz = o.predictedHz[k];
            }
        if (c.predictedIndex >= 0)
            c.evidence.push_back("within " + number(100 * o.matchTolerance) + " % of the predicted mode at " + number(c.predictedHz) + " Hz (" +
                                 number(100 * (c.frequencyHz - c.predictedHz) / c.predictedHz) + " %)");
        out.push_back(c);
    }
    std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.frequencyHz < b.frequencyHz; });
    return out;
}

json RingDown::toJson() const
{
    return {{"frequencyHz", frequencyHz}, {"frequencyUncertainty", frequencyUncertainty}, {"decayRate", decayRate}, {"quality", quality},
            {"qualityUncertainty", qualityUncertainty}, {"dampingRatio", dampingRatio}, {"energyDecayRate", energyDecayRate},
            {"frequencyDriftHz", frequencyDriftHz}, {"relativeResidual", relativeResidual}, {"method", method}, {"assumptions", assumptions},
            {"warnings", warnings}};
}

Result<RingDown> ringDown(const std::vector<double>& x, double dt, double minHz, double maxHz)
{
    using R = Result<RingDown>;
    dsp::PencilOptions po;
    po.minHz = minHz;
    if (maxHz > 0.0)
        po.maxHz = maxHz;
    po.maxModes = 4;
    auto id = dsp::identifyByMatrixPencil(x, dt, po);
    if (!id)
        return R::failure(id.message());
    if (id.value().resonances.empty())
        return R::failure("no decaying oscillation was found");
    const auto dominant = *std::max_element(id.value().resonances.begin(), id.value().resonances.end(),
                                            [](const auto& a, const auto& b) { return a.amplitude < b.amplitude; });
    RingDown d;
    d.frequencyHz = dominant.frequencyHz;
    d.frequencyUncertainty = dominant.frequencyUncertainty;
    d.decayRate = dominant.decayRate;
    d.quality = dominant.quality;
    d.qualityUncertainty = dominant.qualityUncertainty;
    d.dampingRatio = d.quality > 0 ? 1.0 / (2.0 * d.quality) : 0.0;
    d.energyDecayRate = 2.0 * d.decayRate;
    d.relativeResidual = id.value().relativeResidual;
    d.warnings = id.value().warnings;
    d.assumptions = {"a free decay (no excitation during the record)", "linear and lightly damped: an exponentially decaying sinusoid",
                     "the stored energy is taken as proportional to the squared amplitude: a model-derived rate, not a measured energy"};
    if (id.value().resonances.size() > 1)
        d.warnings.push_back(std::to_string(id.value().resonances.size()) + " components found; the strongest is reported");
    // Frequency stability: the two halves separately.
    const size_t half = x.size() / 2;
    auto a = dsp::identifyByMatrixPencil(std::vector<double>(x.begin(), x.begin() + static_cast<long>(half)), dt, po);
    auto b = dsp::identifyByMatrixPencil(std::vector<double>(x.begin() + static_cast<long>(half), x.end()), dt, po);
    const auto nearest = [&](const dsp::Identification& i) {
        double best = 0.0;
        for (const auto& r : i.resonances)
            if (best == 0.0 || std::abs(r.frequencyHz - d.frequencyHz) < std::abs(best - d.frequencyHz))
                best = r.frequencyHz;
        return best;
    };
    if (a && b && !a.value().resonances.empty() && !b.value().resonances.empty())
        d.frequencyDriftHz = nearest(b.value()) - nearest(a.value());
    else
        d.warnings.push_back("the halves of the record are too short to check the frequency's stability");
    if (d.relativeResidual > 0.1)
        d.warnings.push_back("the decay model leaves " + number(100 * d.relativeResidual) + " % of the signal unexplained (nonlinear, beating or noisy)");
    return d;
}

}  // namespace resonant::laboratory
