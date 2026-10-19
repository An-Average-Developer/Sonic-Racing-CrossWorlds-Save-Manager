#pragma once
#include <string>
#include "Graphics/Color.h"
#include "Graphics/Graphics.h"
#include "Seasonal/SeasonalTheme.h"

// ThemeService.cs + the MaterialDesign brushes the XAML used. The C# toggled dark (default) and light
// modes and re-themed both for the active season; every widget reads its colors from here.
namespace SonicRacingSaveManager::UI::Material {

using Season = Seasonal::Season;

// A MaterialDesign ColorPair: the color and the contrasting text color on it.
struct ColorPair {
    Color color;
    Color foreground;
};

// The MaterialDesign brushes (MaterialDesign.Brush.*), named as in the toolkit.
struct Palette {
    bool baseDark = false;          // BaseTheme.Dark / Light (drives every derived brush below)
    Color background;               // MaterialDesignPaper
    Color foreground;               // MaterialDesignBody
    Color foregroundLight;          // MaterialDesignBodyLight / CheckBox.Off / TextBox.Border
    Color divider;                  // MaterialDesignDivider (TextBox.HoverBackground)
    Color cardBackground;           // SeasonalCardBackgroundBrush: the implicit Card style in App.xaml uses it for every Card
    Color toolkitCard;              // theme.Cards.Background (MaterialDesignCardBackground: popups, menus)
    Color cardBorder;
    ColorPair primaryLight, primaryMid, primaryDark;
    ColorPair secondaryLight, secondaryMid, secondaryDark;
    Color validationError;
    // Base-theme brushes
    Color textBoxOutlineBorder, textBoxOutlineInactiveBorder, textBoxHoverBorder, textBoxFilledBackground;
    Color comboOutlineBorder, comboOutlineInactiveBorder, comboHoverBorder;
    Color comboPopupBackground;     // light popup background (ComboBox.Popup.LightBackground on light, Dark on dark)
    Color checkBoxUncheckedBorder, checkBoxDisabled;
    Color switchTrackOff;
    Color flatClick, buttonRipple;
    Color separator;
    Color scrollBarForeground;
    Color snackbarBackground, snackbarMouseOver;
    Color toolTipBackground;
    Color listBoxItemSelected, listViewHover;
    Color tabDivider;
    // App-specific (App.xaml resources set by ThemeService)
    Color sectionHeader;            // section titles: the primary color, or the season's own
    bool sectionHeaderHalo = false; // SeasonalSectionHeaderEffect: dark halo while the scenery is drawn
    Color themeBlue = Hex(0x2196F3);    // ThemeBlueBrush
    Color themeYellow = Hex(0xFFEB3B);  // ThemeYellowBrush
};

// Current palette (rebuilt when the mode or the season changes).
const Palette& P();

// Dark mode = the C#'s default (classic greys). Light = purple/lime. Seasons use their own pair.
bool IsDarkTheme();
void SetDarkTheme(bool dark);
void ToggleTheme();
// Re-read the season after the season theme setting changed.
void RefreshSeason();
Season ActiveSeason();
std::wstring SeasonName(Season s);     // "classic" | "halloween" | "summer" | "christmas"
// The night scenery (SeasonalBackdrop) is only drawn for dark seasonal themes.
bool SceneryVisible();
// Increments whenever the palette changes (screens can cache derived values).
unsigned Revision();

// MaterialDesign ColorAssist.ContrastingForegroundColor
Color ContrastingForeground(Color c);

// ── Typography (MaterialDesign TextBlock styles; Segoe UI, see Graphics/Fonts.h) ──
TextStyle Headline4();   // 34 Regular
TextStyle Headline5();   // 24 Regular
TextStyle Headline6();   // 20 Medium
TextStyle Subtitle1();   // 16 Regular
TextStyle Subtitle2();   // 14 Medium
TextStyle Body1();       // 16 Regular
TextStyle Body2();       // 14 Regular
TextStyle Caption();     // 12 Regular
TextStyle Text(float size, DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL);
inline TextStyle Medium(float size) { return Text(size, DWRITE_FONT_WEIGHT_MEDIUM); }
inline TextStyle SemiBold(float size) { return Text(size, DWRITE_FONT_WEIGHT_SEMI_BOLD); }
inline TextStyle Bold(float size) { return Text(size, DWRITE_FONT_WEIGHT_BOLD); }

} // namespace SonicRacingSaveManager::UI::Material
