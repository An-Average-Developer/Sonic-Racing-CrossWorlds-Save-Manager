#pragma once
#include <string>
#include "Graphics/Images.h"
#include "UI/Style.h"
#include "UI/Ui.h"

namespace SonicRacingSaveManager::UI {

// Button frame with the app-wide WPF template: background + border, opacity 0.85 on hover,
// 0.7 while pressed, 0.4 when disabled. Content is drawn by the caller between construction
// and destruction (it gets the same opacity).
class ButtonScope {
public:
    ButtonScope(Ui& ui, Id id, const Rect& r, const ButtonStyle& style, bool enabled = true, bool sounds = true);
    ~ButtonScope();
    ButtonScope(const ButtonScope&) = delete;
    ButtonScope& operator=(const ButtonScope&) = delete;

    bool Clicked() const { return state.clicked; }
    Interaction state;
    Rect bounds;
    Rect content;   // inside border + padding

private:
    Ui& ui;
    bool layer = false;
    bool scaled = false;
};

// Width a button needs for `text` (Padding + MinWidth + border, like WPF auto sizing).
float ButtonWidth(Ui& ui, const std::wstring& text, const TextStyle& textStyle, const ButtonStyle& style);
float ButtonWidthFor(float contentWidth, const ButtonStyle& style);

// Text centred (or aligned per textStyle) inside a styled button. Returns true when clicked.
bool TextButton(Ui& ui, Id id, const Rect& r, const std::wstring& text, const TextStyle& textStyle, Color textColor,
                const ButtonStyle& style, bool enabled = true);

// Glyph (Segoe MDL2) + text, left aligned inside the padding — the "icon + label" list buttons.
bool GlyphTextButton(Ui& ui, Id id, const Rect& r, wchar_t glyph, float glyphSize, Color glyphColor, const std::wstring& text,
                     const TextStyle& textStyle, Color textColor, const ButtonStyle& style, float gap = 10.f, bool enabled = true);

// Image + text list button.
bool ImageTextButton(Ui& ui, Id id, const Rect& r, const ImagePtr& image, float imageSize, const std::wstring& text,
                     const TextStyle& textStyle, Color textColor, const ButtonStyle& style, float gap = 10.f, bool enabled = true);

// ON / OFF toggle (SetToggle in the old code). Returns true when clicked.
bool Toggle(Ui& ui, Id id, const Rect& r, bool on);
// The right-aligned toggle box width
float ToggleWidth(Ui& ui, bool on);

// Selectable full-width option (window mode, icon theme, corners...). Returns true when clicked.
bool OptionButton(Ui& ui, Id id, const Rect& r, const std::wstring& text, bool selected, float fontSize = 11.f,
                  HAlign align = HAlign::Left, Font font = Font::Mono);
float OptionButtonHeight(Ui& ui, float fontSize = 11.f);

// Hotkey binding button ("Ctrl + Shift + Enter" / "Press keys..." while capturing).
bool KeyBindButton(Ui& ui, Id id, const Rect& r, const std::wstring& text, bool capturing);

// Underlined text link.
bool LinkButton(Ui& ui, Id id, const Rect& r, const std::wstring& text, const TextStyle& style, Color color);

// Draws a Segoe MDL2 glyph centred in r.
void Glyph(Ui& ui, wchar_t glyph, const Rect& r, float size, Color color);

} // namespace SonicRacingSaveManager::UI
