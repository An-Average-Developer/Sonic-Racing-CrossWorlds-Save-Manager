#include "Windows/Main/Tabs/TabBanner.h"
#include <algorithm>
#include <cmath>
#include "App/Paths.h"
#include "Graphics/Animation.h"
#include "UI/Material/Elevation.h"
#include "UI/Material/Motion.h"
#include "UI/Material/Theme.h"
#include "Windows/Main/Tabs/TabKit.h"

namespace SonicRacingSaveManager::Main {

namespace {

namespace M = UI::Material;
using TabKit::LineH;
using TabKit::Style;

constexpr float kPi = 3.14159265f, kRadius = 16.f;

// Stable pseudo-random 0..1 (the sparks need no state)
float Hash(double index, double channel) {
    const double v = std::sin(index * 12.9898 + channel * 78.233) * 43758.5453;
    return static_cast<float>(v - std::floor(v));
}

} // namespace

void TabBanner(UI::Ui& ui, const Rect& r, const TabBannerSpec& spec, double loadedAt) {
    const M::Palette& p = M::P();
    const Color ink = p.primaryDark.foreground, spark = p.secondaryMid.color;
    const double time = ui.Time();
    const float t = static_cast<float>(time);
    ui.KeepAnimatingThrottled();

    // Background
    M::Elevation(ui.g, r, kRadius, 3.f);
    ui.g.PushRoundClip(r, kRadius);
    const float drift = 0.18f * std::sin(t * 0.33f);
    ui.g.FillLinear(r, 0.f, { { 0.f, p.primaryDark.color }, { 0.6f, p.primaryMid.color }, { 1.f, Lerp(p.primaryDark.color, Colors::Black, 0.3f) } },
                    { drift, 0.f }, { 1.f, 1.f - drift });
    const float gx = r.x + r.w * (0.7f + 0.15f * std::sin(t * 0.21f));
    ui.g.FillRadial({ gx - 240.f, r.CY() - 150.f, 480.f, 300.f }, { { 0.f, spark.Fade(0.25f) }, { 1.f, spark.Fade(0.f) } });
    for (int i = 0; i < 22; ++i) {
        const float period = 3.f + 4.f * Hash(i, 2);
        const float phase = static_cast<float>(std::fmod(time / period + Hash(i, 3), 1.0));
        const float x = r.x + Hash(i, 1) * r.w + 8.f * std::sin(t * 1.2f + static_cast<float>(i));
        const float y = r.B() + 6.f - phase * (r.h + 12.f);
        const float size = 1.f + 2.f * Hash(i, 4);
        ui.g.FillEllipse(x, y, size, size, Lerp(spark, Colors::White, 0.4f).Fade(0.7f * std::sin(phase * kPi)));
    }
    const float sweep = static_cast<float>(std::fmod(time, 7.0) / 1.4);
    if (sweep < 1.f) {
        ui.KeepAnimating();
        const float sx = r.x - 200.f + (r.w + 400.f) * Anim::EaseInOutCubic(sweep);
        ui.g.PushTransform(D2D1::Matrix3x2F::Skew(-24.f, 0.f, D2D1::Point2F(sx, r.CY())));
        ui.g.FillLinear({ sx, r.y, 180.f, r.h }, 0.f, { { 0.f, ink.Fade(0.f) }, { 0.5f, ink.Fade(0.12f) }, { 1.f, ink.Fade(0.f) } }, { 0.f, 0.f }, { 1.f, 0.f });
        ui.g.PopTransform();
    }
    ui.g.FillRect({ r.x, r.B() - 3.f, r.w, 3.f }, spark.Fade(0.45f));
    ui.g.PopLayer();

    // Emblem: the icon inside two turning arcs, with its own motion
    const float er = 32.f, ecx = r.x + 30.f + er, ecy = r.CY();
    ui.g.FillEllipse(ecx, ecy, er, er, Colors::Black.Fade(0.2f));
    ui.g.StrokeEllipse(ecx, ecy, er, er, ink.Fade(0.3f), 1.5f);
    for (int i = 0; i < 2; ++i) TabKit::Arc(ui.g, ecx, ecy, er + 6.f, t * 70.f + 180.f * static_cast<float>(i), 70.f, spark.Fade(0.85f), 2.5f);
    D2D1::Matrix3x2F motion = D2D1::Matrix3x2F::Identity();
    switch (spec.motion) {
        case TabBannerSpec::Motion::Wobble:
            motion = D2D1::Matrix3x2F::Rotation(12.f * std::sin(t * 6.f) * (0.5f + 0.5f * std::sin(t * 0.9f)), D2D1::Point2F(ecx, ecy)) *
                     D2D1::Matrix3x2F::Translation(3.f * std::sin(t * 2.3f), 0.f);
            break;
        case TabBannerSpec::Motion::Float: motion = D2D1::Matrix3x2F::Translation(0.f, 4.f * std::sin(t * 2.f)); break;
        case TabBannerSpec::Motion::Spin: motion = D2D1::Matrix3x2F::Rotation(std::fmod(t * 40.f, 360.f), D2D1::Point2F(ecx, ecy)); break;
        case TabBannerSpec::Motion::SpinBack: motion = D2D1::Matrix3x2F::Rotation(-std::fmod(t * 40.f, 360.f), D2D1::Point2F(ecx, ecy)); break;
    }
    ui.g.PushTransform(motion);
    M::DrawIcon(ui.g, spec.icon, ecx, ecy, 34.f, Lerp(spark, Colors::White, 0.3f));
    ui.g.PopTransform();

    // Pill on the right
    float right = r.R() - 26.f;
    if (!spec.pill.empty()) {
        TextStyle pillText = Style(12.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        pillText.valign = VAlign::Center;
        const float pw = std::ceil(ui.g.Measure(spec.pill, pillText).w) + 52.f;
        const Rect pill{ right - pw, r.CY() - 18.f, pw, 36.f };
        ui.g.FillRound(pill, 18.f, Colors::Black.Fade(0.22f));
        ui.g.StrokeRound(pill.Inset(0.5f), 17.5f, ink.Fade(0.25f), 1.f);
        if (!spec.pillLogo.empty()) {
            if (auto logo = Images::Load(Paths::Asset(spec.pillLogo))) ui.g.DrawImage(*logo, { pill.x + 12.f, pill.CY() - 10.f, 20.f, 20.f }, 1.f);
        } else {
            // The text's colour with a touch of the accent: the accent alone can be as dark as the
            // pill (the grey themes) and the icon vanished
            ui.g.FillEllipse(pill.x + 22.f, pill.CY(), 13.f, 13.f, ink.Fade(0.12f));
            M::DrawIcon(ui.g, spec.pillIcon, pill.x + 22.f, pill.CY(), 16.f, Lerp(ink, spark, 0.3f));
        }
        ui.g.Text(spec.pill, { pill.x + 40.f, pill.y, pw - 48.f, pill.h }, ink, pillText);
        right = pill.x - 20.f;
    }

    // Title dropping in letter by letter, the subtitle under it
    const float tx = ecx + er + 26.f;
    TextStyle heading = Style(30.f, DWRITE_FONT_WEIGHT_BLACK);
    heading.valign = VAlign::Top;
    TextStyle sub = Style(13.f);
    sub.ellipsis = true;
    const float titleH = LineH(ui, heading), subH = LineH(ui, sub);
    const float y = r.CY() - (titleH + 4.f + subH) * 0.5f;
    for (size_t i = 0; i < spec.title.size(); ++i) {
        const float k = M::Motion::Stagger(ui, loadedAt + 0.15, static_cast<int>(i), 0.04f, 0.5f);
        const float cx = ui.g.Measure(spec.title.substr(0, i), heading).w;
        ui.g.Text(std::wstring(1, spec.title[i]), { tx + cx, y - 22.f * (1.f - Anim::EaseOutBack(k, 0.6f)), 60.f, titleH },
                  ink.Fade(Anim::Clamp01(k * 1.5f)), heading);
    }
    ui.g.Text(spec.subtitle, { tx, y + titleH + 4.f, std::max(0.f, right - tx), subH }, ink.Fade(0.8f), sub);
}

} // namespace SonicRacingSaveManager::Main
