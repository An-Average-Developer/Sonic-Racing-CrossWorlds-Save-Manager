#include "UI/Material/Toggles.h"
#include <algorithm>
#include "Graphics/Animation.h"
#include "Graphics/Geometry.h"
#include "UI/Material/PackIcon.h"

namespace SonicRacingSaveManager::UI::Material {

namespace {

constexpr float kBoxSize = 18.f;   // CheckBoxAssist.CheckBoxSize / RadioButtonAssist.RadioButtonSize

const char* kRadioBlank = "M12,20A8,8 0 0,1 4,12A8,8 0 0,1 12,4A8,8 0 0,1 20,12A8,8 0 0,1 12,20M12,2A10,10 0 0,0 2,12A10,10 0 0,0 12,22A10,10 0 0,0 22,12A10,10 0 0,0 12,2Z";
const char* kRadioMarked = "M12,20A8,8 0 0,1 4,12A8,8 0 0,1 12,4A8,8 0 0,1 20,12A8,8 0 0,1 12,20M12,2A10,10 0 0,0 2,12A10,10 0 0,0 12,22A10,10 0 0,0 22,12A10,10 0 0,0 12,2M12,7A5,5 0 0,0 7,12A5,5 0 0,0 12,17A5,5 0 0,0 17,12A5,5 0 0,0 12,7Z";

void FillPath(Graphics& g, ID2D1Geometry* geometry, const Rect& r, Color c) {
    const float s = r.w / 24.f;
    g.PushTransform(D2D1::Matrix3x2F::Scale(s, s) * D2D1::Matrix3x2F::Translation(r.x, r.y));
    g.FillGeometry(geometry, c);
    g.PopTransform();
}

// The InteractionEllipse pulse played on click: grows to twice the box and fades out (0.3 s)
void ClickPulse(Ui& ui, Id id, bool clicked, float cx, float cy, Color c) {
    double& start = ui.Data<double>(HashId(id, 0x9015E));
    if (clicked) start = ui.Time();
    if (start <= 0.0) return;
    const float t = static_cast<float>((ui.Time() - start) / 0.3);
    if (t >= 1.f) {
        start = 0.0;
        return;
    }
    ui.KeepAnimating();
    const float rad = kBoxSize * Anim::EaseOutCubic(t);
    ui.g.FillEllipse(cx, cy, rad, rad, c.Fade(0.3f * (1.f - t)));
}

// Box/circle + content shared by CheckBox and RadioButton
bool OptionRow(Ui& ui, Id id, const Rect& r, const std::wstring& text, const TextStyle& style, Color foreground, bool enabled,
               float disabledOpacity, const std::function<void(const Rect&)>& drawMark) {
    const Interaction it = ui.Interact(id, r, enabled);
    if (!enabled) ui.g.PushOpacity(disabledOpacity);
    const Rect box{ r.x, r.CY() - kBoxSize * 0.5f, kBoxSize, kBoxSize };
    ClickPulse(ui, id, it.clicked, box.CX(), box.CY(), foreground);
    drawMark(box);
    if (!text.empty()) {
        TextStyle t = style;
        t.valign = VAlign::Center;
        // Padding="4,2,0,0"
        ui.g.Text(text, { box.R() + 4.f, r.y + 2.f, r.R() - box.R() - 4.f, r.h - 2.f }, foreground, t);
    }
    if (!enabled) ui.g.PopLayer();
    return it.clicked;
}

} // namespace

bool Switch(Ui& ui, Id id, const Rect& r, bool on, bool enabled) {
    const Palette& p = P();
    const Rect track{ r.CX() - kSwitchWidth * 0.5f, r.CY() - kSwitchHeight * 0.5f, kSwitchWidth, kSwitchHeight };
    const Interaction it = ui.Interact(id, track, enabled);

    // Thumb slides 0 -> 20 with BackEase (Amplitude .35) over 0.2 s
    const float pos = ui.Tween(HashId(id, 0x5A1D), on ? 1.f : 0.f, 0.2f, on ? 1.f : 0.f);
    const float eased = on ? Anim::EaseOutBack(pos, 0.35f) : 1.f - Anim::EaseOutBack(1.f - pos, 0.35f);
    const bool pressed = it.held && it.hovered;

    // Track: border 2 (ForegroundLight off, Primary on), fill TrackOffBackground off, Primary on
    const float trackAlpha = enabled ? 1.f : 0.12f;
    const Color fill = Lerp(p.switchTrackOff, p.primaryMid.color, pos).Fade(trackAlpha);
    const Color border = Lerp(p.foregroundLight, p.primaryMid.color, pos).Fade(trackAlpha);
    ui.g.Border(track, 16.f, fill, border, 2.f);

    // Thumb: 16 off, 24 on, 28 while pressed; ForegroundLight off (Foreground on hover), paper on
    float size = Anim::Lerp(16.f, 24.f, pos);
    if (pressed) size = 28.f;
    const float cx = track.x + 16.f + 20.f * eased, cy = track.CY();
    // State layer: a 40 px halo (16 x 2.5) at .08 on hover, .12 pressed
    if (enabled && (it.hovered || pressed)) {
        const Color layer = on ? p.primaryMid.color : p.foreground;
        ui.g.FillEllipse(cx, cy, 20.f, 20.f, layer.Fade(pressed ? 0.12f : 0.08f));
    }
    Color thumb = on ? p.background : (it.hovered ? p.foreground : p.foregroundLight);
    if (!on && pos > 0.f) thumb = Lerp(p.foregroundLight, p.background, pos);
    if (!enabled) thumb = thumb.Fade(0.38f);
    ui.g.FillEllipse(cx, cy, size * 0.5f, size * 0.5f, thumb);
    // OnContent: check icon in the primary color
    if (pos > 0.5f) DrawIcon(ui.g, Icon::Check, cx, cy, 16.f, p.primaryMid.color.Fade((pos - 0.5f) * 2.f * (enabled ? 1.f : 0.38f)));
    return it.clicked;
}

bool CheckBox(Ui& ui, Id id, const Rect& r, bool checked, const std::wstring& text, const TextStyle& style, Color foreground,
              bool enabled) {
    return OptionRow(ui, id, r, text, style, foreground, enabled, 0.56f, [&](const Rect& box) {
        const Palette& p = P();
        Color fill = checked ? p.primaryMid.color : p.foregroundLight;
        if (!enabled) fill = p.checkBoxDisabled;
        DrawIcon(ui.g, checked ? Icon::CheckboxMarked : Icon::CheckboxBlankOutline, box, fill);
    });
}

float CheckBoxWidth(Ui& ui, const std::wstring& text, const TextStyle& style) {
    return kBoxSize + 4.f + ui.g.Measure(text, style).w + 2.f;
}

bool RadioButton(Ui& ui, Id id, const Rect& r, bool checked, const std::wstring& text, const TextStyle& style, Color foreground,
                 bool enabled) {
    return OptionRow(ui, id, r, text, style, foreground, enabled, 0.26f, [&](const Rect& box) {
        static const auto blank = Geometry::FromPathData(kRadioBlank);
        static const auto marked = Geometry::FromPathData(kRadioMarked);
        const Palette& p = P();
        Color fill = checked ? p.primaryMid.color : p.foregroundLight;   // RadioButton.Outline when unchecked
        if (!enabled) fill = p.checkBoxDisabled;
        FillPath(ui.g, (checked ? marked : blank).Get(), box, fill);
    });
}

} // namespace SonicRacingSaveManager::UI::Material
