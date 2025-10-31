using System;
using System.Windows;
using System.Windows.Media;
using MaterialDesignThemes.Wpf;
using MaterialDesignColors;

namespace SonicRacingSaveManager.Common.Services
{
    public class ThemeService
    {
        private static ThemeService? _instance;
        public static ThemeService Instance => _instance ??= new ThemeService();

        private readonly PaletteHelper _paletteHelper;
        private bool _isDarkTheme;

        public bool IsDarkTheme
        {
            get => _isDarkTheme;
            set
            {
                _isDarkTheme = value;
                ApplyTheme(value);
            }
        }

        public event EventHandler<bool>? ThemeChanged;

        private ThemeService()
        {
            _paletteHelper = new PaletteHelper();
            _isDarkTheme = true; // Start with dark theme
        }

        public void ToggleTheme()
        {
            IsDarkTheme = !IsDarkTheme;
            ThemeChanged?.Invoke(this, IsDarkTheme);
        }

        private void ApplyTheme(bool isDark)
        {
            var theme = _paletteHelper.GetTheme();

            if (isDark)
            {
                // Custom dark theme with rich colors
                theme.SetBaseTheme(BaseTheme.Dark);

                // Main backgrounds - very dark
                theme.Background = Color.FromRgb(18, 18, 18);              // Main dark background
                theme.Foreground = Color.FromRgb(230, 230, 235);           // Light text for better contrast

                // Enhanced primary color for dark mode (brighter and more vibrant purple)
                theme.SetPrimaryColor(Color.FromRgb(142, 98, 255));        // Bright vivid purple
                theme.PrimaryLight = Color.FromRgb(179, 157, 255);         // Light variant
                theme.PrimaryMid = Color.FromRgb(124, 77, 255);            // Mid variant
                theme.PrimaryDark = Color.FromRgb(98, 60, 234);            // Dark variant

                // Enhanced secondary color - brighter lime for dark mode
                theme.SetSecondaryColor(Color.FromRgb(205, 220, 57));      // Lime (good contrast on dark)
                theme.SecondaryLight = Color.FromRgb(220, 231, 117);       // Light lime
                theme.SecondaryMid = Color.FromRgb(205, 220, 57);          // Mid lime
                theme.SecondaryDark = Color.FromRgb(175, 180, 43);         // Dark lime

                // Validation/error color
                theme.ValidationError = Color.FromRgb(244, 67, 54);        // Bright red for visibility
            }
            else
            {
                // Light theme - clean and crisp
                theme.SetBaseTheme(BaseTheme.Light);

                // Main backgrounds
                theme.Background = Color.FromRgb(250, 250, 250);           // Light gray background
                theme.Foreground = Color.FromRgb(33, 33, 33);              // Dark text

                // Primary color - standard deep purple
                theme.SetPrimaryColor(Color.FromRgb(103, 58, 183));        // Deep Purple
                theme.PrimaryLight = Color.FromRgb(179, 157, 219);         // Light variant
                theme.PrimaryMid = Color.FromRgb(103, 58, 183);            // Mid variant
                theme.PrimaryDark = Color.FromRgb(77, 40, 140);            // Dark variant

                // Secondary color
                theme.SetSecondaryColor(Color.FromRgb(205, 220, 57));      // Lime
                theme.SecondaryLight = Color.FromRgb(220, 231, 117);       // Light lime
                theme.SecondaryMid = Color.FromRgb(205, 220, 57);          // Mid lime
                theme.SecondaryDark = Color.FromRgb(175, 180, 43);         // Dark lime

                // Validation color
                theme.ValidationError = Color.FromRgb(211, 47, 47);        // Red
            }

            _paletteHelper.SetTheme(theme);
        }

        public Color GetThemeColor()
        {
            return IsDarkTheme ? Color.FromRgb(18, 18, 18) : Color.FromRgb(250, 250, 250);
        }
    }
}
