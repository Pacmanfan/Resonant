#include "scene/Scene.h"

#include <algorithm>

namespace resonant {

bool operator==(const ObjectState& a, const ObjectState& b)
{
    const bool sameComponent = (a.component == b.component)
                               || (a.component && b.component && a.component->equals(*b.component));
    return a.name == b.name && a.local == b.local && a.visible == b.visible && a.geometry == b.geometry
           && a.materialId == b.materialId && sameComponent;
}

int SceneObject::indexInParent() const
{
    if (!parent_)
        return -1;
    const auto& siblings = parent_->children_;
    const auto it = std::find_if(siblings.begin(), siblings.end(),
                                 [this](const std::unique_ptr<SceneObject>& child) { return child.get() == this; });
    return static_cast<int>(it - siblings.begin());
}

bool SceneObject::isAncestorOf(const SceneObject& other) const
{
    for (const SceneObject* node = other.parent_; node; node = node->parent_)
        if (node == this)
            return true;
    return false;
}

std::unique_ptr<SceneObject> SceneObject::cloneSubtree(bool newIds) const
{
    auto copy = std::make_unique<SceneObject>(newIds ? Id::generate() : id_, type_);
    copy->state = state;
    for (const auto& child : children_)
        copy->addChild(child->cloneSubtree(newIds));
    return copy;
}

SceneObject* SceneObject::addChild(std::unique_ptr<SceneObject> child)
{
    child->parent_ = this;
    children_.push_back(std::move(child));
    return children_.back().get();
}

Scene::Scene() : root_(std::make_unique<SceneObject>(Id("scene-root"), "root"))
{
    root_->state.name = "Scene";
}

SceneObject* Scene::find(const Id& id)
{
    const auto it = index_.find(id);
    return it == index_.end() ? nullptr : it->second;
}

const SceneObject* Scene::find(const Id& id) const
{
    const auto it = index_.find(id);
    return it == index_.end() ? nullptr : it->second;
}

SceneObject* Scene::resolveParent(const Id& parentId)
{
    return parentId.isNull() ? root_.get() : find(parentId);
}

Id Scene::publicParentId(const SceneObject& object) const
{
    return (!object.parent_ || object.parent_ == root_.get()) ? Id() : object.parent_->id();
}

void Scene::indexSubtree(SceneObject& object)
{
    index_[object.id()] = &object;
    for (const auto& child : object.children_)
        indexSubtree(*child);
}

void Scene::unindexSubtree(const SceneObject& object)
{
    index_.erase(object.id());
    for (const auto& child : object.children_)
        unindexSubtree(*child);
}

bool Scene::subtreeCollides(const SceneObject& object) const
{
    if (object.id().isNull() || index_.contains(object.id()) || object.id() == root_->id())
        return true;
    return std::any_of(object.children_.begin(), object.children_.end(),
                       [this](const std::unique_ptr<SceneObject>& child) { return subtreeCollides(*child); });
}

Result<SceneObject*> Scene::insert(std::unique_ptr<SceneObject> object, const Id& parentId, int index)
{
    if (!object)
        return Result<SceneObject*>::failure("no object to insert");
    SceneObject* parent = resolveParent(parentId);
    if (!parent)
        return Result<SceneObject*>::failure("parent object " + parentId.str() + " does not exist");
    if (subtreeCollides(*object))
        return Result<SceneObject*>::failure("object id " + object->id().str() + " is null or already in the scene");

    SceneObject* raw = object.get();
    raw->parent_ = parent;
    auto& siblings = parent->children_;
    if (index < 0 || index > static_cast<int>(siblings.size()))
        index = static_cast<int>(siblings.size());
    siblings.insert(siblings.begin() + index, std::move(object));
    indexSubtree(*raw);
    notify({SceneEvent::Kind::Added, raw->id(), publicParentId(*raw)});
    return raw;
}

std::unique_ptr<SceneObject> Scene::detach(const Id& id)
{
    SceneObject* object = find(id);
    if (!object)
        return nullptr;
    const Id parentId = publicParentId(*object);
    auto& siblings = object->parent_->children_;
    const int position = object->indexInParent();
    std::unique_ptr<SceneObject> owned = std::move(siblings[static_cast<size_t>(position)]);
    siblings.erase(siblings.begin() + position);
    owned->parent_ = nullptr;
    unindexSubtree(*owned);
    notify({SceneEvent::Kind::Removed, id, parentId});
    return owned;
}

Status Scene::move(const Id& id, const Id& newParentId, int index)
{
    SceneObject* object = find(id);
    if (!object)
        return Status::failure("object " + id.str() + " does not exist");
    SceneObject* newParent = resolveParent(newParentId);
    if (!newParent)
        return Status::failure("parent object " + newParentId.str() + " does not exist");
    if (newParent == object || object->isAncestorOf(*newParent))
        return Status::failure("an object cannot be moved into itself or one of its descendants");

    auto& oldSiblings = object->parent_->children_;
    const int position = object->indexInParent();
    std::unique_ptr<SceneObject> owned = std::move(oldSiblings[static_cast<size_t>(position)]);
    oldSiblings.erase(oldSiblings.begin() + position);

    auto& siblings = newParent->children_;
    if (index < 0 || index > static_cast<int>(siblings.size()))
        index = static_cast<int>(siblings.size());
    owned->parent_ = newParent;
    siblings.insert(siblings.begin() + index, std::move(owned));
    notify({SceneEvent::Kind::Moved, id, publicParentId(*object)});
    return Status::success();
}

Status Scene::setState(const Id& id, ObjectState state)
{
    SceneObject* object = find(id);
    if (!object)
        return Status::failure("object " + id.str() + " does not exist");
    object->state = std::move(state);
    notify({SceneEvent::Kind::Changed, id, publicParentId(*object)});
    return Status::success();
}

void Scene::clear()
{
    root_->children_.clear();
    index_.clear();
    notify({SceneEvent::Kind::Reset, Id(), Id()});
}

Mat4 Scene::worldMatrix(const SceneObject& object) const
{
    Mat4 matrix = Mat4::Identity();
    for (const SceneObject* node = &object; node && node != root_.get(); node = node->parent_)
        matrix = node->state.local.matrix() * matrix;
    return matrix;
}

bool Scene::isEffectivelyVisible(const SceneObject& object) const
{
    for (const SceneObject* node = &object; node && node != root_.get(); node = node->parent_)
        if (!node->state.visible)
            return false;
    return true;
}

namespace {
void visitSubtree(const SceneObject& object, const std::function<void(const SceneObject&)>& visit)
{
    visit(object);
    for (const auto& child : object.children())
        visitSubtree(*child, visit);
}
}  // namespace

void Scene::forEach(const std::function<void(const SceneObject&)>& visit) const
{
    for (const auto& child : root_->children_)
        visitSubtree(*child, visit);
}

std::string Scene::uniqueName(const std::string& base) const
{
    const auto taken = [this](const std::string& name) {
        return std::any_of(index_.begin(), index_.end(),
                           [&name](const auto& entry) { return entry.second->state.name == name; });
    };
    if (!taken(base))
        return base;
    for (int suffix = 2;; ++suffix) {
        std::string candidate = base + " " + std::to_string(suffix);
        if (!taken(candidate))
            return candidate;
    }
}

int Scene::addListener(Listener listener)
{
    const int handle = nextListener_++;
    listeners_.emplace_back(handle, std::move(listener));
    return handle;
}

void Scene::removeListener(int handle)
{
    std::erase_if(listeners_, [handle](const auto& entry) { return entry.first == handle; });
}

void Scene::notify(const SceneEvent& event)
{
    // Copy: a listener may add or remove listeners.
    const auto listeners = listeners_;
    for (const auto& [handle, listener] : listeners)
        listener(event);
}

std::unique_ptr<SceneObject> makeGroup(std::string name)
{
    auto object = std::make_unique<SceneObject>(Id::generate(), object_types::Group);
    object->state.name = std::move(name);
    return object;
}

std::unique_ptr<SceneObject> makeGeometryObject(std::string name, Geometry geometry)
{
    auto object = std::make_unique<SceneObject>(Id::generate(), object_types::Geometry);
    object->state.name = std::move(name);
    object->state.geometry = std::move(geometry);
    return object;
}

}  // namespace resonant
