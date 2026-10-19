#pragma once
#include <exception>
#include <optional>
#include <string>
#include <vector>
#include "Models/BackupInfo.h"
#include "Models/SaveAccount.h"

// Features\Backup\Services\BackupService.cs — the save folders (%APPDATA%\SEGA\SonicRacingCrossWorlds
// \steam\<id>\*.sav + .vdf) and the backups (...\Backups\<name>\ with backup_info.json). Blocking:
// the tabs call it from worker threads. Split by area:
//   BackupService.cpp    accounts, create / list / restore / delete, opening the folders
//   BackupTransfer.cpp   export / import (.zip or folder) and Import Save Files
//   BackupMetadata.cpp   backup_info.json
//   SteamPersona.cpp     the Steam name of an account (GetSteamAccountName)
namespace SonicRacingSaveManager::Services::Backup {

// The C# threw Exception(message) and the view models showed ex.Message: the same messages travel
// in this exception (translated)
class Error : public std::exception {
public:
    explicit Error(std::wstring message) : text(std::move(message)) {}
    const std::wstring& Message() const { return text; }
    const char* what() const noexcept override { return "SonicRacingSaveManager::Backup::Error"; }

private:
    std::wstring text;
};

struct CreatedBackup {
    std::wstring backupPath;
    int fileCount = 0;
};

class BackupService {
public:
    BackupService();   // creates the Backups folder

    const std::wstring& BaseSaveDirectory() const { return baseSaveDir; }
    const std::wstring& BackupDirectory() const { return backupDir; }

    // Numeric folders with at least one .sav, newest first
    std::vector<Models::SaveAccount> GetSaveAccounts() const;
    // *.sav then *.vdf of a folder, sorted by file name
    std::vector<std::wstring> GetSaveFiles(const std::wstring& accountPath) const;

    // Everything below throws Error on failure
    CreatedBackup CreateBackup(const std::wstring& accountId, const std::optional<std::wstring>& customName) const;
    std::vector<Models::BackupInfo> ListBackups() const;   // newest first
    // Restores into `targetAccountId`, or into the backup's own account when nullopt
    int RestoreBackup(const std::wstring& backupName, const std::optional<std::wstring>& targetAccountId = std::nullopt) const;
    void DeleteBackup(const std::wstring& backupName) const;
    std::wstring ExportBackup(const std::wstring& backupName, std::wstring exportPath) const;
    std::wstring ImportBackup(const std::wstring& importPath) const;
    int ImportFilesDirectly(const std::wstring& filePath, const std::wstring& targetAccountId) const;

    void OpenBackupFolder() const;
    void OpenSaveFolder() const;

    // backup_info.json (BackupMetadata.cpp); nullopt when the file is missing or invalid
    static std::optional<Models::BackupMetadata> ReadMetadata(const std::wstring& path);
    static bool WriteMetadata(const std::wstring& path, const Models::BackupMetadata& metadata);

private:
    static std::wstring GetSteamAccountName(const std::wstring& steamId);   // SteamPersona.cpp

    std::wstring baseSaveDir, backupDir;
};

} // namespace SonicRacingSaveManager::Services::Backup
