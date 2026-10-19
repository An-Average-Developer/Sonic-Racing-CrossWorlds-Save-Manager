#pragma once
#include <string>
#include "Graphics/Graphics.h"
#include "UI/Material/PackIcon.h"
#include "UI/Ui.h"

namespace SonicRacingSaveManager::Main {

// The animated banner on top of the Updates, Changelogs and Reports tabs: a drifting gradient of
// the primary colour with a glow, rising sparks and a light sweep; the icon in a ring of turning
// arcs (with its own little motion), the title dropping in letter by letter, the subtitle, and a
// pill on the right.
struct TabBannerSpec {
    enum class Motion { Wobble, Float, Spin, SpinBack };   // SpinBack: counterclockwise
    UI::Material::Icon icon = UI::Material::Icon::Star;
    Motion motion = Motion::Float;
    std::wstring title, subtitle;
    std::wstring pill;                       // "" = none
    UI::Material::Icon pillIcon = UI::Material::Icon::Information;
    std::wstring pillLogo;                   // a picture (relative to Assets) instead of the icon
};

void TabBanner(UI::Ui& ui, const Rect& r, const TabBannerSpec& spec, double loadedAt);

inline constexpr float kTabBannerHeight = 118.f;

} // namespace SonicRacingSaveManager::Main
