#pragma once
#include <string>
#include <vector>
#include "Graphics/Graphics.h"
#include "UI/Material/PackIcon.h"
#include "UI/Ui.h"

// Pieces shared by the redesigned tabs (the banner is in TabBanner).
namespace SonicRacingSaveManager::Main::TabKit {

// CardStyle defaults (Margin 10, Padding 20)
inline constexpr float kCardMargin = 10.f, kCardPadding = 20.f;

// Text style anchored to the top (wrapping optional) and the heights it takes
TextStyle Style(float size, DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL, bool wrap = false, HAlign align = HAlign::Left);
float LineH(UI::Ui& ui, const TextStyle& style);
float TextH(UI::Ui& ui, const std::wstring& text, const TextStyle& style, float width);

// Arc of a circle (angles in degrees, 0 = up, clockwise)
void Arc(Graphics& g, float cx, float cy, float radius, float startDeg, float sweepDeg, Color c, float width);

// Rounded button: filled in `color` (or outlined), icon + label, glows and lifts on hover, presses.
// `iconAngle` turns the icon (a spinning refresh while checking).
bool PillButton(UI::Ui& ui, UI::Id id, const Rect& r, UI::Material::Icon icon, const std::wstring& text, Color color, bool filled,
                bool enabled = true, float iconAngle = 0.f);
// Width that fits `text` in a PillButton
float PillWidth(UI::Ui& ui, const std::wstring& text);

// A colour made readable as text on the current cards (lighter on dark themes, darker on light ones)
Color Readable(Color c);

// A card with a caps title and numbered steps joined by a line, each sliding in from `revealAt`.
// With `draw` false it only measures; returns the height it needs.
float StepsCard(UI::Ui& ui, const Rect& r, const char* key, const std::wstring& title, const std::vector<std::wstring>& steps, double revealAt,
                bool draw = true);

} // namespace SonicRacingSaveManager::Main::TabKit
