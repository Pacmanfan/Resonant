#include "coupling/LaboratoryStudies.h"

#include "coupling/LaboratoryModels.h"
#include "electromagnetics/Retrieval.h"
#include "foundation/Time.h"
#include "laboratory/ExperimentalResonance.h"
#include "laboratory/ParameterEstimation.h"
#include "laboratory/Validation.h"
#include "optimization/Persistence.h"
#include "project/Project.h"
#include "project/ProjectSerializer.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <sstream>

namespace resonant::coupling {

using nlohmann::json;
namespace ex = experiments;
namespace lab = laboratory;

namespace {

std::string number(double v)
{
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.4g", v);
    return buf;
}

bool synthetic(const ex::MeasurementRecord& r)
{
    return r.simulatedDevice || r.acquisition.value("provenance", std::string()) == "synthetic";
}

json describe(const ex::MeasurementRecord& r)
{
    return {{"dataset", r.id}, {"quantity", r.quantity.describe()}, {"unit", r.quantity.unit()}, {"category", std::string(ex::toString(r.category))},
            {"provenance", synthetic(r) ? std::string("synthetic") : r.acquisition.value("provenance", std::string("measured"))},
            {"instrument", r.deviceId}, {"calibration", r.calibrationId.empty() ? std::string("uncalibrated or not stated") : r.calibrationId},
            {"history", r.history.size()}};
}

// The standard uncertainty of each value: per point, or from the record's budget.
std::vector<double> pointUncertainty(const ex::ProcessingItem& item)
{
    std::vector<double> u;
    const size_t n = item.data.size();
    if (item.record.acquisition.value("auxiliary", std::string()) == "standard-uncertainty" && item.data.auxiliary.size() == n)
        return item.data.auxiliary;
    if (item.record.uncertainty.empty())
        return u;
    for (size_t k = 0; k < n; ++k)
        u.push_back(item.record.uncertainty.combinedStandard(item.data.isComplex() ? std::abs(item.data.complex[k]) : std::abs(item.data.real[k])));
    return u;
}

std::vector<double> timesOf(const ex::ProcessingItem& item)
{
    if (item.data.axis.size() == item.data.size())
        return item.data.axis;
    std::vector<double> t;
    const double rate = item.record.timeBase.sampleRateHz;
    for (size_t k = 0; k < item.data.size(); ++k)
        t.push_back(item.record.timeBase.startSeconds + (rate > 0 ? k / rate : static_cast<double>(k)));
    return t;
}

ex::FrequencySeries seriesOf(const ex::ProcessingItem& item)
{
    ex::FrequencySeries s;
    s.frequencyHz = item.data.axis;
    s.values = item.data.complex;
    if (s.values.empty())
        for (double v : item.data.real)
            s.values.push_back(v);
    s.standardUncertainty = pointUncertainty(item);
    s.quantity = item.record.quantity;
    s.category = item.record.category;
    s.label = item.record.id;
    return s;
}

std::map<std::string, double> parameterMap(const json& j)
{
    std::map<std::string, double> out;
    if (j.is_object())
        for (const auto& [k, v] : j.items())
            if (v.is_number())
                out[k] = v.get<double>();
    return out;
}

json findById(const json& list, const std::string& id)
{
    if (list.is_array())
        for (const json& x : list)
            if (x.is_object() && x.value("id", std::string()) == id)
                return x;
    return json();
}

json findObject(const json& objects, const std::string& id)
{
    if (!objects.is_array())
        return json();
    for (const json& o : objects) {
        if (o.value("id", std::string()) == id)
            return o;
        json c = findObject(o.value("children", json::array()), id);
        if (!c.is_null())
            return c;
    }
    return json();
}

}  // namespace

std::string currentRevision(const Project& project, const lab::ModelLink& link)
{
    const json doc = serializeProject(project);
    const json ext = doc.value("extensions", json::object());
    json content;
    if (link.kind == "physical-system")
        content = findById(ext.value("multiphysics", json::object()).value("systems", json::array()), link.id);
    else if (link.kind == "functional-structure")
        content = findById(ext.value("functional", json::object()).value("structures", json::array()), link.id);
    else if (link.kind == "material")
        content = findById(doc.value("materials", json::array()), link.id);
    else if (link.kind == "material-state-study")
        content = findById(ext.value("material-state", json::object()).value("studies", json::array()), link.id);
    else if (link.kind == "design-revision")
        content = findById(ext.value("optimization", json::object()).value("revisions", json::array()), link.id);
    else if (link.kind == "multiscale-representation")
        content = findById(ext.value("multiscale", json::object()).value("representations", json::array()), link.id);
    else if (link.kind == "scene-object")
        content = findObject(doc.value("scene", json::object()).value("objects", json::array()), link.id);
    else if (link.kind == "accounting-study")
        content = findById(ext.value("accounting", json::object()).value("studies", json::array()), link.id);
    else if (link.kind == "matter-architecture")
        content = findById(ext.value("matter", json::object()).value("architectures", json::array()), link.id);
    return content.is_null() ? std::string() : lab::fingerprint(content);
}

std::vector<std::string> staleLinks(const Project& project, const lab::Study& study)
{
    std::vector<std::string> out;
    for (const lab::ModelLink& l : study.models) {
        const std::string now = currentRevision(project, l);
        if (now.empty())
            out.push_back(l.kind + " '" + l.id + "' no longer exists");
        else if (!l.revision.empty() && now != l.revision)
            out.push_back(l.kind + " '" + l.id + "' changed since it was linked (revision " + l.revision + ", now " + now + ")");
    }
    return out;
}

// --- comparisons ------------------------------------------------------------------------------------

Result<json> runComparison(const json& spec, const Project& project, const DatasetLoader& load, JobContext* ctx)
{
    using R = Result<json>;
    const std::string kind = spec.value("kind", std::string());
    auto measured = load(spec.value("measured", std::string()));
    if (!measured)
        return R::failure(measured.message());
    const ex::ProcessingItem& m = measured.value();
    const json predictedSpec = spec.value("predicted", json::object());
    const json options = spec.value("options", json::object());
    json record{{"id", spec.value("id", std::string())}, {"name", spec.value("name", std::string())}, {"study", spec.value("study", std::string())},
                {"kind", kind}, {"created", utcNowIso8601()}, {"measured", describe(m.record)}, {"alignment", json::array()}, {"notes", json::array()}};
    if (spec.contains("link"))
        record["link"] = spec["link"];
    if (synthetic(m.record))
        record["notes"].push_back("the measured side is synthetic: this verifies the method, it is not physical evidence");
    // The predicted side: a model, another dataset, an inline value or grid.
    std::optional<ex::ProcessingItem> other;
    std::optional<Prediction> prediction;
    const auto predict = [&](const std::vector<double>& axis) -> Status {
        const std::string model = predictedSpec.value("model", std::string());
        if (model.empty())
            return Status::success();
        auto p = predictLaboratoryModel(model, predictedSpec.value("configuration", json::object()), parameterMap(predictedSpec.value("parameters", json::object())),
                                        axis, project, ctx);
        if (!p)
            return Status::failure(p.message());
        prediction = p.take();
        record["predicted"] = {{"model", model}, {"solver", prediction->solver}, {"configuration", predictedSpec.value("configuration", json::object())},
                               {"parameters", predictedSpec.value("parameters", json::object())}, {"details", prediction->details},
                               {"category", "simulated"}};
        return Status::success();
    };
    if (predictedSpec.contains("dataset")) {
        auto o = load(predictedSpec["dataset"].get<std::string>());
        if (!o)
            return R::failure(o.message());
        other = o.take();
        record["predicted"] = describe(other->record);
        if (other->record.category != ex::DataCategory::Simulated)
            record["notes"].push_back("the other side is a dataset of category " + std::string(ex::toString(other->record.category)) +
                                      ": a comparison of two observations, not a validation of a model");
    }
    if (kind == "response") {
        if (!m.data.isComplex() || m.data.axis.size() != m.data.size())
            return R::failure("a response comparison needs a frequency response dataset");
        ex::FrequencySeries ms = seriesOf(m), ps;
        if (other) {
            ps = seriesOf(*other);
        } else {
            if (Status st = predict(m.data.axis); !st)
                return R::failure(st.message());
            if (!prediction)
                return R::failure("the comparison names no prediction (a model or a dataset)");
            ps.frequencyHz = prediction->axis;
            ps.values = prediction->values;
            ps.quantity = prediction->quantity;
            ps.category = ex::DataCategory::Simulated;
            ps.model = prediction->model;
        }
        ex::ComparisonOptions o;
        o.relative = options.value("relative", false);
        o.comparePhase = options.value("comparePhase", true) && m.record.acquisition.value("phaseMeasured", true);
        o.coverageFactor = options.value("coverage", 2.0);
        if (!m.record.acquisition.value("phaseMeasured", true))
            record["alignment"].push_back("the phase was not measured: only magnitudes are compared");
        if (o.relative)
            record["alignment"].push_back("both normalized to their maximum (shape only): absolute levels not compared");
        record["alignment"].push_back("the prediction interpolated onto the measured frequencies; the overlap only");
        auto c = laboratory::compareResponses(ms, ps, o);
        if (!c)
            return R::failure(c.message());
        record["result"] = c.value().toJson();
        record["verdict"] = c.value().verdict;
    } else if (kind == "transient") {
        if (m.data.isComplex())
            return R::failure("a transient comparison needs a real time series");
        const std::vector<double> t = timesOf(m);
        std::vector<double> pt, pv;
        double pu = 0.0;
        std::string unit = m.record.quantity.unit();
        if (other) {
            if (!(other->record.quantity == m.record.quantity))
                return R::failure("the datasets measure different quantities (" + m.record.quantity.describe() + ", " + other->record.quantity.describe() + ")");
            pt = timesOf(*other);
            pv = other->data.real;
        } else {
            if (Status st = predict(t); !st)
                return R::failure(st.message());
            if (!prediction)
                return R::failure("the comparison names no prediction");
            if (!(prediction->quantity == m.record.quantity))
                return R::failure("the prediction is " + prediction->quantity.describe() + " and the measurement " + m.record.quantity.describe());
            pt = prediction->axis;
            for (const auto& v : prediction->values)
                pv.push_back(v.real());
        }
        auto c = laboratory::compareTransients(t, m.data.real, pointUncertainty(m), pt, pv, pu, unit, options.value("coverage", 2.0));
        if (!c)
            return R::failure(c.message());
        record["alignment"].push_back("the prediction interpolated onto the measured times; the overlap only");
        record["result"] = c.value().toJson();
        record["verdict"] = c.value().verdict;
    } else if (kind == "field") {
        if (m.data.axis.size() != 3 * m.data.size())
            return R::failure("a field comparison needs a spatial dataset (x, y, z per point)");
        std::vector<std::array<double, 3>> pos;
        for (size_t k = 0; k < m.data.size(); ++k)
            pos.push_back({m.data.axis[3 * k], m.data.axis[3 * k + 1], m.data.axis[3 * k + 2]});
        std::vector<double> mv;
        for (size_t k = 0; k < m.data.size(); ++k)
            mv.push_back(m.data.isComplex() ? std::abs(m.data.complex[k]) : m.data.real[k]);
        const json g = predictedSpec.value("grid", json::object());
        if (g.empty())
            return R::failure("a field comparison needs the predicted grid {origin, spacing, size, values, frame, unit}");
        laboratory::FieldGrid grid;
        grid.origin = g.value("origin", std::array<double, 3>{0, 0, 0});
        grid.spacing = g.value("spacing", std::array<double, 3>{1, 1, 1});
        grid.size = g.value("size", std::array<int, 3>{1, 1, 1});
        grid.values = g.value("values", std::vector<double>{});
        grid.frame = g.value("frame", std::string("scene"));
        grid.unit = g.value("unit", std::string());
        if (!grid.unit.empty() && grid.unit != m.record.quantity.unit())
            return R::failure("the predicted field is in " + grid.unit + " and the measurement in " + m.record.quantity.unit());
        std::array<double, 16> t{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        if (options.contains("transform"))
            t = options["transform"].get<std::array<double, 16>>();
        auto c = laboratory::compareFields(pos, mv, pointUncertainty(m), m.record.acquisition.value("frame", std::string()), grid, t,
                                           options.value("coverage", 2.0));
        if (!c)
            return R::failure(c.message());
        record["predicted"] = {{"grid", {{"frame", grid.frame}, {"size", grid.size}}}, {"source", predictedSpec.value("source", std::string("a supplied grid"))},
                               {"category", "simulated"}};
        record["result"] = c.value().toJson();
        record["verdict"] = c.value().verdict;
    } else if (kind == "scalar" || kind == "resonance") {
        // The measured value(s).
        std::vector<std::pair<std::string, laboratory::ScalarValue>> measuredValues;
        if (kind == "scalar") {
            if (m.data.size() != 1)
                return R::failure("a scalar comparison needs a scalar dataset");
            const double v = m.data.isComplex() ? std::abs(m.data.complex[0]) : m.data.real[0];
            measuredValues.push_back({"value", {v, m.record.uncertainty.combinedStandard(v), options.value("coverage", 2.0), m.record.quantity.unit(), m.record.id}});
        } else {
            json extracted;
            if (m.data.isComplex()) {
                auto rs = laboratory::resonancesFromResponse(m.data.axis, m.data.complex, m.record.acquisition.value("phaseMeasured", true));
                if (!rs)
                    return R::failure(rs.message());
                const double near = predictedSpec.value("frequency", json::object()).value("value", 0.0);
                const laboratory::ResonanceCandidate* best = nullptr;
                for (const auto& c : rs.value())
                    if (c.classification != "artifact" && (!best || (near > 0 ? std::abs(c.frequencyHz - near) < std::abs(best->frequencyHz - near)
                                                                              : c.amplitude > best->amplitude)))
                        best = &c;
                if (!best)
                    return R::failure("no resonance candidate in the measured response");
                extracted = best->toJson();
                measuredValues.push_back({"frequency", {best->frequencyHz, best->frequencyUncertainty, 2.0, "Hz", "measured resonance"}});
                measuredValues.push_back({"quality", {best->quality, best->qualityUncertainty, 2.0, "1", "measured Q"}});
                if (best->classification != "natural mode")
                    record["notes"].push_back("the measured peak is classified '" + best->classification + "', not a natural mode");
            } else {
                const auto t = timesOf(m);
                auto rd = laboratory::ringDown(m.data.real, t.size() > 1 ? t[1] - t[0] : 1.0 / m.record.timeBase.sampleRateHz, options.value("minHz", 0.0),
                                               options.value("maxHz", 0.0));
                if (!rd)
                    return R::failure(rd.message());
                extracted = rd.value().toJson();
                measuredValues.push_back({"frequency", {rd.value().frequencyHz, rd.value().frequencyUncertainty, 2.0, "Hz", "measured ring-down"}});
                measuredValues.push_back({"quality", {rd.value().quality, rd.value().qualityUncertainty, 2.0, "1", "measured Q"}});
            }
            record["extracted"] = extracted;
        }
        json results = json::object();
        bool allConsistent = true, anyAssessed = false;
        for (const auto& [key, mvalue] : measuredValues) {
            const json ps = kind == "scalar" ? predictedSpec : predictedSpec.value(key, json::object());
            if (ps.empty())
                continue;
            laboratory::ScalarValue pv;
            if (ps.contains("model")) {
                auto p = predictLaboratoryModel(ps["model"].get<std::string>(), ps.value("configuration", json::object()),
                                                parameterMap(ps.value("parameters", json::object())), {}, project, ctx);
                if (!p)
                    return R::failure(p.message());
                if (p.value().values.empty())
                    return R::failure("the model predicted nothing");
                pv = {p.value().values[0].real(), ps.value("standard", p.value().standard), 2.0, ps.value("unit", p.value().unit.empty() ? mvalue.unit : p.value().unit),
                      ps["model"].get<std::string>()};
                record["predicted"][key] = {{"model", ps["model"]}, {"solver", p.value().solver}, {"details", p.value().details}, {"category", "simulated"}};
            } else if (ps.contains("value")) {
                pv = {ps["value"].get<double>(), ps.value("standard", 0.0), 2.0, ps.value("unit", mvalue.unit), ps.value("label", std::string("stated"))};
                record["predicted"][key] = ps;
            } else {
                continue;
            }
            auto c = laboratory::compareScalars(mvalue, pv);
            if (!c)
                return R::failure(c.message());
            results[key] = c.value().toJson();
            anyAssessed = anyAssessed || c.value().verdict != "not assessable";
            allConsistent = allConsistent && c.value().verdict == "consistent";
        }
        if (results.empty())
            return R::failure("the comparison names no prediction");
        record["result"] = results;
        record["verdict"] = !anyAssessed ? "not assessable" : allConsistent ? "consistent" : "inconsistent";
    } else if (kind == "retrieval") {
        // Effective properties retrieved from measured reflection and transmission: the measured S-parameters,
        // the retrieved effective values and a predicted value are three different things, and the record keeps them apart.
        auto tr = load(spec.value("transmission", std::string()));
        if (!tr)
            return R::failure("a retrieval needs the transmission dataset: " + tr.message());
        if (!m.data.isComplex() || !tr.value().data.isComplex() || m.data.axis != tr.value().data.axis)
            return R::failure("a retrieval needs complex reflection and transmission on the same frequencies");
        const double d = options.value("thickness", 0.0), a = options.value("cellSize", 0.0);
        auto rv = em::retrieveSlab(m.data.axis, m.data.complex, tr.value().data.complex, d, a > 0.0 ? a : d);
        if (!rv)
            return R::failure(rv.message());
        json points = json::array();
        size_t nonlocal = 0, active = 0;
        for (const auto& p : rv.value().points) {
            points.push_back({{"frequencyHz", p.frequencyHz}, {"permittivity", {p.permittivity.real(), p.permittivity.imag()}},
                              {"permeability", {p.permeability.real(), p.permeability.imag()}}, {"index", {p.index.real(), p.index.imag()}},
                              {"passive", p.passive}, {"antiresonance", p.antiresonance}, {"nonlocal", p.nonlocal}, {"cellOverWavelength", p.cellOverWavelength},
                              {"illConditioned", p.illConditioned}});
            nonlocal += p.nonlocal ? 1 : 0;
            active += p.passive ? 0 : 1;
        }
        record["measured"]["transmission"] = describe(tr.value().record);
        record["alignment"].push_back("retrieved by the impedance and index method (Smith et al. 2002, Chen et al. 2004) from the measured r and t");
        record["result"] = {{"retrieved", points}, {"warnings", rv.value().warnings}, {"nonlocalPoints", nonlocal}, {"activePoints", active},
                            {"categories", {{"measured", "S-parameters (directly measured)"}, {"retrieved", "effective properties (derived by retrieval)"},
                                            {"predicted", "effective properties (simulated or analytical)"}}}};
        // A predicted permittivity at one frequency, if given.
        const double at = options.value("atHz", 0.0);
        const json pe = predictedSpec.value("permittivity", json::object());
        if (at > 0.0 && pe.contains("value")) {
            const em::RetrievedPoint* best = nullptr;
            for (const auto& p : rv.value().points)
                if (!best || std::abs(p.frequencyHz - at) < std::abs(best->frequencyHz - at))
                    best = &p;
            const double u = options.value("retrievedStandard", 0.0);
            auto c = laboratory::compareScalars({best->permittivity.real(), u, 2.0, "1", "retrieved"}, {pe["value"].get<double>(), pe.value("standard", 0.0), 2.0, "1", "predicted"});
            if (!c)
                return R::failure(c.message());
            record["result"]["permittivityAt"] = c.value().toJson();
            record["result"]["permittivityAt"]["frequencyHz"] = best->frequencyHz;
            record["result"]["permittivityAt"]["homogenizationRegime"] = best->nonlocal ? "outside (the cell is not small against the effective wavelength)" : "inside";
            record["verdict"] = best->nonlocal ? std::string("not assessable: outside the homogenization regime at that frequency") : c.value().verdict;
            record["predicted"] = {{"permittivity", pe}, {"category", "predicted effective property"}};
        } else {
            record["verdict"] = nonlocal > 0 ? "retrieved (part of the band is outside the homogenization regime)" : "retrieved";
        }
    } else {
        return R::failure("unknown comparison kind '" + kind + "' (response, transient, field, scalar, resonance, retrieval)");
    }
    return record;
}

// --- calibration and validation ---------------------------------------------------------------------

namespace {

struct Observable {
    std::vector<laboratory::Observation> observations;
    std::vector<std::vector<double>> axes;
    std::vector<std::string> datasets;
};

// axisIds: per dataset, another dataset whose values are its axis (a strain path for a stress), or empty.
Result<Observable> observationsOf(const std::vector<std::string>& ids, const std::string& observable, const DatasetLoader& load, json& range,
                                  const std::vector<std::string>& axisIds = {})
{
    using R = Result<Observable>;
    Observable out;
    range = json::array();
    for (size_t index = 0; index < ids.size(); ++index) {
        const std::string& id = ids[index];
        auto it = load(id);
        if (!it)
            return R::failure(it.message());
        const ex::ProcessingItem& d = it.value();
        laboratory::Observation o;
        o.dataset = id;
        o.unit = d.record.quantity.unit();
        const std::vector<double> u = pointUncertainty(d);
        std::vector<double> axis = d.data.axis.size() == d.data.size() ? d.data.axis : timesOf(d);
        if (index < axisIds.size() && !axisIds[index].empty()) {
            auto a = load(axisIds[index]);
            if (!a)
                return R::failure(a.message());
            if (a.value().data.real.size() != d.data.size())
                return R::failure("the axis dataset '" + axisIds[index] + "' and '" + id + "' differ in length");
            axis = a.value().data.real;
        }
        if (d.data.isComplex()) {
            if (observable == "real-imag") {
                for (size_t k = 0; k < d.data.size(); ++k) {
                    o.values.push_back(d.data.complex[k].real());
                    o.values.push_back(d.data.complex[k].imag());
                    if (!u.empty()) {
                        o.standard.push_back(u[k] / std::sqrt(2.0));
                        o.standard.push_back(u[k] / std::sqrt(2.0));
                    }
                }
            } else {
                for (size_t k = 0; k < d.data.size(); ++k) {
                    o.values.push_back(std::abs(d.data.complex[k]));
                    if (!u.empty())
                        o.standard.push_back(u[k]);
                }
            }
        } else {
            o.values = d.data.real;
            o.standard = u;
        }
        for (double v : o.values)
            if (!std::isfinite(v))
                return R::failure("the dataset '" + id + "' has values that are not finite: exclude them explicitly before calibrating");
        o.axis = axis;
        out.axes.push_back(axis);
        out.datasets.push_back(id);
        out.observations.push_back(o);
        range.push_back({{"dataset", id}, {"from", axis.empty() ? 0.0 : *std::min_element(axis.begin(), axis.end())},
                         {"to", axis.empty() ? 0.0 : *std::max_element(axis.begin(), axis.end())}});
    }
    return out;
}

std::vector<double> transformPrediction(const Prediction& p, const std::string& observable)
{
    std::vector<double> y;
    for (const auto& v : p.values) {
        if (!p.complex)
            y.push_back(v.real());
        else if (observable == "real-imag") {
            y.push_back(v.real());
            y.push_back(v.imag());
        } else
            y.push_back(std::abs(v));
    }
    return y;
}

}  // namespace

Result<json> runCalibration(const json& spec, const Project& project, const DatasetLoader& load, JobContext* ctx)
{
    using R = Result<json>;
    const std::string model = spec.value("model", std::string());
    const json cfg = spec.value("configuration", json::object());
    const std::string observable = spec.value("observable", std::string("magnitude"));
    auto available = laboratoryModelParameters(model, cfg, project);
    if (!available)
        return R::failure(available.message());
    std::vector<laboratory::EstimatedParameter> params;
    json original = json::object();
    for (const json& p : spec.value("parameters", json::array())) {
        const std::string id = p.value("id", std::string());
        const LabModelParameter* known = nullptr;
        for (const auto& a : available.value())
            if (a.id == id)
                known = &a;
        if (!known)
            return R::failure("'" + id + "' is not a physical parameter of the model " + model + " (calibration varies only those)");
        laboratory::EstimatedParameter e;
        e.id = id;
        e.label = known->label;
        e.unit = known->unit;
        e.meaning = known->meaning;
        e.initial = p.value("initial", known->value);
        e.lower = p.value("lower", known->lower);
        e.upper = p.value("upper", known->upper);
        if (p.contains("prior")) {
            e.priorMean = p["prior"].value("mean", e.initial);
            e.priorStandard = p["prior"].value("standard", 0.0);
        }
        original[id] = known->value;
        params.push_back(e);
    }
    if (params.empty())
        return R::failure("the calibration names no parameter");
    json range;
    auto obs = observationsOf(spec.value("datasets", std::vector<std::string>{}), observable, load, range,
                              spec.value("axisDatasets", std::vector<std::string>{}));
    if (!obs)
        return R::failure(obs.message());
    if (obs.value().observations.empty())
        return R::failure("the calibration names no dataset");
    const laboratory::Predictor predict = [&](const std::vector<double>& values, size_t i) -> Result<std::vector<double>> {
        std::map<std::string, double> pm;
        for (size_t k = 0; k < params.size(); ++k)
            pm[params[k].id] = values[k];
        auto p = predictLaboratoryModel(model, cfg, pm, obs.value().axes[i], project, nullptr);
        if (!p) {
            std::string at;
            for (const auto& [k, v] : pm)
                at += (at.empty() ? "" : ", ") + k + " = " + number(v);
            return Result<std::vector<double>>::failure(p.message() + " (at " + at + ")");
        }
        return transformPrediction(p.value(), observable);
    };
    laboratory::EstimationOptions eo;
    eo.maxIterations = spec.value("options", json::object()).value("maxIterations", 60);
    auto est = laboratory::estimateParameters(params, obs.value().observations, predict, eo, ctx);
    if (!est)
        return R::failure(est.message());
    const auto& e = est.value();
    std::string status = !e.converged ? "not converged" : e.identified() ? "calibrated" : "not identifiable";
    for (const auto& p : e.parameters)
        if (p.identifiability == "poorly identified" && status == "calibrated")
            status = "calibrated (poorly identified parameters)";
    json record{{"id", spec.value("id", std::string())},
                {"study", spec.value("study", std::string())},
                {"model", model},
                {"configuration", cfg},
                {"observable", observable},
                {"datasets", obs.value().datasets},
                {"axisDatasets", spec.value("axisDatasets", std::vector<std::string>{})},
                {"original", original},
                {"method", e.method},
                {"options", spec.value("options", json::object())},
                {"result", e.toJson()},
                {"status", status},
                {"operatingRange", range},
                {"created", utcNowIso8601()},
                {"notes", e.notes}};
    if (spec.contains("link"))
        record["link"] = spec["link"];
    json estimates = json::object();
    for (const auto& p : e.parameters)
        estimates[p.id] = p.estimate;
    record["estimates"] = estimates;
    bool anySynthetic = false;
    for (const std::string& id : obs.value().datasets)
        if (auto d = load(id); d && synthetic(d.value().record))
            anySynthetic = true;
    if (anySynthetic)
        record["notes"].push_back("calibrated against synthetic data: this verifies the method, it is not physical evidence");
    return record;
}

Result<json> runValidation(const json& cal, const std::vector<std::string>& datasets, const std::string& id, const Project& project,
                           const DatasetLoader& load, JobContext* ctx, const std::vector<std::string>& axisDatasets)
{
    using R = Result<json>;
    const auto calibrated = cal.value("datasets", std::vector<std::string>{});
    for (const std::string& d : datasets)
        if (std::find(calibrated.begin(), calibrated.end(), d) != calibrated.end())
            return R::failure("not independent: the dataset '" + d + "' was used for the calibration; validate with other data");
    if (datasets.empty())
        return R::failure("validation needs datasets");
    const std::string model = cal.value("model", std::string()), observable = cal.value("observable", std::string("magnitude"));
    const json cfg = cal.value("configuration", json::object());
    json range;
    auto obs = observationsOf(datasets, observable, load, range, axisDatasets);
    if (!obs)
        return R::failure(obs.message());
    std::map<std::string, double> estimates = parameterMap(cal.value("estimates", json::object())), original = parameterMap(cal.value("original", json::object()));
    // The parameters' covariance, for the prediction's uncertainty (first order).
    const json res = cal.value("result", json::object());
    const json covJ = res.value("covariance", json::array());
    std::vector<std::string> ids;
    for (const json& p : res.value("parameters", json::array()))
        ids.push_back(p.value("id", std::string()));
    const auto evaluate = [&](const std::map<std::string, double>& params, size_t i) -> Result<std::vector<double>> {
        auto p = predictLaboratoryModel(model, cfg, params, obs.value().axes[i], project, ctx);
        if (!p)
            return Result<std::vector<double>>::failure(p.message());
        return transformPrediction(p.value(), observable);
    };
    json performance = json::array();
    bool allWithin = true, assessed = false;
    for (size_t i = 0; i < obs.value().observations.size(); ++i) {
        const auto& o = obs.value().observations[i];
        auto yc = evaluate(estimates, i);
        auto yo = evaluate(original, i);
        if (!yc || !yo)
            return R::failure(!yc ? yc.message() : yo.message());
        // Prediction variance: g C g^T with finite-difference gradients.
        std::vector<double> predVar(o.values.size(), 0.0);
        if (covJ.size() == ids.size() && !ids.empty()) {
            std::vector<std::vector<double>> grads;
            for (const std::string& pid : ids) {
                std::map<std::string, double> q = estimates;
                const double h = 1e-6 * std::max(1e-12, std::abs(q[pid]));
                q[pid] += h;
                auto y = evaluate(q, i);
                if (!y)
                    return R::failure(y.message());
                std::vector<double> g;
                for (size_t k = 0; k < o.values.size(); ++k)
                    g.push_back((y.value()[k] - yc.value()[k]) / h);
                grads.push_back(g);
            }
            for (size_t k = 0; k < o.values.size(); ++k)
                for (size_t a = 0; a < ids.size(); ++a)
                    for (size_t b = 0; b < ids.size(); ++b)
                        predVar[k] += grads[a][k] * grads[b][k] * (covJ[a][b].is_number() ? covJ[a][b].get<double>() : 0.0);
        }
        double ssc = 0, sso = 0;
        size_t within = 0;
        for (size_t k = 0; k < o.values.size(); ++k) {
            const double s = o.standard.empty() ? 1.0 : o.standard[k];
            ssc += std::pow((o.values[k] - yc.value()[k]) / s, 2);
            sso += std::pow((o.values[k] - yo.value()[k]) / s, 2);
            if (!o.standard.empty() && std::abs(o.values[k] - yc.value()[k]) <= 2.0 * std::sqrt(s * s + predVar[k]))
                ++within;
        }
        const double n = static_cast<double>(o.values.size());
        const double fraction = o.standard.empty() ? -1.0 : within / n;
        const double threshold = laboratory::consistencyThreshold(o.values.size(), 2.0);
        if (fraction >= 0.0) {
            assessed = true;
            allWithin = allWithin && fraction >= threshold;
        }
        performance.push_back({{"dataset", o.dataset}, {"points", o.values.size()}, {"calibratedRms", std::sqrt(ssc / n)}, {"originalRms", std::sqrt(sso / n)},
                               {"normalized", !o.standard.empty()}, {"fractionWithin", fraction}, {"threshold", threshold},
                               {"criterion", "the fraction within twice the combined standard uncertainty (measurement and prediction), at least the "
                                             "normal coverage less three binomial standard deviations"}});
    }
    // Inside the calibrated operating range?
    double lo = INFINITY, hi = -INFINITY;
    for (const json& r : cal.value("operatingRange", json::array())) {
        lo = std::min(lo, r.value("from", 0.0));
        hi = std::max(hi, r.value("to", 0.0));
    }
    json notes = json::array();
    for (const json& r : range)
        if (r.value("from", 0.0) < lo - 1e-12 * std::abs(lo) || r.value("to", 0.0) > hi + 1e-12 * std::abs(hi))
            notes.push_back("the dataset '" + r.value("dataset", std::string()) + "' extends outside the calibrated range [" + number(lo) + ", " + number(hi) +
                            "]: an extrapolation");
    const std::string status = !assessed ? "not assessed (no measurement uncertainty)" : allWithin ? "validated within uncertainty" : "not validated";
    if (cal.value("status", std::string()) == "not identifiable")
        notes.push_back("the calibration's parameters are not identifiable: agreement does not establish their values");
    return json{{"id", id},
                {"calibration", cal.value("id", std::string())},
                {"study", cal.value("study", std::string())},
                {"datasets", datasets},
                {"calibrationDatasets", calibrated},
                {"calibrationPerformance", {{"reducedChiSquare", res.value("reducedChiSquare", 0.0)}}},
                {"validationPerformance", performance},
                {"range", range},
                {"status", status},
                {"notes", notes},
                {"created", utcNowIso8601()}};
}

Result<json> extractResonances(const json& spec, const DatasetLoader& load)
{
    using R = Result<json>;
    auto d = load(spec.value("dataset", std::string()));
    if (!d)
        return R::failure(d.message());
    const json o = spec.value("options", json::object());
    json out{{"dataset", describe(d.value().record)}, {"created", utcNowIso8601()}};
    if (d.value().data.isComplex()) {
        laboratory::ResonanceOptions ro;
        ro.maxPeaks = o.value("maxPeaks", 6);
        ro.minSnrDb = o.value("minSnrDb", 10.0);
        ro.referencePeaksHz = o.value("referencePeaksHz", std::vector<double>{});
        ro.predictedHz = o.value("predictedHz", std::vector<double>{});
        ro.mainsHz = o.value("mainsHz", 50.0);
        auto r = laboratory::resonancesFromResponse(d.value().data.axis, d.value().data.complex, d.value().record.acquisition.value("phaseMeasured", true), ro);
        if (!r)
            return R::failure(r.message());
        out["method"] = "response";
        out["resonances"] = json::array();
        for (const auto& c : r.value())
            out["resonances"].push_back(c.toJson());
    } else {
        const auto t = timesOf(d.value());
        auto r = laboratory::ringDown(d.value().data.real, t.size() > 1 ? t[1] - t[0] : 1.0, o.value("minHz", 0.0), o.value("maxHz", 0.0));
        if (!r)
            return R::failure(r.message());
        out["method"] = "ring-down";
        out["ringDown"] = r.value().toJson();
    }
    return out;
}

// --- explanations and reports -----------------------------------------------------------------------

json explainRecord(const json& rec, const lab::Workspace& w, const Project& project)
{
    json findings = json::array(), hypotheses = json::array(), missing = json::array(), suggestions = json::array();
    const std::string id = rec.value("id", std::string());
    const auto finding = [&](std::string s, std::string evidence) { findings.push_back({{"statement", std::move(s)}, {"evidence", std::move(evidence)}}); };
    const auto hypothesis = [&](std::string s, std::string basis) { hypotheses.push_back({{"statement", std::move(s)}, {"basis", std::move(basis)}}); };
    const lab::Study* study = w.study(rec.value("study", std::string()));
    if (rec.contains("kind")) {  // a comparison
        const json r = rec.value("result", json::object());
        finding("verdict: " + rec.value("verdict", std::string()), id + ".verdict");
        if (r.contains("peakOffsetRelative")) {
            const double off = r.value("peakOffsetRelative", 0.0);
            finding("the measured peak lies " + number(100.0 * std::abs(off)) + " % " + (off < 0 ? "below" : "above") + " the predicted one",
                    id + ".result.peakOffsetRelative");
            if (std::abs(off) > 0.005)
                hypothesis(std::string("a ") + (off < 0 ? "lower stiffness or a higher mass" : "higher stiffness or a lower mass") +
                               " than modelled: the actual dimensions, the material batch or the boundary conditions (mounting) may differ from the nominal ones",
                           "the frequency offset; to be tested, not established");
        }
        if (r.contains("frequency") && r.contains("quality")) {
            const double df = r["frequency"].value("relative", 0.0), dq = r["quality"].value("relative", 0.0);
            finding("frequency: E_n " + (r["frequency"]["normalizedError"].is_number() ? number(r["frequency"]["normalizedError"].get<double>()) : std::string("not assessable")) +
                        "; Q: E_n " + (r["quality"]["normalizedError"].is_number() ? number(r["quality"]["normalizedError"].get<double>()) : std::string("not assessable")),
                    id + ".result");
            if (dq < -0.1)
                hypothesis("an additional loss mechanism not in the model: the mounting, the sensor's mass and cable, air damping or material damping",
                           "the measured Q is " + number(100 * std::abs(dq)) + " % lower");
            if (df < -0.01)
                hypothesis("the measured frequency is lower: added mass of the sensor, a softer support or a softer material", "the frequency offset");
        }
        const json mside = rec.value("measured", json::object());
        if (mside.value("provenance", std::string()) == "synthetic")
            finding("the measured side is synthetic data: the comparison verifies the method, not the physics", id + ".measured.provenance");
        if (mside.value("instrument", std::string()).empty())
            missing.push_back("the instrument of the measured dataset is not identified");
        if (mside.value("calibration", std::string()).find("not stated") != std::string::npos)
            missing.push_back("the measured dataset's calibration is not stated: its traceability cannot be claimed");
        if (r.value("fractionWithinCombined", 0.0) < 0.0 || rec.value("verdict", std::string()).find("not assessable") != std::string::npos) {
            missing.push_back("the measurement's uncertainty is not stated: agreement cannot be judged");
            suggestions.push_back("state the measurement uncertainty (an error budget) or repeat the measurement to estimate its repeatability");
        }
    } else if (rec.contains("estimates")) {  // a calibration
        finding("status: " + rec.value("status", std::string()), id + ".status");
        for (const json& p : rec.value("result", json::object()).value("parameters", json::array()))
            finding(p.value("id", std::string()) + " = " + number(p.value("estimate", 0.0)) + " (95 % interval " + number(p.value("lower", 0.0)) + " to " +
                        number(p.value("upper", 0.0)) + " " + p.value("unit", std::string()) + "), " + p.value("identifiability", std::string()),
                    id + ".result.parameters");
        for (const json& n : rec.value("notes", json::array()))
            finding(n.get<std::string>(), id + ".notes");
        if (rec.value("status", std::string()) == "not identifiable")
            suggestions.push_back("add measurements that separate the correlated parameters: another frequency range, a known added mass, a different boundary condition");
        if (rec.value("result", json::object()).value("reducedChiSquare", 0.0) > 3.0)
            hypothesis("the model form misses a mechanism the data show (a further mode, nonlinearity, a frequency-dependent loss)",
                       "the reduced chi-square exceeds 3");
        bool validated = false;
        for (const json& v : w.validations)
            if (v.value("calibration", std::string()) == id)
                validated = true;
        if (!validated)
            missing.push_back("no independent validation: the calibration has been tested only against the data it was fitted to");
    } else if (rec.contains("calibration")) {  // a validation
        finding("status: " + rec.value("status", std::string()), id + ".status");
        for (const json& n : rec.value("notes", json::array()))
            finding(n.get<std::string>(), id + ".notes");
    }
    if (study) {
        for (const auto& d : study->deviations)
            hypothesis("the measured " + d.parameter + " (" + number(d.measured) + " " + d.unit + " against " + number(d.nominal) + " nominal) may explain part of the difference",
                       "a recorded deviation of the study");
        for (const std::string& s : staleLinks(project, *study))
            missing.push_back("the linked model changed: " + s);
    }
    return {{"record", id}, {"findings", findings}, {"hypotheses", hypotheses}, {"missing", missing}, {"suggestions", suggestions},
            {"disclaimer", "built from the records; hypotheses are not findings; no model or parameter was changed"}};
}

Result<std::string> laboratoryReport(const std::string& studyId, const lab::Workspace& w, const Project& project, const std::string& format)
{
    using R = Result<std::string>;
    const lab::Study* s = w.study(studyId);
    if (!s)
        return R::failure("there is no study '" + studyId + "'");
    if (format != "markdown" && format != "html")
        return R::failure("the report format is markdown or html");
    std::ostringstream o;
    o << "# " << (s->name.empty() ? s->id : s->name) << "\n\n";
    o << "- Objective: " << (s->objective.empty() ? "not stated" : s->objective) << "\n";
    o << "- Hypothesis: " << (s->hypothesis.empty() ? "not stated" : s->hypothesis) << "\n";
    o << "- Specimen: " << (s->specimen.empty() ? "not stated" : s->specimen) << "; domain: " << s->domain << "; status: " << s->status << "\n";
    if (!s->operatorName.empty())
        o << "- Operator: " << s->operatorName << "\n";
    o << "\n## Configuration\n\n";
    if (s->deviations.empty())
        o << "No measured deviation from the nominal configuration is recorded.\n";
    else {
        o << "| Parameter | Nominal | Measured | Uncertainty | Unit | Method |\n|---|---|---|---|---|---|\n";
        for (const auto& d : s->deviations)
            o << "| " << d.parameter << " | " << number(d.nominal) << " | " << number(d.measured) << " | " << number(d.uncertainty) << " | " << d.unit << " | "
              << d.method << " |\n";
    }
    o << "\n## Models\n\n";
    const auto stale = staleLinks(project, *s);
    for (const auto& l : s->models)
        o << "- " << l.kind << " `" << l.id << "` at revision " << (l.revision.empty() ? std::string("(not recorded)") : l.revision) << "\n";
    for (const auto& x : stale)
        o << "- **Stale:** " << x << "\n";
    o << "\n## Datasets\n\n| Dataset | Role |\n|---|---|\n";
    for (const auto& d : s->datasets)
        o << "| " << d.measurementId << " | " << lab::toString(d.role) << " |\n";
    const auto section = [&](const char* title, const json& list, auto line) {
        o << "\n## " << title << "\n\n";
        bool any = false;
        for (const json& r : list)
            if (r.value("study", std::string()) == s->id) {
                line(r);
                any = true;
            }
        if (!any)
            o << "None.\n";
    };
    section("Comparisons", w.comparisons, [&](const json& r) {
        o << "- **" << r.value("name", r.value("id", std::string())) << "** (" << r.value("kind", std::string()) << "): " << r.value("verdict", std::string()) << "\n";
        for (const json& n : r.value("notes", json::array()))
            o << "  - " << n.get<std::string>() << "\n";
    });
    section("Calibrations", w.calibrations, [&](const json& r) {
        o << "- **" << r.value("id", std::string()) << "** (" << r.value("model", std::string()) << ", " << r.value("method", std::string()) << "): "
          << r.value("status", std::string()) << "\n";
        for (const json& p : r.value("result", json::object()).value("parameters", json::array()))
            o << "  - " << p.value("id", std::string()) << " = " << number(p.value("estimate", 0.0)) << " " << p.value("unit", std::string()) << " (95 %: "
              << number(p.value("lower", 0.0)) << " to " << number(p.value("upper", 0.0)) << "), " << p.value("identifiability", std::string()) << "\n";
    });
    section("Validations", w.validations, [&](const json& r) {
        o << "- **" << r.value("id", std::string()) << "** of " << r.value("calibration", std::string()) << " on " << r["datasets"].dump() << ": "
          << r.value("status", std::string()) << "\n";
        for (const json& n : r.value("notes", json::array()))
            o << "  - " << n.get<std::string>() << "\n";
    });
    o << "\n## Conclusions and limitations\n\n";
    bool validated = false;
    for (const json& v : w.validations)
        if (v.value("study", std::string()) == s->id && v.value("status", std::string()) == "validated within uncertainty")
            validated = true;
    o << (validated ? "- The calibrated model agrees with independent data within the measurement uncertainty, inside the calibrated operating range only.\n"
                    : "- No independent validation within uncertainty is recorded: the model is not validated by this study.\n");
    o << "- Hypotheses about discrepancies are not findings until tested.\n";
    if (format == "markdown")
        return o.str();
    // HTML: the Markdown's lines as paragraphs, list items and table rows (escaped).
    std::string html = "<!doctype html><html><head><meta charset=\"utf-8\"><title>Laboratory report</title></head><body>\n";
    std::istringstream in(o.str());
    std::string line;
    const auto esc = [](std::string t) {
        std::string out;
        for (char c : t)
            out += c == '<' ? "&lt;" : c == '>' ? "&gt;" : c == '&' ? "&amp;" : std::string(1, c);
        return out;
    };
    while (std::getline(in, line)) {
        if (line.rfind("# ", 0) == 0)
            html += "<h1>" + esc(line.substr(2)) + "</h1>\n";
        else if (line.rfind("## ", 0) == 0)
            html += "<h2>" + esc(line.substr(3)) + "</h2>\n";
        else if (line.rfind("- ", 0) == 0 || line.rfind("  - ", 0) == 0)
            html += "<p>&bull; " + esc(line.substr(line.find("- ") + 2)) + "</p>\n";
        else if (line.rfind("|", 0) == 0 && line.find("---") == std::string::npos)
            html += "<pre>" + esc(line) + "</pre>\n";
        else if (!line.empty() && line.find("---") == std::string::npos)
            html += "<p>" + esc(line) + "</p>\n";
    }
    return html + "</body></html>\n";
}

}  // namespace resonant::coupling
