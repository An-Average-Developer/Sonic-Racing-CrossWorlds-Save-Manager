#pragma once
#include "Graphics/Graphics.h"
#include "UI/Material/PackIcon.h"
#include "UI/Material/Theme.h"

namespace SonicRacingSaveManager::Main {

class MainWindow;

// ThemeToggleButton_Click: a snapshot of the old theme covers the window and a ring centered on the
// button opens a hole in it (RingMaskBrush: RadialGradientBrush with RadiusX/Y 3.0 of the window,
// inner stop 0 -> 1 and outer stop 0.02 -> 1 over 0.8 s, CubicEase InOut). Meanwhile the button's
// icon spins 360 degrees in 0.6 s while shrinking to 0, swapping sun / moon and growing back.
class ThemeTransition {
public:
    static ThemeTransition* Of(MainWindow& main);

    // Requested by the button; the next frame takes the snapshot (drawn with the old theme) and
    // only then switches the theme
    void Request(float centerX, float centerY);
    bool Pending() const { return pending; }
    bool Animating() const { return pending || active; }
    // Called by the window with the freshly drawn old-theme snapshot
    void Begin(ComPtr<ID2D1Bitmap1> snapshot, double now);
    // Draws the masked snapshot over the new theme (z 1000, not hit-testable)
    void Draw(Graphics& g, const Rect& screen, double now);

    UI::Material::Icon Icon() const { return icon; }
    // The theme changed without the transition (a seasonal theme forces dark mode): show its icon
    void SyncIcon() { icon = nextIcon = UI::Material::IsDarkTheme() ? UI::Material::Icon::WeatherSunny : UI::Material::Icon::WeatherNight; }
    float IconScale(double now) const;
    float IconAngle(double now) const;

private:
    bool pending = false, active = false;
    float cx = 0.f, cy = 0.f;
    double startedAt = -10.0;
    ComPtr<ID2D1Bitmap1> snapshot;
    UI::Material::Icon icon = UI::Material::Icon::WeatherSunny;   // ThemeIcon Kind="WeatherSunny"
    UI::Material::Icon nextIcon = UI::Material::Icon::WeatherSunny;
};

} // namespace SonicRacingSaveManager::Main
