#include "optimization/Random.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace resonant::optimization {

namespace {
std::uint64_t splitmix(std::uint64_t& x)
{
    std::uint64_t z = (x += 0x9e3779b97f4a7c15ull);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
}
std::uint64_t rotl(std::uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }
}  // namespace

Random::Random(std::uint64_t seed)
{
    std::uint64_t x = seed;
    for (auto& s : s_)
        s = splitmix(x);
}

Random Random::stream(std::uint64_t seed, std::uint64_t k)
{
    std::uint64_t x = seed ^ (0xd1b54a32d192ed03ull * (k + 1));
    return Random(splitmix(x));
}

std::uint64_t Random::next()
{
    const std::uint64_t result = rotl(s_[1] * 5, 7) * 9;
    const std::uint64_t t = s_[1] << 17;
    s_[2] ^= s_[0];
    s_[3] ^= s_[1];
    s_[1] ^= s_[2];
    s_[0] ^= s_[3];
    s_[2] ^= t;
    s_[3] = rotl(s_[3], 45);
    return result;
}

double Random::uniform() { return static_cast<double>(next() >> 11) * 0x1.0p-53; }

double Random::uniform(double lo, double hi) { return lo + (hi - lo) * uniform(); }

double Random::normal()
{
    if (spare_) {
        spare_ = false;
        return spareValue_;
    }
    double u1 = uniform();
    while (u1 <= 0.0)
        u1 = uniform();
    const double u2 = uniform();
    const double r = std::sqrt(-2.0 * std::log(u1));
    spareValue_ = r * std::sin(2.0 * std::numbers::pi * u2);
    spare_ = true;
    return r * std::cos(2.0 * std::numbers::pi * u2);
}

int Random::below(int n)
{
    if (n <= 1)
        return 0;
    return static_cast<int>(next() % static_cast<std::uint64_t>(n));
}

std::vector<std::vector<double>> uniformSamples(int n, int d, Random& random)
{
    std::vector<std::vector<double>> out(static_cast<size_t>(std::max(0, n)), std::vector<double>(static_cast<size_t>(d)));
    for (auto& p : out)
        for (double& x : p)
            x = random.uniform();
    return out;
}

std::vector<std::vector<double>> latinHypercube(int n, int d, Random& random, int tries)
{
    std::vector<std::vector<double>> best;
    double bestDistance = -1.0;
    for (int t = 0; t < std::max(1, tries); ++t) {
        std::vector<std::vector<double>> pts(static_cast<size_t>(n), std::vector<double>(static_cast<size_t>(d)));
        for (int k = 0; k < d; ++k) {
            std::vector<int> perm(static_cast<size_t>(n));
            for (int i = 0; i < n; ++i)
                perm[static_cast<size_t>(i)] = i;
            for (int i = n - 1; i > 0; --i)  // Fisher-Yates
                std::swap(perm[static_cast<size_t>(i)], perm[static_cast<size_t>(random.below(i + 1))]);
            for (int i = 0; i < n; ++i)
                pts[static_cast<size_t>(i)][static_cast<size_t>(k)] = (perm[static_cast<size_t>(i)] + random.uniform()) / n;
        }
        double smallest = std::numeric_limits<double>::infinity();
        for (int i = 0; i < n; ++i)
            for (int j = i + 1; j < n; ++j) {
                double s = 0.0;
                for (int k = 0; k < d; ++k) {
                    const double e = pts[static_cast<size_t>(i)][static_cast<size_t>(k)] - pts[static_cast<size_t>(j)][static_cast<size_t>(k)];
                    s += e * e;
                }
                smallest = std::min(smallest, s);
            }
        if (smallest > bestDistance) {
            bestDistance = smallest;
            best = std::move(pts);
        }
    }
    return best;
}

std::vector<std::vector<double>> fullFactorial(const std::vector<int>& levels)
{
    std::vector<std::vector<double>> out;
    if (levels.empty())
        return out;
    size_t total = 1;
    for (int l : levels)
        total *= static_cast<size_t>(std::max(1, l));
    out.reserve(total);
    std::vector<int> index(levels.size(), 0);
    for (size_t n = 0; n < total; ++n) {
        std::vector<double> p(levels.size());
        for (size_t k = 0; k < levels.size(); ++k) {
            const int l = std::max(1, levels[k]);
            p[k] = l == 1 ? 0.5 : static_cast<double>(index[k]) / (l - 1);
        }
        out.push_back(std::move(p));
        for (size_t k = 0; k < levels.size(); ++k) {
            if (++index[k] < std::max(1, levels[k]))
                break;
            index[k] = 0;
        }
    }
    return out;
}

}  // namespace resonant::optimization
