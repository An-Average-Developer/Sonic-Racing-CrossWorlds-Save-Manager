#pragma once
#include <exception>
#include <functional>
#include <string>
#include <vector>
#include "Models/ModInfo.h"

// Features\ModManager\Services\ModManagerService.cs — one mods folder: every subfolder is a mod, its
// files renamed to "*.off" while disabled, an optional info.json ("version", "mod_page") and an
// optional "01 - " priority prefix on the folder name. Blocking: call from worker threads. Split by
// area:
//   ModScanner.cpp     scan, enable / disable / toggle, opening the folder
//   GameBananaApi.cpp  update checks and the files of a mod (apiv11), version comparison
//   ModUpdater.cpp     DownloadAndUpdateModAsync
//   ModPriority.cpp    priority mode (folder name prefixes)
namespace SonicRacingSaveManager::Services::Mods {

// The C# threw (DirectoryNotFoundException, IOException, HttpRequestException...) and the view
// models showed ex.Message: the same messages travel in this exception (translated)
class Error : public std::exception {
public:
    explicit Error(std::wstring message) : text(std::move(message)) {}
    const std::wstring& Message() const { return text; }
    const char* what() const noexcept override { return "SonicRacingSaveManager::Mods::Error"; }

private:
    std::wstring text;
};

class ModManager {
public:
    explicit ModManager(std::wstring modsDirectory) : modsDirectory(std::move(modsDirectory)) {}
    const std::wstring& ModsDirectory() const { return modsDirectory; }
    bool ModsDirectoryExists() const;

    // ── ModScanner.cpp ──
    // Priority order first, then by name
    std::vector<Models::ModInfo> ScanForMods() const;
    void EnableMod(Models::ModInfo& mod) const;
    void DisableMod(Models::ModInfo& mod) const;
    void ToggleMod(Models::ModInfo& mod) const;
    // Sends the mod's folder to the Recycle Bin
    void UninstallMod(const Models::ModInfo& mod) const;
    void OpenModsFolder() const;

    // ── ModUpdater.cpp ──
    using Progress = std::function<void(double percentage, const std::wstring& status)>;
    void DownloadAndUpdateMod(Models::ModInfo& mod, const std::vector<Models::GameBananaFile>& selectedFiles, const std::wstring& latestVersion,
                              const Progress& progress) const;

    // ── ModPriority.cpp ──
    void EnablePriorityMode(std::vector<Models::ModInfo>& mods) const;
    void DisablePriorityMode(std::vector<Models::ModInfo>& mods) const;
    void UpdateModPriority(Models::ModInfo& mod, int newPriority) const;
    void ReorderMods(std::vector<Models::ModInfo>& mods) const;
    static bool IsPriorityModeEnabled(const std::vector<Models::ModInfo>& mods);
    // "^(\d+)\s*-\s*(.+)$" → (number - 1, trimmed name), else (-1, folderName)
    static std::pair<int, std::wstring> ExtractPriorityFromFolderName(const std::wstring& folderName);

private:
    std::wstring modsDirectory;
};

} // namespace SonicRacingSaveManager::Services::Mods
