#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Season 2's Play Missions as the game describes them (DT_PlayMissionDisplayText): a mission type
// (MissionTypeId, 1101001...), its text and which of its 3 parameters (A, B, C) it counts to and fills
// {Rank} / {Level} / {Progress} with. MissionCatalog.cpp is generated (scratch genmissions.py from the
// game's files); the lookups are in MissionCatalogLookup.cpp.
namespace SonicRacingSaveManager::Data::MissionCatalog {

struct MissionType {
    int id;
    int goal;    // the parameter that is the count to reach: 0 A, 1 B, 2 C
    int value;   // the one {Rank} / {Level} / {Progress} shows, -1 none
    const wchar_t* text[4];   // en, es, fr, it
};

const std::vector<MissionType>& All();
// "1st".."12th" (DB_RaceData race_rank_NN_text) per language
extern const wchar_t* const kRanks[12][4];

const MissionType* Find(int id);
// Its text in a language ("en", "es", "fr", "it"; English for any other) with its parameters put in;
// a type the catalog doesn't know shows its id
std::wstring Text(int id, const int64_t params[3], const std::wstring& lang);
// The count it needs (its goal parameter; 1 when it has none)
int64_t Goal(int id, const int64_t params[3]);

} // namespace SonicRacingSaveManager::Data::MissionCatalog
