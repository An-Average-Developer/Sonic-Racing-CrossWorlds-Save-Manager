#pragma once
#include <string>
#include <vector>

// Every id the save editor knows per list, for "unlock" / "add what's missing". The enum ones are
// the value names without their "EDriverId::" prefix; ids with "Extnd" / "EXTND" are DLC content
// and are only touched when the save already has them.
namespace SonicRacingSaveManager::Data::SaveCatalog {

const std::vector<std::wstring>& Drivers();        // EDriverId
const std::vector<std::wstring>& Machines();       // EMachineId: the bodies and parts (TYPE_01..20, TYPE_51)
const std::vector<std::wstring>& Gadgets();        // EGadgetId
const std::vector<std::wstring>& Horns();          // EMachineHornType
const std::vector<std::wstring>& Auras();          // EMachineAuraId
const std::vector<std::wstring>& ColorPresets();   // EMachineColorPresetId
const std::vector<std::wstring>& Challenges();     // EChallengeId
const std::vector<int>& Stickers();
const std::vector<int>& HonorTitles();
const std::vector<int>& Albums();
const std::vector<int>& Tracks();

// The character an EDriverId is a version of ("SonicRacer", "SuperSonic", "Werehog" -> "Sonic";
// "EggpawnRed" -> "Eggpawn"); its own id when it has no other versions
std::wstring DriverFamily(const std::wstring& id);

// The honor titles that go together (DT_HonorTitleListDataTable): a character's four (its name, 104 +
// 7 per EDriverId, and three more), a DLC festival's six (xx1..xx6, shown by "<X> Festival", xx4), the
// World Championship's seven (2000..2006), Season 2's shop titles (9001..9015, as 9001), its friends /
// squad ones (9900) and its feats (6001); the title that stands for them, or the id itself
int TitleFamily(int id);
// Their order: 0 characters, 1 festivals, 2 the World Championship, 3 the rest
int TitleGroup(int id);

// What a challenge (CHALLENGE001.., by its number) needs, in the unit its StatsCount counts in
// (DT_ChallengeListData: "Obtain 10 Super Item Boxes" 10, "Drive the length of the Earth" 40000);
// 0 when unknown
int ChallengeGoal(int number);

} // namespace SonicRacingSaveManager::Data::SaveCatalog
