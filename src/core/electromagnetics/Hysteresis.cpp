#include "electromagnetics/Hysteresis.h"

#include <cmath>

namespace resonant::em {

Status PreisachParameters::validate() const
{
    if (!(saturationMagnetization > 0.0) || !(saturationField > 0.0))
        return Status::failure("the Preisach model needs a positive saturation magnetization and field");
    if (reversibleSusceptibility < 0.0)
        return Status::failure("the reversible susceptibility must not be negative");
    if (levels < 4 || levels > 2000)
        return Status::failure("between 4 and 2000 levels");
    if (density != "uniform" && density != "gaussian")
        return Status::failure("density must be uniform or gaussian");
    if (density == "gaussian" && (!(coerciveSpread > 0.0) || !(interactionSpread > 0.0) || coercivity < 0.0))
        return Status::failure("a gaussian density needs a coercivity and positive spreads");
    return Status::success();
}

Result<PreisachModel> PreisachModel::create(const PreisachParameters& p)
{
    if (Status v = p.validate(); !v)
        return Result<PreisachModel>::failure(v.message());
    PreisachModel m;
    m.p_ = p;
    const int N = p.levels;
    const double d = 2.0 * p.saturationField / N;
    double total = 0.0;
    for (int i = 0; i < N; ++i)
        for (int j = 0; j <= i; ++j) {
            const double a = -p.saturationField + (i + 0.5) * d, b = -p.saturationField + (j + 0.5) * d;
            double w = i == j ? 0.5 : 1.0;  // the diagonal cell is half inside the triangle alpha >= beta
            if (p.density == "gaussian") {
                const double hc = 0.5 * (a - b), hu = 0.5 * (a + b);
                w *= std::exp(-0.5 * std::pow((hc - p.coercivity) / p.coerciveSpread, 2)) * std::exp(-0.5 * std::pow(hu / p.interactionSpread, 2));
            }
            m.alpha_.push_back(a);
            m.beta_.push_back(b);
            m.weight_.push_back(w);
            total += w;
        }
    for (double& w : m.weight_)
        w /= total;
    m.reset();
    return m;
}

void PreisachModel::reset()
{
    state_.assign(alpha_.size(), -1);
    H_ = -p_.saturationField;
    memory_ = {H_};
}

double PreisachModel::apply(double H)
{
    for (size_t r = 0; r < state_.size(); ++r) {
        if (H >= alpha_[r])
            state_[r] = 1;
        else if (H <= beta_[r])
            state_[r] = -1;
    }
    // The staircase memory: a new extremum wipes out every earlier extremum it exceeds.
    if (H != H_) {
        const bool rising = H > H_;
        if (memory_.size() >= 2 && ((rising && memory_.back() < H_) || (!rising && memory_.back() > H_)))
            memory_.push_back(H_);  // H_ was a turning point
        else if (memory_.size() < 2 && memory_.back() != H_)
            memory_.push_back(H_);
        while (memory_.size() >= 2 && ((rising && H >= memory_[memory_.size() - 2]) || (!rising && H <= memory_[memory_.size() - 2])))
            memory_.erase(memory_.end() - 2, memory_.end());
        if (memory_.empty())
            memory_.push_back(rising ? p_.saturationField : -p_.saturationField);
    }
    H_ = H;
    return magnetization();
}

double PreisachModel::irreversibleFraction() const
{
    double s = 0.0;
    for (size_t r = 0; r < state_.size(); ++r)
        s += weight_[r] * state_[r];
    return s;
}

double PreisachModel::magnetization() const
{
    return p_.saturationMagnetization * irreversibleFraction() + p_.reversibleSusceptibility * H_;
}

}  // namespace resonant::em
