#pragma once
#include <string>
#include <vector>
#include "Core/Text.h"

namespace SonicRacingSaveManager::Text {
inline std::wstring ToWide(const std::wstring& s) { return s; }
inline std::wstring ToWide(const wchar_t* s) { return s; }
inline std::wstring ToWide(int v) { return std::to_wstring(v); }
inline std::wstring ToWide(long v) { return std::to_wstring(v); }
inline std::wstring ToWide(long long v) { return std::to_wstring(v); }
inline std::wstring ToWide(unsigned v) { return std::to_wstring(v); }
inline std::wstring ToWide(unsigned long v) { return std::to_wstring(v); }
inline std::wstring ToWide(unsigned long long v) { return std::to_wstring(v); }
} // namespace SonicRacingSaveManager::Text

// Translations from Translations\<lang>.txt ("key = value" lines, "\n" escapes, # comments).
namespace SonicRacingSaveManager::Loc {

void Load(const std::wstring& lang);   // also remembers the choice in language.cfg
std::wstring LoadSaved();              // saved language (default "en")
const std::wstring& CurrentLang();
// Increments every time the language changes (screens rebuild cached strings)
unsigned Revision();

// The translated string, or the key itself when missing.
const std::wstring& Get(const std::string& key);
std::wstring Get(const std::string& key, const std::vector<std::wstring>& args);
template <typename... T>
std::wstring Fmt(const std::string& key, const T&... args) {
    return Get(key, std::vector<std::wstring>{ Text::ToWide(args)... });
}

} // namespace SonicRacingSaveManager::Loc
