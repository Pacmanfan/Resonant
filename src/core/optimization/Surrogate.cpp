#include "optimization/Surrogate.h"

#include "optimization/Candidate.h"
#include "optimization/Problem.h"

#include <Eigen/Cholesky>
#include <Eigen/Dense>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <numeric>

namespace resonant::optimization {

namespace {

double squaredDistance(const std::vector<double>& a, const std::vector<double>& b)
{
    double s = 0.0;
    for (size_t i = 0; i < a.size() && i < b.size(); ++i)
        s += (a[i] - b[i]) * (a[i] - b[i]);
    return s;
}

// --- quadratic response surface -------------------------------------------------------------------

class Quadratic final : public SurrogateModel {
public:
    std::string kind() const override { return "quadratic"; }
    Status fit(const std::vector<std::vector<double>>& x, const std::vector<double>& y) override
    {
        if (x.empty())
            return Status::failure("no training data");
        d_ = x.front().size();
        const size_t full = 1 + 2 * d_ + d_ * (d_ - 1) / 2;
        cross_ = x.size() >= full + 1;
        const size_t terms = cross_ ? full : 1 + 2 * d_;
        if (x.size() < terms)
            return Status::failure("a quadratic surface over " + std::to_string(d_) + " variables needs at least " + std::to_string(terms) +
                                   " points");
        Eigen::MatrixXd a(x.size(), terms);
        Eigen::VectorXd b(x.size());
        for (size_t i = 0; i < x.size(); ++i) {
            a.row(i) = basis(x[i]).transpose();
            b(i) = y[i];
        }
        coefficients_ = a.colPivHouseholderQr().solve(b);
        return Status::success();
    }
    Prediction predict(const std::vector<double>& x) const override { return {basis(x).dot(coefficients_), 0.0, false}; }

private:
    Eigen::VectorXd basis(const std::vector<double>& x) const
    {
        const size_t terms = cross_ ? 1 + 2 * d_ + d_ * (d_ - 1) / 2 : 1 + 2 * d_;
        Eigen::VectorXd v(terms);
        size_t k = 0;
        v(k++) = 1.0;
        for (size_t i = 0; i < d_; ++i)
            v(k++) = x[i];
        for (size_t i = 0; i < d_; ++i)
            v(k++) = x[i] * x[i];
        if (cross_)
            for (size_t i = 0; i < d_; ++i)
                for (size_t j = i + 1; j < d_; ++j)
                    v(k++) = x[i] * x[j];
        return v;
    }
    size_t d_ = 0;
    bool cross_ = false;
    Eigen::VectorXd coefficients_;
};

// --- cubic RBF -------------------------------------------------------------------------------------

class Rbf final : public SurrogateModel {
public:
    std::string kind() const override { return "rbf"; }
    Status fit(const std::vector<std::vector<double>>& x, const std::vector<double>& y) override
    {
        if (x.empty())
            return Status::failure("no training data");
        x_ = x;
        const size_t n = x.size(), d = x.front().size();
        const size_t m = n + d + 1;
        Eigen::MatrixXd a = Eigen::MatrixXd::Zero(m, m);
        Eigen::VectorXd b = Eigen::VectorXd::Zero(m);
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = 0; j < n; ++j)
                a(i, j) = std::pow(std::sqrt(squaredDistance(x[i], x[j])), 3.0);
            a(i, n) = a(n, i) = 1.0;
            for (size_t k = 0; k < d; ++k)
                a(i, n + 1 + k) = a(n + 1 + k, i) = x[i][k];
            b(i) = y[i];
        }
        // A tiny ridge keeps duplicated points from making the system singular.
        for (size_t i = 0; i < n; ++i)
            a(i, i) += 1e-10;
        coefficients_ = a.completeOrthogonalDecomposition().solve(b);
        d_ = d;
        return Status::success();
    }
    Prediction predict(const std::vector<double>& x) const override
    {
        const size_t n = x_.size();
        double v = coefficients_(static_cast<Eigen::Index>(n));
        for (size_t i = 0; i < n; ++i)
            v += coefficients_(static_cast<Eigen::Index>(i)) * std::pow(std::sqrt(squaredDistance(x, x_[i])), 3.0);
        for (size_t k = 0; k < d_; ++k)
            v += coefficients_(static_cast<Eigen::Index>(n + 1 + k)) * x[k];
        return {v, 0.0, false};
    }

private:
    std::vector<std::vector<double>> x_;
    size_t d_ = 0;
    Eigen::VectorXd coefficients_;
};

