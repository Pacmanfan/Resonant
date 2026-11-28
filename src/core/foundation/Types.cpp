#include "foundation/Types.h"

namespace resonant {

std::string_view toString(PhysicsDomain domain)
{
    switch (domain) {
    case PhysicsDomain::Acoustic: return "acoustic";
    case PhysicsDomain::Mechanical: return "mechanical";
    case PhysicsDomain::Electromagnetic: return "electromagnetic";
    case PhysicsDomain::Thermal: return "thermal";
    }
    return "acoustic";
}

std::optional<PhysicsDomain> physicsDomainFromString(std::string_view text)
{
    if (text == "acoustic") return PhysicsDomain::Acoustic;
    if (text == "mechanical") return PhysicsDomain::Mechanical;
    if (text == "electromagnetic") return PhysicsDomain::Electromagnetic;
    if (text == "thermal") return PhysicsDomain::Thermal;
    return std::nullopt;
}

}  // namespace resonant
