#pragma once
#include <string>
#include <vector>

// The courses the Save Editor shows, from the game's DT_GrandPrixStages (which three courses each Grand
// Prix has) and EStageId (the order of the save's per-course arrays).
namespace SonicRacingSaveManager::Data::StageCatalog {

struct Course {
    const wchar_t* id;   // EStageId value name: "STG1016"
    int cup;             // its Grand Prix (GrandPrixCompletionStatus slot = EGrandPrixId), -1 for none
};

// The cups' courses in the game's order, then the CrossWorlds (no cup of their own)
const std::vector<Course>& Courses();

// The course's slot in RedStarRingAcquisitionPerStage / TimeTrialCourseDataArray (its EStageId value);
// -1 for an id EStageId doesn't have
int StageIndex(const std::wstring& id);

} // namespace SonicRacingSaveManager::Data::StageCatalog
