#include "laboratory/LabImport.h"

#include "foundation/Dimensions.h"
#include "foundation/Time.h"
#include "laboratory/Sha256.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <sstream>

namespace resonant::laboratory {

using nlohmann::json;
namespace ex = experiments;

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kPi = 3.14159265358979323846;

// Text from a file as valid UTF-8 (invalid bytes replaced), so that records can always be written as JSON.
std::string utf8(const std::string& s)
{
    return json::parse(json(s).dump(-1, ' ', false, json::error_handler_t::replace)).get<std::string>();
}

std::string trim(std::string s)
{
    const auto b = s.find_first_not_of(" \t\r\"");
    const auto e = s.find_last_not_of(" \t\r\"");
    return b == std::string::npos ? std::string() : s.substr(b, e - b + 1);
}

std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

struct Column {
    std::string name, unit, raw;
    bool hasUnit = false;
};

// "name [unit]" or "name (unit)"
Column parseHeader(const std::string& cell)
{
    Column c;
    c.raw = utf8(trim(cell));
    const auto open = c.raw.find_first_of("[(");
    const auto close = c.raw.find_last_of("])");
    if (open != std::string::npos && close != std::string::npos && close > open) {
        c.name = trim(c.raw.substr(0, open));
        c.unit = trim(c.raw.substr(open + 1, close - open - 1));
        c.hasUnit = true;
    } else {
        c.name = c.raw;
    }
    return c;
}

std::vector<std::string> split(const std::string& line, char sep)
{
    std::vector<std::string> out;
    std::string cell;
    bool quoted = false;
    for (char ch : line) {
        if (ch == '"')
            quoted = !quoted;
        else if (ch == sep && !quoted) {
            out.push_back(cell);
            cell.clear();
            continue;
        }
        cell.push_back(ch);
    }
    out.push_back(cell);
    return out;
}

// The quantity of a unit: a single quantity, or one per another.
struct UnitMatch {
    bool ok = false;
    ex::QuantitySpec spec;
    double factor = 1.0, offset = 0.0;  // SI = factor * value + offset
    std::string note;
};

std::optional<ex::Quantity> byName(const std::string& name)
{
    const std::string n = lower(name);
    const std::vector<std::pair<const char*, ex::Quantity>> words = {
        {"pressure", ex::Quantity::Pressure},     {"stress", ex::Quantity::Stress},         {"strain", ex::Quantity::Strain},
        {"accel", ex::Quantity::Acceleration},    {"velocity", ex::Quantity::Velocity},     {"displacement", ex::Quantity::Displacement},
        {"force", ex::Quantity::Force},           {"voltage", ex::Quantity::Voltage},       {"power", ex::Quantity::Power},
        {"temperature", ex::Quantity::Temperature}, {"impedance", ex::Quantity::Impedance}, {"s11", ex::Quantity::ScatteringParameter},
        {"s21", ex::Quantity::ScatteringParameter}, {"reflection", ex::Quantity::ScatteringParameter},
        {"electric", ex::Quantity::ElectricField}, {"magnetic", ex::Quantity::MagneticField}};
    for (const auto& [w, q] : words)
        if (n.find(w) != std::string::npos)
            return q;
    return std::nullopt;
}

UnitMatch matchUnit(const std::string& unit, const std::string& columnName, const std::string& stated)
{
    UnitMatch m;
    if (unit == "degC" || unit == "\xC2\xB0" "C" || (unit == "C" && lower(columnName).find("temp") != std::string::npos)) {
        m.ok = true;
        m.spec.quantity = ex::Quantity::Temperature;
        m.offset = 273.15;
        m.note = "converted from degC to K by adding 273.15";
        return m;
    }
    auto u = parseUnit(unit);
    if (!u)
        return m;
    m.factor = u.value().factor;
    const Dimension d = u.value().dimension;
    const auto dimOf = [](ex::Quantity q) { return parseUnit(ex::siUnit(q)).value().dimension; };
    const std::vector<ex::Quantity> all = {
        ex::Quantity::Voltage,       ex::Quantity::Pressure,      ex::Quantity::Acceleration, ex::Quantity::Velocity,
        ex::Quantity::Displacement,  ex::Quantity::Force,         ex::Quantity::Strain,       ex::Quantity::Stress,
        ex::Quantity::Power,         ex::Quantity::ElectricField, ex::Quantity::MagneticField, ex::Quantity::Impedance,
        ex::Quantity::Temperature,   ex::Quantity::Angle,         ex::Quantity::Time,         ex::Quantity::Frequency,
        ex::Quantity::ScatteringParameter, ex::Quantity::Dimensionless};
    // A quantity stated in the metadata, or named by the column, wins when its dimension agrees.
    std::optional<ex::Quantity> wanted;
    if (!stated.empty())
        if (auto q = ex::quantityFromString(stated))
            wanted = q.value();
    if (!wanted)
        wanted = byName(columnName);
    if (wanted && dimOf(*wanted) == d) {
        m.ok = true;
        m.spec.quantity = *wanted;
        return m;
    }
    for (ex::Quantity q : all)
        if (dimOf(q) == d) {
            m.ok = true;
            m.spec.quantity = q;
            m.note = "the quantity " + std::string(ex::toString(q)) + " was taken from the unit " + unit + " (state it in the metadata to be explicit)";
            return m;
        }
    // A ratio: response per excitation (preferred pairs first).
    const std::vector<std::pair<ex::Quantity, ex::Quantity>> ratios = {
        {ex::Quantity::Displacement, ex::Quantity::Force}, {ex::Quantity::Velocity, ex::Quantity::Force},
        {ex::Quantity::Acceleration, ex::Quantity::Force}, {ex::Quantity::Pressure, ex::Quantity::Voltage},
        {ex::Quantity::Voltage, ex::Quantity::Pressure},   {ex::Quantity::Voltage, ex::Quantity::Acceleration},
        {ex::Quantity::Pressure, ex::Quantity::Force}};
    for (const auto& [a, b] : ratios)
        if (dimOf(a) / dimOf(b) == d) {
            m.ok = true;
            m.spec.quantity = a;
            m.spec.per = b;
            return m;
        }
    return m;
}

std::optional<double> number(const std::string& cell)
{
    const std::string t = trim(cell);
    if (t.empty())
        return std::nullopt;
    const std::string l = lower(t);
    if (l == "nan")
        return kNaN;
    if (l == "inf" || l == "+inf" || l == "infinity")
        return std::numeric_limits<double>::infinity();
    if (l == "-inf" || l == "-infinity")
        return -std::numeric_limits<double>::infinity();
    char* end = nullptr;
    const double v = std::strtod(t.c_str(), &end);
    if (end == t.c_str() || *end != '\0')
        return std::nullopt;
    return v;
}

PhysicsDomain domainOf(const std::string& s, ex::Quantity q)
{
    const std::string d = lower(s);
    if (d == "acoustic")
        return PhysicsDomain::Acoustic;
    if (d == "mechanical")
        return PhysicsDomain::Mechanical;
    if (d == "electromagnetic")
        return PhysicsDomain::Electromagnetic;
    if (d == "thermal")
        return PhysicsDomain::Thermal;
    switch (q) {
    case ex::Quantity::Pressure:
        return PhysicsDomain::Acoustic;
    case ex::Quantity::Temperature:
        return PhysicsDomain::Thermal;
    case ex::Quantity::Voltage:
    case ex::Quantity::ElectricField:
    case ex::Quantity::MagneticField:
    case ex::Quantity::ScatteringParameter:
    case ex::Quantity::Impedance:
        return PhysicsDomain::Electromagnetic;
    default:
        return PhysicsDomain::Mechanical;
    }
}

std::string idSafe(std::string s)
{
    for (char& c : s)
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_' && c != '.')
            c = '-';
    return lower(s);
}

class Importer {
public:
    Importer(std::string_view content, const ImportOptions& o) : content_(content), o_(o)
    {
        r_.report.file = utf8(o.fileName);
        r_.report.bytes = content.size();
        r_.report.sha256 = sha256Hex(content);
        r_.report.created = utcNowIso8601();
        r_.report.id = "imp-" + r_.report.sha256.substr(0, 12);
        std::string stem = o.fileName.substr(o.fileName.find_last_of('/') + 1);
        stem = stem.substr(0, stem.find_last_of('.'));
        prefix_ = idSafe(o.idPrefix.empty() ? (stem.empty() ? std::string("dataset") : stem) : o.idPrefix);
    }

