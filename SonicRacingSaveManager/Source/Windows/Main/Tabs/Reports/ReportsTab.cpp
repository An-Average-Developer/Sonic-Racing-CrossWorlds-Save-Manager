#include "Windows/Main/Tabs/Reports/ReportsTab.h"
#include <algorithm>
#include <cmath>
#include <cwctype>
#include "App/Paths.h"
#include "Core/Files.h"
#include "Core/System.h"
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "Services/App/Log.h"
#include "Services/Community/ReportServices.h"
#include "UI/Material/Buttons.h"
#include "UI/Material/Card.h"
#include "UI/Material/Elevation.h"
#include "UI/Material/Fields.h"
#include "UI/Material/Motion.h"
#include "UI/Material/Theme.h"
#include "UI/Material/Toggles.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Overlays/DiscordInfoOverlay.h"
#include "Windows/Main/Tabs/TabKit.h"
#include "Windows/Main/Tabs/TabBanner.h"

namespace SonicRacingSaveManager::Main {

namespace M = UI::Material;
namespace OAuth = Services::DiscordOAuth;
using namespace TabKit;

namespace {

constexpr float kRightW = 360.f, kGap = 16.f, kRadius = 16.f;
const Color kDiscord = Rgba(0x58, 0x65, 0xF2), kGreen = Argb(0xFF4CAF50);

std::wstring Trim(const std::wstring& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::iswspace(s[a])) ++a;
    while (b > a && std::iswspace(s[b - 1])) --b;
    return s.substr(a, b - a);
}

TextStyle Caps() {
    TextStyle t = Style(11.f, DWRITE_FONT_WEIGHT_BOLD);
    t.valign = VAlign::Center;
    return t;
}

} // namespace

void ReportsTab::OnSelected() {
    loadedAt = -1.0;
    if (auto cached = OAuth::LoadCached()) {
        discordUser = cached;
        ShowConnected(cached->username);
    } else {
        ShowDisconnected();
    }
}

void ReportsTab::Draw(UI::Ui& ui, const Rect& area) {
    if (loadedAt < 0.0) loadedAt = ui.Time();
    const Rect grid = area.Inset(20.f);

    const Rect banner{ grid.x, grid.y, grid.w, kTabBannerHeight };
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt, 0, 0.f, 0.5f), 14.f);
        TabBannerSpec spec;
        spec.icon = M::Icon::Bug;
        spec.motion = TabBannerSpec::Motion::Wobble;
        spec.title = Loc::Get("reports_title");
        spec.subtitle = Loc::Get("reports_subtitle");
        spec.pill = Loc::Get("reports_pill");
        spec.pillLogo = L"Logos\\Discord.png";
        TabBanner(ui, banner, spec, loadedAt);
    }

    const float top = banner.B() + kGap;
    const Rect left{ grid.x, top, grid.w - kRightW - kGap, grid.B() - top };
    const Rect right{ left.R() + kGap, top, kRightW, grid.B() - top };
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.1, 0, 0.f, 0.5f), 18.f);
        M::CardLook look;
        look.radius = kRadius;
        M::Card card(ui, ui.MakeId("rep-form"), left, Edges(24.f, 20.f), false, 1.f, look);
        Form(ui, card.content);
    }
    const float discordH = DiscordCard(ui, { right.x, right.y, right.w, 0.f }, false);
    const Rect discord{ right.x, right.y, right.w, discordH };
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.18, 0, 0.f, 0.5f), 0.f, 28.f);
        DiscordCard(ui, discord, true);
    }
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.26, 0, 0.f, 0.5f), 0.f, 28.f);
        Checklist(ui, { right.x, discord.B() + kGap, right.w, right.B() - discord.B() - kGap });
    }
}

