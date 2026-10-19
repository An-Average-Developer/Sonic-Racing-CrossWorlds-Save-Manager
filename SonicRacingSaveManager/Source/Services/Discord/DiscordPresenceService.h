#pragma once
#include <memory>
#include <string>
#include "Services/Discord/DiscordIpcClient.h"

namespace SonicRacingSaveManager::Services {

// DiscordPresenceService.cs — Discord Rich Presence (application "Sonic Racing CrossWorlds Save
// Manager"; art assets "savemanager", "updates", "error"). Only three states, as in the C#:
//   Normal  = every tab except Updates / Changelogs / Reports
//   Updates = the Updates tab (and the Changelogs tab, new in the C++ port)
//   Error   = the Reports tab
//   Large image = "savemanager" (always)
//   Small image = "updates" / "error" depending on the mode, otherwise none
//   Details     = mode-specific text, else the app name
//   State       = version
class DiscordPresenceService {
public:
    enum class Mode { Normal, Updates, Changelogs, Error };

    DiscordPresenceService();
    ~DiscordPresenceService();
    // On startup and whenever "Show Activity on Discord" changes (Settings popup)
    void ApplySettings(bool showActivity);
    // Version on the State line ("v2.1.0")
    void SetVersion(const std::wstring& version);
    void SetMode(Mode mode);

private:
    void Send();

    std::unique_ptr<DiscordIpcClient> client;
    long long sessionStart = 0;
    std::wstring versionLine;
    bool showActivity = true;
    Mode mode = Mode::Normal;
};

} // namespace SonicRacingSaveManager::Services
