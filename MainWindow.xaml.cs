using System;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Animation;
using System.Windows.Media.Imaging;
using MaterialDesignThemes.Wpf;
using SonicRacingSaveManager.ViewModels;
using SonicRacingSaveManager.Common.Services;

namespace SonicRacingSaveManager
{
    public partial class MainWindow : Window
    {
        private MainViewModel? ViewModel => DataContext as MainViewModel;

        public MainWindow()
        {
            InitializeComponent();
            DataContext = new MainViewModel();
        }

        private void CheckForUpdatesBanner_Click(object sender, System.Windows.Input.MouseButtonEventArgs e)
        {
            if (ViewModel != null && ViewModel.CheckForUpdatesCommand.CanExecute(null))
            {
                ViewModel.CheckForUpdatesCommand.Execute(null);
            }
        }

        private void UpdateStatusButton_Click(object sender, System.Windows.Input.MouseButtonEventArgs e)
        {
            if (ViewModel != null &&
                ViewModel.IsUpdateAvailable &&
                ViewModel.ShowInstallationViewCommand.CanExecute(null))
            {
                ViewModel.ShowInstallationViewCommand.Execute(null);
            }
        }

        private void TicketEditorCard_Click(object sender, System.Windows.Input.MouseButtonEventArgs e)
        {
            if (ViewModel != null && ViewModel.MemoryEditor.SelectTicketEditorCommand.CanExecute(null))
            {
                ViewModel.MemoryEditor.SelectTicketEditorCommand.Execute(null);
            }
        }

        private void ThemeToggleButton_Click(object sender, RoutedEventArgs e)
        {
            // Capture a STATIC snapshot of the current theme
            var renderBitmap = new RenderTargetBitmap(
                (int)MainContent.ActualWidth,
                (int)MainContent.ActualHeight,
                96, 96,
                PixelFormats.Pbgra32);
            renderBitmap.Render(MainContent);
            OldThemeSnapshot.Source = renderBitmap;

            // Get button position for animation origin
            var button = ThemeToggleButton;
            var buttonPosition = button.TransformToAncestor(MainContent).Transform(new Point(0, 0));
            var buttonCenter = new Point(
                buttonPosition.X + button.ActualWidth / 2,
                buttonPosition.Y + button.ActualHeight / 2
            );

            // Calculate distance to farthest corner to ensure full coverage
            var distanceToCorners = new[]
            {
                Math.Sqrt(Math.Pow(buttonCenter.X, 2) + Math.Pow(buttonCenter.Y, 2)), // Top-left
                Math.Sqrt(Math.Pow(MainContent.ActualWidth - buttonCenter.X, 2) + Math.Pow(buttonCenter.Y, 2)), // Top-right
                Math.Sqrt(Math.Pow(buttonCenter.X, 2) + Math.Pow(MainContent.ActualHeight - buttonCenter.Y, 2)), // Bottom-left
                Math.Sqrt(Math.Pow(MainContent.ActualWidth - buttonCenter.X, 2) + Math.Pow(MainContent.ActualHeight - buttonCenter.Y, 2)) // Bottom-right
            };
            var maxDistance = distanceToCorners.Max();
            var maxDimension = Math.Max(MainContent.ActualWidth, MainContent.ActualHeight);

            // Get the gradient brush from resources
            var ringMask = (RadialGradientBrush)this.Resources["RingMaskBrush"];
            var innerEdge = ringMask.GradientStops[1];
            var outerEdge = ringMask.GradientStops[2];

            // Position the gradient center at the button (normalized coordinates)
            var normalizedX = buttonCenter.X / MainContent.ActualWidth;
            var normalizedY = buttonCenter.Y / MainContent.ActualHeight;
            ringMask.GradientOrigin = new Point(normalizedX, normalizedY);
            ringMask.Center = new Point(normalizedX, normalizedY);

            // Reset the gradient stops to start
            innerEdge.Offset = 0;
            outerEdge.Offset = 0.02;

            // Toggle theme immediately
            ThemeService.Instance.ToggleTheme();

            // Update icon
            ThemeIcon.Kind = ThemeService.Instance.IsDarkTheme ? PackIconKind.WeatherSunny : PackIconKind.WeatherNight;

            // Show the overlay (shows static snapshot of old theme)
            ThemeTransitionOverlay.Visibility = Visibility.Visible;

            // Animate to offset 1.0 (gradient stops are clamped 0-1)
            // The gradient radius is already enlarged to 3.0, so 1.0 offset will cover everything
            var maxOffset = 1.0;

            // Start the ring animation
            var ringAnimation = new DoubleAnimation
            {
                From = 0,
                To = maxOffset,
                Duration = TimeSpan.FromMilliseconds(800),
                EasingFunction = new CubicEase { EasingMode = EasingMode.EaseInOut }
            };

            ringAnimation.Completed += (s, args) =>
            {
                ThemeTransitionOverlay.Visibility = Visibility.Collapsed;
                OldThemeSnapshot.Source = null;
                innerEdge.Offset = 0;
                outerEdge.Offset = 0.02;
            };

            innerEdge.BeginAnimation(GradientStop.OffsetProperty, ringAnimation);

            var outerAnimation = new DoubleAnimation
            {
                From = 0.02,
                To = 1.0,
                Duration = TimeSpan.FromMilliseconds(800),
                EasingFunction = new CubicEase { EasingMode = EasingMode.EaseInOut }
            };
            outerEdge.BeginAnimation(GradientStop.OffsetProperty, outerAnimation);
        }
    }
}
