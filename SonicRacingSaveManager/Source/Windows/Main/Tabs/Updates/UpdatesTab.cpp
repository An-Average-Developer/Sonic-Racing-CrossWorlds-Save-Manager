#include "Windows/Main/Tabs/Updates/UpdatesTab.h"
#include <algorithm>
#include <cmath>
#include <cwctype>
#include "App/Config.h"
#include "App/Paths.h"
#include "Core/Async.h"
#include "Core/Process.h"
#include "Localization/Localization.h"
#include "Graphics/Animation.h"
#include "Graphics/Images.h"
#include "Services/App/AppVersion.h"
#include "Services/App/VersionCompare.h"
#include "UI/Material/Card.h"
#include "UI/Material/Elevation.h"
#include "UI/Material/Motion.h"
#include "UI/Material/PackIcon.h"
#include "UI/Material/Theme.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Tabs/TabBanner.h"
#include "Windows/Main/Tabs/TabKit.h"

namespace SonicRacingSaveManager::Main {

namespace M = UI::Material;
using namespace TabKit;
using TabKit::PillButton;
using TabKit::PillWidth;

namespace {

constexpr float kRightW = 360.f, kGap = 16.f, kRadius = 16.f;
const Color kGreen = Argb(0xFF4CAF50), kBlue = Rgba(33, 150, 243), kOrange = Argb(0xFFF4821A), kGrey = Argb(0xFF9E9E9E),
            kRed = Argb(0xFFF4511E);

TextStyle Caps() {
    TextStyle t = Style(11.f, DWRITE_FONT_WEIGHT_BOLD);
    t.valign = VAlign::Center;
    return t;
}

std::wstring WithV(const std::wstring& version) {
    if (!version.empty() && std::towlower(version[0]) == L'v') return version;
    return L"v" + version;
}

bool Blank(const std::wstring& text) {
    return std::all_of(text.begin(), text.end(), [](wchar_t c) { return std::iswspace(c) != 0; });
}

// "Label  value" inside a soft rounded box
void InfoChip(UI::Ui& ui, const Rect& r, M::Icon icon, const std::wstring& label, const std::wstring& value) {
    const M::Palette& p = M::P();
    ui.g.FillRound(r, 12.f, p.foreground.Fade(0.05f));
    M::DrawIcon(ui.g, icon, r.x + 20.f, r.CY(), 18.f, p.primaryMid.color);
    TextStyle tiny = Style(10.f);
    tiny.valign = VAlign::Center;
    TextStyle big = Style(13.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    big.valign = VAlign::Center;
    big.ellipsis = true;
    ui.g.Text(label, { r.x + 38.f, r.y + 6.f, r.w - 44.f, 16.f }, p.foreground.Fade(0.55f), tiny);
    ui.g.Text(value, { r.x + 38.f, r.y + 20.f, r.w - 44.f, r.h - 26.f }, p.foreground, big);
}

} // namespace

void UpdatesTab::Draw(UI::Ui& ui, const Rect& area) {
    MainState& s = main.State();
    // Switching between the overview, the installation and the manual update views plays the
    // entrance again
    const int view = s.showingInstallationView ? 1 : manualView ? 2 : 0;
    if (view != shownView) {
        shownView = view;
        loadedAt = -1.0;
    }
    if (loadedAt < 0.0) loadedAt = ui.Time();
    const Rect grid = area.Inset(20.f);
    if (view == 1) DrawInstallation(ui, grid);
    else if (view == 2) DrawManual(ui, grid);
    else DrawOverview(ui, grid);
}

// ── Overview ──────────────────────────────────────────────────────────────────────────────────

void UpdatesTab::DrawOverview(UI::Ui& ui, const Rect& grid) {
    MainState& s = main.State();
    const Rect banner{ grid.x, grid.y, grid.w, kTabBannerHeight };
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt, 0, 0.f, 0.5f), 14.f);
        TabBannerSpec spec;
        spec.icon = M::Icon::CloudDownload;
        spec.motion = TabBannerSpec::Motion::Float;
        spec.title = Loc::Get("updates_title");
        spec.subtitle = Loc::Get("updates_subtitle");
        spec.pill = Loc::Fmt("updates_installed_pill", s.currentVersion);
        spec.pillIcon = M::Icon::CheckCircle;
        TabBanner(ui, banner, spec, loadedAt);
    }
    const float top = banner.B() + kGap;
    const Rect left{ grid.x, top, grid.w - kRightW - kGap, grid.B() - top };
    const Rect right{ left.R() + kGap, top, kRightW, grid.B() - top };
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.1, 0, 0.f, 0.5f), 18.f);
        StatusHero(ui, left);
    }
    float y = right.y;
    const Rect version{ right.x, y, right.w, VersionCard(ui, { right.x, y, right.w, 0.f }, false) };
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.18, 0, 0.f, 0.5f), 0.f, 28.f);
        VersionCard(ui, version, true);
    }
    y = version.B() + kGap;
    const Rect sources{ right.x, y, right.w, SourcesCard(ui, { right.x, y, right.w, 0.f }, false) };
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.26, 0, 0.f, 0.5f), 0.f, 28.f);
        SourcesCard(ui, sources, true);
    }
    y = sources.B() + kGap;
    {
        const std::wstring steps[] = { Loc::Get("updates_how_step1"), Loc::Get("updates_how_step2"), Loc::Get("updates_how_step3") };
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.34, 0, 0.f, 0.5f), 0.f, 28.f);
        HowCard(ui, { right.x, y, right.w, right.B() - y }, "upd-how", Loc::Get("updates_how_title"), steps, 3);
    }
}