    ImportResult run()
    {
        std::string format = lower(o_.format);
        if (format.empty() || format == "auto") {
            const std::string n = lower(o_.fileName);
            const auto ends = [&](const char* e) { return n.size() >= std::strlen(e) && n.compare(n.size() - std::strlen(e), std::strlen(e), e) == 0; };
            format = ends(".tsv") ? "tsv" : ends(".json") ? "json" : ends(".wav") ? "wav" : "csv";
        }
        r_.report.format = format;
        if (content_.empty())
            issue("error", "empty", "the file is empty");
        else if (format == "csv" || format == "tsv")
            delimited(format == "tsv" ? '\t' : ',');
        else if (format == "json")
            jsonDataset();
        else if (format == "wav")
            wav();
        else
            issue("error", "unreadable", "unknown format '" + format + "' (csv, tsv, json, wav)");
        finish();
        return std::move(r_);
    }

private:
    void issue(std::string severity, std::string code, std::string message, long row = -1, std::string column = {})
    {
        r_.report.issues.push_back({std::move(severity), std::move(code), std::move(message), row, std::move(column)});
    }

    std::string meta(const char* key) const
    {
        const json& m = r_.report.metadata;
        return m.contains(key) && m[key].is_string() ? m[key].get<std::string>() : std::string();
    }

