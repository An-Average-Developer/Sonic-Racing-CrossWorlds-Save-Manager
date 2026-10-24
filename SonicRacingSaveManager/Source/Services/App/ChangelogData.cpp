// Manager changelog. Add new versions at the TOP (newest first).
// Each entry: version, label shown after it ("| dd/mm/yyyy" = the release date, or e.g.
// "[C++ UPDATE!]"), and its lines: "## " section headers (Additions, Redesigns, Seasonal Themes,
// Compatibility, Improvements, Changes, Performance, Bug Fixes, Maintenance, Other) and "- " changes.
// The history comes from the GameBanana updates (gamebanana.com/tools/updates/21025). The published
// changelog.txt uses the same text (see EmbeddedText; Tools\GenerateChangelog.ps1 writes it).
#include "Services/App/ChangelogData.h"

namespace SonicRacingSaveManager::Services::Changelog {

const std::vector<Entry>& Entries() {
    static const std::vector<Entry> entries = {
        { L"3.0.1", L"[MODS UPDATE!]", {
            L"## Additions",
            L"- New Mod Store tab: browse CrossWorlds mods on GameBanana (search, sorting and categories) and install them in one click (zip, 7z, rar), choosing the files when there are several",
            L"- Mod Store: an info card for every mod with its pictures, description, credits and files; mods you already have show as Installed or Update available",
            L"- Mod Manager: each mod shows what it replaces next to its name (\x2192 Shadow, \x2192 Blaze \x00B7 Voice...), including styles, guest characters and voice, text, music or UI mods",
            L"- Mod Manager: an Uninstall button (the mod's folder goes to the Recycle Bin) and a Reinstall button on each mod, which opens it in the Mod Store to replace all its files with the ones you pick",
            L"- Discord and Steam profile pictures in Reports and in the Backups account lists",
            L"## Redesigns",
            L"- The navigation rail groups Backups (Create, Restore, Manage) and Mods (Mod Manager, Mod Store) into one entry each, with a bar to switch sections (Ctrl+Tab)",
            L"## Changes",
            L"- Backups show readable names (\"<account>'s backup\" or the name you gave it) and their folders are named \"<name> - <date> <time>\"",
            L"## Bug Fixes",
            L"- Reordering the mods no longer makes the list disappear: they slide to their new place while a \"Reordering mods...\" screen shows",
            L"- Long button texts in other languages now shrink to fit instead of being cut off",
        } },
        { L"3.0.0", L"[C++ UPDATE!]", {
            L"## Additions",
            L"- The manager has been rewritten from C# (WPF) to C++: it no longer needs .NET installed and is a single exe",
            L"- Added a loading screen while the manager starts",
            L"- Added a Changelogs tab with the whole history of the manager",
            L"- Added updates from GameBanana as an alternative to GitHub, with its own install button in the Updates tab",
            L"- Mod Manager: drag and drop the mods to change their priority",
            L"- Tickets Menu: a search box to find any tool (Ctrl+F to focus it, Esc to clear it)",
            L"- Tickets Menu: a category list that follows the scroll; click a category to jump to it",
            L"- Tickets Menu: a counter of how many options are on, and your ticket amount once it's located",
            L"- Reports: the manager's log can be attached to bug reports",
            L"- Added an option to turn off the Discord Rich Presence",
            L"## Redesigns",
            L"- New Tickets Menu: an animated banner with the connection status and the Attach / Detach button, and the tools as tiles grouped in Patches and Ticket Editor",
            L"- Updates: the changes of every version since yours, as folding cards grouped by type (Additions, Bug Fixes...)",
            L"- New header, navigation rail and status bar, with animated banners on every tab",
            L"- Rounder cards and new animations all over the app: screens sliding in, hover and press effects, and options lighting up when they turn on",
            L"- Messages and errors now use the manager's own dialogs instead of the Windows message boxes",
            L"- The window is now borderless with a fixed size",
            L"## Seasonal Themes",
            L"- The seasonal themes are dark only: the light / dark button is locked there (grey, with a padlock) and tells you it only works with the Default and Classic themes",
            L"## Bug Fixes",
            L"- The Mod Manager now detects Sonic Racing CrossWorlds running instead of the wrong process",
            L"- Multiply Tickets and Multiply Tickets x500 can no longer be on at the same time (they patch the same spot): each one locks while the other is on",
            L"- The Tickets Menu now notices when the game is closed and waits for it to start again",
            L"- The Mod Manager's status messages are now shown in the tab",
            L"- The selected backup or mod is kept when the lists refresh",
        } },
        { L"2.1.0", L"| 10/10/2026", {
            L"## Additions",
            L"- Added seasonal themes (Summer, Halloween and Christmas), with animated backgrounds and seasonal colors",
            L"- Added a Settings button in the header to pick the season theme (Default, Classic, Summer, Halloween or Christmas)",
            L"## Improvements",
            L"- Replaced the ticket pointer system with a step-by-step Find Tickets guide that searches the game's memory for your ticket amount, so tickets can be found reliably after game updates",
            L"## Changes",
            L"- Renamed the Cheat Menu tab to Tickets Menu",
            L"## Bug Fixes",
            L"- Updated the Freeze, Multiply and x500 ticket patches for the latest game version (1.4.3)",
            L"- Fixed the Reports and Steam tabs being misaligned compared to the other tabs",
        } },
        { L"2.0.0", L"| 17/08/2026", {
            L"## Additions",
            L"- Added a Manual Update option as a fallback for when the automatic updater doesn't detect or install a new version",
            L"- Added Discord Rich Presence, showing your current activity in the app on your Discord profile",
            L"- Added a Report a Bug tab to send bug reports directly from the app, with Discord account verification and optional screenshots",
            L"- Added an Auto-Attach toggle to the Memory Editor, with a manual process picker for when the game isn't detected automatically",
            L"- Added an Add to Steam tab to add this app as a non-Steam game shortcut, complete with its own icon and library artwork",
            L"## Bug Fixes",
            L"- The app now requires administrator privileges to run, needed for reliable memory access",
        } },
        { L"1.2.3", L"| 26/06/2026", {
            L"## Bug Fixes",
            L"- The tool has been updated to work with the version 1.4.1",
        } },
        { L"1.2.2", L"| 31/05/2026", {
            L"## Additions",
            L"- Added a method to duplicate tickets instead of wasting",
            L"- Added a method that makes each purchase grant 500\x00D7 the ticket cost",
        } },
        { L"1.2.1", L"| 27/05/2026", {
            L"## Bug Fixes",
            L"- Fixed Freeze Tickets Option to work with the update 1.3.2",
        } },
        { L"1.2.0", L"| 23/03/2026", {
            L"## Bug Fixes",
            L"- Fixed Freeze Tickets Option to work with the update 1.3.1",
            L"- Fixed Tickets editor to work with the update 1.3.1",
        } },
        { L"1.1.7", L"| 18/12/2025", {
            L"## Bug Fixes",
            L"- Fixed Freeze Tickets Option to work with the update 1.2.2",
            L"- Fixed Tickets editor to work with the update 1.2.2",
        } },
        { L"1.1.6", L"| 15/12/2025", {
            L"## Additions",
            L"- Added a Freeze Tickets Option",
            L"## Bug Fixes",
            L"- Fixed Tickets editor to work with the update 1.2.0",
        } },
        { L"1.1.5", L"| 02/11/2025", {
            L"## Bug Fixes",
            L"- Fixed mod folder not found",
        } },
        { L"1.1.4", L"| 02/11/2025", {
            L"## Additions",
            L"- Added Mod Manager (in beta, only updater and manager)",
            L"## Improvements",
            L"- Better dark theme",
            L"- Better animations when changing themes",
        } },
        { L"1.1.3", L"| 31/10/2025", {
            L"## Additions",
            L"- Themes have been added",
            L"## Bug Fixes",
            L"- Minor visual bugs fixed",
        } },
        { L"1.1.2", L"| 29/10/2025", {
            L"## Improvements",
            L"- Improved Ticket Menu interface",
            L"## Bug Fixes",
            L"- Minor bugs fixed",
        } },
        { L"1.1.1", L"| 28/10/2025", {
            L"## Bug Fixes",
            L"- Fixed some errors in the ticket editor system",
        } },
        { L"1.1.0", L"| 28/10/2025", {
            L"## Additions",
            L"- Added a ticket editor system",
        } },
        { L"1.0.1", L"| 24/10/2025", {
            L"## Additions",
            L"- Added In App Updates",
            L"## Bug Fixes",
            L"- Fixed some UI errors",
        } },
    };
    return entries;
}

} // namespace SonicRacingSaveManager::Services::Changelog
