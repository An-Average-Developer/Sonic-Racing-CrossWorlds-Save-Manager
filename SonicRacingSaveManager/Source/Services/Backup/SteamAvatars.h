#pragma once
#include <string>

// Steam profile pictures of the save accounts (their folder name is the SteamID64). Blocking: call
// from worker threads.
namespace SonicRacingSaveManager::Services::SteamAvatars {

// The picture's bytes: Steam's own cache (Steam\config\avatarcache\<id>.png, accounts that signed
// in on this PC), else the public profile (steamcommunity.com/profiles/<id>?xml=1, its avatarFull),
// kept in %LOCALAPPDATA%\SonicRacingSaveManager\SteamAvatars for when Steam can't be reached. Empty
// when there's none (private profile, offline...).
std::string Fetch(const std::wstring& steamId64);

} // namespace SonicRacingSaveManager::Services::SteamAvatars