    ex::MeasurementRecord record(const std::string& column, const ex::QuantitySpec& spec, ex::MeasurementKind kind)
    {
        ex::MeasurementRecord rec;
        rec.id = prefix_ + "-" + idSafe(column);
        rec.experimentId = o_.experimentId;
        rec.kind = kind;
        rec.category = ex::DataCategory::Raw;
        rec.quantity = spec;
        rec.domain = domainOf(o_.domain.empty() ? meta("domain") : o_.domain, spec.quantity);
        rec.createdUtc = r_.report.created;
        rec.deviceId = meta("instrument");
        rec.sensorId = meta("sensor");
        rec.simulatedDevice = provenance_ == "synthetic";
        rec.acquisition = {{"source", {{"file", r_.report.file}, {"sha256", r_.report.sha256}, {"format", r_.report.format}, {"column", column}}},
                           {"provenance", provenance_},
                           {"metadata", r_.report.metadata},
                           {"import", r_.report.id}};
        if (!meta("frame").empty())
            rec.acquisition["frame"] = meta("frame");
        // A stated uncertainty ("2 %" or an absolute value in the column's unit).
        const std::string u = meta("uncertainty");
        if (!u.empty()) {
            ex::UncertaintyComponent c;
            c.source = ex::UncertaintySource::InstrumentAccuracy;
            c.type = ex::UncertaintyType::InstrumentSpecification;
            c.description = "stated in the file: " + u;
            if (u.find('%') != std::string::npos)
                c.relative = std::strtod(u.c_str(), nullptr) / 100.0;
            else
                c.absolute = std::strtod(u.c_str(), nullptr);
            rec.uncertainty.add(c);
        }
        rec.note = provenance_ == "synthetic" ? "synthetic: generated with stated parameters, not a physical measurement" : std::string();
        return rec;
    }

    // What the import did (unit conversions, rows left out). A raw record has no processing history: these belong to
    // its acquisition, as the record of how the file's numbers became the record's.
    void addStep(ex::MeasurementRecord& rec, const std::string& op, const std::string& description, json parameters = json::object())
    {
        rec.acquisition["importSteps"].push_back({{"operation", op}, {"parameters", std::move(parameters)}, {"description", description}});
    }

    void metadataLine(const std::string& line, long row)
    {
        const std::string body = trim(line.substr(1));
        const auto colon = body.find(':');
        if (colon == std::string::npos) {
            r_.report.metadata["comments"].push_back(utf8(body));
            return;
        }
        const std::string key = utf8(lower(trim(body.substr(0, colon))));
        const std::string value = utf8(trim(body.substr(colon + 1)));
        if (key.empty()) {
            issue("warning", "malformed-row", "a metadata line without a key", row);
            return;
        }
        r_.report.metadata[key] = value;
    }

    void provenanceFromMetadata()
    {
        provenance_ = !o_.provenance.empty() ? lower(o_.provenance) : lower(meta("provenance"));
        if (provenance_.empty()) {
            provenance_ = "unstated";
            issue("warning", "missing-metadata", "the provenance is not stated (measured or synthetic): recorded as unstated");
        } else if (provenance_ != "measured" && provenance_ != "synthetic") {
            issue("warning", "missing-metadata", "unknown provenance '" + provenance_ + "' (measured or synthetic): recorded as stated");
        }
        r_.report.provenance = provenance_;
        if (meta("instrument").empty() && provenance_ != "synthetic")
            issue("warning", "missing-metadata", "the instrument is not identified: the datasets cannot claim calibration traceability");
    }

