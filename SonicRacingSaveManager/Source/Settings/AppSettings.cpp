#include "Settings/AppSettings.h"
#include "Settings/ConfigFile.h"

namespace SonicRacingSaveManager::Settings {

namespace {
constexpr const wchar_t* kFile = L"settings.json";
constexpr const wchar_t* kLocalFile = L"app_settings.json";
} // namespace

std::optional<AppSettings> AppSettings::Load() {
    bool exists = false;
    const Json::Value json = Config::ReadJson(kFile, &exists, Config::Folder::Roaming);
    if (!exists || !json.IsObject()) return std::nullopt;
    AppSettings s;
    s.modsDirectory = json["ModsDirectory"].OptWStr();
    return s;
}

void AppSettings::Save() const {
    // Same property name as System.Text.Json wrote it (WriteIndented = true)
    Json::Value json = Json::Value::MakeObject();
    json.Set("ModsDirectory", modsDirectory ? Json::Value(*modsDirectory) : Json::Value(nullptr));
    Config::WriteJson(kFile, json, true, Config::Folder::Roaming);
}

std::optional<std::wstring> AppSettings::GetModsDirectory() {
    const auto s = Load();
    return s ? s->modsDirectory : std::nullopt;
}

void AppSettings::SaveModsDirectory(const std::wstring& modsDirectory) {
    AppSettings s = Load().value_or(AppSettings{});
    s.modsDirectory = modsDirectory;
    s.Save();
}

LocalSettings LocalSettings::Load() {
    LocalSettings s;
    const Json::Value json = Config::ReadJson(kLocalFile);
    if (json.IsObject()) s.showLoadingScreen = json["ShowLoadingScreen"].Bool(true);
    return s;
}

void LocalSettings::Save() const {
    Json::Value json = Json::Value::MakeObject();
    json.Set("ShowLoadingScreen", showLoadingScreen);
    Config::WriteJson(kLocalFile, json, true);
}

bool LocalSettings::GetShowLoadingScreen() { return Load().showLoadingScreen; }

void LocalSettings::SaveShowLoadingScreen(bool show) {
    LocalSettings s = Load();
    s.showLoadingScreen = show;
    s.Save();
}

} // namespace SonicRacingSaveManager::Settings
