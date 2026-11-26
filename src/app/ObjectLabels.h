// Display names of scene object types, in one place so the tree, the
// properties panel and messages agree on terminology.
#pragma once

#include "scene/SceneObject.h"

#include <QString>

// "Group", "Box", "Sphere", ... for an object.
QString objectTypeLabel(const resonant::SceneObject& object);
