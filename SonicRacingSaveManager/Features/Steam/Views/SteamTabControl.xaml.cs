using System;
using System.Diagnostics;
using System.IO;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Animation;
using System.Windows.Media.Imaging;
using SonicRacingSaveManager.Common.Services;

namespace SonicRacingSaveManager.Features.Steam.Views
{
    public partial class SteamTabControl : UserControl
    {
        public SteamTabControl()
        {
            InitializeComponent();
            Loaded += SteamTabControl_Loaded;
        }

        private void SteamTabControl_Loaded(object sender, RoutedEventArgs e)
        {
            var bannerPath = Path.Combine(SteamAssetExtractor.TempDir, "Assets", "Steam", "Banner.jpg");
            if (File.Exists(bannerPath))
                BannerImage.Source = LoadImagePreview(bannerPath);

            var iconPath = Path.Combine(SteamAssetExtractor.TempDir, "Assets", "Steam", "icons", "iconHD.png");
            if (File.Exists(iconPath))
                IconImage.Source = LoadImagePreview(iconPath);

            RefreshStatus();
        }

        private void RefreshStatus()
        {
            if (SteamShortcutService.IsSteamRunning())
            {
                StatusIcon.Kind = MaterialDesignThemes.Wpf.PackIconKind.CheckCircleOutline;
                StatusIcon.Foreground = new SolidColorBrush(Color.FromRgb(0x00, 0xC8, 0x53));
                StatusText.Text = "Steam is running — it will be closed and reopened to apply the shortcut.";
            }
            else
            {
                StatusIcon.Kind = MaterialDesignThemes.Wpf.PackIconKind.InformationOutline;
                StatusIcon.Foreground = new SolidColorBrush(Color.FromRgb(0x64, 0xB5, 0xF6));
                StatusText.Text = "Steam is not currently running — the shortcut will be added directly.";
            }
        }

        // BitmapCacheOption.OnLoad forces the whole file to be decoded into memory immediately and
        // the underlying file handle closed right after — without it, BitmapImage streams lazily
        // from disk and keeps the source file locked open for as long as this Image control exists,
        // which would block re-extracting assets (SteamAssetExtractor.Extract()) on the next launch.
        private static BitmapImage LoadImagePreview(string path)
        {
            var bmp = new BitmapImage();
            bmp.BeginInit();
            bmp.CacheOption = BitmapCacheOption.OnLoad;
            bmp.UriSource = new Uri(path);
            bmp.EndInit();
            bmp.Freeze();
            return bmp;
        }

        // ── Add to Steam (writes/updates our own shortcuts.vdf entry) ────────────

        private void AddToSteamBtn_Click(object sender, RoutedEventArgs e)
        {
            var iconPath = Path.Combine(SteamAssetExtractor.TempDir, "Assets", "Steam", "icons", "iconHD.png");
            AddToSteamIconDefaultImg.Source = File.Exists(iconPath) ? LoadImagePreview(iconPath) : null;

            AddToSteamWarningText.Text = SteamShortcutService.IsSteamRunning()
                ? "Steam will be closed and reopened to apply this shortcut."
                : "Steam is not currently running — the shortcut will be added directly.";
            OpenAddToSteamOverlay();
        }

        private void OpenAddToSteamOverlay()
        {
            AddToSteamOverlay.Opacity = 0;
            AddToSteamOverlay.Visibility = Visibility.Visible;
            var ease = new CubicEase { EasingMode = EasingMode.EaseOut };
            AddToSteamOverlay.BeginAnimation(UIElement.OpacityProperty,
                new DoubleAnimation(0, 1, TimeSpan.FromMilliseconds(220)));
            AddToSteamPopupScale.BeginAnimation(ScaleTransform.ScaleXProperty,
                new DoubleAnimation(0.85, 1, TimeSpan.FromMilliseconds(260)) { EasingFunction = ease });
            AddToSteamPopupScale.BeginAnimation(ScaleTransform.ScaleYProperty,
                new DoubleAnimation(0.85, 1, TimeSpan.FromMilliseconds(260)) { EasingFunction = ease });
        }

        private void CloseAddToSteamOverlay()
        {
            var ease = new CubicEase { EasingMode = EasingMode.EaseIn };
            var fade = new DoubleAnimation(1, 0, TimeSpan.FromMilliseconds(180)) { EasingFunction = ease };
            fade.Completed += (_, _) => AddToSteamOverlay.Visibility = Visibility.Collapsed;
            AddToSteamOverlay.BeginAnimation(UIElement.OpacityProperty, fade);
        }

        private void AddToSteamCancelBtn_Click(object sender, RoutedEventArgs e) => CloseAddToSteamOverlay();

        // The actual close-Steam/edit-vdf/restart-Steam sequence runs in a standalone helper
        // process instead of here: if this exe is the one Steam is currently tracking as "the
        // running game" (e.g. launched from a Steam shortcut this feature added), Steam
        // force-kills the process it's tracking when it shuts down, which would kill the
        // operation before it finishes. A freshly spawned, Steam-untracked process isn't subject
        // to that.
        private void AddToSteamConfirmBtn_Click(object sender, RoutedEventArgs e)
        {
            CloseAddToSteamOverlay();

            var exePath = Environment.ProcessPath;
            if (string.IsNullOrEmpty(exePath))
            {
                MessageBox.Show(
                    "Could not determine this application's executable path.",
                    "Add to Steam",
                    MessageBoxButton.OK,
                    MessageBoxImage.Error);
                return;
            }

            var iconSourcePath = Path.Combine(SteamAssetExtractor.TempDir, "Assets", "Steam", "icons", "iconHD.png");
            if (!File.Exists(iconSourcePath))
            {
                MessageBox.Show(
                    $"Icon asset was not found at:\n{iconSourcePath}\n\nTry restarting the app.",
                    "Add to Steam",
                    MessageBoxButton.OK,
                    MessageBoxImage.Error);
                return;
            }

            try
            {
                var psi = new ProcessStartInfo(exePath) { UseShellExecute = true, Verb = "runas" };
                psi.ArgumentList.Add(SteamShortcutHelperEntryPoint.Arg);
                psi.ArgumentList.Add(exePath);
                psi.ArgumentList.Add(iconSourcePath);
                Process.Start(psi);
            }
            catch (Exception ex)
            {
                MessageBox.Show(
                    $"Error adding to Steam:\n{ex}",
                    "Add to Steam",
                    MessageBoxButton.OK,
                    MessageBoxImage.Error);
                return;
            }

            // A graceful shutdown isn't enough here: if this exe is the one Steam is tracking as
            // "the running game", Steam's library still shows it as running until the process is
            // actually gone, and the helper is about to shut Steam down right after this — so this
            // process must be dead first, not just mid-cleanup. Hard-exit instead of waiting on
            // window/thread teardown.
            Environment.Exit(0);
        }
    }
}
