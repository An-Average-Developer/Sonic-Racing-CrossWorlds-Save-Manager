#include "Windows/Main/Tabs/Steam/SteamTab.h"
#include <algorithm>
#include <cmath>
#include "App/Paths.h"
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "Services/Platforms/SteamShortcutService.h"
#include "UI/Material/Card.h"
#include "UI/Material/Motion.h"
#include "UI/Material/PackIcon.h"
#include "UI/Material/Theme.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Tabs/Steam/SteamOverlay.h"
#include "Windows/Main/Tabs/TabBanner.h"
#include "Windows/Main/Tabs/TabKit.h"

namespace SonicRacingSaveManager::Main {

namespace {

namespace M = UI::Material;
using TabKit::Style;
using TabKit::TextH;

constexpr float kRightW = 360.f, kGap = 16.f, kRadius = 16.f, kPad = 20.f;
const Color kSteamBlue = Rgba(0x66, 0xC0, 0xF4), kSteamMid = Rgba(0x2A, 0x47, 0x5E), kSteamDark = Rgba(0x17, 0x1A, 0x21), kGreen = Rgba(0x00, 0xC8, 0x53),
            kInfoBlue = Rgba(0x64, 0xB5, 0xF6);

ImagePtr Art(const wchar_t* file) { return Images::Load(Paths::Asset(std::wstring(L"Steam\\") + file)); }

// A picture in a rounded frame with a caption under it, lifting on hover
void Thumb(UI::Ui& ui, const char* key, const Rect& r, const ImagePtr& image, const std::wstring& caption, Stretch stretch) {
    const UI::Id id = ui.MakeId(key);
    const UI::Interaction it = ui.Interact(id, r);
    const float hover = M::Motion::Hover(ui, id, it.hovered, false).hover;
    const Rect frame{ r.x, r.y - 3.f * hover, r.w, r.h - 22.f };
    if (hover > 0.f) ui.g.GlowRound(frame, 10.f, kSteamBlue, 12.f, 0.4f * hover);
    ui.g.FillRound(frame, 10.f, kSteamDark);
    if (image) ui.g.DrawImageRounded(*image, frame, 10.f, 1.f, stretch);
    ui.g.StrokeRound(frame.Inset(0.5f), 9.5f, kSteamBlue.Fade(0.25f + 0.4f * hover), 1.f);
    TextStyle t = Style(11.f, DWRITE_FONT_WEIGHT_NORMAL, false, HAlign::Center);
    t.valign = VAlign::Center;
    ui.g.Text(caption, { r.x, r.B() - 18.f, r.w, 18.f }, M::P().foreground.Fade(0.6f), t);
}

} // namespace

SteamTab::SteamTab(MainWindow& window) : Tab(window), overlay(std::make_unique<AddToSteamOverlay>(window)) { window.AddOverlay(overlay.get()); }

SteamTab::~SteamTab() = default;

void SteamTab::OnSelected() {
    loadedAt = -1.0;
    steamRunning = Services::SteamShortcuts::IsSteamRunning();
}

void SteamTab::Draw(UI::Ui& ui, const Rect& area) {
    if (loadedAt < 0.0) {
        loadedAt = ui.Time();
        steamRunning = Services::SteamShortcuts::IsSteamRunning();
    }
    const Rect grid = area.Inset(20.f);
    const Rect banner{ grid.x, grid.y, grid.w, kTabBannerHeight };
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt, 0, 0.f, 0.5f), 14.f);
        TabBannerSpec spec;
        spec.icon = M::Icon::Steam;
        spec.motion = TabBannerSpec::Motion::Spin;
        spec.title = Loc::Get("steam_title");
        spec.subtitle = Loc::Get("steam_subtitle");
        spec.pill = Loc::Get(steamRunning ? "steam_pill_running" : "steam_pill_not_running");
        spec.pillIcon = steamRunning ? M::Icon::CheckCircle : M::Icon::InformationOutline;
        TabBanner(ui, banner, spec, loadedAt);
    }
    const float top = banner.B() + kGap;
    const Rect left{ grid.x, top, grid.w - kRightW - kGap, grid.B() - top };
    const Rect right{ left.R() + kGap, top, kRightW, grid.B() - top };
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.1, 0, 0.f, 0.5f), 18.f);
        Preview(ui, left);
    }
    const float statusH = StatusCard(ui, right, false);
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.18, 0, 0.f, 0.5f), 0.f, 28.f);
        StatusCard(ui, { right.x, right.y, right.w, statusH }, true);
    }
    const float y = right.y + statusH + kGap;
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.26, 0, 0.f, 0.5f), 0.f, 28.f);
        TabKit::StepsCard(ui, { right.x, y, right.w, right.B() - y }, "st-what", Loc::Get("steam_what_title"),
                          { Loc::Get("steam_what_step1"), Loc::Get("steam_what_step2"), Loc::Get("steam_what_step3"), Loc::Get("steam_what_step4") },
                          loadedAt + 0.4);
    }
}

