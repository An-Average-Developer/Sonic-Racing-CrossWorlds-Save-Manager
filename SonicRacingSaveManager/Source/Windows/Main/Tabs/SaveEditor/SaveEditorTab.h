#pragma once
#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "Services/SaveFile/AppSave.h"
#include "UI/Material/PackIcon.h"
#include "Windows/Main/Tabs/SaveEditor/SaveEditorKit.h"
#include "Windows/Main/Tabs/Tab.h"

namespace SonicRacingSaveManager::Main {

class SaveEditorVariants;
class SaveEditorCourse;
class SaveEditorBuild;
class SaveEditorSwitch;

// The Save Editor tab: edits app.sav itself with the game closed. A header with the account, the
// save's state and its actions sits on top of:
//   - no save open: the accounts to pick one from, and how it works
//   - a save open: the menu, a big card per section (Characters, Courses, Honor titles) with its
//     picture and progress; in a section, a row to go back to the menu or to another section
//   - the characters: a bar (title, counters, filter, search, Unlock all) over them, as picture
//     tiles when the game's pictures are cached, as rows otherwise. The versions of a character
//     (its AI racer, Super Sonic, Werehog, the Prime ones...) share one tile with "unlocked / total"
//     under it, and clicking it opens them in a card (SaveEditorVariants)
//   - the courses: a bar (Red Star Rings count, search, every ring) over a photo tile per course, by
//     Grand Prix; clicking one opens its card (SaveEditorCourse): its speed classes, and in each its
//     Grand Prix result, Time Trial record and Red Star Rings
//   - the honor titles: the bar and tags over a banner per title as the game draws it (its plate with
//     its name on it), a character's / festival's together; an unlocked one can be put on the card
//   - the stickers: the bar and tags (characters, designs, events, competitions, brands) over a
//     tile per sticker, a pack's (a festival, a brand campaign...) together
//   - the horns: the bar and tags (the base game's, the festivals') over a tile per horn
//   - the vehicles: the garage (the machines built, the one raced with first: parts, colour, horn,
//     aura, stickers; another can be raced with) over tags for the complete machines, the parts,
//     colours and auras, each as a category
// Clicking an item unlocks / adds it or locks it; every save backs up the account first. Split:
//   SaveEditorTab.cpp       layout, opening / saving / closing, header, account picker
//   SaveEditorOverview.cpp  the profile section: the whole save's progress, the tools, the profile's numbers and the tutorials
//   SaveEditorItems.cpp     a category: its bar, tiles, rows and the profile's values
//   SaveEditorVariants.cpp  the card with a character's versions
//   SaveEditorCourses.cpp   the courses section; SaveEditorCourse.cpp, a course's card
//   SaveEditorVehicles.cpp  the vehicles section and its garage
//   SaveEditorKit           the shared look (rings, chips, cards, bursts, colours)
class SaveEditorTab : public Tab {
public:
    explicit SaveEditorTab(MainWindow& window);
    ~SaveEditorTab() override;
    void Draw(UI::Ui& ui, const Rect& area) override;
    void OnSelected() override { loadedAt = -1.0; }
    void OnDeselected() override;

private:
    friend class SaveEditorVariants;
    friend class SaveEditorCourse;
    friend class SaveEditorBuild;
    friend class SaveEditorSwitch;

    // A category: the profile's numbers or one of the save's lists
    struct Category {
        int group;   // 0 Profile, 1 Characters & machines, 2 Customisation, 3 Progress
        bool numbers;
        Services::SaveFile::ListId list;
        UI::Material::Icon icon;
        const char* key;   // translation key of its name
    };
    static const std::vector<Category>& Categories();
    // The category that lists `list`; the characters'
    static int CategoryOf(Services::SaveFile::ListId list);
    static int CharactersCategory() { return CategoryOf(Services::SaveFile::ListId::Drivers); }
    // The open category (the first one while the summary shows, so a stray call never reads past the list)
    const Category& Current() const { return Categories()[static_cast<size_t>(std::max(category, 0))]; }
    // Unlocked / total per category and the id whose picture stands for it (read again after edits)
    struct Stat {
        int done = 0, total = 0;
        std::wstring cover;
    };
    const std::vector<Stat>& Stats();
    // The vehicles section's lists added up (complete machines, parts, colours, auras)
    static Stat VehicleStat(const std::vector<Stat>& all);
    // Every category added up: the whole save's progress
    Stat OverallStat();

