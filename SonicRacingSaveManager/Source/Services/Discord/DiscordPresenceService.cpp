#include "Services/Discord/DiscordPresenceService.h"
#include <ctime>
#include "App/Config.h"

namespace SonicRacingSaveManager::Services {

DiscordPresenceService::DiscordPresenceService()
    : client(std::make_unique<DiscordIpcClient>(Config::Discord::ClientIdUtf8)), sessionStart(static_cast<long long>(std::time(nullptr))) {}

DiscordPresenceService::~DiscordPresenceService() {
    if (client) client->SetActivity(Json::Value());   // ClearPresence on dispose
}

void DiscordPresenceService::ApplySettings(bool show) {
    showActivity = show;
    if (!showActivity) {
        if (client) client->SetActivity(Json::Value());
        return;
    }
    Send();
}

void DiscordPresenceService::SetVersion(const std::wstring& version) {
    versionLine = version;
    Send();
}

void DiscordPresenceService::SetMode(Mode newMode) {
    if (newMode == mode) return;
    mode = newMode;
    Send();
}

void DiscordPresenceService::Send() {
    if (!client || !showActivity) return;
    std::wstring details = Config::Discord::PresenceAppName, smallImageKey, smallImageText;
    switch (mode) {
        case Mode::Updates: details = smallImageText = L"Checking for Updates"; smallImageKey = L"updates"; break;
        case Mode::Changelogs: details = smallImageText = L"Viewing Changelog"; smallImageKey = L"updates"; break;
        case Mode::Error: details = smallImageText = L"Reporting a Bug"; smallImageKey = L"error"; break;
        default: break;
    }

    // Discord rejects empty (and 1-char) strings, so those fields are left out (as DiscordRPC did)
    const auto put = [](Json::Value& obj, const char* key, const std::wstring& value) {
        if (value.size() >= 2) obj.Set(key, value.size() > 128 ? value.substr(0, 128) : value);
    };
    Json::Value activity = Json::Value::MakeObject();
    put(activity, "details", details);
    put(activity, "state", versionLine);
    Json::Value timestamps = Json::Value::MakeObject();
    timestamps.Set("start", sessionStart);
    activity.Set("timestamps", timestamps);
    Json::Value assets = Json::Value::MakeObject();
    put(assets, "large_image", L"savemanager");
    put(assets, "large_text", Config::Discord::PresenceAppName);
    put(assets, "small_image", smallImageKey);
    put(assets, "small_text", smallImageText);
    activity.Set("assets", assets);
    client->SetActivity(activity);
}

} // namespace SonicRacingSaveManager::Services
