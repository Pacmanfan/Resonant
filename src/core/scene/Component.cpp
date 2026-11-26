#include "scene/Component.h"

#include <nlohmann/json.hpp>

namespace resonant {

// Defined by the modules that own component types. Registration is explicit
// because a static library drops self-registering objects nobody references.
namespace acoustics {
void registerAcousticComponents(ComponentRegistry& registry);
}

ComponentRegistry& ComponentRegistry::instance()
{
    static ComponentRegistry registry = [] {
        ComponentRegistry created;
        acoustics::registerAcousticComponents(created);
        return created;
    }();
    return registry;
}

void ComponentRegistry::add(std::string typeId, Factory factory)
{
    factories_[std::move(typeId)] = std::move(factory);
}

bool ComponentRegistry::knows(std::string_view typeId) const
{
    return factories_.find(typeId) != factories_.end();
}

Result<ComponentPtr> ComponentRegistry::create(std::string_view typeId, const nlohmann::json& data) const
{
    const auto it = factories_.find(typeId);
    if (it == factories_.end())
        return Result<ComponentPtr>::failure("unknown component type '" + std::string(typeId) + "'");
    return it->second(data);
}

}  // namespace resonant
