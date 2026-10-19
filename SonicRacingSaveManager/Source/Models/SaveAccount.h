#pragma once
#include <string>
#include "Core/Time.h"

namespace SonicRacingSaveManager::Models {

// Features\Backup\Models\SaveAccount.cs — one %APPDATA%\SEGA\SonicRacingCrossWorlds\steam\<id> folder
struct SaveAccount {
    std::wstring accountId;
    std::wstring accountName;   // Steam persona name, "" when it couldn't be found
    std::wstring path;
    int fileCount = 0;          // *.sav files
    Time::Ticks lastModified = 0;

    // "Name (id)" or just the id
    std::wstring ShortDisplay() const { return accountName.empty() ? accountId : accountName + L" (" + accountId + L")"; }
};

} // namespace SonicRacingSaveManager::Models
