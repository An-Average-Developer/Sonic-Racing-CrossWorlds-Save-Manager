#include "Models/BackupInfo.h"
#include "Localization/Localization.h"

namespace SonicRacingSaveManager::Models {

std::wstring BackupInfo::DisplayDate() const { return Time::Display(backupDate); }

std::wstring BackupInfo::TimeAgo() const {
    const double seconds = static_cast<double>(Time::Now() - backupDate) / Time::kTicksPerSecond;
    const double minutes = seconds / 60.0, hours = minutes / 60.0, days = hours / 24.0;
    if (minutes < 1) return Loc::Get("time_just_now");
    if (minutes < 60) return Loc::Fmt("time_minutes_ago", static_cast<int>(minutes));
    if (hours < 24) return Loc::Fmt("time_hours_ago", static_cast<int>(hours));
    if (days < 7) return Loc::Fmt("time_days_ago", static_cast<int>(days));
    return DisplayDate();
}

std::wstring BackupInfo::DisplayTitle() const {
    if (!title.empty()) return title;
    // Created without a name (3.0.1 on): after the account
    if (autoTitle) return Loc::Fmt("backup_title_auto", AccountDisplay());
    // Older folders: "backup_<id or name>_20261023_152045", "<name>_20261023_152045",
    // "imported_20261023_152045"; the date is shown next to the title already
    std::wstring n = name;
    const auto digits = [&](size_t from, size_t count) {
        if (from + count > n.size()) return false;
        for (size_t i = from; i < from + count; ++i)
            if (n[i] < L'0' || n[i] > L'9') return false;
        return true;
    };
    if (n.size() > 16 && n[n.size() - 16] == L'_' && n[n.size() - 7] == L'_' && digits(n.size() - 15, 8) && digits(n.size() - 6, 6)) n.resize(n.size() - 16);
    if (n == L"imported") return Loc::Get("backup_title_imported");
    if (n.size() > 7 && n.compare(0, 7, L"backup_") == 0) {
        const std::wstring owner = n.substr(7);
        return Loc::Fmt("backup_title_auto", !accountName.empty() && (owner == accountId || owner.empty()) ? accountName : owner);
    }
    return n.empty() ? name : n;
}

} // namespace SonicRacingSaveManager::Models
