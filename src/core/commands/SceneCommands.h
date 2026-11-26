// The undoable operations on a scene and on a material library.
#pragma once

#include "commands/Command.h"
#include "materials/Material.h"
#include "scene/Scene.h"

namespace resonant {

// Inserts an object (with its subtree). Used for create, duplicate, paste.
class InsertObjectCommand final : public Command {
public:
    InsertObjectCommand(Scene& scene, std::unique_ptr<SceneObject> object, Id parentId = Id(), int index = -1,
                        std::string verb = "Add");

    const Id& objectId() const { return id_; }

    std::string description() const override { return description_; }
    Status execute() override;
    Status undo() override;

private:
    Scene& scene_;
    std::unique_ptr<SceneObject> object_;  // held while the object is not in the scene
    Id id_;
    Id parentId_;
    int index_;
    std::string description_;
};

// Removes an object and its subtree; undo puts it back where it was.
class RemoveObjectCommand final : public Command {
public:
    RemoveObjectCommand(Scene& scene, Id id);

    std::string description() const override { return description_; }
    Status execute() override;
    Status undo() override;

private:
    Scene& scene_;
    Id id_;
    Id parentId_;
    int index_ = -1;
    std::unique_ptr<SceneObject> object_;
    std::string description_;
};

// Replaces an object's editable state: rename, move, rotate, scale, edit
// geometry parameters, assign a material, hide / show, edit a component.
class SetObjectStateCommand final : public Command {
public:
    SetObjectStateCommand(Scene& scene, Id id, ObjectState newState, std::string description);

    std::string description() const override { return description_; }
    Status execute() override;
    Status undo() override;

private:
    Scene& scene_;
    Id id_;
    ObjectState newState_;
    ObjectState oldState_;
    std::string description_;
};

// Moves an object to another parent / position, keeping its local transform.
class MoveObjectCommand final : public Command {
public:
    MoveObjectCommand(Scene& scene, Id id, Id newParentId, int index = -1);

    std::string description() const override { return description_; }
    Status execute() override;
    Status undo() override;

private:
    Scene& scene_;
    Id id_;
    Id newParentId_;
    int newIndex_;
    Id oldParentId_;
    int oldIndex_ = -1;
    std::string description_;
};

// Replaces the content of a material library (add, edit, delete material).
// A library is small, so the command keeps both versions.
class SetMaterialLibraryCommand final : public Command {
public:
    using Changed = std::function<void()>;

    SetMaterialLibraryCommand(MaterialLibrary& library, MaterialLibrary newContent, std::string description,
                              Changed changed = {});

    std::string description() const override { return description_; }
    Status execute() override;
    Status undo() override;

private:
    MaterialLibrary& library_;
    MaterialLibrary newContent_;
    MaterialLibrary oldContent_;
    std::string description_;
    Changed changed_;
};

}  // namespace resonant

namespace resonant {

class Project;

// Replaces one entry of a project's "extensions" (project-level settings of
// a feature, such as the acoustic medium). Undo restores the previous
// entry, or removes it when there was none.
class SetProjectExtensionCommand final : public Command {
public:
    using Changed = std::function<void()>;

    SetProjectExtensionCommand(Project& project, std::string key, std::string newJsonText, std::string description,
                               Changed changed = {});

    std::string description() const override { return description_; }
    Status execute() override;
    Status undo() override;

private:
    Project& project_;
    std::string key_;
    std::string newText_;   // JSON text, so this header needs no JSON type
    std::string oldText_;
    bool hadOld_ = false;
    std::string description_;
    Changed changed_;
};

}  // namespace resonant
