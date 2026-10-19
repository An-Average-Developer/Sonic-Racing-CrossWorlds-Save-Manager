#include "Windows/Main/Overlays/DiscordInfoOverlay.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include "App/Paths.h"
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "UI/Material/Buttons.h"
#include "UI/Material/Elevation.h"
#include "UI/Material/Motion.h"
#include "UI/Material/PackIcon.h"
#include "UI/Material/Theme.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Tabs/TabKit.h"

namespace SonicRacingSaveManager::Main {

namespace {

namespace M = UI::Material;
using TabKit::Style;
using TabKit::TextH;

constexpr double kOpen = 0.45, kClose = 0.2;
constexpr float kWidth = 520.f, kRadius = 18.f, kPad = 26.f, kHeaderH = 96.f, kPointGap = 12.f, kFooterH = 76.f;
const Color kDiscord = Rgba(0x58, 0x65, 0xF2), kGreen = Argb(0xFF00C853);

TextStyle IntroStyle() { return Style(15.f, DWRITE_FONT_WEIGHT_NORMAL, true); }
TextStyle PointStyle() { return Style(12.f, DWRITE_FONT_WEIGHT_NORMAL, true); }

struct Point {
    M::Icon icon;
    Color color;
    const char* key;
    const wchar_t* tag;   // the OAuth scope, shown as a pill (not translated)
};
const Point kPoints[] = {
    { M::Icon::CheckDecagram, kGreen, "discord_info_scope", L"identify" },
    { M::Icon::ShieldLock, Rgba(0xF0, 0x62, 0x92), "discord_info_never", nullptr },
    { M::Icon::AccountConvert, kDiscord, "discord_info_same", nullptr },
};
constexpr float kIconCol = 52.f;

// "Connecting lets us know {0}, so we can...", with {0} ("who sent each report") in bold
std::wstring Intro(std::vector<TextSpan>& spans) {
    const std::wstring& who = Loc::Get("discord_info_intro_bold");
    const size_t at = Loc::Get("discord_info_intro").find(L"{0}");
    if (at != std::wstring::npos) spans.push_back({ at, who.size(), DWRITE_FONT_WEIGHT_BOLD, kDiscord });
    return Loc::Fmt("discord_info_intro", who);
}

float PointHeight(UI::Ui& ui, const Point& point, float width) {
    const float text = TextH(ui, Loc::Get(point.key), PointStyle(), width - kIconCol - 16.f);
    return std::max(48.f, (point.tag ? 22.f : 0.f) + text + 24.f);
}

} // namespace

void DiscordInfoOverlay::Open() {
    if (open) return;
    open = true;
    changedAt = -1.0;
}

void DiscordInfoOverlay::Close() {
    if (!open) return;
    open = false;
    changedAt = -2.0;
}

float DiscordInfoOverlay::Reveal(UI::Ui& ui, int index) const {
    return open ? M::Motion::Stagger(ui, changedAt + 0.15, index, 0.07f, 0.4f) : 1.f;
}

float DiscordInfoOverlay::CardHeight(UI::Ui& ui, float width) const {
    const float inner = width - kPad * 2.f;
    std::vector<TextSpan> spans;
    const std::wstring intro = Intro(spans);
    float h = kHeaderH + kPad + std::ceil(ui.g.MeasureRich(intro, IntroStyle(), spans, inner).h) + 18.f;
    for (const auto& point : kPoints) h += PointHeight(ui, point, inner) + kPointGap;
    return h + kFooterH - kPointGap + 6.f;
}

void DiscordInfoOverlay::Draw(UI::Ui& ui, const Rect& screen) {
    const double now = ui.Time();
    if (changedAt == -1.0 || changedAt == -2.0) changedAt = now;
    const float raw = Anim::Clamp01(static_cast<float>((now - changedAt) / (open ? kOpen : kClose)));
    if (!open && raw >= 1.f) {
        changedAt = -10.0;
        return;
    }
    ui.KeepAnimating();
    const float fade = open ? Anim::EaseOutCubic(raw) : 1.f - Anim::EaseInCubic(raw);
    const float scale = open ? Anim::Lerp(0.86f, 1.f, Anim::EaseOutBack(raw, 1.2f)) : Anim::Lerp(1.f, 0.94f, raw);
    const float rise = open ? 40.f * (1.f - Anim::EaseOutCubic(raw)) : 16.f * raw;

    // The window dims; a click on it, Escape or the buttons close the card
    ui.Block(screen);
    ui.g.FillRect(screen, Colors::Black.Fade(0.55f * fade));
    const float h = CardHeight(ui, kWidth);
    const Rect card{ screen.CX() - kWidth * 0.5f, screen.CY() - h * 0.5f, kWidth, h };
    if (open && (ui.KeyPressed(VK_ESCAPE) || (ui.In().pressed[0] && !card.Contains(ui.MouseX(), ui.MouseY())))) Close();

    ui.g.PushOpacity(std::max(0.f, fade));
    ui.PushTranslate(0.f, rise);
    ui.PushScale(scale, card.CX(), card.CY());
    ui.Block(card);
    DrawCard(ui, card);
    ui.PopTransform();
    ui.PopTransform();
    ui.g.PopLayer();
}

void DiscordInfoOverlay::DrawCard(UI::Ui& ui, const Rect& card) {
    const M::Palette& p = M::P();
    const Color fg = p.foreground;
    const float t = static_cast<float>(ui.Time());
    M::Elevation(ui.g, card, kRadius, 18.f);
    ui.g.GlowRound(card, kRadius, kDiscord, 22.f, 0.35f);
    ui.g.FillRound(card, kRadius, p.background);

    // Header: Discord gradient with a drifting glow, the logo bobbing in a pulsing ring, the title
    const Rect header{ card.x, card.y, card.w, kHeaderH };
    {
        M::Motion::Enter enter(ui, Reveal(ui, 0), 0.f, -10.f);
        // A rounded box taller than the header, clipped to it: rounded top corners, square bottom
        ui.g.PushClip(header);
        ui.g.FillLinear({ header.x, header.y, header.w, header.h + kRadius }, kRadius,
                        { { 0.f, Lerp(kDiscord, Colors::Black, 0.25f) }, { 0.6f, kDiscord }, { 1.f, Lerp(kDiscord, Colors::White, 0.15f) } },
                        { 0.f, 0.f }, { 1.f, 1.f });
        const float gx = header.x + header.w * (0.65f + 0.2f * std::sin(t * 0.6f));
        ui.g.FillRadial({ gx - 160.f, header.y - 70.f, 320.f, 200.f }, { { 0.f, Colors::White.Fade(0.22f) }, { 1.f, Colors::White.Fade(0.f) } });
        ui.g.PopClip();

        const float lx = header.x + kPad + 26.f, ly = header.CY() + 3.f * std::sin(t * 2.2f);
        const float ring = static_cast<float>(std::fmod(ui.Time(), 1.8) / 1.8);
        ui.g.StrokeEllipse(lx, ly, 26.f + 12.f * ring, 26.f + 12.f * ring, Colors::White.Fade(0.45f * (1.f - ring)), 2.f);
        ui.g.FillEllipse(lx, ly, 26.f, 26.f, Colors::White);
        if (auto logo = Images::Load(Paths::Asset(L"Logos\\Discord.png"))) ui.g.DrawImage(*logo, { lx - 16.f, ly - 16.f, 32.f, 32.f }, 1.f);

        TextStyle title = Style(19.f, DWRITE_FONT_WEIGHT_BOLD);
        title.valign = VAlign::Center;
        title.ellipsis = true;
        const float tx = lx + 26.f + 16.f;
        ui.g.Text(Loc::Get("discord_info_header"), { tx, header.y, header.R() - 56.f - tx, header.h }, Colors::White, title);

        // Close (X): turns a quarter on hover
        const Rect x{ header.R() - 48.f, header.y + 14.f, 34.f, 34.f };
        const UI::Id xId = ui.MakeId("di-x");
        const UI::Interaction it = ui.Interact(xId, x);
        if (it.hovered) ui.SetCursor(IDC_HAND);
        const float hover = M::Motion::Hover(ui, xId, it.hovered, it.held).hover;
        ui.g.FillEllipse(x.CX(), x.CY(), 17.f, 17.f, Colors::White.Fade(0.12f + 0.18f * hover));
        ui.g.PushTransform(D2D1::Matrix3x2F::Rotation(90.f * hover, D2D1::Point2F(x.CX(), x.CY())));
        M::DrawIcon(ui.g, M::Icon::Close, x.CX(), x.CY(), 18.f, Colors::White);
        ui.g.PopTransform();
        if (it.clicked) Close();
    }

    const float x = card.x + kPad, w = card.w - kPad * 2.f;
    float y = header.B() + kPad;

    // Intro, the key part in bold Discord blue
    {
        M::Motion::Enter enter(ui, Reveal(ui, 1), 12.f);
        std::vector<TextSpan> spans;
        const std::wstring intro = Intro(spans);
        const float ih = std::ceil(ui.g.MeasureRich(intro, IntroStyle(), spans, w).h);
        ui.g.RichText(intro, { x, y, w, ih }, fg, IntroStyle(), spans);
        y += ih + 18.f;
    }

    // The three points
    int index = 2;
    for (const auto& point : kPoints) {
        const float ph = PointHeight(ui, point, w);
        const Rect row{ x, y, w, ph };
        y += ph + kPointGap;
        M::Motion::Enter enter(ui, Reveal(ui, index++), 0.f, 24.f);
        const UI::Id id = ui.MakeId("di-point", index);
        const float hover = M::Motion::Hover(ui, id, ui.MouseOver(row), false).hover;
        ui.g.FillRound(row, 14.f, Lerp(fg.Fade(0.04f), point.color.Fade(0.1f), hover));
        ui.g.FillRect({ row.x, row.y + 12.f, 3.f, row.h - 24.f }, point.color.Fade(0.7f));
        const float icx = row.x + kIconCol * 0.5f + 4.f, icy = row.y + 24.f;
        ui.g.FillEllipse(icx, icy, 16.f, 16.f, point.color.Fade(0.16f + 0.1f * hover));
        M::DrawIcon(ui.g, point.icon, icx, icy - 2.f * hover, 20.f * (1.f + 0.1f * hover), point.color);
        float ty = row.y + 12.f;
        const float tx = row.x + kIconCol + 8.f, tw = row.R() - 12.f - tx;
        if (point.tag) {
            TextStyle tag = Style(11.f, DWRITE_FONT_WEIGHT_BOLD, false, HAlign::Center);
            tag.valign = VAlign::Center;
            const float tagW = std::ceil(ui.g.Measure(point.tag, tag).w) + 18.f;
            const Rect pill{ tx, ty, tagW, 18.f };
            ui.g.Border(pill, 9.f, point.color.Fade(0.14f), point.color.Fade(0.5f), 1.f);
            ui.g.Text(point.tag, pill, p.baseDark ? Lerp(point.color, Colors::White, 0.25f) : Lerp(point.color, Colors::Black, 0.35f), tag);
            ty += 22.f;
        }
        const std::wstring& text = Loc::Get(point.key);
        ui.g.Text(text, { tx, ty, tw, TextH(ui, text, PointStyle(), tw) }, fg.Fade(0.75f), PointStyle());
    }

    // GOT IT: Discord-coloured pill that glows on hover
    {
        M::Motion::Enter enter(ui, Reveal(ui, index), 10.f);
        const std::wstring& label = Loc::Get("discord_info_got_it");
        TextStyle text = Style(14.f, DWRITE_FONT_WEIGHT_SEMI_BOLD, false, HAlign::Center);
        text.valign = VAlign::Center;
        const float bw = std::max(150.f, std::ceil(ui.g.Measure(label, text).w) + 56.f);
        const Rect button{ card.R() - kPad - bw, card.B() - kPad - 44.f + 6.f, bw, 44.f };
        const UI::Id id = ui.MakeId("di-ok");
        const UI::Interaction it = ui.Interact(id, button);
        if (it.hovered) ui.SetCursor(IDC_HAND);
        const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, it.held);
        ui.PushScale(1.f + 0.03f * fb.hover - 0.04f * fb.press, button.CX(), button.CY());
        if (fb.hover > 0.f) ui.g.GlowRound(button, 22.f, kDiscord, 14.f, 0.6f * fb.hover);
        ui.g.FillRound(button, 22.f, Lerp(kDiscord, Colors::White, 0.12f * fb.hover));
        ui.g.Text(label, button, Colors::White, text);
        ui.PopTransform();
        if (it.clicked) Close();
    }
}

} // namespace SonicRacingSaveManager::Main
