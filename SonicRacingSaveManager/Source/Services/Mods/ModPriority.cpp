#include <windows.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <regex>
#include "Core/Files.h"
#include "Core/System.h"
#include "Core/Text.h"
#include "Localization/Localization.h"
#include "Services/Mods/ModManager.h"

namespace SonicRacingSaveManager::Services::Mods {

namespace {

// Directory.Move: throws when it fails (the target already exists...)
void MoveDirectory(const std::wstring& from, const std::wstring& to) {
    if (!MoveFileExW(from.c_str(), to.c_str(), 0)) throw Error(Loc::Fmt("mods_error_rename", Files::FileName(from), Files::FileName(to)));
}

} // namespace

std::pair<int, std::wstring> ModManager::ExtractPriorityFromFolderName(const std::wstring& folderName) {
    static const std::wregex pattern(LR"(^(\d+)\s*-\s*(.+)$)");
    std::wsmatch match;
    if (std::regex_match(folderName, match, pattern)) {
        const auto number = Text::ToInt(match[1].str());
        if (number && *number <= INT32_MAX) return { static_cast<int>(*number) - 1, Text::Trim(match[2].str()) };   // int.TryParse
    }
    return { -1, folderName };
}

void ModManager::EnablePriorityMode(std::vector<Models::ModInfo>& mods) const {
    for (size_t i = 0; i < mods.size(); ++i)
        if (mods[i].priority < 0) UpdateModPriority(mods[i], static_cast<int>(i));
}

void ModManager::DisablePriorityMode(std::vector<Models::ModInfo>& mods) const {
    for (auto& mod : mods) {
        if (mod.priority < 0) continue;
        const auto [unused, cleanName] = ExtractPriorityFromFolderName(Files::FileName(mod.folderPath));
        (void)unused;
        const std::wstring newFolderPath = Files::Combine(modsDirectory, cleanName);
        if (mod.folderPath != newFolderPath && Files::DirectoryExists(mod.folderPath)) {
            MoveDirectory(mod.folderPath, newFolderPath);
            mod.folderPath = newFolderPath;
            mod.priority = -1;
        }
    }
}

void ModManager::UpdateModPriority(Models::ModInfo& mod, int newPriority) const {
    const auto [unused, cleanName] = ExtractPriorityFromFolderName(Files::FileName(mod.folderPath));
    (void)unused;
    wchar_t prefix[16];
    swprintf_s(prefix, L"%02d - ", newPriority + 1);
    const std::wstring newFolderPath = Files::Combine(modsDirectory, prefix + cleanName);
    if (mod.folderPath != newFolderPath && Files::DirectoryExists(mod.folderPath)) {
        if (Files::DirectoryExists(newFolderPath)) {
            // (as in the C#: the mod itself goes to a temporary name first, and the move below still
            // finds the other folder there)
            const std::wstring tempFolderPath = Files::Combine(modsDirectory, L"_temp_" + System::NewGuid());
            MoveDirectory(mod.folderPath, tempFolderPath);
            mod.folderPath = tempFolderPath;
        }
        MoveDirectory(mod.folderPath, newFolderPath);
        mod.folderPath = newFolderPath;
    }
    mod.priority = newPriority;
}

void ModManager::ReorderMods(std::vector<Models::ModInfo>& mods) const {
    for (size_t i = 0; i < mods.size(); ++i)
        if (mods[i].priority >= 0) UpdateModPriority(mods[i], static_cast<int>(i));
}

bool ModManager::IsPriorityModeEnabled(const std::vector<Models::ModInfo>& mods) {
    return std::any_of(mods.begin(), mods.end(), [](const auto& m) { return m.priority >= 0; });
}

} // namespace SonicRacingSaveManager::Services::Mods
