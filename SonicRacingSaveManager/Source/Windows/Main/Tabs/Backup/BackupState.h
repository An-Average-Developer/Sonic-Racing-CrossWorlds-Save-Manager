#pragma once
#include <functional>
#include <string>
#include <vector>
#include "Models/BackupInfo.h"
#include "Models/SaveAccount.h"
#include "Services/Backup/BackupService.h"

namespace SonicRacingSaveManager::Main {

class MainState;

// The backup half of ViewModels\MainViewModel.cs: the accounts and backups shared by the Create
// Backup, Restore and Manage tabs, their selections and commands, and the header's Import Save
// Files. Every command runs the BackupService on a worker thread, shows its progress in the status
// bar and reports the result with the manager's dialogs (the C#'s MessageBoxes). Selections are
// kept by id / name, so a refresh doesn't drop them when the item is still there.
class BackupState {
public:
    explicit BackupState(MainState& state);
    // InitializeAsync: accounts, backups, then the first account selected in both pickers
    void Initialize();

    const std::vector<Models::SaveAccount>& Accounts() const { return accounts; }
    const std::vector<Models::BackupInfo>& Backups() const { return backups; }
    bool AccountsLoaded() const { return accountsLoaded; }
    bool BackupsLoaded() const { return backupsLoaded; }
    // Increments whenever the lists are reloaded (tabs replay their entrance)
    unsigned Revision() const { return revision; }

    // ── Selections (SelectedAccount / SelectedRestoreAccount / SelectedBackup / SelectedManageBackup) ──
    std::wstring selectedAccountId, selectedRestoreAccountId;
    std::wstring selectedBackupName, selectedManageBackupName;
    std::wstring backupName;   // "Backup Name (Optional)"
    const Models::SaveAccount* SelectedAccount() const { return FindAccount(selectedAccountId); }
    const Models::SaveAccount* SelectedRestoreAccount() const { return FindAccount(selectedRestoreAccountId); }
    const Models::BackupInfo* SelectedBackup() const { return FindBackup(selectedBackupName); }
    const Models::BackupInfo* SelectedManageBackup() const { return FindBackup(selectedManageBackupName); }

    // ── Commands (CanExecute where the C# had one) ──
    void RefreshAccounts(std::function<void()> then = {});
    void RefreshBackups(std::function<void()> then = {});
    bool CanCreateBackup() const { return SelectedAccount() != nullptr && !busy; }
    void CreateBackup();
    bool CanRestore() const { return SelectedBackup() != nullptr && !busy; }
    void RestoreBackup();           // Restore (Original)
    void RestoreBackupToCustom();   // Restore (Selected)
    bool CanManage() const { return SelectedManageBackup() != nullptr && !busy; }
    void DeleteBackup();
    void ExportBackup();
    void ImportBackup();
    void ImportFilesDirectly();     // header: Import Save Files
    void OpenBackupFolder() const { service.OpenBackupFolder(); }
    void OpenSaveFolder() const { service.OpenSaveFolder(); }

    const std::wstring& SaveDirectory() const { return service.BaseSaveDirectory(); }
    const std::wstring& BackupDirectory() const { return service.BackupDirectory(); }
    bool Busy() const { return busy > 0; }

private:
    const Models::SaveAccount* FindAccount(const std::wstring& id) const;
    const Models::BackupInfo* FindBackup(const std::wstring& name) const;
    // Runs `work` (may throw Backup::Error) on a worker thread with the status bar busy; `done`
    // gets the result, `failed` the error message
    template <typename T>
    void Run(const std::wstring& status, std::function<T()> work, std::function<void(T)> done, std::function<void(const std::wstring&)> failed);
    void Fail(const char* titleKey, const char* messageKey, const std::wstring& error);

    MainState& state;
    Services::Backup::BackupService service;
    std::vector<Models::SaveAccount> accounts;
    std::vector<Models::BackupInfo> backups;
    bool accountsLoaded = false, backupsLoaded = false;
    unsigned revision = 0;
    int busy = 0;
};

} // namespace SonicRacingSaveManager::Main
