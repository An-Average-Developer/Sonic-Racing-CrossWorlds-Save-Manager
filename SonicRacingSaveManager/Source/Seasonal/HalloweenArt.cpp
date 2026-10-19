#include <cmath>
#include "Seasonal/SeasonalArt.h"

// Vector Halloween decorations (pumpkins, witch hats, bats, spiders, trees, ghosts, cobwebs, moon).
namespace SonicRacingSaveManager::Seasonal::Art::Halloween {

namespace {

constexpr Color Orange = Rgba(255, 117, 24);
constexpr Color OrangeLight = Rgba(255, 150, 58);
constexpr Color Stem = Rgba(91, 123, 30);
constexpr Color HatBrush = Rgba(43, 15, 71);
constexpr Color HatBand = Rgba(255, 122, 26);
constexpr Color Night = Rgba(12, 6, 20);
constexpr Color GhostBrush = Rgba(245, 240, 255, 235);
constexpr Color MoonBrush = Rgba(255, 243, 196);
constexpr Color Crater = Rgba(230, 214, 160, 160);
constexpr Color EyeBrush = Rgba(255, 170, 40);

const Pen PumpkinPen{ Rgba(184, 68, 11), 0.9f };
const Pen BucklePen{ Rgba(255, 213, 79), 1.0f };
const Pen OutlinePen{ Rgba(160, 110, 230), 0.8f };
const Pen LegPen{ Rgba(200, 170, 255), 0.9f };
const Pen ThreadPenDark{ Rgba(255, 255, 255, 110), 0.6f };
const Pen WebPenDark{ Rgba(255, 255, 255, 80), 0.7f };
const Pen ThreadPenLight{ Rgba(74, 30, 120, 130), 0.6f };
const Pen WebPenLight{ Rgba(74, 30, 120, 95), 0.7f };
const Pen& ThreadPen() { return LightMode() ? ThreadPenLight : ThreadPenDark; }
const Pen& WebPen() { return LightMode() ? WebPenLight : WebPenDark; }

// ── Shared animations ──
// Flickering candle light inside the pumpkins' faces
Color Glow() {
    const float opacity = Keyframes({ { 0.0, 1.f, false }, { 0.18, 0.55f, false }, { 0.32, 0.9f, false },
                                      { 0.7, 0.7f, false }, { 1.1, 1.f, false }, { 1.6, 1.f, false } });
    return Rgba(255, 214, 79).Fade(opacity);
}
float Bob() { return Wave(0.f, -3.f, 1.4); }          // gentle up/down float (ghosts, bats, hats)
float Swing() { return Wave(-9.f, 9.f, 1.8); }        // pendulum swing (hanging spiders)
float Flap() { return Wave(1.f, 0.3f, 0.17, false); } // wing flap (bats)

constexpr const char* PumpkinStem = "M -1.5,-9 L 1.5,-9 L 3,-14 L 0.5,-14.5 Z";
constexpr const char* PumpkinEyes = "M -6.5,-2.5 L -2.5,-2.5 L -4.5,-6 Z M 2.5,-2.5 L 6.5,-2.5 L 4.5,-6 Z M -1,0.5 L 1,0.5 L 0,-1.5 Z";
constexpr const char* PumpkinMouth = "M -6.5,2.5 L -4.5,4.5 L -2.5,3 L -0.5,5 L 1.5,3 L 3.5,4.5 L 6.5,2.5 C 5,7.5 -5,7.5 -6.5,2.5 Z";
constexpr const char* HatCone = "M -8,-1 L 8,-1 L 2.5,-17 C 2,-22 6,-25 9,-26 C 3,-26 -2,-22 -3,-16 Z";
constexpr const char* HatBandShape = "M -7.3,-3 L 7.3,-3 L 6.6,-6.5 L -6.6,-6.5 Z";
constexpr const char* BatWing = "M 1.8,-1 C 6,-7 11,-8 16,-4 C 13,-3 12.5,-0.5 12.5,1.5 C 10.5,-0.5 8.5,0 7.5,2.5 C 6,0.5 4,0.5 1.8,2 Z";
constexpr const char* BatEars = "M -2.2,-3 L -1.6,-6.5 L -0.4,-3.6 Z M 2.2,-3 L 1.6,-6.5 L 0.4,-3.6 Z";
constexpr const char* GhostBody = "M -8,9 L -8,-2 C -8,-13 8,-13 8,-2 L 8,9 L 5.3,6.5 L 2.7,9 L 0,6.5 L -2.7,9 L -5.3,6.5 Z";
constexpr const char* TreeShape =
    "M -4,0 C -3,-8 -2,-14 -2.2,-20 C -6,-23 -10,-26 -14,-32 L -12,-33 C -9,-29 -5,-26 -1.6,-24 "
    "C -1.2,-30 -3.5,-36 -2.5,-43 L -0.5,-43 C 0,-36 1.2,-31 1.6,-26 C 4,-29 8,-31 11,-37 L 13,-36 "
    "C 10,-30 6,-26 2.2,-21.5 C 2.4,-14 3,-7 5,0 Z "
    "M -8.5,-28.5 L -12,-25 L -11,-24.5 L -8,-27.5 Z M 7,-30 L 9,-27 L 10,-27.5 L 8.5,-30.5 Z";
constexpr const char* TreeMound = "M -14,1 C -8,-4 8,-4 14,1 Z";

// Quarter web anchored at the origin, spreading into +X/+Y
ID2D1PathGeometry* Cobweb() {
    static ComPtr<ID2D1PathGeometry> geo = Geometry::Build([](ID2D1GeometrySink* sink) {
        constexpr double r = 34;
        constexpr double angles[] = { 0.0, 22.5, 45.0, 67.5, 90.0 };
        constexpr double rings[] = { 9.0, 17.0, 25.0, 33.0 };
        const auto P = [](double radius, double deg) {
            return D2D1::Point2F(static_cast<float>(radius * std::cos(deg * 3.14159265358979 / 180)),
                                 static_cast<float>(radius * std::sin(deg * 3.14159265358979 / 180)));
        };
        for (double a : angles) {
            sink->BeginFigure(D2D1::Point2F(0, 0), D2D1_FIGURE_BEGIN_HOLLOW);
            sink->AddLine(P(r, a));
            sink->EndFigure(D2D1_FIGURE_END_OPEN);
        }
        for (double ring : rings) {
            sink->BeginFigure(P(ring, angles[0]), D2D1_FIGURE_BEGIN_HOLLOW);
            for (size_t i = 1; i < std::size(angles); ++i) {
                const double mid = (angles[i - 1] + angles[i]) / 2;
                sink->AddQuadraticBezier(D2D1::QuadraticBezierSegment(P(ring * 0.82, mid), P(ring, angles[i])));
            }
            sink->EndFigure(D2D1_FIGURE_END_OPEN);
        }
    }, D2D1_FILL_MODE_ALTERNATE);
    return geo.Get();
}

} // namespace

void DrawPumpkin(Graphics& g, float x, float y, float scale) {
    At at(g, x, y, scale);
    Ellipse(g, -5.5f, 0.f, 7.f, 9.f, Orange, PumpkinPen);
    Ellipse(g, 5.5f, 0.f, 7.f, 9.f, Orange, PumpkinPen);
    Ellipse(g, 0.f, 0.f, 6.5f, 9.6f, OrangeLight, PumpkinPen);
    Fill(g, Path(PumpkinStem), Stem);
    const Color glow = Glow();
    Fill(g, Path(PumpkinEyes), glow);
    Fill(g, Path(PumpkinMouth), glow);
}

void DrawWitchHat(Graphics& g, float x, float y, float scale, float angle) {
    At at(g, x, y, scale, angle);
    Push bob(g, Translate(0.f, Bob()));
    Ellipse(g, 0.f, 0.f, 14.f, 3.6f, HatBrush, OutlinePen);
    FillStroke(g, Path(HatCone), HatBrush, OutlinePen);
    Fill(g, Path(HatBandShape), HatBand);
    StrokeRectangle(g, { -2.f, -6.3f, 4.f, 3.1f }, BucklePen);
}

void DrawBat(Graphics& g, float x, float y, float scale, bool bob) {
    At at(g, x, y, scale);
    if (bob) g.PushTransform(Translate(0.f, Bob()));
    {
        Push flap(g, Scale(1.f, Flap()));
        FillStroke(g, Path(BatWing), Night, OutlinePen);
        Push mirror(g, Scale(-1.f, 1.f));
        FillStroke(g, Path(BatWing), Night, OutlinePen);
    }
    FillStroke(g, Path(BatEars), Night, OutlinePen);
    Ellipse(g, 0.f, 0.f, 2.6f, 3.8f, Night, OutlinePen);
    Ellipse(g, -0.9f, -1.4f, 0.55f, 0.55f, EyeBrush);
    Ellipse(g, 0.9f, -1.4f, 0.55f, 0.55f, EyeBrush);
    if (bob) g.PopTransform();
}

void DrawSpider(Graphics& g, float x, float y, float length, float scale) {
    Push at(g, Translate(x, y));
    Push swing(g, Rotate(Swing()));
    Line(g, 0.f, 0.f, 0.f, length, ThreadPen());
    Push down(g, Translate(0.f, length));
    Push size(g, Scale(scale));
    for (int side = -1; side <= 1; side += 2) {
        for (int i = 0; i < 4; ++i) {
            const float y0 = -1.5f + i * 1.3f;
            Line(g, side * 2.f, y0, side * 5.5f, y0 - 2.5f + i * 0.8f, LegPen);
            Line(g, side * 5.5f, y0 - 2.5f + i * 0.8f, side * 7.5f, y0 + 1.5f + i * 1.2f, LegPen);
        }
    }
    Ellipse(g, 0.f, 1.5f, 3.2f, 4.f, Night, OutlinePen);
    Ellipse(g, 0.f, -3.f, 2.2f, 2.f, Night, OutlinePen);
    Ellipse(g, -0.8f, -3.2f, 0.5f, 0.5f, EyeBrush);
    Ellipse(g, 0.8f, -3.2f, 0.5f, 0.5f, EyeBrush);
}

void DrawGhost(Graphics& g, float x, float y, float scale) {
    At at(g, x, y, scale);
    Push bob(g, Translate(0.f, Bob()));
    FillStroke(g, Path(GhostBody), GhostBrush, OutlinePen);
    Ellipse(g, -2.8f, -3.f, 1.4f, 2.f, Night);
    Ellipse(g, 2.8f, -3.f, 1.4f, 2.f, Night);
    Ellipse(g, 0.f, 1.5f, 1.6f, 1.2f, Night);
}

void DrawTree(Graphics& g, float x, float y, float scale) {
    At at(g, x, y, scale);
    FillStroke(g, Path(TreeMound), Night, OutlinePen);
    FillStroke(g, Path(TreeShape), Night, OutlinePen);
}

void DrawCobweb(Graphics& g, float x, float y, float scale, bool flipX, bool flipY) {
    Push at(g, Translate(x, y));
    Push size(g, Scale(flipX ? -scale : scale, flipY ? -scale : scale));
    Stroke(g, Cobweb(), WebPen());
}

void DrawMoon(Graphics& g, float x, float y, float radius) {
    GlowEllipse(g, x, y, radius * 2.f, radius * 2.f, Rgba(255, 214, 120, 90));
    Ellipse(g, x, y, radius, radius, MoonBrush);
    Ellipse(g, x - radius * 0.35f, y - radius * 0.2f, radius * 0.22f, radius * 0.22f, Crater);
    Ellipse(g, x + radius * 0.3f, y + radius * 0.35f, radius * 0.15f, radius * 0.15f, Crater);
    Ellipse(g, x + radius * 0.25f, y - radius * 0.4f, radius * 0.1f, radius * 0.1f, Crater);
}

} // namespace SonicRacingSaveManager::Seasonal::Art::Halloween
