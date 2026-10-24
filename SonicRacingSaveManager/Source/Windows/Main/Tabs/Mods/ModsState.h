#pragma once
#include <windows.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "Core/Async.h"
#include "Models/ModInfo.h"
#include "Services/Mods/ModManager.h"

namespace SonicRacingSaveManager::Main {

// Features\ModManager\ViewModels\ModManagerViewModel.cs: the mods of the chosen folder, their
// commands (CanExecute included), the game-running lock checked every second and the GameBanana
// update check every 60 s. Created when the Mod Manager tab is first shown, as the C# created it
// when the tab's bindings first asked for it. Split:
//   ModsState.cpp     init, refresh, toggles, folders, update checks, the game-running lock
//   ModsActions.cpp   priority mode, move up / down and drag-and-drop
class ModsState {
public:
    explicit ModsState(HWND owner);
    ~ModsState();
    ModsState(const ModsState&) = delete;
    ModsState& operator=(const ModsState&) = delete;
    void EnsureInitialized();
    bool Initialized() const { return isInitialized; }

    const std::vector<Models::ModInfo>& Mods() const { return mods; }
    std::wstring selectedMod;   // SelectedMod, by name
    const Models::ModInfo* SelectedMod() const;
    int SelectedIndex() const;
    std::wstring statusMessage;
    bool isLoading = false;
    bool isPriorityMode = false;
    bool isGameRunning = false;
    unsigned revision = 0;   // bumped whenever the list is reloaded (the rows play their entrance again)
    // The folders are being renamed (a move, priority mode on / off): the list shows "Reordering"
    bool isReordering = false;
    unsigned reorders = 0;   // bumped when a reordering starts

    int EnabledModCount() const;
    int DisabledModCount() const;
    int TotalModCount() const { return static_cast<int>(mods.size()); }
    bool ModsDirectoryExists() const { return service && service->ModsDirectoryExists(); }
    std::wstring ModsDirectory() const { return service ? service->ModsDirectory() : L""; }

    // ── Commands ──
    // `replay`: the rows play their entrance again (not after a reordering: they just slide)
    void RefreshMods(std::function<void()> then = {}, bool replay = true);
    bool CanToggleMod() const { return SelectedMod() != nullptr && !isGameRunning; }
    void ToggleSelectedMod();
    bool CanUninstallMod() const { return SelectedMod() != nullptr && !isGameRunning && !isLoading; }
    void UninstallSelectedMod();
    bool CanEnableAll() const;
    void EnableAllMods();
    bool CanDisableAll() const;
    void DisableAllMods();
    void OpenModsFolder();
    void SelectModsFolder();
    void CheckForUpdates();
    bool CanShowUpdateDialog() const { return !isGameRunning; }
    void ShowUpdateDialog(const std::wstring& modName);
    // ModsActions.cpp
    bool CanTogglePriorityMode() const { return !isGameRunning && !isReordering; }
    void TogglePriorityMode();
    bool CanMove() const { return SelectedMod() != nullptr && isPriorityMode && !isGameRunning && !isReordering; }
    void MoveModUp();
    void MoveModDown();
    bool CanDrag() const { return isPriorityMode && !isGameRunning && !isReordering; }
    void HandleDrop(int droppedIndex, int targetIndex);

    // The update card (ModUpdateDialog) is the tab's: it gets the mod and the service, and calls
    // UpdateFinished when the update went through
    std::function<void(const Models::ModInfo& mod, Services::Mods::ModManager& service)> onShowUpdate;
    void UpdateFinished(const Models::ModInfo& mod);

private:
    void UpdateCheckTimerTick();
    void CheckGameRunning();
    bool ShowGameRunningWarning();
    void Fail(const char* messageKey, const std::wstring& error, const char* titleKey = "dialog_error");
    void Reorder(int from, int to, const std::wstring& status);
    // Runs `work` over a copy of the mods on a worker thread, then puts the copies back
    void RunOnCopies(std::function<int(std::vector<Models::ModInfo>&, std::wstring&)> work, std::function<void(int)> done,
                     const char* errorKey);

    HWND owner;
    std::unique_ptr<Services::Mods::ModManager> service;
    std::vector<Models::ModInfo> mods;
    Async::TimerId updateCheckTimer = 0;
    int updateCheckCounter = 0;
    bool isInitialized = false;
};

} // namespace SonicRacingSaveManager::Main
