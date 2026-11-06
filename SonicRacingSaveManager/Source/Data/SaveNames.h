#pragma once
#include <string_view>

// The names the game itself shows (its Game.locres, en / es / fr / it) for the ids the save keeps.
// Keys are "<list>:<id>": driver:Sonic, machine:SPD_01, front: / rear: / tire:SPD_01, gadget:Item2_PlusStock,
// horn:HORNTYPE_01, aura:MACHINE_AURA_001, challenge:CHALLENGE001, sticker:10004, title:5, track:10011,
// stageidx:<EStageId index>, stage:STG1016, cup:<GrandPrixCompletionStatus slot>, speed:<EMenuSpeedClassId>; "<key>#unlock" is how the game says to get it
// (parts, auras). An id the game has no name for isn't in the game yet.
namespace SonicRacingSaveManager::Data::SaveNames {

// nullptr when the game has no name for it; `lang` is the app's language ("en", "es", "fr", "it")
const wchar_t* Find(std::string_view key, std::wstring_view lang);

} // namespace SonicRacingSaveManager::Data::SaveNames
