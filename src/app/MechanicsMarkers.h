#pragma once

#include "SceneRenderBridge.h"

// Markers for actuators (orange sphere and force arrow), sensors (green)
// and vibration targets (magenta; red when suppressing).
SceneRenderBridge::Decorator makeMechanicsDecorator();

QString mechanicsTypeLabel(const resonant::SceneObject& object);
