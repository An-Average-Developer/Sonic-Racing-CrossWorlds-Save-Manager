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

} // namespace SonicRacingSaveManager::Models