    void delimited(char sep)
    {
        std::istringstream in{std::string(content_)};
        std::string line;
        long row = 0;
        std::vector<Column> cols;
        std::vector<std::vector<double>> values;
        std::vector<long> rowOf;
        std::vector<long> malformed;
        while (std::getline(in, line)) {
            ++row;
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (trim(line).empty())
                continue;
            if (line[0] == '#') {
                metadataLine(line, row);
                continue;
            }
            const auto cells = split(line, sep);
            if (cols.empty()) {
                for (const auto& c : cells)
                    cols.push_back(parseHeader(c));
                values.assign(cols.size(), {});
                continue;
            }
            if (cells.size() != cols.size()) {
                malformed.push_back(row);
                issue("error", "malformed-row", "has " + std::to_string(cells.size()) + " fields, the header " + std::to_string(cols.size()), row);
                continue;
            }
            bool parsed = true;
            std::vector<double> v(cols.size(), kNaN);
            for (size_t c = 0; c < cols.size(); ++c) {
                auto x = number(cells[c]);
                if (!x && !trim(cells[c]).empty()) {
                    parsed = false;
                    issue("error", "malformed-row", "'" + trim(cells[c]) + "' is not a number", row, cols[c].name);
                } else if (!x) {
                    issue("warning", "missing-value", "an empty value, kept as NaN", row, cols[c].name);
                } else {
                    v[c] = *x;
                    if (!std::isfinite(*x))
                        issue("warning", "not-finite", std::string(std::isnan(*x) ? "NaN" : "an infinite value") + " kept as read", row, cols[c].name);
                }
            }
            if (!parsed) {
                malformed.push_back(row);
                continue;
            }
            for (size_t c = 0; c < cols.size(); ++c)
                values[c].push_back(v[c]);
            rowOf.push_back(row);
        }
        provenanceFromMetadata();
        r_.report.rows = rowOf.size();
        r_.report.columns = cols.size();
        if (cols.empty()) {
            issue("error", "empty", "no header row");
            return;
        }
        for (const std::string& want : o_.expectedColumns) {
            bool found = false;
            for (const Column& c : cols)
                found = found || lower(c.name) == lower(want);
            if (!found)
                issue("error", "missing-column", "the expected column '" + want + "' is missing", -1, want);
        }
        if (rowOf.empty()) {
            issue("error", "empty", "no data rows");
            return;
        }
        if (!malformed.empty())
            excludedRows_ = malformed;
        build(cols, values, rowOf);
    }

    // Builds the datasets of a table: the first column decides the kind.
    void build(const std::vector<Column>& cols, const std::vector<std::vector<double>>& values, const std::vector<long>& rows)
    {
        const std::string first = lower(cols[0].name);
        if (first == "quantity") {
            issue("error", "unreadable", "a scalar table has text columns: use the JSON format for scalar measurements");
            return;
        }
        for (size_t c = 0; c < cols.size(); ++c)
            if (!cols[c].hasUnit)
                issue("error", "missing-unit", "the column '" + cols[c].name + "' has no unit (write it as name [unit])", -1, cols[c].name);
        if (r_.report.count("error") > 0 && !o_.acceptPartial)
            return;
        const auto axisUnit = parseUnit(cols[0].unit);
        const bool time = axisUnit && axisUnit.value().dimension == parseUnit("s").value().dimension;
        const bool frequency = axisUnit && axisUnit.value().dimension == parseUnit("Hz").value().dimension && first.find("freq") != std::string::npos;
        const bool spatial = cols.size() >= 4 && first == "x" && lower(cols[1].name) == "y" && lower(cols[2].name) == "z";
        if (time)
            timeSeries(cols, values, rows, axisUnit.value().factor);
        else if (frequency)
            frequencyResponse(cols, values, axisUnit.value().factor);
        else if (spatial)
            spatialPoints(cols, values);
        else
            issue("error", "unreadable", "the first column must be time [s], frequency [Hz] or x, y, z [m] (found '" + cols[0].raw + "')", -1, cols[0].name);
    }

    bool unitFor(const Column& c, UnitMatch& m)
    {
        m = matchUnit(c.unit, c.name, meta("quantity"));
        if (!m.ok) {
            issue("error", "unknown-unit", "the unit '" + c.unit + "' of '" + c.name + "' is not a unit of a measured quantity", -1, c.name);
            return false;
        }
        if (!m.note.empty())
            issue("info", m.offset != 0.0 ? "conversion" : "assumed-quantity", m.note, -1, c.name);
        return true;
    }

