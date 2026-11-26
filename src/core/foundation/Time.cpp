#include "foundation/Time.h"

#include <chrono>
#include <ctime>

namespace resonant {

std::string utcNowIso8601()
{
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm utc{};
    gmtime_r(&now, &utc);
    char text[32];
    std::strftime(text, sizeof(text), "%Y-%m-%dT%H:%M:%SZ", &utc);
    return text;
}

}  // namespace resonant
