#include "Services/App/UpdateService.h"
#include <windows.h>
#include <shellapi.h>
#include "App/Config.h"
#include "Core/Files.h"
#include "Core/Http.h"
#include "Core/Json.h"
#include "Core/Process.h"
#include "Core/Text.h"
#include "Core/Zip.h"
#include "Services/App/AppVersion.h"
#include "Services/App/Log.h"
#include "Services/App/ChangelogData.h"
#include "Services/App/VersionCompare.h"

namespace SonicRacingSaveManager::Services::Updates {

namespace {

const std::vector<std::pair<std::wstring, std::wstring>> kHeaders = { { L"User-Agent", Config::UserAgent } };

std::wstring RawUrl(const wchar_t* branch, const wchar_t* file) {
    return std::wstring(L"https://raw.githubusercontent.com/") + AppVersion::GitHubOwner + L"/" + AppVersion::GitHubRepo + L"/" + branch +
           L"/" + file;
}

std::wstring ReleasesUrl() { return std::wstring(L"https://github.com/") + AppVersion::GitHubOwner + L"/" + AppVersion::GitHubRepo + L"/releases/latest"; }

// version.txt with \r \n \t removed and trimmed; nullopt on a network error or empty file
std::optional<std::wstring> FetchVersion(const wchar_t* branch) {
    const Http::Response r = Http::Get(RawUrl(branch, L"version.txt"), kHeaders);
    if (!r.Ok()) return std::nullopt;
    std::wstring v;
    for (wchar_t c : Text::Widen(r.body))
        if (c != L'\r' && c != L'\n' && c != L'\t') v += c;
    v = Text::Trim(v);
    if (v.empty()) return std::nullopt;
    return v;
}

long long HeadContentLength(const std::wstring& url) {
    Http::Request head;
    head.method = L"HEAD";
    head.url = url;
    head.headers = kHeaders;
    const Http::Response r = Http::Send(head);
    return r.Ok() ? Text::ToInt(r.Header(L"content-length")).value_or(0) : 0;
}

bool DownloadTo(const std::wstring& url, const std::wstring& path, const Progress& progress) {
    return Http::Download(url, path, [&](uint64_t received, uint64_t total) {
        if (progress && total > 0) progress(static_cast<int>(received * 100 / total));
        return true;
    }, nullptr, kHeaders);
}

// The manager exe by name, then the old C# name, then the current exe's name, then any exe (recursive)
std::wstring FindExecutable(const std::wstring& dir) {
    const std::wstring current = Files::FileName(Files::ExecutablePath());
    const std::wstring names[] = { Config::GitHub::ExeName, L"Sonic Racing Save Manager.exe", current };
    for (const auto& name : names)
        if (Files::Exists(Files::Combine(dir, name))) return Files::Combine(dir, name);
    for (const auto& name : names) {
        auto found = Files::ListFiles(dir, name, true);
        if (!found.empty()) return found.front();
    }
    auto any = Files::ListFiles(dir, L"*.exe", true);
    return any.empty() ? std::wstring() : any.front();
}

// Batch lines that wait (up to ~30 s) until this process is gone, then about one second more (the
// launcher port's fix for the C#'s fixed "timeout /t 2", which returned at once when cmd runs
// hidden on some PCs; ping is used to wait because timeout fails without a console).
std::wstring WaitForExitLines() {
    const std::wstring pid = std::to_wstring(GetCurrentProcessId());
    return L"set WAITS=30\r\n:wait\r\n"
           // Full paths: a Git / MSYS folder on PATH brings its own (Unix) find
           L"\"%SystemRoot%\\System32\\tasklist.exe\" /FI \"PID eq " + pid + L"\" /NH 2>nul | \"%SystemRoot%\\System32\\find.exe\" /I \".exe\" >nul\r\n"
           L"if not errorlevel 1 (\r\n    set /a WAITS-=1\r\n    if %WAITS% GTR 0 (\r\n"
           L"        ping -n 2 127.0.0.1 >nul\r\n        goto wait\r\n    )\r\n)\r\n"
           L"ping -n 2 127.0.0.1 >nul\r\n";
}

[[noreturn]] void RunBatchAndExit(const std::wstring& batchPath, const std::string& content) {
    Files::WriteBytes(batchPath, content);
    SHELLEXECUTEINFOW info{ sizeof(info) };
    info.lpVerb = L"open";
    info.lpFile = L"cmd.exe";
    const std::wstring args = L"/c \"" + batchPath + L"\"";
    info.lpParameters = args.c_str();
    info.nShow = SW_HIDE;
    ShellExecuteExW(&info);
    ExitProcess(0);   // the batch waits for this process to be gone
}

std::wstring UpdateTempDir() { return Files::Combine(Files::TempDirectory(), std::wstring(Config::AppFolder) + L"_Update"); }

} // namespace

UpdateInfo CheckForUpdates() {
    UpdateInfo info;
    info.latestVersion = AppVersion::CurrentVersion;
    const auto latest = FetchVersion(AppVersion::GitHubBranch);
    if (!latest) return info;
    // A real version number (X.Y...), not a 404 page or error body
    std::wstring digits;
    for (wchar_t c : *latest)
        if ((c >= L'0' && c <= L'9') || c == L'.') digits += c;
    const auto parts = Text::Split(digits, L'.', true);
    if (parts.size() < 2) return info;
    for (const auto& p : parts)
        if (!Text::ToInt(p)) return info;

    info.latestVersion = *latest;
    info.changelog = FetchChangelog();
    info.isUpdateAvailable = Versions::IsNewer(AppVersion::CurrentVersion, *latest);
    if (info.isUpdateAvailable) {
        info.fileName = Config::GitHub::ReleaseZipPrefix + Text::Replace(*latest, L".", L"-") + L".zip";
        info.downloadUrl = ReleasesUrl() + L"/download/" + info.fileName;
        info.releaseUrl = ReleasesUrl();
        info.fileSize = HeadContentLength(info.downloadUrl);
    }
    return info;
}

UpdateInfo LatestReleaseInfo() {
    UpdateInfo info;
    info.latestVersion = AppVersion::CurrentVersion;
    const auto latest = FetchVersion(AppVersion::GitHubBranch);
    if (!latest) return info;
    info.latestVersion = *latest;
    info.isUpdateAvailable = true;
    info.changelog = FetchChangelog();
    info.downloadUrl = Config::GitHub::LatestExeUrl;
    info.fileName = Config::GitHub::ExeName;
    info.releaseUrl = ReleasesUrl();
    info.fileSize = HeadContentLength(Config::GitHub::LatestExeUrl);
    return info;
}

std::wstring FetchChangelog() {
    const Http::Response r = Http::Get(RawUrl(AppVersion::GitHubBranch, L"changelog.txt"), kHeaders);
    if (r.Ok()) {
        const std::wstring changelog = Text::Trim(Text::Widen(r.body));
        if (!changelog.empty()) return changelog;
    }
    return Changelog::EmbeddedText();
}

GbUpdateInfo CheckGameBanana() {
    GbUpdateInfo info;
    // "Version" is not a valid GB API field — the version is _sVersion inside Files().aFiles()
    const std::wstring url = std::wstring(Config::GameBanana::ApiItemUrl) + std::to_wstring(Config::GameBanana::ToolId) +
                             L"&fields=name,Files().aFiles()";
    const Http::Response r = Http::Get(url, kHeaders);
    if (!r.Ok()) return info;
    // Response is [name, filesObject]
    const Json::Value root = Json::Parse(std::string_view(r.body));
    if (!root.IsArray() || root.Size() < 2 || !root[1].IsObject()) return info;
    const auto& files = root[1].Items();
    if (files.empty()) return info;
    const Json::Value& file = files.front().second;   // the first (newest) file only
    info.latestVersion = file["_sVersion"].IsString() ? Text::Trim(file["_sVersion"].WStr()) : L"";
    info.isUpdateAvailable = Versions::IsNewer(AppVersion::CurrentVersion, info.latestVersion);
    if (info.isUpdateAvailable) {
        info.downloadUrl = file["_sDownloadUrl"].IsString() ? file["_sDownloadUrl"].WStr() : L"";
        info.fileName = file["_sFile"].IsString() ? file["_sFile"].WStr() : L"";
        info.fileSize = file["_nFilesize"].IsNumber() ? file["_nFilesize"].Int() : 0;
        info.changelog = FetchChangelog();
    }
    return info;
}

bool DownloadAndInstall(const UpdateInfo& info, const Progress& progress) {
    if (Text::Trim(info.downloadUrl).empty()) return false;
    const std::wstring tempDir = UpdateTempDir();
    Files::DeleteDirectory(tempDir);
    Files::CreateDirectories(tempDir);
    const std::wstring zipPath = Files::Combine(tempDir, info.fileName);
    Log::Info(L"Update: downloading " + info.fileName + L" (" + info.latestVersion + L")");
    const auto fail = [&](const wchar_t* why) {
        Log::Error(std::wstring(L"Update failed: ") + why);
        Files::DeleteDirectory(tempDir);
        return false;
    };
    if (!DownloadTo(info.downloadUrl, zipPath, progress)) return fail(L"the download did not finish");
    if (progress) progress(100);

    const std::wstring extractDir = Files::Combine(tempDir, L"extracted");
    Files::CreateDirectories(extractDir);
    if (!Zip::ExtractToDirectory(zipPath, extractDir, false)) return fail(L"the zip could not be extracted");
    const std::wstring newExe = FindExecutable(extractDir);
    if (newExe.empty() || !Files::Exists(newExe)) {
        Log::Error(L"Update failed: no manager exe in the downloaded zip");
        return false;
    }

    const std::wstring currentExe = Files::ExecutablePath();
    const std::wstring currentDir = Files::DirectoryName(currentExe);
    const std::wstring extractedDir = Files::DirectoryName(newExe);
    if (currentExe.empty() || currentDir.empty() || extractedDir.empty()) return false;
    // The copy's output goes to update.log; on failure the temp folder (zip, extracted files, log)
    // stays for diagnosis ("Sharing violation" = the exe was still locked)
    const std::wstring log = Files::Combine(tempDir, L"update.log");
    const std::string batch = Text::Narrow(
        L"@echo off\r\nchcp 65001 >nul\r\n" + WaitForExitLines() +
        // Retried while the exe is still locked (antivirus scan...)
        L"set RETRIES=15\r\n:copy\r\n"
        L"xcopy \"" + extractedDir + L"\\*\" \"" + currentDir + L"\" /E /Y /I /Q >> \"" + log + L"\" 2>&1\r\n"
        L"if not errorlevel 1 goto copied\r\n"
        L"set /a RETRIES-=1\r\nif %RETRIES% LEQ 0 exit /b 1\r\n"
        L"ping -n 2 127.0.0.1 >nul\r\ngoto copy\r\n:copied\r\n"
        L"start \"\" \"" + currentExe + L"\"\r\n"
        L"rd /s /q \"" + tempDir + L"\"\r\nexit\r\n");
    Log::Info(L"Update: downloaded, closing to let update.bat replace the files");
    RunBatchAndExit(Files::Combine(tempDir, L"update.bat"), batch);
}

bool DownloadAndInstallGameBanana(const GbUpdateInfo& gb, const Progress& progress) {
    UpdateInfo mapped;
    mapped.downloadUrl = gb.downloadUrl;
    mapped.fileName = Text::Trim(gb.fileName).empty() ? L"gamebanana_update.zip" : gb.fileName;
    mapped.isUpdateAvailable = true;
    mapped.latestVersion = gb.latestVersion;
    return DownloadAndInstall(mapped, progress);
}

bool DownloadAndInstallManagerExe(const Progress& progress) {
    const std::wstring currentDir = Files::DirectoryName(Files::ExecutablePath());
    const std::wstring currentExe = Files::Combine(currentDir, Config::GitHub::ExeName);
    const std::wstring staging = Files::Combine(currentDir, L"SonicRacingSaveManager.update.exe");
    if (!DownloadTo(Config::GitHub::LatestExeUrl, staging, progress)) {
        Files::Delete(staging);
        return false;
    }
    const std::wstring tempDir = UpdateTempDir();
    Files::DeleteDirectory(tempDir);
    Files::CreateDirectories(tempDir);
    const std::string batch = Text::Narrow(
        L"@echo off\r\nchcp 65001 >nul\r\n" + WaitForExitLines() + L"set RETRIES=10\r\n:retry\r\n"
        L"move /y \"" + staging + L"\" \"" + currentExe + L"\" >> \"" + Files::Combine(tempDir, L"update.log") + L"\" 2>&1\r\n"
        L"if exist \"" + staging + L"\" (\r\n    set /a RETRIES-=1\r\n    if %RETRIES% GTR 0 (\r\n"
        L"        ping -n 2 127.0.0.1 >nul\r\n        goto retry\r\n    )\r\n)\r\n"
        L"start \"\" \"" + currentExe + L"\"\r\n"
        L"rd /s /q \"" + tempDir + L"\"\r\nexit");
    RunBatchAndExit(Files::Combine(tempDir, L"update.bat"), batch);
}

void OpenGameBananaPage() { Process::OpenUrl(Config::GameBanana::ToolPage); }

void OpenGitHubPage() { Process::OpenUrl(AppVersion::GitHubRepoUrl()); }

std::wstring FormatFileSize(long long bytes) { return Versions::FormatFileSize(bytes); }

} // namespace SonicRacingSaveManager::Services::Updates
