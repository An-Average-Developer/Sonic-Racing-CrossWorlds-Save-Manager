#include "Core/Text.h"
#include <windows.h>
#include <algorithm>
#include <cstdio>
#include <cwctype>
#include <locale.h>

namespace SonicRacingSaveManager::Text {

std::wstring Widen(std::string_view utf8) {
    if (utf8.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), n);
    return out;
}

std::string Narrow(std::wstring_view wide) {
    if (wide.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), out.data(), n, nullptr, nullptr);
    return out;
}

namespace {

bool IsSpace(wchar_t c) { return c == L' ' || c == L'\t' || c == L'\r' || c == L'\n' || c == 0xFEFF || c == 0xA0; }

wchar_t Fold(wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); }

} // namespace

std::wstring Trim(std::wstring_view s) {
    size_t a = 0, b = s.size();
    while (a < b && IsSpace(s[a])) ++a;
    while (b > a && IsSpace(s[b - 1])) --b;
    return std::wstring(s.substr(a, b - a));
}

std::string Trim(std::string_view s) {
    size_t a = 0, b = s.size();
    const auto space = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    // Skip a UTF-8 byte order mark too
    if (s.size() >= 3 && static_cast<unsigned char>(s[0]) == 0xEF && static_cast<unsigned char>(s[1]) == 0xBB &&
        static_cast<unsigned char>(s[2]) == 0xBF) a = 3;
    while (a < b && space(s[a])) ++a;
    while (b > a && space(s[b - 1])) --b;
    return std::string(s.substr(a, b - a));
}

std::wstring TrimStart(std::wstring_view s) {
    size_t a = 0;
    while (a < s.size() && IsSpace(s[a])) ++a;
    return std::wstring(s.substr(a));
}

std::wstring Lower(std::wstring_view s) {
    std::wstring out(s);
    for (auto& c : out) c = Fold(c);
    return out;
}

std::wstring Upper(std::wstring_view s) {
    std::wstring out(s);
    if (!out.empty()) CharUpperBuffW(out.data(), static_cast<DWORD>(out.size()));
    return out;
}

std::string Lower(std::string_view s) {
    std::string out(s);
    for (auto& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

bool StartsWith(std::wstring_view s, std::wstring_view prefix, bool ignoreCase) {
    if (prefix.size() > s.size()) return false;
    return Equals(s.substr(0, prefix.size()), prefix, ignoreCase);
}

bool EndsWith(std::wstring_view s, std::wstring_view suffix, bool ignoreCase) {
    if (suffix.size() > s.size()) return false;
    return Equals(s.substr(s.size() - suffix.size()), suffix, ignoreCase);
}

bool Contains(std::wstring_view s, std::wstring_view part, bool ignoreCase) {
    if (!ignoreCase) return s.find(part) != std::wstring_view::npos;
    return Lower(s).find(Lower(part)) != std::wstring::npos;
}

bool Equals(std::wstring_view a, std::wstring_view b, bool ignoreCase) {
    if (a.size() != b.size()) return false;
    if (!ignoreCase) return a == b;
    for (size_t i = 0; i < a.size(); ++i)
        if (Fold(a[i]) != Fold(b[i])) return false;
    return true;
}

bool StartsWith(std::string_view s, std::string_view prefix) {
    return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
}

std::vector<std::wstring> Split(std::wstring_view s, wchar_t separator, bool skipEmpty) {
    std::vector<std::wstring> out;
    size_t start = 0;
    for (size_t i = 0; i <= s.size(); ++i) {
        if (i == s.size() || s[i] == separator) {
            if (!skipEmpty || i > start) out.emplace_back(s.substr(start, i - start));
            start = i + 1;
        }
    }
    return out;
}

std::vector<std::string> Split(std::string_view s, char separator, bool skipEmpty) {
    std::vector<std::string> out;
    size_t start = 0;
    for (size_t i = 0; i <= s.size(); ++i) {
        if (i == s.size() || s[i] == separator) {
            if (!skipEmpty || i > start) out.emplace_back(s.substr(start, i - start));
            start = i + 1;
        }
    }
    return out;
}

std::vector<std::wstring> SplitLines(std::wstring_view s) {
    std::vector<std::wstring> out;
    size_t start = 0;
    for (size_t i = 0; i <= s.size(); ++i) {
        if (i == s.size() || s[i] == L'\n') {
            size_t end = i;
            if (end > start && s[end - 1] == L'\r') --end;
            if (i < s.size() || end > start) out.emplace_back(s.substr(start, end - start));
            start = i + 1;
        }
    }
    return out;
}

std::wstring Join(const std::vector<std::wstring>& parts, std::wstring_view separator) {
    std::wstring out;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i) out += separator;
        out += parts[i];
    }
    return out;
}

std::wstring Replace(std::wstring_view s, std::wstring_view from, std::wstring_view to) {
    if (from.empty()) return std::wstring(s);
    std::wstring out;
    size_t pos = 0;
    while (true) {
        const size_t hit = s.find(from, pos);
        if (hit == std::wstring_view::npos) break;
        out.append(s.substr(pos, hit - pos));
        out.append(to);
        pos = hit + from.size();
    }
    out.append(s.substr(pos));
    return out;
}

