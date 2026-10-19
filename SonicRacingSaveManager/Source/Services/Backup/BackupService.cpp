#include "Services/Backup/BackupService.h"
#include <algorithm>
#include <cwctype>
#include "App/Config.h"
#include "Core/Files.h"
#include "Core/Process.h"
#include "Core/Text.h"
#include "Localization/Localization.h"

namespace SonicRacingSaveManager::Services::Backup {

namespace {

constexpr const wchar_t* kSaveExtensions[] = { L".sav", L".vdf" };

// long.TryParse: optional surrounding whitespace and sign, digits only, fits in 64 bits
bool IsLong(const std::wstring& text) {
    const std::wstring t = Text::Trim(text);
    size_t i = 0;
    if (i < t.size() && (t[i] == L'+' || t[i] == L'-')) ++i;
    if (i == t.size()) return false;
    unsigned long long value = 0;
    for (; i < t.size(); ++i) {
        if (!std::iswdigit(t[i])) return false;
        if (value > (9223372036854775808ULL - (t[i] - L'0')) / 10) return false;
        value = value * 10 + (t[i] - L'0');
    }
    return true;
}

Time::Ticks LastWrite(const std::wstring& path) { return Time::FromFileTimeUtc(Files::LastWriteTime(path).value_or(0)); }

} // namespace

BackupService::BackupService() {
    const std::wstring game = Files::Combine(Files::RoamingAppData(), Config::Game::SegaFolder, Config::Game::GameFolder);
    baseSaveDir = Files::Combine(game, Config::Game::SavesFolder);
    backupDir = Files::Combine(game, Config::Game::BackupsFolder);
    Files::CreateDirectories(backupDir);
}

std::vector<Models::SaveAccount> BackupService::GetSaveAccounts() const {
    std::vector<Models::SaveAccount> accounts;
    if (!Files::DirectoryExists(baseSaveDir)) return accounts;
    for (const auto& dir : Files::ListDirectories(baseSaveDir)) {
        const std::wstring dirName = Files::FileName(dir);
        if (!IsLong(dirName)) continue;
        const auto saveFiles = Files::ListFiles(dir, L"*.sav");
        if (saveFiles.empty()) continue;
        Models::SaveAccount account;
        account.accountId = dirName;
        account.accountName = GetSteamAccountName(dirName);
        account.path = dir;
        account.fileCount = static_cast<int>(saveFiles.size());
        account.lastModified = LastWrite(dir);
        accounts.push_back(std::move(account));
    }
    // OrderByDescending(LastModified) is stable
    std::stable_sort(accounts.begin(), accounts.end(), [](const auto& a, const auto& b) { return a.lastModified > b.lastModified; });
    return accounts;
}

std::vector<std::wstring> BackupService::GetSaveFiles(const std::wstring& accountPath) const {
    std::vector<std::wstring> saveFiles;
    for (const wchar_t* ext : kSaveExtensions) {
        const auto found = Files::ListFiles(accountPath, std::wstring(L"*") + ext);
        saveFiles.insert(saveFiles.end(), found.begin(), found.end());
    }
    // OrderBy(Path.GetFileName): ordinal-insensitive in the C# default culture is close enough to a
    // case-insensitive compare here (save names are ASCII)
    std::stable_sort(saveFiles.begin(), saveFiles.end(),
                     [](const auto& a, const auto& b) { return Text::Lower(Files::FileName(a)) < Text::Lower(Files::FileName(b)); });
    return saveFiles;
}

CreatedBackup BackupService::CreateBackup(const std::wstring& accountId, const std::optional<std::wstring>& customName) const {
    const auto accounts = GetSaveAccounts();
    const auto account = std::find_if(accounts.begin(), accounts.end(), [&](const auto& a) { return a.accountId == accountId; });
    if (account == accounts.end()) throw Error(Loc::Fmt("backup_error_account_not_found", accountId));

    const Time::Ticks now = Time::Now();
    const std::wstring timestamp = Time::Stamp(now);
    const bool blank = !customName || Text::Trim(*customName).empty();
    const std::wstring backupName = blank ? L"backup_" + accountId + L"_" + timestamp : *customName + L"_" + timestamp;

    const std::wstring backupPath = Files::Combine(backupDir, backupName);
    if (!Files::CreateDirectories(backupPath)) throw Error(Loc::Fmt("backup_error_create_folder", backupPath));

    Models::BackupMetadata metadata;
    for (const auto& saveFile : GetSaveFiles(account->path)) {
        const std::wstring fileName = Files::FileName(saveFile);
        if (!Files::Copy(saveFile, Files::Combine(backupPath, fileName), true)) throw Error(Loc::Fmt("backup_error_copy", fileName));
        metadata.files.push_back(fileName);
    }
    metadata.accountId = accountId;
    metadata.backupDate = Time::RoundTrip(now);
    metadata.backupName = backupName;
    if (!WriteMetadata(Files::Combine(backupPath, Config::Game::BackupInfoFile), metadata))
        throw Error(Loc::Fmt("backup_error_write", Config::Game::BackupInfoFile));
    return { backupPath, static_cast<int>(metadata.files.size()) };
}

std::vector<Models::BackupInfo> BackupService::ListBackups() const {
    std::vector<Models::BackupInfo> backups;
    if (!Files::DirectoryExists(backupDir)) return backups;
    for (const auto& folder : Files::ListDirectories(backupDir)) {
        const std::wstring metadataFile = Files::Combine(folder, Config::Game::BackupInfoFile);
        if (Files::Exists(metadataFile)) {
            // Unreadable metadata or a date that doesn't parse: the backup is skipped (catch { })
            const auto metadata = ReadMetadata(metadataFile);
            if (!metadata) continue;
            const auto date = Time::Parse(metadata->backupDate);
            if (!date) continue;
            Models::BackupInfo info;
            info.name = Files::FileName(folder);
            info.path = folder;
            info.accountId = metadata->accountId;
            info.backupDate = *date;
            info.fileCount = static_cast<int>(metadata->files.size());
            info.files = metadata->files;
            backups.push_back(std::move(info));
        } else {
            const auto saveFiles = Files::ListFiles(folder, L"*.sav");
            if (saveFiles.empty()) continue;
            Models::BackupInfo info;
            info.name = Files::FileName(folder);
            info.path = folder;
            info.accountId = L"Unknown";
            info.backupDate = LastWrite(folder);
            info.fileCount = static_cast<int>(saveFiles.size());
            for (const auto& f : saveFiles) info.files.push_back(Files::FileName(f));
            backups.push_back(std::move(info));
        }
    }
    std::stable_sort(backups.begin(), backups.end(), [](const auto& a, const auto& b) { return a.backupDate > b.backupDate; });
    return backups;
}

int BackupService::RestoreBackup(const std::wstring& backupName, const std::optional<std::wstring>& targetAccountId) const {
    const std::wstring backupPath = Files::Combine(backupDir, backupName);
    if (!Files::DirectoryExists(backupPath)) throw Error(Loc::Fmt("backup_error_not_found", backupName));

    std::wstring originalAccount;
    const std::wstring metadataFile = Files::Combine(backupPath, Config::Game::BackupInfoFile);
    if (Files::Exists(metadataFile)) {
        // The C# let a corrupt file throw here
        const auto metadata = ReadMetadata(metadataFile);
        if (!metadata) throw Error(Loc::Fmt("backup_error_read", Config::Game::BackupInfoFile));
        originalAccount = metadata->accountId;
    }
    // targetAccountId ??= originalAccount
    const std::wstring target = targetAccountId ? *targetAccountId : originalAccount;
    if (target.empty()) throw Error(Loc::Get("backup_error_no_target"));

    const std::wstring targetDir = Files::Combine(baseSaveDir, target);
    Files::CreateDirectories(targetDir);
    int restoredCount = 0;
    for (const wchar_t* ext : kSaveExtensions) {
        for (const auto& saveFile : Files::ListFiles(backupPath, std::wstring(L"*") + ext)) {
            const std::wstring fileName = Files::FileName(saveFile);
            if (!Files::Copy(saveFile, Files::Combine(targetDir, fileName), true)) throw Error(Loc::Fmt("backup_error_copy", fileName));
            ++restoredCount;
        }
    }
    return restoredCount;
}

void BackupService::DeleteBackup(const std::wstring& backupName) const {
    const std::wstring backupPath = Files::Combine(backupDir, backupName);
    if (!Files::DirectoryExists(backupPath)) throw Error(Loc::Fmt("backup_error_not_found", backupName));
    if (!Files::DeleteDirectory(backupPath)) throw Error(Loc::Fmt("backup_error_delete", backupName));
}

// Process.Start("explorer.exe", dir) when the folder exists
void BackupService::OpenBackupFolder() const {
    if (Files::DirectoryExists(backupDir)) Process::Open(backupDir);
}

void BackupService::OpenSaveFolder() const {
    if (Files::DirectoryExists(baseSaveDir)) Process::Open(baseSaveDir);
}

} // namespace SonicRacingSaveManager::Services::Backup
