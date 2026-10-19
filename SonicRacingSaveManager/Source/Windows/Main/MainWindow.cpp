#include "Windows/Main/MainWindow.h"
#include "Core/Async.h"
#include "Localization/Localization.h"
#include "Seasonal/SeasonalBackdrop.h"
#include "Seasonal/SeasonalOverlay.h"
#include "Seasonal/SeasonalTheme.h"
#include "Services/App/AppVersion.h"
#include "Services/App/ChangelogData.h"
#include "Services/App/Log.h"
#include "Settings/AppSettings.h"
#include "UI/Material/Motion.h"
#include "UI/Material/Theme.h"
#include "UI/Material/ToolTip.h"
#include "UI/Widgets/ScrollArea.h"
#include "Windows/Dialogs/MessageDialog.h"
#include "Windows/Main/MainScreen/Bars.h"
#include "Windows/Main/Overlays/DiscordInfoOverlay.h"
#include "Windows/Main/Overlays/HeaderPopups.h"
#include "Windows/Main/Overlays/ThemeTransition.h"
#include "Windows/Main/Tabs/Backup/BackupState.h"
#include "Windows/Main/Tabs/Backup/BackupTabs.h"
#include "Windows/Main/Tabs/Changelogs/ChangelogsTab.h"
#include "Windows/Main/Tabs/Mods/ModsTab.h"
#include "Windows/Main/Tabs/Reports/ReportsTab.h"
#include "Windows/Main/Tabs/Steam/SteamTab.h"
#include "Windows/Main/Tabs/Tickets/TicketsTab.h"
#include "Windows/Main/Tabs/Updates/UpdatesTab.h"
#include "Windows/Splash/LoadingScreen.h"

