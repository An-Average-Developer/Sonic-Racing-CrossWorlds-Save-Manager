#pragma once
#include <string>
#include "Windows/Main/Overlays/CardOverlay.h"

namespace SonicRacingSaveManager::Main {

class SaveEditorTab;

// The versions of a character in the Save Editor (Sonic, its AI racer, Super Sonic, Werehog,
// Classic Sonic...) as an in-window card: its picture, name and "unlocked / total" on top, a tile
// per version that unlocks / locks it like the tab's, and Unlock all. The honor titles that go
// together (a character's, a festival's) open in it too, as the tab's banners. Escape or a click
// outside closes it.
class SaveEditorVariants : public CardOverlay {
public:
    SaveEditorVariants(MainWindow& window, SaveEditorTab& editor) : CardOverlay(window), tab(editor) {}
    // `family`: a SaveEditorTab::Family id (the character's own id)
    void Show(const std::wstring& family);

protected:
    float CardWidth() const override;
    float CardHeight(UI::Ui& ui, float width) override;
    void DrawCard(UI::Ui& ui, const Rect& card) override;
    Color Accent() const override;

private:
    int Columns() const;
    float TileW() const;
    float TileH(float tileW) const;

    SaveEditorTab& tab;
    std::wstring family;
    size_t count = 0;   // its versions (for the card's size)
    bool titles = false;   // honor titles: banners
};

} // namespace SonicRacingSaveManager::Main
