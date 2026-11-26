#include "foundation/Id.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <random>

namespace resonant {

Id Id::generate()
{
    static std::mutex mutex;
    static std::mt19937_64 engine{[] {
        std::random_device device;
        std::seed_seq seed{device(), device(), device(), device()};
        return std::mt19937_64(seed);
    }()};

    std::uint64_t high = 0;
    std::uint64_t low = 0;
    {
        std::lock_guard lock(mutex);
        high = engine();
        low = engine();
    }
    // UUID version 4, variant 1.
    high = (high & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
    low = (low & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

    std::array<char, 37> text{};
    std::snprintf(text.data(), text.size(), "%08x-%04x-%04x-%04x-%012llx",
                  static_cast<unsigned>(high >> 32), static_cast<unsigned>((high >> 16) & 0xFFFF),
                  static_cast<unsigned>(high & 0xFFFF), static_cast<unsigned>(low >> 48),
                  static_cast<unsigned long long>(low & 0xFFFFFFFFFFFFULL));
    return Id(std::string(text.data()));
}

}  // namespace resonant
