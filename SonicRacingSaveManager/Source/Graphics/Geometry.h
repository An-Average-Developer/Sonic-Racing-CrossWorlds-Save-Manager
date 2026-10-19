#pragma once
#include <d2d1_3.h>
#include <functional>
#include <string_view>
#include <wrl/client.h>

namespace SonicRacingSaveManager::Geometry {

using Microsoft::WRL::ComPtr;

// Builds a path geometry with a sink callback.
ComPtr<ID2D1PathGeometry> Build(const std::function<void(ID2D1GeometrySink*)>& build,
                                D2D1_FILL_MODE fill = D2D1_FILL_MODE_WINDING);

// Parses SVG / XAML path mini-language ("M8,18 C8,15.8 9.8,14 12,14 H24 L29,20 Z").
// Supports M L H V C S Q T A Z in absolute and relative forms, plus XAML's "F0"/"F1" fill rule prefix.
ComPtr<ID2D1PathGeometry> FromPathData(std::string_view data);

ComPtr<ID2D1EllipseGeometry> Ellipse(float cx, float cy, float rx, float ry);
ComPtr<ID2D1RoundedRectangleGeometry> RoundedRect(const D2D1_RECT_F& r, float rx, float ry);
ComPtr<ID2D1RectangleGeometry> Rectangle(const D2D1_RECT_F& r);

// Rounded rectangle with a different radius per corner (XAML CornerRadius="0,0,4,4").
ComPtr<ID2D1PathGeometry> RoundedRect(const D2D1_RECT_F& r, float topLeft, float topRight, float bottomRight, float bottomLeft);

} // namespace SonicRacingSaveManager::Geometry
