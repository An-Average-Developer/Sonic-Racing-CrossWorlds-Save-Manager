#include "UI/Material/Motion.h"
#include <algorithm>
#include "Graphics/Animation.h"

namespace SonicRacingSaveManager::UI::Material::Motion {

float Stagger(Ui& ui, double start, int index, float step, float duration, int maxSteps) {
    const double begin = start + static_cast<double>(std::min(index, maxSteps)) * step;
    const float t = Anim::Clamp01(static_cast<float>((ui.Time() - begin) / duration));
    if (t < 1.f) ui.KeepAnimating();
    return Anim::EaseOutCubic(t);
}

Enter::Enter(Ui& u, float t, float distance, float dx) : ui(u) {
    if (t >= 1.f) return;
    layer = true;
    ui.g.PushOpacity(std::max(0.f, t));
    moved = distance != 0.f || dx != 0.f;
    if (moved) ui.PushTranslate(dx * (1.f - t), distance * (1.f - t));
}

Enter::~Enter() {
    if (moved) ui.PopTransform();
    if (layer) ui.g.PopLayer();
}

Feedback Hover(Ui& ui, Id id, bool hovered, bool held) {
    Feedback f;
    f.hover = Anim::EaseOutCubic(ui.Tween(HashId(id, 0x40E1), hovered ? 1.f : 0.f, 0.12f, 0.f));
    f.press = ui.Tween(HashId(id, 0x40E2), held ? 1.f : 0.f, 0.08f, 0.f);
    return f;
}

float Pulse(Ui& ui, Id id, bool on, float duration) {
    struct State {
        bool was = false;
        double since = -100.0;
    };
    State& s = ui.Data<State>(HashId(id, 0x9015));
    if (on && !s.was) s.since = ui.Time();
    s.was = on;
    if (!on) return 0.f;
    const float t = static_cast<float>((ui.Time() - s.since) / duration);
    if (t >= 1.f) return 0.f;
    ui.KeepAnimating();
    return 1.f - Anim::EaseOutCubic(t);
}

float Toggle(Ui& ui, Id id, bool open, float seconds) {
    return Anim::EaseInOutCubic(ui.Tween(HashId(id, 0x7066), open ? 1.f : 0.f, seconds));
}

} // namespace SonicRacingSaveManager::UI::Material::Motion