// ── Discord identity: connect / waiting / connected ──
float ReportsTab::DiscordCard(UI::Ui& ui, const Rect& r, bool draw) {
    const M::Palette& p = M::P();
    const Color fg = p.foreground;
    constexpr float kPad = 20.f;
    const float w = r.w - kPad * 2.f;
    const TextStyle why = Style(12.f, DWRITE_FONT_WEIGHT_NORMAL, true);
    const std::wstring& whyText = Loc::Get("reports_discord_why");
    const float whyH = TextH(ui, whyText, why, w);
    const float stateH = connectedVisible ? 64.f : 46.f;
    const float h = kPad * 2.f + 24.f + 10.f + whyH + 16.f + stateH;
    if (!draw) return h;

    M::CardLook look;
    look.radius = kRadius;
    M::Card card(ui, ui.MakeId("rep-discord"), r, Edges(kPad), false, 1.f, look);
    const Rect c = card.content;
    float y = c.y;
    if (auto logo = Images::Load(Paths::Asset(L"Logos\\Discord.png"))) ui.g.DrawImage(*logo, { c.x, y + 1.f, 22.f, 22.f }, 1.f);
    ui.g.Text(Loc::Get("reports_discord_identity"), { c.x + 30.f, y, c.w - 70.f, 24.f }, kDiscord, Caps());
    const UI::Id info = ui.MakeId("rep-info");
    if (M::IconButton(ui, info, { c.R() - 30.f, y - 3.f, 30.f, 30.f }, M::Icon::InformationOutline, 20.f, M::IconButtonLook(fg.Fade(0.7f))))
        main.DiscordInfo().Open();
    ui.Tooltip(info, Loc::Get("reports_discord_why_tooltip"));
    y += 24.f + 10.f;
    ui.g.Text(whyText, { c.x, y, c.w, whyH }, fg.Fade(0.65f), why);
    y += whyH + 16.f;

    // The state pops in whenever it changes
    struct StateAnim {
        int state = -1;
        double since = -10.0;
    };
    StateAnim& anim = ui.Data<StateAnim>(ui.MakeId("rep-discord-state"));
    const int state = connectedVisible ? 2 : waitingRowVisible ? 1 : 0;
    if (anim.state != state) {
        anim.state = state;
        anim.since = ui.Time();
    }
    M::Motion::Enter enter(ui, Anim::EaseOutBack(Anim::Clamp01(static_cast<float>((ui.Time() - anim.since) / 0.4)), 0.6f), 10.f);
    if (ui.Time() - anim.since < 0.4) ui.KeepAnimating();
    const Rect row{ c.x, y, c.w, stateH };

    if (connectRowVisible) {
        // Discord-coloured pill button with the logo in a white circle; a light sweeps over it on hover
        const UI::Id id = ui.MakeId("rep-connect");
        const UI::Interaction it = ui.Interact(id, row);
        if (it.hovered) ui.SetCursor(IDC_HAND);
        const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, it.held);
        ui.PushScale(1.f - 0.03f * fb.press, row.CX(), row.CY());
        if (fb.hover > 0.f) ui.g.GlowRound(row, 23.f, kDiscord, 14.f, 0.6f * fb.hover);
        ui.g.FillRound(row, 23.f, Lerp(kDiscord, Colors::White, 0.12f * fb.hover));
        ui.g.FillEllipse(row.x + 24.f, row.CY(), 15.f, 15.f, Colors::White);
        if (auto logo = Images::Load(Paths::Asset(L"Logos\\Discord.png"))) ui.g.DrawImage(*logo, { row.x + 14.f, row.CY() - 10.f, 20.f, 20.f }, 1.f);
        TextStyle text = Style(14.f, DWRITE_FONT_WEIGHT_SEMI_BOLD, false, HAlign::Center);
        text.valign = VAlign::Center;
        ui.g.Text(Loc::Get("reports_connect_discord"), { row.x + 40.f, row.y, row.w - 50.f, row.h }, Colors::White, text);
        ui.PopTransform();
        if (it.clicked) Connect();
    } else if (waitingRowVisible) {
        ui.g.FillRound(row, 23.f, kDiscord.Fade(0.12f));
        TabKit::Arc(ui.g, row.x + 24.f, row.CY(), 10.f, static_cast<float>(ui.Time()) * 360.f, 250.f, kDiscord, 2.5f);
        ui.KeepAnimating();
        TextStyle text = Style(12.f);
        text.valign = VAlign::Center;
        ui.g.Text(Loc::Get("reports_waiting_auth"), { row.x + 46.f, row.y, row.w - 140.f, row.h }, fg.Fade(0.8f), text);
        M::ButtonLook flat = M::Flat();
        flat.text = M::Medium(13.f);
        flat.text.align = HAlign::Center;
        flat.text.valign = VAlign::Center;
        const float bw = M::ButtonWidth(ui, Loc::Get("cancel"), flat);
        if (M::TextButton(ui, ui.MakeId("rep-cancel-auth"), { row.R() - bw - 6.f, row.CY() - 16.f, bw, 32.f }, Loc::Get("cancel"), flat)) {
            if (authCancel) authCancel->Request();
            waitingRowVisible = false;
            ShowDisconnected();
        }
    } else if (connectedVisible) {
        // Avatar (first letter), the name, a check badge and a disconnect button
        ui.g.FillRound(row, 14.f, fg.Fade(0.05f));
        const float ax = row.x + 32.f, ay = row.CY();
        ui.g.FillEllipse(ax, ay, 20.f, 20.f, kDiscord);
        TextStyle initial = Style(18.f, DWRITE_FONT_WEIGHT_BOLD, false, HAlign::Center);
        initial.valign = VAlign::Center;
        const std::wstring letter = username.empty() ? L"?" : std::wstring(1, static_cast<wchar_t>(std::towupper(username[0])));
        ui.g.Text(letter, { ax - 20.f, ay - 20.f, 40.f, 40.f }, Colors::White, initial);
        ui.g.FillEllipse(ax + 15.f, ay + 14.f, 8.f, 8.f, p.cardBackground);
        M::DrawIcon(ui.g, M::Icon::CheckDecagram, ax + 15.f, ay + 14.f, 14.f, Argb(0xFF00C853));
        TextStyle nameStyle = Style(14.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        nameStyle.valign = VAlign::Center;
        nameStyle.ellipsis = true;
        TextStyle sub = Style(11.f);
        sub.valign = VAlign::Center;
        ui.g.Text(username, { ax + 32.f, ay - 20.f, row.R() - ax - 80.f, 22.f }, fg, nameStyle);
        ui.g.Text(Loc::Get("reports_connected"), { ax + 32.f, ay + 2.f, row.R() - ax - 80.f, 18.f }, fg.Fade(0.6f), sub);
        const UI::Id out = ui.MakeId("rep-disconnect");
        if (M::IconButton(ui, out, { row.R() - 44.f, ay - 16.f, 32.f, 32.f }, M::Icon::LinkOff, 18.f, M::IconButtonLook(fg.Fade(0.7f)))) {
            OAuth::ClearCached();
            discordUser.reset();
            ShowDisconnected();
        }
        ui.Tooltip(out, Loc::Get("reports_disconnect"));
    }
    return h;
}

