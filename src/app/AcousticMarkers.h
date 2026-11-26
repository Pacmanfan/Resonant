// How acoustic objects appear in the viewport: emitters and array elements
// as small spheres with a line along their axis, targets and probes as
// markers, field regions as outlines. Presentation only.
#pragma once

#include "SceneRenderBridge.h"

SceneRenderBridge::Decorator makeAcousticDecorator();

// Display name of an acoustic object's type, or an empty string.
QString acousticTypeLabel(const resonant::SceneObject& object);
