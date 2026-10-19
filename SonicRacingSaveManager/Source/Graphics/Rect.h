#pragma once
#include <d2d1.h>

namespace SonicRacingSaveManager {

// Thickness, like XAML Margin / Padding / BorderThickness ("left,top,right,bottom").
struct Edges {
    float l = 0.f, t = 0.f, r = 0.f, b = 0.f;
    constexpr Edges() = default;
    constexpr Edges(float all) : l(all), t(all), r(all), b(all) {}
    constexpr Edges(float horizontal, float vertical) : l(horizontal), t(vertical), r(horizontal), b(vertical) {}
    constexpr Edges(float left, float top, float right, float bottom) : l(left), t(top), r(right), b(bottom) {}
    constexpr float H() const { return l + r; }
    constexpr float V() const { return t + b; }
};

struct Rect {
    float x = 0.f, y = 0.f, w = 0.f, h = 0.f;

    constexpr float R() const { return x + w; }
    constexpr float B() const { return y + h; }
    constexpr float CX() const { return x + w * 0.5f; }
    constexpr float CY() const { return y + h * 0.5f; }

    constexpr bool Contains(float px, float py) const { return px >= x && py >= y && px < x + w && py < y + h; }
    constexpr bool Empty() const { return w <= 0.f || h <= 0.f; }

    constexpr Rect Inset(float d) const { return { x + d, y + d, w - 2.f * d, h - 2.f * d }; }
    constexpr Rect Inset(float dx, float dy) const { return { x + dx, y + dy, w - 2.f * dx, h - 2.f * dy }; }
    constexpr Rect Inset(const Edges& e) const { return { x + e.l, y + e.t, w - e.H(), h - e.V() }; }
    constexpr Rect Outset(const Edges& e) const { return { x - e.l, y - e.t, w + e.H(), h + e.V() }; }
    constexpr Rect Offset(float dx, float dy) const { return { x + dx, y + dy, w, h }; }

    // Pieces of the rectangle (XAML-like docking)
    constexpr Rect Top(float height) const { return { x, y, w, height }; }
    constexpr Rect Bottom(float height) const { return { x, B() - height, w, height }; }
    constexpr Rect Left(float width) const { return { x, y, width, h }; }
    constexpr Rect Right(float width) const { return { R() - width, y, width, h }; }
    constexpr Rect CutTop(float height) const { return { x, y + height, w, h - height }; }
    constexpr Rect CutBottom(float height) const { return { x, y, w, h - height }; }
    constexpr Rect CutLeft(float width) const { return { x + width, y, w - width, h }; }
    constexpr Rect CutRight(float width) const { return { x, y, w - width, h }; }

    // A w×h box centred inside this one
    constexpr Rect Center(float width, float height) const { return { x + (w - width) * 0.5f, y + (h - height) * 0.5f, width, height }; }
    constexpr Rect CenterV(float height) const { return { x, y + (h - height) * 0.5f, w, height }; }
    constexpr Rect CenterH(float width) const { return { x + (w - width) * 0.5f, y, width, h }; }

    Rect Intersect(const Rect& o) const;
    D2D1_RECT_F D2D() const { return D2D1::RectF(x, y, x + w, y + h); }
};

inline Rect Rect::Intersect(const Rect& o) const {
    const float l = x > o.x ? x : o.x, t = y > o.y ? y : o.y;
    const float r = R() < o.R() ? R() : o.R(), b = B() < o.B() ? B() : o.B();
    return { l, t, r > l ? r - l : 0.f, b > t ? b - t : 0.f };
}

} // namespace SonicRacingSaveManager
