using System;
using System.Windows;
using SonicRacingSaveManager.Common.Services;

namespace SonicRacingSaveManager
{
    public partial class App : Application
    {
        protected override void OnStartup(StartupEventArgs e)
        {
            if (e.Args.Length > 0 && e.Args[0] == SteamShortcutHelperEntryPoint.Arg)
            {
                Environment.Exit(SteamShortcutHelperEntryPoint.Run(e.Args));
                return;
            }

            base.OnStartup(e);

            SteamAssetExtractor.Extract();

            var themeService = ThemeService.Instance;
            themeService.IsDarkTheme = true;
        }
    }
}
