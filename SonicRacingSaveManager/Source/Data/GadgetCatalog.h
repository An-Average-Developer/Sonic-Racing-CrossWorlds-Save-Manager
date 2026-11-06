#pragma once
#include <string>
#include <string_view>
#include <vector>

// The gadgets as the game defines them (DT_GadgetData): how many of the plate's slots each takes (1..4)
// and what it does, in the app's languages. Generated from the game's files (GadgetCatalog.cpp).
namespace SonicRacingSaveManager::Data::GadgetCatalog {

struct Gadget {
    const wchar_t* id;            // EGadgetId value name: "Run2_QuickAirtrick"
    int slots;                    // slots it takes on the plate
    const wchar_t* about[4];      // what it does: en, es, fr, it
};

const std::vector<Gadget>& All();

// Gadget ids are FNames: the game spells some two ways ("Various36_AirtrickActionKit" in its tables,
// "..AirTrickActionKit" in its enum), so they match ignoring case
inline bool SameId(std::wstring_view a, std::wstring_view b) {
    if (a.size() != b.size()) return false;
    for (size_t k = 0; k < a.size(); ++k) {
        const wchar_t x = a[k] >= L'A' && a[k] <= L'Z' ? static_cast<wchar_t>(a[k] + 32) : a[k];
        const wchar_t y = b[k] >= L'A' && b[k] <= L'Z' ? static_cast<wchar_t>(b[k] + 32) : b[k];
        if (x != y) return false;
    }
    return true;
}

// Season 2's special slot (the plate's 7th): the gadgets it takes, each with what it does there
// (DB gadget_guide_<kind><n>_sp: stronger than on the plate)
struct SpecialGadget {
    const wchar_t* id;
    const wchar_t* about[4];      // en, es, fr, it
};
const std::vector<SpecialGadget>& Specials();
inline bool IsSpecial(std::wstring_view id) {
    for (const auto& g : Specials())
        if (SameId(id, g.id)) return true;
    return false;
}
inline std::wstring SpecialAbout(std::wstring_view id, std::wstring_view lang) {
    static constexpr std::wstring_view kLangs[] = { L"en", L"es", L"fr", L"it" };
    size_t l = 0;
    for (size_t k = 0; k < 4; ++k)
        if (lang == kLangs[k]) l = k;
    for (const auto& g : Specials())
        if (SameId(id, g.id)) return g.about[l];
    return {};
}

// The slots a gadget takes (1 when the game's table doesn't have it)
inline int Slots(std::wstring_view id) {
    for (const auto& g : All())
        if (SameId(id, g.id)) return g.slots;
    return 1;
}

// What it does in `lang` ("en", "es", "fr", "it"); empty when unknown
inline std::wstring About(std::wstring_view id, std::wstring_view lang) {
    static constexpr std::wstring_view kLangs[] = { L"en", L"es", L"fr", L"it" };
    size_t l = 0;
    for (size_t k = 0; k < 4; ++k)
        if (lang == kLangs[k]) l = k;
    for (const auto& g : All())
        if (SameId(id, g.id)) return g.about[l];
    return {};
}

} // namespace SonicRacingSaveManager::Data::GadgetCatalog
