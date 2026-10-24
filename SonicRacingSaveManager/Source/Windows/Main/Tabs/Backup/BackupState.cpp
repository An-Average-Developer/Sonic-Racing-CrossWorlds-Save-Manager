#include "Windows/Main/Tabs/Backup/BackupState.h"
#include <algorithm>
#include "Core/Async.h"
#include "Core/System.h"
#include "Core/Text.h"
#include "Localization/Localization.h"
#include "Windows/Dialogs/MessageDialog.h"
#include "Windows/Main/MainState.h"

namespace SonicRacingSaveManager::Main {

namespace {

namespace B = Services::Backup;
using Dialogs::MessageButtons;
using Dialogs::MessageIcon;
using Dialogs::MessageResult;

// A worker's outcome: the value, or the Backup::Error message
template <typename T>
struct Outcome {
    T value{};
    bool ok = false;
    std::wstring error;
};

} // namespace

BackupState::BackupState(MainState& mainState) : state(mainState) {}

template <typename T>
void BackupState::Run(const std::wstring& status, std::function<T()> work, std::function<void(T)> done,
                      std::function<void(const std::wstring&)> failed) {
    ++busy;
    state.BeginBusy(status);
    Async::Run(
        [work]() {
            Outcome<T> o;
            try {
                o.value = work();
                o.ok = true;
            } catch (const B::Error& e) {
                o.error = e.Message();
            }
            return o;
        },
        [this, done, failed](Outcome<T> o) {
            --busy;
            state.EndBusy();
            if (o.ok) done(std::move(o.value));
            else failed(o.error);
        });
}

void BackupState::Fail(const char* titleKey, const char* messageKey, const std::wstring& error) {
    state.statusMessage = Loc::Fmt("status_error", error);
    Dialogs::Message(state.Owner(), Loc::Fmt(messageKey, error), Loc::Get(titleKey), MessageButtons::Ok, MessageIcon::Error);
}

const Models::SaveAccount* BackupState::FindAccount(const std::wstring& id) const {
    if (id.empty()) return nullptr;
    const auto it = std::find_if(accounts.begin(), accounts.end(), [&](const auto& a) { return a.accountId == id; });
    return it == accounts.end() ? nullptr : &*it;
}

const Models::BackupInfo* BackupState::FindBackup(const std::wstring& name) const {
    if (name.empty()) return nullptr;
    const auto it = std::find_if(backups.begin(), backups.end(), [&](const auto& b) { return b.name == name; });
    return it == backups.end() ? nullptr : &*it;
}

void BackupState::Initialize() {
    RefreshAccounts([this] {
        RefreshBackups([this] {
            if (!accounts.empty()) {
                selectedAccountId = accounts.front().accountId;
                selectedRestoreAccountId = accounts.front().accountId;
            }
        });
    });
}

void BackupState::RefreshAccounts(std::function<void()> then) {
    Run<std::vector<Models::SaveAccount>>(
        Loc::Get("status_loading_accounts"), [this] { return service.GetSaveAccounts(); },
        [this, then](std::vector<Models::SaveAccount> result) {
            accounts = std::move(result);
            accountsLoaded = true;
            ++revision;
            const int n = static_cast<int>(accounts.size());
            state.statusMessage = n > 0 ? Loc::Fmt(n == 1 ? "status_accounts_found_one" : "status_accounts_found_many", n)
                                        : Loc::Get("status_no_saves");
            if (then) then();
        },
        [this, then](const std::wstring& error) {
            Fail("dialog_error", "backup_load_accounts_failed", error);
            if (then) then();
        });
}

void BackupState::RefreshBackups(std::function<void()> then) {
    Run<std::vector<Models::BackupInfo>>(
        Loc::Get("status_loading_backups"), [this] { return service.ListBackups(); },
        [this, then](std::vector<Models::BackupInfo> result) {
            backups = std::move(result);
            backupsLoaded = true;
            ++revision;
            const int n = static_cast<int>(backups.size());
            state.statusMessage = n > 0 ? Loc::Fmt(n == 1 ? "status_backups_found_one" : "status_backups_found_many", n)
                                        : Loc::Get("status_no_backups");
            if (then) then();
        },
        [this, then](const std::wstring& error) {
            Fail("dialog_error", "backup_load_backups_failed", error);
            if (then) then();
        });
}

void BackupState::CreateBackup() {
    const Models::SaveAccount* account = SelectedAccount();
    if (!account || busy) return;
    const std::wstring accountId = account->accountId;
    const std::optional<std::wstring> customName = Text::Trim(backupName).empty() ? std::nullopt : std::optional(backupName);
    Run<B::CreatedBackup>(
        Loc::Get("status_creating_backup"), [this, accountId, customName] { return service.CreateBackup(accountId, customName); },
        [this](B::CreatedBackup created) {
            backupName.clear();
            RefreshBackups([this, created] {
                state.statusMessage = Loc::Get("status_backup_created");
                Dialogs::Message(state.Owner(), Loc::Fmt("backup_created_message", created.backupPath, created.fileCount),
                                 Loc::Get("dialog_success"), MessageButtons::Ok, MessageIcon::Information);
            });
        },
        [this](const std::wstring& error) { Fail("dialog_error", "backup_create_failed", error); });
}

void BackupState::RestoreBackup() {
    const Models::BackupInfo* backup = SelectedBackup();
    if (!backup || busy) return;
    const std::wstring name = backup->name;
    Dialogs::Message(state.Owner(), Loc::Fmt("backup_restore_confirm", backup->DisplayTitle()), Loc::Get("backup_restore_confirm_title"), MessageButtons::YesNo,
                     MessageIcon::Warning, [this, name](MessageResult result) {
                         if (result != MessageResult::Yes) return;
                         Run<int>(
                             Loc::Get("status_restoring_backup"), [this, name] { return service.RestoreBackup(name); },
                             [this](int fileCount) {
                                 state.statusMessage = Loc::Get("status_backup_restored");
                                 Dialogs::Message(state.Owner(), Loc::Fmt("backup_restored_message", fileCount), Loc::Get("dialog_success"),
                                                  MessageButtons::Ok, MessageIcon::Information);
                             },
                             [this](const std::wstring& error) { Fail("dialog_error", "backup_restore_failed", error); });
                     });
}

void BackupState::RestoreBackupToCustom() {
    const Models::BackupInfo* backup = SelectedBackup();
    const Models::SaveAccount* target = SelectedRestoreAccount();
    if (!backup || !target || busy) return;
    const std::wstring name = backup->name, accountId = target->accountId, display = target->ShortDisplay();
    Dialogs::Message(state.Owner(), Loc::Fmt("backup_restore_to_confirm", backup->DisplayTitle(), display), Loc::Get("backup_restore_confirm_title"),
                     MessageButtons::YesNo, MessageIcon::Warning, [this, name, accountId, display](MessageResult result) {
                         if (result != MessageResult::Yes) return;
                         Run<int>(
                             Loc::Get("status_restoring_backup"), [this, name, accountId] { return service.RestoreBackup(name, accountId); },
                             [this, display](int fileCount) {
                                 state.statusMessage = Loc::Get("status_backup_restored");
                                 Dialogs::Message(state.Owner(), Loc::Fmt("backup_restored_to_message", fileCount, display), Loc::Get("dialog_success"),
                                                  MessageButtons::Ok, MessageIcon::Information);
                             },
                             [this](const std::wstring& error) { Fail("dialog_error", "backup_restore_failed", error); });
                     });
}

void BackupState::DeleteBackup() {
    const Models::BackupInfo* backup = SelectedManageBackup();
    if (!backup || busy) return;
    const std::wstring name = backup->name;
    Dialogs::Message(state.Owner(), Loc::Fmt("backup_delete_confirm", backup->DisplayTitle()), Loc::Get("backup_delete_confirm_title"), MessageButtons::YesNo,
                     MessageIcon::Warning, [this, name](MessageResult result) {
                         if (result != MessageResult::Yes) return;
                         Run<bool>(
                             Loc::Get("status_deleting_backup"),
                             [this, name] {
                                 service.DeleteBackup(name);
                                 return true;
                             },
                             [this](bool) { RefreshBackups([this] { state.statusMessage = Loc::Get("status_backup_deleted"); }); },
                             [this](const std::wstring& error) { Fail("dialog_error", "backup_delete_failed", error); });
                     });
}

void BackupState::ExportBackup() {
    const Models::BackupInfo* backup = SelectedManageBackup();
    if (!backup || busy) return;
    const std::wstring name = backup->name;
    const std::wstring path = System::PickSaveFile(state.Owner(), Loc::Get("backup_export_title"),
                                                   { { Loc::Get("filter_zip"), L"*.zip" }, { Loc::Get("filter_all"), L"*.*" } }, name + L".zip", L"zip");
    if (path.empty()) return;
    Run<std::wstring>(
        Loc::Get("status_exporting_backup"), [this, name, path] { return service.ExportBackup(name, path); },
        [this](std::wstring exportPath) {
            state.statusMessage = Loc::Get("status_backup_exported");
            Dialogs::Message(state.Owner(), Loc::Fmt("backup_exported_message", exportPath), Loc::Get("dialog_success"), MessageButtons::Ok,
                             MessageIcon::Information);
        },
        [this](const std::wstring& error) { Fail("dialog_error", "backup_export_failed", error); });
}

void BackupState::ImportBackup() {
    if (busy) return;
    const std::wstring path =
        System::PickFile(state.Owner(), Loc::Get("backup_import_title"), { { Loc::Get("filter_zip"), L"*.zip" }, { Loc::Get("filter_all"), L"*.*" } });
    if (path.empty()) return;
    Run<std::wstring>(
        Loc::Get("status_importing_backup"), [this, path] { return service.ImportBackup(path); },
        [this](std::wstring name) {
            RefreshBackups([this, name] {
                state.statusMessage = Loc::Get("status_backup_imported");
                Dialogs::Message(state.Owner(), Loc::Fmt("backup_imported_message", name), Loc::Get("dialog_success"), MessageButtons::Ok,
                                 MessageIcon::Information);
            });
        },
        [this](const std::wstring& error) { Fail("dialog_error", "backup_import_failed", error); });
}

// The header's Import Save Files: replaces the saves of the account selected in Create Backup
void BackupState::ImportFilesDirectly() {
    if (busy) return;
    const Models::SaveAccount* account = SelectedAccount();
    if (!account) {
        Dialogs::Message(state.Owner(), Loc::Get("import_no_account"), Loc::Get("dialog_error"), MessageButtons::Ok, MessageIcon::Error);
        return;
    }
    const std::wstring path = System::PickFile(state.Owner(), Loc::Get("import_title"),
                                               { { Loc::Get("filter_saves"), L"*.sav;*.zip" },
                                                 { Loc::Get("filter_zip"), L"*.zip" },
                                                 { Loc::Get("filter_sav"), L"*.sav" },
                                                 { Loc::Get("filter_all"), L"*.*" } });
    if (path.empty()) return;
    const std::wstring accountId = account->accountId, display = account->ShortDisplay();
    Dialogs::Message(state.Owner(), Loc::Fmt("import_confirm", display), Loc::Get("import_confirm_title"), MessageButtons::YesNo,
                     MessageIcon::Warning, [this, path, accountId](MessageResult result) {
                         if (result != MessageResult::Yes) return;
                         Run<int>(
                             Loc::Get("status_importing_files"), [this, path, accountId] { return service.ImportFilesDirectly(path, accountId); },
                             [this](int fileCount) {
                                 RefreshAccounts([this, fileCount] {
                                     state.statusMessage = Loc::Get("status_files_imported");
                                     Dialogs::Message(state.Owner(), Loc::Fmt("import_done_message", fileCount), Loc::Get("import_done_title"),
                                                      MessageButtons::Ok, MessageIcon::Information);
                                 });
                             },
                             [this](const std::wstring& error) { Fail("dialog_error", "import_failed", error); });
                     });
}

} // namespace SonicRacingSaveManager::Main