namespace SonicRacingSaveManager::Main {

namespace {
MainWindow* instance = nullptr;
}

MainWindow* MainWindow::Instance() { return instance; }

MainWindow::MainWindow() {
    instance = this;
    settings = std::make_unique<SettingsPopup>(*this);
    languages = std::make_unique<LanguagePopup>(*this);
    discordInfo = std::make_unique<DiscordInfoOverlay>(*this);
    themeTransition = std::make_unique<ThemeTransition>();
    loading = std::make_unique<LoadingOverlay>(*this);
    // The loading screen over the main content; the header popups (windows of their own in WPF) and
    // the in-window cards on top
    overlays = { loading.get(), settings.get(), languages.get(), discordInfo.get() };
    tabs[CreateBackup] = std::make_unique<CreateBackupTab>(*this);
    tabs[Restore] = std::make_unique<RestoreTab>(*this);
    tabs[Manage] = std::make_unique<ManageTab>(*this);
    tabs[Tickets] = std::make_unique<TicketsTab>(*this);
    tabs[Mods] = std::make_unique<ModsTab>(*this);
    tabs[Updates] = std::make_unique<UpdatesTab>(*this);
    tabs[Changelogs] = std::make_unique<ChangelogsTab>(*this);
    tabs[Reports] = std::make_unique<ReportsTab>(*this);
    tabs[Steam] = std::make_unique<SteamTab>(*this);
}

void MainWindow::AddOverlay(Overlay* overlay) { overlays.insert(overlays.begin() + 1, overlay); }

MainWindow::~MainWindow() {
    if (instance == this) instance = nullptr;
}

bool MainWindow::Open() {
    UI::Window::Options o;
    o.title = L"Sonic Racing CrossWorlds - Save Manager";   // taskbar / Alt+Tab
    // Borderless and fixed size, like the other C++ ports (the C# window was resizable); minimizable
    // from the taskbar
    o.style = WS_POPUP | WS_MINIMIZEBOX | WS_SYSMENU;
    o.width = static_cast<int>(MainLayout::WindowWidth);
    o.height = static_cast<int>(MainLayout::WindowHeight);
    o.showCommand = SW_HIDE;
    if (!Create(o)) return false;
    state = std::make_unique<MainState>(hwnd);
    backups = std::make_unique<BackupState>(*state);
    for (auto& tab : tabs) tab->OnWindowOpened();
    ui.tooltipRenderer = [](Graphics& gr, const std::wstring& text, float mx, float my) { UI::Material::DrawToolTip(gr, text, mx, my); };

    // WindowStartupLocation="CenterScreen"
    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    RECT r{};
    GetWindowRect(hwnd, &r);
    MoveTo(work.left + (work.right - work.left - (r.right - r.left)) / 2, work.top + (work.bottom - work.top - (r.bottom - r.top)) / 2);

    HINSTANCE module = GetModuleHandleW(nullptr);
    SetIcon(static_cast<HICON>(LoadImageW(module, MAKEINTRESOURCEW(101), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0)),
            static_cast<HICON>(LoadImageW(module, MAKEINTRESOURCEW(101), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0)));
    presence.SetVersion(AppVersion::DisplayVersion());
    presence.ApplySettings(discordSettings.showActivity);
    if (!Settings::LocalSettings::GetShowLoadingScreen()) loading->Hide();

    Show(SW_SHOW);
    Activate();
    // Seasonal scenery and particles: idle frames at ~60 fps
    SetContinuous(true, true);

    // MainViewModel(): accounts + backups, then the silent update check
    backups->Initialize();
    state->Initialize();
#ifdef _DEBUG
    // Development aid: SRCW_OPEN_TAB=<index> opens that tab at startup (screenshots); "install" shows
    // the Updates tab's installation view, "manualupdate" its manual view, "discordinfo" the Reports
    // tab's "Why Discord?" card
    wchar_t open[64]{};
    if (GetEnvironmentVariableW(L"SRCW_OPEN_TAB", open, 64) > 0) {
        const std::wstring name = open;
        Async::After(1500, [this, name] {
            if (name == L"install") {
                MainState& s = *state;
                s.currentVersion = L"v2.0.0";
                s.latestVersion = L"v" + Services::Changelog::Entries().front().version;
                s.updateFileName = L"SRCW-SMv2-1-0.zip";
                s.updateFileSize = L"12.34 MB";
                s.installTitle = Loc::Get("updates_install_title_github");
                s.changelog = Services::Changelog::EmbeddedText();
                s.showingInstallationView = true;
                SelectTab(Updates);
            } else if (name == L"manualupdate") {
                static_cast<UpdatesTab&>(*tabs[Updates]).OpenManual();
                SelectTab(Updates);
            } else if (name == L"discordinfo") {
                SelectTab(Reports);
                discordInfo->Open();
            } else {
                SelectTab(_wtoi(name.c_str()));
            }
        });
    }
#endif
    UpdateDiscordPresence();
    // With the loading screen disabled the update notification still comes after a second
    if (!loading->Visible()) Async::After(1000, [this] { CheckAndNotifyUpdates(); });
    return true;
}

void MainWindow::SelectTab(int index) {
    if (index == selectedTab || index < 0 || index >= TabCount) return;
    tabs[selectedTab]->OnDeselected();
    tabDirection = index > selectedTab ? 1.f : -1.f;
    tabChangedAt = ui.Time();
    selectedTab = index;
    static const wchar_t* const names[TabCount] = { L"Create Backup", L"Restore", L"Manage", L"Tickets Menu", L"Mod Manager",
                                                    L"Updates",       L"Changelogs", L"Reports", L"Steam" };
    Log::Info(std::wstring(L"Tab: ") + names[index]);
    tabs[index]->OnSelected();
    UpdateDiscordPresence();   // MainTabControl_SelectionChanged
}

void MainWindow::ToggleTheme(const Rect& buttonRect) { themeTransition->Request(buttonRect.CX(), buttonRect.CY()); }

bool MainWindow::ThemeAnimating() const { return themeTransition->Animating(); }

void MainWindow::ApplySeasonTheme() {
    UI::Material::RefreshSeason();
    // The seasonal themes are dark only (light / dark works with Default and Classic)
    if (Seasonal::IsSeasonal() && !UI::Material::IsDarkTheme()) {
        UI::Material::SetDarkTheme(true);
        themeTransition->SyncIcon();
    }
    Invalidate();
}

void MainWindow::UpdateDiscordPresence() {
    using Mode = Services::DiscordPresenceService::Mode;
    switch (selectedTab) {
        case Updates: presence.SetMode(Mode::Updates); break;
        case Changelogs: presence.SetMode(Mode::Changelogs); break;
        case Reports: presence.SetMode(Mode::Error); break;
        default: presence.SetMode(Mode::Normal); break;
    }
}

void MainWindow::BeginDrag() {
    dragging = true;
    ReleaseCapture();
    SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);   // returns when the drag ends
    dragging = false;
}