    // Opens `accountId`'s app.sav (asking first when the open one has unsaved changes)
    void Open(const std::wstring& accountId, const std::wstring& path);
    // A Nintendo Switch save: the "app" file of a JKSV backup folder (same format and key as the PC's app.sav).
    // It opens as the account kSwitchAccount; saving backs it up to DataDir\SwitchBackups first, then writes
    // <folder>.zip next to the folder with all its files, for JKSV to restore.
    static constexpr const wchar_t* kSwitchAccount = L"switch";
    bool IsSwitch() const { return accountId == kSwitchAccount; }
    // The card saying which file it is and how to get it out of the console (SaveEditorSwitch)
    void OpenSwitchSave();
    // The file picker for the "app" file, then opens it
    void PickSwitchSave();
    void Reload();
    void CloseSave();
    // Backs up the account, then writes; asks first when the game is running or wrote the file
    // since it was opened (unless `overwrite`)
    void SaveToDisk(bool overwrite = false);
    void UnlockEverything();
    void Status(const std::wstring& text, bool error = false);
    // Runs `f` (an edit), showing a SaveError instead of letting it escape, then reads the rows again
    template <typename F>
    void Edit(F&& f);
    // Unlocks / adds or locks one item, playing its burst
    void ToggleItem(UI::Ui& ui, const Services::SaveFile::ItemRow& row);
    // Unlock all of the current category, the items bursting one after another
    void UnlockCategory(UI::Ui& ui);

