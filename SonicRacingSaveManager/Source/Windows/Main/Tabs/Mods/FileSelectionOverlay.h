#pragma once
#include <functional>
#include <string>
#include <vector>
#include "Windows/Main/Overlays/CardOverlay.h"

namespace SonicRacingSaveManager::Main {

// Features\ModManager\Views\FileSelectionDialog.xaml (+ its view models) as an in-window card, over
// the mod update card: the mod's downloadable files with checkboxes (the first one checked), Select
// All / Deselect All, Cancel and Continue (enabled while something is checked).
class FileSelectionOverlay : public CardOverlay {
public:
    using CardOverlay::CardOverlay;

    struct Item {
        std::wstring fileName;
        long long fileSize = 0;
        bool isSelected = false;
    };
    // `done` gets the items with their checks, or nothing when cancelled / closed
    void Show(const std::wstring& modName, std::vector<Item> items, std::function<void(const std::vector<Item>*)> done);

protected:
    float CardWidth() const override { return 600.f; }
    float CardHeight(UI::Ui& ui, float width) override;
    void DrawCard(UI::Ui& ui, const Rect& card) override;

private:
    void Finish(bool confirmed);

    std::wstring modName;
    std::vector<Item> items;
    std::function<void(const std::vector<Item>*)> done;
};

} // namespace SonicRacingSaveManager::Main
