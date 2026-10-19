#include <windows.h>
#include <algorithm>
#include <cstdio>
#include <functional>
#include "Core/Files.h"
#include "Core/Http.h"
#include "Core/Json.h"
#include "Core/System.h"
#include "Core/Text.h"
#include "Core/Zip.h"
#include "Localization/Localization.h"
#include "Services/App/Log.h"
#include "Services/Mods/GameBananaApi.h"
#include "Services/Mods/ModManager.h"

namespace SonicRacingSaveManager::Services::Mods {

namespace {

// "{x:F1}"
std::wstring Mb(double bytes) {
    wchar_t text[32];
    swprintf_s(text, L"%.1f", bytes / 1024.0 / 1024.0);
    return text;
}

// CopyDirectoryContents: every file and folder, info.json left out at every level when asked;
// throws when a copy fails
void CopyDirectoryContents(const std::wstring& sourcePath, const std::wstring& destinationPath, bool excludeInfoJson) {
    Files::CreateDirectories(destinationPath);
    for (const auto& file : Files::ListFiles(sourcePath)) {
        const std::wstring fileName = Files::FileName(file);
        if (excludeInfoJson && Text::Equals(fileName, L"info.json", true)) continue;
        if (!Files::Copy(file, Files::Combine(destinationPath, fileName), true)) throw Error(Loc::Fmt("mods_error_copy", fileName));
    }
    for (const auto& directory : Files::ListDirectories(sourcePath))
        CopyDirectoryContents(directory, Files::Combine(destinationPath, Files::FileName(directory)), excludeInfoJson);
}

// info.json gets the new "version" (JsonSerializer.Serialize of ModInfoJson, indented: only
// "version" and "mod_page" are kept, as in the C#)
void UpdateInfoJson(const std::wstring& folder, const std::wstring& latestVersion) {
    const std::wstring path = Files::Combine(folder, L"info.json");
    if (!Files::Exists(path)) return;
    const auto text = Files::ReadBytes(path);
    bool ok = false;
    const Json::Value json = text ? Json::Parse(std::string_view(*text), &ok) : Json::Value();
    if (!ok || !json.IsObject()) throw Error(Loc::Fmt("mods_error_read", L"info.json"));
    Json::Value out = Json::Value::MakeObject();
    out.Set("version", latestVersion);
    out.Set("mod_page", json["mod_page"].IsString() ? Json::Value(json["mod_page"].WStr()) : Json::Value(nullptr));
    if (!Files::WriteBytes(path, out.Serialize(true))) throw Error(Loc::Fmt("mods_error_write", L"info.json"));
}

} // namespace

void ModManager::DownloadAndUpdateMod(Models::ModInfo& mod, const std::vector<Models::GameBananaFile>& selectedFiles, const std::wstring& latestVersion,
                                      const Progress& progress) const {
    if (selectedFiles.empty()) throw Error(Loc::Get("mods_error_no_files_selected"));
    const auto report = [&](double percentage, const std::wstring& status) {
        if (progress) progress(percentage, status);
    };

    const bool wasEnabled = mod.isEnabled;
    if (wasEnabled) {
        report(5, Loc::Get("mod_update_disabling"));
        DisableMod(mod);
    }

    std::vector<std::wstring> downloadedFiles;
    const std::wstring tempExtractPath = Files::Combine(Files::TempDirectory(), L"mod_extract_" + System::NewGuid());
    Files::CreateDirectories(tempExtractPath);
    // finally: the downloads and the extraction folder go away whatever happens
    struct Cleanup {
        std::vector<std::wstring>& files;
        const std::wstring& dir;
        ~Cleanup() {
            for (const auto& f : files)
                if (Files::Exists(f) && !Files::Delete(f)) Log::Warning(L"Mod update: could not delete " + f);
            if (Files::DirectoryExists(dir) && !Files::DeleteDirectory(dir)) Log::Warning(L"Mod update: could not delete " + dir);
        }
    } cleanup{ downloadedFiles, tempExtractPath };

    const int count = static_cast<int>(selectedFiles.size());
    int fileIndex = 0;
    for (const auto& fileToDownload : selectedFiles) {
        ++fileIndex;
        if (!fileToDownload.downloadUrl || fileToDownload.downloadUrl->empty()) continue;
        const double progressOffset = 5.0 + static_cast<double>(fileIndex - 1) / count * 60.0;
        const double progressRange = 60.0 / count;
        const std::wstring name = fileToDownload.fileName.value_or(L"");
        report(progressOffset, Loc::Fmt("mod_update_downloading_file", fileIndex, count, name));

        const std::wstring tempPath =
            Files::Combine(Files::TempDirectory(), fileToDownload.fileName ? *fileToDownload.fileName : L"mod_download_" + std::to_wstring(fileIndex) + L".zip");
        std::wstring error;
        const bool ok = Http::Download(
            *fileToDownload.downloadUrl, tempPath,
            [&](uint64_t received, uint64_t total) {
                if (total > 0) {
                    const double downloaded = static_cast<double>(received) / static_cast<double>(total) * progressRange;
                    report(progressOffset + downloaded,
                           Loc::Fmt("mod_update_downloading_mb", name, Mb(static_cast<double>(received)), Mb(static_cast<double>(total))));
                }
                return true;
            },
            &error, GameBanana::Headers());
        if (!ok) throw Error(error.empty() ? Loc::Get("mods_error_network") : error);
        downloadedFiles.push_back(tempPath);
    }

    report(70, Loc::Get("mod_update_extracting"));
    for (const auto& downloadedFile : downloadedFiles) {
        if (Text::EndsWith(downloadedFile, L".zip", true)) {
            std::wstring error;
            if (!Zip::ExtractToDirectory(downloadedFile, tempExtractPath, true, &error)) throw Error(error.empty() ? Loc::Fmt("mods_error_extract", Files::FileName(downloadedFile)) : error);
        } else {
            if (!Files::Copy(downloadedFile, Files::Combine(tempExtractPath, Files::FileName(downloadedFile)), true))
                throw Error(Loc::Fmt("mods_error_copy", Files::FileName(downloadedFile)));
        }
    }

    // A zip holding a single folder: that folder is the mod
    std::wstring sourcePath = tempExtractPath;
    const auto topFiles = Files::ListFiles(tempExtractPath);
    const auto topDirs = Files::ListDirectories(tempExtractPath);
    if (topFiles.empty() && topDirs.size() == 1) sourcePath = topDirs.front();

    report(85, Loc::Get("mod_update_removing"));
    // Every file but the ones ending in "info.json", then the folders left empty (deepest first)
    for (const auto& file : Files::ListFiles(mod.folderPath, L"*.*", true)) {
        if (Text::EndsWith(file, L"info.json", true)) continue;
        if (!Files::Delete(file)) Log::Warning(L"Mod update: could not delete " + file);
    }
    std::vector<std::wstring> dirs;
    const std::function<void(const std::wstring&)> collect = [&](const std::wstring& dir) {
        for (const auto& sub : Files::ListDirectories(dir)) {
            dirs.push_back(sub);
            collect(sub);
        }
    };
    collect(mod.folderPath);
    std::stable_sort(dirs.begin(), dirs.end(), [](const auto& a, const auto& b) { return a.size() > b.size(); });
    for (const auto& dir : dirs)
        if (Files::DirectoryExists(dir) && Files::ListFiles(dir).empty() && Files::ListDirectories(dir).empty()) RemoveDirectoryW(dir.c_str());

    report(90, Loc::Get("mod_update_installing"));
    CopyDirectoryContents(sourcePath, mod.folderPath, true);

    report(95, Loc::Get("mod_update_info"));
    UpdateInfoJson(mod.folderPath, latestVersion);

    if (wasEnabled) {
        report(97, Loc::Get("mod_update_reenabling"));
        EnableMod(mod);
    } else {
        // (the mod is already marked disabled, so this does nothing: the new files stay active, as in the C#)
        report(97, Loc::Get("mod_update_applying_disabled"));
        DisableMod(mod);
    }
    report(100, Loc::Get("mod_update_completed"));
}

} // namespace SonicRacingSaveManager::Services::Mods
