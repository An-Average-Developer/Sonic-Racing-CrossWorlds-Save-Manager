#pragma once
#include <string>
#include <vector>
#include "Core/Time.h"

namespace SonicRacingSaveManager::Models {

// Features\Backup\Models\BackupInfo.cs — a folder under %APPDATA%\SEGA\SonicRacingCrossWorlds\Backups
struct BackupInfo {
    std::wstring name;        // the folder name (DisplayName)
    std::wstring path;
    std::wstring accountId;   // "Unknown" for folders without backup_info.json
    Time::Ticks backupDate = 0;
    int fileCount = 0;
    std::vector<std::wstring> files;

    // "yyyy-MM-dd HH:mm:ss"
    std::wstring DisplayDate() const;
    // "Just now", "5m ago", "3h ago", "2d ago", else DisplayDate (translated)
    std::wstring TimeAgo() const;
};

// BackupMetadata — backup_info.json (Newtonsoft, PascalCase, indented)
struct BackupMetadata {
    std::wstring accountId;
    std::wstring backupDate;   // DateTime.Now.ToString("o")
    std::vector<std::wstring> files;
    std::wstring backupName;
};

} // namespace SonicRacingSaveManager::Models