std::string Replace(std::string_view s, std::string_view from, std::string_view to) {
    if (from.empty()) return std::string(s);
    std::string out;
    size_t pos = 0;
    while (true) {
        const size_t hit = s.find(from, pos);
        if (hit == std::string_view::npos) break;
        out.append(s.substr(pos, hit - pos));
        out.append(to);
        pos = hit + from.size();
    }
    out.append(s.substr(pos));
    return out;
}

std::wstring Format(std::wstring_view pattern, const std::vector<std::wstring>& args) {
    std::wstring out;
    out.reserve(pattern.size());
    for (size_t i = 0; i < pattern.size(); ++i) {
        const wchar_t c = pattern[i];
        if (c == L'{' && i + 1 < pattern.size() && pattern[i + 1] == L'{') { out += L'{'; ++i; continue; }
        if (c == L'}' && i + 1 < pattern.size() && pattern[i + 1] == L'}') { out += L'}'; ++i; continue; }
        if (c == L'{') {
            const size_t close = pattern.find(L'}', i);
            if (close != std::wstring_view::npos) {
                // "{0}" or "{0:N0}" — only the index matters here
                std::wstring_view inner = pattern.substr(i + 1, close - i - 1);
                const size_t colon = inner.find(L':');
                if (colon != std::wstring_view::npos) inner = inner.substr(0, colon);
                if (auto index = ToInt(inner); index && *index >= 0 && static_cast<size_t>(*index) < args.size()) {
                    out += args[static_cast<size_t>(*index)];
                    i = close;
                    continue;
                }
            }
        }
        out += c;
    }
    return out;
}

std::wstring FromInt(long long v) { return std::to_wstring(v); }

std::wstring FromDouble(double v, int decimals) {
    wchar_t buffer[64];
    swprintf_s(buffer, L"%.*f", decimals, v);
    return buffer;
}

std::optional<long long> ToInt(std::wstring_view s) {
    const std::wstring t = Trim(s);
    if (t.empty()) return std::nullopt;
    size_t i = 0;
    bool negative = false;
    if (t[0] == L'-' || t[0] == L'+') { negative = t[0] == L'-'; i = 1; }
    if (i >= t.size()) return std::nullopt;
    long long value = 0;
    for (; i < t.size(); ++i) {
        if (t[i] < L'0' || t[i] > L'9') return std::nullopt;
        value = value * 10 + (t[i] - L'0');
    }
    return negative ? -value : value;
}

std::optional<long long> ToInt(std::string_view s) { return ToInt(std::wstring_view(Widen(s))); }

std::optional<unsigned long long> ToUInt64(std::wstring_view s) {
    const std::wstring t = Trim(s);
    if (t.empty()) return std::nullopt;
    unsigned long long value = 0;
    for (wchar_t c : t) {
        if (c < L'0' || c > L'9') return std::nullopt;
        value = value * 10 + static_cast<unsigned long long>(c - L'0');
    }
    return value;
}

std::optional<double> ToDouble(std::wstring_view s) {
    const std::wstring t = Trim(s);
    if (t.empty()) return std::nullopt;
    wchar_t* end = nullptr;
    _locale_t c = _create_locale(LC_NUMERIC, "C");
    const double v = _wcstod_l(t.c_str(), &end, c);
    _free_locale(c);
    if (end == t.c_str() || *end != L'\0') return std::nullopt;
    return v;
}

std::wstring Hex(uint64_t v, int digits) {
    wchar_t buffer[32];
    if (digits > 0) swprintf_s(buffer, L"%0*llX", digits, static_cast<unsigned long long>(v));
    else swprintf_s(buffer, L"%llX", static_cast<unsigned long long>(v));
    return buffer;
}

std::string UrlEncode(std::string_view s) {
    static const char* digits = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out += static_cast<char>(c);
        else {
            out += '%';
            out += digits[c >> 4];
            out += digits[c & 15];
        }
    }
    return out;
}

std::wstring UrlEncode(std::wstring_view s) { return Widen(UrlEncode(Narrow(s))); }

std::wstring FormatBytes(unsigned long long bytes) {
    const wchar_t* units[] = { L"B", L"KB", L"MB", L"GB", L"TB" };
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        ++unit;
    }
    return FromDouble(value, unit == 0 ? 0 : 1) + L" " + units[unit];
}

std::wstring SafeFileName(std::wstring_view s) {
    std::wstring out;
    for (wchar_t c : s) {
        if (c < 32 || wcschr(L"<>:\"/\\|?*", c)) out += L'_';
        else out += c;
    }
    return Trim(out);
}

std::wstring FileSize(long long bytes) {
    wchar_t buf[32];
    if (bytes <= 0) return L"\x2014";
    if (bytes < 1024) swprintf_s(buf, L"%lld B", bytes);
    else if (bytes < 1024 * 1024) swprintf_s(buf, L"%.1f KB", static_cast<double>(bytes) / 1024.0);
    else swprintf_s(buf, L"%.2f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
    return buf;
}

} // namespace SonicRacingSaveManager::Text
