#include "Windows/Main/Tabs/Tickets/TicketsKit.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cwctype>
#include "App/Paths.h"
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "UI/Material/Buttons.h"
#include "UI/Material/Card.h"
#include "UI/Material/Motion.h"
#include "UI/Material/Theme.h"
#include "UI/Material/Toggles.h"
#include "Windows/Main/Tabs/TabKit.h"

namespace SonicRacingSaveManager::Main::TicketsKit {

namespace M = UI::Material;
using TabKit::LineH;
using TabKit::Style;

namespace {

constexpr float kGap = 12.f, kMinColumn = 330.f, kRadius = 14.f;
constexpr float kPadX = 14.f, kPadY = 12.f, kHead = 52.f, kBadge = 48.f, kDetailsGap = 10.f;
constexpr float kTextX = kPadX + kBadge + 14.f;   // where the texts and the details start
constexpr float kPi = 3.14159265f;

struct TileState {
    bool wasHovered = false;
    double hoverSince = -10.0;
};

struct SwitchState {
    bool overriding = false;
    bool value = false;
    bool boundAtClick = false;
};

} // namespace

float RowsHeight(const Rows& rows) {
    float h = 0.f;
    for (const auto& r : rows) h += r.height + r.margin.V();
    return h;
}

void DrawRows(const Rows& rows, const Rect& area) {
    float y = area.y;
    for (const auto& r : rows) {
        y += r.margin.t;
        const float avail = area.w - r.margin.H();
        const float w = r.width > 0.f ? std::min(r.width, avail) : avail;
        if (r.draw) r.draw({ area.x + r.margin.l, y, w, r.height });
        y += r.height + r.margin.b;
    }
}

Row SwitchRow(UI::Ui& ui, UI::Id id, bool on, bool enabled, std::function<void()> onClick, bool locked, const std::wstring& tooltip) {
    Row r;
    r.width = M::kSwitchWidth + (locked ? 28.f : 0.f);
    r.height = M::kSwitchHeight;
    r.draw = [&ui, id, on, enabled, onClick, locked, tooltip](const Rect& rect) {
        const Rect sw{ rect.R() - M::kSwitchWidth, rect.y, M::kSwitchWidth, M::kSwitchHeight };
        if (locked) {
            // The padlock of a patch blocked by the other one
            const M::Palette& p = M::P();
            ui.g.FillEllipse(rect.x + 11.f, rect.CY(), 11.f, 11.f, p.foreground.Fade(0.1f));
            M::DrawIcon(ui.g, M::Icon::Lock, rect.x + 11.f, rect.CY(), 13.f, p.foreground.Fade(0.75f));
        }
        SwitchState& s = ui.Data<SwitchState>(UI::HashId(id, 0x5C17));
        if (s.overriding && on != s.boundAtClick) s.overriding = false;   // the bound value changed: follow it again
        const bool shown = s.overriding ? s.value : on;
        if (M::Switch(ui, id, sw, shown, enabled)) {
            s.overriding = true;
            s.value = !shown;
            s.boundAtClick = on;
            if (onClick) onClick();
        }
        if (!tooltip.empty()) ui.Tooltip(id, tooltip);
    };
    return r;
}

Row OpenButtonRow(UI::Ui& ui, UI::Id id, const std::wstring& label, std::function<void()> onClick, bool enabled) {
    M::ButtonLook look = M::SeasonalAccent();
    look.background = M::P().secondaryMid.color;
    look.padding = Edges{ 12.f, 0.f };
    look.radius = 14.f;
    look.text = Style(11.f, DWRITE_FONT_WEIGHT_MEDIUM);
    look.text.valign = VAlign::Center;
    Row r;
    r.width = M::ButtonWidth(ui, label, look, 14.f, 4.f);
    r.height = 28.f;
    r.draw = [&ui, id, label, onClick, enabled, look](const Rect& rect) {
        if (M::IconTextButton(ui, id, rect, M::Icon::ChevronRight, label, look, enabled, 14.f, 4.f) && onClick) onClick();
    };
    return r;
}

float SectionTitle(UI::Ui& ui, float x, float y, float w, int number, const std::wstring& text, int count, int active, double revealAt) {
    const M::Palette& p = M::P();
    constexpr float kRowH = 44.f;
    const Rect row{ x, y, w, kRowH };
    const float reveal = M::Motion::Stagger(ui, revealAt, 0, 0.f, 0.7f);

    // Big faded index ("01"), sliding in from the left
    TextStyle big = Style(30.f, DWRITE_FONT_WEIGHT_BLACK);
    big.valign = VAlign::Center;
    wchar_t digits[8];
    swprintf_s(digits, L"%02d", number);
    const float numberW = std::ceil(ui.g.Measure(digits, big).w) + 2.f;
    ui.g.Text(digits, { x - 16.f * (1.f - reveal), row.y, numberW, kRowH }, p.sectionHeader.Fade(0.28f * reveal), big);

    // Title (a dark halo while the night scenery is drawn)
    TextStyle style = Style(18.f, DWRITE_FONT_WEIGHT_BOLD);
    style.valign = VAlign::Center;
    const float titleW = std::ceil(ui.g.Measure(text, style).w) + 2.f;
    const Rect title{ x + numberW + 8.f, row.y, titleW, kRowH };
    if (p.sectionHeaderHalo) ui.g.GlowText(text, title, style, Colors::Black, 8.f, 0.95f);
    ui.g.Text(text, title, p.sectionHeader, style);

    // Pills: number of tools, number of options on
    TextStyle pill = Style(11.f, DWRITE_FONT_WEIGHT_SEMI_BOLD, false, HAlign::Center);
    pill.valign = VAlign::Center;
    float px = title.R() + 10.f;
    const auto Pill = [&](const std::wstring& label, Color background, Color foreground) {
        const float pw = std::ceil(ui.g.Measure(label, pill).w) + 16.f;
        const Rect r{ px, row.CY() - 10.f, pw, 20.f };
        ui.g.FillRound(r, 10.f, background);
        ui.g.Text(label, r, foreground, pill);
        px = r.R() + 6.f;
    };
    Pill(Loc::Fmt(count == 1 ? "tools_count_one" : "tools_count_many", count), p.cardBackground, p.foreground.Fade(0.8f));
    if (active > 0) Pill(Loc::Fmt("tools_on_pill", active), p.primaryMid.color, p.primaryMid.foreground);

    // Rule to the edge: draws itself in, then a spark runs along it every few seconds
    const float ruleX = px + 6.f, ruleW = std::max(0.f, row.R() - ruleX) * reveal;
    if (ruleW > 0.f) {
        ui.g.FillRect({ ruleX, row.CY(), ruleW, 2.f }, p.sectionHeader.Fade(0.25f));
        const float t = static_cast<float>(std::fmod(ui.Time() + number * 0.9, 5.0) / 1.2);
        if (t < 1.f && reveal >= 1.f) {
            const float sx = ruleX + ruleW * Anim::EaseInOutCubic(t);
            ui.g.FillEllipse(sx, row.CY() + 1.f, 3.f, 3.f, p.sectionHeader.Fade(0.9f * (1.f - t)));
            ui.g.FillRect({ std::max(ruleX, sx - 40.f), row.CY(), std::min(40.f, sx - ruleX), 2.f }, p.sectionHeader.Fade(0.5f * (1.f - t)));
        }
        ui.KeepAnimatingThrottled();
    }
    return kRowH + 8.f;
}

bool Matches(const ToolSpec& tool, const std::wstring& query) {
    if (query.empty()) return true;
    const auto lower = [](std::wstring s) {
        for (auto& c : s) c = static_cast<wchar_t>(std::towlower(c));
        return s;
    };
    return lower(tool.title + L" " + tool.subtitle + L" " + tool.search).find(lower(query)) != std::wstring::npos;
}

float ToolGrid(UI::Ui& ui, float x, float y, float w, std::vector<ToolSpec>& tools, double revealAt, int& index) {
    const M::Palette& p = M::P();
    const Color fg = p.foreground;
    const int columns = std::max(1, static_cast<int>((w + kGap) / (kMinColumn + kGap)));
    const float columnW = (w - kGap * static_cast<float>(columns - 1)) / static_cast<float>(columns);

    TextStyle titleStyle = Style(14.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    titleStyle.ellipsis = true;
    TextStyle subStyle = Style(11.f);
    subStyle.ellipsis = true;
    const float titleH = LineH(ui, titleStyle), subH = LineH(ui, subStyle);

    // Lay the visible tiles out in lines, measuring their details at the width they get
    struct Placed {
        ToolSpec* spec;
        Rows details;
        float width, height;
    };
    std::vector<std::vector<Placed>> lines(1);
    int used = 0;
    for (auto& tool : tools) {
        if (!tool.visible) continue;
        const int span = std::min(columns, tool.wide ? 2 : 1);
        if (used > 0 && used + span > columns) {
            lines.emplace_back();
            used = 0;
        }
        const float width = columnW * static_cast<float>(span) + kGap * static_cast<float>(span - 1);
        Rows details = tool.details ? tool.details(width - kTextX - kPadX) : Rows{};
        const float h = kPadY * 2.f + kHead + (details.empty() ? 0.f : kDetailsGap + RowsHeight(details));
        lines.back().push_back({ &tool, std::move(details), width, h });
        used += span;
    }

    float cy = y;
    for (auto& line : lines) {
        if (line.empty()) continue;
        float lineH = 0.f;
        for (const auto& placed : line) lineH = std::max(lineH, placed.height);
        float cx = x;
        for (auto& placed : line) {
            ToolSpec& spec = *placed.spec;
            const UI::Id id = ui.MakeId(spec.key);
            // The tile glides to its place (search, resizes)
            const Rect target{ cx, cy, placed.width, lineH };
            cx += placed.width + kGap;
            const Rect r{ ui.Animate(UI::HashId(id, 0xA1), target.x, 16.f), ui.Animate(UI::HashId(id, 0xA2), target.y, 16.f),
                          ui.Animate(UI::HashId(id, 0xA3), target.w, 16.f), ui.Animate(UI::HashId(id, 0xA4), target.h, 18.f) };
            const bool settling = std::fabs(r.x - target.x) + std::fabs(r.y - target.y) + std::fabs(r.w - target.w) + std::fabs(r.h - target.h) > 0.5f;
            if (settling) ui.KeepAnimating();

            // Staggered entrance from the right, then hover / press / active feedback
            M::Motion::Enter enter(ui, M::Motion::Stagger(ui, revealAt, index++, 0.035f, 0.45f, 18), 6.f, 36.f);
            const bool hovered = ui.MouseOver(r);
            bool held = false;
            if (spec.clickable) {
                const UI::Interaction it = ui.Interact(id, r);
                held = it.held;
                if (it.hovered) ui.SetCursor(IDC_HAND);
                if (it.clicked && spec.onClick) spec.onClick();
            }
            const M::Motion::Feedback fb = M::Motion::Hover(ui, id, hovered, held);
            const float on = ui.Tween(UI::HashId(id, 0xAC71), spec.active ? 1.f : 0.f, 0.3f);
            const float pulse = M::Motion::Pulse(ui, id, spec.active, 0.9f);
            TileState& state = ui.Data<TileState>(UI::HashId(id, 0x5EE9));
            if (hovered && !state.wasHovered) state.hoverSince = ui.Time();
            state.wasHovered = hovered;

            ui.PushTranslate(0.f, -2.f * fb.hover);
            const bool pressed = fb.press > 0.f;
            if (pressed) ui.PushScale(1.f - 0.02f * fb.press, r.CX(), r.CY());

            if (on > 0.f) ui.g.GlowRound({ r.x, r.y, target.w, target.h }, kRadius, p.primaryMid.color, 18.f, (0.2f + 0.55f * pulse) * on);
            M::CardLook look;
            look.radius = kRadius;
            look.elevation = 1.f + 4.f * fb.hover;
            const float borderAlpha = std::max(0.45f * fb.hover, on * 0.9f);
            if (borderAlpha > 0.01f) {
                look.border = p.primaryMid.color.Fade(borderAlpha);
                look.borderWidth = 1.f + 0.5f * on;
            }
            {
                M::Card card(ui, id, r, Edges(0.f), false, 1.f, look);
                if (settling) ui.PushClip(r);

                // Active wash from the left + the accent bar with a light running down it
                if (on > 0.f) {
                    const bool fadeWash = on < 1.f;
                    if (fadeWash) ui.g.PushOpacity(on);
                    ui.g.FillLinear(r, kRadius, { { 0.f, p.primaryMid.color.Fade(0.16f) }, { 0.65f, p.primaryMid.color.Fade(0.f) } }, { 0.f, 0.f }, { 1.f, 0.f });
                    if (fadeWash) ui.g.PopLayer();
                    const Rect bar{ r.x, r.y + 12.f, 4.f, r.h - 24.f };
                    ui.g.FillRound(bar, 2.f, p.primaryMid.color.Fade(on));
                    const float run = static_cast<float>(std::fmod(ui.Time(), 1.8) / 1.8);
                    const float segment = std::min(18.f, bar.h);
                    ui.g.FillRound({ bar.x, bar.y + (bar.h - segment) * run, 4.f, segment }, 2.f, Colors::White.Fade(0.65f * on * std::sin(run * kPi)));
                    ui.KeepAnimatingThrottled();
                }
                // Hover: a soft tint and a light band sweeping across once
                if (fb.hover > 0.f) ui.g.FillRound(r, kRadius, p.primaryMid.color.Fade(0.05f * fb.hover));
                const float sweep = static_cast<float>((ui.Time() - state.hoverSince) / 0.65);
                if (sweep >= 0.f && sweep < 1.f) {
                    ui.KeepAnimating();
                    const float bx = r.x - 140.f + (r.w + 180.f) * Anim::EaseOutCubic(sweep);
                    ui.g.PushRoundClip(r, kRadius);
                    ui.g.FillLinear({ bx, r.y, 140.f, r.h }, 0.f,
                                    { { 0.f, p.primaryLight.color.Fade(0.f) }, { 0.5f, p.primaryLight.color.Fade(0.22f) }, { 1.f, p.primaryLight.color.Fade(0.f) } },
                                    { 0.f, 0.f }, { 1.f, 0.f });
                    ui.g.PopLayer();
                }

                // Badge: lights up while on, pops when it turns on, tilts on hover
                const float bcx = r.x + kPadX + kBadge * 0.5f, bcy = r.y + kPadY + kHead * 0.5f;
                const float scale = 1.f + 0.08f * fb.hover + 0.18f * pulse * pulse;
                const float radius = kBadge * 0.5f * scale;
                if (pulse > 0.f) {
                    // Burst: an expanding ring and sparks flying out
                    const float k = 1.f - pulse;
                    ui.g.StrokeEllipse(bcx, bcy, radius + 34.f * k, radius + 34.f * k, p.primaryMid.color.Fade(0.8f * pulse), 2.5f);
                    for (int s = 0; s < 8; ++s) {
                        const float a = (s * 45.f + 22.5f) * kPi / 180.f;
                        const float r1 = radius + 6.f + 26.f * k, r2 = r1 + 8.f * pulse;
                        ui.g.Line(bcx + r1 * std::sin(a), bcy - r1 * std::cos(a), bcx + r2 * std::sin(a), bcy - r2 * std::cos(a),
                                  p.secondaryMid.color.Fade(pulse), 2.f, true);
                    }
                }
                ui.g.FillEllipse(bcx, bcy, radius, radius, Lerp(fg.Fade(0.07f), p.primaryMid.color, on));
                ui.g.StrokeEllipse(bcx, bcy, radius - 0.5f, radius - 0.5f, Lerp(fg.Fade(0.12f), p.primaryLight.color.Fade(0.6f), on), 1.f);
                if (on > 0.f) {
                    // An orbiting spark while on
                    const float a = static_cast<float>(std::fmod(ui.Time() * 2.2 + index, 2.0 * kPi));
                    const float orbit = radius + 4.f;
                    ui.g.FillEllipse(bcx + orbit * std::sin(a), bcy - orbit * std::cos(a), 2.5f, 2.5f, p.secondaryMid.color.Fade(on));
                    TabKit::Arc(ui.g, bcx, bcy, orbit, a * 180.f / kPi - 70.f, 60.f, p.secondaryMid.color.Fade(0.45f * on), 1.5f);
                }
                ui.g.PushTransform(D2D1::Matrix3x2F::Rotation(-10.f * fb.hover + 8.f * pulse, D2D1::Point2F(bcx, bcy)));
                if (!spec.image.empty()) {
                    if (auto image = Images::Load(Paths::Asset(spec.image))) {
                        const float s = 34.f * scale;
                        ui.g.DrawImage(*image, { bcx - s * 0.5f, bcy - s * 0.5f, s, s }, 1.f, Stretch::Uniform);
                    }
                } else {
                    M::DrawIcon(ui.g, spec.icon, bcx, bcy, 26.f * scale, Lerp(fg, p.primaryMid.foreground, on));
                }
                ui.g.PopTransform();

                // Trailing control
                const float headY = r.y + kPadY;
                float right = r.R() - kPadX;
                if (spec.trailing.draw) {
                    const Rect t{ right - spec.trailing.width, headY + (kHead - spec.trailing.height) * 0.5f, spec.trailing.width, spec.trailing.height };
                    spec.trailing.draw(t);
                    right = t.x - 8.f;
                }

                // Title + subtitle (nudged right on hover)
                const float tx = r.x + kTextX + 3.f * fb.hover, tw = std::max(0.f, right - tx - 4.f);
                const float stackH = spec.subtitle.empty() ? titleH : titleH + 2.f + subH;
                const float ty = headY + (kHead - stackH) * 0.5f;
                ui.g.Text(spec.title, { tx, ty, tw, titleH }, fg, titleStyle);
                if (!spec.subtitle.empty()) ui.g.Text(spec.subtitle, { tx, ty + titleH + 2.f, tw, subH }, fg.Fade(0.62f), subStyle);

                // Details under the title line
                if (!placed.details.empty())
                    DrawRows(placed.details, { r.x + kTextX, headY + kHead + kDetailsGap, r.w - kTextX - kPadX, RowsHeight(placed.details) });

                if (settling) ui.PopClip();
            }
            if (pressed) ui.PopTransform();
            ui.PopTransform();
        }
        cy += lineH + kGap;
    }
    return cy - y;
}

} // namespace SonicRacingSaveManager::Main::TicketsKit
