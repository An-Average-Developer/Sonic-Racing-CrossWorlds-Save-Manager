#pragma once
#include <functional>
#include <string>
#include <vector>
#include "Models/ModInfo.h"

// The Mod Store: Sonic Racing: CrossWorlds' mods on GameBanana (game 21640) through apiv11 — the
// pages of the list, the search, the root categories, the preview pictures (cached on disk) and the
// installation of a mod into the mods folder. Blocking: call from worker threads. Failed requests
// throw Mods::Error.
namespace SonicRacingSaveManager::Services::Mods::Store {

struct StoreMod {
    long long id = 0;
    std::wstring name, author, version;
    std::wstring category, subCategory;   // "Skins", "Espio"
    std::wstring profileUrl;              // https://gamebanana.com/mods/<id>
    std::wstring imageUrl;                // the first screenshot, 530 px wide ("" = none)
    long long likes = 0, views = 0;
    long long dateAdded = 0, dateUpdated = 0;   // Unix seconds
    bool hasFiles = true, obsolete = false;
    bool rated = false;                   // content ratings (mature): the picture stays hidden
};

enum class Sort { Popular, Newest, Updated, MostLiked };

struct Page {
    std::vector<StoreMod> mods;
    long long total = 0;   // records matching
    int perPage = 0;
};

struct Category {
    long long id = 0;
    std::wstring name;
    long long count = 0;
};

// Everything the info card shows about one mod (its profile page)
struct ModDetails {
    struct Picture {
        std::wstring url;       // 800 px wide when GameBanana has it
        std::wstring thumb;     // 220 px wide
        std::wstring caption;
    };
    struct File {
        std::wstring name, description;
        long long size = 0, downloads = 0;
    };
    struct Credit {
        std::wstring group, name, role;
    };
    std::wstring summary;       // the one-line description
    std::wstring text;          // the description, from its HTML to plain text
    std::vector<Picture> pictures;
    std::vector<File> files;
    std::vector<Credit> credits;
    long long downloads = 0, likes = 0, views = 0;
    long long dateAdded = 0, dateUpdated = 0;
    std::wstring version, license;
    StoreMod mod;               // the mod as the list shows it (the card opened from the Mod Manager has only its id)
};
ModDetails FetchDetails(long long id);

// One page (1-based). `search` not empty: GameBanana's search (best match; sort and category don't
// apply there), else the list in `sort` order, in `categoryId` (0 = all)
Page FetchPage(int page, Sort sort, long long categoryId, const std::wstring& search);
// The game's root categories (Skins, HUD/GUI, Maps...), biggest first
std::vector<Category> FetchCategories();
// The picture's bytes, from %LOCALAPPDATA%\SonicRacingSaveManager\StoreCache when it's there
std::string FetchImage(const std::wstring& url);

// The installed mod `mod` is (by its GameBanana page, else by name), or -1
int FindInstalled(const StoreMod& mod, const std::vector<Models::ModInfo>& installed);
// That installed mod is older than the store's version
bool IsOutdated(const StoreMod& mod, const Models::ModInfo& installed);

// Downloads `files`, extracts them (.zip, .7z, .rar), finds the folder with the .pak files and
// copies it into a new folder of `modsDirectory` named after the mod (with the next priority
// number when the folder uses them), with an info.json (name, version, author, mod_page). Returns
// the new folder.
using Progress = std::function<void(double percentage, const std::wstring& status)>;
std::wstring Install(const StoreMod& mod, const std::vector<Models::GameBananaFile>& files, const std::wstring& modsDirectory,
                     const std::vector<Models::ModInfo>& installed, const Progress& progress);
// Downloads `files` the same way and replaces everything in the installed mod's folder with them
// (folder name, priority and enabled / disabled state kept). The old folder is only deleted once
// the new files are in its place; when they can't be, it's put back.
void Reinstall(const StoreMod& mod, const std::vector<Models::GameBananaFile>& files, const Models::ModInfo& installed, const Progress& progress);
// The GameBanana id in a mod page URL (".../mods/628335"), or 0
long long ModIdFromUrl(const std::wstring& url);

} // namespace SonicRacingSaveManager::Services::Mods::Store
