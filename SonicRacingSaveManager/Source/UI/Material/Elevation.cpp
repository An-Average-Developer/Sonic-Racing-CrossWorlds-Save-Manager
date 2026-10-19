#include "UI/Material/Elevation.h"
#include <algorithm>
#include <cmath>
#include "Graphics/Animation.h"

namespace SonicRacingSaveManager::UI::Material {

namespace {

// MaterialDesignTheme.Shadows.xaml (MaterialDesignElevationShadowN): BlurRadius 2N+1, ShadowDepth
// 0.5+0.5N for N <= 8, then Dp12 25/6.5, Dp16 33/8.5, Dp24 49/12.5. Direction 270, opacity .45.
void ShadowFor(float dp, float& blur, float& depth) {
    if (dp <= 8.f) {
        blur = 2.f * dp + 1.f;
        depth = 0.5f + 0.5f * dp;
    } else if (dp <= 12.f) {
        const float t = (dp - 8.f) / 4.f;
        blur = Anim::Lerp(17.f, 25.f, t);
        depth = Anim::Lerp(4.5f, 6.5f, t);
    } else if (dp <= 16.f) {
        const float t = (dp - 12.f) / 4.f;
        blur = Anim::Lerp(25.f, 33.f, t);
        depth = Anim::Lerp(6.5f, 8.5f, t);
    } else {
        const float t = std::min(1.f, (dp - 16.f) / 8.f);
        blur = Anim::Lerp(33.f, 49.f, t);
        depth = Anim::Lerp(8.5f, 12.5f, t);
    }
}

constexpr float kShadowOpacity = 0.45f;

struct RippleState {
    float x = 0.f, y = 0.f;
    double start = -1.0, release = -1.0;
};

} // namespace

void Elevation(Graphics& g, const Rect& r, float radius, float dp, bool darken) {
    if (dp <= 0.01f) return;
    float blur, depth;
    ShadowFor(dp, blur, depth);
    // Below 1dp the shadow fades instead of popping (elevation animations start at 0)
    const float fade = std::min(1.f, dp);
    const float opacity = (darken ? 1.f : kShadowOpacity) * fade;
    g.GlowRound({ r.x, r.y + depth, r.w, r.h }, radius, Colors::Black, blur, opacity);
}

void EllipseElevation(Graphics& g, float cx, float cy, float rx, float dp, bool darken) {
    Elevation(g, { cx - rx, cy - rx, rx * 2.f, rx * 2.f }, rx, dp, darken);
}

void RingElevation(Graphics& g, float cx, float cy, float radius, float thickness, float dp, bool darken) {
    if (dp <= 0.01f || thickness <= 0.f) return;
    float blur, depth;
    ShadowFor(dp, blur, depth);
    const float opacity = (darken ? 1.f : kShadowOpacity) * std::min(1.f, dp);
    g.GlowRing(cx, cy + depth, radius, thickness, Colors::Black, blur, opacity);
}

void Ripple(Ui& ui, Id id, const Interaction& state, const Rect& r, float radius, Color feedback) {
    RippleState& s = ui.Data<RippleState>(HashId(id, 0x819E));
    const double now = ui.Time();
    if (state.hovered && ui.In().pressed[0]) {
        s.x = ui.MouseX();
        s.y = ui.MouseY();
        s.start = now;
        s.release = -1.0;
    }
    if (s.start < 0.0) return;
    if (!state.held && s.release < 0.0) s.release = now;

    // Grows to cover the farthest corner; held ripples stay visible, released ones fade out
    const float reach = std::max({ std::hypot(s.x - r.x, s.y - r.y), std::hypot(s.x - r.R(), s.y - r.y),
                                 std::hypot(s.x - r.x, s.y - r.B()), std::hypot(s.x - r.R(), s.y - r.B()) });
    const float grow = Anim::EaseOutCubic(Anim::Clamp01(static_cast<float>((now - s.start) / 0.45)));
    float opacity = 0.26f;
    if (s.release >= 0.0) {
        // Released early: finish growing quickly while fading
        const float fade = Anim::Clamp01(static_cast<float>((now - std::max(s.release, s.start + 0.12)) / 0.3));
        opacity *= 1.f - fade;
        if (fade >= 1.f) {
            s.start = -1.0;
            return;
        }
    }
    ui.KeepAnimating();
    ui.g.PushRoundClip(r, radius);
    const float rad = reach * grow;
    ui.g.FillEllipse(s.x, s.y, rad, rad, feedback.Fade(opacity));
    ui.g.PopLayer();
}

} // namespace SonicRacingSaveManager::UI::Material
