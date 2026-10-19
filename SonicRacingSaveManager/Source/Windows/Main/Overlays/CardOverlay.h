#pragma once
#include <functional>
#include "Graphics/Color.h"
#include "Windows/Main/Overlays/Overlay.h"

namespace SonicRacingSaveManager::Main {

// The in-window replacement of the C#'s dialog windows (ValueAppliedDialog, ModUpdateDialog,
// FileSelectionDialog, the Add-to-Steam overlay...), drawn like DiscordInfoOverlay: the window dims,
// the card springs up with a little bounce and its rows come in one by one. Escape or a click
// outside closes it while Closable() (the C#'s window close button).
class CardOverlay : public Overlay {
public:
    using Overlay::Overlay;
    void Open();
    void Close();
    bool IsOpen() const { return open; }
    bool Visible() const override { return open || changedAt > -10.0; }
    void Draw(UI::Ui& ui, const Rect& screen) final;

    // Runs once the card is closed (any way)
    std::function<void()> onClosed;
    // A card opened over this one (the file picker over the mod update): while it's shown this one
    // ignores Escape and outside clicks
    void SetCover(const CardOverlay* card) { cover = card; }

protected:
    virtual float CardWidth() const { return 520.f; }
    virtual float CardHeight(UI::Ui& ui, float width) = 0;
    virtual void DrawCard(UI::Ui& ui, const Rect& card) = 0;
    virtual bool Closable() const { return true; }
    virtual Color Accent() const;   // the glow around the card (the primary colour)
    // Entrance (0..1) of content row `index` while the card opens
    float Reveal(UI::Ui& ui, int index) const;
    double OpenedAt() const { return changedAt; }

    // The card's frame: shadow, glow and background (call first in DrawCard)
    void Frame(UI::Ui& ui, const Rect& card) const;

    static constexpr float kRadius = 18.f, kPad = 26.f;

private:
    bool open = false;
    double changedAt = -10.0;   // -1 / -2: stamp on the next frame (open / close)
    const CardOverlay* cover = nullptr;
};

} // namespace SonicRacingSaveManager::Main
