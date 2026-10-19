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
    // The C# TabControl's order (Changelogs is new, next to Updates)
    enum TabIndex { CreateBackup, Restore, Manage, Tickets, Mods, Updates, Changelogs, Reports, Steam, TabCount };

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
    double tabChangedAt = -10.0;   // the new tab slides in (from below when it's lower in the rail)
    float tabDirection = 1.f;
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
