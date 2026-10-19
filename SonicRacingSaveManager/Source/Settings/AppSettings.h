#pragma once
#include <optional>
#include <string>

namespace SonicRacingSaveManager::Settings {

// Configuration\AppSettings.cs — %APPDATA%\SonicRacingSaveManager\settings.json (indented). Like the
// C# SettingsService, every getter re-reads the file and every save rewrites it whole.
struct AppSettings {
    std::optional<std::wstring> modsDirectory;

    static std::optional<AppSettings> Load();   // nullopt when missing or unreadable
    void Save() const;
    static std::optional<std::wstring> GetModsDirectory();
    static void SaveModsDirectory(const std::wstring& modsDirectory);
};

// Options new in the C++ port — %LOCALAPPDATA%\SonicRacingSaveManager\app_settings.json, so the
// C# manager's settings.json keeps only what it knows (it would drop other keys when saving).
struct LocalSettings {
    bool showLoadingScreen = true;

    static LocalSettings Load();
    void Save() const;
    static bool GetShowLoadingScreen();
    static void SaveShowLoadingScreen(bool show);
};

} // namespace SonicRacingSaveManager::Settings
