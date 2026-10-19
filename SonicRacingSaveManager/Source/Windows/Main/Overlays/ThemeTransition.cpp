#include "Windows/Main/Overlays/ThemeTransition.h"
#include <algorithm>
#include "Graphics/Animation.h"
#include "UI/Material/Theme.h"
#include "Windows/Main/MainWindow.h"

namespace SonicRacingSaveManager::Main {

namespace {
constexpr double kRing = 0.8, kSpin = 0.6, kHalf = 0.3;
}

void ThemeTransition::Request(float centerX, float centerY) {
    if (Animating()) return;   // _isThemeAnimating
    pending = true;
    cx = centerX;
    cy = centerY;
}

void ThemeTransition::Begin(ComPtr<ID2D1Bitmap1> bitmap, double now) {
    pending = false;
    active = true;
    snapshot = std::move(bitmap);
    startedAt = now;
    UI::Material::ToggleTheme();
    nextIcon = UI::Material::IsDarkTheme() ? UI::Material::Icon::WeatherSunny : UI::Material::Icon::WeatherNight;
}

float ThemeTransition::IconScale(double now) const {
    const double t = now - startedAt;
    if (t < 0.0 || t >= kHalf * 2.0) return 1.f;
    if (t < kHalf) return 1.f - Anim::EaseInCubic(static_cast<float>(t / kHalf));
    return Anim::EaseOutCubic(static_cast<float>((t - kHalf) / kHalf));
}

float ThemeTransition::IconAngle(double now) const {
    const double t = now - startedAt;
    if (t < 0.0 || t >= kSpin) return 0.f;
    return 360.f * Anim::EaseInOutCubic(static_cast<float>(t / kSpin));
}

void ThemeTransition::Draw(Graphics& g, const Rect& screen, double now) {
    if (!active) return;
    const double t = now - startedAt;
    // The icon swaps when the shrink finishes
    if (t >= kHalf) icon = nextIcon;
    if (t >= kRing || !snapshot) {
        active = false;
        snapshot.Reset();
        icon = nextIcon;
        return;
    }
    const float p = Anim::EaseInOutCubic(static_cast<float>(t / kRing));
    const float inner = p, outer = Anim::Lerp(0.02f, 1.f, p);

    // RingMaskBrush: transparent inside the inner edge, black (snapshot visible) past the outer edge
    ID2D1DeviceContext2* dc = g.Context();
    const D2D1_GRADIENT_STOP stops[] = {
        { 0.f, D2D1::ColorF(0, 0, 0, 0) },
        { inner, D2D1::ColorF(0, 0, 0, 0) },
        { std::max(outer, inner + 0.0001f), D2D1::ColorF(0, 0, 0, 1) },
        { 1.f, D2D1::ColorF(0, 0, 0, 1) },
    };
    ComPtr<ID2D1GradientStopCollection> collection;
    ComPtr<ID2D1RadialGradientBrush> mask;
    if (FAILED(dc->CreateGradientStopCollection(stops, 4, &collection)) ||
        FAILED(dc->CreateRadialGradientBrush(D2D1::RadialGradientBrushProperties(D2D1::Point2F(cx, cy), D2D1::Point2F(0, 0),
                                                                                 screen.w * 3.f, screen.h * 3.f),
                                             collection.Get(), &mask)))
        return;
    g.PushMaskLayer(1.f, mask.Get());
    g.DrawBitmap(snapshot.Get(), screen);
    g.PopLayer();
}

ThemeTransition* ThemeTransition::Of(MainWindow& main) { return main.themeTransition.get(); }

} // namespace SonicRacingSaveManager::Main