// The big status: an orb (spinning while checking, a check when up to date, a bouncing cloud when an
// update is available), title, message and the actions
void UpdatesTab::StatusHero(UI::Ui& ui, const Rect& r) {
    MainState& s = main.State();
    const M::Palette& p = M::P();
    const Color fg = p.foreground;
    const float t = static_cast<float>(ui.Time());
    M::CardLook look;
    look.radius = kRadius;
    M::Card card(ui, ui.MakeId("upd-hero"), r, Edges(28.f), false, 1.f, look);
    const Rect c = card.content;

    enum class State { Unknown, Checking, UpToDate, Available, GameBanana };
    State state = State::Unknown;
    if (s.Checking()) state = State::Checking;
    else if (s.isUpdateAvailable) state = State::Available;
    else if (s.isGbUpdateAvailable) state = State::GameBanana;
    else if (s.hasCheckedForUpdates) state = State::UpToDate;
    const Color color = state == State::Available ? kGreen : state == State::GameBanana ? kOrange : state == State::UpToDate ? kBlue : kGrey;

    // The state pops in whenever it changes
    struct StateAnim {
        int state = -1;
        double since = -10.0;
    };
    StateAnim& anim = ui.Data<StateAnim>(ui.MakeId("upd-hero-state"));
    if (anim.state != static_cast<int>(state)) {
        anim.state = static_cast<int>(state);
        anim.since = ui.Time();
    }
    const float pop = Anim::EaseOutBack(Anim::Clamp01(static_cast<float>((ui.Time() - anim.since) / 0.5)), 0.8f);
    if (ui.Time() - anim.since < 0.5) ui.KeepAnimating();
    ui.KeepAnimatingThrottled();

    // Orb
    const float ocx = c.CX(), ocy = c.y + 92.f, orb = 54.f * (0.85f + 0.15f * pop);
    const float ring = static_cast<float>(std::fmod(ui.Time(), 2.0) / 2.0);
    if (state == State::Available || state == State::GameBanana || state == State::UpToDate) {
        for (int i = 0; i < 2; ++i) {
            const float k = std::fmod(ring + 0.5f * static_cast<float>(i), 1.f);
            ui.g.StrokeEllipse(ocx, ocy, orb + 30.f * k, orb + 30.f * k, color.Fade(0.4f * (1.f - k)), 2.f);
        }
    }
    ui.g.GlowRound({ ocx - orb, ocy - orb, orb * 2.f, orb * 2.f }, orb, color, 24.f, 0.45f);
    ui.g.FillRadial({ ocx - orb, ocy - orb, orb * 2.f, orb * 2.f }, { { 0.f, Lerp(color, Colors::White, 0.35f) }, { 1.f, color } });
    if (state == State::Checking) {
        for (int i = 0; i < 2; ++i) TabKit::Arc(ui.g, ocx, ocy, orb + 10.f, t * 300.f + 180.f * static_cast<float>(i), 80.f, color, 3.f);
        ui.KeepAnimating();
    }
    M::Icon icon = M::Icon::Information;
    float bob = 0.f, spin = 0.f;
    switch (state) {
        case State::Checking: icon = M::Icon::CloudSearch; spin = 8.f * std::sin(t * 6.f); break;
        case State::UpToDate: icon = M::Icon::CheckCircle; break;
        case State::Available: icon = M::Icon::CloudDownload; bob = 5.f * std::sin(t * 3.f); break;
        case State::GameBanana: icon = M::Icon::Web; bob = 5.f * std::sin(t * 3.f); break;
        default: break;
    }
    ui.g.PushTransform(D2D1::Matrix3x2F::Rotation(spin, D2D1::Point2F(ocx, ocy)) * D2D1::Matrix3x2F::Translation(0.f, bob));
    M::DrawIcon(ui.g, icon, ocx, ocy, orb * 1.05f * pop, Colors::White);
    ui.g.PopTransform();

    // Title + message
    std::wstring title, message;
    switch (state) {
        case State::Checking: title = Loc::Get("updates_checking_title"); message = Loc::Get("updates_checking_message"); break;
        case State::UpToDate: title = Loc::Get("updates_up_to_date_title"); message = Loc::Fmt("updates_up_to_date_message", s.currentVersion); break;
        case State::Available: title = Loc::Fmt("updates_available_title", s.latestVersion); message = Loc::Get("updates_available_message"); break;
        case State::GameBanana:
            title = Loc::Get("updates_gb_title");
            message = Loc::Fmt("updates_gb_message", s.gbLatestVersion);
            break;
        default: title = Loc::Get("updates_check_title"); message = Loc::Get("updates_check_message"); break;
    }
    float y = ocy + orb + 34.f;
    {
        M::Motion::Enter enter(ui, pop, 10.f);
        TextStyle head = Style(24.f, DWRITE_FONT_WEIGHT_BOLD, false, HAlign::Center);
        head.valign = VAlign::Center;
        ui.g.Text(title, { c.x, y, c.w, 34.f }, fg, head);
        TextStyle body = Style(14.f, DWRITE_FONT_WEIGHT_NORMAL, true, HAlign::Center);
        const float bh = TextH(ui, message, body, c.w - 80.f);
        ui.g.Text(message, { c.x + 40.f, y + 40.f, c.w - 80.f, bh }, fg.Fade(0.65f), body);
        y += 40.f + bh + 24.f;
    }

    // Details of the new version
    if (state == State::Available) {
        const float chipW = std::min(220.f, (c.w - 24.f) / 3.f);
        const float x0 = c.CX() - (chipW * 3.f + 24.f) * 0.5f;
        M::Motion::Enter enter(ui, pop, 10.f);
        InfoChip(ui, { x0, y, chipW, 52.f }, M::Icon::StarCircle, Loc::Get("updates_latest_version"),
                 s.latestVersion.empty() ? Loc::Get("updates_not_available") : s.latestVersion);
        InfoChip(ui, { x0 + chipW + 12.f, y, chipW, 52.f }, M::Icon::FileDocumentOutline, Loc::Get("updates_file_name"), s.updateFileName);
        InfoChip(ui, { x0 + (chipW + 12.f) * 2.f, y, chipW, 52.f }, M::Icon::Download, Loc::Get("updates_file_size"), s.updateFileSize);
        y += 52.f + 24.f;
    }

    // Actions: Update now (the green status banner of the C#) / GameBanana, Check for updates
    {
        M::Motion::Enter enter(ui, pop, 10.f);
        std::vector<std::pair<std::wstring, int>> buttons;   // label, kind
        if (state == State::Available) buttons.push_back({ Loc::Get("updates_update_now"), 0 });
        if (s.isGbUpdateAvailable) buttons.push_back({ Loc::Get("updates_install_from_gb"), 1 });
        buttons.push_back({ Loc::Get(state == State::Checking ? "updates_checking_button" : "updates_check_button"), 2 });
        float total = 0.f;
        for (const auto& b : buttons) total += PillWidth(ui, b.first) + 12.f;
        float x = c.CX() - (total - 12.f) * 0.5f;
        for (const auto& [label, kind] : buttons) {
            const float w = PillWidth(ui, label);
            const Rect br{ x, y, w, 46.f };
            x += w + 12.f;
            if (kind == 0) {
                if (PillButton(ui, ui.MakeId("upd-now"), br, M::Icon::Download, label, kGreen, true)) s.ShowInstallationView();
            } else if (kind == 1) {
                if (PillButton(ui, ui.MakeId("upd-gb"), br, M::Icon::Web, label, kOrange, true)) s.ShowGbInstallationView();
            } else {
                const float angle = state == State::Checking ? std::fmod(t * 360.f, 360.f) : 0.f;
                if (PillButton(ui, ui.MakeId("upd-check"), br, M::Icon::Refresh, label, p.primaryMid.color, state == State::Unknown, state != State::Checking,
                               angle))
                    s.CheckForUpdates();
            }
        }
        y += 46.f + 18.f;
    }

    // Download progress (started from the installation view)
    if (s.isDownloading) {
        const float barW = std::min(480.f, c.w - 40.f);
        const Rect bar{ c.CX() - barW * 0.5f, y, barW, 10.f };
        ui.g.FillRound(bar, 5.f, fg.Fade(0.08f));
        const float shown = ui.Animate(ui.MakeId("upd-hero-progress"), s.downloadProgress / 100.f, 8.f, 0.f);
        ui.g.FillRound({ bar.x, bar.y, std::max(10.f, bar.w * shown), bar.h }, 5.f, kGreen);
        TextStyle caption = Style(12.f, DWRITE_FONT_WEIGHT_NORMAL, false, HAlign::Center);
        ui.g.Text(s.downloadProgressText, { c.x, y + 16.f, c.w, 18.f }, fg.Fade(0.7f), caption);
    }

    // Manual update link at the bottom
    {
        TextStyle link = Style(12.f, DWRITE_FONT_WEIGHT_NORMAL, false, HAlign::Center);
        link.valign = VAlign::Center;
        const std::wstring& text = Loc::Get("updates_manual");
        const float lw = std::ceil(ui.g.Measure(text, link).w) + 30.f;
        const Rect lr{ c.CX() - lw * 0.5f, c.B() - 30.f, lw, 30.f };
        const UI::Id id = ui.MakeId("upd-manual");
        const UI::Interaction it = ui.Interact(id, lr);
        if (it.hovered) ui.SetCursor(IDC_HAND);
        const float hover = M::Motion::Hover(ui, id, it.hovered, false).hover;
        M::DrawIcon(ui.g, M::Icon::Wrench, lr.x + 8.f, lr.CY(), 14.f, Lerp(fg.Fade(0.55f), p.primaryMid.color, hover));
        TextStyle l = link;
        l.underline = hover > 0.5f;
        ui.g.Text(text, { lr.x + 20.f, lr.y, lw - 20.f, lr.h }, Lerp(fg.Fade(0.55f), p.primaryMid.color, hover), l);
        if (it.clicked) OpenManual();
    }
}

