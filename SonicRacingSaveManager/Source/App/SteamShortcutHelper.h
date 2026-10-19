#pragma once

namespace SonicRacingSaveManager::App {

// --steam-shortcut-helper <exePath> <iconSourcePath>   (SteamShortcutHelperEntryPoint.cs)
// A separate process (never the one Steam tracks) closes Steam, edits shortcuts.vdf and restarts
// Steam, asking for confirmation first and reporting the result in a dialog. The split matters
// because the running manager can itself be the Steam-tracked "game" (launched from the shortcut
// this feature adds): Steam kills the process it tracks when it shuts down.
inline constexpr const wchar_t* SteamShortcutHelperArg = L"--steam-shortcut-helper";
int RunSteamShortcutHelper(int argc, wchar_t** argv);

} // namespace SonicRacingSaveManager::App