// Keeps the borderless window reachable after a move (not while dragging)
void MainWindow::EnsureWindowIsVisible() {
    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    RECT r{};
    GetWindowRect(hwnd, &r);
    const int minVisible = static_cast<int>(50 * DpiScale());   // minimum visible part of the header
    int left = r.left, top = r.top;
    const int width = r.right - r.left;
    if (top < work.top) top = work.top;
    if (top + minVisible > work.bottom) top = work.bottom - minVisible;
    if (left + minVisible > work.right) left = work.right - minVisible;
    if (left + width < work.left + minVisible) left = work.left + minVisible - width;
    if (left != r.left || top != r.top) SetWindowPos(hwnd, nullptr, left, top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void MainWindow::LoadingFinished() { CheckAndNotifyUpdates(); }

// New in the C++ port (as in the Inazuma editor): an available update is offered once at startup
void MainWindow::CheckAndNotifyUpdates() {
    const auto offer = [this] {
        if (!state->isUpdateAvailable) return;
        Dialogs::Message(hwnd, Loc::Fmt("startup_update_message", state->latestVersion), Loc::Get("startup_update_title"),
                         Dialogs::MessageButtons::YesNo, Dialogs::MessageIcon::Information, [this](Dialogs::MessageResult result) {
                             if (result != Dialogs::MessageResult::Yes || !state->CanShowInstallationView()) return;
                             state->ShowInstallationView();
                             SelectTab(Updates);   // the installation view lives in the Updates tab
                         });
    };
    // Wait for the silent check when it hasn't finished yet
    if (state->hasCheckedForUpdates) Async::After(500, offer);
    else state->onUpdatesChecked = [this, offer] {
        state->onUpdatesChecked = nullptr;
        Async::After(500, offer);
    };
}

Color MainWindow::ClearColor() const { return UI::Material::P().background; }

void MainWindow::OnFrame() {
    const Rect screen{ 0.f, 0.f, g.Width(), g.Height() };
    layout = MainLayout::Compute(screen);
    UI::ScrollArea::SetMaterialColors(UI::Material::P().scrollBarForeground, UI::Material::P().foreground);
    // The log follows the status bar (bug reports can attach it)
    if (state->statusMessage != loggedStatus) {
        loggedStatus = state->statusMessage;
        if (!loggedStatus.empty()) Log::Info(L"Status: " + loggedStatus);
    }

    if (themeTransition->Pending()) {
        // RenderTargetBitmap of MainContent with the old theme, then ThemeService.ToggleTheme()
        auto snapshot = g.CreateWindowBitmap();
        if (snapshot) {
            g.BeginOffscreen(snapshot.Get());
            g.Clear(ClearColor());
            DrawMainContent(screen);
            g.EndOffscreen();
            g.DrawBitmap(snapshot.Get(), screen);
        } else {
            DrawMainContent(screen);
        }
        themeTransition->Begin(snapshot, ui.Time());
    } else {
        DrawMainContent(screen);
        themeTransition->Draw(g, screen, ui.Time());
        if (themeTransition->Animating()) ui.KeepAnimating();
    }
    for (Overlay* o : overlays)
        if (o->Visible()) o->Draw(ui, screen);
}

void MainWindow::DrawMainContent(const Rect& screen) {
    const auto& p = UI::Material::P();
    g.FillRect(screen, p.background);   // Background="{DynamicResource MaterialDesignPaper}"

    const double seasonTime = Seasonal::AnimationTime(ui.Time(), false);
    if (Seasonal::IsSeasonal()) ui.KeepAnimatingThrottled();
    {
        // Seasonal scenery behind the whole UI; the garland hangs under the header
        NoColorFilter keep(g);
        Seasonal::Backdrop::Draw(g, screen, MainLayout::HeaderHeight, seasonTime);
    }
    Header::Draw(ui, *this, layout.header);
    NavRail::Draw(ui, *this, layout.rail);
    {
        ui.PushClip(layout.content);
        {
            UI::Material::Motion::Enter enter(ui, UI::Material::Motion::Stagger(ui, tabChangedAt, 0, 0.f, 0.32f), 28.f * tabDirection);
            tabs[selectedTab]->Draw(ui, layout.content);
        }
        ui.PopClip();
    }
    StatusBar::Draw(ui, *this, layout.statusBar);
    {
        // Seasonal ambient animations (click-through, z 450)
        NoColorFilter keep(g);
        Seasonal::Ambient::Draw(g, screen, seasonTime);
    }
}

bool MainWindow::OnMessage(UINT msg, WPARAM wp, LPARAM lp, LRESULT& result) {
    (void)lp;
    switch (msg) {
        case WM_SYSCOMMAND:
            // Fixed size: no maximize
            if ((wp & 0xFFF0) == SC_MAXIMIZE) {
                result = 0;
                return true;
            }
            return false;
        case WM_MOVE:
            if (!dragging) EnsureWindowIsVisible();
            return false;
        default: return false;
    }
}

// MainWindow_Closing: _discordPresence.Dispose()
void MainWindow::OnClosing(bool& cancel) {
    cancel = false;
    for (auto& tab : tabs) tab->OnClosing();
    presence.ApplySettings(false);
}

void MainWindow::OnDestroyed() { PostQuitMessage(0); }

} // namespace SonicRacingSaveManager::Main