    void Header(UI::Ui& ui, const Rect& r);
    // The menu: a card per section
    void Menu(UI::Ui& ui, const Rect& r);
    // In a section: back to the menu, and the sections
    void Sections(UI::Ui& ui, const Rect& r);
    void SwitchSection(UI::Ui& ui, int index);   // -1: the menu
    // The honor titles as banners, a tile per character / festival (SaveEditorItems.cpp)
    void Banners(UI::Ui& ui, const Rect& r);
    // A row of tags over a category (All and its kinds, each with unlocked / total); `current` is the
    // picked one (-1 all), `kindOf` a row's kind. Returns what is left under it.
    struct TagInfo {
        int kind;
        const char* key;
        UI::Material::Icon icon;
    };
    Rect TagRow(UI::Ui& ui, const Rect& r, int& current, const std::vector<TagInfo>& tags, const std::function<int(const Services::SaveFile::ItemRow&)>& kindOf);
    // The Shop tag's kind: it takes what the shop sells (ItemRow::shop) whatever its other kind
    static constexpr int kShopTag = 99;
    // The vehicles section: the garage over tags for the complete machines, the parts, colours and
    // auras (SaveEditorVehicles.cpp)
    void VehiclesView(UI::Ui& ui, const Rect& r);
    void BuildCard(UI::Ui& ui, UI::Id id, const Rect& r, const Services::SaveFile::MachineBuild& b);
    // The first card of the garage: builds a new machine of a type
    void NewBuildCard(UI::Ui& ui, const Rect& r);
    // The machines built in the game (read again after edits)
    const std::vector<Services::SaveFile::MachineBuild>& Garage();
    // The gadgets section: a player's presets beside the gadgets to put on the edited one, or the list of
    // every gadget (SaveEditorGadgets.cpp)
    void GadgetsView(UI::Ui& ui, const Rect& r);
    void GadgetPresetCard(UI::Ui& ui, UI::Id id, const Rect& r, const Services::SaveFile::GadgetPreset& preset, bool editing);
    void GadgetPicker(UI::Ui& ui, const Rect& r, const Services::SaveFile::GadgetPreset& preset);
    // The shown player's presets, and the gadgets that can be put on (read again after edits)
    const std::vector<Services::SaveFile::GadgetPreset>& GadgetPresets();
    const std::vector<Services::SaveFile::ItemRow>& GadgetRows();
    std::wstring GadgetName(const std::wstring& id);
    // The courses section: its bar and a tile per course (SaveEditorCourses.cpp)
    void CoursesView(UI::Ui& ui, const Rect& r);
    void CoursesBar(UI::Ui& ui, const Rect& r);
    // The courses of the save (read again after edits)
    const std::vector<Services::SaveFile::Course>& Courses();
    void Picker(UI::Ui& ui, const Rect& r);
    // The profile section: the whole save's progress with Max out the profile and Unlock EVERYTHING, the
    // profile's numbers (level, races, records) and the tutorials and hints
    void ProfileView(UI::Ui& ui, const Rect& r);
    // The music section: the jukebox's albums as their jackets, and the picked album's tracks (SaveEditorMusic.cpp)
    void MusicView(UI::Ui& ui, const Rect& r);
    // The albums' and tracks' rows (read again after edits)
    void MusicRows();
    // The statistics section: the race totals, the characters' and machines' races (SaveEditorStats.cpp)
    void StatsView(UI::Ui& ui, const Rect& r);
    // The Play Missions section: each period's missions with their count, Complete and Reset (SaveEditorStats.cpp)
    void MissionsView(UI::Ui& ui, const Rect& r);
    void CategoryView(UI::Ui& ui, const Rect& r);
    void Bar(UI::Ui& ui, const Rect& r);
    void Tiles(UI::Ui& ui, const Rect& r);
    void List(UI::Ui& ui, const Rect& r);
    // The profile's numbers as cards (only the currencies' when `currencies`)
    void Numbers(UI::Ui& ui, const Rect& r, bool currencies = false);
    // The currencies section: what they are over the cards of your tickets, Duel Medals, Red Star Rings...
    void CurrencyView(UI::Ui& ui, const Rect& r);
    // A number's card (the currencies' and the profile's): picture, name, amount, box and Max
    void NumberCard(UI::Ui& ui, const Services::SaveFile::NumberField& f, const Rect& c, bool big, double reveal, int order);
    void OpenCategory(UI::Ui& ui, int index);   // -1: back to the summary
    // The rows of the current category (cached until the save changes) and the ones shown
    const std::vector<Services::SaveFile::ItemRow>& Rows();
    const std::vector<size_t>& Visible();
    // The rows of the current category put together by character (one each outside Characters):
    // indices into Rows(), the one that names it first, and how many are unlocked
    struct Family {
        std::wstring id;
        std::vector<size_t> members;
        int done = 0;
    };
    const std::vector<Family>& Families();
    // Whether a family (a character's versions, a sticker pack) shows under `tag` of a tag row
    bool FamilyHasTag(const Family& f, int tag, const std::function<int(const Services::SaveFile::ItemRow&)>& kindOf);
    // A family's name: a sticker pack's own name, else its first row's
    std::wstring FamilyName(const Family& f);
    // The families with a visible row (indices into Families())
    const std::vector<size_t>& ShownFamilies();
    // How an item tile looks: the state drawn, the chip under the name, whether a click does something
    struct TileLook {
        Services::SaveFile::ItemState state = Services::SaveFile::ItemState::Missing;
        std::wstring chip;
        Color chipTone;
        bool can = true;
        float burst = -1.f;
    };
    TileLook FamilyLook(const std::vector<Services::SaveFile::ItemRow>& all, const Family& f, double now) const;
    // One tile (picture, state badge, name, chip); `row` gives the picture and the name
    UI::Interaction Tile(UI::Ui& ui, UI::Id id, const Rect& tile, const Services::SaveFile::ItemRow& row, const TileLook& look);
    // A title's tile: the card and its banner (the plate with its name on it) with the state on the
    // corner, gold when it is on the player's card (`on`); the line under it for one title (state,
    // name, Put on / On your card)
    UI::Interaction Banner(UI::Ui& ui, UI::Id id, const Rect& tile, float bannerH, const Services::SaveFile::ItemRow& row, const TileLook& look, bool on);
    void BannerLine(UI::Ui& ui, UI::Id id, const Rect& tile, float bannerH, const Services::SaveFile::ItemRow& row, bool on);
    // A character with versions as a line in List (no pictures)
    void FamilyLine(UI::Ui& ui, UI::Id id, const Rect& r, const std::vector<Services::SaveFile::ItemRow>& all, const Family& family, double now);

    double loadedAt = -1.0, screenAt = -10.0;
    std::optional<Services::SaveFile::AppSave> save;
    std::wstring accountId;
    int category = -1;   // -1: the summary
    std::wstring search;
    int filter = 0;      // 0 all, 1 unlocked, 2 locked, 3 missing
    bool busy = false;   // a save is being written
    SaveEditorKit::Bursts bursts;
    double animateUntil = 0.0;   // a wave of bursts still playing (Unlock all)