    void timeSeries(const std::vector<Column>& cols, const std::vector<std::vector<double>>& values, const std::vector<long>& rows, double tf)
    {
        r_.report.kind = "time-series";
        std::vector<double> t;
        for (double x : values[0])
            t.push_back(x * tf);
        bool bad = false;
        for (size_t k = 1; k < t.size(); ++k) {
            if (!(t[k] > t[k - 1])) {
                bad = true;
                issue("error", t[k] == t[k - 1] ? "duplicate-time" : "non-monotonic-time",
                      t[k] == t[k - 1] ? "the time repeats the previous row's" : "the time goes backwards", rows[k], cols[0].name);
            }
        }
        if (bad && !o_.acceptPartial)
            return;
        double rate = 0.0;
        if (t.size() >= 2) {
            const double mean = (t.back() - t.front()) / (t.size() - 1);
            double worst = 0.0;
            for (size_t k = 1; k < t.size(); ++k)
                worst = std::max(worst, std::abs((t[k] - t[k - 1]) - mean));
            rate = 1.0 / mean;
            if (worst > 1e-3 * mean)
                issue("warning", "inconsistent-sampling",
                      "the time step varies by up to " + std::to_string(100.0 * worst / mean) + " % of its mean: the timestamps are kept; resample explicitly");
            const std::string stated = meta("sample rate");
            if (!stated.empty()) {
                const double s = std::strtod(stated.c_str(), nullptr);
                if (s > 0.0 && std::abs(s - rate) > 1e-3 * rate)
                    issue("warning", "sampling-mismatch", "the stated sample rate " + stated + " differs from the timestamps' " + std::to_string(rate) + " Hz");
            }
        }
        // "<name> uncertainty [unit]" columns hold the standard uncertainty of the column <name>.
        const auto uncertaintyOf = [&](const std::string& name) -> int {
            for (size_t u = 1; u < cols.size(); ++u)
                if (lower(cols[u].name) == lower(name) + " uncertainty")
                    return static_cast<int>(u);
            return -1;
        };
        for (size_t c = 1; c < cols.size(); ++c) {
            const std::string n = lower(cols[c].name);
            if (n.size() > 12 && n.compare(n.size() - 12, 12, " uncertainty") == 0)
                continue;  // attached to its column below
            UnitMatch m;
            if (!unitFor(cols[c], m))
                continue;
            ImportedDataset d;
            d.record = record(cols[c].name, m.spec, ex::MeasurementKind::TimeSeries);
            if (const int u = uncertaintyOf(cols[c].name); u >= 0) {
                UnitMatch um;
                if (!unitFor(cols[static_cast<size_t>(u)], um) || um.spec.quantity != m.spec.quantity) {
                    issue("error", "unknown-unit", "the uncertainty column '" + cols[static_cast<size_t>(u)].name + "' is not in a unit of its quantity", -1,
                          cols[static_cast<size_t>(u)].name);
                    continue;
                }
                for (double x : values[static_cast<size_t>(u)])
                    d.data.auxiliary.push_back(x * um.factor);
                d.record.acquisition["auxiliary"] = "standard-uncertainty";
            }
            d.record.timeBase.sampleRateHz = rate;
            d.record.timeBase.startSeconds = t.empty() ? 0.0 : t.front();
            for (double x : values[c])
                d.data.real.push_back(x * m.factor + m.offset);
            d.data.axis = t;
            d.record.shape = {d.data.real.size()};
            addStep(d.record, "import", "imported from " + r_.report.file + " (" + r_.report.format + "), column '" + cols[c].raw + "'",
                    {{"factor", m.factor}, {"offset", m.offset}, {"unit", cols[c].unit}, {"timeUnit", cols[0].unit}});
            noteExclusions(d.record);
            r_.datasets.push_back(std::move(d));
        }
    }

    void frequencyResponse(const std::vector<Column>& cols, const std::vector<std::vector<double>>& values, double ff)
    {
        r_.report.kind = "frequency-response";
        std::vector<double> f;
        for (double x : values[0])
            f.push_back(x * ff);
        for (size_t k = 1; k < f.size(); ++k)
            if (!(f[k] > f[k - 1])) {
                issue("error", "non-monotonic-time", "the frequency axis does not increase", -1, cols[0].name);
                if (!o_.acceptPartial)
                    return;
                break;
            }
        // Pairs: <name> real / imag, or magnitude / phase, plus optional coherence and uncertainty.
        int re = -1, im = -1, mag = -1, ph = -1, coh = -1, unc = -1;
        for (size_t c = 1; c < cols.size(); ++c) {
            const std::string n = lower(cols[c].name);
            if (n == "real" || n == "re")
                re = static_cast<int>(c);
            else if (n == "imag" || n == "im" || n == "imaginary")
                im = static_cast<int>(c);
            else if (n == "magnitude" || n == "mag" || n == "amplitude")
                mag = static_cast<int>(c);
            else if (n == "phase")
                ph = static_cast<int>(c);
            else if (n == "coherence")
                coh = static_cast<int>(c);
            else if (n == "uncertainty")
                unc = static_cast<int>(c);
            else
                issue("warning", "missing-column", "the column '" + cols[c].name + "' is not read (real, imag, magnitude, phase, coherence, uncertainty)",
                      -1, cols[c].name);
        }
        const int valueCol = re >= 0 ? re : mag;
        if (valueCol < 0 || (re >= 0 && im < 0)) {
            issue("error", "missing-column", "a frequency response needs real and imag, or magnitude (and phase) columns");
            return;
        }
        UnitMatch m;
        if (!unitFor(cols[static_cast<size_t>(valueCol)], m))
            return;
        double phaseFactor = 1.0;
        if (ph >= 0) {
            auto pu = parseUnit(cols[static_cast<size_t>(ph)].unit);
            if (!pu || !pu.value().dimension.dimensionless()) {
                issue("error", "unknown-unit", "the phase needs deg or rad", -1, cols[static_cast<size_t>(ph)].name);
                return;
            }
            phaseFactor = lower(cols[static_cast<size_t>(ph)].unit) == "deg" ? kPi / 180.0 : pu.value().factor;
        }
        ImportedDataset d;
        d.record = record(re >= 0 ? std::string("response") : cols[static_cast<size_t>(mag)].name == "magnitude" ? std::string("response")
                                                                                                                  : cols[static_cast<size_t>(mag)].name,
                          m.spec, m.spec.per ? ex::MeasurementKind::TransferFunction : ex::MeasurementKind::Spectrum);
        d.data.axis = f;
        for (size_t k = 0; k < f.size(); ++k) {
            if (re >= 0)
                d.data.complex.push_back({values[static_cast<size_t>(re)][k] * m.factor, values[static_cast<size_t>(im)][k] * m.factor});
            else
                d.data.complex.push_back(std::polar(values[static_cast<size_t>(mag)][k] * m.factor, ph >= 0 ? values[static_cast<size_t>(ph)][k] * phaseFactor : 0.0));
        }
        d.record.acquisition["phaseMeasured"] = re >= 0 || ph >= 0;
        if (re < 0 && ph < 0)
            issue("info", "missing-column", "no phase column: the phase is not measured and is not compared");
        if (coh >= 0) {
            d.data.auxiliary = values[static_cast<size_t>(coh)];
            d.record.acquisition["auxiliary"] = "coherence";
        } else if (unc >= 0) {
            for (double x : values[static_cast<size_t>(unc)])
                d.data.auxiliary.push_back(x * m.factor);
            d.record.acquisition["auxiliary"] = "standard-uncertainty";
        }
        d.record.shape = {f.size()};
        addStep(d.record, "import", "imported from " + r_.report.file + " (" + r_.report.format + ")",
                {{"factor", m.factor}, {"phaseFactor", phaseFactor}, {"unit", cols[static_cast<size_t>(valueCol)].unit}});
        noteExclusions(d.record);
        r_.datasets.push_back(std::move(d));
    }

