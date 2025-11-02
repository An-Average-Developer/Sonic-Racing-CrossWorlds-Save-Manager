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
            _isDarkTheme = true;
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
                theme.SetBaseTheme(BaseTheme.Dark);

                theme.Background = Color.FromRgb(18, 18, 18);
                theme.Foreground = Color.FromRgb(245, 245, 245);

                theme.SetPrimaryColor(Color.FromRgb(200, 200, 200));
                theme.PrimaryLight = Color.FromRgb(230, 230, 230);
                theme.PrimaryMid = Color.FromRgb(180, 180, 180);
                theme.PrimaryDark = Color.FromRgb(150, 150, 150);

                theme.SetSecondaryColor(Color.FromRgb(160, 160, 160));
                theme.SecondaryLight = Color.FromRgb(190, 190, 190);
                theme.SecondaryMid = Color.FromRgb(140, 140, 140);
                theme.SecondaryDark = Color.FromRgb(120, 120, 120);

                theme.ValidationError = Color.FromRgb(180, 180, 180);
            }
            else
            {
                theme.SetBaseTheme(BaseTheme.Light);

                theme.Background = Color.FromRgb(250, 250, 250);
                theme.Foreground = Color.FromRgb(33, 33, 33);

                theme.SetPrimaryColor(Color.FromRgb(103, 58, 183));
                theme.PrimaryLight = Color.FromRgb(179, 157, 219);
                theme.PrimaryMid = Color.FromRgb(103, 58, 183);
                theme.PrimaryDark = Color.FromRgb(77, 40, 140);

                theme.SetSecondaryColor(Color.FromRgb(205, 220, 57));
                theme.SecondaryLight = Color.FromRgb(220, 231, 117);
                theme.SecondaryMid = Color.FromRgb(205, 220, 57);
                theme.SecondaryDark = Color.FromRgb(175, 180, 43);

                theme.ValidationError = Color.FromRgb(211, 47, 47);
            }

            _paletteHelper.SetTheme(theme);
        }

        public Color GetThemeColor()
        {
            return IsDarkTheme ? Color.FromRgb(18, 18, 18) : Color.FromRgb(250, 250, 250);
        }
    }
}
