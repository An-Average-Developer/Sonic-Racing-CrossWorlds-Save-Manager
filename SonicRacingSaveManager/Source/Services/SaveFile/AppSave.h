#pragma once
#include <cstdint>
#include <exception>
#include <string>
#include <vector>
#include "Services/SaveFile/Gvas.h"

// app.sav as the Save Editor tab sees it: the profile's numbers and the lists of things you own or
// have unlocked (characters, machines, parts, customisation, gadgets, progress). Every list row is
// Unlocked, Locked (the save has it but locked) or Missing (not in the save yet); Unlock adds a
// missing one. DLC ids ("Extnd") are only listed when the save already has them.
namespace SonicRacingSaveManager::Services::SaveFile {

class SaveError : public std::exception {
public:
    explicit SaveError(std::wstring message) : text(std::move(message)) {}
    const std::wstring& Message() const { return text; }
    const char* what() const noexcept override { return "SonicRacingSaveManager::SaveFile::SaveError"; }

private:
    std::wstring text;
};

enum class ListId {
    Drivers, Machines, FrontParts, RearParts, TireParts,
    ColorPresets, Stickers, Horns, Auras, Gadgets,
    GrandPrix, TimeTrial, RedStarRings, Challenges, HonorTitles, Albums, Tracks, Tutorials,
    Count
};

enum class ItemState { Unlocked, Locked, Missing };

struct ItemRow {
    std::wstring id;       // the key in the save ("Sonic", "SPD_01", "10003", "3" for an index)
    std::wstring name;     // shown
    ItemState state = ItemState::Missing;
    std::wstring detail;   // "3 / 5", "New"...
    std::wstring hint;     // how the game says to get it ("Win against Team Dark in Race Park")
    bool canLock = true;   // false for what the game needs (the selected character / machine...)
    bool canUnlock = true; // false for DLC the game unlocks from your Steam purchases, not the save
    bool unreleased = false;   // an id the game has no name for: not in the game yet
    bool dlc = false;          // DLC: Steam decides whether you have it, the editor never changes it
    bool shop = false;         // sold in the game's shop (for Donpa Tickets or Duel Medals)
    int64_t progress = 0, goal = 0;   // a challenge's count and what it needs (goal 0: none)
    // Its separate parts, when it has some: a course's 5 Red Star Rings, its 2 Time Trial rewards
    std::vector<bool> flags;
};

// A course as the Save Editor's Courses section shows it: its Red Star Rings, its Grand Prix at each
// speed class and its Time Trial records
inline constexpr int kSpeedCount = 5;              // EMenuSpeedClassId: Normal, High, Sonic, Super Sonic, Mirror (Sonic)
inline constexpr int kTrialSpeeds[2] = { 2, 3 };   // the Time Trial's: Sonic and Super Sonic
inline constexpr int kCupWon = 6;                  // GrandPrixCompletionStatus: the best result, per speed class

struct TrialRecord {
    bool valid = false;
    int64_t time = 0;          // 1/6000 s (the game's target times are in ms: 6 of these each)
    int64_t laps[3] = {};
    std::wstring rank;         // ETimeTrialClearRankId value name: "Gold"...
    std::wstring date;         // "2025-09-27T22:31:34.386Z"
    std::wstring driver;       // EDriverId value name
    bool reward = false;       // its reward (Acquistion) was received
};

struct Course {
    std::wstring id;           // EStageId value name: "STG1016"
    int index = -1;            // slot in the per-course arrays
    std::wstring name;
    std::vector<bool> rings;   // its 5 Red Star Rings (one set for every speed class)
    int cup = -1;              // GrandPrixCompletionStatus slot, -1 for none
    std::wstring cupName;
    bool cupOpen = false;
    int cupRank[kSpeedCount] = { -1, -1, -1, -1, -1 };   // per EMenuSpeedClassId; kCupWon = won, -1 = none
    TrialRecord trials[2];     // Sonic, Super Sonic
};

// A machine of the player's garage (_UserMachineAssemblyData.CustomizeDataList): its slot (an EMachineId
// of its type, "DSH_102"; 10 per type), the parts it is built from and how it is dressed
struct MachineBuild {
    std::wstring slot;
    bool used = false;         // built in the game (the rest are empty slots)
    bool selected = false;     // the one raced with (_UserCommonData.SelectedMachineId)
    std::wstring front, rear, tire;   // EMachineId value names of its parts ("DSH_13")
    std::wstring colorPreset;  // EMachineColorPresetId value name ("ColorPreset44"); empty with custom colours
    std::wstring colorName;    // its name as the colour list shows it
    std::wstring horn;         // EMachineHornType value name
    std::wstring aura;         // EMachineAuraId value name
    int stickers = 0;          // stickers put on it
};

// What of a machine can be changed
enum class BuildPart { Front, Rear, Tire, Color, Horn, Aura };

// Gadget presets (_UserMachineAssemblyData.GadgetPresetDataLists): one list per local player (split
// screen), 5 presets each, a plate of 6 slots; a gadget takes 1..4 slots side by side from its SlotIndex.
// The one each player races with is _UserCommonData.SelectedGadgetPresetIds[player].
// Since Season 2: 10 presets (A..J) and a 7th, special slot past the plate's 6 (SpGadgetPresetDataLists:
// one gadget of GadgetCatalog::Specials, SlotIndex 6), open when GadgetSlotNum reaches 7
inline constexpr int kGadgetPlayers = 4, kGadgetPresets = 10, kGadgetPlate = 6, kGadgetSpecialSlot = 6, kGadgetSlotsAll = 7;
struct PlacedGadget {
    std::wstring id;   // EGadgetId value name
    int slot = 0;      // its first slot
    int size = 1;      // the slots it takes
};
struct GadgetPreset {
    int index = 0;
    bool selected = false;
    std::vector<PlacedGadget> gadgets;   // by slot
    std::wstring special;                // the special slot's gadget (EGadgetId value name), empty when none
};

struct NumberField {
    std::string key;       // translation key of the label: "savefile_num_<key>"
    int64_t value = 0;
    int64_t min = 0, max = INT32_MAX;
    int64_t suggested = 0; // what "Max" sets (a value the game shows well)
    bool currency = false; // tickets, Duel Medals, Red Star Rings...: the Currencies section's
};

// The game's statistics (_StatsData): a character's races and wins, a machine's races (DriverList /
// MachineList, only the ones raced with)
struct StatUsage {
    std::wstring id;       // EDriverId / EMachineId value name ("Sonic", "DSH_102")
    int64_t used = 0;
    int64_t wins = 0;
    bool hasWins = false;  // the characters' (the machines count races only)
};

// Season 2's Play Missions (_UserPlayMissionData): periods (a week) of missions the game gets from its
// server, each to do a number of times for a reward. Done = Cleared; the game then hands the reward when
// it is received in its Play Missions menu (RewardState Claimed).
enum class MissionState { NotStarted, InProgress, Cleared };
inline constexpr int kRewardDonpaTickets = 12, kRewardDuelMedals = 18;   // ERewardType
struct Mission {
    int period = 0, index = 0;   // its place: _UserPlayMissionData[period].MissionDetails[index]
    int id = 0, type = 0;        // MissionId, MissionTypeId (Data::MissionCatalog)
    int64_t params[3] = {};      // MissionParameter A, B, C
    int64_t progress = 0, goal = 1;
    MissionState state = MissionState::NotStarted;
    bool claimed = false;
    int rewardType = -1;         // ERewardType (kRewardDonpaTickets...)
    int64_t rewardCount = 0;
    int64_t endsAt = 0;          // its period's end as a UTC FILETIME (0: unknown)
};

// The extras the Konami code shows in the Save Editor: the World Championship 2026's placing titles (2000
// Grand Champion .. 2005 Finalist), which only the event's real ranking gives. Hidden from the lists (and
// from Unlock all / Unlock EVERYTHING) unless the save has them already.
void SetKonamiExtras(bool on);
bool KonamiExtras();

class AppSave {
public:
    // Reads and decrypts; throws SaveError
    static AppSave Open(const std::wstring& path);
    // Encrypts and writes over the file; throws SaveError
    void Write();
    // The same in steps, for writing on a worker thread: the encrypted bytes, writing them (any
    // thread; throws SaveError), then Written on the save once they are on disk
    std::string Encoded() const;
    static void WriteFile(const std::wstring& path, const std::string& bytes);
    void Written(uint64_t bytes);