    void spatialPoints(const std::vector<Column>& cols, const std::vector<std::vector<double>>& values)
    {
        r_.report.kind = "spatial";
        double lf[3];
        for (int a = 0; a < 3; ++a) {
            auto u = parseUnit(cols[static_cast<size_t>(a)].unit);
            if (!u || u.value().dimension != parseUnit("m").value().dimension) {
                issue("error", "unknown-unit", "the coordinates need a length unit", -1, cols[static_cast<size_t>(a)].name);
                return;
            }
            lf[a] = u.value().factor;
        }
        if (meta("frame").empty())
            issue("warning", "missing-frame", "the coordinate frame is not stated: a comparison needs it (or a transform)");
        int ph = -1;
        for (size_t c = 4; c < cols.size(); ++c)
            if (lower(cols[c].name) == "phase")
                ph = static_cast<int>(c);
        UnitMatch m;
        if (!unitFor(cols[3], m))
            return;
        ImportedDataset d;
        d.record = record(cols[3].name, m.spec, ex::MeasurementKind::Vector);
        const size_t n = values[0].size();
        for (size_t k = 0; k < n; ++k) {
            for (int a = 0; a < 3; ++a)
                d.data.axis.push_back(values[static_cast<size_t>(a)][k] * lf[a]);
            const double v = values[3][k] * m.factor + m.offset;
            if (ph >= 0)
                d.data.complex.push_back(std::polar(v, values[static_cast<size_t>(ph)][k] * (lower(cols[static_cast<size_t>(ph)].unit) == "deg" ? kPi / 180.0 : 1.0)));
            else
                d.data.real.push_back(v);
        }
        d.record.acquisition["layout"] = "points xyz (axis holds x, y, z per point)";
        d.record.shape = {n};
        addStep(d.record, "import", "imported from " + r_.report.file + " (" + r_.report.format + "), spatial points",
                {{"factor", m.factor}, {"lengthFactor", lf[0]}});
        noteExclusions(d.record);
        r_.datasets.push_back(std::move(d));
    }

    void noteExclusions(ex::MeasurementRecord& rec)
    {
        if (excludedRows_.empty())
            return;
        addStep(rec, "exclude-rows", "rows left out of the import (malformed): " + std::to_string(excludedRows_.size()) + " (see the import report)",
                {{"rows", excludedRows_}});
        rec.quality.notes.push_back("partial import: " + std::to_string(excludedRows_.size()) + " malformed rows left out");
    }

