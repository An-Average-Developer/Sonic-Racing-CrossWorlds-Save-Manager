#include "Windows/Main/MainState.h"
#include <cwctype>
#include "Core/Async.h"
#include "Localization/Localization.h"
#include "Services/App/AppVersion.h"
#include "Windows/Dialogs/MessageDialog.h"

namespace SonicRacingSaveManager::Main {

namespace {

namespace U = Services::Updates;

// "v" + version unless it already starts with v/V
std::wstring WithV(const std::wstring& version) {
    if (!version.empty() && std::towlower(version[0]) == L'v') return version;
    return L"v" + version;
}

// $"{sizeInMB:F2} MB"
std::wstring Megabytes(long long bytes) {
    wchar_t text[32];
    swprintf_s(text, L"%.2f MB", bytes / (1024.0 * 1024.0));
    return text;
}

} // namespace

MainState::MainState(HWND ownerWindow) : owner(ownerWindow) {
    currentVersion = AppVersion::DisplayVersion();
    statusMessage = Loc::Get("status_ready");
    installTitle = Loc::Get("updates_install_title");
    installButtonLabel = Loc::Get("updates_download_install");
}

void MainState::Initialize() { CheckForUpdates(true); }

void MainState::BeginBusy(const std::wstring& status) {
    ++busy;
    isLoading = true;
    statusMessage = status;
}

void MainState::EndBusy() {
    if (busy > 0) --busy;
    isLoading = busy > 0;
}

// CheckForUpdatesAsync. UpdateService swallows its own errors (an unreachable server just means
// "no update"), so the C#'s "Update Check Failed" branch can't be reached and isn't ported (as in
// the other C++ ports).
void MainState::CheckForUpdates(bool silent) {
    if (checking) return;
    checking = true;
    // Reset the GameBanana state on every check
    isGbUpdateAvailable = false;
    currentGbUpdateInfo.reset();
    if (!silent) BeginBusy(Loc::Get("status_checking_updates"));
    Async::Run([] { return U::CheckForUpdates(); }, [this, silent](U::UpdateInfo info) {
        ApplyUpdateInfo(info, silent);
        const auto finish = [this, silent] {
            if (!silent) EndBusy();
            checking = false;
            if (onUpdatesChecked) onUpdatesChecked();
        };
        // No GitHub update: fall back to the GameBanana check
        if (!info.isUpdateAvailable) CheckGameBanana(finish);
        else finish();
    });
}

void MainState::ApplyUpdateInfo(const U::UpdateInfo& info, bool silent) {
    currentUpdateInfo = info;
    hasCheckedForUpdates = true;
    latestVersion = WithV(info.latestVersion);
    updateFileName = info.fileName;
    updateFileSize = info.fileSize > 0 ? Megabytes(info.fileSize) : Loc::Get("updates_unknown_size");
    isUpdateAvailable = info.isUpdateAvailable;
    if (info.isUpdateAvailable) {
        updateStatusTitle = Loc::Get("updates_status_available_title");
        updateStatusMessage = Loc::Fmt("updates_status_available_message", latestVersion);
        updateStatusIcon = UI::Material::Icon::Cloud;
        updateStatusColor = Rgba(76, 175, 80);
        updateStatusHand = true;
        if (!silent) statusMessage = Loc::Fmt("status_update_available", latestVersion);
    } else {
        updateStatusTitle = Loc::Get("updates_up_to_date_title");
        updateStatusMessage = Loc::Fmt("updates_up_to_date_message", currentVersion);
        updateStatusIcon = UI::Material::Icon::CheckCircle;
        updateStatusColor = Rgba(33, 150, 243);
        updateStatusHand = false;
        if (!silent) statusMessage = Loc::Get("status_no_updates");
    }
}

void MainState::CheckGameBanana(std::function<void()> then) {
    Async::Run([] { return U::CheckGameBanana(); }, [this, then](U::GbUpdateInfo gb) {
        currentGbUpdateInfo = gb;
        if (gb.isUpdateAvailable) {
            gbLatestVersion = WithV(gb.latestVersion);
            gbUpdateFileSize = gb.fileSize > 0 ? Megabytes(gb.fileSize) : Loc::Get("updates_unknown_size");
            isGbUpdateAvailable = true;
        }
        if (then) then();
    });
}

void MainState::ShowInstallationView() {
    if (!currentUpdateInfo || !isUpdateAvailable) return;
    isGbInstallation = false;
    installTitle = Loc::Get("updates_install_title_github");
    installButtonLabel = Loc::Get("updates_download_install");
    changelog = currentUpdateInfo->changelog;
    showingInstallationView = true;
}

void MainState::CancelInstallation() {
    showingInstallationView = false;
    isGbInstallation = false;
    installTitle = Loc::Get("updates_install_title");
    installButtonLabel = Loc::Get("updates_download_install");
    statusMessage = Loc::Get("status_update_cancelled");
}

void MainState::ShowGbInstallationView() {
    if (!currentGbUpdateInfo || !isGbUpdateAvailable) return;
    isGbInstallation = true;
    installTitle = Loc::Get("updates_install_title_gb");
    installButtonLabel = Loc::Get("updates_download_gb");
    // The installation view's properties take the GameBanana data for now
    latestVersion = gbLatestVersion;
    updateFileName = currentGbUpdateInfo->fileName;
    updateFileSize = gbUpdateFileSize;
    bool blank = true;
    for (wchar_t c : currentGbUpdateInfo->changelog)
        if (!std::iswspace(c)) blank = false;
    changelog = !blank ? currentGbUpdateInfo->changelog : Loc::Get("updates_gb_no_changelog");
    showingInstallationView = true;
}

void MainState::OpenGitHub() { U::OpenGitHubPage(); }

void MainState::AcceptInstallation() {
    if (isDownloading) return;
    if (isGbInstallation) {
        AcceptGbInstallation();
        return;
    }
    if (!currentUpdateInfo || !isUpdateAvailable) return;
    isDownloading = true;
    downloadProgress = 0;
    downloadProgressText = Loc::Get("updates_starting_download");
    statusMessage = Loc::Get("status_downloading_update");
    const U::UpdateInfo info = *currentUpdateInfo;
    Async::Run([this, info] {
        return U::DownloadAndInstall(info, [this](int percent) {
            Async::Post([this, percent] {
                downloadProgress = percent;
                downloadProgressText = percent < 100 ? Loc::Fmt("updates_downloading", percent) : Loc::Get("updates_installing");
            });
        });
    }, [this](bool success) {
        if (!success) {
            statusMessage = Loc::Get("status_update_failed");
            Dialogs::Message(owner, Loc::Get("updates_failed_message"), Loc::Get("updates_failed_title"), Dialogs::MessageButtons::Ok,
                             Dialogs::MessageIcon::Error);
            showingInstallationView = false;
        }
        isDownloading = false;
        downloadProgress = 0;
        downloadProgressText.clear();
    });
}

void MainState::AcceptGbInstallation() {
    if (!currentGbUpdateInfo) return;
    // No direct download URL: just open the page
    if (currentGbUpdateInfo->downloadUrl.empty()) {
        U::OpenGameBananaPage();
        showingInstallationView = false;
        isGbInstallation = false;
        return;
    }
    isDownloading = true;
    downloadProgress = 0;
    downloadProgressText = Loc::Get("updates_starting_download_gb");
    statusMessage = Loc::Get("status_downloading_gb");
    const U::GbUpdateInfo info = *currentGbUpdateInfo;
    Async::Run([this, info] {
        return U::DownloadAndInstallGameBanana(info, [this](int percent) {
            Async::Post([this, percent] {
                downloadProgress = percent;
                downloadProgressText = percent < 100 ? Loc::Fmt("updates_downloading", percent) : Loc::Get("updates_installing");
            });
        });
    }, [this](bool success) {
        if (!success) {
            statusMessage = Loc::Get("status_gb_failed");
            Dialogs::Message(owner, Loc::Get("updates_gb_failed_message"), Loc::Get("updates_gb_failed_title"), Dialogs::MessageButtons::Ok,
                             Dialogs::MessageIcon::Warning, [](Dialogs::MessageResult) { U::OpenGameBananaPage(); });
            showingInstallationView = false;
            isGbInstallation = false;
        }
        isDownloading = false;
        downloadProgress = 0;
        downloadProgressText.clear();
    });
}

} // namespace SonicRacingSaveManager::Main
