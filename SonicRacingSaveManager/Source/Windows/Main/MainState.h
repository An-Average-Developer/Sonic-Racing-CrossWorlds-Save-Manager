#pragma once
#include <windows.h>
#include <functional>
#include <optional>
#include <string>
#include "Graphics/Color.h"
#include "Services/App/UpdateService.h"
#include "UI/Material/PackIcon.h"

namespace SonicRacingSaveManager::Main {

// ViewModels\MainViewModel.cs: the status bar (StatusMessage / IsLoading), the update checks (GitHub,
// then GameBanana, as in the Inazuma editor) and the Updates tab's installation view. The accounts and backups shared by the Create Backup,
// Restore and Manage tabs live in Tabs\Backup\BackupState.
class MainState {
public:
    explicit MainState(HWND owner);
    void Initialize();   // InitializeAsync's silent update check (the backups load themselves)

    // ── Status bar ──
    std::wstring statusMessage;
    bool isLoading = false;
    // Several operations can run at once (a backup refresh during an update check...): the bar
    // shows its progress while any of them is busy
    void BeginBusy(const std::wstring& status);
    void EndBusy();

    // ── Updates ──
    std::wstring currentVersion;
    std::wstring latestVersion;
    std::wstring updateFileName, updateFileSize;
    bool isUpdateAvailable = false;
    bool hasCheckedForUpdates = false;
    bool isDownloading = false;
    int downloadProgress = 0;
    std::wstring downloadProgressText;
    std::wstring updateStatusTitle, updateStatusMessage;
    UI::Material::Icon updateStatusIcon = UI::Material::Icon::CheckCircle;
    Color updateStatusColor = Rgba(128, 128, 128);   // Colors.Gray
    bool updateStatusHand = false;                    // UpdateStatusCursor == "Hand"
    bool showingInstallationView = false;
    std::wstring changelog;
    std::wstring installTitle;
    std::wstring installButtonLabel;
    // GameBanana
    bool isGbUpdateAvailable = false;
    std::wstring gbLatestVersion;
    bool isGbInstallation = false;

    // A GitHub or GameBanana update is available: the "breathing" ring on the Updates tab
    bool HasAnyUpdateAvailable() const { return isUpdateAvailable || isGbUpdateAvailable; }

    void CheckForUpdates(bool silent = false);
    bool Checking() const { return checking; }
    void ShowInstallationView();
    bool CanShowInstallationView() const { return isUpdateAvailable; }
    void CancelInstallation();
    void ShowGbInstallationView();
    void AcceptInstallation();
    void OpenGitHub();
    // Runs after a check finishes (the startup notification waits for it)
    std::function<void()> onUpdatesChecked;

    HWND Owner() const { return owner; }

private:
    void ApplyUpdateInfo(const Services::Updates::UpdateInfo& info, bool silent);
    void CheckGameBanana(std::function<void()> then);
    void AcceptGbInstallation();

    HWND owner;
    std::optional<Services::Updates::UpdateInfo> currentUpdateInfo;
    std::optional<Services::Updates::GbUpdateInfo> currentGbUpdateInfo;
    std::wstring gbUpdateFileSize;
    bool checking = false;
    int busy = 0;
};

} // namespace SonicRacingSaveManager::Main
