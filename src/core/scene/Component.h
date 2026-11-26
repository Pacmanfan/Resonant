// A component is the domain-specific part of a scene object: the data that
// makes an object an acoustic emitter, an array, a target, and so on. The
// scene itself knows nothing about any physics domain; a domain module
// defines component classes and registers them here so projects can be
// loaded.
//
// Components are immutable once attached to an object. To change one, make
// a modified copy and set it (ObjectState::component), which is what makes
// object-state snapshots for undo cheap and safe.
#pragma once

#include "foundation/Result.h"

#include <nlohmann/json_fwd.hpp>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>

namespace resonant {

class Component {
public:
    virtual ~Component() = default;

    // Stable type identifier stored in project files ("acoustic.emitter").
    virtual std::string typeId() const = 0;
    virtual nlohmann::json toJson() const = 0;
    virtual bool equals(const Component& other) const = 0;
};

using ComponentPtr = std::shared_ptr<const Component>;

class ComponentRegistry {
public:
    using Factory = std::function<Result<ComponentPtr>(const nlohmann::json&)>;

    // The registry holding the component types of every core module.
    static ComponentRegistry& instance();

    void add(std::string typeId, Factory factory);
    bool knows(std::string_view typeId) const;
    Result<ComponentPtr> create(std::string_view typeId, const nlohmann::json& data) const;

private:
    std::map<std::string, Factory, std::less<>> factories_;
};

}  // namespace resonant
