#pragma once
#include "Windows/Main/Overlays/CardOverlay.h"

namespace SonicRacingSaveManager::Main {

class SaveEditorTab;

// How to edit a Nintendo Switch save, as an in-window card (it goes with the window, minimized or not):
// what the file is (a JKSV backup's "app"), the console's 3 systems and which one to use (CFW on sysMMC,
// which shares its saves with the stock system; emuMMC doesn't), the steps one by one, and the button
// that picks the "app" file. Saving it writes a .zip of the whole backup folder next to it, ready for
// JKSV to restore.
class SaveEditorSwitch : public CardOverlay {
public:
    SaveEditorSwitch(MainWindow& window, SaveEditorTab& editor) : CardOverlay(window), tab(editor) {}

protected:
    float CardWidth() const override { return 780.f; }
    float CardHeight(UI::Ui& ui, float width) override;
    void DrawCard(UI::Ui& ui, const Rect& card) override;
    Color Accent() const override;

private:
    // Lays the card out from `top`; draws it too when `draw`. Returns where it ends.
    float Body(UI::Ui& ui, const Rect& card, bool draw);

    SaveEditorTab& tab;
};

} // namespace SonicRacingSaveManager::Main
