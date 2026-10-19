#include "UI/Widgets/Button.h"
#include <algorithm>

namespace SonicRacingSaveManager::UI {

ButtonScope::ButtonScope(Ui& u, Id id, const Rect& r, const ButtonStyle& style, bool enabled, bool sounds) : ui(u) {
    bounds = r;
    state = ui.Interact(id, r, enabled, sounds);
    float target = 1.f;
    if (!enabled) target = 0.4f;
    else if (state.held && state.hovered) target = 0.7f;
    else if (state.hovered) target = 0.85f;
    // Hover / press fade smoothly instead of jumping (enabled state changes stay instant)
    const float opacity = enabled ? ui.Animate(HashId(id, 0xB077), target, 22.f) : target;
    if (opacity < 0.999f) {
        ui.g.PushOpacity(opacity);
        layer = true;
    }
    // Slight press-in while held
    const float press = ui.Animate(HashId(id, 0xB5CA), state.held && state.hovered ? 0.965f : 1.f, 30.f);
    if (press < 0.999f) {
        ui.PushScale(press, r.CX(), r.CY());
        scaled = true;
    }
    ui.g.Border(r, style.radius, style.background, style.border, style.thickness);
    content = r.Inset(style.thickness).Inset(style.padding);
}

ButtonScope::~ButtonScope() {
    if (scaled) ui.PopTransform();
    if (layer) ui.g.PopLayer();
}

float ButtonWidthFor(float contentWidth, const ButtonStyle& style) {
    return std::max(style.minWidth, contentWidth + style.padding.H() + style.thickness * 2.f);
}

float ButtonWidth(Ui& ui, const std::wstring& text, const TextStyle& textStyle, const ButtonStyle& style) {
    return ButtonWidthFor(ui.g.Measure(text, textStyle).w, style);
}

bool TextButton(Ui& ui, Id id, const Rect& r, const std::wstring& text, const TextStyle& textStyle, Color textColor,
                const ButtonStyle& style, bool enabled) {
    ButtonScope b(ui, id, r, style, enabled);
    TextStyle t = textStyle;
    if (t.align == HAlign::Left && style.padding.l == 0.f && style.padding.r == 0.f) t.align = HAlign::Center;
    t.valign = VAlign::Center;
    ui.g.Text(text, b.content, textColor, t);
    return b.Clicked();
}

void Glyph(Ui& ui, wchar_t glyph, const Rect& r, float size, Color color) {
    TextStyle s = IconFont(size);
    s.valign = VAlign::Center;
    ui.g.Text(std::wstring(1, glyph), r, color, s);
}

bool GlyphTextButton(Ui& ui, Id id, const Rect& r, wchar_t glyph, float glyphSize, Color glyphColor, const std::wstring& text,
                     const TextStyle& textStyle, Color textColor, const ButtonStyle& style, float gap, bool enabled) {
    ButtonScope b(ui, id, r, style, enabled);
    const float gw = std::max(glyphSize + 2.f, 18.f);
    Glyph(ui, glyph, { b.content.x, b.content.y, gw, b.content.h }, glyphSize, glyphColor);
    TextStyle t = textStyle;
    t.valign = VAlign::Center;
    ui.g.Text(text, b.content.CutLeft(gw + gap), textColor, t);
    return b.Clicked();
}

bool ImageTextButton(Ui& ui, Id id, const Rect& r, const ImagePtr& image, float imageSize, const std::wstring& text,
                     const TextStyle& textStyle, Color textColor, const ButtonStyle& style, float gap, bool enabled) {
    ButtonScope b(ui, id, r, style, enabled);
    if (image) ui.g.DrawImage(*image, b.content.Left(imageSize).CenterV(imageSize));
    TextStyle t = textStyle;
    t.valign = VAlign::Center;
    ui.g.Text(text, b.content.CutLeft(imageSize + gap), textColor, t);
    return b.Clicked();
}

float ToggleWidth(Ui& ui, bool on) {
    return ButtonWidth(ui, on ? L"ON" : L"OFF", Mono(10.f, true), Styles::Toggle());
}

bool Toggle(Ui& ui, Id id, const Rect& r, bool on) {
    return TextButton(ui, id, r, on ? L"ON" : L"OFF", Mono(10.f, true).Center(), on ? Palette::Teal : Palette::OffText,
                      Styles::Toggle());
}

float OptionButtonHeight(Ui& ui, float fontSize) {
    return ui.g.Measure(L"Ag", Mono(fontSize, true)).h + 20.f + 2.f;
}

bool OptionButton(Ui& ui, Id id, const Rect& r, const std::wstring& text, bool selected, float fontSize, HAlign align, Font font) {
    TextStyle t = Mono(fontSize, true);
    t.font = font;
    t.align = align;
    return TextButton(ui, id, r, text, t, selected ? Palette::Teal : Palette::Dim,
                      selected ? Styles::OptionSelected() : Styles::Option());
}

bool KeyBindButton(Ui& ui, Id id, const Rect& r, const std::wstring& text, bool capturing) {
    ButtonStyle s = Styles::KeyBind();
    if (capturing) s.border = Palette::Teal;
    return TextButton(ui, id, r, text, Mono(10.f).Center(), capturing ? Palette::Teal : Palette::Label, s);
}

bool LinkButton(Ui& ui, Id id, const Rect& r, const std::wstring& text, const TextStyle& style, Color color) {
    ButtonScope b(ui, id, r, Styles::Flat());
    TextStyle t = style;
    t.underline = true;
    ui.g.Text(text, r, color, t);
    return b.Clicked();
}

} // namespace SonicRacingSaveManager::UI