    const std::wstring& Path() const { return path; }
    uint64_t Size() const { return size; }
    bool Dirty() const { return dirty; }
    // The game (or anything else) wrote the file since it was read
    bool ChangedOnDisk() const;

    std::vector<NumberField> Numbers() const;
    void SetNumber(const std::string& key, int64_t value);

    std::vector<ItemRow> Rows(ListId list) const;
    // false when there was nothing to change
    bool Unlock(ListId list, const std::wstring& id);
    bool Lock(ListId list, const std::wstring& id);
    int UnlockAll(ListId list);
    // One part of a row with flags (a single Red Star Ring of a course); false when nothing changed
    bool SetFlag(ListId list, const std::wstring& id, size_t index, bool on);
    int UnlockEverything();
    // Every challenge not done and its count at 0; how many changed
    int ResetChallenges();

    std::vector<Course> Courses() const;
    // The garage: every slot, built ones first (the selected one first of all)
    std::vector<MachineBuild> Garage() const;
    // Races with another built machine; false when nothing changed
    bool SetSelectedMachine(const std::wstring& slot);
    // Changes one thing of a built machine: a part (EMachineId value name), its colour preset (whose
    // colours it copies into the machine, as the game does), horn or aura; false when nothing changed
    bool SetBuild(const std::wstring& slot, BuildPart part, const std::wstring& value);
    // Builds a new machine of a type ("SPD") in its first free garage slot (101..105), from the
    // type's first machine; its slot, empty when the type's slots are all taken
    std::wstring NewBuild(const std::wstring& type);
    // Empties a garage slot (not the one raced with); false when nothing changed
    bool ClearBuild(const std::wstring& slot);

