#pragma once
#include <optional>
#include <string>
#include "Core/Async.h"

// Discord OAuth2 implicit grant (scope: identify) through the system browser and a local
// listener on http://localhost:45689/callback (Config::Discord).
namespace SonicRacingSaveManager::Services::DiscordOAuth {

struct User {
    std::wstring id, username;
    std::wstring avatar;        // the avatar hash (empty: Discord's default avatar)
    std::wstring accessToken;   // the sign-in's OAuth token (identify, ~7 days), to follow avatar changes
};

// Opens the browser and waits (up to 5 minutes, or until cancelled) for the user to authorize.
// Blocking: call on a worker thread.
std::optional<User> Authenticate(const Async::Cancel& cancel);

std::optional<User> LoadCached();   // discord_user.json
void SaveCached(const User& user);
void ClearCached();

// The user's current avatar: looked up with the sign-in's token (or the optional lookup bot), so it
// follows avatar changes, else the hash cached at sign-in, else Discord's default avatar for the id.
// `animated` keeps a GIF for animated avatars (PNG otherwise). Blocking.
std::wstring AvatarUrl(const User& user, unsigned size = 128, bool animated = false);
// %LOCALAPPDATA%\SonicRacingSaveManager\Avatars\<id>.png (the last downloaded avatar, may not exist)
std::wstring AvatarFile(const User& user);
// Downloads the current avatar (PNG) to AvatarFile. True when written. Blocking.
bool DownloadAvatar(const User& user);

} // namespace SonicRacingSaveManager::Services::DiscordOAuth
