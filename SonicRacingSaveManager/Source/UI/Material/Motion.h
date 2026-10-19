#pragma once
#include "UI/Ui.h"

// The redesign's motion vocabulary, shared by every screen: staggered entrances, hover / press
// feedback, the pulse of something that just turned on, and open / close tweens.
namespace SonicRacingSaveManager::UI::Material::Motion {

// Entrance progress (0..1, EaseOutCubic) of item `index` of a group revealed at `start`: each item
// starts `step` seconds after the previous one (capped at `maxSteps`) and takes `duration`.
float Stagger(Ui& ui, double start, int index, float step = 0.03f, float duration = 0.35f, int maxSteps = 16);

// Draws what follows faded in and slid by (`dx`, `distance`) * (1 - t): up by default, sideways for
// page transitions (hit testing follows the slide).
class Enter {
public:
    Enter(Ui& ui, float t, float distance = 14.f, float dx = 0.f);
    ~Enter();
    Enter(const Enter&) = delete;
    Enter& operator=(const Enter&) = delete;

private:
    Ui& ui;
    bool layer = false, moved = false;
};

// Animated hover (0..1, ~120 ms) and press (0..1, ~80 ms) of a widget
struct Feedback {
    float hover = 0.f;
    float press = 0.f;
};
Feedback Hover(Ui& ui, Id id, bool hovered, bool held);

// 1 the moment `on` becomes true, fading to 0 over `duration` (0 while off or once faded)
float Pulse(Ui& ui, Id id, bool on, float duration = 0.8f);

// Open / close progress (0..1, EaseInOutCubic over `seconds`)
float Toggle(Ui& ui, Id id, bool open, float seconds = 0.25f);

} // namespace SonicRacingSaveManager::UI::Material::Motion
