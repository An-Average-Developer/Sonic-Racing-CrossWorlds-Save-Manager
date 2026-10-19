#include "Settings/DiscordPresenceSettings.h"
#include "Settings/ConfigFile.h"

namespace SonicRacingSaveManager::Settings {

namespace {
constexpr const wchar_t* kFile = L"discord_settings.json";
}

DiscordPresence DiscordPresence::Load() {
    DiscordPresence d;
    const Json::Value json = Config::ReadJson(kFile);
    if (json.IsObject()) d.showActivity = json["ShowActivity"].Bool(true);
    return d;
}

void DiscordPresence::Save() const {
    Json::Value json = Json::Value::MakeObject();
    json.Set("ShowActivity", showActivity);
    Config::WriteJson(kFile, json);
}

} // namespace SonicRacingSaveManager::Settings
