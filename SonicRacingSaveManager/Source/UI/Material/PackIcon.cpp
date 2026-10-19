#include "UI/Material/PackIcon.h"
#include <algorithm>
#include <unordered_map>
#include "Graphics/Geometry.h"

namespace SonicRacingSaveManager::UI::Material {

namespace {

// Parsed once per kind (the path data never changes)
ID2D1PathGeometry* Geometry(Icon icon) {
    static std::unordered_map<int, ComPtr<ID2D1PathGeometry>> cache;
    auto& slot = cache[static_cast<int>(icon)];
    if (!slot) slot = Geometry::FromPathData(IconPathData(icon));
    return slot.Get();
}

} // namespace

void DrawIcon(Graphics& g, Icon icon, const Rect& r, Color c) {
    ID2D1PathGeometry* geometry = Geometry(icon);
    if (!geometry || r.w <= 0.f || r.h <= 0.f) return;
    const float s = std::min(r.w, r.h) / 24.f;
    const float ox = r.x + (r.w - 24.f * s) * 0.5f, oy = r.y + (r.h - 24.f * s) * 0.5f;
    g.PushTransform(D2D1::Matrix3x2F::Scale(s, s) * D2D1::Matrix3x2F::Translation(ox, oy));
    g.FillGeometry(geometry, c);
    g.PopTransform();
}

void DrawIcon(Graphics& g, Icon icon, float cx, float cy, float size, Color c) {
    DrawIcon(g, icon, { cx - size * 0.5f, cy - size * 0.5f, size, size }, c);
}

} // namespace SonicRacingSaveManager::UI::Material
