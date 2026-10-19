#pragma once
#include "UI/Material/Theme.h"
#include "UI/Ui.h"

namespace SonicRacingSaveManager::UI::Material {

// materialDesign:Card as App.xaml styles it: SeasonalCardBackgroundBrush, UniformCornerRadius 4,
// Elevation Dp1. With `decorated` (the CardStyle resource) it also gets the seasonal corner art.
// Draw the content between construction and destruction; `content` is the padded area. While
// `opacity` < 1 (a fade-in) the card also rises into place.
// Corner radius of the redesigned cards (MaterialDesign's was 4)
inline constexpr float kCardRadius = 10.f;

// Shape of a card
struct CardLook {
    float radius = kCardRadius;             // UniformCornerRadius
    float elevation = 1.f;                  // dp
    Color border = Colors::Transparent;     // drawn over the content
    float borderWidth = 1.f;
};

class Card {
public:
    Card(Ui& ui, Id id, const Rect& r, const Edges& padding, bool decorated = true, float opacity = 1.f, const CardLook& look = {});
    ~Card();
    Card(const Card&) = delete;
    Card& operator=(const Card&) = delete;

    Rect bounds, content;

private:
    Ui& ui;
    Id id;
    bool decorated;
    float opacity;
    CardLook look;
    bool moved = false;
};

// CardStyle's FadeInAnimation (0 -> 1 over 0.3 s, CubicEase out) from `loadedAt`.
float CardFade(Ui& ui, double loadedAt);

// The "MAINTENANCE" overlay on a card: #CC808080 over the whole card with a white lock (48) and
// "MAINTENANCE" (or "UNDER" / "MAINTENANCE"), optionally a small caption underneath. Blocks input.
void MaintenanceOverlay(Ui& ui, const Rect& r, float radius, bool underWord = false, const wchar_t* caption = nullptr,
                        float lockSize = 48.f);

} // namespace SonicRacingSaveManager::UI::Material
