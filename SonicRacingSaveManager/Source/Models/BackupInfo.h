#pragma once
#include <string>
#include <vector>
#include "Core/Time.h"

namespace SonicRacingSaveManager::Models {

// Features\Backup\Models\BackupInfo.cs — a folder under %APPDATA%\SEGA\SonicRacingCrossWorlds\Backups
struct BackupInfo {
    std::wstring name;        // the folder name: what identifies the backup (selection, restore, delete)
    std::wstring path;
    std::wstring accountId;   // "Unknown" for folders without backup_info.json
    std::wstring accountName; // the Steam name of the account ("" when unknown)
    std::wstring title;       // the name typed when it was created ("" = none, or an older backup)
    bool autoTitle = false;   // created without a name (backup_info.json from 3.0.1 on)
    Time::Ticks backupDate = 0;
    int fileCount = 0;
    std::vector<std::wstring> files;

    // "yyyy-MM-dd HH:mm:ss"
    std::wstring DisplayDate() const;
    // "Just now", "5m ago", "3h ago", "2d ago", else DisplayDate (translated)
    std::wstring TimeAgo() const;
    // What the lists show: the typed name, "Abril's backup" for one created without a name,
    // "Imported backup"; older folders lose their "backup_" prefix and timestamp (translated)
    std::wstring DisplayTitle() const;
    // The account's Steam name, else its id
    std::wstring AccountDisplay() const { return accountName.empty() ? accountId : accountName; }
};

// BackupMetadata — backup_info.json (Newtonsoft, PascalCase, indented). Title and AccountName are
// new in 3.0.1 (older files don't have them).
struct BackupMetadata {
    std::wstring accountId;
    std::wstring backupDate;   // DateTime.Now.ToString("o")
    std::vector<std::wstring> files;
    std::wstring backupName;
    std::wstring title, accountName;
    bool hasTitle = false;     // the file has "Title" (even empty: created without a name)
};

} // namespace SonicRacingSaveManager::Models
