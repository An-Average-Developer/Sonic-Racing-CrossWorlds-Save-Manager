#pragma once
#include <string>

// Every external id, webhook, link, path and process name the manager uses, in one place. The
// services reference these instead of keeping their own copies.
namespace SonicRacingSaveManager::Config {

// User-Agent sent to GitHub (UpdateService.cs)
inline constexpr const wchar_t* UserAgent = L"SonicRacingSaveManager";

// Folder name under %APPDATA%, %LOCALAPPDATA% and %TEMP% (settings, Discord, extracted assets)
inline constexpr const wchar_t* AppFolder = L"SonicRacingSaveManager";

// ── Manager repository (updates, changelog, manual download) ────────────────
namespace GitHub {
inline constexpr const wchar_t* Owner = L"An-Average-Developer";
inline constexpr const wchar_t* Repo = L"Sonic-Racing-CrossWorlds-Save-Manager";
inline constexpr const wchar_t* Branch = L"main";
// Release zip SRCW-SMv<x-y-z>.zip (PopulateDownloadFieldsAsync)
inline constexpr const wchar_t* ReleaseZipPrefix = L"SRCW-SMv";
inline constexpr const wchar_t* ExeName = L"SonicRacingSaveManager.exe";
inline constexpr const wchar_t* LatestExeUrl =
    L"https://github.com/An-Average-Developer/Sonic-Racing-CrossWorlds-Save-Manager/releases/latest/download/SonicRacingSaveManager.exe";
} // namespace GitHub

// ── The game ────────────────────────────────────────────────────────────────
namespace Game {
// Process.GetProcessesByName (Tickets Menu attach, and the Mod Manager's "game is running" lock)
inline constexpr const wchar_t* ProcessName = L"SonicRacingCrossWorldsSteam";
// %APPDATA%\SEGA\SonicRacingCrossWorlds\steam\<account id>\*.sav (+ .vdf) and \Backups\<backup>
inline constexpr const wchar_t* SegaFolder = L"SEGA";
inline constexpr const wchar_t* GameFolder = L"SonicRacingCrossWorlds";
inline constexpr const wchar_t* SavesFolder = L"steam";
inline constexpr const wchar_t* BackupsFolder = L"Backups";
inline constexpr const wchar_t* BackupInfoFile = L"backup_info.json";
} // namespace Game

// ── GameBanana: the manager's own page (alternative update channel) and the Mod Manager's
// update checks (apiv11) ──
namespace GameBanana {
inline constexpr int ToolId = 21025;
inline constexpr const wchar_t* ToolPage = L"https://gamebanana.com/tools/21025";
inline constexpr const wchar_t* ToolUpdatesPage = L"https://gamebanana.com/tools/updates/21025";
inline constexpr const wchar_t* ApiItemUrl = L"https://api.gamebanana.com/Core/Item/Data?itemtype=Tool&itemid=";
inline constexpr const wchar_t* ApiUrl = L"https://gamebanana.com/apiv11/";   // + ItemType/id?_csvProperties=...
// ModManagerService's static HttpClient headers, 10 s timeout
inline constexpr const wchar_t* UserAgent =
    L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36";
inline constexpr const wchar_t* Accept = L"application/json";
inline constexpr const wchar_t* AcceptLanguage = L"en-US,en;q=0.9";
inline constexpr const wchar_t* Referer = L"https://gamebanana.com";
inline constexpr const wchar_t* Origin = L"https://gamebanana.com";
inline constexpr int TimeoutMs = 10000;
} // namespace GameBanana

// ── Discord ─────────────────────────────────────────────────────────────────
namespace Discord {
// Application used for Rich Presence and for "Connect with Discord" in the Reports tab (OAuth)
inline constexpr const wchar_t* ClientId = L"ClientId";
inline constexpr const char* ClientIdUtf8 = "ClientId";
inline constexpr const wchar_t* PresenceAppName = L"Sonic Racing CrossWorlds Save Manager";
// OAuth implicit flow: the browser comes back to a local listener
inline constexpr int OAuthPort = 45689;
inline constexpr const wchar_t* OAuthRedirectUri = L"http://localhost:45689/callback";
inline constexpr const wchar_t* OAuthAuthorizeUrl = L"https://discord.com/oauth2/authorize";
inline constexpr const wchar_t* ApiUserUrl = L"https://discord.com/api/users/@me";
inline constexpr const wchar_t* UserProfileUrl = L"https://discord.com/users/";   // + user id

// Bug reports channel
inline constexpr const wchar_t* BugReportWebhook =
    L"BugReportWebhook";
inline constexpr int ReportEmbedColor = 0x0066CC;
} // namespace Discord

} // namespace SonicRacingSaveManager::Config