// --- Gaussian process ----------------------------------------------------------------------------------

class Gp final : public SurrogateModel {
public:
    std::string kind() const override { return "gaussian-process"; }
    Status fit(const std::vector<std::vector<double>>& x, const std::vector<double>& y) override
    {
        if (x.size() < 2)
            return Status::failure("a Gaussian process needs at least two points");
        x_ = x;
        const size_t n = x.size();
        mean_ = std::accumulate(y.begin(), y.end(), 0.0) / n;
        double var = 0.0;
        for (double v : y)
            var += (v - mean_) * (v - mean_);
        scale_ = std::sqrt(var / n);
        if (!(scale_ > 0.0))
            scale_ = 1.0;
        Eigen::VectorXd t(n);
        for (size_t i = 0; i < n; ++i)
            t(i) = (y[i] - mean_) / scale_;
        double bestLikelihood = -std::numeric_limits<double>::infinity();
        // Grid of length scales (unit cube) and relative noise levels.
        for (double length : {0.05, 0.08, 0.12, 0.18, 0.25, 0.35, 0.5, 0.7, 1.0, 1.5, 2.5}) {
            for (double noise : {1e-8, 1e-6, 1e-4, 1e-3, 1e-2, 5e-2}) {
                Eigen::MatrixXd k = kernelMatrix(length);
                k.diagonal().array() += noise;
                Eigen::LLT<Eigen::MatrixXd> llt(k);
                if (llt.info() != Eigen::Success)
                    continue;
                const Eigen::VectorXd alpha = llt.solve(t);
                const Eigen::MatrixXd l = llt.matrixL();
                double logDet = 0.0;
                for (size_t i = 0; i < n; ++i)
                    logDet += 2.0 * std::log(l(i, i));
                const double likelihood = -0.5 * t.dot(alpha) - 0.5 * logDet - 0.5 * n * std::log(2.0 * std::numbers::pi);
                if (likelihood > bestLikelihood) {
                    bestLikelihood = likelihood;
                    length_ = length;
                    noise_ = noise;
                    alpha_ = alpha;
                    llt_ = llt;
                }
            }
        }
        if (!std::isfinite(bestLikelihood))
            return Status::failure("the Gaussian-process covariance could not be factorized");
        return Status::success();
    }
    Prediction predict(const std::vector<double>& x) const override
    {
        const size_t n = x_.size();
        Eigen::VectorXd k(n);
        for (size_t i = 0; i < n; ++i)
            k(i) = kernel(x, x_[i], length_);
        const double m = k.dot(alpha_);
        const Eigen::VectorXd v = llt_.matrixL().solve(k);
        const double var = std::max(0.0, 1.0 + noise_ - v.squaredNorm());
        return {mean_ + scale_ * m, scale_ * std::sqrt(var), true};
    }

private:
    static double kernel(const std::vector<double>& a, const std::vector<double>& b, double length)
    {
        return std::exp(-0.5 * squaredDistance(a, b) / (length * length));
    }
    Eigen::MatrixXd kernelMatrix(double length) const
    {
        const size_t n = x_.size();
        Eigen::MatrixXd k(n, n);
        for (size_t i = 0; i < n; ++i)
            for (size_t j = 0; j <= i; ++j)
                k(i, j) = k(j, i) = kernel(x_[i], x_[j], length);
        return k;
    }
    std::vector<std::vector<double>> x_;
    double mean_ = 0.0, scale_ = 1.0, length_ = 0.3, noise_ = 1e-6;
    Eigen::VectorXd alpha_;
    Eigen::LLT<Eigen::MatrixXd> llt_;
};

}  // namespace

std::unique_ptr<SurrogateModel> makeSurrogate(const std::string& kind)
{
    if (kind == "quadratic")
        return std::make_unique<Quadratic>();
    if (kind == "rbf")
        return std::make_unique<Rbf>();
    if (kind == "gaussian-process")
        return std::make_unique<Gp>();
    return nullptr;
}

