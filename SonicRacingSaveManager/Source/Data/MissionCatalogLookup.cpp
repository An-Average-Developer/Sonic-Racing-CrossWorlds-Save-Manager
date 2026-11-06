#include "Data/MissionCatalog.h"
#include <unordered_map>

namespace SonicRacingSaveManager::Data::MissionCatalog {

namespace {
size_t LangIndex(const std::wstring& lang) {
    if (lang == L"es") return 1;
    if (lang == L"fr") return 2;
    if (lang == L"it") return 3;
    return 0;
}

void ReplaceAll(std::wstring& s, const std::wstring& from, const std::wstring& to) {
    for (size_t at = s.find(from); at != std::wstring::npos; at = s.find(from, at + to.size())) s.replace(at, from.size(), to);
}
} // namespace

const MissionType* Find(int id) {
    static const auto index = [] {
        std::unordered_map<int, const MissionType*> m;
        for (const auto& t : All()) m[t.id] = &t;
        return m;
    }();
    const auto it = index.find(id);
    return it == index.end() ? nullptr : it->second;
}

std::wstring Text(int id, const int64_t params[3], const std::wstring& lang) {
    const MissionType* t = Find(id);
    if (!t) return L"#" + std::to_wstring(id);
    const size_t li = LangIndex(lang);
    std::wstring s = t->text[li] && *t->text[li] ? t->text[li] : t->text[0];
    if (t->value >= 0) {
        const int64_t v = params[t->value];
        ReplaceAll(s, L"{Level}", std::to_wstring(v));
        ReplaceAll(s, L"{Progress}", std::to_wstring(v));
        ReplaceAll(s, L"{Rank}", v >= 1 && v <= 12 ? std::wstring(kRanks[v - 1][li]) : std::to_wstring(v));
    }
    return s;
}

int64_t Goal(int id, const int64_t params[3]) {
    const MissionType* t = Find(id);
    const int64_t g = t ? params[t->goal] : 1;
    return g > 0 ? g : 1;
}

} // namespace SonicRacingSaveManager::Data::MissionCatalog
