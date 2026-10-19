#pragma once
#include <string>

// ThemeService.ActiveSeason / SeasonChanged for the seasonal art. The colors of each season live in
// UI/Material/Theme (the MaterialDesign palettes); this side only knows which season is drawn.
namespace SonicRacingSaveManager::Seasonal {

enum class Season { Classic, Halloween, Summer, Christmas };

// The season follows the season theme setting; themes without visuals use the classic look.
Season Active();
bool IsSeasonal();
std::wstring Name(Season season);   // "classic" | "halloween" | "summer" | "christmas"
void Refresh();                     // re-read after the season theme setting changes
// Light / dark mode of the app (set by UI::Material::Theme). The night scenery is only drawn in
// dark mode, and the art switches some pens to dark ones on light backgrounds (SyncArt).
void SetDarkMode(bool dark);
bool DarkMode();
// Increments when the season or the mode changes (ThemeService.SeasonChanged)
unsigned Revision();

// Clock of the seasonal animations (scenery, decorations, particles). While `paused` it slows down
// smoothly to a stop over ~1.6 s, and speeds back up the same way.
double AnimationTime(double realTime, bool paused);
double LastAnimationTime();   // value returned by the last AnimationTime call
bool AnimationStopped();      // fully stopped: nothing seasonal moves, no need to redraw for it

} // namespace SonicRacingSaveManager::Seasonal