    // {"format": "resonant.dataset", "quantity", "unit", "kind", "axis": {"name", "unit", "values"}, "values" | "real" + "imag", "metadata"}
    // or a scalar list {"format": "resonant.dataset", "scalars": [{"name", "quantity", "unit", "value", "uncertainty"}]}
    void jsonDataset()
    {
        json j = json::parse(content_.begin(), content_.end(), nullptr, false);
        if (j.is_discarded() || !j.is_object()) {
            issue("error", "unreadable", "not a JSON object");
            return;
        }
        const json metadata = j.value("metadata", json::object());  // named: items() of a temporary would dangle
        for (const auto& [k, v] : metadata.items())
            r_.report.metadata[lower(k)] = v.is_string() ? v.get<std::string>() : v.dump();
        provenanceFromMetadata();
        if (j.contains("scalars")) {
            r_.report.kind = "scalar";
            for (const json& s : j["scalars"]) {
                const std::string name = s.value("name", std::string("value"));
                if (!s.contains("unit")) {
                    issue("error", "missing-unit", "the scalar '" + name + "' has no unit", -1, name);
                    continue;
                }
                UnitMatch m = matchUnit(s.value("unit", std::string()), name, s.value("quantity", std::string()));
                if (!m.ok) {
                    issue("error", "unknown-unit", "the unit '" + s.value("unit", std::string()) + "' is not a unit of a measured quantity", -1, name);
                    continue;
                }
                const double v = s.value("value", kNaN);
                if (!std::isfinite(v))
                    issue("warning", "not-finite", "the value is not finite", -1, name);
                ImportedDataset d;
                d.record = record(name, m.spec, ex::MeasurementKind::Scalar);
                d.data.real = {v * m.factor + m.offset};
                d.record.shape = {1};
                if (s.contains("uncertainty")) {
                    ex::UncertaintyComponent c;
                    c.source = ex::UncertaintySource::InstrumentAccuracy;
                    c.type = ex::UncertaintyType::InstrumentSpecification;
                    c.absolute = s["uncertainty"].get<double>() * m.factor;
                    c.description = s.value("uncertaintyNote", std::string("stated in the file (standard uncertainty)"));
                    d.record.uncertainty.add(c);
                }
                addStep(d.record, "import", "imported from " + r_.report.file + " (json scalar '" + name + "')", {{"factor", m.factor}, {"offset", m.offset}});
                r_.datasets.push_back(std::move(d));
            }
            r_.report.rows = j["scalars"].size();
            return;
        }
        const json axis = j.value("axis", json::object());
        if (!j.contains("unit")) {
            issue("error", "missing-unit", "the dataset has no unit");
            return;
        }
        if (!axis.contains("unit")) {
            issue("error", "missing-unit", "the axis has no unit");
            return;
        }
        std::vector<Column> cols{{axis.value("name", std::string("time")), axis.value("unit", std::string()), "", true}};
        std::vector<std::vector<double>> values{axis.value("values", std::vector<double>{})};
        const std::string unit = j.value("unit", std::string());
        if (j.contains("real")) {
            cols.push_back({"real", unit, "", true});
            cols.push_back({"imag", unit, "", true});
            values.push_back(j.value("real", std::vector<double>{}));
            values.push_back(j.value("imag", std::vector<double>{}));
        } else {
            cols.push_back({j.value("name", lower(axis.value("name", std::string("time"))).find("freq") != std::string::npos ? std::string("magnitude")
                                                                                                                                  : std::string("value")),
                            unit, "", true});
            values.push_back(j.value("values", std::vector<double>{}));
        }
        for (size_t c = 1; c < values.size(); ++c)
            if (values[c].size() != values[0].size()) {
                issue("error", "malformed-row", "the values and the axis differ in length");
                return;
            }
        for (size_t c = 0; c < values.size(); ++c)
            for (size_t k = 0; k < values[c].size(); ++k)
                if (!std::isfinite(values[c][k]))
                    issue("warning", "not-finite", "a value is not finite", static_cast<long>(k + 1), cols[c].name);
        if (!j.value("quantity", std::string()).empty())
            r_.report.metadata["quantity"] = j["quantity"];
        r_.report.rows = values[0].size();
        r_.report.columns = cols.size();
        if (values[0].empty()) {
            issue("error", "empty", "no values");
            return;
        }
        std::vector<long> rows;
        for (size_t k = 0; k < values[0].size(); ++k)
            rows.push_back(static_cast<long>(k + 1));
        build(cols, values, rows);
    }

