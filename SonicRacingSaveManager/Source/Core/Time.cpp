#include "Core/Time.h"
#include <windows.h>
#include <cstdio>
#include <cwctype>

namespace SonicRacingSaveManager::Time {

namespace {

Ticks FromSystemTime(const SYSTEMTIME& st) {
    FILETIME ft{};
    if (!SystemTimeToFileTime(&st, &ft)) return 0;
    return (static_cast<Ticks>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

SYSTEMTIME ToSystemTime(Ticks ticks) {
    FILETIME ft{ static_cast<DWORD>(ticks & 0xFFFFFFFF), static_cast<DWORD>(ticks >> 32) };
    SYSTEMTIME st{};
    FileTimeToSystemTime(&ft, &st);
    return st;
}

Ticks UtcToLocal(Ticks utc) {
    const SYSTEMTIME u = ToSystemTime(utc);
    SYSTEMTIME l{};
    if (!SystemTimeToTzSpecificLocalTime(nullptr, &u, &l)) return utc;
    // Keep the sub-millisecond ticks SYSTEMTIME drops
    return FromSystemTime(l) + utc % 10'000;
}

Ticks LocalToUtc(Ticks local) {
    const SYSTEMTIME l = ToSystemTime(local);
    SYSTEMTIME u{};
    if (!TzSpecificLocalTimeToSystemTime(nullptr, &l, &u)) return local;
    return FromSystemTime(u) + local % 10'000;
}

// Reads `count` digits at `pos` (advancing it)
bool Digits(const std::wstring& s, size_t& pos, int count, int& value) {
    value = 0;
    for (int i = 0; i < count; ++i, ++pos) {
        if (pos >= s.size() || !std::iswdigit(s[pos])) return false;
        value = value * 10 + (s[pos] - L'0');
    }
    return true;
}

} // namespace

Ticks Now() {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    FILETIME utc{};
    GetSystemTimePreciseAsFileTime(&utc);
    // GetLocalTime's millisecond precision, plus the precise clock's sub-millisecond ticks (DateTime.Now has 100 ns)
    const Ticks precise = (static_cast<Ticks>(utc.dwHighDateTime) << 32) | utc.dwLowDateTime;
    return FromSystemTime(st) + precise % 10'000;
}

Ticks FromFileTimeUtc(int64_t fileTimeUtc) { return UtcToLocal(fileTimeUtc); }

std::wstring RoundTrip(Ticks local) {
    const SYSTEMTIME st = ToSystemTime(local);
    const long long fraction = local % kTicksPerSecond;
    const long long offsetMinutes = (local - LocalToUtc(local)) / (60 * kTicksPerSecond);
    const long long absOffset = offsetMinutes < 0 ? -offsetMinutes : offsetMinutes;
    wchar_t text[48];
    swprintf_s(text, L"%04u-%02u-%02uT%02u:%02u:%02u.%07lld%c%02lld:%02lld", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
               fraction, offsetMinutes < 0 ? L'-' : L'+', absOffset / 60, absOffset % 60);
    return text;
}

std::optional<Ticks> Parse(const std::wstring& raw) {
    size_t b = 0, e = raw.size();
    while (b < e && std::iswspace(raw[b])) ++b;
    while (e > b && std::iswspace(raw[e - 1])) --e;
    const std::wstring s = raw.substr(b, e - b);
    size_t pos = 0;
    SYSTEMTIME st{};
    int v = 0;
    if (!Digits(s, pos, 4, v)) return std::nullopt;
    st.wYear = static_cast<WORD>(v);
    if (pos >= s.size() || s[pos++] != L'-' || !Digits(s, pos, 2, v)) return std::nullopt;
    st.wMonth = static_cast<WORD>(v);
    if (pos >= s.size() || s[pos++] != L'-' || !Digits(s, pos, 2, v)) return std::nullopt;
    st.wDay = static_cast<WORD>(v);
    long long fraction = 0;
    if (pos < s.size() && (s[pos] == L'T' || s[pos] == L' ')) {
        ++pos;
        if (!Digits(s, pos, 2, v)) return std::nullopt;
        st.wHour = static_cast<WORD>(v);
        if (pos >= s.size() || s[pos++] != L':' || !Digits(s, pos, 2, v)) return std::nullopt;
        st.wMinute = static_cast<WORD>(v);
        if (pos < s.size() && s[pos] == L':') {
            ++pos;
            if (!Digits(s, pos, 2, v)) return std::nullopt;
            st.wSecond = static_cast<WORD>(v);
            if (pos < s.size() && s[pos] == L'.') {
                ++pos;
                // Up to 7 fraction digits (100 ns ticks); more are ignored
                long long scale = 1'000'000;
                while (pos < s.size() && std::iswdigit(s[pos])) {
                    if (scale > 0) fraction += (s[pos] - L'0') * scale;
                    scale /= 10;
                    ++pos;
                }
            }
        }
    }
    FILETIME ft{};
    if (!SystemTimeToFileTime(&st, &ft)) return std::nullopt;   // validates the date
    const Ticks clock = ((static_cast<Ticks>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime) + fraction;
    if (pos == s.size()) return clock;   // no offset: already local
    if (s[pos] == L'Z' && pos + 1 == s.size()) return UtcToLocal(clock);
    if (s[pos] != L'+' && s[pos] != L'-') return std::nullopt;
    const bool negative = s[pos++] == L'-';
    int hours = 0, minutes = 0;
    if (!Digits(s, pos, 2, hours)) return std::nullopt;
    if (pos < s.size() && s[pos] == L':') ++pos;
    if (pos < s.size() && !Digits(s, pos, 2, minutes)) return std::nullopt;
    if (pos != s.size()) return std::nullopt;
    const Ticks offset = (hours * 60LL + minutes) * 60 * kTicksPerSecond;
    return UtcToLocal(negative ? clock + offset : clock - offset);
}

std::wstring Display(Ticks local) {
    const SYSTEMTIME st = ToSystemTime(local);
    wchar_t text[32];
    swprintf_s(text, L"%04u-%02u-%02u %02u:%02u:%02u", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return text;
}

std::wstring Stamp(Ticks local) {
    const SYSTEMTIME st = ToSystemTime(local);
    wchar_t text[32];
    swprintf_s(text, L"%04u%02u%02u_%02u%02u%02u", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return text;
}

std::wstring Clock(Ticks local) {
    const SYSTEMTIME st = ToSystemTime(local);
    wchar_t text[16];
    swprintf_s(text, L"%02u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
    return text;
}

} // namespace SonicRacingSaveManager::Time