    int version = 0;   // bumped by every edit
    std::vector<Services::SaveFile::ItemRow> rows;
    std::string rowsKey;
    std::vector<size_t> visible;
    std::string visibleKey;
    std::vector<Family> families;
    std::string familiesKey;
    std::vector<size_t> shownFamilies;
    std::string shownKey;
    std::unique_ptr<SaveEditorVariants> variants;

    static constexpr int kProfileSection = 10, kMusicSection = 11, kStatsSection = 12, kMissionsSection = 13;
    int section = -1;   // -1 the menu, 0 characters, 1 courses, 2 honor titles, 3 stickers, 4 horns, 5 vehicles, 6 gadgets,
                        // 7 currencies, 8 challenges, 9 auras, 10 profile (shown first), 11 music, 12 statistics, 13 Play Missions
    int titleTag = -1;
    int stickerTag = -1;
    int colorTag = -1;     // the colour presets shown: -1 all, 0 the machines' own, 1 the festival teams'
    int shopTag = -1;      // characters, auras, gadgets: -1 all, kShopTag only the shop's
    int hornTag = -1;      // the horns shown: -1 all, 0 the base game's, 1 the festivals'   // the stickers shown: -1 all, else a StickerCatalog::Group  // the honor titles shown: -1 all, else a SaveCatalog::TitleGroup (0 racers, 1 festivals, 2 competitions)
    // The menu's covers: characters (the one on the card first) and honor titles (the one on the card first)
    std::vector<std::wstring> menuDrivers, menuTitles, menuStickers, menuHorns;
    std::vector<std::pair<Services::SaveFile::ListId, std::wstring>> menuVehicles;   // the garage's front parts, then complete machines
    std::vector<Services::SaveFile::MachineBuild> garage;
    int garageFor = -1;
    // The Konami code's extras, hidden until it is typed in the tab (up up down down left right left right
    // B A; again hides them), for this session only: custom cars (every type's parts in one machine) and
    // the World Championship's placing titles (SaveFile::SetKonamiExtras)
    bool konami = false;
    int konamiStep = 0;
    void KonamiCode(UI::Ui& ui);
    // Garage slots made with "Custom": their editor offers the parts of every type (a mixed build does too)
    std::set<std::wstring> mixedSlots;
    // The gadgets section: its view (0 presets, 1 the list), the player shown, the preset edited (-1: the
    // one raced with), the kind shown (-1 all)
    int gadgetView = 0, gadgetPlayer = 0, gadgetPreset = -1, gadgetTag = -1;
    bool gadgetSpecial = false;   // the picker fills the edited preset's special slot (not its plate)
    std::vector<Services::SaveFile::GadgetPreset> gadgetPresets;
    int gadgetPresetsFor = -1;
    std::vector<Services::SaveFile::ItemRow> gadgetRows;
    int gadgetRowsFor = -1;
    std::vector<std::wstring> menuGadgets;
    // The challenges' cover: the honor titles they give, the ones you have first (title ids)
    std::vector<std::wstring> menuChallenges;
    // The auras' cover: the auras you have, the exchange's (with their own picture) first
    std::vector<std::wstring> menuAuras;
    // The music's cover: the jackets of the albums you have first
    std::vector<std::wstring> menuAlbums;
    // The statistics' cover: the characters raced with most
    std::vector<std::wstring> menuStatDrivers;
    int menuFor = -1;
    // The music section: the album shown (-1: the first), and the rows by id
    int musicAlbum = -1;
    std::vector<Services::SaveFile::ItemRow> musicAlbumRows, musicTrackRows;
    int musicFor = -1;
    std::wstring courseSearch;
    std::vector<Services::SaveFile::Course> courses;
    int coursesFor = -1;
    std::unique_ptr<SaveEditorCourse> course;
    std::unique_ptr<SaveEditorBuild> build;
    std::unique_ptr<SaveEditorSwitch> switchHelp;
    std::vector<Stat> stats;
    int statsFor = -1;

    bool changedOnDisk = false;
    double diskCheckedAt = -10.0;
};

template <typename F>
void SaveEditorTab::Edit(F&& f) {
    if (!save || busy) return;
    try {
        f();
    } catch (const Services::SaveFile::SaveError& e) {
        Status(e.Message(), true);
    }
    ++version;
}

} // namespace SonicRacingSaveManager::Main
