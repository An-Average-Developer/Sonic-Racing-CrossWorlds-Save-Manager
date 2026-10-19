#pragma once
#include "Graphics/Graphics.h"

namespace SonicRacingSaveManager::Seasonal::Backdrop {

// Scenery behind the whole main screen for the active season (moon and graves, sunset beach, snowy
// hills); the garland hangs from the bottom of the top bar. Draws nothing outside a season.
// parallaxX / parallaxY (-1..1) shift the layers with the mouse: far ones less, near ones more.
void Draw(Graphics& g, const Rect& screen, float topBarHeight, double time, float parallaxX = 0.f, float parallaxY = 0.f);

} // namespace SonicRacingSaveManager::Seasonal::Backdrop