// Your version, big, with the update state under it
float UpdatesTab::VersionCard(UI::Ui& ui, const Rect& r, bool draw) {
    constexpr float kH = 20.f * 2.f + 20.f + 10.f + 48.f + 6.f + 20.f;
    if (!draw) return kH;
    MainState& s = main.State();
    const M::Palette& p = M::P();
    const Color fg = p.foreground;
    M::CardLook look;
    look.radius = kRadius;
    M::Card card(ui, ui.MakeId("upd-version"), r, Edges(20.f), false, 1.f, look);
    const Rect c = card.content;
    ui.g.Text(Loc::Get("updates_your_version"), { c.x, c.y, c.w, 20.f }, p.primaryMid.color, Caps());
    TextStyle big = Style(38.f, DWRITE_FONT_WEIGHT_BLACK);
    big.valign = VAlign::Center;
    ui.g.Text(s.currentVersion, { c.x, c.y + 30.f, c.w, 48.f }, fg, big);
    const bool upToDate = s.hasCheckedForUpdates && !s.HasAnyUpdateAvailable();
    const Color dot = s.HasAnyUpdateAvailable() ? kGreen : upToDate ? kBlue : kGrey;
    const std::wstring& state = Loc::Get(s.HasAnyUpdateAvailable() ? "updates_state_newer" : upToDate ? "updates_state_latest" : "updates_state_not_checked");
    const float k = static_cast<float>(std::fmod(ui.Time(), 1.6) / 1.6);
    const float dy = c.y + 30.f + 48.f + 6.f + 10.f;
    ui.g.StrokeEllipse(c.x + 6.f, dy, 4.f + 6.f * k, 4.f + 6.f * k, dot.Fade(0.6f * (1.f - k)), 1.5f);
    ui.g.FillEllipse(c.x + 6.f, dy, 4.f, 4.f, dot);
    ui.KeepAnimatingThrottled();
    TextStyle tiny = Style(12.f);
    tiny.valign = VAlign::Center;
    ui.g.Text(state, { c.x + 18.f, dy - 10.f, c.w - 18.f, 20.f }, fg.Fade(0.7f), tiny);
    return kH;
}

