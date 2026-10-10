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

        /// <summary>Card background used by the implicit Card style (DynamicResource in App.xaml).</summary>
        public const string CardBackgroundKey = "SeasonalCardBackgroundBrush";

        // Legacy Material Design brush keys still used by many views. PaletteHelper doesn't
        // refresh them, so seasonal palettes override them at the application level.
        private static readonly string[] LegacyBrushKeys =
        {
            "PrimaryHueLightBrush", "PrimaryHueMidBrush", "PrimaryHueDarkBrush",
            "SecondaryHueLightBrush", "SecondaryHueMidBrush", "SecondaryHueDarkBrush",
        };

        private readonly PaletteHelper _paletteHelper;
        private bool _isDarkTheme;

        /// <summary>
        /// Dark (default) = classic grey look, light = classic purple/lime look.
        /// While a season is active (see <see cref="ActiveSeason"/>) both modes use that
        /// season's dark or light variant instead.
        /// </summary>
        public bool IsDarkTheme
        {
            get => _isDarkTheme;
            set
            {
                _isDarkTheme = value;
                ApplyTheme();
            }
        }

        /// <summary>
        /// The season whose visuals are currently shown ("halloween", …), or "classic" when
        /// none. Follows the season theme setting, in both dark and light mode.
        /// </summary>
        public string ActiveSeason { get; private set; } = "classic";

        public event EventHandler<bool>? ThemeChanged;

        /// <summary>Raised whenever <see cref="ActiveSeason"/> or the dark/light mode changes.</summary>
        public event EventHandler<string>? SeasonChanged;

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

        /// <summary>Re-applies the theme, e.g. after the season theme setting changes.</summary>
        public void RefreshSeason()
        {
            if (ResolveSeason() != ActiveSeason) ApplyTheme();
        }

        private bool? _appliedIsDark;

        private static string ResolveSeason()
        {
            return SeasonThemeSettings.Current.EffectiveTheme switch
            {
                "halloween" => "halloween",
                "summer"    => "summer",
                "christmas" => "christmas",
                _           => "classic",
            };
        }

        private void ApplyTheme()
        {
            var season = ResolveSeason();
            var theme = _paletteHelper.GetTheme();
            Color? cardBackground = null; // null = the base theme's card color

            if (season == "halloween" && _isDarkTheme)
            {
                theme.SetBaseTheme(BaseTheme.Dark);

                theme.Background = Color.FromRgb(18, 10, 28);    // near-black purple
                theme.Foreground = Color.FromRgb(243, 233, 255);

                // Light enough to read as text on the near-black background.
                theme.SetPrimaryColor(Color.FromRgb(155, 93, 229));
                theme.PrimaryLight = Color.FromRgb(195, 155, 255);
                theme.PrimaryMid = Color.FromRgb(155, 93, 229);
                theme.PrimaryDark = Color.FromRgb(106, 47, 181);

                theme.SetSecondaryColor(Color.FromRgb(255, 122, 26));
                theme.SecondaryLight = Color.FromRgb(255, 167, 77);
                theme.SecondaryMid = Color.FromRgb(255, 122, 26);
                theme.SecondaryDark = Color.FromRgb(214, 84, 0);

                cardBackground = Color.FromRgb(36, 20, 56);
                theme.Cards.Background = cardBackground.Value;

                theme.ValidationError = Color.FromRgb(255, 82, 82);
            }
            else if (season == "halloween")
            {
                theme.SetBaseTheme(BaseTheme.Light);

                theme.Background = Color.FromRgb(244, 234, 251);  // pale lavender
                theme.Foreground = Color.FromRgb(42, 18, 56);

                theme.SetPrimaryColor(Color.FromRgb(106, 47, 181));
                theme.PrimaryLight = Color.FromRgb(155, 93, 229);
                theme.PrimaryMid = Color.FromRgb(106, 47, 181);
                theme.PrimaryDark = Color.FromRgb(74, 30, 120);

                theme.SetSecondaryColor(Color.FromRgb(255, 122, 26));
                theme.SecondaryLight = Color.FromRgb(255, 167, 77);
                theme.SecondaryMid = Color.FromRgb(255, 122, 26);
                theme.SecondaryDark = Color.FromRgb(214, 84, 0);

                cardBackground = Color.FromRgb(255, 233, 210);    // soft pumpkin
                theme.Cards.Background = cardBackground.Value;

                theme.ValidationError = Color.FromRgb(211, 47, 47);
            }
            else if (season == "summer" && _isDarkTheme)
            {
                // Tropical night: deep ocean blues with sunny yellow accents.
                theme.SetBaseTheme(BaseTheme.Dark);

                theme.Background = Color.FromRgb(7, 30, 38);
                theme.Foreground = Color.FromRgb(230, 251, 255);

                theme.SetPrimaryColor(Color.FromRgb(16, 152, 173));
                theme.PrimaryLight = Color.FromRgb(102, 217, 232);
                theme.PrimaryMid = Color.FromRgb(16, 152, 173);
                theme.PrimaryDark = Color.FromRgb(11, 114, 133);

                theme.SetSecondaryColor(Color.FromRgb(255, 182, 39));
                theme.SecondaryLight = Color.FromRgb(255, 209, 102);
                theme.SecondaryMid = Color.FromRgb(255, 182, 39);
                theme.SecondaryDark = Color.FromRgb(240, 140, 0);

                cardBackground = Color.FromRgb(15, 52, 65);
                theme.Cards.Background = cardBackground.Value;

                theme.ValidationError = Color.FromRgb(255, 107, 107);
            }
            else if (season == "summer")
            {
                // Beach day: warm sand background, light sea-foam cards, ocean and coral accents.
                theme.SetBaseTheme(BaseTheme.Light);

                theme.Background = Color.FromRgb(255, 244, 220);
                theme.Foreground = Color.FromRgb(31, 45, 58);

                theme.SetPrimaryColor(Color.FromRgb(11, 114, 133));
                theme.PrimaryLight = Color.FromRgb(16, 152, 173);
                theme.PrimaryMid = Color.FromRgb(11, 114, 133);
                theme.PrimaryDark = Color.FromRgb(9, 92, 107);

                theme.SetSecondaryColor(Color.FromRgb(255, 127, 80));
                theme.SecondaryLight = Color.FromRgb(255, 160, 122);
                theme.SecondaryMid = Color.FromRgb(255, 127, 80);
                theme.SecondaryDark = Color.FromRgb(232, 96, 60);

                cardBackground = Color.FromRgb(227, 246, 248);
                theme.Cards.Background = cardBackground.Value;

                theme.ValidationError = Color.FromRgb(211, 47, 47);
            }
            else if (season == "christmas" && _isDarkTheme)
            {
                // Christmas night: dark pine background, red header, gold accents.
                theme.SetBaseTheme(BaseTheme.Dark);

                theme.Background = Color.FromRgb(12, 26, 18);
                theme.Foreground = Color.FromRgb(241, 248, 243);

                theme.SetPrimaryColor(Color.FromRgb(229, 57, 53));
                theme.PrimaryLight = Color.FromRgb(239, 108, 108);
                theme.PrimaryMid = Color.FromRgb(229, 57, 53);
                theme.PrimaryDark = Color.FromRgb(183, 28, 28);

                theme.SetSecondaryColor(Color.FromRgb(255, 201, 40));
                theme.SecondaryLight = Color.FromRgb(255, 224, 130);
                theme.SecondaryMid = Color.FromRgb(255, 201, 40);
                theme.SecondaryDark = Color.FromRgb(255, 160, 0);

                cardBackground = Color.FromRgb(21, 41, 29);
                theme.Cards.Background = cardBackground.Value;

                theme.ValidationError = Color.FromRgb(255, 107, 107);
            }
            else if (season == "christmas")
            {
                // Snowy day: cool white background, pine-green header, red accents.
                theme.SetBaseTheme(BaseTheme.Light);

                theme.Background = Color.FromRgb(244, 248, 251);
                theme.Foreground = Color.FromRgb(30, 42, 34);

                theme.SetPrimaryColor(Color.FromRgb(27, 94, 32));
                theme.PrimaryLight = Color.FromRgb(46, 125, 50);
                theme.PrimaryMid = Color.FromRgb(27, 94, 32);
                theme.PrimaryDark = Color.FromRgb(13, 59, 18);

                theme.SetSecondaryColor(Color.FromRgb(211, 47, 47));
                theme.SecondaryLight = Color.FromRgb(239, 83, 80);
                theme.SecondaryMid = Color.FromRgb(211, 47, 47);
                theme.SecondaryDark = Color.FromRgb(183, 28, 28);

                cardBackground = Color.FromRgb(255, 253, 247);
                theme.Cards.Background = cardBackground.Value;

                theme.ValidationError = Color.FromRgb(211, 47, 47);
            }
            else if (_isDarkTheme)
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

            if (Application.Current != null)
            {
                var resources = Application.Current.Resources;
                resources[CardBackgroundKey] = Frozen(cardBackground ?? theme.Cards.Background);

                if (season == "classic")
                {
                    foreach (var key in LegacyBrushKeys) resources.Remove(key);
                }
                else
                {
                    var colors = new[]
                    {
                        theme.PrimaryLight.Color, theme.PrimaryMid.Color, theme.PrimaryDark.Color,
                        theme.SecondaryLight.Color, theme.SecondaryMid.Color, theme.SecondaryDark.Color,
                    };
                    for (int i = 0; i < LegacyBrushKeys.Length; i++)
                        resources[LegacyBrushKeys[i]] = Frozen(colors[i]);
                }
            }

            if (season != ActiveSeason || _appliedIsDark != _isDarkTheme)
            {
                ActiveSeason = season;
                _appliedIsDark = _isDarkTheme;
                SeasonChanged?.Invoke(this, season);
            }
        }

        private static SolidColorBrush Frozen(Color color)
        {
            var brush = new SolidColorBrush(color);
            brush.Freeze();
            return brush;
        }

        public Color GetThemeColor()
        {
            return IsDarkTheme ? Color.FromRgb(18, 18, 18) : Color.FromRgb(250, 250, 250);
        }
    }
}
