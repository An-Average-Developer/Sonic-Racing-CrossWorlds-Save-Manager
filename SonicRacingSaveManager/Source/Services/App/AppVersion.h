#pragma once
#include <string>
#include "App/Config.h"
#include "Core/Text.h"
#include "Version.h"

// Configuration\AppVersion.cs
namespace SonicRacingSaveManager::AppVersion {

inline const std::wstring CurrentVersion = Text::Widen(APP_VERSION_STRING);

// The repository lives in App/Config.h
inline constexpr const wchar_t* GitHubOwner = Config::GitHub::Owner;
inline constexpr const wchar_t* GitHubRepo = Config::GitHub::Repo;
inline constexpr const wchar_t* GitHubBranch = Config::GitHub::Branch;

inline constexpr bool AutoUpdateEnabled = true;

// Controlled by <SummerMode> / <HalloweenMode> / <ChristmasMode> in SonicRacingSaveManager.vcxproj
#if defined(SUMMER_MODE)
inline constexpr bool SummerMode = true;
#else
inline constexpr bool SummerMode = false;
#endif
#if defined(HALLOWEEN_MODE)
inline constexpr bool HalloweenMode = true;
#else
inline constexpr bool HalloweenMode = false;
#endif
#if defined(CHRISTMAS_MODE)
inline constexpr bool ChristmasMode = true;
#else
inline constexpr bool ChristmasMode = false;
#endif

inline std::wstring DisplayVersion() { return L"v" + CurrentVersion; }
inline std::wstring GitHubRepoUrl() { return std::wstring(L"https://github.com/") + GitHubOwner + L"/" + GitHubRepo; }

// The season theme baked into the build (used when the user's season theme setting is "default").
inline std::wstring BuildDefaultTheme() {
    if (SummerMode) return L"summer";
    if (HalloweenMode) return L"halloween";
    if (ChristmasMode) return L"christmas";
    return L"classic";
}

} // namespace SonicRacingSaveManager::AppVersion
