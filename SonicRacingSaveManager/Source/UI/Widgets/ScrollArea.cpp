#include "UI/Widgets/ScrollArea.h"
#include <algorithm>
#include "UI/Style.h"

namespace SonicRacingSaveManager::UI {

struct ScrollState {
    float target = 0.f;
    float current = 0.f;
    float content = 0.f;      // content size measured last frame
    bool dragging = false;
    float dragStartMouse = 0.f, dragStartOffset = 0.f;
};

namespace {

constexpr float kBar = 6.f;
Color materialThumb = Hex(0xD8D8D8), materialArrow = Argb(0xE5000000);

} // namespace

void ScrollArea::SetMaterialColors(Color thumb, Color arrow) {
    materialThumb = thumb;
    materialArrow = arrow;
}

ScrollArea::ScrollArea(Ui& u, Id scrollId, const Rect& r, bool isHorizontal, float wheelStep, Bar barStyle)
    : viewport(r), ui(u), id(scrollId), horizontal(isHorizontal), bar(barStyle) {
    auto& s = ui.Data<ScrollState>(id);
    const float view = horizontal ? r.w : r.h;
    const float maxOffset = std::max(0.f, s.content - view);
    barShown = (bar == Bar::Classic || bar == Bar::Material) && !horizontal && maxOffset > 0.5f;

    ui.WheelTarget(id, r);
    if (ui.OwnsWheel(id) && ui.In().wheel != 0.f && maxOffset > 0.f) {
        s.target -= ui.In().wheel / 120.f * wheelStep;
        ui.In().wheel = 0.f;
    }
    s.target = std::clamp(s.target, 0.f, maxOffset);
    if (s.dragging) s.current = s.target;
    else {
        s.current += (s.target - s.current) * std::min(1.f, ui.Dt() * 18.f);
        if (std::abs(s.current - s.target) < 0.5f) s.current = s.target;
        else ui.KeepAnimating();
    }
    s.current = std::clamp(s.current, 0.f, maxOffset);
    offset = s.current;

    ui.PushClip(r);
    if (horizontal) ui.PushTranslate(-offset, 0.f);
    else ui.PushTranslate(0.f, -offset);
}

ScrollArea::~ScrollArea() {
    ui.PopTransform();
    ui.PopClip();

    auto& s = ui.Data<ScrollState>(id);
    s.content = contentSize;
    const float view = horizontal ? viewport.w : viewport.h;
    if (contentSize <= view + 0.5f) {
        s.dragging = false;
        return;
    }
    if ((bar == Bar::Classic || bar == Bar::Material) && !horizontal) {
        DrawClassic(s, view);
        return;
    }
    // Thumb
    const float track = view;
    const float thumbLen = std::max(24.f, track * view / contentSize);
    const float maxOffset = contentSize - view;
    const float pos = (track - thumbLen) * (offset / maxOffset);
    Rect thumb = horizontal ? Rect{ viewport.x + pos, viewport.B() - kBar, thumbLen, kBar }
                            : Rect{ viewport.R() - kBar - 2.f, viewport.y + pos, kBar, thumbLen };
    const Rect hitArea = horizontal ? Rect{ thumb.x, thumb.y - 4.f, thumb.w, thumb.h + 4.f }
                                    : Rect{ thumb.x - 4.f, thumb.y, thumb.w + 6.f, thumb.h };
    const Interaction it = ui.Interact(HashId(id, 0x7B), hitArea, true, false);
    const float mouse = horizontal ? ui.MouseX() : ui.MouseY();
    if (it.held && !s.dragging) {
        s.dragging = true;
        s.dragStartMouse = mouse;
        s.dragStartOffset = s.target;
    }
    if (s.dragging) {
        if (!ui.In().down[0]) s.dragging = false;
        else {
            const float delta = mouse - s.dragStartMouse;
            s.target = std::clamp(s.dragStartOffset + delta * maxOffset / std::max(1.f, track - thumbLen), 0.f, maxOffset);
        }
    }
    const Color c = it.hovered || s.dragging ? Palette::Teal : Palette::Faint;
    ui.g.FillRound(thumb, kBar * 0.5f, c);
}

// Windows 10 WPF ScrollBar: #F0F0F0 track, 17×17 arrow buttons, #CDCDCD thumb.
void ScrollArea::DrawClassic(ScrollState& s, float view) {
    auto& g = ui.g;
    const float maxOffset = contentSize - view;
    const Rect barRect{ viewport.R() - kClassicWidth, viewport.y, kClassicWidth, viewport.h };
    ui.Block(barRect);
    ui.WheelTarget(id, barRect);   // the wheel over the bar scrolls this area
    const bool material = bar == Bar::Material;
    // MaterialDesignScrollBar: the whole bar at 0.5 opacity, 0.8 (eased over 0.2 s) while hovered
    const float barOpacity = material ? ui.Tween(HashId(id, 0x0BA2), ui.MouseOver(barRect) || s.dragging ? 0.8f : 0.5f, 0.2f / 0.3f, 0.5f) : 1.f;
    if (material) g.PushOpacity(barOpacity);
    else g.FillRect(barRect, Hex(0xF0F0F0));

    // Arrow buttons (repeat while held, one line = 16 DIPs like LineUp/LineDown)
    for (int dir = -1; dir <= 1; dir += 2) {
        const Rect button = dir < 0 ? Rect{ barRect.x, barRect.y, kClassicWidth, kClassicWidth }
                                    : Rect{ barRect.x, barRect.B() - kClassicWidth, kClassicWidth, kClassicWidth };
        const Interaction it = ui.Interact(HashId(id, dir < 0 ? 0xA1 : 0xA2), button, true, false);
        float& repeat = ui.Slot(HashId(id, dir < 0 ? 0xB1 : 0xB2));
        if (it.held) {
            repeat -= ui.Dt();
            if (ui.In().pressed[0]) repeat = 0.f;
            if (repeat <= 0.f) {
                s.target = std::clamp(s.target + dir * 16.f, 0.f, maxOffset);
                repeat = ui.In().pressed[0] ? 0.5f : 0.05f;
            }
            ui.KeepAnimating();
        }
        const float cx = button.CX(), cy = button.CY();
        if (material) {
            // 12 px ArrowUp / ArrowDown glyph (Viewbox Margin 2,3,2,1)
            const float ay = cy + 1.f;
            g.Line(cx, ay - 4.5f, cx, ay + 4.5f, materialArrow, 1.6f);
            g.Line(cx - 4.f, ay + dir * -0.5f, cx, ay + dir * 4.5f, materialArrow, 1.6f);
            g.Line(cx + 4.f, ay + dir * -0.5f, cx, ay + dir * 4.5f, materialArrow, 1.6f);
            continue;
        }
        if (it.held) g.FillRect(button, Hex(0x606060));
        else if (it.hovered) g.FillRect(button, Hex(0xDADADA));
        const Color arrow = it.held ? Hex(0xFFFFFF) : it.hovered ? Hex(0x000000) : Hex(0x606060);
        g.Line(cx - 3.5f, cy + dir * -1.75f, cx, cy + dir * 1.75f, arrow, 1.f);
        g.Line(cx, cy + dir * 1.75f, cx + 3.5f, cy + dir * -1.75f, arrow, 1.f);
    }

    // Thumb (registered after the track under it, so it gets the hover and can be dragged)
    const float track = barRect.h - kClassicWidth * 2.f;
    const float thumbLen = std::max(17.f, track * view / contentSize);
    const float pos = (track - thumbLen) * (offset / maxOffset);
    const Rect thumb{ barRect.x + 1.f, barRect.y + kClassicWidth + pos, kClassicWidth - 2.f, thumbLen };
    const Rect trackRect{ barRect.x, barRect.y + kClassicWidth, kClassicWidth, track };
    const Interaction trackIt = ui.Interact(HashId(id, 0x7C), trackRect, true, false);
    const Interaction it = ui.Interact(HashId(id, 0x7B), thumb, true, false);
    const float mouse = ui.MouseY();
    if (it.held && !s.dragging) {
        s.dragging = true;
        s.dragStartMouse = mouse;
        s.dragStartOffset = s.target;
    }
    if (s.dragging) {
        if (!ui.In().down[0]) s.dragging = false;
        else s.target = std::clamp(s.dragStartOffset + (mouse - s.dragStartMouse) * maxOffset / std::max(1.f, track - thumbLen), 0.f, maxOffset);
    }
    // Clicking the track (outside the thumb) pages up / down
    if (trackIt.held && ui.In().pressed[0] && !it.hovered)
        s.target = std::clamp(s.target + (mouse < thumb.y ? -view : view), 0.f, maxOffset);
    if (material) {
        // Rounded thumb that brightens while hovered / dragged
        const float active = ui.Tween(HashId(id, 0x7B7B), it.hovered || s.dragging ? 1.f : 0.f, 0.12f, 0.f);
        const Rect pill = thumb.Inset(2.f + 1.f * (1.f - active), 0.f);
        g.FillRound(pill, pill.w * 0.5f, Lerp(materialThumb, materialArrow, 0.35f * active));
        g.PopLayer();
        return;
    }
    g.FillRect(thumb, s.dragging ? Hex(0x606060) : it.hovered ? Hex(0xA6A6A6) : Hex(0xCDCDCD));
}

void ScrollArea::ScrollTo(Ui& ui, Id id, float offset) {
    auto& s = ui.Data<ScrollState>(id);
    s.target = offset;
    s.current = offset;
}

void ScrollArea::ScrollBy(Ui& ui, Id id, float delta) {
    auto& s = ui.Data<ScrollState>(id);
    s.target += delta;
}

} // namespace SonicRacingSaveManager::UI
