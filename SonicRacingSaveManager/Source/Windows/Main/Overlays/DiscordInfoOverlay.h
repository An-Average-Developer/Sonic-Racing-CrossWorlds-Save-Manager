#pragma once
#include "Windows/Main/Overlays/Overlay.h"

namespace SonicRacingSaveManager::Main {

// Why the Reports tab asks to connect with Discord (was the DiscordInfoDialog window), drawn inside
// the main window: the window dims, the card springs up with the Discord-coloured header (the logo
// bobbing in a pulsing ring), then the intro and the three points come in one by one. Closed with
// GOT IT, the X, Escape or a click outside.
class DiscordInfoOverlay : public Overlay {
public:
    using Overlay::Overlay;
    void Open();
    void Close();
    bool Visible() const override { return open || changedAt > -10.0; }
    void Draw(UI::Ui& ui, const Rect& screen) override;

private:
    float CardHeight(UI::Ui& ui, float width) const;
    void DrawCard(UI::Ui& ui, const Rect& card);
    float Reveal(UI::Ui& ui, int index) const;

    bool open = false;
    double changedAt = -10.0;   // -1 / -2: stamp on the next frame (open / close)
};

} // namespace SonicRacingSaveManager::Main
