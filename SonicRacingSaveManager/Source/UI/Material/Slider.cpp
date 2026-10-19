#include "UI/Material/Slider.h"
#include <algorithm>

namespace SonicRacingSaveManager::UI::Material {

bool Slider(Ui& ui, Id id, const Rect& r, float& value, float minimum, float maximum, Color color, bool enabled, bool* dragging) {
    constexpr float kTrack = 4.f, kThumb = 20.f;
    const Interaction it = ui.Interact(id, r, enabled, false);
    const float range = std::max(1e-6f, maximum - minimum);
    bool changed = false;
    if (it.held) {
        const float t = std::clamp((ui.MouseX() - r.x - kThumb * 0.5f) / std::max(1.f, r.w - kThumb), 0.f, 1.f);
        const float v = minimum + t * range;
        if (v != value) {
            value = v;
            changed = true;
        }
    }
    if (dragging) *dragging = it.held;
    if (!enabled) ui.g.PushOpacity(0.38f);
    const float t = std::clamp((value - minimum) / range, 0.f, 1.f);
    const float x = r.x + kThumb * 0.5f + (r.w - kThumb) * t, cy = r.CY();
    ui.g.FillRound({ r.x + kThumb * 0.5f, cy - kTrack * 0.5f, r.w - kThumb, kTrack }, kTrack * 0.5f, color.Fade(0.38f));
    ui.g.FillRound({ r.x + kThumb * 0.5f, cy - kTrack * 0.5f, x - r.x - kThumb * 0.5f, kTrack }, kTrack * 0.5f, color);
    if (enabled && (it.hovered || it.held)) ui.g.FillEllipse(x, cy, kThumb, kThumb, color.Fade(it.held ? 0.18f : 0.12f));
    ui.g.FillEllipse(x, cy, kThumb * 0.5f, kThumb * 0.5f, color);
    if (!enabled) ui.g.PopLayer();
    return changed;
}

} // namespace SonicRacingSaveManager::UI::Material
