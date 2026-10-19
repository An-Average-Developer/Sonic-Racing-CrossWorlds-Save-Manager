#pragma once
#include <string>

// Well-known folders (same locations as the C# manager, so existing settings carry over).
namespace SonicRacingSaveManager::Paths {

// %APPDATA%\SonicRacingSaveManager — settings.json (ModsDirectory)
const std::wstring& RoamingDir();
std::wstring RoamingFile(const std::wstring& name);

// %LOCALAPPDATA%\SonicRacingSaveManager — season_theme_settings.json, discord_user.json, discord_settings.json,
// app_settings.json, language.cfg, logs\, SteamIcon\.
// DataDir/DataFile are the launcher names for it (ConfigFile, Localization).
const std::wstring& DataDir();
std::wstring DataFile(const std::wstring& name);

// %TEMP%\SonicRacingSaveManager — embedded assets are unpacked here on startup
const std::wstring& TempDir();
std::wstring Asset(const std::wstring& relative);          // TempDir\Assets\<relative>
std::wstring Translation(const std::wstring& fileName);    // TempDir\Translations\<file>

} // namespace SonicRacingSaveManager::Paths
