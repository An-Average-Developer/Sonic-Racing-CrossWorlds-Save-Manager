#pragma once

namespace SonicRacingSaveManager::Settings {

// "Show Activity on Discord" in the Settings popup (new in the C++ port: the C# always showed it) —
// %LOCALAPPDATA%\SonicRacingSaveManager\discord_settings.json
struct DiscordPresence {
    bool showActivity = true;

    void Save() const;
    static DiscordPresence Load();
};

} // namespace SonicRacingSaveManager::Settings
