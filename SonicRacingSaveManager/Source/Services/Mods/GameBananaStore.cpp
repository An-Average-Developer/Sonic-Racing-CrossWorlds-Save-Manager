#include "Services/Mods/GameBananaStore.h"
#include <windows.h>
#include <algorithm>
#include <cstdio>
#include <cwctype>
#include "App/Config.h"
#include "App/Paths.h"
#include "Core/Files.h"
#include "Core/Http.h"
#include "Core/Json.h"
#include "Core/Process.h"
#include "Core/System.h"
#include "Core/Text.h"
#include "Core/Zip.h"
#include "Localization/Localization.h"
#include "Services/App/Log.h"
#include "Services/Mods/GameBananaApi.h"
#include "Services/Mods/ModManager.h"

namespace SonicRacingSaveManager::Services::Mods::Store {

namespace {

namespace C = Config::GameBanana;

constexpr const wchar_t* kGameId = L"21640";   // Sonic Racing: CrossWorlds
constexpr int kListPerPage = 20, kSearchPerPage = 15;   // the search always answers 15 per page

Json::Value GetJson(const std::wstring& url) {
    const Http::Response r = Http::Get(url, GameBanana::Headers(), C::TimeoutMs);
    if (!r.Ok()) {
        if (r.status == 0) throw Error(r.error.empty() ? Loc::Get("mods_error_network") : r.error);
        throw Error(Loc::Fmt("mods_error_status", r.status));
    }
    bool ok = false;
    Json::Value json = Json::Parse(std::string_view(r.body), &ok);
    if (!ok) throw Error(Loc::Get("mods_error_bad_response"));
    return json;
}

StoreMod ParseMod(const Json::Value& r) {
    StoreMod m;
    m.id = r["_idRow"].Int();
    m.name = r["_sName"].WStr();
    m.author = r["_aSubmitter"]["_sName"].WStr();
    m.version = r["_sVersion"].WStr();
    m.category = r["_aRootCategory"]["_sName"].WStr();
    m.subCategory = r["_aSubCategory"]["_sName"].WStr();
    m.profileUrl = r["_sProfileUrl"].WStr();
    if (m.profileUrl.empty()) m.profileUrl = L"https://gamebanana.com/mods/" + std::to_wstring(m.id);
    m.likes = r["_nLikeCount"].Int();
    m.views = r["_nViewCount"].Int();
    m.dateAdded = r["_tsDateAdded"].Int();
    m.dateUpdated = r["_tsDateModified"].Int();
    m.hasFiles = r["_bHasFiles"].Bool(true);
    m.obsolete = r["_bIsObsolete"].Bool();
    m.rated = r["_bHasContentRatings"].Bool();
    // The first picture, in its 530 px version when there is one
    for (const auto& image : r["_aPreviewMedia"]["_aImages"].Elements()) {
        std::wstring file = image["_sFile530"].WStr();
        if (file.empty()) file = image["_sFile220"].WStr();
        if (file.empty()) file = image["_sFile"].WStr();
        if (!file.empty()) {
            m.imageUrl = image["_sBaseUrl"].WStr() + L"/" + file;
            break;
        }
    }
    return m;
}

const wchar_t* SortName(Sort sort) {
    switch (sort) {
        case Sort::Newest: return L"Generic_Newest";
        case Sort::Updated: return L"Generic_LatestUpdated";
        case Sort::MostLiked: return L"Generic_MostLiked";
        default: return L"Generic_MostDownloaded";
    }
}

// "Joe Swanson over Zavok" → "joeswansonoverzavok"
std::wstring Normalize(const std::wstring& s) {
    std::wstring out;
    for (wchar_t c : s)
        if (std::iswalnum(c)) out += static_cast<wchar_t>(std::towlower(c));
    return out;
}

// A folder name Windows accepts, from the mod's name
std::wstring FolderName(const StoreMod& mod) {
    std::wstring name;
    for (wchar_t c : mod.name)
        if (c >= 32 && !wcschr(L"<>:\"/\\|?*", c)) name += c;
    name = Text::Trim(name);
    while (!name.empty() && (name.back() == L'.' || name.back() == L' ')) name.pop_back();
    if (name.size() > 60) name = Text::Trim(name.substr(0, 60));
    return name.empty() ? L"Mod " + std::to_wstring(mod.id) : name;
}

// Windows' bsdtar (libarchive) extracts .7z and .rar, and zips the built-in reader can't
void ExtractWithTar(const std::wstring& archive, const std::wstring& directory) {
    wchar_t system[MAX_PATH]{};
    GetSystemDirectoryW(system, MAX_PATH);
    const std::wstring tar = Files::Combine(system, L"tar.exe");
    if (!Files::Exists(tar)) throw Error(Loc::Fmt("mods_store_error_format", Files::FileName(archive)));
    std::string output;
    unsigned long code = 1;
    const bool ran = Process::RunAndCapture(tar, L"-xf " + Process::QuoteArgument(archive) + L" -C " + Process::QuoteArgument(directory), output, &code,
                                            10 * 60 * 1000);
    if (!ran || code != 0) {
        Log::Warning(L"Mod Store: tar could not extract " + Files::FileName(archive) + L": " + Text::Widen(Text::Trim(output)));
        throw Error(Loc::Fmt("mods_error_extract", Files::FileName(archive)));
    }
}

bool HasPaks(const std::wstring& dir) { return !Files::ListFiles(dir, L"*.pak", true).empty() || !Files::ListFiles(dir, L"*.utoc", true).empty(); }

// The folder holding the mod: a "~mods" folder when the archive copies the game's layout, then down
// through folders that only hold one folder
std::wstring FindModRoot(const std::wstring& extracted) {
    std::wstring root = extracted;
    std::vector<std::wstring> pending{ extracted };
    while (!pending.empty()) {
        const std::wstring dir = pending.back();
        pending.pop_back();
        if (Text::Equals(Files::FileName(dir), L"~mods") && HasPaks(dir)) {
            root = dir;
            break;
        }
        for (auto& sub : Files::ListDirectories(dir)) pending.push_back(std::move(sub));
    }
    for (;;) {
        const auto dirs = Files::ListDirectories(root);
        if (!Files::ListFiles(root).empty() || dirs.size() != 1) break;
        root = dirs.front();
    }
    return root;
}

// The description's HTML as plain text: line breaks for <br>, paragraphs, headings and list items
// ("• " in front), the other tags dropped, the usual entities decoded, at most one empty line
std::wstring HtmlToText(const std::wstring& html) {
    std::wstring out;
    const auto newline = [&] {
        while (!out.empty() && out.back() == L' ') out.pop_back();
        out += L'\n';
    };
    for (size_t i = 0; i < html.size();) {
        const wchar_t c = html[i];
        if (c == L'<') {
            const size_t end = html.find(L'>', i);
            if (end == std::wstring::npos) break;
            std::wstring tag = Text::Lower(html.substr(i + 1, end - i - 1));
            const bool closing = !tag.empty() && tag[0] == L'/';
            if (closing) tag.erase(0, 1);
            const std::wstring name = tag.substr(0, tag.find_first_of(L" /\t\n"));
            if (name == L"br") newline();
            else if (name == L"li" && !closing) {
                newline();
                out += L"\x2022 ";
            } else if (name == L"p" || name == L"div" || name == L"ul" || name == L"ol" || name == L"blockquote" || name == L"hr" ||
                       (name.size() == 2 && name[0] == L'h' && name[1] >= L'1' && name[1] <= L'6')) {
                newline();
                if (closing && name != L"li") newline();
            }
            i = end + 1;
            continue;
        }
        if (c == L'&') {
            const size_t end = html.find(L';', i);
            if (end != std::wstring::npos && end - i <= 8) {
                const std::wstring entity = html.substr(i + 1, end - i - 1);
                wchar_t decoded = 0;
                if (entity == L"amp") decoded = L'&';
                else if (entity == L"lt") decoded = L'<';
                else if (entity == L"gt") decoded = L'>';
                else if (entity == L"quot") decoded = L'"';
                else if (entity == L"apos" || entity == L"#39") decoded = L'\'';
                else if (entity == L"nbsp") decoded = L' ';
                else if (entity.size() > 1 && entity[0] == L'#') {
                    const long v = entity[1] == L'x' ? std::wcstol(entity.c_str() + 2, nullptr, 16) : std::wcstol(entity.c_str() + 1, nullptr, 10);
                    if (v > 0 && v < 0xFFFF) decoded = static_cast<wchar_t>(v);
                }
                if (decoded) {
                    out += decoded;
                    i = end + 1;
                    continue;
                }
            }
        }
        out += (c == L'\r' || c == L'\n' || c == L'\t' || c == L'\x00A0') ? L' ' : c;
        ++i;
    }
    // Spaces at line starts, three or more line breaks, and the ends
    std::wstring clean;
    int breaks = 0;
    bool lineStart = true;
    for (wchar_t c : out) {
        if (c == L'\n') {
            while (!clean.empty() && clean.back() == L' ') clean.pop_back();
            if (++breaks <= 2) clean += c;
            lineStart = true;
            continue;
        }
        if (lineStart && c == L' ') continue;
        breaks = 0;
        lineStart = false;
        clean += c;
    }
    return Text::Trim(clean);
}

} // namespace

ModDetails FetchDetails(long long id) {
    const Json::Value json = GetJson(std::wstring(C::ApiUrl) + L"Mod/" + std::to_wstring(id) + L"/ProfilePage");
    ModDetails d;
    d.summary = Text::Trim(json["_sDescription"].WStr());
    d.text = HtmlToText(json["_sText"].WStr());
    d.downloads = json["_nDownloadCount"].Int();
    d.likes = json["_nLikeCount"].Int();
    d.views = json["_nViewCount"].Int();
    d.dateAdded = json["_tsDateAdded"].Int();
    d.dateUpdated = json["_tsDateModified"].Int();
    d.version = json["_sVersion"].WStr();
    d.license = Text::Trim(json["_sLicense"].WStr());
    for (const auto& image : json["_aPreviewMedia"]["_aImages"].Elements()) {
        const std::wstring base = image["_sBaseUrl"].WStr() + L"/";
        const std::wstring full = image["_sFile"].WStr(), large = image["_sFile800"].WStr(), medium = image["_sFile530"].WStr(), thumb = image["_sFile220"].WStr();
        if (full.empty()) continue;
        ModDetails::Picture p;
        p.url = base + (!large.empty() ? large : !medium.empty() ? medium : full);
        p.thumb = base + (!thumb.empty() ? thumb : !medium.empty() ? medium : full);
        p.caption = Text::Trim(image["_sCaption"].WStr());
        d.pictures.push_back(std::move(p));
    }
    for (const auto& f : json["_aFiles"].Elements())
        d.files.push_back({ f["_sFile"].WStr(), Text::Trim(f["_sDescription"].WStr()), f["_nFilesize"].Int(), f["_nDownloadCount"].Int() });
    // The profile has no root / sub category: its one category stands for both
    d.mod = ParseMod(json);
    if (d.mod.category.empty()) d.mod.category = json["_aCategory"]["_sName"].WStr();
    d.mod.hasFiles = !d.files.empty();
    for (const auto& group : json["_aCredits"].Elements())
        for (const auto& author : group["_aAuthors"].Elements())
            d.credits.push_back({ group["_sGroupName"].WStr(), author["_sName"].WStr(), Text::Trim(author["_sRole"].WStr()) });
    return d;
}

Page FetchPage(int page, Sort sort, long long categoryId, const std::wstring& search) {
    std::wstring url = C::ApiUrl;
    Page result;
    const std::wstring query = Text::Trim(search);
    if (!query.empty()) {
        url += L"Util/Search/Results?_sModelName=Mod&_sOrder=best_match&_idGameRow=" + std::wstring(kGameId) + L"&_sSearchString=" + Text::UrlEncode(query) +
               L"&_nPage=" + std::to_wstring(page);
        result.perPage = kSearchPerPage;
    } else {
        url += L"Mod/Index?_nPage=" + std::to_wstring(page) + L"&_nPerpage=" + std::to_wstring(kListPerPage) + L"&_aFilters%5BGeneric_Game%5D=" + kGameId +
               L"&_sSort=" + SortName(sort);
        if (categoryId > 0) url += L"&_aFilters%5BGeneric_Category%5D=" + std::to_wstring(categoryId);
        result.perPage = kListPerPage;
    }
    const Json::Value json = GetJson(url);
    result.total = json["_aMetadata"]["_nRecordCount"].Int();
    for (const auto& r : json["_aRecords"].Elements())
        if (r["_sModelName"].WStr() == L"Mod") result.mods.push_back(ParseMod(r));
    return result;
}

std::vector<Category> FetchCategories() {
    const Json::Value json =
        GetJson(std::wstring(C::ApiUrl) + L"Mod/Categories?_idGameRow=" + kGameId + L"&_sSort=a_to_z&_bShowEmpty=true");
    std::vector<Category> categories;
    for (const auto& c : json.Elements()) {
        if (c["_idParentCategoryRow"].Int() > 0) continue;
        categories.push_back({ c["_idRow"].Int(), c["_sName"].WStr(), c["_nItemCount"].Int() });
    }
    std::stable_sort(categories.begin(), categories.end(), [](const auto& a, const auto& b) { return a.count > b.count; });
    return categories;
}

std::string FetchImage(const std::wstring& url) {
    if (url.empty()) return {};
    // FNV-1a of the URL names the cached file
    unsigned long long hash = 1469598103934665603ull;
    for (wchar_t c : url) hash = (hash ^ static_cast<unsigned long long>(c)) * 1099511628211ull;
    wchar_t name[32];
    swprintf_s(name, L"%016llx.img", hash);
    const std::wstring dir = Files::Combine(Paths::DataDir(), L"StoreCache"), file = Files::Combine(dir, name);
    if (auto cached = Files::ReadBytes(file); cached && !cached->empty()) return std::move(*cached);
    const Http::Response r = Http::Get(url, GameBanana::Headers(), 15000);
    if (!r.Ok() || r.body.empty()) return {};
    if (Files::CreateDirectories(dir)) Files::WriteBytes(file, r.body);
    return r.body;
}

int FindInstalled(const StoreMod& mod, const std::vector<Models::ModInfo>& installed) {
    // Its page: ".../mods/<id>" and no more digits after it
    const std::wstring id = L"/mods/" + std::to_wstring(mod.id);
    for (size_t i = 0; i < installed.size(); ++i) {
        const std::wstring& page = installed[i].modPageUrl;
        const size_t at = page.find(id);
        if (at != std::wstring::npos && (at + id.size() == page.size() || !std::iswdigit(page[at + id.size()]))) return static_cast<int>(i);
    }
    // Its name, as the folder or as info.json's "name"
    const std::wstring name = Normalize(mod.name);
    if (name.size() < 3) return -1;
    for (size_t i = 0; i < installed.size(); ++i)
        if (Normalize(installed[i].name) == name || Normalize(installed[i].infoName) == name) return static_cast<int>(i);
    return -1;
}

bool IsOutdated(const StoreMod& mod, const Models::ModInfo& installed) {
    return !installed.version.empty() && !mod.version.empty() && GameBanana::IsNewerVersion(installed.version, mod.version);
}

namespace {

// The temporary folder of one installation: the downloads and the extraction go away whatever happens
struct WorkDir {
    const std::wstring path = Files::Combine(Files::TempDirectory(), L"srcw_store_" + System::NewGuid());
    WorkDir() { Files::CreateDirectories(path); }
    ~WorkDir() {
        if (Files::DirectoryExists(path) && !Files::DeleteDirectory(path)) Log::Warning(L"Mod Store: could not delete " + path);
    }
};

// Downloads `files` into `work` and extracts them; returns the folder holding the mod (inside
// `work`). 2% → 87%.
std::wstring DownloadAndExtract(const std::vector<Models::GameBananaFile>& files, const std::wstring& work, const Progress& report) {
    const std::wstring extracted = Files::Combine(work, L"x");
    Files::CreateDirectories(extracted);
    const int count = static_cast<int>(files.size());
    for (int i = 0; i < count; ++i) {
        const auto& file = files[static_cast<size_t>(i)];
        if (!file.downloadUrl || file.downloadUrl->empty()) continue;
        const std::wstring name = file.fileName.value_or(L"mod_" + std::to_wstring(i + 1) + L".zip");
        const double offset = 2.0 + 70.0 * i / count, range = 70.0 / count;
        report(offset, Loc::Fmt("mod_update_downloading_file", i + 1, count, name));
        const std::wstring archive = Files::Combine(work, name);
        std::wstring error;
        const bool ok = Http::Download(
            *file.downloadUrl, archive,
            [&](uint64_t received, uint64_t total) {
                if (total > 0) report(offset + range * static_cast<double>(received) / static_cast<double>(total), Loc::Fmt("mod_update_downloading_file", i + 1, count, name));
                return true;
            },
            &error, GameBanana::Headers());
        if (!ok) throw Error(error.empty() ? Loc::Get("mods_error_network") : error);

        report(72.0 + 15.0 * i / count, Loc::Get("mod_update_extracting"));
        const std::wstring ext = Text::Lower(Files::Extension(archive));
        if (ext == L".zip") {
            if (!Zip::ExtractToDirectory(archive, extracted, true)) ExtractWithTar(archive, extracted);
        } else if (ext == L".7z" || ext == L".rar" || ext == L".tar" || ext == L".gz") {
            ExtractWithTar(archive, extracted);
        } else if (ext == L".pak" || ext == L".utoc" || ext == L".ucas" || ext == L".sig") {
            if (!Files::Copy(archive, Files::Combine(extracted, Files::FileName(archive)), true)) throw Error(Loc::Fmt("mods_error_copy", Files::FileName(archive)));
        } else {
            throw Error(Loc::Fmt("mods_store_error_format", Files::FileName(archive)));
        }
    }

    const std::wstring root = FindModRoot(extracted);
    // Not a .pak mod (a UE4SS script, a tool...): it goes elsewhere in the game, not in ~mods
    if (!HasPaks(root)) throw Error(Loc::Get("mods_store_error_no_pak"));
    return root;
}

// info.json: name, version, author, mod_page (what the Mod Manager and the store read back)
void WriteInfoJson(const std::wstring& folder, const StoreMod& mod) {
    Json::Value info = Json::Value::MakeObject();
    info.Set("name", mod.name);
    info.Set("version", mod.version.empty() ? std::wstring(L"1.0") : mod.version);
    info.Set("author", mod.author);
    info.Set("mod_type", "pak");
    info.Set("mod_page", mod.profileUrl);
    if (!Files::WriteBytes(Files::Combine(folder, L"info.json"), info.Serialize(true))) Log::Warning(L"Mod Store: could not write info.json in " + folder);
}

} // namespace

std::wstring Install(const StoreMod& mod, const std::vector<Models::GameBananaFile>& files, const std::wstring& modsDirectory,
                     const std::vector<Models::ModInfo>& installed, const Progress& progress) {
    const Progress report = [&](double percentage, const std::wstring& status) {
        if (progress) progress(percentage, status);
    };
    if (files.empty()) throw Error(Loc::Get("mods_error_no_files_selected"));
    if (!Files::DirectoryExists(modsDirectory)) throw Error(Loc::Fmt("mods_error_directory_not_found", modsDirectory));
    const WorkDir work;
    const std::wstring root = DownloadAndExtract(files, work.path, report);

    report(90, Loc::Get("mod_update_installing"));
    std::wstring prefix;
    if (ModManager::IsPriorityModeEnabled(installed)) {
        int highest = -1;
        for (const auto& m : installed) highest = std::max(highest, m.priority);
        wchar_t number[16];
        swprintf_s(number, L"%02d - ", highest + 2);
        prefix = number;
    }
    const std::wstring base = prefix + FolderName(mod);
    std::wstring target = Files::Combine(modsDirectory, base);
    for (int n = 2; Files::DirectoryExists(target) || Files::Exists(target); ++n) target = Files::Combine(modsDirectory, base + L" (" + std::to_wstring(n) + L")");
    if (!Files::CopyDirectory(root, target, true)) {
        Files::DeleteDirectory(target);
        throw Error(Loc::Fmt("mods_error_copy", Files::FileName(target)));
    }

    report(97, Loc::Get("mod_update_info"));
    WriteInfoJson(target, mod);
    Log::Info(L"Mod Store: installed " + mod.name + L" (" + std::to_wstring(mod.id) + L") in " + target);
    report(100, Loc::Get("mod_update_completed"));
    return target;
}

void Reinstall(const StoreMod& mod, const std::vector<Models::GameBananaFile>& files, const Models::ModInfo& installed, const Progress& progress) {
    const Progress report = [&](double percentage, const std::wstring& status) {
        if (progress) progress(percentage, status);
    };
    if (files.empty()) throw Error(Loc::Get("mods_error_no_files_selected"));
    const std::wstring folder = installed.folderPath;
    if (!Files::DirectoryExists(folder)) throw Error(Loc::Fmt("mods_error_folder_not_found", folder));
    const WorkDir work;
    const std::wstring root = DownloadAndExtract(files, work.path, report);

    // The new files get the info.json and the mod's state before they go in
    report(88, Loc::Get("mod_update_info"));
    WriteInfoJson(root, mod);
    if (!installed.isEnabled) {
        Models::ModInfo staged;
        staged.folderPath = root;
        staged.isEnabled = true;
        ModManager(L"").DisableMod(staged);
    }

    // The old folder steps aside (a rename: it fails as a whole when a file is in use), the new one
    // takes its name, then the old one is deleted; when the new one can't go in, the old one comes back
    report(92, Loc::Get("mod_update_removing"));
    const std::wstring old = folder + L".old-" + System::NewGuid().substr(0, 8);
    if (!MoveFileExW(folder.c_str(), old.c_str(), 0)) throw Error(Loc::Fmt("mods_error_move", Files::FileName(folder)));
    report(95, Loc::Get("mod_update_installing"));
    if (!Files::MoveDirectory(root, folder)) {
        Files::DeleteDirectory(folder);
        MoveFileExW(old.c_str(), folder.c_str(), 0);
        throw Error(Loc::Fmt("mods_error_copy", Files::FileName(folder)));
    }
    if (!Files::DeleteDirectory(old)) Log::Warning(L"Mod Store: could not delete " + old);
    Log::Info(L"Mod Store: reinstalled " + mod.name + L" (" + std::to_wstring(mod.id) + L") in " + folder);
    report(100, Loc::Get("mod_update_completed"));
}

long long ModIdFromUrl(const std::wstring& url) {
    const size_t at = url.find(L"/mods/");
    if (at == std::wstring::npos) return 0;
    long long id = 0;
    for (size_t i = at + 6; i < url.size() && std::iswdigit(url[i]) && id < 1'000'000'000'000ll; ++i) id = id * 10 + (url[i] - L'0');
    return id;
}

} // namespace SonicRacingSaveManager::Services::Mods::Store
