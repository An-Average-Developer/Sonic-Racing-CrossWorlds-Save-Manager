#pragma once
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "Graphics/Images.h"
#include "Models/ModInfo.h"
#include "Services/Mods/GameBananaStore.h"
#include "Windows/Main/Tabs/Tab.h"

namespace SonicRacingSaveManager::Main {

class FileSelectionOverlay;
class ModInfoOverlay;

// The Mod Store (new in the C++ port): Sonic Racing CrossWorlds' mods on GameBanana. The banner,
// a toolbar (search, sort and the categories), the mods as cards in a grid (picture, name, author,
// the character it replaces, likes / views, an "Installed" / "Update available" badge for the mods
// already in the mods folder, Install and the GameBanana page) and the pages. Install downloads the
// mod, extracts it into a new folder of the mods folder and tells the Mod Manager. Split:
//   StoreTab.cpp    state, requests, installation, layout, toolbar, pages
//   StoreCards.cpp  the grid and its cards
class StoreTab : public Tab {
public:
    explicit StoreTab(MainWindow& window);
    ~StoreTab() override;
    void Draw(UI::Ui& ui, const Rect& area) override;
    void OnSelected() override;
    // Opens the info card of GameBanana mod `id` (the Mod Manager's Reinstall); `name` shows until
    // its details are here
    void ShowMod(long long id, const std::wstring& name);

private:
    friend class ModInfoOverlay;   // the info card shares the pictures, the installed mods and Install
    using StoreMod = Services::Mods::Store::StoreMod;
    using Sort = Services::Mods::Store::Sort;

    // Requests
    void Load();             // the current page with the current filters
    void LoadCategories();
    void ScanInstalled();
    void Install(const StoreMod& mod);
    void Reinstall(const StoreMod& mod);
    void ChooseFiles(const StoreMod& mod, std::function<void(std::vector<Models::GameBananaFile>)> then);
    void Download(const StoreMod& mod, std::vector<Models::GameBananaFile> files, bool reinstall);
    ImagePtr Picture(const std::wstring& url, unsigned maxSide = 640);   // nullptr until it's loaded

    // Drawing
    void Toolbar(UI::Ui& ui, const Rect& r);
    void SearchBox(UI::Ui& ui, const Rect& r);
    bool Chip(UI::Ui& ui, UI::Id id, const Rect& r, const std::wstring& text, bool on);
    float ChipWidth(UI::Ui& ui, const std::wstring& text) const;
    void Grid(UI::Ui& ui, const Rect& r);
    void Card(UI::Ui& ui, const Rect& r, int index);
    void CardPicture(UI::Ui& ui, const Rect& r, const StoreMod& mod, float hover);
    void Pages(UI::Ui& ui, const Rect& r);
    void Notice(UI::Ui& ui, const Rect& r);

    std::unique_ptr<FileSelectionOverlay> files;
    std::unique_ptr<ModInfoOverlay> info;
    double loadedAt = -1.0;

    // The list
    std::vector<StoreMod> mods;
    long long total = 0;
    int page = 1, perPage = 20;
    Sort sort = Sort::Popular;
    long long categoryId = 0;   // 0 = all
    std::wstring search, appliedSearch;
    double searchChangedAt = -10.0;
    bool searchPending = false;
    bool loading = false, loadedOnce = false;
    std::wstring error;
    unsigned generation = 0;   // a response for an older request is dropped
    double pageShownAt = -10.0;
    std::vector<Services::Mods::Store::Category> categories;

    // The mods folder and what's in it
    std::wstring modsDirectory;
    std::vector<Models::ModInfo> installed;

    // The installation (one at a time)
    long long installingId = 0;
    double installProgress = 0.0;
    std::wstring installStatus;
    std::wstring notice;   // the last result, over the grid
    bool noticeError = false;
    double noticeAt = -10.0;

    // Preview pictures
    struct PictureSlot {
        ImagePtr image;
        bool loading = false, failed = false;
    };
    std::map<std::wstring, PictureSlot> pictures;
    int picturesLoading = 0;
    std::map<long long, bool> revealed;   // pictures of rated mods the user chose to see
};

} // namespace SonicRacingSaveManager::Main