// "Shortcut icon & library artwork preview": the hero banner with the logo on it, the icon tile with
// the app name, the capsule and the poster; Add to Steam under them
void SteamTab::Preview(UI::Ui& ui, const Rect& r) {
    const M::Palette& p = M::P();
    const float t = static_cast<float>(ui.Time());
    M::CardLook look;
    look.radius = kRadius;
    M::Card card(ui, ui.MakeId("st-preview"), r, Edges(24.f, kPad), false, 1.f, look);
    const Rect c = card.content;
    TextStyle caps = Style(11.f, DWRITE_FONT_WEIGHT_BOLD);
    caps.valign = VAlign::Center;
    ui.g.Text(Loc::Get("steam_preview_caps"), { c.x, c.y, c.w, 20.f }, p.primaryMid.color, caps);
    float y = c.y + 28.f;

    constexpr float kButtonH = 52.f;
    const float available = c.B() - kButtonH - 18.f - y;
    // Hero: the banner (3840x1240 in Steam, so about 3:1) with the logo floating over it
    const float heroH = std::min(available * 0.55f, c.w / 3.1f);
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.2, 0, 0.f, 0.45f), 12.f);
        const Rect hero{ c.x, y, c.w, heroH };
        ui.g.FillRound(hero, 14.f, kSteamDark);
        if (auto banner = Art(L"Banner.jpg")) ui.g.DrawImageRounded(*banner, hero, 14.f, 1.f, Stretch::UniformToFill);
        ui.g.FillLinear(hero, 14.f, { { 0.f, Colors::Black.Fade(0.f) }, { 0.6f, Colors::Black.Fade(0.1f) }, { 1.f, Colors::Black.Fade(0.55f) } }, { 0.f, 0.f },
                        { 0.f, 1.f });
        if (auto logo = Art(L"Logo.png")) {
            const float lw = std::min(hero.w * 0.42f, 360.f), lh = hero.h * 0.5f;
            const float bob = 3.f * std::sin(t * 1.6f);
            ui.g.DrawImage(*logo, { hero.x + 26.f, hero.B() - lh - 18.f + bob, lw, lh }, 1.f, Stretch::Uniform);
            ui.KeepAnimatingThrottled();
        }
        // The shine passing over the hero every few seconds
        const float sweep = static_cast<float>(std::fmod(ui.Time(), 6.0) / 1.2);
        if (sweep < 1.f) {
            ui.g.PushRoundClip(hero, 14.f);
            const float sx = hero.x - 160.f + (hero.w + 320.f) * Anim::EaseInOutCubic(sweep);
            ui.g.PushTransform(D2D1::Matrix3x2F::Skew(-22.f, 0.f, D2D1::Point2F(sx, hero.CY())));
            ui.g.FillLinear({ sx, hero.y, 140.f, hero.h }, 0.f, { { 0.f, Colors::White.Fade(0.f) }, { 0.5f, Colors::White.Fade(0.16f) }, { 1.f, Colors::White.Fade(0.f) } },
                            { 0.f, 0.f }, { 1.f, 0.f });
            ui.g.PopTransform();
            ui.g.PopLayer();
            ui.KeepAnimating();
        }
        ui.g.StrokeRound(hero.Inset(0.5f), 13.5f, kSteamBlue.Fade(0.3f), 1.f);
        y += heroH + 16.f;
    }
    // The icon tile + name, the capsule and the poster
    {
        const float rowH = std::max(0.f, c.B() - kButtonH - 18.f - y);
        const float tileS = std::min(rowH - 26.f, 120.f);
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.3, 0, 0.f, 0.45f), 12.f);
        if (tileS > 40.f) {
            const Rect tile{ c.x, y, tileS, tileS };
            ui.g.GlowRound(tile, 20.f, kSteamBlue, 18.f, 0.35f + 0.12f * std::sin(t * 2.f));
            ui.g.FillLinear(tile, 20.f, { { 0.f, kSteamMid }, { 1.f, kSteamDark } }, { 0.f, 0.f }, { 1.f, 1.f });
            if (auto icon = Art(L"icons\\iconHD.png")) ui.g.DrawImage(*icon, tile.Inset(12.f), 1.f, Stretch::Uniform);
            ui.g.StrokeRound(tile.Inset(0.75f), 19.25f, kSteamBlue.Fade(0.6f), 1.5f);
            TextStyle name = Style(15.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
            name.valign = VAlign::Center;
            name.wrap = true;
            TextStyle hint = Style(11.5f);
            hint.valign = VAlign::Center;
            const float nx = tile.R() + 16.f, nw = std::max(120.f, c.w * 0.38f - tileS);
            ui.g.Text(Services::SteamShortcuts::AppName, { nx, tile.CY() - 34.f, nw, 44.f }, p.foreground, name);
            ui.g.Text(Loc::Get("steam_preview_hint"), { nx, tile.CY() + 12.f, nw, 20.f }, p.foreground.Fade(0.6f), hint);
            // Capsule (920x430) and poster (600x900) on the right
            const float posterW = rowH * 600.f / 900.f * 0.86f;
            const float capsuleW = std::min((rowH - 22.f) * 920.f / 430.f, c.R() - (nx + nw + 16.f) - posterW - 12.f);
            if (capsuleW > 60.f) {
                const float capsuleH = capsuleW * 430.f / 920.f + 22.f;
                Thumb(ui, "st-capsule", { c.R() - posterW - 12.f - capsuleW, y + (rowH - capsuleH) * 0.5f, capsuleW, capsuleH }, Art(L"Capsule.jpg"),
                      Loc::Get("steam_capsule"), Stretch::UniformToFill);
                Thumb(ui, "st-poster", { c.R() - posterW, y, posterW, rowH }, Art(L"Poster.jpg"), Loc::Get("steam_poster"), Stretch::UniformToFill);
            }
        }
    }
    // Add to Steam
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.4, 0, 0.f, 0.45f), 12.f);
        if (TabKit::PillButton(ui, ui.MakeId("st-add"), { c.x, c.B() - kButtonH, c.w, kButtonH }, M::Icon::Steam, Loc::Get("steam_add_title"), kSteamBlue, true)) {
            // AddToSteamBtn_Click: the warning follows whether Steam is running right now
            steamRunning = Services::SteamShortcuts::IsSteamRunning();
            overlay->Show(steamRunning);
        }
    }
}

