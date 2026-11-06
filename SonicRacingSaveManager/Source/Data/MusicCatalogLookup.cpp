#include "Data/MusicCatalog.h"
#include <unordered_map>

namespace SonicRacingSaveManager::Data::MusicCatalog {

namespace {
size_t LangIndex(const std::wstring& lang) {
    if (lang == L"es") return 1;
    if (lang == L"fr") return 2;
    if (lang == L"it") return 3;
    return 0;
}
} // namespace

const Album* FindAlbum(int id) {
    static const auto index = [] {
        std::unordered_map<int, const Album*> m;
        for (const auto& a : Albums()) m[a.id] = &a;
        return m;
    }();
    const auto it = index.find(id);
    return it == index.end() ? nullptr : it->second;
}

const Track* FindTrack(int id) {
    static const auto index = [] {
        std::unordered_map<int, const Track*> m;
        for (const auto& t : Tracks()) m[t.id] = &t;
        return m;
    }();
    const auto it = index.find(id);
    return it == index.end() ? nullptr : it->second;
}

const wchar_t* AlbumName(const Album& a, const std::wstring& lang) {
    const wchar_t* n = a.name[LangIndex(lang)];
    return n && *n ? n : a.name[0];
}

const wchar_t* TrackName(const Track& t, const std::wstring& lang) {
    const wchar_t* n = t.name[LangIndex(lang)];
    return n && *n ? n : t.name[0];
}

std::vector<int> TracksOf(int album) {
    std::vector<int> out;
    for (const auto& t : Tracks())
        if (t.album == album) out.push_back(t.id);
    return out;
}

} // namespace SonicRacingSaveManager::Data::MusicCatalog
