#include <algorithm>
#include <cmath>
#include "App/Paths.h"
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "UI/Material/Buttons.h"
#include "UI/Material/Card.h"
#include "UI/Material/Elevation.h"
#include "UI/Material/Motion.h"
#include "UI/Material/PackIcon.h"
#include "UI/Material/Theme.h"
#include "UI/Material/Toggles.h"
#include "Windows/Main/Tabs/TabKit.h"
#include "Windows/Main/Tabs/Tickets/TicketsTab.h"

// The Tickets Menu banner (the C#'s "Current Status" block, redesigned like the Inazuma editor's
// Tool Menu banner: speed lines, golden rings spinning by, the ticket emblem, the title dropping in,
// the connection orb, the status and Attach / Detach).
namespace SonicRacingSaveManager::Main {

namespace {

namespace M = UI::Material;
using TabKit::LineH;
using TabKit::Style;

constexpr float kPi = 3.14159265f;
const Color kGreen = Argb(0xFF4CAF50), kRed = Argb(0xFFD32F2F), kAmber = Argb(0xFFFFB300), kGrey = Argb(0xFF9E9E9E), kGold = Rgba(255, 205, 40);

// Stable pseudo-random 0..1 for (index, channel): the banner's particles need no state
float Hash(double index, double channel) {
    const double v = std::sin(index * 12.9898 + channel * 78.233) * 43758.5453;
    return static_cast<float>(v - std::floor(v));
}

// A golden ring seen at an angle (spinning around its vertical axis)
void Ring(Graphics& g, float cx, float cy, float radius, float spin, float alpha) {
    const float rx = radius * std::max(0.12f, std::fabs(std::cos(spin)));
    g.StrokeEllipse(cx, cy, rx, radius, Lerp(kGold, Colors::Black, 0.25f).Fade(alpha), radius * 0.42f);
    g.StrokeEllipse(cx, cy, rx, radius, kGold.Fade(alpha), radius * 0.28f);
    g.StrokeEllipse(cx - rx * 0.15f, cy - radius * 0.1f, rx * 0.8f, radius * 0.8f, Colors::White.Fade(0.55f * alpha), radius * 0.08f);
}

M::ButtonLook RaisedOn(Color background) {
    M::ButtonLook l = M::RaisedStatic();
    l.background = background;
    l.text.weight = DWRITE_FONT_WEIGHT_MEDIUM;
    l.radius = 20.f;
    return l;
}

} // namespace

void TicketsTab::Banner(UI::Ui& ui, const Rect& r) {
    TicketsState& s = *state;
    const M::Palette& p = M::P();
    const Color ink = p.primaryDark.foreground;
    const Color spark = p.secondaryMid.color;
    const double time = ui.Time();
    const float t = static_cast<float>(time);
    constexpr float kRadius = 18.f;
    ui.KeepAnimatingThrottled();

    M::Elevation(ui.g, r, kRadius, 3.f);
    ui.g.PushRoundClip(r, kRadius);
    // Base: a drifting gradient of the primary + two wandering glows
    const float drift = 0.18f * std::sin(t * 0.35f);
    ui.g.FillLinear(r, 0.f, { { 0.f, p.primaryDark.color }, { 0.55f, p.primaryMid.color }, { 1.f, Lerp(p.primaryDark.color, Colors::Black, 0.35f) } },
                    { drift, 0.f }, { 1.f, 1.f - drift });
    const float b1x = r.x + r.w * (0.25f + 0.12f * std::sin(t * 0.23f)), b1y = r.y + r.h * (0.5f + 0.3f * std::cos(t * 0.31f));
    ui.g.FillRadial({ b1x - 220.f, b1y - 160.f, 440.f, 320.f }, { { 0.f, spark.Fade(0.3f) }, { 1.f, spark.Fade(0.f) } });
    const float b2x = r.x + r.w * (0.78f + 0.1f * std::cos(t * 0.19f)), b2y = r.y + r.h * (0.5f + 0.35f * std::sin(t * 0.27f));
    ui.g.FillRadial({ b2x - 180.f, b2y - 140.f, 360.f, 280.f }, { { 0.f, p.primaryLight.color.Fade(0.35f) }, { 1.f, p.primaryLight.color.Fade(0.f) } });

    // Speed lines racing across (faster ones closer to the middle)
    for (int i = 0; i < 9; ++i) {
        const float speed = 260.f + 90.f * static_cast<float>(i % 4), len = 90.f + 60.f * static_cast<float>(i % 3);
        const float x = r.x + std::fmod(t * speed + i * 211.f, r.w + len) - len;
        const float y = r.y + r.h * (0.08f + 0.105f * static_cast<float>(i));
        ui.g.FillLinear({ x, y, len, 2.f }, 0.f, { { 0.f, ink.Fade(0.f) }, { 1.f, ink.Fade(0.12f) } }, { 0.f, 0.f }, { 1.f, 0.f });
    }
    // Golden rings spinning by, bobbing up and down
    for (int i = 0; i < 5; ++i) {
        const float speed = 40.f + 18.f * Hash(i, 1);
        const float x = r.x + std::fmod(t * speed + Hash(i, 2) * r.w, r.w + 60.f) - 30.f;
        const float y = r.y + r.h * (0.25f + 0.5f * Hash(i, 3)) + 6.f * std::sin(t * 2.f + static_cast<float>(i));
        Ring(ui.g, x, y, 10.f + 5.f * Hash(i, 4), t * 3.f + static_cast<float>(i), 0.55f);
    }
    // Sparks rising
    for (int i = 0; i < 24; ++i) {
        const float period = 3.f + 4.f * Hash(i, 5);
        const float phase = static_cast<float>(std::fmod(time / period + Hash(i, 6), 1.0));
        const float x = r.x + Hash(i, 7) * r.w + 8.f * std::sin(t * 1.3f + static_cast<float>(i));
        const float y = r.B() + 6.f - phase * (r.h + 12.f);
        const float size = 1.f + 2.f * Hash(i, 8);
        ui.g.FillEllipse(x, y, size, size, Lerp(spark, Colors::White, 0.4f).Fade(0.75f * std::sin(phase * kPi)));
    }
    // A glossy band sweeping over everything every few seconds
    const float shine = static_cast<float>(std::fmod(time, 6.5) / 1.4);
    if (shine < 1.f) {
        ui.KeepAnimating();
        const float sx = r.x - 200.f + (r.w + 400.f) * Anim::EaseInOutCubic(shine);
        ui.g.PushTransform(D2D1::Matrix3x2F::Skew(-24.f, 0.f, D2D1::Point2F(sx, r.CY())));
        ui.g.FillLinear({ sx, r.y, 180.f, r.h }, 0.f, { { 0.f, ink.Fade(0.f) }, { 0.5f, ink.Fade(0.13f) }, { 1.f, ink.Fade(0.f) } }, { 0.f, 0.f }, { 1.f, 0.f });
        ui.g.PopTransform();
    }
    // Energy line along the bottom edge
    ui.g.FillRect({ r.x, r.B() - 3.f, r.w, 3.f }, spark.Fade(0.45f));
    const float ex = r.x + std::fmod(t * 300.f, r.w + 220.f) - 220.f;
    ui.g.FillLinear({ ex, r.B() - 3.f, 220.f, 3.f }, 0.f, { { 0.f, Colors::White.Fade(0.f) }, { 0.5f, Colors::White.Fade(0.9f) }, { 1.f, Colors::White.Fade(0.f) } },
                    { 0.f, 0.f }, { 1.f, 0.f });
    ui.g.PopLayer();

    // ── Emblem: the ticket in a ring of turning arcs ──
    const float er = 38.f;
    const float ecx = r.x + 30.f + er, ecy = r.CY();
    ui.g.FillEllipse(ecx, ecy, er, er, Colors::Black.Fade(0.2f));
    ui.g.StrokeEllipse(ecx, ecy, er, er, ink.Fade(0.3f), 1.5f);
    for (int i = 0; i < 3; ++i) TabKit::Arc(ui.g, ecx, ecy, er + 6.f, t * 50.f + 120.f * static_cast<float>(i), 50.f, kGold.Fade(0.85f), 2.5f);
    for (int i = 0; i < 2; ++i) TabKit::Arc(ui.g, ecx, ecy, er + 11.f, -t * 34.f + 180.f * static_cast<float>(i), 70.f, ink.Fade(0.35f), 1.5f);
    if (const ImagePtr ticket = Images::Load(Paths::Asset(L"Tickets.png"))) {
        const float size = er * 1.25f;
        const Rect ir{ ecx - size * 0.5f, ecy - size * 0.5f, size, size };
        ui.g.PushTransform(D2D1::Matrix3x2F::Rotation(6.f * std::sin(t * 1.7f), D2D1::Point2F(ecx, ecy)));
        ui.g.GlowImage(*ticket, ir, kGold, 12.f, 0.5f + 0.2f * std::sin(t * 2.f));
        ui.g.DrawImage(*ticket, ir, 1.f);
        ui.g.PopTransform();
    }

    // ── Right: Attach / Detach, the orb and the status ──
    const bool attached = s.isAttached, waiting = !attached && s.isAutoAttachEnabled;
    const M::ButtonLook attach = RaisedOn(kGreen), detach = RaisedOn(kRed);
    const float bw = std::max(M::ButtonWidth(ui, Loc::Get("tickets_attach"), attach, 20.f, 8.f), M::ButtonWidth(ui, Loc::Get("tickets_detach"), detach, 20.f, 8.f)) + 8.f;
    const Rect button{ r.R() - 26.f - bw, r.CY() - 22.f, bw, 44.f };
    if (!attached) {
        const float breathe = 0.5f + 0.5f * std::sin(t * 3.f);
        ui.g.GlowRound(button, 20.f, kGreen, 14.f, 0.3f + 0.4f * breathe);
        if (M::IconTextButton(ui, ui.MakeId("tk-attach"), button, M::Icon::Link, Loc::Get("tickets_attach"), attach, s.CanAttachToProcess(), 20.f, 8.f))
            s.AttachToProcess();
    } else if (M::IconTextButton(ui, ui.MakeId("tk-detach"), button, M::Icon::LinkOff, Loc::Get("tickets_detach"), detach, true, 20.f, 8.f)) {
        s.DetachFromProcess();
    }

    const TextStyle caption = Style(11.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    const TextStyle status = Style(17.f, DWRITE_FONT_WEIGHT_BOLD);
    TextStyle hint = Style(11.f, DWRITE_FONT_WEIGHT_NORMAL);
    hint.ellipsis = true;
    constexpr float kStatusW = 330.f;
    const float tx = button.x - 18.f - kStatusW;
    const float orbR = 22.f;
    StatusOrb(ui, tx - 18.f - orbR, r.CY(), orbR);
    const float stackH = LineH(ui, caption) + 2.f + LineH(ui, status) + 3.f + LineH(ui, hint);
    float ty = r.CY() - stackH * 0.5f;
    ui.PushClip({ tx, r.y, kStatusW, r.h });
    ui.g.Text(Loc::Get("tickets_current_status"), { tx, ty, kStatusW, LineH(ui, caption) }, ink.Fade(0.7f), caption);
    ty += LineH(ui, caption) + 2.f;
    // StatusMessage slides in again whenever it changes
    struct Shown {
        std::wstring text;
        double since = -10.0;
    };
    Shown& shown = ui.Data<Shown>(ui.MakeId("tk-status-text"));
    if (shown.text != s.statusMessage) {
        shown.text = s.statusMessage;
        shown.since = time;
    }
    const float enter = Anim::EaseOutCubic(Anim::Clamp01(static_cast<float>((time - shown.since) / 0.4)));
    if (enter < 1.f) ui.KeepAnimating();
    TextStyle st = status;
    st.ellipsis = true;
    ui.g.Text(s.statusMessage, { tx + 16.f * (1.f - enter), ty, kStatusW, LineH(ui, status) }, ink.Fade(enter), st);
    ty += LineH(ui, status) + 3.f;
    const std::wstring& hintText = Loc::Get(attached ? "tickets_connected_hint" : waiting ? "tickets_auto_hint" : "tickets_attach_hint");
    ui.g.Text(hintText, { tx, ty, kStatusW, LineH(ui, hint) }, ink.Fade(0.72f), hint);
    ui.PopClip();

    // ── Left: overline, the title dropping in letter by letter, the subtitle typing itself ──
    const bool editor = s.selectedTool == L"tickets";
    const float textX = ecx + er + 28.f;
    const float textW = std::max(0.f, tx - 18.f - orbR * 2.f - 24.f - textX);
    const TextStyle overline = Style(11.f, DWRITE_FONT_WEIGHT_BOLD);
    const TextStyle title = Style(32.f, DWRITE_FONT_WEIGHT_BLACK);
    const TextStyle sub = Style(13.f);
    const float overH = LineH(ui, overline), titleH = LineH(ui, title), subH = LineH(ui, sub);
    float y = r.CY() - (overH + 2.f + titleH + 4.f + subH) * 0.5f;
    ui.PushClip({ textX, r.y, textW, r.h });
    ui.g.Text(L"SONIC RACING CROSSWORLDS  \x00B7  SAVE MANAGER", { textX, y, textW, overH }, Lerp(spark, Colors::White, 0.2f), overline);
    y += overH + 2.f;
    const std::wstring& word = Loc::Get(editor ? "tickets_editor_title" : "tickets_title");
    bool landed = true;
    for (size_t i = 0; i < word.size(); ++i) {
        const float k = M::Motion::Stagger(ui, loadedAt + 0.15, static_cast<int>(i), 0.045f, 0.55f);
        if (k < 1.f) landed = false;
        const float drop = Anim::EaseOutBack(k, 0.6f);
        const float cx = ui.g.Measure(word.substr(0, i), title).w;
        ui.g.Text(std::wstring(1, word[i]), { textX + cx, y - 26.f * (1.f - drop), 60.f, titleH }, ink.Fade(Anim::Clamp01(k * 1.5f)), title);
    }
    if (landed) {
        const float titleW = std::ceil(ui.g.Measure(word, title).w) + 2.f;
        ui.g.GlowText(word, { textX, y, titleW, titleH }, title, spark, 14.f, 0.3f + 0.15f * std::sin(t * 2.f));
        ui.g.Text(word, { textX, y, titleW, titleH }, ink, title);
    }
    y += titleH + 4.f;
    {
        const std::wstring& text = Loc::Get(editor ? "tickets_editor_subtitle" : "tickets_subtitle");
        const float typed = Anim::Clamp01(static_cast<float>((time - loadedAt - 0.6) / 1.6));
        if (typed < 1.f) ui.KeepAnimating();
        const size_t n = static_cast<size_t>(std::lround(typed * static_cast<float>(text.size())));
        TextStyle s2 = sub;
        s2.ellipsis = true;
        std::wstring visible = text.substr(0, n);
        if (time - loadedAt < 3.2 && std::fmod(time, 0.8) < 0.45) visible += L"\x258F";   // a blinking caret while typing
        ui.g.Text(visible, { textX, y, textW, subH }, ink.Fade(0.8f), s2);
    }
    ui.PopClip();
}

// The connection orb: green with spinning arcs while attached, an amber spinner while Auto-Attach
// waits, a slow grey breath otherwise; bursts when the connection is made
void TicketsTab::StatusOrb(UI::Ui& ui, float cx, float cy, float radius) {
    const TicketsState& s = *state;
    const Color ink = M::P().primaryDark.foreground;
    const bool attached = s.isAttached, waiting = !attached && s.isAutoAttachEnabled;
    const Color c = attached ? kGreen : waiting ? kAmber : kGrey;
    const float t = static_cast<float>(ui.Time());
    ui.g.FillEllipse(cx, cy, radius + 12.f, radius + 12.f, Colors::Black.Fade(0.18f));
    if (attached) {
        for (int i = 0; i < 2; ++i) {
            TabKit::Arc(ui.g, cx, cy, radius + 6.f, t * 140.f + 180.f * static_cast<float>(i), 100.f, c.Fade(0.9f), 2.5f);
            TabKit::Arc(ui.g, cx, cy, radius + 10.f, -t * 90.f + 180.f * static_cast<float>(i), 50.f, ink.Fade(0.4f), 1.5f);
        }
    } else if (waiting) {
        const float sweep = 40.f + 220.f * Anim::EaseInOutSine(Anim::PingPong(ui.Time(), 1.4));
        TabKit::Arc(ui.g, cx, cy, radius + 7.f, t * 320.f, sweep, c, 3.f);
    }
    if (attached || waiting) {
        const float k = static_cast<float>(std::fmod(ui.Time(), 1.6) / 1.6);
        ui.g.StrokeEllipse(cx, cy, radius + 2.f + 16.f * k, radius + 2.f + 16.f * k, c.Fade(0.55f * (1.f - k)), 2.f);
    }
    const float pulse = M::Motion::Pulse(ui, ui.MakeId("tk-orb"), attached, 1.f);
    if (pulse > 0.f) {
        const float k = 1.f - pulse;
        ui.g.StrokeEllipse(cx, cy, radius + 40.f * k, radius + 40.f * k, kGreen.Fade(pulse), 3.f);
        ui.g.FillEllipse(cx, cy, radius + 6.f * pulse, radius + 6.f * pulse, Colors::White.Fade(0.35f * pulse));
    }
    const float core = radius * (attached || waiting ? 1.f : 1.f + 0.05f * std::sin(t * 2.f));
    ui.g.FillRadial({ cx - core, cy - core, core * 2.f, core * 2.f }, { { 0.f, Lerp(c, Colors::White, 0.45f) }, { 1.f, c } });
    M::DrawIcon(ui.g, attached ? M::Icon::Link : waiting ? M::Icon::Magnify : M::Icon::LinkOff, cx, cy, radius * 0.95f, Colors::White);
}

} // namespace SonicRacingSaveManager::Main