// RefreshStatus: Steam running (green check: it will be closed and reopened) or not (blue info: added
// directly), and the info about what the shortcut points at
float SteamTab::StatusCard(UI::Ui& ui, const Rect& r, bool draw) {
    const std::wstring& status = Loc::Get(steamRunning ? "steam_status_running" : "steam_status_not_running");
    const std::wstring& info = Loc::Get("steam_info");
    const TextStyle body = Style(12.f, DWRITE_FONT_WEIGHT_NORMAL, true);
    const float w = r.w - kPad * 2.f;
    const float statusH = TextH(ui, status, body, w - 50.f);
    const float infoH = TextH(ui, info, body, w - 30.f);
    const float h = kPad * 2.f + 26.f + std::max(44.f, statusH + 20.f) + 14.f + infoH;
    if (!draw) return h;
    const M::Palette& p = M::P();
    const float t = static_cast<float>(ui.Time());
    M::CardLook look;
    look.radius = kRadius;
    M::Card card(ui, ui.MakeId("st-status"), r, Edges(kPad), false, 1.f, look);
    const Rect c = card.content;
    TextStyle caps = Style(11.f, DWRITE_FONT_WEIGHT_BOLD);
    caps.valign = VAlign::Center;
    ui.g.Text(Loc::Get("steam_status_caps"), { c.x, c.y, c.w, 20.f }, p.primaryMid.color, caps);
    float y = c.y + 26.f;
    const Color color = steamRunning ? kGreen : kInfoBlue;
    const Rect box{ c.x, y, c.w, std::max(44.f, statusH + 20.f) };
    ui.g.FillRound(box, 12.f, color.Fade(0.12f));
    const float pulse = 0.5f + 0.5f * std::sin(t * 2.5f);
    ui.KeepAnimatingThrottled();
    ui.g.FillEllipse(box.x + 22.f, box.CY(), 13.f + 2.f * pulse, 13.f + 2.f * pulse, color.Fade(0.15f));
    M::DrawIcon(ui.g, steamRunning ? M::Icon::CheckCircleOutline : M::Icon::InformationOutline, box.x + 22.f, box.CY(), 22.f, color);
    ui.g.Text(status, { box.x + 44.f, box.CY() - statusH * 0.5f, box.w - 50.f, statusH }, TabKit::Readable(color), body);
    y += box.h + 14.f;
    M::DrawIcon(ui.g, M::Icon::InformationOutline, c.x + 9.f, y + 9.f, 16.f, p.foreground.Fade(0.6f));
    ui.g.Text(info, { c.x + 28.f, y, c.w - 30.f, infoH }, p.foreground.Fade(0.75f), body);
    return h;
}

} // namespace SonicRacingSaveManager::Main
