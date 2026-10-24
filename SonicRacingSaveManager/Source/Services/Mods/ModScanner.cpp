#include <windows.h>
#include <shellapi.h>
#include <algorithm>
#include <climits>
#include "Core/Files.h"
#include "Core/Json.h"
#include "Core/Process.h"
#include "Core/Text.h"
#include "Localization/Localization.h"
#include "Services/Mods/ModContents.h"
#include "Services/Mods/ModManager.h"

namespace SonicRacingSaveManager::Services::Mods {

namespace {

bool EndsWithI(const std::wstring& path, const wchar_t* suffix) { return Text::EndsWith(path, suffix, true); }

// Directory.GetFiles(folder, "*.*", AllDirectories) without the .json files
std::vector<std::wstring> FilesWithoutJson(const std::wstring& folder) {
    std::vector<std::wstring> files;
    for (auto& f : Files::ListFiles(folder, L"*.*", true))
        if (!EndsWithI(f, L".json")) files.push_back(std::move(f));
    return files;
}

// File.Move (no overwrite): throws when it fails
void MoveOrThrow(const std::wstring& from, const std::wstring& to) {
    if (!Files::Move(from, to, false)) throw Error(Loc::Fmt("mods_error_move", Files::FileName(from)));
}

// JsonSerializer.Deserialize<ModInfoJson>: "version" / "mod_page" strings (or null); any other type
// makes the whole file fail, which the C# caught and ignored
void ReadInfoJson(const std::wstring& folder, std::wstring& version, std::wstring& modPage, std::wstring& name) {
    const auto text = Files::ReadBytes(Files::Combine(folder, L"info.json"));
    if (!text) return;
    bool ok = false;
    const Json::Value json = Json::Parse(std::string_view(*text), &ok);
    if (!ok || !json.IsObject()) return;
    if (json["name"].IsString()) name = json["name"].WStr();
    const Json::Value& v = json["version"];
    const Json::Value& m = json["mod_page"];
    if ((!v.IsNull() && !v.IsString()) || (!m.IsNull() && !m.IsString())) return;
    version = v.WStr();
    modPage = m.WStr();
}

// The C#'s default string comparison (culture-aware)
bool NameLess(const std::wstring& a, const std::wstring& b) {
    return CompareStringEx(LOCALE_NAME_USER_DEFAULT, 0, a.c_str(), static_cast<int>(a.size()), b.c_str(), static_cast<int>(b.size()), nullptr,
                           nullptr, 0) == CSTR_LESS_THAN;
}

} // namespace

bool ModManager::ModsDirectoryExists() const { return Files::DirectoryExists(modsDirectory); }

std::vector<Models::ModInfo> ModManager::ScanForMods() const {
    std::vector<Models::ModInfo> mods;
    if (!Files::DirectoryExists(modsDirectory)) return mods;
    for (const auto& folder : Files::ListDirectories(modsDirectory)) {
        const std::wstring folderName = Files::FileName(folder);
        const auto [priority, cleanName] = ExtractPriorityFromFolderName(folderName);
        auto allFiles = FilesWithoutJson(folder);
        if (allFiles.empty()) continue;

        int offFiles = 0;
        long long totalSize = 0;
        for (const auto& f : allFiles) {
            if (EndsWithI(f, L".off")) ++offFiles;
            totalSize += static_cast<long long>(Files::Size(f).value_or(0));
        }
        const int activeFiles = static_cast<int>(allFiles.size()) - offFiles;

        Models::ModInfo mod;
        mod.name = cleanName;
        mod.folderPath = folder;
        mod.isEnabled = offFiles == 0 && activeFiles > 0;
        mod.totalSize = totalSize;
        mod.fileCount = static_cast<int>(allFiles.size());
        mod.files = std::move(allFiles);
        mod.priority = priority;
        ReadInfoJson(folder, mod.version, mod.modPageUrl, mod.infoName);
        ModContents contents = ReadModContents(mod.files);
        mod.characters = std::move(contents.characters);
        mod.contentKinds = std::move(contents.kinds);
        mods.push_back(std::move(mod));
    }
    // OrderBy(Priority >= 0 ? Priority : int.MaxValue).ThenBy(Name)
    std::stable_sort(mods.begin(), mods.end(), [](const auto& a, const auto& b) {
        const int pa = a.priority >= 0 ? a.priority : INT_MAX, pb = b.priority >= 0 ? b.priority : INT_MAX;
        if (pa != pb) return pa < pb;
        return NameLess(a.name, b.name);
    });
    return mods;
}

void ModManager::EnableMod(Models::ModInfo& mod) const {
    if (mod.isEnabled) return;
    if (!Files::DirectoryExists(mod.folderPath)) throw Error(Loc::Fmt("mods_error_folder_not_found", mod.folderPath));
    for (const auto& file : Files::ListFiles(mod.folderPath, L"*.off", true)) {
        if (EndsWithI(file, L".json.off")) continue;
        MoveOrThrow(file, file.substr(0, file.size() - 4));
    }
    mod.isEnabled = true;
    mod.files = FilesWithoutJson(mod.folderPath);
}

void ModManager::DisableMod(Models::ModInfo& mod) const {
    if (!mod.isEnabled) return;
    if (!Files::DirectoryExists(mod.folderPath)) throw Error(Loc::Fmt("mods_error_folder_not_found", mod.folderPath));
    for (const auto& file : Files::ListFiles(mod.folderPath, L"*.*", true)) {
        if (EndsWithI(file, L".json") || EndsWithI(file, L".off")) continue;
        MoveOrThrow(file, file + L".off");
    }
    mod.isEnabled = false;
    mod.files = FilesWithoutJson(mod.folderPath);
}

void ModManager::ToggleMod(Models::ModInfo& mod) const {
    if (mod.isEnabled) DisableMod(mod);
    else EnableMod(mod);
}

void ModManager::UninstallMod(const Models::ModInfo& mod) const {
    if (!Files::DirectoryExists(mod.folderPath)) throw Error(Loc::Fmt("mods_error_directory_not_found", mod.folderPath));
    // SHFileOperation wants a double-null-terminated list
    std::wstring from = mod.folderPath;
    from.push_back(L'\0');
    SHFILEOPSTRUCTW op{};
    op.wFunc = FO_DELETE;
    op.pFrom = from.c_str();
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
    if (SHFileOperationW(&op) != 0 || op.fAnyOperationsAborted) throw Error(Loc::Fmt("mods_error_uninstall", mod.name));
}

void ModManager::OpenModsFolder() const {
    if (!Files::DirectoryExists(modsDirectory)) throw Error(Loc::Fmt("mods_error_directory_not_found", modsDirectory));
    Process::Open(modsDirectory);
}

} // namespace SonicRacingSaveManager::Services::Mods
