#include "commands/Command.h"

#include <algorithm>

namespace resonant {

Status CompositeCommand::execute()
{
    for (size_t i = 0; i < commands_.size(); ++i) {
        if (Status status = commands_[i]->execute(); !status) {
            // Leave nothing half-applied.
            for (size_t j = i; j-- > 0;)
                [[maybe_unused]] const Status reverted = commands_[j]->undo();
            return status;
        }
    }
    return Status::success();
}

Status CompositeCommand::undo()
{
    for (size_t i = commands_.size(); i-- > 0;)
        if (Status status = commands_[i]->undo(); !status)
            return status;
    return Status::success();
}

Status CommandStack::push(std::unique_ptr<Command> command)
{
    if (!command)
        return Status::failure("no command");
    if (Status status = command->execute(); !status)
        return status;
    commands_.erase(commands_.begin() + static_cast<std::ptrdiff_t>(position_), commands_.end());
    if (cleanPosition_ > static_cast<std::ptrdiff_t>(position_))
        cleanPosition_ = -1;  // the saved state was in the discarded redo history
    commands_.push_back(std::move(command));
    position_ = commands_.size();
    notify();
    return Status::success();
}

std::string CommandStack::undoText() const
{
    return canUndo() ? commands_[position_ - 1]->description() : std::string();
}

std::string CommandStack::redoText() const
{
    return canRedo() ? commands_[position_]->description() : std::string();
}

Status CommandStack::undo()
{
    if (!canUndo())
        return Status::failure("nothing to undo");
    if (Status status = commands_[position_ - 1]->undo(); !status)
        return status;
    --position_;
    notify();
    return Status::success();
}

Status CommandStack::redo()
{
    if (!canRedo())
        return Status::failure("nothing to redo");
    if (Status status = commands_[position_]->execute(); !status)
        return status;
    ++position_;
    notify();
    return Status::success();
}

void CommandStack::clear()
{
    commands_.clear();
    position_ = 0;
    cleanPosition_ = 0;
    notify();
}

void CommandStack::markClean()
{
    cleanPosition_ = static_cast<std::ptrdiff_t>(position_);
    notify();
}

bool CommandStack::isClean() const
{
    return cleanPosition_ == static_cast<std::ptrdiff_t>(position_);
}

int CommandStack::addListener(Listener listener)
{
    const int handle = nextListener_++;
    listeners_.emplace_back(handle, std::move(listener));
    return handle;
}

void CommandStack::removeListener(int handle)
{
    std::erase_if(listeners_, [handle](const auto& entry) { return entry.first == handle; });
}

void CommandStack::notify()
{
    const auto listeners = listeners_;
    for (const auto& [handle, listener] : listeners)
        listener();
}

}  // namespace resonant
