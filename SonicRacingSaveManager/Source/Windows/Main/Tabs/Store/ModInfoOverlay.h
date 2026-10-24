#pragma once
#include <optional>
#include <string>
#include "Services/Mods/GameBananaStore.h"
#include "Windows/Main/Overlays/CardOverlay.h"

namespace SonicRacingSaveManager::Main {

class StoreTab;

// The Mod Store's info card: a mod's gallery (the big picture with its caption, arrows / ← → and
// the thumbnails), its stats and Install / GameBanana on the left; its name, author, character,
// description, credits and files on the right (scrolling). The details load when it opens.
class ModInfoOverlay : public CardOverlay {
public:
    ModInfoOverlay(MainWindow& window, StoreTab& store);
    void Show(const Services::Mods::Store::StoreMod& mod);
    // Opened from the Mod Manager: only the id and the installed name are known, the rest comes
    // with the details
    void ShowById(long long id, const std::wstring& name);

protected:
    float CardWidth() const override { return 1060.f; }
    float CardHeight(UI::Ui& ui, float width) override;
    void DrawCard(UI::Ui& ui, const Rect& card) override;

private:
    void Gallery(UI::Ui& ui, const Rect& r);
    void Actions(UI::Ui& ui, const Rect& r);
    void Details(UI::Ui& ui, const Rect& r);

    StoreTab& store;
    Services::Mods::Store::StoreMod mod;
    std::optional<Services::Mods::Store::ModDetails> details;
    std::wstring error;
    bool loading = false;
    bool partial = false;         // `mod` is only the id and name (ShowById)
    unsigned generation = 0;
    int picture = 0;              // the one shown big
    double pictureChangedAt = -10.0;
    float pictureDirection = 1.f;
};

} // namespace SonicRacingSaveManager::Main
