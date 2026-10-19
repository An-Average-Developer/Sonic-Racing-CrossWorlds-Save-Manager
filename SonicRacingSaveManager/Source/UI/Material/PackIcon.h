#pragma once
#include "Graphics/Graphics.h"
#include "UI/Material/IconKind.h"

// materialDesign:PackIcon: the Material Design Icons path data drawn on a 24x24 viewbox.
namespace SonicRacingSaveManager::UI::Material {

const char* IconPathData(Icon icon);   // IconData.cpp (generated)

// Draws the icon scaled uniformly into r (PackIcon's default Stretch is Uniform).
void DrawIcon(Graphics& g, Icon icon, const Rect& r, Color c);
// Square icon of `size` centered on (cx, cy).
void DrawIcon(Graphics& g, Icon icon, float cx, float cy, float size, Color c);

} // namespace SonicRacingSaveManager::UI::Material
