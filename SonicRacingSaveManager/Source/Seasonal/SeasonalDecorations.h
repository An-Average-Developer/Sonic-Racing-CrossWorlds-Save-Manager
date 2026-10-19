#pragma once
#include "Graphics/Graphics.h"

// SeasonalDecorations.cs: the seasonal:SeasonalDecorations.Decoration attached property, drawn on
// top of an element (never blocks clicks). Draws nothing while no season is active.
namespace SonicRacingSaveManager::Seasonal::Decorations {

// Decoration="Card": corner decorations; `variant` picks the arrangement (fixed per card so they
// don't reshuffle). Wide cards (>= 500) get the richer scene. `opacity` follows the card's fade-in.
void Card(Graphics& g, const Rect& card, int variant, double time, float opacity = 1.f);
// Decoration="Header": the scene in the middle of the app header bar.
void Header(Graphics& g, const Rect& header, double time);

} // namespace SonicRacingSaveManager::Seasonal::Decorations