// ── Checklist: ticks itself as the report gets ready, with a progress ring ──
void ReportsTab::Checklist(UI::Ui& ui, const Rect& r) {
    const M::Palette& p = M::P();
    const Color fg = p.foreground;
    M::CardLook look;
    look.radius = kRadius;
    M::Card card(ui, ui.MakeId("rep-checklist"), r, Edges(20.f), false, 1.f, look);
    const Rect c = card.content;

    struct Item {
        std::wstring title;
        std::wstring hint;
        bool done;
        bool optional;
    };
    const Item items[] = {
        { Loc::Get("reports_check_connect"), Loc::Get("reports_check_connect_hint"), discordUser.has_value(), false },
        { Loc::Get("reports_check_title"), Loc::Get("reports_check_title_hint"), !Trim(title).empty(), false },
        { Loc::Get("reports_check_describe"), Loc::Get("reports_check_describe_hint"), Trim(description).size() >= 20, false },
        { Loc::Get("reports_check_screenshot"), Loc::Get("reports_check_screenshot_hint"), !imagePath.empty(), true },
    };
    int done = 0;
    for (const auto& item : items)
        if (item.done && !item.optional) ++done;

    // Header + progress ring
    ui.g.Text(Loc::Get("reports_before_send"), { c.x, c.y, c.w - 70.f, 20.f }, p.primaryMid.color, Caps());
    TextStyle state = Style(12.f);
    state.valign = VAlign::Center;
    ui.g.Text(Loc::Get(done == 3 ? "reports_ready" : "reports_complete_steps"), { c.x, c.y + 20.f, c.w - 70.f, 20.f }, done == 3 ? kGreen : fg.Fade(0.6f), state);
    const float rcx = c.R() - 26.f, rcy = c.y + 22.f;
    const float fraction = ui.Animate(ui.MakeId("rep-progress"), done / 3.f, 6.f, 0.f);
    ui.g.StrokeEllipse(rcx, rcy, 22.f, 22.f, fg.Fade(0.08f), 5.f);
    TabKit::Arc(ui.g, rcx, rcy, 22.f, 0.f, 360.f * fraction, done == 3 ? kGreen : p.primaryMid.color, 5.f);
    TextStyle count = Style(12.f, DWRITE_FONT_WEIGHT_BOLD, false, HAlign::Center);
    count.valign = VAlign::Center;
    ui.g.Text(std::to_wstring(done) + L"/3", { rcx - 20.f, rcy - 10.f, 40.f, 20.f }, fg, count);

    float y = c.y + 64.f;
    const TextStyle name = [] {
        TextStyle s = Style(13.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        s.valign = VAlign::Center;
        return s;
    }();
    const TextStyle hint = Style(11.f, DWRITE_FONT_WEIGHT_NORMAL, true);
    for (int i = 0; i < 4; ++i) {
        const Item& item = items[i];
        const UI::Id id = ui.MakeId("rep-check", i);
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.35, i, 0.07f, 0.4f), 0.f, 18.f);
        const float on = ui.Tween(id, item.done ? 1.f : 0.f, 0.25f);
        const float pulse = M::Motion::Pulse(ui, id, item.done, 0.6f);
        const float hintH = TextH(ui, item.hint, hint, c.w - 44.f);
        const float cx = c.x + 14.f, cy = y + 13.f;
        if (pulse > 0.f) ui.g.StrokeEllipse(cx, cy, 13.f + 12.f * (1.f - pulse), 13.f + 12.f * (1.f - pulse), kGreen.Fade(pulse), 2.f);
        ui.g.FillEllipse(cx, cy, 13.f * (1.f + 0.2f * pulse), 13.f * (1.f + 0.2f * pulse), Lerp(fg.Fade(0.06f), kGreen, on));
        if (on < 1.f) ui.g.StrokeEllipse(cx, cy, 12.5f, 12.5f, fg.Fade(0.25f * (1.f - on)), 1.f);
        if (on > 0.f) M::DrawIcon(ui.g, M::Icon::Check, cx, cy, 16.f * (0.6f + 0.4f * on), Colors::White.Fade(on));
        if (on < 1.f) {
            TextStyle num = Style(11.f, DWRITE_FONT_WEIGHT_BOLD, false, HAlign::Center);
            num.valign = VAlign::Center;
            ui.g.Text(std::to_wstring(i + 1), { cx - 13.f, cy - 13.f, 26.f, 26.f }, fg.Fade(0.6f * (1.f - on)), num);
        }
        const float tx = c.x + 40.f;
        ui.g.Text(item.title, { tx, y, c.w - 44.f, 26.f }, fg.Fade(item.done ? 0.6f : 0.95f), name);
        if (item.optional) {
            const float tw = std::ceil(ui.g.Measure(item.title, name).w);
            TextStyle tag = Style(9.f, DWRITE_FONT_WEIGHT_BOLD, false, HAlign::Center);
            tag.valign = VAlign::Center;
            const std::wstring& optional = Loc::Get("reports_optional");
            const Rect pill{ tx + tw + 8.f, y + 5.f, std::max(60.f, std::ceil(ui.g.Measure(optional, tag).w) + 16.f), 16.f };
            ui.g.FillRound(pill, 8.f, fg.Fade(0.08f));
            ui.g.Text(optional, pill, fg.Fade(0.55f), tag);
        }
        ui.g.Text(item.hint, { tx, y + 26.f, c.w - 44.f, hintH }, fg.Fade(0.55f), hint);
        // Line to the next step
        if (i < 3) ui.g.FillRect({ cx - 1.f, cy + 16.f, 2.f, hintH + 14.f }, Lerp(fg.Fade(0.1f), kGreen.Fade(0.5f), on));
        y += 26.f + hintH + 18.f;
    }

    // Tip at the bottom
    if (c.B() - y > 60.f) {
        const Rect tip{ c.x, c.B() - 52.f, c.w, 52.f };
        ui.g.FillRound(tip, 12.f, p.primaryMid.color.Fade(0.08f));
        M::DrawIcon(ui.g, M::Icon::Information, tip.x + 22.f, tip.CY(), 20.f, p.primaryMid.color);
        TextStyle text = Style(11.f, DWRITE_FONT_WEIGHT_NORMAL, true);
        const std::wstring& tipText = Loc::Get("reports_tip");
        const float th = TextH(ui, tipText, text, tip.w - 54.f);
        ui.g.Text(tipText, { tip.x + 42.f, tip.CY() - th * 0.5f, tip.w - 54.f, th }, fg.Fade(0.75f), text);
    }
}

void ReportsTab::Connect() {
    if (authCancel) authCancel->Request();
    Async::Cancel cancel;
    authCancel = cancel;
    connectRowVisible = false;
    waitingRowVisible = true;
    Async::Run([cancel] { return OAuth::Authenticate(cancel); }, [this](std::optional<OAuth::User> user) {
        waitingRowVisible = false;
        if (user) {
            discordUser = user;
            OAuth::SaveCached(*user);
            ShowConnected(user->username);
        } else {
            ShowDisconnected();
        }
    });
}

void ReportsTab::ShowConnected(const std::wstring& name) {
    username = name;
    connectedVisible = true;
    connectRowVisible = false;
    waitingRowVisible = false;
}

void ReportsTab::ShowDisconnected() {
    connectedVisible = false;
    connectRowVisible = true;
}

} // namespace SonicRacingSaveManager::Main

