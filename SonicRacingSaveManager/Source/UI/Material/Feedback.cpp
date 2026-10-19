#include "UI/Material/Feedback.h"
#include <algorithm>
#include <cmath>
#include "Graphics/Animation.h"

namespace SonicRacingSaveManager::UI::Material {

namespace {

struct BadgeState {
    int shown = 0;
    double changedAt = -10.0;
};

// SineEase EaseOut
float SineOut(float t) { return std::sin(Anim::Clamp01(t) * 1.5707963f); }

constexpr double kReveal = 0.3, kShow = 4.0;

} // namespace

void Badge(Ui& ui, Id id, const Rect& content, int count, Color background) {
    BadgeState& s = ui.Data<BadgeState>(HashId(id, 0xBAD6E));
    if (count != s.shown) {
        if (count > 0) s.changedAt = ui.Time();
        s.shown = count;
    }
    if (count <= 0) return;
    const std::wstring text = std::to_wstring(count);
    TextStyle t = Text(11.f);
    t.align = HAlign::Center;
    t.valign = VAlign::Center;
    // MinWidth / MinHeight 18, Padding 2 + the 1 px margin around the text
    const float w = std::max(18.f, ui.g.Measure(text, t).w + 6.f), h = 18.f;
    const float cx = content.R(), cy = content.y;
    float scale = 1.f;
    const float since = static_cast<float>(ui.Time() - s.changedAt);
    if (since < 0.3f) {
        scale = Anim::Lerp(1.4f, 1.f, Anim::EaseOutCubic(since / 0.3f));
        ui.KeepAnimating();
    }
    const Rect r{ cx - w * 0.5f, cy - h * 0.5f, w, h };
    if (scale != 1.f) ui.PushScale(scale, cx, cy);
    ui.g.FillRound(r, 9.f, background);
    ui.g.Text(text, r, ContrastingForeground(background), t);
    if (scale != 1.f) ui.PopTransform();
}

void ProgressBar(Ui& ui, const Rect& r, float percent, bool indeterminate, Color fill) {
    if (fill.Transparent()) fill = P().primaryMid.color;
    ui.g.FillRect(r, P().primaryLight.color);
    if (indeterminate) {
        const float t = static_cast<float>(std::fmod(ui.Time(), 1.6) / 1.6);
        const float segment = r.w * 0.3f;
        const float x = r.x - segment + (r.w + segment) * t;
        ui.PushClip(r);
        ui.g.FillRect({ x, r.y, segment, r.h }, fill);
        ui.PopClip();
        ui.KeepAnimatingThrottled();
        return;
    }
    const float w = r.w * std::clamp(percent / 100.f, 0.f, 1.f);
    if (w > 0.f) ui.g.FillRect({ r.x, r.y, w, r.h }, fill);
}

void Separator(Ui& ui, float x, float y, float w) { ui.g.FillRect({ x, y, w, 1.f }, P().separator); }

void Snackbar::Enqueue(const std::wstring& message) { queue.push_back(message); }

void Snackbar::Draw(Ui& ui, const Rect& area) {
    const double now = ui.Time();
    if (!current && !queue.empty()) {
        current = queue.front();
        queue.pop_front();
        shownAt = now;
    }
    if (!current) return;
    const double t = now - shownAt;
    if (t >= kReveal + kShow + kReveal) {
        current.reset();
        return;
    }
    ui.KeepAnimatingThrottled();

    // ActivateStoryboard: height 0 -> 1 (SineEase out); content fades in from 75 ms over 225 ms.
    // DeactivateStoryboard: height 1 -> 0 over 0.3 s.
    float reveal = 1.f;
    if (t < kReveal) reveal = SineOut(static_cast<float>(t / kReveal));
    else if (t > kReveal + kShow) reveal = 1.f - SineOut(static_cast<float>((t - kReveal - kShow) / kReveal));
    const float contentOpacity = t < kReveal ? Anim::Clamp01(SineOut(static_cast<float>((t - 0.075) / 0.225))) : 1.f;
    if (reveal < 1.f) ui.KeepAnimating();

    const Palette& p = P();
    TextStyle text = Text(14.f);
    text.wrap = true;
    text.ellipsis = true;
    const float maxText = 568.f - 32.f;
    const auto size = ui.g.Measure(*current, text, maxText);
    const float w = std::clamp(size.w + 32.f, 288.f, 568.f);
    const float h = std::max(48.f, size.h + 32.f);
    const Rect box{ area.CX() - w * 0.5f, area.B() - h, w, h };
    const float visibleH = h * reveal;
    if (visibleH <= 0.5f) return;

    ui.Block({ box.x, box.B() - visibleH, w, visibleH });
    ui.g.PushClip({ box.x, box.B() - visibleH, w, visibleH });
    // The Root panel is bottom-aligned and grows upward, showing the border from its top edge
    const Rect shown{ box.x, box.B() - visibleH, w, h };
    ui.g.FillRound(shown, 3.f, p.snackbarBackground);
    ui.g.Text(*current, shown.Inset(16.f), p.background.Fade(contentOpacity), text);
    ui.g.PopClip();
}

} // namespace SonicRacingSaveManager::UI::Material
