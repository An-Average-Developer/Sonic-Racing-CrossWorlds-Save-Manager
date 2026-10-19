#pragma once
#include <cstdint>
#include <optional>
#include <string>

// Local date and time (System.DateTime with DateTimeKind.Local), stored as FILETIME ticks
// (100 ns since 1601) in local time so it compares and subtracts like DateTime.
namespace SonicRacingSaveManager::Time {

using Ticks = int64_t;
inline constexpr Ticks kTicksPerSecond = 10'000'000;

// DateTime.Now
Ticks Now();
// A file system time (Files::LastWriteTime, UTC FILETIME) as local time — Directory.GetLastWriteTime
Ticks FromFileTimeUtc(int64_t fileTimeUtc);

// DateTime.ToString("o"): "2025-10-19T14:03:22.1234567+02:00" (local offset)
std::wstring RoundTrip(Ticks local);
// DateTime.Parse of the strings the manager writes: "yyyy-MM-dd[THH:mm[:ss[.fffffff]]][Z|±hh:mm]"
// (a space works as the separator too). With an offset the result is converted to local time, as
// DateTime.Parse does. nullopt when the text isn't a date.
std::optional<Ticks> Parse(const std::wstring& text);

// "yyyy-MM-dd HH:mm:ss"
std::wstring Display(Ticks local);
// "yyyyMMdd_HHmmss" (backup and import folder names)
std::wstring Stamp(Ticks local);
// "HH:mm:ss"
std::wstring Clock(Ticks local);

} // namespace SonicRacingSaveManager::Time
