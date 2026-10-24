#pragma once
#include <array>
#include <memory>
#include <string>
#include <vector>
#include "Services/Discord/DiscordPresenceService.h"
#include "Settings/DiscordPresenceSettings.h"
#include "UI/Window.h"
#include "Windows/Main/MainScreen/MainLayout.h"
#include "Windows/Main/MainState.h"

namespace SonicRacingSaveManager::Main {

class Overlay;
class Tab;
class SettingsPopup;
class LanguagePopup;
class DiscordInfoOverlay;
class ThemeTransition;
class LoadingOverlay;
class BackupState;

// MainWindow.xaml(.cs): the header ColorZone with its floating buttons, the navigation rail with the
// tabs, the status bar, the header popups, the theme ring transition and the loading screen.
// Header / rail / status bar live in MainScreen\, popups in Overlays\, tab pages in Tabs\.
class MainWindow : public UI::Window {
public:
    // The C# TabControl's order (Changelogs is new, next to Updates; Store, the Mod Store, next to
    // the Mod Manager)
    enum TabIndex { CreateBackup, Restore, Manage, Tickets, Mods, Store, Updates, Changelogs, Reports, Steam, TabCount };
    // The rail shows groups: the backup tabs together, the mod tabs together, the rest alone. A
    // group with several tabs switches between them with the bar on top of the content (SubTabs).
    enum GroupIndex { BackupsGroup, TicketsGroup, ModsGroup, UpdatesGroup, ChangelogsGroup, ReportsGroup, SteamGroup, GroupCount };
    static constexpr int kGroupFirst[GroupCount] = { CreateBackup, Tickets, Mods, Updates, Changelogs, Reports, Steam };
    static constexpr int kGroupSize[GroupCount] = { 3, 1, 2, 1, 1, 1, 1 };
    static int GroupOf(int tab);

    MainWindow();
    ~MainWindow() override;
    bool Open();
    static MainWindow* Instance();

    MainState& State() { return *state; }
    BackupState& Backups() { return *backups; }
    Services::DiscordPresenceService& Presence() { return presence; }
    Settings::DiscordPresence& DiscordSettings() { return discordSettings; }

    // ── Actions used by the header, the rail and the overlays ──
    int SelectedTab() const { return selectedTab; }
    void SelectTab(int index);
    int SelectedGroup() const { return GroupOf(selectedTab); }
    // Shows the group's tab that was shown last (its first one the first time)
    void SelectGroup(int group);
    void ToggleTheme(const Rect& buttonRect);   // ThemeToggleButton_Click
    bool ThemeAnimating() const;
    void ApplySeasonTheme();                     // SeasonThemeRadio_Checked → ThemeService.RefreshSeason()
    void UpdateDiscordPresence();
    void BeginDrag();                            // dragging the borderless window by the header
    void Quit() { Close(); }
    // Runs the startup update notification once the loading screen is gone
    void LoadingFinished();

    SettingsPopup& SettingsMenu() { return *settings; }
    LanguagePopup& Languages() { return *languages; }
    DiscordInfoOverlay& DiscordInfo() { return *discordInfo; }
    const MainLayout& Layout() const { return layout; }
    // A tab's in-window card (drawn over the content, under the header popups)
    void AddOverlay(Overlay* overlay);
    // The Mod Store changed the mods folder: the Mod Manager scans it again
    void ModsChanged();
    // Opens the Mod Manager on the installed mod `name`
    void ShowInModManager(const std::wstring& name);
    // Opens the Mod Store on GameBanana mod `id`'s info card
    void ShowInStore(long long id, const std::wstring& name);

protected:
    void OnFrame() override;
    bool OnMessage(UINT msg, WPARAM wp, LPARAM lp, LRESULT& result) override;
    void OnClosing(bool& cancel) override;
    void OnDestroyed() override;
    Color ClearColor() const override;

private:
    friend class ThemeTransition;
    // Everything inside MainContent (what the theme transition snapshots)
    void DrawMainContent(const Rect& screen);
    void EnsureWindowIsVisible();
    void CheckAndNotifyUpdates();

    std::unique_ptr<MainState> state;
    std::unique_ptr<BackupState> backups;
    Services::DiscordPresenceService presence;
    Settings::DiscordPresence discordSettings = Settings::DiscordPresence::Load();
    MainLayout layout;
    int selectedTab = CreateBackup;
    // Another group slides in from below / above (lower / higher in the rail); another tab of the
    // same group slides in sideways, under its bar
    double groupChangedAt = -10.0, tabChangedAt = -10.0;
    float groupDirection = 1.f, tabDirection = 1.f;
    std::array<int, GroupCount> lastInGroup{ CreateBackup, Tickets, Mods, Updates, Changelogs, Reports, Steam };
    std::array<std::unique_ptr<Tab>, TabCount> tabs;

    std::unique_ptr<SettingsPopup> settings;
    std::unique_ptr<LanguagePopup> languages;
    std::unique_ptr<DiscordInfoOverlay> discordInfo;
    std::unique_ptr<ThemeTransition> themeTransition;
    std::unique_ptr<LoadingOverlay> loading;
    std::vector<Overlay*> overlays;   // z-order (first = bottom)

    bool dragging = false;
    std::wstring loggedStatus;   // last status message written to the log
};

} // namespace SonicRacingSaveManager::Main
