#include "laboratory/ErrorBudget.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <random>

namespace resonant::laboratory {

using nlohmann::json;

namespace {

// The regularized incomplete beta function I_x(a, b) by its continued fraction.
double betacf(double a, double b, double x)
{
    const int maxIt = 300;
    const double eps = 1e-15, fpmin = 1e-300;
    double qab = a + b, qap = a + 1.0, qam = a - 1.0, c = 1.0, d = 1.0 - qab * x / qap;
    if (std::abs(d) < fpmin)
        d = fpmin;
    d = 1.0 / d;
    double h = d;
    for (int m = 1; m <= maxIt; ++m) {
        const int m2 = 2 * m;
        double aa = m * (b - m) * x / ((qam + m2) * (a + m2));
        d = 1.0 + aa * d;
        if (std::abs(d) < fpmin)
            d = fpmin;
        c = 1.0 + aa / c;
        if (std::abs(c) < fpmin)
            c = fpmin;
        d = 1.0 / d;
        h *= d * c;
        aa = -(a + m) * (qab + m) * x / ((a + m2) * (qap + m2));
        d = 1.0 + aa * d;
        if (std::abs(d) < fpmin)
            d = fpmin;
        c = 1.0 + aa / c;
        if (std::abs(c) < fpmin)
            c = fpmin;
        d = 1.0 / d;
        const double del = d * c;
        h *= del;
        if (std::abs(del - 1.0) < eps)
            break;
    }
    return h;
}

double incompleteBeta(double a, double b, double x)
{
    if (x <= 0.0)
        return 0.0;
    if (x >= 1.0)
        return 1.0;
    const double bt = std::exp(std::lgamma(a + b) - std::lgamma(a) - std::lgamma(b) + a * std::log(x) + b * std::log(1.0 - x));
    return x < (a + 1.0) / (a + b + 2.0) ? bt * betacf(a, b, x) / a : 1.0 - bt * betacf(b, a, 1.0 - x) / b;
}

// P(|T| <= t) for nu degrees of freedom.
double tTwoSided(double t, double nu)
{
    return 1.0 - incompleteBeta(0.5 * nu, 0.5, nu / (nu + t * t));
}

double normalQuantile(double p)
{
    // Acklam's rational approximation, refined by one Newton step.
    static const double a[] = {-3.969683028665376e+01, 2.209460984245205e+02, -2.759285104469687e+02, 1.383577518672690e+02, -3.066479806614716e+01,
                               2.506628277459239e+00};
    static const double b[] = {-5.447609879822406e+01, 1.615858368580409e+02, -1.556989798598866e+02, 6.680131188771972e+01, -1.328068155288572e+01};
    static const double c[] = {-7.784894002430293e-03, -3.223964580411365e-01, -2.400758277161838e+00, -2.549732539343734e+00, 4.374664141464968e+00,
                               2.938163982698783e+00};
    static const double d[] = {7.784695709041462e-03, 3.224671290700398e-01, 2.445134137142996e+00, 3.754408661907416e+00};
    double x;
    if (p < 0.02425) {
        const double q = std::sqrt(-2 * std::log(p));
        x = (((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) / ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1);
    } else if (p > 1 - 0.02425) {
        const double q = std::sqrt(-2 * std::log(1 - p));
        x = -(((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) / ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1);
    } else {
        const double q = p - 0.5, r = q * q;
        x = (((((a[0] * r + a[1]) * r + a[2]) * r + a[3]) * r + a[4]) * r + a[5]) * q /
            (((((b[0] * r + b[1]) * r + b[2]) * r + b[3]) * r + b[4]) * r + 1);
    }
    const double e = 0.5 * std::erfc(-x / std::sqrt(2.0)) - p;
    const double u = e * std::sqrt(2 * 3.14159265358979323846) * std::exp(x * x / 2);
    return x - u / (1 + x * u / 2);
}

double phi(double z)
{
    return 0.5 * std::erfc(-z / std::sqrt(2.0));
}

Eigen::MatrixXd correlationOrIdentity(const Eigen::MatrixXd& r, size_t n)
{
    if (r.size() == 0)
        return Eigen::MatrixXd::Identity(static_cast<Eigen::Index>(n), static_cast<Eigen::Index>(n));
    return r;
}

}  // namespace

double BudgetComponent::standard() const
{
    if (distribution == "rectangular")
        return std::abs(value) / std::sqrt(3.0);
    if (distribution == "triangular")
        return std::abs(value) / std::sqrt(6.0);
    if (distribution == "u-shaped")
        return std::abs(value) / std::sqrt(2.0);
    return std::abs(value) / (statedCoverage > 0.0 ? statedCoverage : 1.0);
}

double coverageFactor(double nu, double p)
{
    if (!(nu > 0.0) || !std::isfinite(nu) || nu > 1e6)
        return normalQuantile(0.5 * (1.0 + p));
    double lo = 0.0, hi = 1000.0;
    for (int i = 0; i < 200; ++i) {
        const double mid = 0.5 * (lo + hi);
        (tTwoSided(mid, nu) < p ? lo : hi) = mid;
    }
    return 0.5 * (lo + hi);
}

json BudgetResult::toJson() const
{
    json j{{"value", value}, {"unit", unit}, {"combined", combined}, {"expanded", expanded}, {"coverageFactor", coverageFactor},
           {"coverageProbability", coverageProbability}, {"correlation", correlation}, {"notes", notes}};
    j["effectiveDegreesOfFreedom"] = std::isfinite(effectiveDegreesOfFreedom) ? json(effectiveDegreesOfFreedom) : json("infinite");
    j["lines"] = json::array();
    for (const BudgetLine& l : lines)
        j["lines"].push_back({{"id", l.id}, {"source", l.source}, {"standard", l.standard}, {"contribution", l.contribution}});
    return j;
}

Result<BudgetResult> ErrorBudget::evaluate() const
{
    using R = Result<BudgetResult>;
    static const std::vector<std::string> sources = {"instrument", "sensor", "calibration", "resolution", "repeatability", "reproducibility",
                                                     "sampling",   "timing", "position",    "environment", "manufacturing", "material",
                                                     "numerical",  "model-form", "other"};
    if (components.empty())
        return R::failure("the budget has no component");
    std::map<std::string, size_t> index;
    for (size_t i = 0; i < components.size(); ++i) {
        const BudgetComponent& c = components[i];
        if (c.id.empty() || !index.emplace(c.id, i).second)
            return R::failure("every component needs a unique id");
        if (std::find(sources.begin(), sources.end(), c.source) == sources.end())
            return R::failure("the component '" + c.id + "' has an unknown source '" + c.source + "'");
        if (c.distribution != "normal" && c.distribution != "rectangular" && c.distribution != "triangular" && c.distribution != "u-shaped")
            return R::failure("the component '" + c.id + "' has an unknown distribution '" + c.distribution + "'");
        if (!std::isfinite(c.value) || !std::isfinite(c.sensitivity))
            return R::failure("the component '" + c.id + "' is not finite");
    }
    const size_t n = components.size();
    Eigen::MatrixXd r = Eigen::MatrixXd::Identity(static_cast<Eigen::Index>(n), static_cast<Eigen::Index>(n));
    for (const BudgetCorrelation& k : correlations) {
        if (!index.count(k.a) || !index.count(k.b) || k.a == k.b)
            return R::failure("a correlation names unknown or identical components");
        if (!(std::abs(k.r) <= 1.0))
            return R::failure("a correlation coefficient lies in [-1, 1]");
        if (k.justification.empty())
            return R::failure("the correlation between '" + k.a + "' and '" + k.b + "' needs a justification");
        r(static_cast<Eigen::Index>(index[k.a]), static_cast<Eigen::Index>(index[k.b])) = k.r;
        r(static_cast<Eigen::Index>(index[k.b]), static_cast<Eigen::Index>(index[k.a])) = k.r;
    }
    Eigen::VectorXd cu(static_cast<Eigen::Index>(n));
    for (size_t i = 0; i < n; ++i)
        cu(static_cast<Eigen::Index>(i)) = components[i].sensitivity * components[i].standard();
    const double uc2 = cu.dot(r * cu);
    if (!(uc2 >= 0.0))
        return R::failure("the stated correlations are not consistent (a negative combined variance)");
    BudgetResult out;
    out.value = value;
    out.unit = unit;
    out.coverageProbability = coverageProbability;
    out.combined = std::sqrt(uc2);
    for (size_t i = 0; i < n; ++i) {
        BudgetLine l;
        l.id = components[i].id;
        l.source = components[i].source;
        l.standard = std::abs(cu(static_cast<Eigen::Index>(i)));
        l.contribution = uc2 > 0.0 ? cu(static_cast<Eigen::Index>(i)) * (r.row(static_cast<Eigen::Index>(i)) * cu)(0) / uc2 : 0.0;
        out.lines.push_back(l);
    }
    if (correlations.empty()) {
        out.correlation = "the components are uncorrelated: no correlation was stated";
        double denom = 0.0;
        for (size_t i = 0; i < n; ++i) {
            const double nu = components[i].degreesOfFreedom > 0.0 ? components[i].degreesOfFreedom : INFINITY;
            if (std::isfinite(nu))
                denom += std::pow(cu(static_cast<Eigen::Index>(i)), 4) / nu;
        }
        out.effectiveDegreesOfFreedom = denom > 0.0 ? uc2 * uc2 / denom : INFINITY;
    } else {
        out.correlation = std::to_string(correlations.size()) + " stated correlation(s), each with its justification";
        out.effectiveDegreesOfFreedom = INFINITY;
        for (const BudgetComponent& c : components)
            if (c.degreesOfFreedom > 0.0) {
                out.notes.push_back("Welch-Satterthwaite does not apply with correlations: the degrees of freedom are taken as infinite");
                break;
            }
    }
    out.coverageFactor = coverageFactor(out.effectiveDegreesOfFreedom, coverageProbability);
    out.expanded = out.coverageFactor * out.combined;
    return out;
}

json ErrorBudget::toJson() const
{
    json j{{"quantity", quantity}, {"unit", unit}, {"value", value}, {"coverageProbability", coverageProbability}};
    j["components"] = json::array();
    for (const BudgetComponent& c : components)
        j["components"].push_back({{"id", c.id}, {"source", c.source}, {"description", c.description}, {"unit", c.unit}, {"method", c.method},
                                   {"distribution", c.distribution}, {"value", c.value}, {"statedCoverage", c.statedCoverage},
                                   {"sensitivity", c.sensitivity}, {"degreesOfFreedom", c.degreesOfFreedom}});
    j["correlations"] = json::array();
    for (const BudgetCorrelation& c : correlations)
        j["correlations"].push_back({{"a", c.a}, {"b", c.b}, {"r", c.r}, {"justification", c.justification}});
    return j;
}

Result<ErrorBudget> ErrorBudget::fromJson(const json& j)
{
    if (!j.is_object())
        return Result<ErrorBudget>::failure("a budget is an object");
    ErrorBudget b;
    b.quantity = j.value("quantity", std::string());
    b.unit = j.value("unit", std::string());
    b.value = j.value("value", 0.0);
    b.coverageProbability = j.value("coverageProbability", 0.95);
    for (const json& c : j.value("components", json::array()))
        b.components.push_back({c.value("id", std::string()), c.value("source", std::string("other")), c.value("description", std::string()),
                                c.value("unit", b.unit), c.value("method", std::string()), c.value("distribution", std::string("normal")),
                                c.value("value", 0.0), c.value("statedCoverage", 1.0), c.value("sensitivity", 1.0), c.value("degreesOfFreedom", 0.0)});
    for (const json& c : j.value("correlations", json::array()))
        b.correlations.push_back({c.value("a", std::string()), c.value("b", std::string()), c.value("r", 0.0), c.value("justification", std::string())});
    return b;
}

json Propagation::toJson() const
{
    return {{"method", method}, {"mean", mean}, {"standard", standard}, {"lower", lower}, {"upper", upper}, {"probability", probability},
            {"samples", samples}, {"sensitivities", sensitivities}, {"notes", notes}};
}

Result<Propagation> propagateLinear(const Model& f, const std::vector<InputQuantity>& in, const Eigen::MatrixXd& correlation)
{
    using R = Result<Propagation>;
    const size_t n = in.size();
    const Eigen::MatrixXd r = correlationOrIdentity(correlation, n);
    if (static_cast<size_t>(r.rows()) != n || static_cast<size_t>(r.cols()) != n)
        return R::failure("the correlation matrix does not match the inputs");
    std::vector<double> x;
    for (const InputQuantity& q : in)
        x.push_back(q.mean);
    auto y0 = f(x);
    if (!y0)
        return R::failure(y0.message());
    Propagation p;
    p.method = "first order (law of propagation of uncertainty, central differences)";
    p.mean = y0.value();
    Eigen::VectorXd cu(static_cast<Eigen::Index>(n));
    for (size_t i = 0; i < n; ++i) {
        const double h = in[i].standard > 0.0 ? 1e-3 * in[i].standard : 1e-6 * std::max(1.0, std::abs(in[i].mean));
        std::vector<double> a = x, b = x;
        a[i] += h;
        b[i] -= h;
        auto fa = f(a), fb = f(b);
        if (!fa || !fb)
            return R::failure("the model fails near the inputs' values");
        const double ci = (fa.value() - fb.value()) / (2.0 * h);
        p.sensitivities.push_back(ci);
        cu(static_cast<Eigen::Index>(i)) = ci * in[i].standard;
    }
    p.standard = std::sqrt(std::max(0.0, cu.dot(r * cu)));
    const double k = coverageFactor(0.0, p.probability);
    p.lower = p.mean - k * p.standard;
    p.upper = p.mean + k * p.standard;
    p.notes.push_back(correlation.size() == 0 ? "inputs uncorrelated (none stated)" : "with the stated input correlations");
    p.notes.push_back("first order: a nonlinear model can bias the mean and skew the interval (compare Monte Carlo)");
    return p;
}

Result<Propagation> propagateMonteCarlo(const Model& f, const std::vector<InputQuantity>& in, const Eigen::MatrixXd& correlation, int samples,
                                        uint64_t seed)
{
    using R = Result<Propagation>;
    const size_t n = in.size();
    if (samples < 100)
        return R::failure("at least 100 samples");
    const Eigen::MatrixXd r = correlationOrIdentity(correlation, n);
    if (static_cast<size_t>(r.rows()) != n || static_cast<size_t>(r.cols()) != n)
        return R::failure("the correlation matrix does not match the inputs");
    Eigen::LLT<Eigen::MatrixXd> llt(r);
    if (llt.info() != Eigen::Success)
        return R::failure("the correlation matrix is not positive definite");
    const Eigen::MatrixXd L = llt.matrixL();
    std::mt19937_64 rng(seed);
    std::normal_distribution<double> normal(0.0, 1.0);
    std::vector<double> y;
    y.reserve(static_cast<size_t>(samples));
    int failed = 0;
    for (int s = 0; s < samples; ++s) {
        Eigen::VectorXd z(static_cast<Eigen::Index>(n));
        for (size_t i = 0; i < n; ++i)
            z(static_cast<Eigen::Index>(i)) = normal(rng);
        const Eigen::VectorXd c = L * z;
        std::vector<double> x(n);
        for (size_t i = 0; i < n; ++i) {
            const double zi = c(static_cast<Eigen::Index>(i));
            if (in[i].distribution == "rectangular")  // Gaussian copula: uniform on mean +- sqrt(3) u
                x[i] = in[i].mean + std::sqrt(3.0) * in[i].standard * (2.0 * phi(zi) - 1.0);
            else
                x[i] = in[i].mean + in[i].standard * zi;
        }
        auto v = f(x);
        if (v && std::isfinite(v.value()))
            y.push_back(v.value());
        else
            ++failed;
    }
    if (y.size() < static_cast<size_t>(samples) / 2)
        return R::failure("the model failed for most samples");
    Propagation p;
    p.method = "Monte Carlo (JCGM 101)";
    p.samples = static_cast<int>(y.size());
    double sum = 0.0;
    for (double v : y)
        sum += v;
    p.mean = sum / y.size();
    double ss = 0.0;
    for (double v : y)
        ss += (v - p.mean) * (v - p.mean);
    p.standard = std::sqrt(ss / (y.size() - 1));
    std::sort(y.begin(), y.end());
    const auto quantile = [&](double q) { return y[static_cast<size_t>(std::clamp(q * (y.size() - 1), 0.0, double(y.size() - 1)))]; };
    p.lower = quantile(0.5 * (1.0 - p.probability));
    p.upper = quantile(0.5 * (1.0 + p.probability));
    p.notes.push_back(correlation.size() == 0 ? "inputs uncorrelated (none stated)" : "with the stated input correlations (Gaussian copula)");
    if (failed > 0)
        p.notes.push_back(std::to_string(failed) + " samples where the model failed were left out");
    p.notes.push_back("the probabilistically symmetric interval of the sampled output");
    return p;
}

}  // namespace resonant::laboratory
