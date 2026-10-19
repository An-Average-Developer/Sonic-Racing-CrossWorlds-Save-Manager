#pragma once
#include "Windows/Main/Overlays/CardOverlay.h"

namespace SonicRacingSaveManager::Main {

// The Add-to-Steam confirmation (AddToSteamOverlay of SteamTabControl.xaml), redesigned like the
// Inazuma editor's: the Steam-blue header, the shortcut tile bobbing with a shine passing over it,
// the app name, what happens to Steam (amber when it has to restart) and Cancel / ADD TO STEAM.
// Confirming relaunches this exe elevated as the Add-to-Steam helper and exits right away.
class AddToSteamOverlay : public CardOverlay {
public:
    using CardOverlay::CardOverlay;
    void Show(bool steamRunning);

protected:
    float CardWidth() const override { return 480.f; }
    float CardHeight(UI::Ui& ui, float width) override;
    void DrawCard(UI::Ui& ui, const Rect& card) override;
    Color Accent() const override;

private:
    void Confirm();
    bool steamRunning = false;
};

} // namespace SonicRacingSaveManager::Main
