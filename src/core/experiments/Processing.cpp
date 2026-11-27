#include "experiments/Processing.h"

#include "dsp/CrossCorrelation.h"
#include "dsp/Filters.h"
#include "dsp/SpectralEstimation.h"
#include "dsp/Spectrum.h"
#include "foundation/Time.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>
#include <sstream>

namespace resonant::experiments {

namespace {

using Item = ProcessingItem;
using R = Result<Item>;

std::string num(double v)
{
    std::ostringstream out;
    out.precision(6);
    out << v;
    return out.str();
}

bool hasGaps(const Item& item)
{
    if (item.record.kind != MeasurementKind::TimeSeries)
        return false;
    for (double v : item.data.real)
        if (std::isnan(v))
            return true;
    for (const auto& v : item.data.complex)
        if (std::isnan(v.real()))
            return true;
    return false;
}

// The record of a data set derived from `source` by `step`.
MeasurementRecord derived(const std::vector<const Item*>& sources, const ProcessingStep& step, MeasurementKind kind,
                          DataCategory category)
{
    const Item& first = *sources.front();
    MeasurementRecord r = first.record;
    std::vector<std::string> ids;
    for (const Item* s : sources)
        ids.push_back(s->record.id);
    r.id = derivedId(ids, step);
    r.kind = kind;
    r.category = category;
    r.sourceIds = ids;
    r.dataRef.clear();
    r.history = first.record.history;
    r.history.push_back(step);
    r.createdUtc = utcNowIso8601();
    r.simulatedDevice = std::any_of(sources.begin(), sources.end(), [](const Item* s) { return s->record.simulatedDevice; });
    for (size_t i = 1; i < sources.size(); ++i) {
        const DataQuality& q = sources[i]->record.quality;
        r.quality.complete = r.quality.complete && q.complete;
        r.quality.clippedSamples += q.clippedSamples;
        r.quality.saturated = r.quality.saturated || q.saturated;
        r.quality.droppedFrames += q.droppedFrames;
        r.quality.gaps += q.gaps;
    }
    return r;
}

Result<dsp::WelchSettings> welch(const nlohmann::json& p)
{
    dsp::WelchSettings s;
    s.segmentLength = p.value("segmentLength", static_cast<size_t>(4096));
    s.overlap = p.value("overlap", 0.5);
    auto window = dsp::windowKindFromString(p.value("window", std::string("hann")));
    if (!window)
        return Result<dsp::WelchSettings>::failure(window.message());
    s.window = window.value();
    s.removeMean = p.value("removeMean", false);
    return s;
}

Status needsTimeSeries(const Item& item, const std::string& op)
{
    if (item.record.kind != MeasurementKind::TimeSeries)
        return Status::failure(op + " needs a time series; '" + item.record.id + "' is a " + std::string(toString(item.record.kind)));
    if (item.data.isComplex() && op != "segment" && op != "spectrum" && op != "rms" && op != "tone")
        return Status::failure(op + " is defined for real time series; '" + item.record.id + "' holds I/Q samples");
    if (!(item.record.timeBase.sampleRateHz > 0.0))
        return Status::failure("'" + item.record.id + "' has no sample rate");
    return Status::success();
}

R opSegment(const Item& in, ProcessingStep step)
{
    if (Status ok = needsTimeSeries(in, "segment"); !ok)
        return R::failure(ok.message());
    const double fs = in.record.timeBase.sampleRateHz;
    const double start = step.parameters.value("startSeconds", 0.0), duration = step.parameters.value("durationSeconds", 0.0);
    const size_t first = static_cast<size_t>(std::llround(start * fs));
    const size_t count = static_cast<size_t>(std::llround(duration * fs));
    const size_t size = in.data.size();
    if (count == 0 || first + count > size)
        return R::failure("the segment " + num(start) + " s + " + num(duration) + " s does not lie within the record (" +
                          num(static_cast<double>(size) / fs) + " s)");
    step.description = "segment of " + std::to_string(count) + " samples starting at sample " + std::to_string(first) + " (" +
                       num(start) + " s)";
    Item out;
    out.record = derived({&in}, step, MeasurementKind::TimeSeries, DataCategory::Processed);
    out.record.timeBase.startSeconds += static_cast<double>(first) / fs;
    if (in.data.isComplex())
        out.data.complex.assign(in.data.complex.begin() + static_cast<long>(first), in.data.complex.begin() + static_cast<long>(first + count));
    else
        out.data.real.assign(in.data.real.begin() + static_cast<long>(first), in.data.real.begin() + static_cast<long>(first + count));
    out.record.shape = {count};
    return out;
}

R opCalibrate(const Item& in, ProcessingStep step, const ProcessingContext& ctx)
{
    std::string id = step.parameters.value("calibrationId", std::string());
    if (id.empty() && ctx.sensorCalibration.count(in.record.sensorId))
        id = ctx.sensorCalibration.at(in.record.sensorId);
    if (id.empty())
        return R::failure("calibrate: '" + in.record.id + "' has no calibration (name one, or select one for its sensor)");
    if (!ctx.calibrations)
        return R::failure("no calibration library is available");
    const CalibrationRecord* cal = ctx.calibrations->find(id);
    if (!cal)
        return R::failure("calibration '" + id + "' is not in the calibration library");
    if (in.record.category != DataCategory::Raw && in.record.category != DataCategory::Processed)
        return R::failure("'" + in.record.id + "' is " + std::string(toString(in.record.category)) + "; only raw or processed "
                          "instrument readings are calibrated");
    if (in.record.quantity.quantity != Quantity::Voltage && in.record.quantity.quantity != Quantity::DigitalFullScale)
        return R::failure("'" + in.record.id + "' is already in physical units (" + in.record.quantity.unit() + ")");
    auto physical = ctx.sensorQuantity.find(in.record.sensorId);
    if (physical == ctx.sensorQuantity.end())
        return R::failure("'" + in.record.id + "' has no sensor with a known measured quantity; cannot calibrate");
    const std::string expectedUnit = std::string(siUnit(in.record.quantity.quantity)) + "/" + std::string(siUnit(physical->second));
    if (!cal->unit.empty() && cal->unit != expectedUnit)
        return R::failure("calibration '" + id + "' is in " + cal->unit + " but the data needs " + expectedUnit);
    const double gain = ctx.bindingGain.count(in.record.bindingId) ? ctx.bindingGain.at(in.record.bindingId) : 1.0;
    double fromHz = 0.0, toHz = in.record.timeBase.sampleRateHz / 2.0;
    if (!in.data.axis.empty()) {
        fromHz = in.data.axis.front();
        toHz = in.data.axis.back();
    }
    const CalibrationStatus status = checkCalibration(*cal, fromHz, toHz, utcNowIso8601());
    if (!status.usable)
        return R::failure(status.problems.front());
    Item out;
    std::string note;
    for (const auto& w : status.warnings)
        note += (note.empty() ? "" : "; ") + w;
    if (in.record.kind == MeasurementKind::TimeSeries) {
        if (hasGaps(in))
            return R::failure("'" + in.record.id + "' has dropped frames; calibrate a gap-free segment");
        std::vector<double> conditioned(in.data.real.size());
        for (size_t i = 0; i < conditioned.size(); ++i)
            conditioned[i] = in.data.real[i] / gain;
        auto values = applySensitivity(conditioned, *cal);
        if (!values)
            return R::failure(values.message());
        out.data.real = values.take();
        step.description = "divided by the conditioning gain " + num(gain) + " and the scalar sensitivity " + num(cal->scalar) +
                           " " + cal->unit + " of calibration " + id;
        out.record = derived({&in}, step, MeasurementKind::TimeSeries, DataCategory::Calibrated);
    } else if (in.record.kind == MeasurementKind::Spectrum || in.record.kind == MeasurementKind::TransferFunction) {
        std::vector<std::complex<double>> conditioned(in.data.complex.size());
        for (size_t i = 0; i < conditioned.size(); ++i)
            conditioned[i] = in.data.complex[i] / gain;
        auto values = applyResponse(in.data.axis, conditioned, *cal);
        if (!values)
            return R::failure(values.message());
        out.data = in.data;
        out.data.complex = values.take();
        step.description = "divided by the conditioning gain " + num(gain) + " and the " +
                           std::string(cal->frequencyDependent() ? "frequency-dependent complex" : "scalar") +
                           " sensitivity of calibration " + id;
        out.record = derived({&in}, step, in.record.kind, DataCategory::Calibrated);
    } else {
        return R::failure("calibrate applies to time series, spectra and transfer functions");
    }
    if (!note.empty())
        out.record.history.back().description += " (" + note + ")";
    out.record.quantity.quantity = physical->second;
    out.record.calibrationId = id;
    out.record.uncertainty = combineForProduct(in.record.uncertainty, cal->uncertainty);
    out.record.shape = {out.data.size()};
    return out;
}

R opSpectrum(const Item& in, ProcessingStep step)
{
    if (Status ok = needsTimeSeries(in, "spectrum"); !ok)
        return R::failure(ok.message());
    if (hasGaps(in))
        return R::failure("'" + in.record.id + "' has dropped frames (NaN); take a gap-free segment first");
    auto window = dsp::windowKindFromString(step.parameters.value("window", std::string("hann")));
    if (!window)
        return R::failure(window.message());
    const size_t n = in.data.size();
    if (n < 16)
        return R::failure("the record is too short for a spectrum");
    size_t padded = 2;
    while (padded < n)
        padded <<= 1;
    const auto w = dsp::windowCoefficients(window.value(), n);
    const double cg = dsp::windowProperties(w).coherentGain;
    std::vector<std::complex<double>> x(padded, {0.0, 0.0});
    for (size_t i = 0; i < n; ++i)
        x[i] = (in.data.isComplex() ? in.data.complex[i] : std::complex<double>(in.data.real[i], 0.0)) * w[i];
    dsp::fft(x);
    const double fs = in.record.timeBase.sampleRateHz;
    Item out;
    if (in.data.isComplex()) {
        // I/Q: two-sided, -fs/2 ... fs/2 (offsets from the centre frequency).
        for (size_t k = 0; k < padded; ++k) {
            const size_t index = (k + padded / 2) % padded;
            const double f = (static_cast<double>(k) - static_cast<double>(padded / 2)) * fs / static_cast<double>(padded);
            out.data.axis.push_back(f);
            out.data.complex.push_back(x[index] / (static_cast<double>(n) * cg));
        }
    } else {
        for (size_t k = 0; k <= padded / 2; ++k) {
            out.data.axis.push_back(static_cast<double>(k) * fs / static_cast<double>(padded));
            const double scale = (k == 0 || k == padded / 2) ? 1.0 : 2.0;
            out.data.complex.push_back(scale * x[k] / (static_cast<double>(n) * cg));
        }
    }
    step.description = std::string(toString(window.value())) + " window over " + std::to_string(n) + " samples, zero-padded to " +
                       std::to_string(padded) + "; amplitudes scaled so a steady sinusoid A cos(2 pi f t + phi) reads A e^{j phi}; "
                       "resolution " + num(dsp::windowProperties(w).noiseBandwidthBins * fs / static_cast<double>(n)) + " Hz" +
                       (in.data.isComplex() ? "; two-sided (I/Q), frequencies relative to the centre frequency" : "");
    out.record = derived({&in}, step, MeasurementKind::Spectrum, DataCategory::Processed);
    out.record.shape = {out.data.size()};
    return out;
}

R opPsd(const Item& in, ProcessingStep step)
{
    if (Status ok = needsTimeSeries(in, "psd"); !ok)
        return R::failure(ok.message());
    if (hasGaps(in))
        return R::failure("'" + in.record.id + "' has dropped frames (NaN); take a gap-free segment first");
    auto settings = welch(step.parameters);
    if (!settings)
        return R::failure(settings.message());
    auto psd = dsp::welchPsd(in.data.real, in.record.timeBase.sampleRateHz, settings.value());
    if (!psd)
        return R::failure(psd.message());
    Item out;
    out.data.axis = psd.value().grid.frequencyHz;
    out.data.real = psd.value().density;
    step.description = settings.value().describe() + ", " + std::to_string(psd.value().grid.averages) +
                       " averages, one-sided density, resolution " + num(psd.value().grid.resolutionHz) + " Hz";
    out.record = derived({&in}, step, MeasurementKind::PowerSpectrum, DataCategory::Processed);
    out.record.quantity.spectralDensity = true;
    out.record.shape = {out.data.size()};
    return out;
}

R opTransferFunction(const Item& y, const Item& x, ProcessingStep step)
{
    for (const Item* i : {&y, &x}) {
        if (Status ok = needsTimeSeries(*i, "transfer-function"); !ok)
            return R::failure(ok.message());
        if (hasGaps(*i))
            return R::failure("'" + i->record.id + "' has dropped frames (NaN); take a gap-free segment first");
        if (i->record.quantity.per || i->record.quantity.spectralDensity)
            return R::failure("'" + i->record.id + "' is not a plain quantity");
    }
    if (std::abs(x.record.timeBase.sampleRateHz - y.record.timeBase.sampleRateHz) > 1e-9 * x.record.timeBase.sampleRateHz)
        return R::failure("the response and the reference have different sample rates");
    if (x.record.runId != y.record.runId)
        return R::failure("the response and the reference come from different runs: they were not recorded together");
    auto settings = welch(step.parameters);
    if (!settings)
        return R::failure(settings.message());
    auto t = dsp::estimateTransferFunction(x.data.real, y.data.real, x.record.timeBase.sampleRateHz, settings.value());
    if (!t)
        return R::failure(t.message());
    Item out;
    out.data.axis = t.value().grid.frequencyHz;
    out.data.complex = t.value().h1;
    out.data.auxiliary = t.value().coherence;
    step.description = "H1 = G_xy / G_xx with the reference '" + x.record.bindingId + "' as input; " + settings.value().describe() +
                       ", " + std::to_string(t.value().grid.averages) + " averages; the coherence is kept with it";
    out.record = derived({&y, &x}, step, MeasurementKind::TransferFunction, DataCategory::Processed);
    out.record.quantity = {y.record.quantity.quantity, x.record.quantity.quantity, false};
    double worst = 0.0;
    for (size_t k = 0; k < t.value().h1.size(); ++k)
        if (t.value().coherence[k] >= 0.9)
            worst = std::max(worst, t.value().magnitudeRandomError[k]);
    UncertaintyComponent random;
    random.source = UncertaintySource::Noise;
    random.type = UncertaintyType::Statistical;
    random.relative = worst;
    random.description = "largest normalized random error of |H1| where the coherence is at least 0.9";
    out.record.uncertainty = combineForProduct(y.record.uncertainty, x.record.uncertainty);
    out.record.uncertainty.add(random);
    out.record.shape = {out.data.size()};
    return out;
}

R opImpulseResponse(const Item& in, ProcessingStep step)
{
    if (in.record.kind != MeasurementKind::TransferFunction)
        return R::failure("impulse-response needs a transfer function");
    const size_t bins = in.data.complex.size();
    if (bins < 3)
        return R::failure("the transfer function is too short");
    dsp::TransferFunctionEstimate t;
    t.h1 = in.data.complex;
    t.grid.settings.segmentLength = 2 * (bins - 1);
    t.grid.sampleRateHz = in.record.timeBase.sampleRateHz;
    const auto ir = dsp::impulseResponse(t);
    Item out;
    out.data.real = ir.samples;
    step.description = "inverse DFT of H1 over " + std::to_string(t.grid.settings.segmentLength) +
                       " samples (periodic: a response longer than that wraps around)";
    out.record = derived({&in}, step, MeasurementKind::TimeSeries, DataCategory::Processed);
    out.record.shape = {out.data.size()};
    return out;
}

R opFilter(const Item& in, ProcessingStep step)
{
    if (Status ok = needsTimeSeries(in, "filter"); !ok)
        return R::failure(ok.message());
    if (hasGaps(in))
        return R::failure("'" + in.record.id + "' has dropped frames (NaN); take a gap-free segment first");
    const auto& p = step.parameters;
    auto band = dsp::filterBandFromString(p.value("band", std::string("lowpass")));
    auto window = dsp::windowKindFromString(p.value("window", std::string("hamming")));
    if (!band)
        return R::failure(band.message());
    if (!window)
        return R::failure(window.message());
    auto fir = dsp::designFir(band.value(), in.record.timeBase.sampleRateHz, p.value("lowHz", 0.0), p.value("highHz", 0.0),
                              p.value("taps", static_cast<size_t>(101)), window.value());
    if (!fir)
        return R::failure(fir.message());
    const auto filtered = dsp::applyFir(fir.value(), in.data.real, p.value("compensateDelay", true));
    Item out;
    out.data.real = filtered.samples;
    step.description = filtered.description + "; " + std::to_string(filtered.edgeSamples) + " samples at each end are affected "
                                                                                             "by the record's ends";
    out.record = derived({&in}, step, MeasurementKind::TimeSeries, DataCategory::Processed);
    out.record.shape = {out.data.size()};
    return out;
}

R opRate(const Item& in, ProcessingStep step, bool decimation)
{
    if (Status ok = needsTimeSeries(in, step.operation); !ok)
        return R::failure(ok.message());
    if (hasGaps(in))
        return R::failure("'" + in.record.id + "' has dropped frames (NaN); take a gap-free segment first");
    auto result = decimation ? dsp::decimate(in.data.real, in.record.timeBase.sampleRateHz, step.parameters.value("factor", 1))
                             : dsp::resample(in.data.real, in.record.timeBase.sampleRateHz, step.parameters.value("sampleRateHz", 0.0));
    if (!result)
        return R::failure(result.message());
    Item out;
    out.data.real = result.value().samples;
    step.description = result.value().description;
    out.record = derived({&in}, step, MeasurementKind::TimeSeries, DataCategory::Processed);
    out.record.timeBase.sampleRateHz = result.value().sampleRateHz;
    out.record.shape = {out.data.size()};
    return out;
}

R opCorrelation(const Item& y, const Item* x, ProcessingStep step)
{
    for (const Item* i : {&y, x}) {
        if (!i)
            continue;
        if (Status ok = needsTimeSeries(*i, step.operation); !ok)
            return R::failure(ok.message());
        if (hasGaps(*i))
            return R::failure("'" + i->record.id + "' has dropped frames (NaN); take a gap-free segment first");
    }
    const double fs = y.record.timeBase.sampleRateHz;
    const long maxLag = static_cast<long>(std::llround(step.parameters.value("maxLagSeconds", 0.01) * fs));
    auto c = x ? dsp::crossCorrelation(x->data.real, y.data.real, maxLag) : dsp::autoCorrelation(y.data.real, maxLag);
    if (!c)
        return R::failure(c.message());
    Item out;
    for (long lag = -maxLag; lag <= maxLag; ++lag) {
        out.data.axis.push_back(static_cast<double>(lag) / fs);
        out.data.real.push_back(c.value().normalization > 0.0 ? c.value().at(lag) / c.value().normalization : 0.0);
    }
    step.description = std::string(x ? "cross-correlation with '" + x->record.bindingId + "'" : "autocorrelation") + " over +-" +
                       std::to_string(maxLag) + " samples, normalized by sqrt(sum x^2 sum y^2); a positive lag means this "
                       "channel lags";
    std::vector<const Item*> sources{&y};
    if (x)
        sources.push_back(x);
    out.record = derived(sources, step, MeasurementKind::Vector, DataCategory::Processed);
    out.record.quantity = {Quantity::Dimensionless, std::nullopt, false};
    out.record.shape = {out.data.size()};
    return out;
}

R opDelay(const Item& y, const Item& x, ProcessingStep step)
{
    for (const Item* i : {&y, &x}) {
        if (Status ok = needsTimeSeries(*i, "delay"); !ok)
            return R::failure(ok.message());
        if (hasGaps(*i))
            return R::failure("'" + i->record.id + "' has dropped frames (NaN); take a gap-free segment first");
    }
    const double fs = y.record.timeBase.sampleRateHz;
    auto d = dsp::estimateDelay(x.data.real, y.data.real, fs,
                                static_cast<long>(std::llround(step.parameters.value("maxLagSeconds", 0.01) * fs)));
    if (!d)
        return R::failure(d.message());
    Item out;
    out.data.real = {d.value().delaySeconds};
    out.data.labels = {"delay"};
    step.description = "delay relative to '" + x.record.bindingId + "': " + d.value().method;
    out.record = derived({&y, &x}, step, MeasurementKind::Scalar, DataCategory::Processed);
    out.record.quantity = {Quantity::Time, std::nullopt, false};
    out.record.uncertainty = UncertaintyBudget{};
    UncertaintyComponent u;
    u.source = UncertaintySource::Clock;
    u.type = UncertaintyType::Statistical;
    u.absolute = d.value().standardUncertaintySeconds;
    u.description = "phase-slope fit residual";
    out.record.uncertainty.add(u);
    out.record.shape = {1};
    return out;
}

R opRms(const Item& in, ProcessingStep step)
{
    if (Status ok = needsTimeSeries(in, "rms"); !ok)
        return R::failure(ok.message());
    if (hasGaps(in))
        return R::failure("'" + in.record.id + "' has dropped frames (NaN); take a gap-free segment first");
    double sum = 0.0;
    for (double v : in.data.real)
        sum += v * v;
    for (const auto& v : in.data.complex)
        sum += std::norm(v);
    Item out;
    out.data.real = {std::sqrt(sum / static_cast<double>(std::max<size_t>(1, in.data.size())))};
    out.data.labels = {"rms"};
    step.description = "root mean square over " + std::to_string(in.data.size()) + " samples (no window, no filter)";
    out.record = derived({&in}, step, MeasurementKind::Scalar, DataCategory::Processed);
    out.record.shape = {1};
    return out;
}

R opTone(const Item& in, ProcessingStep step)
{
    if (Status ok = needsTimeSeries(in, "tone"); !ok)
        return R::failure(ok.message());
    if (hasGaps(in))
        return R::failure("'" + in.record.id + "' has dropped frames (NaN); take a gap-free segment first");
    const double f = step.parameters.value("frequencyHz", 0.0);
    const double fs = in.record.timeBase.sampleRateHz;
    auto window = dsp::windowKindFromString(step.parameters.value("window", std::string("hann")));
    if (!window)
        return R::failure(window.message());
    Item out;
    out.data.axis = {f};
    if (in.data.isComplex()) {
        // I/Q: the component A e^{j(2 pi f t + phi)} at the offset f (may be negative) from the centre frequency.
        if (!(std::abs(f) < fs / 2.0))
            return R::failure("tone needs an offset within +-fs/2 of the centre frequency");
        const auto w = dsp::windowCoefficients(window.value(), in.data.complex.size());
        const double cg = dsp::windowProperties(w).coherentGain;
        std::complex<double> sum(0.0, 0.0);
        for (size_t n = 0; n < in.data.complex.size(); ++n)
            sum += in.data.complex[n] * w[n] * std::polar(1.0, -2.0 * 3.14159265358979323846 * f * static_cast<double>(n) / fs);
        out.data.complex = {sum / (static_cast<double>(in.data.complex.size()) * cg)};
        step.description = "single-frequency DFT of the I/Q record at " + num(f) + " Hz from the centre frequency, " +
                           std::string(toString(window.value())) + " window; A e^{j phi} of A e^{j(2 pi f t + phi)}";
    } else {
        if (!(f > 0.0) || f >= fs / 2.0)
            return R::failure("tone needs a frequency between 0 and the Nyquist frequency");
        out.data.complex = {dsp::toneAmplitude(in.data.real, fs, f, window.value())};
        step.description = "single-frequency DFT at " + num(f) + " Hz with a " + std::string(toString(window.value())) +
                           " window; A e^{j phi} of A cos(2 pi f t + phi)";
    }
    out.record = derived({&in}, step, MeasurementKind::Spectrum, DataCategory::Processed);
    out.record.shape = {1};
    return out;
}

R opAverage(const std::vector<const Item*>& inputs, ProcessingStep step)
{
    if (inputs.size() < 2)
        return R::failure("average needs at least two data sets");
    const Item& first = *inputs.front();
    for (const Item* i : inputs) {
        if (Status ok = needsTimeSeries(*i, "average"); !ok)
            return R::failure(ok.message());
        if (hasGaps(*i))
            return R::failure("'" + i->record.id + "' has dropped frames (NaN)");
        if (i->data.size() != first.data.size() || !(i->record.quantity == first.record.quantity))
            return R::failure("the data sets to average differ in length or quantity");
    }
    const size_t n = first.data.size();
    const double m = static_cast<double>(inputs.size());
    Item out;
    out.data.real.assign(n, 0.0);
    out.data.auxiliary.assign(n, 0.0);
    for (const Item* i : inputs)
        for (size_t k = 0; k < n; ++k)
            out.data.real[k] += i->data.real[k] / m;
    double meanSquareError = 0.0;
    for (size_t k = 0; k < n; ++k) {
        double var = 0.0;
        for (const Item* i : inputs)
            var += (i->data.real[k] - out.data.real[k]) * (i->data.real[k] - out.data.real[k]);
        var /= (m - 1.0);
        out.data.auxiliary[k] = std::sqrt(var / m);
        meanSquareError += var / m;
    }
    step.description = "sample-by-sample mean of " + std::to_string(inputs.size()) + " repetitions; the standard error of the mean "
                       "is kept per sample";
    out.record = derived(inputs, step, MeasurementKind::TimeSeries, DataCategory::Processed);
    out.record.uncertainty.add(statisticalComponent(std::sqrt(meanSquareError / static_cast<double>(n)), m - 1.0,
                                                    "repeatability: RMS standard error of the mean over the record"));
    out.record.shape = {n};
    return out;
}

R opIntegrate(const Item& in, ProcessingStep step)
{
    if (in.record.kind != MeasurementKind::Spectrum && in.record.kind != MeasurementKind::TransferFunction)
        return R::failure("integrate-frequency needs a spectrum or a transfer function");
    const int times = step.parameters.value("times", 1);
    if (times < 1 || times > 2)
        return R::failure("integrate-frequency integrates once or twice");
    Quantity q = in.record.quantity.quantity;
    for (int i = 0; i < times; ++i) {
        if (q == Quantity::Acceleration)
            q = Quantity::Velocity;
        else if (q == Quantity::Velocity)
            q = Quantity::Displacement;
        else
            return R::failure("integrate-frequency: " + std::string(toString(q)) + " cannot be integrated to a kinematic quantity");
    }
    Item out;
    out.data = in.data;
    for (size_t k = 0; k < out.data.complex.size(); ++k) {
        const double w = 2.0 * 3.14159265358979323846 * out.data.axis[k];
        const std::complex<double> jw(0.0, w);
        out.data.complex[k] = w > 0.0 ? out.data.complex[k] / (times == 1 ? jw : jw * jw) : std::complex<double>(0.0, 0.0);
    }
    step.description = "divided by (j omega)^" + std::to_string(times) +
                       ": exact for the steady-state harmonic response; the DC value is set to 0 (undefined)";
    out.record = derived({&in}, step, in.record.kind, DataCategory::Processed);
    out.record.quantity.quantity = q;
    out.record.shape = {out.data.size()};
    return out;
}

R opScale(const Item& in, ProcessingStep step)
{
    const double factor = step.parameters.value("factor", 0.0);
    const std::string reason = step.parameters.value("reason", std::string());
    auto quantity = quantityFromString(step.parameters.value("quantity", std::string()));
    if (!quantity)
        return R::failure("scale: " + quantity.message());
    if (!std::isfinite(factor) || factor == 0.0)
        return R::failure("scale needs a finite, non-zero factor");
    if (reason.empty())
        return R::failure("scale needs the reason (the model that relates the two quantities)");
    if (in.record.quantity.per || in.record.quantity.spectralDensity)
        return R::failure("scale applies to plain quantities");
    Item out;
    out.data = in.data;
    for (double& v : out.data.real)
        v *= factor;
    for (auto& v : out.data.complex)
        v *= factor;
    step.description = "multiplied by " + num(factor) + ": " + reason + " (an inferred quantity, not a measured one)";
    out.record = derived({&in}, step, in.record.kind, DataCategory::Processed);
    out.record.quantity = {quantity.value(), std::nullopt, false};
    out.record.shape = {out.data.size()};
    return out;
}

bool takesReference(const std::string& op)
{
    return op == "transfer-function" || op == "cross-correlation" || op == "delay";
}

}  // namespace

std::vector<std::string> processingOperations()
{
    return {"segment", "calibrate", "spectrum", "psd", "transfer-function", "impulse-response", "filter", "decimate", "resample",
            "cross-correlation", "autocorrelation", "delay", "rms", "tone", "average", "integrate-frequency", "scale"};
}

std::string derivedId(const std::vector<std::string>& sourceIds, const ProcessingStep& step)
{
    uint64_t h = 1469598103934665603ULL;
    auto mix = [&](const std::string& s) {
        for (unsigned char c : s) {
            h ^= c;
            h *= 1099511628211ULL;
        }
        h ^= 0xff;
        h *= 1099511628211ULL;
    };
    for (const auto& s : sourceIds)
        mix(s);
    mix(step.operation);
    mix(step.parameters.dump());
    char text[24];
    std::snprintf(text, sizeof text, "drv-%016llx", static_cast<unsigned long long>(h));
    return text;
}

Result<ProcessingItem> runStep(const ProcessingStep& step, const std::vector<const ProcessingItem*>& inputs,
                               const ProcessingContext& context)
{
    if (inputs.empty() || !inputs.front())
        return R::failure(step.operation + ": no input");
    const Item& in = *inputs.front();
    const Item* reference = inputs.size() > 1 ? inputs[1] : nullptr;
    const std::string& op = step.operation;
    if (takesReference(op) && !reference)
        return R::failure(op + " needs a reference data set");
    if (op == "segment")
        return opSegment(in, step);
    if (op == "calibrate")
        return opCalibrate(in, step, context);
    if (op == "spectrum")
        return opSpectrum(in, step);
    if (op == "psd")
        return opPsd(in, step);
    if (op == "transfer-function")
        return opTransferFunction(in, *reference, step);
    if (op == "impulse-response")
        return opImpulseResponse(in, step);
    if (op == "filter")
        return opFilter(in, step);
    if (op == "decimate")
        return opRate(in, step, true);
    if (op == "resample")
        return opRate(in, step, false);
    if (op == "cross-correlation")
        return opCorrelation(in, reference, step);
    if (op == "autocorrelation")
        return opCorrelation(in, nullptr, step);
    if (op == "delay")
        return opDelay(in, *reference, step);
    if (op == "rms")
        return opRms(in, step);
    if (op == "tone")
        return opTone(in, step);
    if (op == "average")
        return opAverage(inputs, step);
    if (op == "integrate-frequency")
        return opIntegrate(in, step);
    if (op == "scale")
        return opScale(in, step);
    return R::failure("unknown processing operation '" + op + "'");
}

Result<std::vector<ProcessingItem>> runChain(const std::vector<ProcessingStep>& chain,
                                             const std::map<std::string, ProcessingItem>& rawByBinding,
                                             const ProcessingContext& context)
{
    using RV = Result<std::vector<ProcessingItem>>;
    std::map<std::string, ProcessingItem> latest = rawByBinding;
    std::vector<ProcessingItem> out;
    for (const auto& step : chain) {
        std::vector<std::string> bindings = step.parameters.value("bindings", std::vector<std::string>{});
        if (bindings.empty())
            for (const auto& [id, item] : latest)
                if (item.record.category != DataCategory::Command)  // commanded signals only when named
                    bindings.push_back(id);
        const std::string reference = step.parameters.value("reference", std::string());
        if (takesReference(step.operation)) {
            if (!latest.count(reference))
                return RV::failure(step.operation + ": the reference binding '" + reference + "' was not recorded in this run");
            bindings.erase(std::remove(bindings.begin(), bindings.end(), reference), bindings.end());
        }
        if (step.operation == "average") {
            std::vector<const Item*> inputs;
            for (const auto& b : bindings)
                if (latest.count(b))
                    inputs.push_back(&latest.at(b));
            auto r = runStep(step, inputs, context);
            if (!r)
                return RV::failure(r.message());
            out.push_back(r.value());
            latest[bindings.front()] = r.take();
            continue;
        }
        std::map<std::string, ProcessingItem> next = latest;
        for (const auto& b : bindings) {
            if (!latest.count(b))
                return RV::failure(step.operation + ": the binding '" + b + "' was not recorded in this run");
            std::vector<const Item*> inputs{&latest.at(b)};
            if (takesReference(step.operation))
                inputs.push_back(&latest.at(reference));
            auto r = runStep(step, inputs, context);
            if (!r)
                return RV::failure(step.operation + " on '" + b + "': " + r.message());
            out.push_back(r.value());
            next[b] = r.take();
        }
        latest = std::move(next);
    }
    return out;
}

}  // namespace resonant::experiments
