#pragma once
#include "UI/Ui.h"

// The Konami code (up up down down left right left right B A) typed in a tab: true on the key that
// completes it. `step` is the tab's progress through the sequence. What it unlocks is
// SaveFile::KonamiExtras (shared by the Save Editor and the Tickets Menu).
namespace SonicRacingSaveManager::Main {

bool KonamiTyped(UI::Ui& ui, int& step);

} // namespace SonicRacingSaveManager::Main
