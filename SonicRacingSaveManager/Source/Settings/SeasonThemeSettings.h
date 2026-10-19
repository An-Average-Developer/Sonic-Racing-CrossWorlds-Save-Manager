#pragma once
#include <string>

namespace SonicRacingSaveManager::Settings {

// SeasonThemeSettings.cs — %LOCALAPPDATA%\SonicRacingSaveManager\season_theme_settings.json.
// "default" defers to the mode baked into the build (AppVersion::BuildDefaultTheme); any other
// value overrides it at runtime.
struct SeasonTheme {
    std::wstring theme = L"default";   // default | classic | summer | halloween | christmas

    std::wstring EffectiveTheme() const;
    void Save();
    static SeasonTheme& Current();
};

} // namespace SonicRacingSaveManager::Settings