// GitHub and GameBanana, clickable rows
float UpdatesTab::SourcesCard(UI::Ui& ui, const Rect& r, bool draw) {
    constexpr float kH = 20.f * 2.f + 20.f + 10.f + 2.f * 52.f + 8.f;
    if (!draw) return kH;
    MainState& s = main.State();
    const M::Palette& p = M::P();
    const Color fg = p.foreground;
    M::CardLook look;
    look.radius = kRadius;
    M::Card card(ui, ui.MakeId("upd-sources"), r, Edges(20.f), false, 1.f, look);
    const Rect c = card.content;
    ui.g.Text(Loc::Get("updates_sources"), { c.x, c.y, c.w, 20.f }, p.primaryMid.color, Caps());
    float y = c.y + 30.f;
    const auto Row = [&](const char* key, const wchar_t* name, const std::wstring& hint, const wchar_t* logo, M::Icon icon, Color tint) {
        const Rect row{ c.x, y, c.w, 48.f };
        y += 52.f;
        const UI::Id id = ui.MakeId(key);
        const UI::Interaction it = ui.Interact(id, row);
        if (it.hovered) ui.SetCursor(IDC_HAND);
        const float hover = M::Motion::Hover(ui, id, it.hovered, it.held).hover;
        ui.g.FillRound(row, 12.f, Lerp(fg.Fade(0.04f), tint.Fade(0.14f), hover));
        ui.g.FillEllipse(row.x + 24.f, row.CY(), 16.f, 16.f, Colors::White);
        if (logo) {
            if (auto image = Images::Load(Paths::Asset(logo))) ui.g.DrawImage(*image, { row.x + 13.f, row.CY() - 11.f, 22.f, 22.f }, 1.f);
        } else {
            M::DrawIcon(ui.g, icon, row.x + 24.f, row.CY(), 20.f, Rgba(36, 41, 47));
        }
        TextStyle n = Style(13.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        n.valign = VAlign::Center;
        TextStyle h = Style(11.f);
        h.valign = VAlign::Center;
        ui.g.Text(name, { row.x + 50.f, row.y + 6.f, row.w - 90.f, 18.f }, fg, n);
        ui.g.Text(hint, { row.x + 50.f, row.y + 24.f, row.w - 90.f, 18.f }, fg.Fade(0.55f), h);
        M::DrawIcon(ui.g, M::Icon::ChevronRight, row.R() - 20.f + 4.f * hover, row.CY(), 20.f, Lerp(fg.Fade(0.4f), tint, hover));
        return it.clicked;
    };
    if (Row("upd-src-github", L"GitHub", Loc::Get("updates_source_github_hint"), nullptr, M::Icon::Web, p.primaryMid.color)) s.OpenGitHub();
    if (Row("upd-src-gb", L"GameBanana", Loc::Get("updates_source_gb_hint"), L"Logos\\Gamebanana.png", M::Icon::Web, kOrange))
        Process::OpenUrl(Config::GameBanana::ToolPage);
    return kH;
}

// Numbered steps joined by a line
void UpdatesTab::HowCard(UI::Ui& ui, const Rect& r, const char* key, const std::wstring& title, const std::wstring* steps, int count) {
    const M::Palette& p = M::P();
    const Color fg = p.foreground;
    M::CardLook look;
    look.radius = kRadius;
    M::Card card(ui, ui.MakeId(key), r, Edges(20.f), false, 1.f, look);
    const Rect c = card.content;
    ui.g.Text(title, { c.x, c.y, c.w, 20.f }, p.primaryMid.color, Caps());
    float y = c.y + 34.f;
    const TextStyle text = Style(12.f, DWRITE_FONT_WEIGHT_NORMAL, true);
    for (int i = 0; i < count && y < c.B(); ++i) {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.45, i, 0.08f, 0.4f), 0.f, 16.f);
        const float th = std::max(26.f, TextH(ui, steps[i], text, c.w - 40.f));
        ui.g.FillEllipse(c.x + 13.f, y + 13.f, 13.f, 13.f, p.primaryMid.color.Fade(0.15f));
        TextStyle num = Style(11.f, DWRITE_FONT_WEIGHT_BOLD, false, HAlign::Center);
        num.valign = VAlign::Center;
        ui.g.Text(std::to_wstring(i + 1), { c.x, y, 26.f, 26.f }, ChangelogCards::ReadableColor(p.primaryMid.color), num);
        ui.g.Text(steps[i], { c.x + 38.f, y + (th > 26.f ? 0.f : 5.f), c.w - 40.f, th }, fg.Fade(0.8f), text);
        if (i < count - 1) ui.g.FillRect({ c.x + 12.f, y + 28.f, 2.f, th - 8.f }, p.primaryMid.color.Fade(0.2f));
        y += th + 16.f;
    }
}

// ── Installation view ─────────────────────────────────────────────────────────────────────────

