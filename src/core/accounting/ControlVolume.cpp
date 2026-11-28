#include "accounting/ControlVolume.h"

#include <algorithm>

namespace resonant::accounting {

using nlohmann::json;

std::string_view toString(SystemKind k)
{
    switch (k) {
    case SystemKind::Closed:
        return "closed";
    case SystemKind::Open:
        return "open";
    case SystemKind::ControlVolume:
        return "control-volume";
    case SystemKind::Object:
        return "object";
    case SystemKind::Assembly:
        return "assembly";
    case SystemKind::Subsystem:
        return "subsystem";
    }
    return "assembly";
}

std::string_view toString(Role r)
{
    switch (r) {
    case Role::Internal:
        return "internal";
    case Role::Input:
        return "input";
    case Role::Output:
        return "output";
    case Role::Dissipated:
        return "dissipated";
    case Role::Outside:
        return "outside";
    }
    return "outside";
}

bool AccountingSystem::contains(const std::string& entity) const
{
    return std::find(members.begin(), members.end(), entity) != members.end();
}

json AccountingSystem::toJson() const
{
    return {{"id", id}, {"name", name}, {"kind", std::string(toString(kind))}, {"members", members}, {"frame", frame}, {"description", description}};
}

Result<AccountingSystem> AccountingSystem::fromJson(const json& j)
{
    using R = Result<AccountingSystem>;
    if (!j.is_object())
        return R::failure("an accounting system is an object");
    AccountingSystem s;
    s.id = j.value("id", std::string());
    if (s.id.empty())
        return R::failure("an accounting system needs an id");
    s.name = j.value("name", s.id);
    const std::string kind = j.value("kind", std::string("assembly"));
    bool known = false;
    for (SystemKind k : {SystemKind::Closed, SystemKind::Open, SystemKind::ControlVolume, SystemKind::Object, SystemKind::Assembly, SystemKind::Subsystem})
        if (toString(k) == kind) {
            s.kind = k;
            known = true;
        }
    if (!known)
        return R::failure("unknown system kind '" + kind + "'");
    s.members = j.value("members", std::vector<std::string>{});
    s.frame = j.value("frame", std::string("world"));
    s.description = j.value("description", std::string());
    return s;
}

AccountingSystem wholeSystem(const Ledger& ledger)
{
    AccountingSystem s;
    s.id = "whole";
    s.name = "Whole system";
    s.kind = SystemKind::Assembly;
    s.description = "every modeled entity; sources, sinks and the environment outside";
    for (const Entity& e : ledger.entities)
        if (!e.external())
            s.members.push_back(e.id);
    return s;
}

Status validateFor(const AccountingSystem& system, const Ledger& ledger)
{
    if (system.members.empty())
        return Status::failure("the system '" + system.id + "' has no members");
    for (const std::string& m : system.members) {
        const Entity* e = ledger.entity(m);
        if (!e)
            return Status::failure("the system '" + system.id + "' names '" + m + "', which is not in the ledger");
        if (e->external())
            return Status::failure("the system '" + system.id + "' includes '" + m + "': a source, sink or the environment is never a member");
    }
    return Status::success();
}

Role classify(const Exchange& x, const Ledger& ledger, const AccountingSystem& s)
{
    const bool from = s.contains(x.from), to = s.contains(x.to);
    if (from && to)
        return Role::Internal;
    if (to)
        return Role::Input;
    if (from) {
        const Entity* e = ledger.entity(x.to);
        return e && e->kind == EntityKind::Sink ? Role::Dissipated : Role::Output;
    }
    return Role::Outside;
}

}  // namespace resonant::accounting
