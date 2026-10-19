#include "UI/Material/ToolTip.h"
#include <algorithm>
#include <cmath>
#include "UI/Material/Theme.h"

namespace SonicRacingSaveManager::UI::Material {

void DrawToolTip(Graphics& g, const std::wstring& text, float mouseX, float mouseY) {
    const Palette& p = P();
    TextStyle style = Text(12.f);
    style.wrap = true;
    style.valign = VAlign::Top;
    const TextSize size = g.Measure(text, style, 400.f);
    const float w = std::ceil(size.w) + 16.f, h = std::ceil(size.h) + 16.f;
    float x = mouseX - w * 0.5f, y = mouseY + 20.f;
    x = std::clamp(x, 0.f, std::max(0.f, g.Width() - w));
    if (y + h > g.Height()) y = mouseY - h - 4.f;
    const Rect box{ std::round(x), std::round(y), w, h };
    g.FillRound(box, 2.f, p.toolTipBackground);
    g.Text(text, box.Inset(8.f), p.background, style);
}

} // namespace SonicRacingSaveManager::UI::Material
