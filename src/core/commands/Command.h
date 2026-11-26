// Undo / redo. Every change to a project's scene or materials made by the
// user goes through a Command pushed on a CommandStack: the command applies
// the change and knows how to revert it.
#pragma once

#include "foundation/Result.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace resonant {

class Command {
public:
    virtual ~Command() = default;

    // Short text for the Edit menu: "Move Box".
    virtual std::string description() const = 0;
    // Applies the change. Called once by push() and again by each redo.
    virtual Status execute() = 0;
    // Reverts what execute() did.
    virtual Status undo() = 0;
};

// Several commands applied (and undone, in reverse) as one step.
class CompositeCommand final : public Command {
public:
    explicit CompositeCommand(std::string description) : description_(std::move(description)) {}

    void add(std::unique_ptr<Command> command) { commands_.push_back(std::move(command)); }
    bool empty() const { return commands_.empty(); }

    std::string description() const override { return description_; }
    Status execute() override;
    Status undo() override;

private:
    std::string description_;
    std::vector<std::unique_ptr<Command>> commands_;
};

class CommandStack {
public:
    using Listener = std::function<void()>;

    // Executes the command; on success it becomes the newest undo step and
    // the redo history is discarded. On failure nothing is recorded.
    Status push(std::unique_ptr<Command> command);

    bool canUndo() const { return position_ > 0; }
    bool canRedo() const { return position_ < commands_.size(); }
    std::string undoText() const;
    std::string redoText() const;
    Status undo();
    Status redo();

    void clear();
    size_t size() const { return commands_.size(); }

    // The "saved" marker: isClean() is true when the stack is at the state
    // that was last marked clean.
    void markClean();
    bool isClean() const;

    // Called after every push, undo, redo, clear and markClean.
    int addListener(Listener listener);
    void removeListener(int handle);

private:
    void notify();

    std::vector<std::unique_ptr<Command>> commands_;
    size_t position_ = 0;                // number of applied commands
    std::ptrdiff_t cleanPosition_ = 0;   // -1 when the clean state is unreachable
    std::vector<std::pair<int, Listener>> listeners_;
    int nextListener_ = 1;
};

}  // namespace resonant
