#include "UI/Material/Theme.h"
#include <cmath>

namespace SonicRacingSaveManager::UI::Material {

namespace {

bool darkTheme = true;   // ThemeService starts dark (App.OnStartup sets IsDarkTheme = true)
Season season = Season::Classic;
Palette palette;
unsigned revision = 0;
bool built = false;

constexpr Color Rgb(int r, int g, int b) { return Rgba(r, g, b); }

ColorPair Pair(Color c) { return { c, ContrastingForeground(c) }; }

// SetBaseTheme(Light / Dark): the MaterialDesignTheme.Light / .Dark brushes
void BaseTheme(Palette& p, bool dark) {
    p.baseDark = dark;
    if (!dark) {
        p.background = Argb(0xFFF1F1F1);
        p.foreground = Argb(0xE5000000);
        p.foregroundLight = Argb(0x7F000000);
        p.divider = Argb(0x19000000);
        p.toolkitCard = Argb(0xFFFFFFFF);
        p.cardBorder = Argb(0xFFB9B9B9);
        p.validationError = Argb(0xFFB00020);
        p.textBoxOutlineBorder = Argb(0xB2000000);
        p.textBoxOutlineInactiveBorder = Argb(0x33000000);
        p.textBoxHoverBorder = Argb(0xE5000000);
        p.textBoxFilledBackground = Argb(0x0C000000);
        p.comboOutlineBorder = Argb(0xB2000000);
        p.comboOutlineInactiveBorder = Argb(0x33000000);
        p.comboHoverBorder = Argb(0xE5000000);
        p.comboPopupBackground = Argb(0xFFF1F1F1);
        p.checkBoxUncheckedBorder = Argb(0x19000000);
        p.checkBoxDisabled = Argb(0xFFB9B9B9);
        p.switchTrackOff = Argb(0xFFFFFFFF);
        p.flatClick = Argb(0xFFD8D8D8);
        p.buttonRipple = Argb(0xFFB9B9B9);
        p.separator = Argb(0xFFD8D8D8);
        p.scrollBarForeground = Argb(0xFFD8D8D8);
        p.snackbarBackground = Argb(0xFF323232);
        p.snackbarMouseOver = Argb(0xFF454545);
        p.toolTipBackground = Argb(0xFF757575);
        p.listBoxItemSelected = Argb(0x19000000);
        p.listViewHover = Argb(0x19000000);
        p.tabDivider = Argb(0x19000000);
    } else {
        p.background = Argb(0xFF323232);
        p.foreground = Argb(0xE5FFFFFF);
        p.foregroundLight = Argb(0x7FFFFFFF);
        p.divider = Argb(0x19FFFFFF);
        p.toolkitCard = Argb(0xFF454545);
        p.cardBorder = Argb(0xFF868686);
        p.validationError = Argb(0xFFCF6679);
        p.textBoxOutlineBorder = Argb(0xB2FFFFFF);
        p.textBoxOutlineInactiveBorder = Argb(0x33FFFFFF);
        p.textBoxHoverBorder = Argb(0xE5FFFFFF);
        p.textBoxFilledBackground = Argb(0x19FFFFFF);
        p.comboOutlineBorder = Argb(0xB2FFFFFF);
        p.comboOutlineInactiveBorder = Argb(0x33FFFFFF);
        p.comboHoverBorder = Argb(0xE5FFFFFF);
        p.comboPopupBackground = Argb(0xFF323232);
        p.checkBoxUncheckedBorder = Argb(0x19FFFFFF);
        p.checkBoxDisabled = Argb(0xFF616161);
        p.switchTrackOff = Argb(0xFF454545);
        p.flatClick = Argb(0x19757575);
        p.buttonRipple = Argb(0xFFB9B9B9);
        p.separator = Argb(0xFF757575);
        p.scrollBarForeground = Argb(0xFF757575);
        p.snackbarBackground = Argb(0xFFD8D8D8);
        p.snackbarMouseOver = Argb(0xFFB9B9B9);
        p.toolTipBackground = Argb(0xFFF1F1F1);
        p.listBoxItemSelected = Argb(0x19FFFFFF);
        p.listViewHover = Argb(0x19FFFFFF);
        p.tabDivider = Argb(0x19FFFFFF);
    }
}

void Primary(Palette& p, Color light, Color mid, Color dark) {
    p.primaryLight = Pair(light);
    p.primaryMid = Pair(mid);
    p.primaryDark = Pair(dark);
}

void Secondary(Palette& p, Color light, Color mid, Color dark) {
    p.secondaryLight = Pair(light);
    p.secondaryMid = Pair(mid);
    p.secondaryDark = Pair(dark);
}

// ThemeService.ApplyTheme, one branch per season / mode
void Build() {
    season = Seasonal::Active();
    Seasonal::SetDarkMode(darkTheme);
    Palette p;
    Color cardBackground = Rgb(66, 66, 66);   // #424242 (SeasonalCardBackgroundBrush default in App.xaml)
    bool headerSet = false;
    Color sectionHeader;

    if (season == Season::Halloween && darkTheme) {
        BaseTheme(p, true);
        p.background = Rgb(18, 10, 28);      // near-black purple
        p.foreground = Rgb(243, 233, 255);
        // Light enough to read as text on the near-black background
        Primary(p, Rgb(195, 155, 255), Rgb(155, 93, 229), Rgb(106, 47, 181));
        Secondary(p, Rgb(255, 167, 77), Rgb(255, 122, 26), Rgb(214, 84, 0));
        cardBackground = Rgb(36, 20, 56);
        sectionHeader = Rgb(255, 154, 60);   // pumpkin orange
        headerSet = true;
        p.validationError = Rgb(255, 82, 82);
    } else if (season == Season::Halloween) {
        BaseTheme(p, false);
        p.background = Rgb(244, 234, 251);   // pale lavender
        p.foreground = Rgb(42, 18, 56);
        Primary(p, Rgb(155, 93, 229), Rgb(106, 47, 181), Rgb(74, 30, 120));
        Secondary(p, Rgb(255, 167, 77), Rgb(255, 122, 26), Rgb(214, 84, 0));
        cardBackground = Rgb(255, 233, 210);  // soft pumpkin
        p.validationError = Rgb(211, 47, 47);
    } else if (season == Season::Summer && darkTheme) {
        // Tropical night: deep ocean blues with sunny yellow accents
        BaseTheme(p, true);
        p.background = Rgb(7, 30, 38);
        p.foreground = Rgb(230, 251, 255);
        Primary(p, Rgb(102, 217, 232), Rgb(16, 152, 173), Rgb(11, 114, 133));
        Secondary(p, Rgb(255, 209, 102), Rgb(255, 182, 39), Rgb(240, 140, 0));
        cardBackground = Rgb(15, 52, 65);
        sectionHeader = Rgb(255, 209, 102);  // sunny yellow
        headerSet = true;
        p.validationError = Rgb(255, 107, 107);
    } else if (season == Season::Summer) {
        // Beach day: warm sand background, light sea-foam cards, ocean and coral accents
        BaseTheme(p, false);
        p.background = Rgb(255, 244, 220);
        p.foreground = Rgb(31, 45, 58);
        Primary(p, Rgb(16, 152, 173), Rgb(11, 114, 133), Rgb(9, 92, 107));
        Secondary(p, Rgb(255, 160, 122), Rgb(255, 127, 80), Rgb(232, 96, 60));
        cardBackground = Rgb(227, 246, 248);
        p.validationError = Rgb(211, 47, 47);
    } else if (season == Season::Christmas && darkTheme) {
        // Christmas night: dark pine background, red header, gold accents
        BaseTheme(p, true);
        p.background = Rgb(12, 26, 18);
        p.foreground = Rgb(241, 248, 243);
        Primary(p, Rgb(239, 108, 108), Rgb(229, 57, 53), Rgb(183, 28, 28));
        Secondary(p, Rgb(255, 224, 130), Rgb(255, 201, 40), Rgb(255, 160, 0));
        cardBackground = Rgb(21, 41, 29);
        sectionHeader = Rgb(255, 213, 79);   // gold
        headerSet = true;
        p.validationError = Rgb(255, 107, 107);
    } else if (season == Season::Christmas) {
        // Snowy day: cool white background, pine-green header, red accents
        BaseTheme(p, false);
        p.background = Rgb(244, 248, 251);
        p.foreground = Rgb(30, 42, 34);
        Primary(p, Rgb(46, 125, 50), Rgb(27, 94, 32), Rgb(13, 59, 18));
        Secondary(p, Rgb(239, 83, 80), Rgb(211, 47, 47), Rgb(183, 28, 28));
        cardBackground = Rgb(255, 253, 247);
        sectionHeader = Rgb(198, 40, 40);    // red
        headerSet = true;
        p.validationError = Rgb(211, 47, 47);
    } else if (darkTheme) {
        // Classic dark (default): greys on the dark base theme
        BaseTheme(p, true);
        p.background = Rgb(18, 18, 18);
        p.foreground = Rgb(245, 245, 245);
        Primary(p, Rgb(230, 230, 230), Rgb(180, 180, 180), Rgb(150, 150, 150));
        Secondary(p, Rgb(190, 190, 190), Rgb(140, 140, 140), Rgb(120, 120, 120));
        p.validationError = Rgb(180, 180, 180);
        cardBackground = p.toolkitCard;   // the base theme's card color
    } else {
        // Classic light: purple / lime
        BaseTheme(p, false);
        p.background = Rgb(250, 250, 250);
        p.foreground = Rgb(33, 33, 33);
        Primary(p, Rgb(179, 157, 219), Rgb(103, 58, 183), Rgb(77, 40, 140));
        Secondary(p, Rgb(220, 231, 117), Rgb(205, 220, 57), Rgb(175, 180, 43));
        p.validationError = Rgb(211, 47, 47);
        cardBackground = p.toolkitCard;
    }

    // Seasonal branches also set theme.Cards.Background; classic keeps the base theme's
    if (season != Season::Classic) p.toolkitCard = cardBackground;
    p.cardBackground = cardBackground;
    // Section titles: the primary color, or the season's own color
    p.sectionHeader = headerSet ? sectionHeader : p.primaryMid.color;
    p.sectionHeaderHalo = season != Season::Classic && darkTheme;

    palette = p;
    built = true;
    ++revision;
}

} // namespace

const Palette& P() {
    if (!built) Build();
    return palette;
}

bool IsDarkTheme() { return darkTheme; }

void SetDarkTheme(bool dark) {
    darkTheme = dark;
    Build();
}

void ToggleTheme() { SetDarkTheme(!darkTheme); }

void RefreshSeason() {
    Seasonal::Refresh();
    if (!built || Seasonal::Active() != season) Build();
}

Season ActiveSeason() {
    if (!built) Build();
    return season;
}

std::wstring SeasonName(Season s) { return Seasonal::Name(s); }

bool SceneryVisible() { return ActiveSeason() != Season::Classic && darkTheme; }

unsigned Revision() {
    if (!built) Build();
    return revision;
}

Color ContrastingForeground(Color c) {
    const auto lin = [](float v) { return v > 0.03928f ? std::pow((v + 0.055f) / 1.055f, 2.4f) : v / 12.92f; };
    const double luminance = 0.2126 * lin(c.r) + 0.7152 * lin(c.g) + 0.0722 * lin(c.b);
    return luminance > 0.179 ? Colors::Black : Colors::White;
}

TextStyle Text(float size, DWRITE_FONT_WEIGHT weight) {
    TextStyle s;
    s.size = size;
    s.font = Font::Ui;
    s.weight = weight;
    return s;
}

TextStyle Headline4() { return Text(34.f); }
TextStyle Headline5() { return Text(24.f); }
TextStyle Headline6() { return Text(20.f, DWRITE_FONT_WEIGHT_MEDIUM); }
TextStyle Subtitle1() { return Text(16.f); }
TextStyle Subtitle2() { return Text(14.f, DWRITE_FONT_WEIGHT_MEDIUM); }
TextStyle Body1() { return Text(16.f); }
TextStyle Body2() { return Text(14.f); }
TextStyle Caption() { return Text(12.f); }

} // namespace SonicRacingSaveManager::UI::Material
