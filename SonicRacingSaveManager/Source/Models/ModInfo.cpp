#include "Models/ModInfo.h"
#include <cstdio>
#include "Localization/Localization.h"

namespace SonicRacingSaveManager::Models {

std::wstring ModInfo::FileSizeFormatted() const {
    if (totalSize == 0) return L"0 KB";
    double bytes = static_cast<double>(totalSize);
    static const wchar_t* const sizes[] = { L"B", L"KB", L"MB", L"GB" };
    int order = 0;
    while (bytes >= 1024 && order < 3) {
        ++order;
        bytes /= 1024;
    }
    // {bytes:0.##}: up to two decimals, trailing zeros dropped
    wchar_t text[32];
    swprintf_s(text, L"%.2f", bytes);
    std::wstring number = text;
    while (number.back() == L'0') number.pop_back();
    if (number.back() == L'.') number.pop_back();
    return number + L" " + sizes[order];
}

std::wstring ModInfo::StatusText() const { return Loc::Get(isEnabled ? "mods_status_enabled" : "mods_status_disabled"); }

std::wstring ModInfo::VersionDisplay() const {
    if (version.empty()) return L"-";
    if (hasUpdate && !latestVersion.empty()) return version + L" \x2192 " + latestVersion;
    return version;
}

std::wstring ModInfo::PriorityDisplay() const {
    if (priority < 0) return L"-";
    wchar_t text[16];
    swprintf_s(text, L"%02d", priority + 1);
    return text;
}

std::wstring ModInfo::UpdateStatusText() const {
    if (version.empty()) return L"-";   // no version info available
    return Loc::Get(hasUpdate ? "mods_update_available" : "mods_up_to_date");
}

Color ModInfo::UpdateStatusColor() const {
    if (version.empty()) return Argb(0xFF808080);   // grey for no version info
    return hasUpdate ? Argb(0xFFFF6B00) : Argb(0xFF4CAF50);
}

} // namespace SonicRacingSaveManager::Models
