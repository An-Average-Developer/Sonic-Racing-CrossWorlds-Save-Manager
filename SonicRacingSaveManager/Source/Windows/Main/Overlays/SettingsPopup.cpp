#include <algorithm>
#include "Localization/Localization.h"
#include "Settings/AppSettings.h"
#include "Settings/SeasonThemeSettings.h"
#include "UI/Material/Feedback.h"
#include "UI/Material/Motion.h"
#include "UI/Material/Theme.h"
#include "UI/Material/Toggles.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Overlays/HeaderPopups.h"

namespace SonicRacingSaveManager::Main {

namespace {

namespace M = UI::Material;

struct ThemeOption {
    const wchar_t* tag;
    const char* text;   // translation key
};
constexpr ThemeOption kThemes[] = {
    { L"default", "theme_default" },     { L"classic", "theme_classic" },     { L"summer", "theme_summer" },
    { L"halloween", "theme_halloween" }, { L"christmas", "theme_christmas" },
};

constexpr float kPadding = 10.f;
constexpr float kRowH = 18.f;            // CheckBox / RadioButton height (18 px mark)
constexpr float kDescriptionWidth = 220.f;

TextStyle Body() { return M::Text(12.f); }   // the window's default FontSize
TextStyle DescriptionStyle() {
    TextStyle t = M::Text(11.f);
    t.wrap = true;
    t.valign = VAlign::Top;
    return t;
}

} // namespace

void SettingsPopup::OnOpened() { showLoadingScreen = Settings::LocalSettings::GetShowLoadingScreen(); }

float SettingsPopup::Width(UI::Ui& ui) {
    float content = kDescriptionWidth;
    content = std::max(content, M::CheckBoxWidth(ui, Loc::Get("settings_loading_screen"), Body()));
    content = std::max(content, M::CheckBoxWidth(ui, Loc::Get("settings_discord_activity"), Body()));
    content = std::max(content, ui.g.Measure(Loc::Get("settings_season_theme"), M::SemiBold(12.f)).w);
    return content + 10.f + kPadding * 2.f + 4.f;
}

float SettingsPopup::Height(UI::Ui& ui, float width) {
    (void)width;
    float h = kRowH + 10.f;                                                   // loading screen checkbox
    h += kRowH + 10.f;                                                        // Discord checkbox
    h += 1.f + 16.f;                                                          // Separator Margin 5,8
    h += 16.f + 2.f;                                                          // "Season Theme"
    h += ui.g.Measure(Loc::Get("settings_season_description"), DescriptionStyle(), kDescriptionWidth).h + 4.f;
    h += (kRowH + 6.f) * static_cast<float>(std::size(kThemes));              // radios, Margin 5,3
    return h + kPadding * 2.f + 4.f;
}

void SettingsPopup::DrawContent(UI::Ui& ui, const Rect& box) {
    const M::Palette& p = M::P();
    const Color fg = p.foreground;   // TextElement.Foreground = MaterialDesignBody
    const Rect inner = box.Inset(kPadding);
    const float x = inner.x + 5.f, w = inner.w - 10.f;
    float y = inner.y;
    int row = 0;   // the rows slide in one by one while the popup opens

    // Show Loading Screen (saved right away)
    {
        M::Motion::Enter enter(ui, RowReveal(ui, row++), 0.f, 16.f);
        if (M::CheckBox(ui, ui.MakeId("set-loading"), { x, y + 5.f, w, kRowH }, showLoadingScreen, Loc::Get("settings_loading_screen"), Body(), fg)) {
            showLoadingScreen = !showLoadingScreen;
            Settings::LocalSettings::SaveShowLoadingScreen(showLoadingScreen);
        }
    }
    y += kRowH + 10.f;

    // Show Activity on Discord (new in the C++ port: the C# always showed it)
    {
        M::Motion::Enter enter(ui, RowReveal(ui, row++), 0.f, 16.f);
        auto& discord = main.DiscordSettings();
        if (M::CheckBox(ui, ui.MakeId("set-discord"), { x, y + 5.f, w, kRowH }, discord.showActivity, Loc::Get("settings_discord_activity"), Body(),
                        fg)) {
            discord.showActivity = !discord.showActivity;
            discord.Save();
            main.Presence().ApplySettings(discord.showActivity);
        }
        y += kRowH + 10.f;
    }

    // Season Theme (SeasonThemePanel)
    {
        M::Motion::Enter enter(ui, RowReveal(ui, row++), 0.f, 16.f);
        M::Separator(ui, x, y + 8.f, w);
        y += 17.f;
        TextStyle heading = M::SemiBold(12.f);
        heading.valign = VAlign::Top;
        ui.g.Text(Loc::Get("settings_season_theme"), { x, y, w, 16.f }, fg, heading);
        y += 18.f;
        const TextStyle description = DescriptionStyle();
        const std::wstring& text = Loc::Get("settings_season_description");
        const float dh = ui.g.Measure(text, description, kDescriptionWidth).h;
        ui.g.Text(text, { x, y, kDescriptionWidth, dh }, fg.Fade(0.6f), description);
        y += dh + 4.f;
    }

    const std::wstring current = Settings::SeasonTheme::Current().theme;
    for (const auto& option : kThemes) {
        const bool checked = current == option.tag;
        M::Motion::Enter enter(ui, RowReveal(ui, row++), 0.f, 16.f);
        if (M::RadioButton(ui, ui.MakeId("set-theme", reinterpret_cast<intptr_t>(option.tag)), { x, y + 3.f, w, kRowH }, checked,
                           Loc::Get(option.text), Body(), fg) &&
            !checked) {
            auto& settings = Settings::SeasonTheme::Current();
            settings.theme = option.tag;
            settings.Save();
            main.ApplySeasonTheme();   // ThemeService.Instance.RefreshSeason()
        }
        y += kRowH + 6.f;
    }
}

} // namespace SonicRacingSaveManager::Main
