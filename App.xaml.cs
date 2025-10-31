using System.Windows;
using SonicRacingSaveManager.Common.Services;

namespace SonicRacingSaveManager
{
    public partial class App : Application
    {
        protected override void OnStartup(StartupEventArgs e)
        {
            base.OnStartup(e);

            // Initialize and apply theme
            var themeService = ThemeService.Instance;
            themeService.IsDarkTheme = true; // This will trigger ApplyTheme
        }
    }
}
