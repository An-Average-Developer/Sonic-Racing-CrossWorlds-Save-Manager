#pragma once
#include <d2d1.h>
#include <cstdint>

namespace SonicRacingSaveManager {

struct Color {
    float r = 0.f, g = 0.f, b = 0.f, a = 1.f;

    constexpr Color() = default;
    constexpr Color(float r_, float g_, float b_, float a_ = 1.f) : r(r_), g(g_), b(b_), a(a_) {}

    constexpr Color WithAlpha(float alpha) const { return { r, g, b, alpha }; }
    constexpr Color Fade(float factor) const { return { r, g, b, a * factor }; }
    constexpr bool Transparent() const { return a <= 0.f; }
    D2D1_COLOR_F ToD2D() const { return D2D1::ColorF(r, g, b, a); }
    uint32_t Packed() const;  // 0xAARRGGBB
};

// 0xRRGGBB (like "#00CCAA" in XAML)
constexpr Color Hex(uint32_t rgb, float alpha = 1.f) {
    return { ((rgb >> 16) & 0xFF) / 255.f, ((rgb >> 8) & 0xFF) / 255.f, (rgb & 0xFF) / 255.f, alpha };
}

// 0xAARRGGBB (like "#BB000000" in XAML)
constexpr Color Argb(uint32_t argb) {
    return { ((argb >> 16) & 0xFF) / 255.f, ((argb >> 8) & 0xFF) / 255.f, (argb & 0xFF) / 255.f, ((argb >> 24) & 0xFF) / 255.f };
}

// Color.FromArgb(a, r, g, b) with byte components
constexpr Color Rgba(int r, int g, int b, int a = 255) { return { r / 255.f, g / 255.f, b / 255.f, a / 255.f }; }

inline Color Lerp(const Color& x, const Color& y, float t) {
    return { x.r + (y.r - x.r) * t, x.g + (y.g - x.g) * t, x.b + (y.b - x.b) * t, x.a + (y.a - x.a) * t };
}

inline uint32_t Color::Packed() const {
    const auto byte = [](float v) { return static_cast<uint32_t>((v < 0.f ? 0.f : v > 1.f ? 1.f : v) * 255.f + 0.5f); };
    return byte(a) << 24 | byte(r) << 16 | byte(g) << 8 | byte(b);
}

namespace Colors {
inline constexpr Color Transparent{ 0.f, 0.f, 0.f, 0.f };
inline constexpr Color White{ 1.f, 1.f, 1.f, 1.f };
inline constexpr Color Black{ 0.f, 0.f, 0.f, 1.f };
} // namespace Colors

} // namespace SonicRacingSaveManager
