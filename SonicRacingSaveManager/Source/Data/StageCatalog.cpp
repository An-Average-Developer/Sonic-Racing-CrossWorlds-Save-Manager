#include "Data/StageCatalog.h"
#include <cwchar>

namespace SonicRacingSaveManager::Data::StageCatalog {

const std::vector<Course>& Courses() {
    // DT_GrandPrixStages, the cups whose courses are all in the game (SegaGP_02 / 03, Collabo_03 and
    // AnotherWorld list test courses or the CrossWorld pool)
    static const std::vector<Course> list = {
        { L"STG1016", 1 },  { L"STG1017", 1 },  { L"STG1018", 1 },    // Purple_01
        { L"STG1023", 2 },  { L"STG1021", 2 },  { L"STG1020", 2 },    // Yellow_02
        { L"STG1034", 3 },  { L"STG1005", 3 },  { L"STG1027", 3 },    // Red_03
        { L"STG1025", 4 },  { L"STG1032", 4 },  { L"STG1024", 4 },    // Blue_04
        { L"STG1022", 5 },  { L"STG1003", 5 },  { L"STG1030", 5 },    // Cyan_05
        { L"STG1029", 6 },  { L"STG1001", 6 },  { L"STG1033", 6 },    // White_06
        { L"STG1026", 7 },  { L"STG1028", 7 },  { L"STG1031", 7 },    // Green_07
        { L"STG1035", 8 },  { L"STG1036", 8 },  { L"STG1037", 8 },    // SegaGP_01
        { L"STG1503", 11 }, { L"STG1506", 11 }, { L"STG1502", 11 },   // Collabo_01
        { L"STG1501", 12 }, { L"STG1505", 12 }, { L"STG1508", 12 },   // Collabo_02
        // The CrossWorlds
        { L"STG2001", -1 }, { L"STG2002", -1 }, { L"STG2003", -1 }, { L"STG2004", -1 }, { L"STG2005", -1 }, { L"STG2006", -1 },
        { L"STG2007", -1 }, { L"STG2008", -1 }, { L"STG2009", -1 }, { L"STG2010", -1 }, { L"STG2011", -1 }, { L"STG2012", -1 },
        { L"STG2013", -1 }, { L"STG2014", -1 }, { L"STG2015", -1 }, { L"STG2016", -1 }, { L"STG2017", -1 }, { L"STG2018", -1 },
        { L"STG2019", -1 }, { L"STG2020", -1 },
    };
    return list;
}

int StageIndex(const std::wstring& id) {
    // EStageId: STG1001-1042, 1501-1510, 1901-1902, 2001-2020, 3001-3010, 4001-4010, one after another
    struct Block {
        int first, count;
    };
    static constexpr Block kBlocks[] = { { 1001, 42 }, { 1501, 10 }, { 1901, 2 }, { 2001, 20 }, { 3001, 10 }, { 4001, 10 } };
    if (id.size() != 7 || id.compare(0, 3, L"STG") != 0) return -1;
    const int n = static_cast<int>(std::wcstol(id.c_str() + 3, nullptr, 10));
    int index = 0;
    for (const Block& b : kBlocks) {
        if (n >= b.first && n < b.first + b.count) return index + n - b.first;
        index += b.count;
    }
    return -1;
}

} // namespace SonicRacingSaveManager::Data::StageCatalog
