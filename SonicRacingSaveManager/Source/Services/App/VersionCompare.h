#pragma once
#include <string>
#include "Core/Text.h"

namespace SonicRacingSaveManager::Versions {

struct Parsed {
    int major = 0, minor = 0, patch = 0;
};

// Keeps digits and dots only ("v3.0.1-beta" → 3.0.1).
inline Parsed Parse(const std::wstring& version) {
    std::wstring clean;
    for (wchar_t c : version)
        if ((c >= L'0' && c <= L'9') || c == L'.') clean += c;
    const auto parts = Text::Split(clean, L'.');
    Parsed p;
    if (parts.size() > 0) p.major = static_cast<int>(Text::ToInt(parts[0]).value_or(0));
    if (parts.size() > 1) p.minor = static_cast<int>(Text::ToInt(parts[1]).value_or(0));
    if (parts.size() > 2) p.patch = static_cast<int>(Text::ToInt(parts[2]).value_or(0));
    return p;
}

inline bool IsNewer(const std::wstring& current, const std::wstring& latest) {
    const Parsed c = Parse(current), l = Parse(latest);
    if (l.major != c.major) return l.major > c.major;
    if (l.minor != c.minor) return l.minor > c.minor;
    return l.patch > c.patch;
}

// "—" / "512 B" / "1.5 KB" / "12.34 MB" (UpdateService.FormatFileSize)
inline std::wstring FormatFileSize(long long bytes) {
    if (bytes <= 0) return L"\x2014";
    if (bytes < 1024) return std::to_wstring(bytes) + L" B";
    if (bytes < 1024 * 1024) return Text::FromDouble(bytes / 1024.0, 1) + L" KB";
    return Text::FromDouble(bytes / (1024.0 * 1024.0), 2) + L" MB";
}

} // namespace SonicRacingSaveManager::Versions
