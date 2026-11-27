// One undoable change from a changed copy of a project (a script's or a
// plugin's result, ADR-000215): the differences between the live project and
// the copy become a composite of the ordinary scene, material and extension
// commands, so undo restores the project exactly and every listener sees
// ordinary scene events.
#pragma once

#include "commands/Command.h"
#include "foundation/Result.h"

#include <memory>
#include <string>
#include <vector>

namespace resonant {

class Project;

struct ProjectChange {
    std::unique_ptr<CompositeCommand> command;  // empty composite when nothing changed
    int removed = 0, added = 0, moved = 0, changed = 0, extensions = 0;
    bool materials = false;
    std::vector<std::string> notApplied;  // differences there is no command for (reported, not applied)
    bool empty() const { return removed + added + moved + changed + extensions == 0 && !materials; }
};

// Fails (and changes nothing) when the copy cannot be expressed as such a
// change, for example when an existing object was moved under a new one.
Result<ProjectChange> projectChangeCommand(Project& live, const Project& changed, const std::string& description);

}  // namespace resonant
