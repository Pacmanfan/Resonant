#pragma once

// Reproducible random numbers and sampling plans (RES-OPT-017, RES-OPT-025).
//
// The generator is xoshiro256** seeded by SplitMix64 and the conversions to
// uniform and normal deviates are written out here (the standard library's
// distributions differ between implementations), so the same seed gives the
// same candidates on every platform and compiler.
#include <cstdint>
#include <vector>

namespace resonant::optimization {

class Random {
public:
    explicit Random(std::uint64_t seed = 1);
    std::uint64_t next();
    double uniform();                       // [0, 1)
    double uniform(double lo, double hi);   // [lo, hi)
    double normal();                        // standard normal (Box-Muller)
    int below(int n);                       // [0, n)
    // A derived generator for an independent stream (stream k of this seed).
    static Random stream(std::uint64_t seed, std::uint64_t k);

private:
    std::uint64_t s_[4];
    bool spare_ = false;
    double spareValue_ = 0.0;
};

// n points of a Latin hypercube in [0,1]^d: each axis is cut into n equal
// strata and every stratum holds exactly one point. Of `tries` random
// hypercubes the one with the largest smallest pairwise distance is kept.
std::vector<std::vector<double>> latinHypercube(int n, int d, Random& random, int tries = 10);
// Full-factorial grid with `levels[i]` values along axis i (cell centres of
// categorical / integer axes are handled by the design space mapping).
std::vector<std::vector<double>> fullFactorial(const std::vector<int>& levels);
std::vector<std::vector<double>> uniformSamples(int n, int d, Random& random);

}  // namespace resonant::optimization
