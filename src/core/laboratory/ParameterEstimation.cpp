#include "laboratory/ParameterEstimation.h"

#include "laboratory/ErrorBudget.h"
#include "simulation/Job.h"

#include <cmath>
#include <sstream>

namespace resonant::laboratory {

using nlohmann::json;

namespace {

std::string number(double v)
{
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.3g", v);
    return buf;
}

}  // namespace

bool EstimationResult::identified() const
{
    for (const ParameterEstimate& p : parameters)
        if (p.identifiability != "identified")
            return false;
    return !parameters.empty();
}

json EstimationResult::toJson() const
{
    json j{{"method", method},       {"weighted", weighted}, {"converged", converged}, {"iterations", iterations}, {"evaluations", evaluations},
           {"degreesOfFreedom", degreesOfFreedom}, {"chiSquare", chiSquare}, {"reducedChiSquare", reducedChiSquare},
           {"conditionNumber", conditionNumber}, {"singularValues", singularValues}, {"fitted", fitted}, {"residuals", residuals}, {"notes", notes},
           {"identified", identified()}};
    j["parameters"] = json::array();
    for (const ParameterEstimate& p : parameters)
        j["parameters"].push_back({{"id", p.id}, {"unit", p.unit}, {"initial", p.initial}, {"estimate", p.estimate}, {"standardError", p.standardError},
                                   {"lower", p.lower}, {"upper", p.upper}, {"relativeError", p.relativeError}, {"identifiability", p.identifiability},
                                   {"atBound", p.atBound}});
    const auto matrix = [](const Eigen::MatrixXd& m) {
        json out = json::array();
        for (Eigen::Index r = 0; r < m.rows(); ++r) {
            json row = json::array();
            for (Eigen::Index c = 0; c < m.cols(); ++c)
                row.push_back(std::isfinite(m(r, c)) ? json(m(r, c)) : json(nullptr));
            out.push_back(row);
        }
        return out;
    };
    j["covariance"] = matrix(covariance);
    j["correlation"] = matrix(correlation);
    return j;
}

Result<EstimationResult> estimateParameters(const std::vector<EstimatedParameter>& params, const std::vector<Observation>& obs, const Predictor& predict,
                                            const EstimationOptions& o, JobContext* ctx)
{
    using R = Result<EstimationResult>;
    const size_t np = params.size();
    if (np == 0)
        return R::failure("no parameter to estimate");
    if (obs.empty())
        return R::failure("no observation");
    bool weighted = true;
    size_t nres = 0;
    for (const Observation& ob : obs) {
        if (ob.values.empty())
            return R::failure("the observation '" + ob.dataset + "' has no values");
        if (!ob.standard.empty() && ob.standard.size() != ob.values.size())
            return R::failure("the observation '" + ob.dataset + "' has uncertainties for some values only");
        for (double s : ob.standard)
            if (!(s > 0.0))
                return R::failure("the observation '" + ob.dataset + "' has a non-positive uncertainty");
        weighted = weighted && !ob.standard.empty();
        nres += ob.values.size();
    }
    if (!weighted)
        for (const Observation& ob : obs)
            if (!ob.standard.empty())
                return R::failure("either every observation states its uncertainties or none does");
    // Scaled variables theta = p / s.
    std::vector<double> scale(np);
    Eigen::VectorXd theta(static_cast<Eigen::Index>(np));
    for (size_t i = 0; i < np; ++i) {
        const auto& p = params[i];
        if (!(p.lower <= p.initial && p.initial <= p.upper))
            return R::failure("the initial value of '" + p.id + "' lies outside its bounds");
        scale[i] = std::abs(p.initial) > 0.0 ? std::abs(p.initial) : 1.0;
        theta(static_cast<Eigen::Index>(i)) = p.initial / scale[i];
    }
    size_t priors = 0;
    for (const auto& p : params)
        if (p.priorMean && p.priorStandard && *p.priorStandard > 0.0)
            ++priors;
    const size_t nr = nres + priors;
    if (nr < np)
        return R::failure("fewer observations (" + std::to_string(nr) + ") than parameters (" + std::to_string(np) + ")");
    EstimationResult out;
    out.weighted = weighted;
    out.method = std::string(priors > 0 ? "maximum a posteriori (Gaussian priors), " : "") + (weighted ? "weighted " : "") +
                 "nonlinear least squares (Levenberg-Marquardt)";
    const auto values = [&](const Eigen::VectorXd& t) {
        std::vector<double> p(np);
        for (size_t i = 0; i < np; ++i)
            p[i] = std::clamp(t(static_cast<Eigen::Index>(i)) * scale[i], params[i].lower, params[i].upper);
        return p;
    };
    const auto residuals = [&](const Eigen::VectorXd& t, Eigen::VectorXd& r, std::vector<std::vector<double>>* fitted) -> Status {
        const std::vector<double> p = values(t);
        r.resize(static_cast<Eigen::Index>(nr));
        Eigen::Index k = 0;
        if (fitted)
            fitted->clear();
        for (size_t j = 0; j < obs.size(); ++j) {
            auto y = predict(p, j);
            ++out.evaluations;
            if (!y)
                return Status::failure("the model failed: " + y.message());
            if (y.value().size() != obs[j].values.size())
                return Status::failure("the model predicted " + std::to_string(y.value().size()) + " values for " + std::to_string(obs[j].values.size()) +
                                       " observations");
            for (size_t i = 0; i < obs[j].values.size(); ++i) {
                const double s = weighted ? obs[j].standard[i] : 1.0;
                const double v = (obs[j].values[i] - y.value()[i]) / s;
                if (!std::isfinite(v))
                    return Status::failure("a non-finite residual (an observation or the prediction is not finite)");
                r(k++) = v;
            }
            if (fitted)
                fitted->push_back(y.value());
        }
        for (size_t i = 0; i < np; ++i)
            if (params[i].priorMean && params[i].priorStandard && *params[i].priorStandard > 0.0)
                r(k++) = (p[i] - *params[i].priorMean) / *params[i].priorStandard;
        return Status::success();
    };
    const auto jacobian = [&](const Eigen::VectorXd& t, const Eigen::VectorXd& r0, Eigen::MatrixXd& J) -> Status {
        J.resize(static_cast<Eigen::Index>(nr), static_cast<Eigen::Index>(np));
        for (size_t i = 0; i < np; ++i) {
            Eigen::VectorXd tp = t;
            double h = o.relativeStep * std::max(1.0, std::abs(t(static_cast<Eigen::Index>(i))));
            // Step away from an active upper bound.
            if (t(static_cast<Eigen::Index>(i)) * scale[i] + h * scale[i] > params[i].upper)
                h = -h;
            tp(static_cast<Eigen::Index>(i)) += h;
            Eigen::VectorXd r1;
            if (Status st = residuals(tp, r1, nullptr); !st) {
                // The other side, before giving up (a bound or a model limit on this side).
                tp(static_cast<Eigen::Index>(i)) -= 2.0 * h;
                h = -h;
                if (Status st2 = residuals(tp, r1, nullptr); !st2)
                    return st2;
            }
            // r = y - f, so d r / d theta = -d f / d theta: J is of f.
            J.col(static_cast<Eigen::Index>(i)) = -(r1 - r0) / h;
        }
        return Status::success();
    };
    Eigen::VectorXd r;
    if (Status st = residuals(theta, r, nullptr); !st)
        return R::failure(st.message());
    double cost = r.squaredNorm(), lambda = 1e-3;
    Eigen::MatrixXd J;
    for (int it = 0; it < o.maxIterations; ++it) {
        if (ctx && ctx->cancelRequested())
            return R::failure("cancelled");
        out.iterations = it + 1;
        if (Status st = jacobian(theta, r, J); !st)
            return R::failure(st.message());
        const Eigen::MatrixXd A = J.transpose() * J;
        const Eigen::VectorXd g = J.transpose() * r;  // the step solves A d = J^T r (f moves toward y)
        bool improved = false;
        for (int tries = 0; tries < 20; ++tries) {
            Eigen::MatrixXd Al = A;
            for (Eigen::Index d = 0; d < Al.rows(); ++d)
                Al(d, d) += lambda * std::max(A(d, d), 1e-12);
            const Eigen::VectorXd step = Al.ldlt().solve(g);
            Eigen::VectorXd tn = theta + step;
            for (size_t i = 0; i < np; ++i)
                tn(static_cast<Eigen::Index>(i)) = std::clamp(tn(static_cast<Eigen::Index>(i)), params[i].lower / scale[i], params[i].upper / scale[i]);
            Eigen::VectorXd rn;
            if (!residuals(tn, rn, nullptr)) {
                lambda *= 10.0;
                continue;
            }
            const double cn = rn.squaredNorm();
            if (cn < cost) {
                const double change = (cost - cn) / std::max(cost, 1e-300);
                theta = tn;
                r = rn;
                cost = cn;
                lambda = std::max(lambda / 10.0, 1e-12);
                improved = true;
                if (change < o.tolerance || step.norm() < 1e-12 * std::max(1.0, theta.norm()))
                    out.converged = true;
                break;
            }
            lambda *= 10.0;
        }
        if (!improved) {
            out.converged = true;  // no step lowers the cost: a minimum (to the finite-difference resolution)
            break;
        }
        if (out.converged)
            break;
        if (ctx)
            ctx->reportProgress(static_cast<double>(it + 1) / o.maxIterations, "fitting");
    }
    if (!out.converged)
        out.notes.push_back("not converged in " + std::to_string(o.maxIterations) + " iterations");
    // Statistics at the estimate.
    if (Status st = jacobian(theta, r, J); !st)
        return R::failure(st.message());
    std::vector<std::vector<double>> fitted;
    Eigen::VectorXd rf;
    (void)residuals(theta, rf, &fitted);
    out.fitted = fitted;
    {
        Eigen::Index k = 0;
        for (const Observation& ob : obs) {
            std::vector<double> rr;
            for (size_t i = 0; i < ob.values.size(); ++i)
                rr.push_back(rf(k++));
            out.residuals.push_back(rr);
        }
    }
    out.chiSquare = cost;
    out.degreesOfFreedom = static_cast<int>(nr) - static_cast<int>(np);
    out.reducedChiSquare = out.degreesOfFreedom > 0 ? cost / out.degreesOfFreedom : 0.0;
    Eigen::JacobiSVD<Eigen::MatrixXd> svd(J, Eigen::ComputeThinU | Eigen::ComputeThinV);
    const Eigen::VectorXd sv = svd.singularValues();
    for (Eigen::Index i = 0; i < sv.size(); ++i)
        out.singularValues.push_back(sv(i));
    const double smax = sv.size() ? sv(0) : 0.0, smin = sv.size() ? sv(sv.size() - 1) : 0.0;
    out.conditionNumber = smin > 0.0 ? smax / smin : INFINITY;
    // Covariance of theta: (J^T J)^+ (pseudo-inverse), scaled by the residual variance when unweighted.
    Eigen::MatrixXd Ct = Eigen::MatrixXd::Zero(static_cast<Eigen::Index>(np), static_cast<Eigen::Index>(np));
    for (Eigen::Index i = 0; i < sv.size(); ++i)
        if (sv(i) > 1e-12 * smax)
            Ct += svd.matrixV().col(i) * svd.matrixV().col(i).transpose() / (sv(i) * sv(i));
    const bool singular = !(smin > 1e-6 * smax);
    if (!weighted && out.degreesOfFreedom > 0) {
        Ct *= out.reducedChiSquare;
        out.notes.push_back("no uncertainties were stated: the fit is unweighted and the covariance is scaled by the residual variance");
    }
    if (weighted && out.reducedChiSquare > 3.0)
        out.notes.push_back("the reduced chi-square is " + number(out.reducedChiSquare) +
                            ": the model does not explain the data within their uncertainties (or the uncertainties are understated)");
    out.covariance.resize(static_cast<Eigen::Index>(np), static_cast<Eigen::Index>(np));
    for (size_t a = 0; a < np; ++a)
        for (size_t b = 0; b < np; ++b)
            out.covariance(static_cast<Eigen::Index>(a), static_cast<Eigen::Index>(b)) =
                Ct(static_cast<Eigen::Index>(a), static_cast<Eigen::Index>(b)) * scale[a] * scale[b];
    out.correlation = Eigen::MatrixXd::Identity(static_cast<Eigen::Index>(np), static_cast<Eigen::Index>(np));
    for (size_t a = 0; a < np; ++a)
        for (size_t b = 0; b < np; ++b) {
            const double d = std::sqrt(out.covariance(static_cast<Eigen::Index>(a), static_cast<Eigen::Index>(a)) *
                                       out.covariance(static_cast<Eigen::Index>(b), static_cast<Eigen::Index>(b)));
            out.correlation(static_cast<Eigen::Index>(a), static_cast<Eigen::Index>(b)) =
                d > 0.0 ? out.covariance(static_cast<Eigen::Index>(a), static_cast<Eigen::Index>(b)) / d : (a == b ? 1.0 : NAN);
        }
    const double k = coverageFactor(out.degreesOfFreedom > 0 ? out.degreesOfFreedom : 0.0, o.coverage);
    const std::vector<double> p = values(theta);
    const Eigen::VectorXd weakest = sv.size() ? svd.matrixV().col(sv.size() - 1) : Eigen::VectorXd();
    for (size_t i = 0; i < np; ++i) {
        ParameterEstimate e;
        e.id = params[i].id;
        e.unit = params[i].unit;
        e.initial = params[i].initial;
        e.estimate = p[i];
        e.standardError = std::sqrt(std::max(0.0, out.covariance(static_cast<Eigen::Index>(i), static_cast<Eigen::Index>(i))));
        e.lower = e.estimate - k * e.standardError;
        e.upper = e.estimate + k * e.standardError;
        e.relativeError = e.estimate != 0.0 ? e.standardError / std::abs(e.estimate) : INFINITY;
        e.atBound = p[i] <= params[i].lower || p[i] >= params[i].upper;
        double maxCorr = 0.0;
        for (size_t b = 0; b < np; ++b)
            if (b != i && std::isfinite(out.correlation(static_cast<Eigen::Index>(i), static_cast<Eigen::Index>(b))))
                maxCorr = std::max(maxCorr, std::abs(out.correlation(static_cast<Eigen::Index>(i), static_cast<Eigen::Index>(b))));
        const bool loadsWeakest = singular && weakest.size() && std::abs(weakest(static_cast<Eigen::Index>(i))) > 0.3;
        if (loadsWeakest || e.relativeError > 1.0)
            e.identifiability = "not identifiable";
        else if (e.relativeError > 0.1 || (maxCorr > 0.99 && e.relativeError > 0.01))
            e.identifiability = "poorly identified";
        else
            e.identifiability = "identified";
        if (e.atBound)
            out.notes.push_back("'" + e.id + "' ended at a bound: its interval is not reliable");
        out.parameters.push_back(e);
    }
    // The combinations the data determine (and those they do not).
    for (size_t a = 0; a < np; ++a)
        for (size_t b = a + 1; b < np; ++b) {
            const double rho = out.correlation(static_cast<Eigen::Index>(a), static_cast<Eigen::Index>(b));
            if (std::isfinite(rho) && std::abs(rho) > 0.95)
                out.notes.push_back("'" + params[a].id + "' and '" + params[b].id + "' are correlated (" + number(rho) +
                                    "): the data determine a combination of them, not each one");
        }
    if (singular && weakest.size()) {
        std::ostringstream s;
        s << "the data hardly change along";
        for (size_t i = 0; i < np; ++i)
            if (std::abs(weakest(static_cast<Eigen::Index>(i))) > 0.1)
                s << " " << (weakest(static_cast<Eigen::Index>(i)) >= 0 ? "+" : "") << number(weakest(static_cast<Eigen::Index>(i))) << " " << params[i].id;
        s << " (relative changes): many parameter combinations explain them equally well";
        out.notes.push_back(s.str());
    }
    return out;
}

}  // namespace resonant::laboratory