void UpdatesTab::DrawInstallation(UI::Ui& ui, const Rect& grid) {
    MainState& s = main.State();
    const Rect banner{ grid.x, grid.y, grid.w, kTabBannerHeight };
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt, 0, 0.f, 0.5f), 14.f);
        TabBannerSpec spec;
        spec.icon = M::Icon::Download;
        spec.motion = TabBannerSpec::Motion::Float;
        spec.title = Loc::Get("updates_ready_title");
        spec.subtitle = Loc::Fmt("updates_ready_subtitle", s.installTitle, s.latestVersion);
        spec.pill = s.currentVersion + L"  \x2192  " + s.latestVersion;
        spec.pillIcon = M::Icon::CloudDownload;
        TabBanner(ui, banner, spec, loadedAt);
    }
    const float top = banner.B() + kGap;
    const Rect left{ grid.x, top, grid.w - kRightW - kGap, grid.B() - top };
    const Rect right{ left.R() + kGap, top, kRightW, grid.B() - top };
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.1, 0, 0.f, 0.5f), 18.f);
        M::CardLook look;
        look.radius = kRadius;
        M::Card card(ui, ui.MakeId("upd-install-news"), left, Edges(20.f), false, 1.f, look);
        changelog.Draw(ui, card.content, s.changelog, s.currentVersion, s.latestVersion);
    }
    constexpr float kInstallH = 400.f;
    const Rect install{ right.x, right.y, right.w, std::min(kInstallH, right.h) };
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.18, 0, 0.f, 0.5f), 0.f, 28.f);
        InstallCard(ui, install);
    }
    if (right.B() - install.B() > 120.f) {
        const std::wstring steps[] = { Loc::Get("updates_next_step1"), Loc::Get("updates_next_step2"), Loc::Get("updates_next_step3") };
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.26, 0, 0.f, 0.5f), 0.f, 28.f);
        HowCard(ui, { right.x, install.B() + kGap, right.w, right.B() - install.B() - kGap }, "upd-next", Loc::Get("updates_next_title"), steps, 3);
    }
}

// From → to, the file, Download & Install / Cancel, or the progress ring while downloading
void UpdatesTab::InstallCard(UI::Ui& ui, const Rect& r) {
    MainState& s = main.State();
    const M::Palette& p = M::P();
    const Color fg = p.foreground;
    const float t = static_cast<float>(ui.Time());
    M::CardLook look;
    look.radius = kRadius;
    M::Card card(ui, ui.MakeId("upd-install"), r, Edges(20.f), false, 1.f, look);
    const Rect c = card.content;
    ui.g.Text(Loc::Get(s.isGbInstallation ? "updates_install_from_gb_caps" : "updates_install_from_github_caps"), { c.x, c.y, c.w, 20.f }, p.primaryMid.color, Caps());
    float y = c.y + 34.f;

    // vOld → vNew, the arrow sliding along
    {
        TextStyle v = Style(18.f, DWRITE_FONT_WEIGHT_BOLD, false, HAlign::Center);
        v.valign = VAlign::Center;
        const float boxW = (c.w - 50.f) * 0.5f;
        const Rect from{ c.x, y, boxW, 56.f }, to{ c.R() - boxW, y, boxW, 56.f };
        ui.g.FillRound(from, 12.f, fg.Fade(0.05f));
        ui.g.FillRound(to, 12.f, kGreen.Fade(0.14f));
        ui.g.StrokeRound(to.Inset(0.75f), 11.25f, kGreen.Fade(0.6f), 1.5f);
        TextStyle cap = Style(10.f, DWRITE_FONT_WEIGHT_NORMAL, false, HAlign::Center);
        ui.g.Text(Loc::Get("updates_installed"), { from.x, from.y + 6.f, from.w, 14.f }, fg.Fade(0.55f), cap);
        ui.g.Text(Loc::Get("updates_new"), { to.x, to.y + 6.f, to.w, 14.f }, ChangelogCards::ReadableColor(kGreen), cap);
        ui.g.Text(s.currentVersion, { from.x, from.y + 18.f, from.w, 34.f }, fg.Fade(0.8f), v);
        ui.g.Text(s.latestVersion, { to.x, to.y + 18.f, to.w, 34.f }, fg, v);
        const float slide = 6.f * std::sin(t * 3.f);
        ui.KeepAnimatingThrottled();
        M::DrawIcon(ui.g, M::Icon::ArrowRight, c.CX() + slide * 0.5f, from.CY(), 24.f, kGreen);
        y += 56.f + 14.f;
    }
    InfoChip(ui, { c.x, y, c.w, 52.f }, M::Icon::FileDocumentOutline, Loc::Get("updates_file_name"), s.updateFileName);
    y += 52.f + 10.f;
    InfoChip(ui, { c.x, y, c.w, 52.f }, M::Icon::Download, Loc::Get("updates_file_size"), s.updateFileSize);
    y += 52.f + 20.f;

    if (s.isDownloading) {
        // Progress ring
        const float shown = ui.Animate(ui.MakeId("upd-install-progress"), s.downloadProgress / 100.f, 8.f, 0.f);
        const float rcx = c.CX(), rcy = std::min(y + 56.f, c.B() - 70.f), radius = 46.f;
        ui.g.StrokeEllipse(rcx, rcy, radius, radius, fg.Fade(0.08f), 8.f);
        TabKit::Arc(ui.g, rcx, rcy, radius, 0.f, std::max(2.f, 360.f * shown), kGreen, 8.f);
        const float a = (360.f * shown) * 3.14159265f / 180.f;
        ui.g.FillEllipse(rcx + radius * std::sin(a), rcy - radius * std::cos(a), 6.f, 6.f, Colors::White);
        TextStyle pct = Style(20.f, DWRITE_FONT_WEIGHT_BOLD, false, HAlign::Center);
        pct.valign = VAlign::Center;
        ui.g.Text(std::to_wstring(s.downloadProgress) + L"%", { rcx - radius, rcy - 14.f, radius * 2.f, 28.f }, fg, pct);
        TextStyle caption = Style(12.f, DWRITE_FONT_WEIGHT_NORMAL, false, HAlign::Center);
        ui.g.Text(s.downloadProgressText, { c.x, rcy + radius + 12.f, c.w, 18.f }, fg.Fade(0.7f), caption);
        ui.KeepAnimatingThrottled();
        return;
    }
    const Rect accept{ c.x, y, c.w, 48.f };
    if (PillButton(ui, ui.MakeId("upd-accept"), accept, M::Icon::Download, s.installButtonLabel, s.isGbInstallation ? kOrange : kGreen, true))
        s.AcceptInstallation();
    y += 48.f + 10.f;
    if (PillButton(ui, ui.MakeId("upd-cancel"), { c.x, y, c.w, 42.f }, M::Icon::Close, Loc::Get("cancel"), fg.Fade(0.8f), false)) s.CancelInstallation();
}

