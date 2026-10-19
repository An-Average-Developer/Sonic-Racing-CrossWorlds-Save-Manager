#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// String helpers shared by the whole app. Everything user-facing is UTF-16 (std::wstring);
// files, JSON and HTTP bodies are UTF-8 (std::string).
namespace SonicRacingSaveManager::Text {

std::wstring Widen(std::string_view utf8);
std::string Narrow(std::wstring_view wide);

std::wstring Trim(std::wstring_view s);
std::string Trim(std::string_view s);
std::wstring TrimStart(std::wstring_view s);
std::wstring Lower(std::wstring_view s);
std::wstring Upper(std::wstring_view s);
std::string Lower(std::string_view s);

bool StartsWith(std::wstring_view s, std::wstring_view prefix, bool ignoreCase = false);
bool EndsWith(std::wstring_view s, std::wstring_view suffix, bool ignoreCase = false);
bool Contains(std::wstring_view s, std::wstring_view part, bool ignoreCase = false);
bool Equals(std::wstring_view a, std::wstring_view b, bool ignoreCase = true);
bool StartsWith(std::string_view s, std::string_view prefix);

std::vector<std::wstring> Split(std::wstring_view s, wchar_t separator, bool skipEmpty = false);
std::vector<std::string> Split(std::string_view s, char separator, bool skipEmpty = false);
std::vector<std::wstring> SplitLines(std::wstring_view s);
std::wstring Join(const std::vector<std::wstring>& parts, std::wstring_view separator);
// "512 B" / "1.5 KB" / "12.34 MB" ("—" for 0)
std::wstring FileSize(long long bytes);
std::wstring Replace(std::wstring_view s, std::wstring_view from, std::wstring_view to);
std::string Replace(std::string_view s, std::string_view from, std::string_view to);

// "{0} of {1}" style formatting, like string.Format with positional arguments only.
std::wstring Format(std::wstring_view pattern, const std::vector<std::wstring>& args);
template <typename... T>
std::wstring Fmt(std::wstring_view pattern, const T&... args) {
    return Format(pattern, { std::wstring(args)... });
}

std::wstring FromInt(long long v);
std::wstring FromDouble(double v, int decimals);  // invariant culture ('.' separator)
std::optional<long long> ToInt(std::wstring_view s);
std::optional<unsigned long long> ToUInt64(std::wstring_view s);
std::optional<double> ToDouble(std::wstring_view s);  // invariant culture
std::optional<long long> ToInt(std::string_view s);

std::wstring Hex(uint64_t v, int digits = 0);
std::string UrlEncode(std::string_view s);
std::wstring UrlEncode(std::wstring_view s);

// Human readable byte counts ("1.2 MB").
std::wstring FormatBytes(unsigned long long bytes);

// Removes characters that can't appear in a Windows file name.
std::wstring SafeFileName(std::wstring_view s);

} // namespace SonicRacingSaveManager::Text
