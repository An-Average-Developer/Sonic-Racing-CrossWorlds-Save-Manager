#pragma once
#include "UI/Material/Theme.h"
#include "UI/Ui.h"

namespace SonicRacingSaveManager::UI::Material {

// MaterialDesign3 slider (the implicit Slider style): 4 px track filled up to the value, a round
// thumb in `color` (Foreground) with a hover halo. Returns true while the value changes;
// `dragging` reports an active drag (Thumb.DragStarted … DragCompleted).
bool Slider(Ui& ui, Id id, const Rect& r, float& value, float minimum, float maximum, Color color, bool enabled = true, bool* dragging = nullptr);

} // namespace SonicRacingSaveManager::UI::Material