// ── Manual update view ────────────────────────────────────────────────────────────────────────

void UpdatesTab::OpenManual() {
    manualView = true;
    if (manual != Manual::Downloading) ManualCheck();
}

void UpdatesTab::ManualCheck() {
    manual = Manual::Checking;
    manualRelease.reset();
    Async::Run([] { return Services::Updates::LatestReleaseInfo(); }, [this](Services::Updates::UpdateInfo info) {
        manual = Blank(info.downloadUrl) ? Manual::Unreachable : Manual::Ready;
        manualRelease = std::move(info);
    });
}

void UpdatesTab::ManualDownload() {
    if (!manualRelease || manual == Manual::Downloading) return;
    manual = Manual::Downloading;
    manualProgress = 0;
    Async::Run(
        [this] {
            return Services::Updates::DownloadAndInstallManagerExe([this](int percent) { Async::Post([this, percent] { manualProgress = percent; }); });
        },
        [this](bool success) {
            if (!success) manual = Manual::Failed;
        });
}

void UpdatesTab::ManualOpenBrowser() {
    const bool hasUrl = manualRelease && !Blank(manualRelease->releaseUrl);
    Process::OpenUrl(hasUrl ? manualRelease->releaseUrl : AppVersion::GitHubRepoUrl() + L"/releases/latest");
}

void UpdatesTab::DrawManual(UI::Ui& ui, const Rect& grid) {
    MainState& s = main.State();
    const Rect banner{ grid.x, grid.y, grid.w, kTabBannerHeight };
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt, 0, 0.f, 0.5f), 14.f);
        TabBannerSpec spec;
        spec.icon = M::Icon::Wrench;
        spec.motion = TabBannerSpec::Motion::Wobble;
        spec.title = Loc::Get("manual_update_title");
        spec.subtitle = Loc::Get("manual_update_subtitle");
        spec.pill = Loc::Fmt("updates_installed_pill", s.currentVersion);
        spec.pillIcon = M::Icon::CheckCircle;
        TabBanner(ui, banner, spec, loadedAt);
    }
    const float top = banner.B() + kGap;
    const Rect left{ grid.x, top, grid.w - kRightW - kGap, grid.B() - top };
    const Rect right{ left.R() + kGap, top, kRightW, grid.B() - top };
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.1, 0, 0.f, 0.5f), 18.f);
        ManualHero(ui, left);
    }
    const Rect release{ right.x, right.y, right.w, ManualReleaseCard(ui, { right.x, right.y, right.w, 0.f }, false) };
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.18, 0, 0.f, 0.5f), 0.f, 28.f);
        ManualReleaseCard(ui, release, true);
    }
    const float y = release.B() + kGap;
    {
        const std::wstring steps[] = { Loc::Get("manual_update_when1"), Loc::Get("manual_update_when2"), Loc::Get("manual_update_when3") };
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.26, 0, 0.f, 0.5f), 0.f, 28.f);
        HowCard(ui, { right.x, y, right.w, right.B() - y }, "upd-manual-when", Loc::Get("manual_update_when_title"), steps, 3);
    }
}

