#pragma once
#include <cstdint>
#include <string>

namespace SonicRacingSaveManager::Models {

// Features\MemoryEditor\Models\MemoryValue.cs (the Tickets entry of the Ticket Editor)
struct MemoryValue {
    std::wstring name;
    int32_t currentValue = 0;
    int32_t newValue = 0;
    std::wstring description;
    int64_t address = 0;   // absolute address found with the ticket search; 0 until located
};

// Features\MemoryEditor\Models\GameProcessInfo.cs (the manual attach list)
struct GameProcessInfo {
    uint32_t id = 0;
    std::wstring displayName;   // "SonicRacingCrossWorldsSteam.exe  (PID: 1234)"
};

} // namespace SonicRacingSaveManager::Models
