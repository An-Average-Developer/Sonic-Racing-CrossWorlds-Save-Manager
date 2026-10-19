#pragma once
#include "Windows/Main/Overlays/CardOverlay.h"

namespace SonicRacingSaveManager::Main {

// Features\MemoryEditor\Views\ValueAppliedDialog.xaml as an in-window card: the green "Value
// Applied Successfully!" header, the red "FOR THE CHANGES TO TAKE EFFECT, SPEND TICKETS ON
// ANYTHING" (pulsing gently) and OK.
class ValueAppliedOverlay : public CardOverlay {
public:
    using CardOverlay::CardOverlay;

protected:
    float CardWidth() const override { return 600.f; }
    float CardHeight(UI::Ui& ui, float width) override;
    void DrawCard(UI::Ui& ui, const Rect& card) override;
    Color Accent() const override;
};

} // namespace SonicRacingSaveManager::Main