    // Gadget presets of a local player (0..3); empty when the save has none
    std::vector<GadgetPreset> GadgetPresets(int player) const;
    // The plate's unlocked slots (GadgetSlotNum, 1..6)
    int GadgetSlotCount() const;
    // The special slot is open (GadgetSlotNum 7)
    bool SpecialSlotOpen() const;
    // Puts a gadget in a preset's special slot (one of GadgetCatalog::Specials, not on its plate), or
    // empties it (empty id); false when nothing changed or it can't
    bool SetSpecialGadget(int player, int preset, const std::wstring& id);
    // Puts a gadget on a preset's plate from `slot`: it must fit in the free, unlocked slots and not be on it
    // already; false when it can't. (Unlocking it is the caller's.)
    bool PlaceGadget(int player, int preset, const std::wstring& id, int slot);
    // Takes the gadget at `slot` off a preset (any of its slots); false when there is none
    bool RemoveGadget(int player, int preset, int slot);
    // Takes every gadget off a preset
    bool ClearGadgetPreset(int player, int preset);
    // Races with that preset; false when nothing changed
    bool SelectGadgetPreset(int player, int preset);

    // The honor title shown on the player's card (_UserHonorTitleData.HonorTitleIndex); -1 when the
    // save has none. Only an unlocked title can be put on.
    int SelectedTitle() const;
    bool SetSelectedTitle(int id);
    // A course's Red Star Ring `ring`, its Time Trial reward at kTrialSpeeds[`trial`]; false when nothing changed
    bool SetRing(const Course& course, size_t ring, bool on);
    bool SetTrialReward(const Course& course, size_t trial, bool on);
    // A Time Trial record that exists (raced once in the game: it has a ghost): one lap's time (1/6000 s;
    // the total follows as the laps' sum) and its medal (ETimeTrialClearRankId value name: "Gold")
    bool SetTrialLap(int courseIndex, size_t trial, size_t lap, int64_t time);
    bool SetTrialRank(int courseIndex, size_t trial, const std::wstring& rank);
    // A Grand Prix: opened or not, its result at a speed class (0 none .. kCupWon; a result opens it)
    bool SetCupOpen(int cup, bool open);
    bool SetCupRank(int cup, int speed, int rank);

    // Statistics: the race totals (_StatsData.RaceDetails) as numbers whose keys start with "stat_" (SetNumber
    // writes them), and the characters' / machines' races and wins
    std::vector<NumberField> StatNumbers() const;
    std::vector<StatUsage> Usage(bool machines) const;
    bool SetUsage(bool machines, const std::wstring& id, bool wins, int64_t value);

    // Play Missions: every period's, in the save's order
    std::vector<Mission> Missions() const;
    // A mission's count (0..its goal); its state follows (the goal: Cleared, its reward still to receive in
    // the game); false when nothing changed
    bool SetMissionProgress(int period, int index, int64_t progress);
    // Every mission not done yet to its goal; how many changed
    int CompleteMissions();

private:
    AppSave() = default;
    Gvas::Props* Section(const char* name);
    const Gvas::Props* Section(const char* name) const;
    // The RaceData of a course's Time Trial record (nullptr when the save has none)
    Gvas::Value* TrialRace(int courseIndex, size_t trial);
    // A garage slot's UserMachineCustomizeData (nullptr when the save has none)
    Gvas::Value* BuildEntry(const std::wstring& slot);
    // A gadget preset's GadgetDataList array (nullptr when the save has none)
    Gvas::Value* GadgetList(int player, int preset);
    // Its special slot's EquippedSpGadgets array (nullptr when the save has none)
    Gvas::Value* SpecialList(int player, int preset);

    std::wstring path;
    uint64_t size = 0;
    int64_t writtenAt = 0;
    bool dirty = false;
    Gvas::File file;
};

} // namespace SonicRacingSaveManager::Services::SaveFile
