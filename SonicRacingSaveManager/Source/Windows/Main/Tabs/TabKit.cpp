#include "Windows/Main/Tabs/TabKit.h"
#include <algorithm>
#include <cmath>
#include "Graphics/Geometry.h"
#include "UI/Material/Card.h"
#include "UI/Material/Elevation.h"
#include "UI/Material/Motion.h"
#include "UI/Material/Theme.h"

namespace SonicRacingSaveManager::Main::TabKit {

namespace M = UI::Material;
constexpr float kPi = 3.14159265f;

TextStyle Style(float size, DWRITE_FONT_WEIGHT weight, bool wrap, HAlign align) {
    TextStyle t = M::Text(size, weight);
    t.align = align;
    t.valign = VAlign::Top;
    t.wrap = wrap;
    return t;
}

float LineH(UI::Ui& ui, const TextStyle& style) { return std::ceil(ui.g.Measure(L"Ag", style).h); }

float TextH(UI::Ui& ui, const std::wstring& text, const TextStyle& style, float width) {
    return std::ceil(ui.g.Measure(text, style, style.wrap ? width : 100000.f).h);
}

void Arc(Graphics& g, float cx, float cy, float radius, float startDeg, float sweepDeg, Color c, float width) {
    if (c.Transparent() || radius <= 0.f || std::fabs(sweepDeg) < 0.1f) return;
    if (std::fabs(sweepDeg) >= 359.9f) {
        g.StrokeEllipse(cx, cy, radius, radius, c, width);
        return;
    }
    const auto At = [&](float deg) {
        const float a = deg * kPi / 180.f;
        return D2D1::Point2F(cx + radius * std::sin(a), cy - radius * std::cos(a));
    };
    auto geometry = Geometry::Build([&](ID2D1GeometrySink* sink) {
        sink->BeginFigure(At(startDeg), D2D1_FIGURE_BEGIN_HOLLOW);
        sink->AddArc(D2D1::ArcSegment(At(startDeg + sweepDeg), D2D1::SizeF(radius, radius), 0.f,
                                      sweepDeg > 0.f ? D2D1_SWEEP_DIRECTION_CLOCKWISE : D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,
                                      std::fabs(sweepDeg) > 180.f ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL));
        sink->EndFigure(D2D1_FIGURE_END_OPEN);
    });
    g.StrokeGeometry(geometry.Get(), c, width, true);
}

bool PillButton(UI::Ui& ui, UI::Id id, const Rect& r, M::Icon icon, const std::wstring& text, Color color, bool filled, bool enabled,
                float iconAngle) {
    const UI::Interaction it = ui.Interact(id, r, enabled);
    if (it.hovered) ui.SetCursor(IDC_HAND);
    const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered && enabled, it.held);
    const float radius = r.h * 0.5f;
    if (!enabled) ui.g.PushOpacity(0.45f);
    ui.PushScale(1.f + 0.02f * fb.hover - 0.04f * fb.press, r.CX(), r.CY());
    if (fb.hover > 0.f) ui.g.GlowRound(r, radius, color, 14.f, 0.55f * fb.hover);
    const Color fg = filled ? M::ContrastingForeground(color) : M::P().baseDark ? Lerp(color, Colors::White, 0.3f) : Lerp(color, Colors::Black, 0.25f);
    if (filled) {
        M::Elevation(ui.g, r, radius, 2.f + 3.f * fb.hover);
        ui.g.FillRound(r, radius, Lerp(color, Colors::White, 0.1f * fb.hover));
    } else {
        ui.g.FillRound(r, radius, color.Fade(0.06f + 0.12f * fb.hover));
        ui.g.StrokeRound(r.Inset(0.75f), radius - 0.75f, color.Fade(0.6f + 0.4f * fb.hover), 1.5f);
    }
    TextStyle label = Style(14.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    label.valign = VAlign::Center;
    const float lw = std::ceil(ui.g.Measure(text, label).w);
    const float cx = r.CX() - (20.f + 10.f + lw) * 0.5f;
    ui.g.PushTransform(D2D1::Matrix3x2F::Rotation(iconAngle, D2D1::Point2F(cx + 10.f, r.CY())) *
                       D2D1::Matrix3x2F::Translation(0.f, -2.f * fb.hover));
    M::DrawIcon(ui.g, icon, cx + 10.f, r.CY(), 20.f, fg);
    ui.g.PopTransform();
    ui.g.Text(text, { cx + 30.f, r.y, lw + 2.f, r.h }, fg, label);
    ui.PopTransform();
    if (!enabled) ui.g.PopLayer();
    return it.clicked && enabled;
}

float PillWidth(UI::Ui& ui, const std::wstring& text) {
    TextStyle label = Style(14.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    return std::ceil(ui.g.Measure(text, label).w) + 30.f + 48.f;
}

Color Readable(Color c) { return M::P().baseDark ? Lerp(c, Colors::White, 0.2f) : Lerp(c, Colors::Black, 0.45f); }

float StepsCard(UI::Ui& ui, const Rect& r, const char* key, const std::wstring& title, const std::vector<std::wstring>& steps, double revealAt,
                bool draw) {
    const M::Palette& p = M::P();
    const Color fg = p.foreground;
    const TextStyle text = Style(12.f, DWRITE_FONT_WEIGHT_NORMAL, true);
    constexpr float kPad = 20.f;
    const float textW = r.w - kPad * 2.f - 40.f;
    float h = kPad * 2.f + 34.f;
    for (const auto& step : steps) h += std::max(26.f, TextH(ui, step, text, textW)) + 16.f;
    h -= 16.f;
    if (!draw) return h;

    M::CardLook look;
    look.radius = 16.f;
    M::Card card(ui, ui.MakeId(key), r, Edges(kPad), false, 1.f, look);
    const Rect c = card.content;
    TextStyle caps = Style(11.f, DWRITE_FONT_WEIGHT_BOLD);
    caps.valign = VAlign::Center;
    ui.g.Text(title, { c.x, c.y, c.w, 20.f }, p.primaryMid.color, caps);
    float y = c.y + 34.f;
    for (size_t i = 0; i < steps.size() && y < c.B(); ++i) {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, revealAt, static_cast<int>(i), 0.08f, 0.4f), 0.f, 16.f);
        const float th = std::max(26.f, TextH(ui, steps[i], text, c.w - 40.f));
        ui.g.FillEllipse(c.x + 13.f, y + 13.f, 13.f, 13.f, p.primaryMid.color.Fade(0.15f));
        TextStyle num = Style(11.f, DWRITE_FONT_WEIGHT_BOLD, false, HAlign::Center);
        num.valign = VAlign::Center;
        ui.g.Text(std::to_wstring(i + 1), { c.x, y, 26.f, 26.f }, Readable(p.primaryMid.color), num);
        ui.g.Text(steps[i], { c.x + 38.f, y + (th > 26.f ? 0.f : 5.f), c.w - 40.f, th }, fg.Fade(0.8f), text);
        if (i + 1 < steps.size()) ui.g.FillRect({ c.x + 12.f, y + 28.f, 2.f, th - 8.f }, p.primaryMid.color.Fade(0.2f));
        y += th + 16.f;
    }
    return h;
}

} // namespace SonicRacingSaveManager::Main::TabKit
