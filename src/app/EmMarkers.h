#pragma once

#include "SceneRenderBridge.h"

// Markers for electromagnetic objects without geometry: ideal sources (a
// cube and their axis; a plane wave shows its direction of travel and E)
// and RF arrays (one cube per element, its brightness the element's
// relative amplitude, grey when switched off). Antennas and guides have
// geometry and are drawn as solids.
SceneRenderBridge::Decorator makeEmDecorator();
