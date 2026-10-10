using System;
using System.IO;
using System.Text.Json;
using SonicRacingSaveManager.Configuration;

namespace SonicRacingSaveManager.Common.Services
{
    /// <summary>
    /// User-selectable season theme.
    /// "default" defers to whichever mode is baked into the .csproj at build time
    /// (see AppVersion.BuildDefaultTheme); any other value overrides it at runtime.
    /// </summary>
    public class SeasonThemeSettings
    {
        public string Theme { get; set; } = "default"; // "default" | "classic" | "summer" | "halloween" | "christmas"

        private static readonly string FilePath = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "SonicRacingSaveManager", "season_theme_settings.json");

        private static SeasonThemeSettings? _current;
        public static SeasonThemeSettings Current => _current ??= Load();

        public static SeasonThemeSettings Load()
        {
            try
            {
                if (File.Exists(FilePath))
                    return _current = JsonSerializer.Deserialize<SeasonThemeSettings>(File.ReadAllText(FilePath)) ?? new();
            }
            catch { }
            return _current = new();
        }

        public void Save()
        {
            try
            {
                Directory.CreateDirectory(Path.GetDirectoryName(FilePath)!);
                File.WriteAllText(FilePath, JsonSerializer.Serialize(this));
            }
            catch { }
            _current = this;
        }

        public string EffectiveTheme => Theme == "default" ? AppVersion.BuildDefaultTheme : Theme;
    }
}