// Back button, the orb (searching arcs / bouncing cloud / shaking alert / progress ring around the
// percentage), title, message, the release's details and the actions
void UpdatesTab::ManualHero(UI::Ui& ui, const Rect& r) {
    const M::Palette& p = M::P();
    const Color fg = p.foreground;
    const float t = static_cast<float>(ui.Time());
    M::CardLook look;
    look.radius = kRadius;
    M::Card card(ui, ui.MakeId("upd-manual-hero"), r, Edges(28.f), false, 1.f, look);
    const Rect c = card.content;

    const bool error = manual == Manual::Unreachable || manual == Manual::Failed;
    const Color color = error ? kRed : manual == Manual::Checking ? p.primaryMid.color : kGreen;
    struct StateAnim {
        int state = -1;
        double since = -10.0;
    };
    StateAnim& anim = ui.Data<StateAnim>(ui.MakeId("upd-manual-state"));
    if (anim.state != static_cast<int>(manual)) {
        anim.state = static_cast<int>(manual);
        anim.since = ui.Time();
    }
    const float since = static_cast<float>(ui.Time() - anim.since);
    const float pop = Anim::EaseOutBack(Anim::Clamp01(since / 0.5f), 0.8f);
    if (since < 0.6f) ui.KeepAnimating();
    ui.KeepAnimatingThrottled();

    // Back to the overview (not while it's installing)
    {
        const std::wstring& label = Loc::Get("manual_update_back");
        const Rect br{ c.x, c.y, PillWidth(ui, label) - 20.f, 38.f };
        if (PillButton(ui, ui.MakeId("upd-manual-back"), br, M::Icon::ArrowLeft, label, fg.Fade(0.8f), false, manual != Manual::Downloading))
            manualView = false;
    }

    // Orb
    const float ocx = c.CX(), ocy = c.y + 120.f, orb = 54.f * (0.85f + 0.15f * pop);
    const float shake = error && since < 0.5f ? 10.f * (1.f - since / 0.5f) * std::sin(since * 50.f) : 0.f;
    if (manual == Manual::Ready) {
        const float ring = static_cast<float>(std::fmod(ui.Time(), 2.0) / 2.0);
        for (int i = 0; i < 2; ++i) {
            const float k = std::fmod(ring + 0.5f * static_cast<float>(i), 1.f);
            ui.g.StrokeEllipse(ocx, ocy, orb + 30.f * k, orb + 30.f * k, color.Fade(0.4f * (1.f - k)), 2.f);
        }
    }
    ui.PushTranslate(shake, 0.f);
    ui.g.GlowRound({ ocx - orb, ocy - orb, orb * 2.f, orb * 2.f }, orb, color, 24.f, 0.45f);
    ui.g.FillRadial({ ocx - orb, ocy - orb, orb * 2.f, orb * 2.f }, { { 0.f, Lerp(color, Colors::White, 0.35f) }, { 1.f, color } });
    if (manual == Manual::Checking) {
        for (int i = 0; i < 2; ++i) TabKit::Arc(ui.g, ocx, ocy, orb + 10.f, t * 300.f + 180.f * static_cast<float>(i), 80.f, color, 3.f);
        ui.KeepAnimating();
    }
    if (manual == Manual::Downloading) {
        // Progress ring around the orb, the percentage inside
        const float shown = ui.Animate(ui.MakeId("upd-manual-progress"), manualProgress / 100.f, 8.f, 0.f);
        const float radius = orb + 14.f;
        ui.g.StrokeEllipse(ocx, ocy, radius, radius, fg.Fade(0.08f), 6.f);
        TabKit::Arc(ui.g, ocx, ocy, radius, 0.f, std::max(2.f, 360.f * shown), kGreen, 6.f);
        const float a = (360.f * shown) * 3.14159265f / 180.f;
        ui.g.FillEllipse(ocx + radius * std::sin(a), ocy - radius * std::cos(a), 5.f, 5.f, Colors::White);
        TextStyle pct = Style(26.f, DWRITE_FONT_WEIGHT_BOLD, false, HAlign::Center);
        pct.valign = VAlign::Center;
        ui.g.Text(std::to_wstring(manualProgress) + L"%", { ocx - orb, ocy - 18.f, orb * 2.f, 36.f }, Colors::White, pct);
        ui.KeepAnimating();
    } else {
        M::Icon icon = M::Icon::CloudSearch;
        float bob = 0.f, spin = 0.f;
        switch (manual) {
            case Manual::Checking: spin = 8.f * std::sin(t * 6.f); break;
            case Manual::Ready: icon = M::Icon::CloudDownload; bob = 5.f * std::sin(t * 3.f); break;
            default: icon = M::Icon::AlertCircle; break;
        }
        ui.g.PushTransform(D2D1::Matrix3x2F::Rotation(spin, D2D1::Point2F(ocx, ocy)) * D2D1::Matrix3x2F::Translation(0.f, bob));
        M::DrawIcon(ui.g, icon, ocx, ocy, orb * 1.05f * pop, Colors::White);
        ui.g.PopTransform();
    }
    ui.PopTransform();

    // Title + message
    std::wstring title, message;
    switch (manual) {
        case Manual::Checking: title = Loc::Get("manual_update_checking"); message = Loc::Get("manual_update_checking_message"); break;
        case Manual::Ready:
            title = Loc::Fmt("manual_update_ready", WithV(manualRelease->latestVersion));
            message = Loc::Get("manual_update_ready_message");
            break;
        case Manual::Unreachable: title = Loc::Get("manual_update_unreachable_title"); message = Loc::Get("manual_update_unreachable_message"); break;
        case Manual::Downloading:
            title = manualProgress < 100 ? Loc::Fmt("updates_downloading", manualProgress) : Loc::Get("updates_installing");
            message = Loc::Get("updates_how_step3");
            break;
        case Manual::Failed: title = Loc::Get("manual_update_failed_title"); message = Loc::Get("manual_update_failed_message"); break;
    }
    float y = ocy + orb + 34.f;
    {
        M::Motion::Enter enter(ui, pop, 10.f);
        TextStyle head = Style(24.f, DWRITE_FONT_WEIGHT_BOLD, false, HAlign::Center);
        head.valign = VAlign::Center;
        ui.g.Text(title, { c.x, y, c.w, 34.f }, fg, head);
        TextStyle body = Style(14.f, DWRITE_FONT_WEIGHT_NORMAL, true, HAlign::Center);
        const float bh = TextH(ui, message, body, c.w - 80.f);
        ui.g.Text(message, { c.x + 40.f, y + 40.f, c.w - 80.f, bh }, fg.Fade(0.65f), body);
        y += 40.f + bh + 24.f;
    }

    // The release: version, file and size
    if (manualRelease && !Blank(manualRelease->downloadUrl)) {
        const float chipW = std::min(220.f, (c.w - 24.f) / 3.f);
        const float x0 = c.CX() - (chipW * 3.f + 24.f) * 0.5f;
        for (int i = 0; i < 3; ++i) {
            M::Motion::Enter enter(ui, M::Motion::Stagger(ui, anim.since + 0.1, i, 0.07f, 0.35f), 12.f);
            const Rect chip{ x0 + (chipW + 12.f) * static_cast<float>(i), y, chipW, 52.f };
            if (i == 0) InfoChip(ui, chip, M::Icon::StarCircle, Loc::Get("updates_latest_version"), WithV(manualRelease->latestVersion));
            else if (i == 1) InfoChip(ui, chip, M::Icon::FileDocumentOutline, Loc::Get("updates_file_name"), manualRelease->fileName);
            else InfoChip(ui, chip, M::Icon::Download, Loc::Get("updates_file_size"), Services::Updates::FormatFileSize(manualRelease->fileSize));
        }
        y += 52.f + 24.f;
    }

    // Actions
    if (manual == Manual::Downloading) return;
    M::Motion::Enter enter(ui, pop, 10.f);
    const std::wstring& browser = Loc::Get("manual_update_open_browser");
    const bool retry = error;
    const std::wstring& primary = Loc::Get(retry ? "manual_update_retry" : "updates_download_install");
    const float pw = PillWidth(ui, primary), bw = PillWidth(ui, browser);
    float x = c.CX() - (pw + 12.f + bw) * 0.5f;
    const Rect primaryRect{ x, y, pw, 46.f };
    x += pw + 12.f;
    const float angle = manual == Manual::Checking ? std::fmod(t * 360.f, 360.f) : 0.f;
    if (PillButton(ui, ui.MakeId("upd-manual-go"), primaryRect, retry ? M::Icon::Refresh : M::Icon::Download, primary, kGreen, true,
                   manual != Manual::Checking, angle)) {
        if (manual == Manual::Unreachable) ManualCheck();
        else ManualDownload();
    }
    if (PillButton(ui, ui.MakeId("upd-manual-browser"), { x, y, bw, 46.f }, M::Icon::Web, browser, p.primaryMid.color, false)) ManualOpenBrowser();
}

