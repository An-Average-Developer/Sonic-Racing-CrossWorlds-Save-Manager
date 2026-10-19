#pragma once
#include <functional>
#include <string>

// UpdateService.cs — the manager's self-update (GitHub raw version.txt / changelog.txt, release zip
// SRCW-SMv<x-y-z>.zip) and, as in the Inazuma editor, GameBanana (tools/21025) when GitHub has
// nothing newer. Every function blocks: call from worker threads.
namespace SonicRacingSaveManager::Services::Updates {

struct UpdateInfo {
    std::wstring latestVersion;
    std::wstring changelog;
    std::wstring downloadUrl;
    std::wstring fileName;
    long long fileSize = 0;
    bool isUpdateAvailable = false;
    std::wstring releaseUrl;
};

struct GbUpdateInfo {
    std::wstring latestVersion;
    std::wstring downloadUrl;
    std::wstring fileName;
    long long fileSize = 0;
    bool isUpdateAvailable = false;
    std::wstring changelog;
};

// Failures are swallowed (as in the C#): the result then says "no update".
UpdateInfo CheckForUpdates();
// Manual fallback: download info of the latest release exe regardless of the local version.
UpdateInfo LatestReleaseInfo();
GbUpdateInfo CheckGameBanana();
// The live changelog.txt from GitHub, or the embedded one when the fetch fails / is empty.
std::wstring FetchChangelog();

using Progress = std::function<void(int percent)>;
// Download, extract and swap via a helper batch file, relaunching the manager; exits the process.
// Returns false on failure.
bool DownloadAndInstall(const UpdateInfo& info, const Progress& progress);
bool DownloadAndInstallGameBanana(const GbUpdateInfo& info, const Progress& progress);
// Manual update view: downloads only SonicRacingSaveManager.exe next to the running one.
bool DownloadAndInstallManagerExe(const Progress& progress);

void OpenGameBananaPage();
void OpenGitHubPage();

// "—" / "512 B" / "1.5 KB" / "12.34 MB"
std::wstring FormatFileSize(long long bytes);

} // namespace SonicRacingSaveManager::Services::Updates
