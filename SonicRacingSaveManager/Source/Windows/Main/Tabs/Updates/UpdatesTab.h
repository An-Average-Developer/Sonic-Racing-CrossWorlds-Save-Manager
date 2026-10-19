#pragma once
#include <optional>
#include "Services/App/UpdateService.h"
#include "Windows/Main/Tabs/Tab.h"
#include "Windows/Main/Tabs/Updates/ChangelogCards.h"

namespace SonicRacingSaveManager::Main {

// The Updates tab of MainWindow.xaml, redesigned like the Reports tab: the animated banner over two
// columns. Overview: the update status (checking / up to date / update available) with its actions
// on the left; your version, the update source and how updates work on the right. Installation view (MainViewModel.ShowingInstallationView): what's new since
// your version on the left, the install card (from → to, file, Download & Install or the progress
// ring) on the right. Manual update view (was the ManualUpdateDialog): always fetches the latest
// published release, whatever the version comparison says, to force a reinstall or open it in a
// browser.
class UpdatesTab : public Tab {
public:
    using Tab::Tab;
    void Draw(UI::Ui& ui, const Rect& area) override;
    void OnSelected() override { loadedAt = -1.0; }
    void OpenManual();

private:
    enum class Manual { Checking, Ready, Unreachable, Downloading, Failed };

    void DrawOverview(UI::Ui& ui, const Rect& top);
    void DrawInstallation(UI::Ui& ui, const Rect& top);
    void DrawManual(UI::Ui& ui, const Rect& top);
    void StatusHero(UI::Ui& ui, const Rect& r);
    void ManualHero(UI::Ui& ui, const Rect& r);
    float VersionCard(UI::Ui& ui, const Rect& r, bool draw);
    float ManualReleaseCard(UI::Ui& ui, const Rect& r, bool draw);
    float SourcesCard(UI::Ui& ui, const Rect& r, bool draw);
    void HowCard(UI::Ui& ui, const Rect& r, const char* key, const std::wstring& title, const std::wstring* steps, int count);
    void InstallCard(UI::Ui& ui, const Rect& r);
    void ManualCheck();
    void ManualDownload();
    void ManualOpenBrowser();

    double loadedAt = -1.0;       // entrance of the whole page
    int shownView = -1;           // 0 overview, 1 installation, 2 manual
    ChangelogCards changelog{ "upd-news" };   // What's New of the installation view

    bool manualView = false;
    Manual manual = Manual::Checking;
    std::optional<Services::Updates::UpdateInfo> manualRelease;
    int manualProgress = 0;
};

} // namespace SonicRacingSaveManager::Main