    // PCM 8/16/24/32-bit integer or 32-bit float; each channel a time series in digital full scale.
    void wav()
    {
        const auto u16 = [&](size_t p) { return static_cast<uint32_t>(static_cast<unsigned char>(content_[p])) | (uint32_t(static_cast<unsigned char>(content_[p + 1])) << 8); };
        const auto u32 = [&](size_t p) { return u16(p) | (u16(p + 2) << 16); };
        if (content_.size() < 12 || content_.substr(0, 4) != "RIFF" || content_.substr(8, 4) != "WAVE") {
            issue("error", "unreadable", "not a RIFF/WAVE file");
            return;
        }
        provenanceFromMetadata();
        size_t p = 12;
        uint32_t format = 0, channels = 0, rate = 0, bits = 0;
        std::string_view data;
        while (p + 8 <= content_.size()) {
            const std::string_view id = content_.substr(p, 4);
            const uint32_t size = u32(p + 4);
            if (p + 8 + size > content_.size()) {
                issue("error", "unreadable", "the chunk '" + std::string(id) + "' runs past the end of the file (truncated)");
                return;
            }
            if (id == "fmt ") {
                format = u16(p + 8);
                channels = u16(p + 10);
                rate = u32(p + 12);
                bits = u16(p + 22);
            } else if (id == "data") {
                data = content_.substr(p + 8, size);
            }
            p += 8 + size + (size & 1);
        }
        if (channels == 0 || rate == 0 || data.empty() || (format != 1 && format != 3) || (format == 3 && bits != 32) || bits % 8 != 0) {
            issue("error", "unreadable", "unsupported WAVE format (PCM 8/16/24/32-bit or 32-bit float)");
            return;
        }
        r_.report.kind = "time-series";
        const size_t bytes = bits / 8, frames = data.size() / (bytes * channels);
        r_.report.rows = frames;
        r_.report.columns = channels;
        for (uint32_t ch = 0; ch < channels; ++ch) {
            ImportedDataset d;
            ex::QuantitySpec spec;
            spec.quantity = ex::Quantity::DigitalFullScale;
            d.record = record("channel " + std::to_string(ch + 1), spec, ex::MeasurementKind::TimeSeries);
            d.record.timeBase.sampleRateHz = rate;
            for (size_t f = 0; f < frames; ++f) {
                const size_t q = (f * channels + ch) * bytes;
                double v = 0.0;
                if (format == 3) {
                    float x;
                    std::memcpy(&x, data.data() + q, 4);
                    v = x;
                } else if (bits == 8) {
                    v = (static_cast<unsigned char>(data[q]) - 128) / 128.0;
                } else {
                    int64_t s = 0;
                    for (size_t b = 0; b < bytes; ++b)
                        s |= static_cast<int64_t>(static_cast<unsigned char>(data[q + b])) << (8 * b);
                    const int64_t sign = int64_t(1) << (bits - 1);
                    if (s & sign)
                        s -= sign << 1;
                    v = static_cast<double>(s) / static_cast<double>(sign);
                }
                d.data.real.push_back(v);
            }
            d.record.shape = {frames};
            addStep(d.record, "import", "imported from " + r_.report.file + " (WAVE, " + std::to_string(bits) + "-bit, channel " + std::to_string(ch + 1) + ")",
                    {{"bits", bits}, {"sampleRate", rate}});
            d.record.note += (d.record.note.empty() ? "" : "; ") + std::string("uncalibrated (digital full scale): apply a calibration for physical units");
            r_.datasets.push_back(std::move(d));
        }
        issue("info", "missing-metadata", "WAVE carries no units: the channels are in digital full scale (uncalibrated)");
    }

    void finish()
    {
        const size_t errors = r_.report.count("error");
        r_.report.accepted = !r_.datasets.empty() && (errors == 0 || o_.acceptPartial);
        if (!r_.report.accepted) {
            if (!r_.datasets.empty())
                issue("info", "unreadable", "no dataset was created: the import has errors (acceptPartial keeps what could be read)");
            r_.datasets.clear();
        } else if (errors > 0) {
            for (ImportedDataset& d : r_.datasets)
                d.record.quality.notes.push_back("imported with " + std::to_string(errors) + " error(s): see the import report " + r_.report.id);
        }
        for (const ImportedDataset& d : r_.datasets)
            r_.report.datasets.push_back(d.record.id);
    }

    std::string_view content_;
    ImportOptions o_;
    ImportResult r_;
    std::string prefix_, provenance_;
    std::vector<long> excludedRows_;
};

}  // namespace

size_t ImportReport::count(const std::string& severity) const
{
    return static_cast<size_t>(std::count_if(issues.begin(), issues.end(), [&](const ImportIssue& i) { return i.severity == severity; }));
}

json ImportReport::toJson() const
{
    json j{{"id", id}, {"file", file}, {"format", format}, {"sha256", sha256}, {"kind", kind}, {"provenance", provenance}, {"created", created},
           {"bytes", bytes}, {"rows", rows}, {"columns", columns}, {"accepted", accepted}, {"datasets", datasets}, {"metadata", metadata},
           {"errors", count("error")}, {"warnings", count("warning")}};
    j["issues"] = json::array();
    for (const ImportIssue& i : issues) {
        json x{{"severity", i.severity}, {"code", i.code}, {"message", i.message}};
        if (i.row >= 0)
            x["row"] = i.row;
        if (!i.column.empty())
            x["column"] = i.column;
        j["issues"].push_back(x);
    }
    return j;
}

ImportResult importData(std::string_view content, const ImportOptions& options)
{
    return Importer(content, options).run();
}

}  // namespace resonant::laboratory
