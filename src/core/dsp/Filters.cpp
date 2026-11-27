#include "dsp/Filters.h"

#include "foundation/Units.h"

#include <cmath>
#include <sstream>

namespace resonant::dsp {

namespace {

double sinc(double x) { return std::abs(x) < 1e-12 ? 1.0 : std::sin(units::pi * x) / (units::pi * x); }

// Symmetric window of length n (the periodic window of n - 1 points, closed).
std::vector<double> symmetricWindow(WindowKind kind, size_t n)
{
    if (n < 2)
        return std::vector<double>(n, 1.0);
    std::vector<double> w = windowCoefficients(kind, n - 1);
    w.push_back(w.front());
    return w;
}

// Windowed-sinc low-pass of unity DC gain.
std::vector<double> lowPassTaps(double fs, double cutoff, const std::vector<double>& w)
{
    const size_t n = w.size();
    const double mid = static_cast<double>(n - 1) / 2.0;
    const double fc = 2.0 * cutoff / fs;
    std::vector<double> h(n);
    double sum = 0.0;
    for (size_t i = 0; i < n; ++i) {
        h[i] = fc * sinc(fc * (static_cast<double>(i) - mid)) * w[i];
        sum += h[i];
    }
    for (double& v : h)
        v /= sum;
    return h;
}

}  // namespace

std::string_view toString(FilterBand band)
{
    switch (band) {
    case FilterBand::LowPass: return "lowpass";
    case FilterBand::HighPass: return "highpass";
    case FilterBand::BandPass: return "bandpass";
    case FilterBand::BandStop: return "bandstop";
    }
    return "lowpass";
}

Result<FilterBand> filterBandFromString(std::string_view text)
{
    for (FilterBand b : {FilterBand::LowPass, FilterBand::HighPass, FilterBand::BandPass, FilterBand::BandStop})
        if (text == toString(b))
            return b;
    return Result<FilterBand>::failure("unknown filter band '" + std::string(text) + "' (lowpass, highpass, bandpass, bandstop)");
}

std::string FirFilter::describe() const
{
    std::ostringstream out;
    out << taps.size() << "-tap linear-phase FIR " << toString(band) << " (windowed sinc, " << toString(window) << "), ";
    if (band == FilterBand::LowPass || band == FilterBand::HighPass)
        out << "cut-off " << lowHz << " Hz";
    else
        out << lowHz << " ... " << highHz << " Hz";
    out << " at " << sampleRateHz << " Hz, group delay " << groupDelaySamples() << " samples";
    return out.str();
}

Result<FirFilter> designFir(FilterBand band, double fs, double lowHz, double highHz, size_t numTaps, WindowKind window)
{
    if (!(fs > 0.0))
        return Result<FirFilter>::failure("the sample rate must be positive");
    if (numTaps < 3 || numTaps % 2 == 0)
        return Result<FirFilter>::failure("the number of taps must be odd and at least 3");
    const double nyquist = fs / 2.0;
    const bool twoEdges = band == FilterBand::BandPass || band == FilterBand::BandStop;
    if (!(lowHz > 0.0 && lowHz < nyquist))
        return Result<FirFilter>::failure("the cut-off must lie between 0 and the Nyquist frequency (" + std::to_string(nyquist) + " Hz)");
    if (twoEdges && !(highHz > lowHz && highHz < nyquist))
        return Result<FirFilter>::failure("the upper edge must lie between the lower edge and the Nyquist frequency");
    const std::vector<double> w = symmetricWindow(window, numTaps);
    FirFilter f;
    f.band = band;
    f.sampleRateHz = fs;
    f.lowHz = lowHz;
    f.highHz = twoEdges ? highHz : 0.0;
    f.window = window;
    const size_t mid = (numTaps - 1) / 2;
    switch (band) {
    case FilterBand::LowPass: f.taps = lowPassTaps(fs, lowHz, w); break;
    case FilterBand::HighPass:
        f.taps = lowPassTaps(fs, lowHz, w);
        for (double& v : f.taps)
            v = -v;
        f.taps[mid] += 1.0;
        break;
    case FilterBand::BandPass: {
        f.taps = lowPassTaps(fs, highHz, w);
        const auto low = lowPassTaps(fs, lowHz, w);
        for (size_t i = 0; i < numTaps; ++i)
            f.taps[i] -= low[i];
        break;
    }
    case FilterBand::BandStop: {
        const auto high = lowPassTaps(fs, highHz, w);
        const auto low = lowPassTaps(fs, lowHz, w);
        f.taps.resize(numTaps);
        for (size_t i = 0; i < numTaps; ++i)
            f.taps[i] = low[i] - high[i];
        f.taps[mid] += 1.0;
        break;
    }
    }
    return f;
}

FilteredRecord applyFir(const FirFilter& filter, const std::vector<double>& x, bool compensateDelay)
{
    FilteredRecord out;
    const size_t delay = compensateDelay ? filter.groupDelaySamples() : 0;
    const size_t taps = filter.taps.size();
    out.samples.assign(x.size(), 0.0);
    for (size_t n = 0; n < x.size(); ++n) {
        const size_t at = n + delay;  // output index in the causal filter's time
        double sum = 0.0;
        for (size_t k = 0; k < taps; ++k) {
            if (k > at)
                break;
            const size_t i = at - k;
            if (i < x.size())
                sum += filter.taps[k] * x[i];
        }
        out.samples[n] = sum;
    }
    out.edgeSamples = filter.groupDelaySamples();
    out.description = filter.describe() + (compensateDelay ? "; output shifted by the group delay" : "; output delayed by the group delay");
    return out;
}

Result<DecimatedRecord> decimate(const std::vector<double>& x, double fs, int factor)
{
    if (factor < 1)
        return Result<DecimatedRecord>::failure("the decimation factor must be at least 1");
    DecimatedRecord out;
    out.sampleRateHz = fs / factor;
    if (factor == 1) {
        out.samples = x;
        out.description = "decimation by 1 (unchanged)";
        return out;
    }
    auto filter = designFir(FilterBand::LowPass, fs, 0.8 * out.sampleRateHz / 2.0, 0.0, static_cast<size_t>(20 * factor + 1));
    if (!filter)
        return Result<DecimatedRecord>::failure(filter.message());
    const FilteredRecord filtered = applyFir(filter.value(), x, true);
    for (size_t i = 0; i < filtered.samples.size(); i += static_cast<size_t>(factor))
        out.samples.push_back(filtered.samples[i]);
    out.description = "decimation by " + std::to_string(factor) + " after a " + filter.value().describe();
    return out;
}

Result<DecimatedRecord> resample(const std::vector<double>& x, double fs, double newFs, int halfWidth)
{
    if (!(fs > 0.0) || !(newFs > 0.0))
        return Result<DecimatedRecord>::failure("the sample rates must be positive");
    if (halfWidth < 2)
        return Result<DecimatedRecord>::failure("the interpolation kernel needs at least 2 zero crossings on each side");
    if (x.empty())
        return Result<DecimatedRecord>::failure("cannot resample an empty record");
    const double cutoff = 0.9 * std::min(fs, newFs) / 2.0;
    const double fc = 2.0 * cutoff / fs;  // normalized to the input rate
    const double support = static_cast<double>(halfWidth) / fc;  // in input samples
    DecimatedRecord out;
    out.sampleRateHz = newFs;
    const double duration = static_cast<double>(x.size() - 1) / fs;
    const size_t count = static_cast<size_t>(std::floor(duration * newFs)) + 1;
    out.samples.resize(count);
    for (size_t j = 0; j < count; ++j) {
        const double position = static_cast<double>(j) * fs / newFs;  // in input samples
        const long first = static_cast<long>(std::ceil(position - support));
        const long last = static_cast<long>(std::floor(position + support));
        double sum = 0.0;
        for (long i = std::max(0L, first); i <= std::min(static_cast<long>(x.size()) - 1, last); ++i) {
            const double d = position - static_cast<double>(i);
            const double u = d / support;
            const double w = 0.5 * (1.0 + std::cos(units::pi * u));
            sum += x[static_cast<size_t>(i)] * fc * sinc(fc * d) * w;
        }
        out.samples[j] = sum;
    }
    std::ostringstream text;
    text << "band-limited resampling " << fs << " Hz -> " << newFs << " Hz (windowed sinc, " << halfWidth
         << " zero crossings each side, cut-off " << cutoff << " Hz)";
    out.description = text.str();
    return out;
}

}  // namespace resonant::dsp
