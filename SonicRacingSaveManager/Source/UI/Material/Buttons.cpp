#include "UI/Material/Buttons.h"
#include <algorithm>
#include "Graphics/Animation.h"
#include "UI/Material/Elevation.h"

namespace SonicRacingSaveManager::UI::Material {

namespace {

TextStyle ButtonText() {
    TextStyle t = Medium(14.f);
    t.align = HAlign::Center;
    t.valign = VAlign::Center;
    return t;
}

} // namespace

ButtonLook Raised() {
    ButtonLook l = RaisedStatic();
    l.hoverElevation = 8.f;
    return l;
}

ButtonLook RaisedStatic() {
    const Palette& p = P();
    ButtonLook l;
    l.background = p.primaryMid.color;
    l.border = p.primaryMid.color;
    l.borderThickness = 1.f;
    l.foreground = p.primaryMid.foreground;
    l.ripple = Colors::White;
    l.elevation = 2.f;
    l.darkenOnHover = true;
    l.text = ButtonText();
    return l;
}

ButtonLook RaisedSecondary() {
    const Palette& p = P();
    ButtonLook l = RaisedStatic();
    l.background = p.secondaryMid.color;
    l.border = p.secondaryMid.color;
    l.foreground = p.secondaryMid.foreground;
    return l;
}

ButtonLook SeasonalAccent() {
    ButtonLook l = RaisedStatic();
    l.foreground = Colors::Black;
    l.disabledForeground = P().foreground;
    return l;
}

ButtonLook Flat() {
    ButtonLook l;
    l.foreground = P().primaryMid.color;
    l.ripple = P().primaryMid.color;
    l.hoverOverlay = true;
    l.text = ButtonText();
    return l;
}

ButtonLook Outlined() {
    ButtonLook l = Flat();
    l.border = P().primaryMid.color;
    l.borderThickness = 1.f;
    return l;
}

ButtonLook CustomOutlined() {
    ButtonLook l = Outlined();
    l.border = P().divider;
    return l;
}

ButtonLook Fab(Color background) {
    ButtonLook l;
    l.background = background;
    l.border = background;
    l.borderThickness = 1.f;
    l.foreground = ContrastingForeground(background);
    l.ripple = Colors::White;
    l.elevation = 6.f;
    l.darkenOnHover = true;
    l.ellipse = true;
    l.padding = Edges(1.f);
    l.text = ButtonText();
    return l;
}

ButtonLook IconButtonLook() { return IconButtonLook(P().primaryMid.color); }

ButtonLook IconButtonLook(Color foreground) {
    ButtonLook l;
    l.foreground = foreground;
    l.ripple = foreground;
    l.ellipse = true;
    l.hoverOverlay = true;
    l.padding = Edges(0.f);
    l.text = ButtonText();
    return l;
}

Button::Button(Ui& u, Id id, const Rect& r, const ButtonLook& look, bool enabled) : ui(u) {
    bounds = r;
    state = ui.Interact(id, r, enabled);
    foreground = look.foreground;
    if (!enabled) {
        if (!look.disabledForeground.Transparent()) foreground = look.disabledForeground;
        ui.g.PushOpacity(0.38f);
        layer = true;
    }
    const float radius = look.ellipse ? std::min(r.w, r.h) * 0.5f : look.radius;
    // Redesign: a quick press (the hit test above stays on the unscaled rect)
    const float press = ui.Tween(HashId(id, 0x9E55), state.held ? 1.f : 0.f, 0.08f, 0.f);
    if (press > 0.f) {
        ui.PushScale(1.f - 0.03f * press, r.CX(), r.CY());
        pressed = true;
    }

    // The implicit Button style animates the elevation 2 -> 8 over 0.2 s while hovered
    float dp = look.elevation;
    if (look.hoverElevation >= 0.f) {
        const float t = ui.Tween(HashId(id, 0xE1E7), state.hovered ? 1.f : 0.f, 0.2f, 0.f);
        dp = Anim::Lerp(look.elevation, look.hoverElevation, t);
    }
    const bool darken = look.darkenOnHover && state.hovered;
    if (dp > 0.f && look.ellipse && look.background.Transparent())
        RingElevation(ui.g, r.CX(), r.CY(), radius, look.borderThickness, dp, darken);
    else if (dp > 0.f)
        Elevation(ui.g, r, radius, dp, darken);

    if (look.ellipse) {
        const float rad = std::min(r.w, r.h) * 0.5f;
        ui.g.FillEllipse(r.CX(), r.CY(), rad, rad, look.background);
        if (look.borderThickness > 0.f && !look.border.Transparent()) {
            const float t = look.borderThickness;
            ui.g.StrokeEllipse(r.CX(), r.CY(), rad - t * 0.5f, rad - t * 0.5f, look.border, t);
        }
    } else {
        ui.g.Border(r, radius, look.background, look.border, look.borderThickness);
    }
    if (look.hoverOverlay && state.hovered) {
        if (look.ellipse) ui.g.FillEllipse(r.CX(), r.CY(), r.w * 0.5f, r.h * 0.5f, foreground.Fade(0.16f));
        else ui.g.FillRound(r, radius, foreground.Fade(0.16f));
    }
    Ripple(ui, id, state, r, radius, look.ripple);
    content = r.Inset(look.borderThickness).Inset(look.padding);
}

Button::~Button() {
    if (pressed) ui.PopTransform();
    if (layer) ui.g.PopLayer();
}

float ButtonWidth(Ui& ui, const std::wstring& text, const ButtonLook& look, float iconSize, float gap) {
    float w = text.empty() ? 0.f : ui.g.Measure(text, look.text).w;
    if (iconSize > 0.f) w += iconSize + (text.empty() ? 0.f : gap);
    return w + look.padding.H() + look.borderThickness * 2.f;
}

bool TextButton(Ui& ui, Id id, const Rect& r, const std::wstring& text, const ButtonLook& look, bool enabled) {
    Button b(ui, id, r, look, enabled);
    ui.g.Text(text, b.content, b.Foreground(), look.text);
    return b.Clicked();
}

bool IconTextButton(Ui& ui, Id id, const Rect& r, Icon icon, const std::wstring& text, const ButtonLook& look, bool enabled,
                    float iconSize, float gap) {
    Button b(ui, id, r, look, enabled);
    const float textW = text.empty() ? 0.f : ui.g.Measure(text, look.text).w;
    const float total = iconSize + (text.empty() ? 0.f : gap + textW);
    const float x = b.content.CX() - total * 0.5f;
    DrawIcon(ui.g, icon, { x, b.content.CY() - iconSize * 0.5f, iconSize, iconSize }, b.Foreground());
    if (!text.empty()) {
        TextStyle t = look.text;
        t.align = HAlign::Left;
        ui.g.Text(text, { x + iconSize + gap, b.content.y, textW + 2.f, b.content.h }, b.Foreground(), t);
    }
    return b.Clicked();
}

bool IconButton(Ui& ui, Id id, const Rect& r, Icon icon, float iconSize, const ButtonLook& look, bool enabled, Color iconColor) {
    Button b(ui, id, r, look, enabled);
    DrawIcon(ui.g, icon, r.CX(), r.CY(), iconSize, iconColor.Transparent() ? b.Foreground() : iconColor);
    return b.Clicked();
}

bool ImageFab(Ui& ui, Id id, const Rect& r, const ImagePtr& image, float imageSize, const ButtonLook& look, bool clip,
              Stretch stretch) {
    Button b(ui, id, r, look, true);
    if (image) {
        const Rect ir{ r.CX() - imageSize * 0.5f, r.CY() - imageSize * 0.5f, imageSize, imageSize };
        if (clip) ui.g.DrawImageRounded(*image, ir, imageSize * 0.5f, 1.f, stretch);
        else ui.g.DrawImage(*image, ir, 1.f, stretch);
    }
    return b.Clicked();
}

} // namespace SonicRacingSaveManager::UI::Material
