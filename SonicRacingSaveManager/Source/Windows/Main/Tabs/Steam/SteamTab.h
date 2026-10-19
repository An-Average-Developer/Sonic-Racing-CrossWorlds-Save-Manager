#pragma once
#include <memory>
#include "Windows/Main/Tabs/Tab.h"

namespace SonicRacingSaveManager::Main {

class AddToSteamOverlay;

// Features\Steam\Views\SteamTabControl, redesigned like the Inazuma editor's Steam cards: the banner
// over two columns, the shortcut and library artwork preview (hero banner with the logo, the icon,
// capsule and poster) with Add to Steam on the left; whether Steam is running and what the shortcut
// adds on the right. Add to Steam opens the confirmation card (SteamOverlay.cpp), which hands the
// work to the helper process and closes the manager.
class SteamTab : public Tab {
public:
    explicit SteamTab(MainWindow& window);
    ~SteamTab() override;
    void Draw(UI::Ui& ui, const Rect& area) override;
    void OnSelected() override;   // SteamTabControl_Loaded → RefreshStatus

private:
    void Preview(UI::Ui& ui, const Rect& r);
    float StatusCard(UI::Ui& ui, const Rect& r, bool draw);

    std::unique_ptr<AddToSteamOverlay> overlay;
    double loadedAt = -1.0;
    bool steamRunning = false;
};

} // namespace SonicRacingSaveManager::Main