// Installed → published, and what downloading it will do
float UpdatesTab::ManualReleaseCard(UI::Ui& ui, const Rect& r, bool draw) {
    constexpr float kH = 20.f * 2.f + 34.f + 56.f + 14.f + 48.f;
    if (!draw) return kH;
    MainState& s = main.State();
    const M::Palette& p = M::P();
    const Color fg = p.foreground;
    const float t = static_cast<float>(ui.Time());
    M::CardLook look;
    look.radius = kRadius;
    M::Card card(ui, ui.MakeId("upd-manual-release"), r, Edges(20.f), false, 1.f, look);
    const Rect c = card.content;
    ui.g.Text(Loc::Get("manual_update_release_caps"), { c.x, c.y, c.w, 20.f }, p.primaryMid.color, Caps());
    float y = c.y + 34.f;

    const bool known = manualRelease && !Blank(manualRelease->latestVersion);
    const std::wstring published = known ? WithV(manualRelease->latestVersion)
                                         : manual == Manual::Checking ? std::wstring(1 + static_cast<int>(std::fmod(t * 3.f, 3.f)), L'.')
                                                                     : Loc::Get("updates_not_available");
    TextStyle v = Style(18.f, DWRITE_FONT_WEIGHT_BOLD, false, HAlign::Center);
    v.valign = VAlign::Center;
    const float boxW = (c.w - 50.f) * 0.5f;
    const Rect from{ c.x, y, boxW, 56.f }, to{ c.R() - boxW, y, boxW, 56.f };
    ui.g.FillRound(from, 12.f, fg.Fade(0.05f));
    ui.g.FillRound(to, 12.f, kGreen.Fade(known ? 0.14f : 0.05f));
    if (known) ui.g.StrokeRound(to.Inset(0.75f), 11.25f, kGreen.Fade(0.6f), 1.5f);
    TextStyle cap = Style(10.f, DWRITE_FONT_WEIGHT_NORMAL, false, HAlign::Center);
    ui.g.Text(Loc::Get("updates_installed"), { from.x, from.y + 6.f, from.w, 14.f }, fg.Fade(0.55f), cap);
    ui.g.Text(Loc::Get("manual_update_published"), { to.x, to.y + 6.f, to.w, 14.f }, ChangelogCards::ReadableColor(kGreen), cap);
    ui.g.Text(s.currentVersion, { from.x, from.y + 18.f, from.w, 34.f }, fg.Fade(0.8f), v);
    {
        // The published version pops in once it's known
        const float shown = ui.Tween(ui.MakeId("upd-manual-known"), known ? 1.f : 0.f, 0.35f);
        M::Motion::Enter enter(ui, known ? Anim::EaseOutBack(shown, 0.8f) : 1.f, 0.f, 12.f);
        ui.g.Text(published, { to.x, to.y + 18.f, to.w, 34.f }, fg, v);
    }
    const float slide = 6.f * std::sin(t * 3.f);
    ui.KeepAnimatingThrottled();
    M::DrawIcon(ui.g, M::Icon::ArrowRight, c.CX() + slide * 0.5f, from.CY(), 24.f, kGreen);
    y += 56.f + 14.f;

    // What it will do
    const char* note = "manual_update_note_unknown";
    if (known) {
        if (Versions::IsNewer(s.currentVersion, manualRelease->latestVersion)) note = "manual_update_newer";
        else if (Versions::IsNewer(manualRelease->latestVersion, s.currentVersion)) note = "manual_update_older";
        else note = "manual_update_same";
    }
    const Rect box{ c.x, y, c.w, 48.f };
    ui.g.FillRound(box, 12.f, p.primaryMid.color.Fade(0.08f));
    M::DrawIcon(ui.g, M::Icon::Information, box.x + 20.f, box.CY(), 18.f, p.primaryMid.color);
    TextStyle text = Style(11.f, DWRITE_FONT_WEIGHT_NORMAL, true);
    const std::wstring& noteText = Loc::Get(note);
    const float th = std::min(box.h - 6.f, TextH(ui, noteText, text, box.w - 50.f));
    ui.g.Text(noteText, { box.x + 38.f, box.CY() - th * 0.5f, box.w - 50.f, th }, fg.Fade(0.75f), text);
    return kH;
}

} // namespace SonicRacingSaveManager::Main
