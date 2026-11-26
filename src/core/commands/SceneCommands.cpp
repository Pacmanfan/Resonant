#include "commands/SceneCommands.h"

namespace resonant {

InsertObjectCommand::InsertObjectCommand(Scene& scene, std::unique_ptr<SceneObject> object, Id parentId, int index,
                                         std::string verb)
    : scene_(scene), object_(std::move(object)), parentId_(std::move(parentId)), index_(index)
{
    if (object_) {
        id_ = object_->id();
        description_ = verb + " " + object_->state.name;
    }
}

Status InsertObjectCommand::execute()
{
    if (!object_)
        return Status::failure("no object to insert");
    // Scene::insert consumes the object even when it fails, so keep a copy
    // to stay able to retry.
    auto candidate = std::move(object_);
    auto backup = candidate->cloneSubtree(false);
    auto result = scene_.insert(std::move(candidate), parentId_, index_);
    if (!result) {
        object_ = std::move(backup);
        return Status::failure(result.message());
    }
    return Status::success();
}

Status InsertObjectCommand::undo()
{
    object_ = scene_.detach(id_);
    if (!object_)
        return Status::failure("object " + id_.str() + " is no longer in the scene");
    return Status::success();
}

RemoveObjectCommand::RemoveObjectCommand(Scene& scene, Id id) : scene_(scene), id_(std::move(id))
{
    const SceneObject* object = scene_.find(id_);
    description_ = "Delete " + (object ? object->state.name : std::string("object"));
}

Status RemoveObjectCommand::execute()
{
    const SceneObject* object = scene_.find(id_);
    if (!object)
        return Status::failure("object " + id_.str() + " does not exist");
    const SceneObject* parent = object->parent();
    parentId_ = (parent && !scene_.isRoot(*parent)) ? parent->id() : Id();
    index_ = object->indexInParent();
    object_ = scene_.detach(id_);
    return Status::success();
}

Status RemoveObjectCommand::undo()
{
    if (!object_)
        return Status::failure("nothing to restore");
    auto backup = object_->cloneSubtree(false);
    auto result = scene_.insert(std::move(object_), parentId_, index_);
    if (!result) {
        object_ = std::move(backup);
        return Status::failure(result.message());
    }
    return Status::success();
}

SetObjectStateCommand::SetObjectStateCommand(Scene& scene, Id id, ObjectState newState, std::string description)
    : scene_(scene), id_(std::move(id)), newState_(std::move(newState)), description_(std::move(description))
{
}

Status SetObjectStateCommand::execute()
{
    const SceneObject* object = scene_.find(id_);
    if (!object)
        return Status::failure("object " + id_.str() + " does not exist");
    if (newState_.geometry) {
        if (Status status = newState_.geometry->validate(); !status)
            return status;
    }
    oldState_ = object->state;
    return scene_.setState(id_, newState_);
}

Status SetObjectStateCommand::undo()
{
    return scene_.setState(id_, oldState_);
}

MoveObjectCommand::MoveObjectCommand(Scene& scene, Id id, Id newParentId, int index)
    : scene_(scene), id_(std::move(id)), newParentId_(std::move(newParentId)), newIndex_(index)
{
    const SceneObject* object = scene_.find(id_);
    description_ = "Move " + (object ? object->state.name : std::string("object")) + " in hierarchy";
}

Status MoveObjectCommand::execute()
{
    const SceneObject* object = scene_.find(id_);
    if (!object)
        return Status::failure("object " + id_.str() + " does not exist");
    const SceneObject* parent = object->parent();
    oldParentId_ = (parent && !scene_.isRoot(*parent)) ? parent->id() : Id();
    oldIndex_ = object->indexInParent();
    return scene_.move(id_, newParentId_, newIndex_);
}

Status MoveObjectCommand::undo()
{
    return scene_.move(id_, oldParentId_, oldIndex_);
}

SetMaterialLibraryCommand::SetMaterialLibraryCommand(MaterialLibrary& library, MaterialLibrary newContent,
                                                     std::string description, Changed changed)
    : library_(library), newContent_(std::move(newContent)), description_(std::move(description)),
      changed_(std::move(changed))
{
}

Status SetMaterialLibraryCommand::execute()
{
    oldContent_ = library_;
    library_ = newContent_;
    if (changed_)
        changed_();
    return Status::success();
}

Status SetMaterialLibraryCommand::undo()
{
    library_ = oldContent_;
    if (changed_)
        changed_();
    return Status::success();
}

}  // namespace resonant

#include "project/Project.h"

namespace resonant {

SetProjectExtensionCommand::SetProjectExtensionCommand(Project& project, std::string key, std::string newJsonText,
                                                       std::string description, Changed changed)
    : project_(project), key_(std::move(key)), newText_(std::move(newJsonText)),
      description_(std::move(description)), changed_(std::move(changed))
{
}

Status SetProjectExtensionCommand::execute()
{
    nlohmann::json value = nlohmann::json::parse(newText_, nullptr, false);
    if (value.is_discarded())
        return Status::failure("the new settings are not valid JSON");
    const auto it = project_.extensions.find(key_);
    hadOld_ = it != project_.extensions.end();
    oldText_ = hadOld_ ? it->dump() : std::string();
    project_.extensions[key_] = std::move(value);
    if (changed_)
        changed_();
    return Status::success();
}

Status SetProjectExtensionCommand::undo()
{
    if (hadOld_)
        project_.extensions[key_] = nlohmann::json::parse(oldText_);
    else
        project_.extensions.erase(key_);
    if (changed_)
        changed_();
    return Status::success();
}

}  // namespace resonant
