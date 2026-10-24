#include "Windows/Main/Tabs/Mods/ModsState.h"
#include <algorithm>
#include <future>
#include <map>
#include "App/Config.h"
#include "Core/Process.h"
#include "Core/System.h"
#include "Localization/Localization.h"
#include "Services/Mods/GameBananaApi.h"
#include "Settings/AppSettings.h"
#include "Windows/Dialogs/MessageDialog.h"

namespace SonicRacingSaveManager::Main {

namespace {
using Dialogs::MessageButtons;
using Dialogs::MessageIcon;
using Dialogs::MessageResult;
namespace Mods = Services::Mods;
constexpr int UPDATE_CHECK_INTERVAL = 60;   // seconds between GameBanana checks
} // namespace

ModsState::ModsState(HWND ownerWindow) : owner(ownerWindow) {
    statusMessage = Loc::Get("status_ready");
    updateCheckTimer = Async::Every(1000, [this] { UpdateCheckTimerTick(); });
}

ModsState::~ModsState() {
    if (updateCheckTimer) Async::CancelTimer(updateCheckTimer);
}

const Models::ModInfo* ModsState::SelectedMod() const {
    if (selectedMod.empty()) return nullptr;
    const auto it = std::find_if(mods.begin(), mods.end(), [&](const auto& m) { return m.name == selectedMod; });
    return it == mods.end() ? nullptr : &*it;
}

int ModsState::SelectedIndex() const {
    const Models::ModInfo* mod = SelectedMod();
    return mod ? static_cast<int>(mod - mods.data()) : -1;
}

int ModsState::EnabledModCount() const { return static_cast<int>(std::count_if(mods.begin(), mods.end(), [](const auto& m) { return m.isEnabled; })); }
int ModsState::DisabledModCount() const { return TotalModCount() - EnabledModCount(); }
bool ModsState::CanEnableAll() const { return DisabledModCount() > 0 && !isGameRunning; }
bool ModsState::CanDisableAll() const { return EnabledModCount() > 0 && !isGameRunning; }

void ModsState::Fail(const char* messageKey, const std::wstring& error, const char* titleKey) {
    statusMessage = Loc::Fmt("status_error", error);
    Dialogs::Message(owner, Loc::Fmt(messageKey, error), Loc::Get(titleKey), MessageButtons::Ok, MessageIcon::Error);
}

void ModsState::EnsureInitialized() {
    if (isInitialized) return;
    isInitialized = true;
    service = std::make_unique<Mods::ModManager>(Settings::AppSettings::GetModsDirectory().value_or(L""));
    RefreshMods();
    CheckForUpdates();
}

// ── The game-running lock and the periodic update check ─────────────────────

void ModsState::UpdateCheckTimerTick() {
    CheckGameRunning();
    if (++updateCheckCounter >= UPDATE_CHECK_INTERVAL) {
        updateCheckCounter = 0;
        CheckForUpdates();
    }
}

// The C# looked for "ASRT-Win64-Shipping" (another game's process); this looks for CrossWorlds
void ModsState::CheckGameRunning() {
    const bool wasRunning = isGameRunning;
    isGameRunning = !Process::FindByName(Config::Game::ProcessName).empty();
    if (isGameRunning && !wasRunning) statusMessage = Loc::Get("mods_status_game_running");
    else if (!isGameRunning && wasRunning) statusMessage = Loc::Get("mods_status_game_closed");
}

bool ModsState::ShowGameRunningWarning() {
    if (!isGameRunning) return false;
    Dialogs::Message(owner, Loc::Get("mods_game_running_message"), Loc::Get("mods_game_running_title"), MessageButtons::Ok, MessageIcon::Warning);
    return true;
}

// ── Scan ────────────────────────────────────────────────────────────────────

void ModsState::RefreshMods(std::function<void()> then, bool replay) {
    if (!service) {
        statusMessage = Loc::Get("mods_status_not_initialized");
        return;
    }
    isLoading = true;
    statusMessage = Loc::Get("mods_status_scanning");
    const Mods::ModManager copy = *service;
    Async::Run([copy] { return copy.ScanForMods(); }, [this, then, replay](std::vector<Models::ModInfo> result) {
        mods = std::move(result);
        if (replay) ++revision;
        if (!SelectedMod()) selectedMod.clear();
        isPriorityMode = Mods::ModManager::IsPriorityModeEnabled(mods);
        statusMessage = !mods.empty() ? Loc::Fmt("mods_status_found", TotalModCount(), EnabledModCount(), DisabledModCount())
                        : ModsDirectoryExists() ? Loc::Get("mods_status_none")
                                                : Loc::Get("mods_status_no_directory");
        isLoading = false;
        if (then) then();
    });
}

// ── Toggles ─────────────────────────────────────────────────────────────────

void ModsState::RunOnCopies(std::function<int(std::vector<Models::ModInfo>&, std::wstring&)> work, std::function<void(int)> done,
                            const char* errorKey) {
    struct Outcome {
        std::vector<Models::ModInfo> mods;
        int count = 0;
        std::wstring error;
        bool ok = false;
    };
    isLoading = true;
    Async::Run(
        [copies = mods, work]() mutable {
            Outcome o;
            o.mods = std::move(copies);
            try {
                std::wstring unused;
                o.count = work(o.mods, unused);
                o.ok = true;
            } catch (const Mods::Error& e) {
                o.error = e.Message();
            }
            return o;
        },
        [this, done, errorKey](Outcome o) {
            // The mods changed on disk as far as the work got, failure or not
            mods = std::move(o.mods);
            isLoading = false;
            isReordering = false;
            if (o.ok) {
                done(o.count);
            } else {
                Fail(errorKey, o.error);
                RefreshMods({}, false);   // back in step with the folder
            }
        });
}

void ModsState::ToggleSelectedMod() {
    const Models::ModInfo* mod = SelectedMod();
    if (!mod || !service) return;
    if (ShowGameRunningWarning()) return;
    const std::wstring name = mod->name;
    const bool wasEnabled = mod->isEnabled;
    const Mods::ModManager copy = *service;
    RunOnCopies(
        [copy, name](std::vector<Models::ModInfo>& list, std::wstring&) {
            for (auto& m : list)
                if (m.name == name) copy.ToggleMod(m);
            return 1;
        },
        [this, name, wasEnabled](int) { statusMessage = Loc::Fmt(wasEnabled ? "mods_status_disabled_one" : "mods_status_enabled_one", name); },
        "mods_toggle_failed");
}

// The folder goes to the Recycle Bin; in priority mode the others are renumbered to close the gap
void ModsState::UninstallSelectedMod() {
    const Models::ModInfo* mod = SelectedMod();
    if (!mod || !service) return;
    if (ShowGameRunningWarning()) return;
    const std::wstring name = mod->name;
    Dialogs::Message(owner, Loc::Fmt("mods_uninstall_confirm", name), Loc::Get("mods_uninstall_title"), MessageButtons::YesNo, MessageIcon::Warning,
                     [this, name](MessageResult result) {
                         if (result != MessageResult::Yes) return;
                         statusMessage = Loc::Fmt("mods_status_uninstalling", name);
                         const Mods::ModManager copy = *service;
                         const bool renumber = isPriorityMode;
                         RunOnCopies(
                             [copy, name, renumber](std::vector<Models::ModInfo>& list, std::wstring&) {
                                 const auto it = std::find_if(list.begin(), list.end(), [&](const auto& m) { return m.name == name; });
                                 if (it == list.end()) return 0;
                                 copy.UninstallMod(*it);
                                 list.erase(it);
                                 if (renumber) copy.ReorderMods(list);
                                 return 1;
                             },
                             [this, name](int) {
                                 selectedMod.clear();
                                 RefreshMods([this, name] { statusMessage = Loc::Fmt("mods_status_uninstalled", name); });
                             },
                             "mods_uninstall_failed");
                     });
}

void ModsState::EnableAllMods() {
    if (!service || ShowGameRunningWarning()) return;
    Dialogs::Message(owner, Loc::Get("mods_enable_all_confirm"), Loc::Get("mods_enable_all_title"), MessageButtons::YesNo, MessageIcon::Question,
                     [this](MessageResult result) {
                         if (result != MessageResult::Yes) return;
                         statusMessage = Loc::Get("mods_status_enabling_all");
                         const Mods::ModManager copy = *service;
                         RunOnCopies(
                             [copy](std::vector<Models::ModInfo>& list, std::wstring&) {
                                 int count = 0;
                                 for (auto& m : list)
                                     if (!m.isEnabled) {
                                         copy.EnableMod(m);
                                         ++count;
                                     }
                                 return count;
                             },
                             [this](int count) { statusMessage = Loc::Fmt("mods_status_enabled_count", count); }, "mods_enable_all_failed");
                     });
}

void ModsState::DisableAllMods() {
    if (!service || ShowGameRunningWarning()) return;
    Dialogs::Message(owner, Loc::Get("mods_disable_all_confirm"), Loc::Get("mods_disable_all_title"), MessageButtons::YesNo, MessageIcon::Question,
                     [this](MessageResult result) {
                         if (result != MessageResult::Yes) return;
                         statusMessage = Loc::Get("mods_status_disabling_all");
                         const Mods::ModManager copy = *service;
                         RunOnCopies(
                             [copy](std::vector<Models::ModInfo>& list, std::wstring&) {
                                 int count = 0;
                                 for (auto& m : list)
                                     if (m.isEnabled) {
                                         copy.DisableMod(m);
                                         ++count;
                                     }
                                 return count;
                             },
                             [this](int count) { statusMessage = Loc::Fmt("mods_status_disabled_count", count); }, "mods_disable_all_failed");
                     });
}

// ── Folders ─────────────────────────────────────────────────────────────────

void ModsState::OpenModsFolder() {
    if (!service) return;
    try {
        service->OpenModsFolder();
    } catch (const Mods::Error& e) {
        Dialogs::Message(owner, Loc::Fmt("mods_open_folder_failed", e.Message()), Loc::Get("dialog_error"), MessageButtons::Ok, MessageIcon::Error);
    }
}

void ModsState::SelectModsFolder() {
    const std::wstring folder = System::PickFolder(owner, Loc::Get("mods_select_folder_title"));
    if (folder.empty()) return;
    Settings::AppSettings::SaveModsDirectory(folder);
    service = std::make_unique<Mods::ModManager>(folder);
    RefreshMods([this, folder] {
        statusMessage = Loc::Get("mods_status_folder_updated");
        Dialogs::Message(owner, Loc::Fmt("mods_folder_set_message", folder), Loc::Get("dialog_success"), MessageButtons::Ok, MessageIcon::Information);
    });
}

// ── GameBanana ──────────────────────────────────────────────────────────────

void ModsState::CheckForUpdates() {
    if (!service) return;
    struct Check {
        std::wstring folderPath, url, version;
    };
    std::vector<Check> checks;
    for (const auto& m : mods)
        if (!m.modPageUrl.empty()) checks.push_back({ m.folderPath, m.modPageUrl, m.version });
    if (checks.empty()) {
        statusMessage = Loc::Get("mods_status_no_urls");
        return;
    }
    isLoading = true;
    statusMessage = Loc::Get("mods_status_checking_updates");
    using Results = std::map<std::wstring, Mods::GameBanana::UpdateCheck>;
    Async::Run(
        [checks] {
            // Task.WhenAll: every mod at once
            std::vector<std::future<std::optional<Mods::GameBanana::UpdateCheck>>> futures;
            for (const auto& c : checks)
                futures.push_back(std::async(std::launch::async, [c] { return Mods::GameBanana::CheckForUpdate(c.url, c.version); }));
            Results results;
            for (size_t i = 0; i < checks.size(); ++i)
                if (auto r = futures[i].get()) results[checks[i].folderPath] = *r;
            return results;
        },
        [this, checks](Results results) {
            int withUpdates = 0;
            for (auto& m : mods) {
                const auto it = results.find(m.folderPath);
                if (it != results.end()) {
                    m.latestVersion = it->second.latestVersion;
                    if (!m.version.empty()) m.hasUpdate = it->second.hasUpdate;
                }
                if (!m.modPageUrl.empty() && m.hasUpdate) ++withUpdates;
            }
            statusMessage = withUpdates > 0 ? Loc::Fmt("mods_status_updates_found", withUpdates) : Loc::Get("mods_status_all_up_to_date");
            isLoading = false;
        });
}

void ModsState::ShowUpdateDialog(const std::wstring& modName) {
    const auto it = std::find_if(mods.begin(), mods.end(), [&](const auto& m) { return m.name == modName; });
    if (it == mods.end() || !it->hasUpdate || !service) return;
    if (ShowGameRunningWarning()) return;
    if (onShowUpdate) onShowUpdate(*it, *service);
}

void ModsState::UpdateFinished(const Models::ModInfo& mod) {
    statusMessage = Loc::Fmt("mods_status_updated", mod.name, mod.latestVersion);
    RefreshMods();
}

} // namespace SonicRacingSaveManager::Main