std::vector<std::string> surrogateKinds() { return {"quadratic", "rbf", "gaussian-process"}; }

double expectedImprovement(const Prediction& p, double best)
{
    if (!p.hasUncertainty || !(p.stddev > 0.0))
        return std::max(0.0, best - p.mean);
    const double z = (best - p.mean) / p.stddev;
    const double pdf = std::exp(-0.5 * z * z) / std::sqrt(2.0 * std::numbers::pi);
    const double cdf = 0.5 * std::erfc(-z / std::sqrt(2.0));
    return (best - p.mean) * cdf + p.stddev * pdf;
}

Result<SurrogateStudy> fitSurrogate(const OptimizationProblem& problem, const std::vector<Candidate>& candidates, size_t objectiveIndex,
                                    const std::string& kind, const std::string& modelId)
{
    using R = Result<SurrogateStudy>;
    if (objectiveIndex >= problem.objectives.size())
        return R::failure("no such objective");
    SurrogateStudy study;
    study.model = makeSurrogate(kind);
    if (!study.model)
        return R::failure("unknown surrogate kind '" + kind + "'");
    const std::string model = modelId.empty() ? problem.modelId : modelId;
    for (const Candidate& c : candidates) {
        if (!c.evaluated() || c.evaluation.model.modelId != model || objectiveIndex >= c.objectiveValues.size() ||
            !std::isfinite(c.objectiveValues[objectiveIndex]))
            continue;
        study.trainingInputs.push_back(problem.space.toUnit(c.point));
        study.trainingOutputs.push_back(c.objectiveValues[objectiveIndex]);
    }
    if (Status s = study.model->fit(study.trainingInputs, study.trainingOutputs); !s)
        return R::failure(s.message());
    SurrogateInfo& info = study.info;
    info.kind = kind;
    info.trainingPoints = study.trainingInputs.size();
    for (const DesignVariable* v : problem.space.freeVariables())
        info.inputs.push_back(v->id);
    info.output = problem.objectives[objectiveIndex].quantity + " (model " + model + ")";
    double train = 0.0;
    for (size_t i = 0; i < study.trainingInputs.size(); ++i) {
        const double e = study.model->predict(study.trainingInputs[i]).mean - study.trainingOutputs[i];
        train += e * e;
    }
    info.trainingRmse = std::sqrt(train / std::max<size_t>(1, study.trainingInputs.size()));
    // Leave-one-out validation.
    double loo = 0.0;
    size_t looCount = 0;
    for (size_t i = 0; i < study.trainingInputs.size() && study.trainingInputs.size() <= 400; ++i) {
        auto x = study.trainingInputs;
        auto y = study.trainingOutputs;
        x.erase(x.begin() + static_cast<long>(i));
        y.erase(y.begin() + static_cast<long>(i));
        auto m = makeSurrogate(kind);
        if (!m->fit(x, y))
            continue;
        const double e = m->predict(study.trainingInputs[i]).mean - study.trainingOutputs[i];
        loo += e * e;
        ++looCount;
    }
    info.validationRmse = looCount ? std::sqrt(loo / looCount) : std::numeric_limits<double>::quiet_NaN();
    info.notes.push_back("a surrogate prediction is not a simulation result; evaluate promising points with the physical model");
    if (looCount == 0)
        info.notes.push_back("too few points for leave-one-out validation");
    return study;
}

std::vector<size_t> mostInformative(const SurrogateStudy& study, const std::vector<std::vector<double>>& pool, size_t k)
{
    std::vector<std::pair<double, size_t>> score;
    for (size_t i = 0; i < pool.size(); ++i) {
        const Prediction p = study.model->predict(pool[i]);
        double s;
        if (p.hasUncertainty) {
            s = p.stddev;
        } else {
            s = std::numeric_limits<double>::infinity();
            for (const auto& t : study.trainingInputs)
                s = std::min(s, squaredDistance(pool[i], t));
        }
        score.emplace_back(s, i);
    }
    std::stable_sort(score.begin(), score.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    std::vector<size_t> out;
    for (size_t i = 0; i < k && i < score.size(); ++i)
        out.push_back(score[i].second);
    return out;
}

}  // namespace resonant::optimization
