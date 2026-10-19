#include "Settings/SeasonThemeSettings.h"
#include "Services/App/AppVersion.h"
#include "Settings/ConfigFile.h"

namespace SonicRacingSaveManager::Settings {

namespace {
constexpr const wchar_t* kFile = L"season_theme_settings.json";
}

SeasonTheme& SeasonTheme::Current() {
    static SeasonTheme current = [] {
        SeasonTheme t;
        const Json::Value json = Config::ReadJson(kFile);
        if (json.Has("Theme")) t.theme = json["Theme"].WStr();
        return t;
    }();
    return current;
}

void SeasonTheme::Save() {
    // JsonSerializer.Serialize(this): {"Theme":"..."}
    Json::Value json = Json::Value::MakeObject();
    json.Set("Theme", theme);
    Config::WriteJson(kFile, json);
    Current() = *this;
}

std::wstring SeasonTheme::EffectiveTheme() const { return theme == L"default" ? AppVersion::BuildDefaultTheme() : theme; }

} // namespace SonicRacingSaveManager::Settings
