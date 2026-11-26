#include "foundation/Types.h"

namespace resonant {

std::string_view toString(PhysicsDomain domain)
{
    switch (domain) {
    case PhysicsDomain::Acoustic: return "acoustic";
    case PhysicsDomain::Mechanical: return "mechanical";
    case PhysicsDomain::Electromagnetic: return "electromagnetic";
    }
    return "acoustic";
}

std::optional<PhysicsDomain> physicsDomainFromString(std::string_view text)
{
    if (text == "acoustic") return PhysicsDomain::Acoustic;
    if (text == "mechanical") return PhysicsDomain::Mechanical;
    if (text == "electromagnetic") return PhysicsDomain::Electromagnetic;
    return std::nullopt;
}

}  // namespace resonant
