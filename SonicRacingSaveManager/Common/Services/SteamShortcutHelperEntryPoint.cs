using System;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;

namespace SonicRacingSaveManager.Common.Services;

/// <summary>
/// Entry point for the Add-to-Steam helper process: <c>SteamTabControl</c> relaunches the app with
/// argv [<see cref="Arg"/>, exePath, iconSourcePath] and shuts down immediately, so this instance —
/// never launched by Steam — is the one that closes Steam, edits shortcuts.vdf, and restarts Steam.
/// That split matters because the running exe can itself be the Steam-tracked "game" process (if the
/// user launched this app from a Steam shortcut this same feature added): Steam force-kills the
/// process it's tracking when it shuts down, which would kill the operation before
/// <see cref="SteamShortcutService.AddOrUpdateShortcut"/> ever ran.
/// </summary>
public static class SteamShortcutHelperEntryPoint
{
    public const string Arg = "--steam-shortcut-helper";

    public static int Run(string[] args)
    {
        if (args.Length < 3)
        {
            MessageBox.Show(
                $"Add-to-Steam helper started with {args.Length} argument(s), expected 3.",
                "Add to Steam", MessageBoxButton.OK, MessageBoxImage.Error);
            return 1;
        }
        var exePath = args[1];
        var iconSourcePath = args[2];

        // Everything below is wrapped so a failure shows up as a dialog instead of the process
        // just vanishing — this instance has no console and no parent to report back to.
        try
        {
            SteamAssetExtractor.Extract();

            var owner = new Window
            {
                Width = 1, Height = 1,
                WindowStyle = WindowStyle.None,
                AllowsTransparency = true,
                Background = Brushes.Transparent,
                ShowInTaskbar = false,
                WindowStartupLocation = WindowStartupLocation.CenterScreen,
            };
            owner.Show();

            var wasRunning = SteamShortcutService.IsSteamRunning();
            var warning = wasRunning
                ? "Steam will be closed and reopened to apply this shortcut."
                : "Steam is not currently running — the shortcut will be added directly.";

            var confirmed = WpfDialog.Show(owner, "ADD TO STEAM",
                MakeText(warning, Color.FromArgb(200, 232, 160, 0)),
                primaryText: "ADD TO STEAM",
                closeText: "Cancel") == WpfDialogResult.Primary;

            if (!confirmed)
            {
                owner.Close();
                return 0;
            }

            string message;
            bool success;
            try
            {
                var steamPath = SteamShortcutService.FindSteamPath()
                    ?? throw new InvalidOperationException("Steam installation not found.");

                if (wasRunning) SteamShortcutService.CloseSteam(steamPath);
                SteamShortcutService.AddOrUpdateShortcut(exePath, iconSourcePath);
                if (wasRunning) SteamShortcutService.StartSteam(steamPath);

                message = "Added to Steam!\nRestart Steam if it doesn't show up yet.";
                success = true;
            }
            catch (Exception ex)
            {
                message = $"Error adding to Steam:\n{ex.Message}";
                success = false;
            }

            WpfDialog.Show(owner, "ADD TO STEAM",
                MakeText(message, success ? Color.FromArgb(200, 0, 204, 170) : Color.FromArgb(200, 160, 180, 200)),
                closeText: "OK");

            owner.Close();
            return 0;
        }
        catch (Exception ex)
        {
            MessageBox.Show(
                $"Add-to-Steam helper crashed:\n\n{ex}",
                "Add to Steam", MessageBoxButton.OK, MessageBoxImage.Error);
            return 1;
        }
    }

    private static TextBlock MakeText(string text, Color color) => new()
    {
        Text = text,
        FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
        FontSize = 12,
        Foreground = new SolidColorBrush(color),
        TextWrapping = TextWrapping.Wrap,
        MaxWidth = 360,
    };
}
