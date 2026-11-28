#pragma once

// SHA-256 (FIPS 180-4), for the integrity of imported raw files (REQ-CHAT-20261126-023,
// RES-LAB-004): the digest of the original bytes is recorded with every import.

#include <string>
#include <string_view>

namespace resonant::laboratory {

// Lower-case hexadecimal digest.
std::string sha256Hex(std::string_view bytes);

}  // namespace resonant::laboratory
