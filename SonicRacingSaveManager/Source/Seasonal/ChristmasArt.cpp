#include <algorithm>
#include <cmath>
#include "Seasonal/SeasonalArt.h"

// Vector Christmas decorations (Santa hats, trees, gifts, candy canes, snowmen, ornaments, holly,
// gingerbread, string lights, snowflakes).
namespace SonicRacingSaveManager::Seasonal::Art::Christmas {

namespace {

constexpr Color Red = Rgba(211, 47, 47);
constexpr Color Green = Rgba(46, 125, 50);
constexpr Color GreenDark = Rgba(27, 94, 32);
constexpr Color Gold = Rgba(255, 202, 40);
constexpr Color White = Rgba(255, 255, 255);
constexpr Color Snow = Rgba(245, 250, 255);
constexpr Color TrunkBrush = Rgba(121, 85, 72);
constexpr Color Ginger = Rgba(196, 128, 64);
constexpr Color Coal = Rgba(40, 40, 45);
constexpr Color Carrot = Rgba(255, 138, 40);
constexpr Color Berry = Rgba(229, 57, 53);

const Pen OutlinePen{ Rgba(30, 30, 40, 120), 0.7f };
const Pen RibbonPen{ Gold, 1.6f };
const Pen IcingPen{ White, 0.8f };
const Pen WirePen{ Rgba(40, 90, 50), 1.0f };
const Pen StripePen{ Red, 2.2f };
const Pen CaneOutline{ Rgba(200, 200, 210), 4.2f };
const Pen CaneWhite{ White, 3.2f };
const Pen FlakePenDark{ Rgba(255, 255, 255, 220), 1.0f };
const Pen FlakePenLight{ Rgba(120, 170, 215, 230), 1.0f };
const Pen ThreadPenDark{ Rgba(255, 255, 255, 110), 0.6f };
const Pen ThreadPenLight{ Rgba(80, 80, 90, 120), 0.6f };
const Pen& FlakePen() { return LightMode() ? FlakePenLight : FlakePenDark; }
const Pen& ThreadPen() { return LightMode() ? ThreadPenLight : ThreadPenDark; }
constexpr Color SnowShaded = Rgba(214, 230, 245);
const Pen TreeOutline{ GreenDark, 0.8f };

// ── Shared animations ──
Color TwinkleBrush(int i) {
    switch (i % 4) {
        case 0: return Rgba(255, 235, 59).Fade(Wave(1.f, 0.25f, 0.7, false, 0.0));
        case 1: return Rgba(105, 240, 174).Fade(Wave(1.f, 0.3f, 0.9, false, 0.35));
        case 2: return Rgba(255, 82, 82).Fade(Wave(0.25f, 1.f, 0.7, false, 0.0));
        default: return Rgba(64, 196, 255).Fade(Wave(0.3f, 1.f, 0.9, false, 0.35));
    }
}
Color StarGlow() { return Rgba(255, 214, 60, 90).Fade(Wave(0.4f, 1.f, 1.2)); }   // tree-top star glow
float Swing() { return Wave(-7.f, 7.f, 1.9); }       // hanging ornaments
float Bob() { return Wave(0.f, -2.5f, 1.5); }        // gentle float
float Spin() { return Loop(0.f, 360.f, 18.0); }      // snowflakes
float Wobble() { return Wave(-10.f, 10.f, 1.1); }    // Santa hat pom-pom

constexpr const char* HatCone = "M -10,0 C -8,-10 -2,-18 6,-20 C 12,-21 15,-17 15,-12 C 12,-15 8,-15 5,-12 C 3,-8 4,-4 10,0 Z";
constexpr const char* TreeShape = "M 0,-30 L 8,-19 L 5,-19 L 12,-9 L 8,-9 L 15,1 L -15,1 L -8,-9 L -12,-9 L -5,-19 L -8,-19 Z";
constexpr const char* CaneShape = "M 0,14 L 0,-6 C 0,-13 9,-13 9,-6 L 9,-3";
constexpr const char* GingerShape =
    "M 0,-13 C 4,-13 4,-7 1.8,-6.5 L 7,-5 C 9,-4.5 9,-1.5 7,-1.5 L 3.5,-2 L 3.5,3 L 6.5,10 C 7,12 4.5,13 3.5,11 L 0,5 "
    "L -3.5,11 C -4.5,13 -7,12 -6.5,10 L -3.5,3 L -3.5,-2 L -7,-1.5 C -9,-1.5 -9,-4.5 -7,-5 L -1.8,-6.5 C -4,-7 -4,-13 0,-13 Z";
constexpr const char* HollyLeaf = "M 0,0 C 2,-2 3,-1 4.5,-3 C 5,-1.5 6.5,-1.5 8,-3 C 8,-1 9.5,0 11,0 C 9.5,0 8,1 8,3 C 6.5,1.5 5,1.5 4.5,3 C 3,1 2,2 0,0 Z";
constexpr const char* CarrotShape = "M 0,-16 L 5,-15.2 L 0,-14.6 Z";
constexpr const char* GingerSmile = "M -1.5,-8.3 Q 0,-7.3 1.5,-8.3";
constexpr const char* GingerIcing = "M -6.3,-4 L -5,-2.4 M 6.3,-4 L 5,-2.4 M -5.5,9.5 L -3.8,9 M 5.5,9.5 L 3.8,9";

ID2D1PathGeometry* StarShape() {
    static ComPtr<ID2D1PathGeometry> geo = Geometry::Build([](ID2D1GeometrySink* sink) {
        constexpr int points = 5;
        for (int i = 0; i < points * 2; ++i) {
            const double r = i % 2 == 0 ? 5.5 : 2.3;
            const double a = -3.14159265358979 / 2 + i * 3.14159265358979 / points;
            const auto p = D2D1::Point2F(static_cast<float>(std::cos(a) * r), static_cast<float>(std::sin(a) * r));
            if (i == 0) sink->BeginFigure(p, D2D1_FIGURE_BEGIN_FILLED);
            else sink->AddLine(p);
        }
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    }, D2D1_FILL_MODE_ALTERNATE);
    return geo.Get();
}

ID2D1PathGeometry* FlakeShape() {
    static ComPtr<ID2D1PathGeometry> geo = Geometry::Build([](ID2D1GeometrySink* sink) {
        for (int i = 0; i < 6; ++i) {
            const double a = i * 3.14159265358979 / 3;
            const auto P = [a](double r, double da) {
                return D2D1::Point2F(static_cast<float>(std::cos(a + da) * r), static_cast<float>(std::sin(a + da) * r));
            };
            const auto segment = [sink](D2D1_POINT_2F from, D2D1_POINT_2F to) {
                sink->BeginFigure(from, D2D1_FIGURE_BEGIN_HOLLOW);
                sink->AddLine(to);
                sink->EndFigure(D2D1_FIGURE_END_OPEN);
            };
            segment(D2D1::Point2F(0, 0), P(8, 0));
            segment(P(4.5, 0), P(6.5, 0.35));
            segment(P(4.5, 0), P(6.5, -0.35));
        }
    }, D2D1_FILL_MODE_ALTERNATE);
    return geo.Get();
}

} // namespace

void DrawSantaHat(Graphics& g, float x, float y, float scale, float angle) {
    At at(g, x, y, scale, angle);
    FillStroke(g, Path(HatCone), Red, OutlinePen);
    {
        Push tip(g, Translate(15.f, -12.f));
        Push wobble(g, Rotate(Wobble()));
        Ellipse(g, 0.f, 0.f, 3.4f, 3.4f, Snow, OutlinePen);
    }
    const auto brim = D2D1::RoundedRect(D2D1::RectF(-12.f, -2.5f, 12.f, 3.f), 2.7f, 2.7f);
    g.Context()->FillRoundedRectangle(brim, g.Brush(Snow));
    g.Context()->DrawRoundedRectangle(brim, g.Brush(OutlinePen.color), OutlinePen.width);
}

void DrawTree(Graphics& g, float x, float y, float scale) {
    At at(g, x, y, scale);
    Rectangle(g, { -2.5f, 0.f, 5.f, 4.f }, TrunkBrush);
    FillStroke(g, Path(TreeShape), Green, TreeOutline);
    constexpr D2D1_POINT_2F bulbs[] = { { -3, -21 }, { 3, -17 }, { -6, -12 }, { 5, -9 }, { 0, -13 },
                                        { -9, -3 }, { -2, -5 }, { 8, -2 }, { 3, -1 } };
    for (int i = 0; i < static_cast<int>(std::size(bulbs)); ++i) Ellipse(g, bulbs[i].x, bulbs[i].y, 1.3f, 1.3f, TwinkleBrush(i));
    Ellipse(g, 0.f, -31.f, 5.f, 5.f, StarGlow());
    Push star(g, Translate(0.f, -31.f));
    Fill(g, StarShape(), Gold);
}

void DrawGift(Graphics& g, float x, float y, float scale, Color box) {
    At at(g, x, y, scale);
    Rectangle(g, { -8.f, -12.f, 16.f, 12.f }, box, OutlinePen);
    Rectangle(g, { -9.f, -15.f, 18.f, 4.f }, box, OutlinePen);
    Line(g, 0.f, -15.f, 0.f, 0.f, RibbonPen);
    Line(g, -9.f, -13.f, 9.f, -13.f, RibbonPen);
    StrokeEllipse(g, -2.8f, -17.f, 2.8f, 1.8f, RibbonPen);
    StrokeEllipse(g, 2.8f, -17.f, 2.8f, 1.8f, RibbonPen);
}

void DrawCandyCane(Graphics& g, float x, float y, float scale, float angle) {
    At at(g, x, y, scale, angle);
    Stroke(g, Path(CaneShape), CaneOutline);
    Stroke(g, Path(CaneShape), CaneWhite);
    PushRectClip(g, { -3.f, -14.f, 15.f, 29.f });
    for (int i = -2; i < 6; ++i) Line(g, -2.f, 12.f - i * 4.5f, 2.f, 9.5f - i * 4.5f, StripePen);
    Line(g, 3.f, -11.5f, 4.f, -8.5f, StripePen);
    Line(g, 7.5f, -10.5f, 6.5f, -7.5f, StripePen);
    g.PopLayer();
}

void DrawSnowman(Graphics& g, float x, float y, float scale) {
    At at(g, x, y, scale);
    // Light mode: slightly blue-tinted snow so the snowman shows up on pale cards
    const Color body = LightMode() ? SnowShaded : Snow;
    Ellipse(g, 0.f, -6.f, 7.5f, 6.5f, body, OutlinePen);
    Ellipse(g, 0.f, -16.5f, 5.2f, 5.f, body, OutlinePen);
    Rectangle(g, { -5.f, -12.5f, 10.f, 2.2f }, Red);
    Rectangle(g, { 2.f, -12.f, 2.2f, 5.f }, Red);
    Ellipse(g, -1.8f, -17.5f, 0.8f, 0.8f, Coal);
    Ellipse(g, 1.8f, -17.5f, 0.8f, 0.8f, Coal);
    Fill(g, Path(CarrotShape), Carrot);
    Ellipse(g, 0.f, -7.5f, 0.8f, 0.8f, Coal);
    Ellipse(g, 0.f, -4.5f, 0.8f, 0.8f, Coal);
    Rectangle(g, { -4.5f, -22.5f, 9.f, 1.4f }, Coal);
    Rectangle(g, { -3.f, -28.f, 6.f, 5.8f }, Coal);
}

void DrawOrnament(Graphics& g, float x, float y, float length, float scale, Color ball) {
    Push at(g, Translate(x, y));
    Push swing(g, Rotate(Swing()));
    Line(g, 0.f, 0.f, 0.f, length, ThreadPen());
    Push down(g, Translate(0.f, length));
    Push size(g, Scale(scale));
    Rectangle(g, { -1.8f, -0.5f, 3.6f, 2.5f }, Gold);
    Ellipse(g, 0.f, 7.f, 5.8f, 5.8f, ball, OutlinePen);
    Ellipse(g, -2.f, 5.f, 1.6f, 1.2f, Rgba(255, 255, 255, 150));
}

void DrawHolly(Graphics& g, float x, float y, float scale, float angle) {
    At at(g, x, y, scale, angle);
    {
        Push leaf(g, Rotate(-150.f));
        FillStroke(g, Path(HollyLeaf), Green, OutlinePen);
    }
    {
        Push leaf(g, Rotate(-30.f));
        FillStroke(g, Path(HollyLeaf), GreenDark, OutlinePen);
    }
    Ellipse(g, -1.5f, 0.5f, 2.f, 2.f, Berry, OutlinePen);
    Ellipse(g, 1.8f, 0.8f, 2.f, 2.f, Berry, OutlinePen);
    Ellipse(g, 0.f, -2.f, 2.f, 2.f, Berry, OutlinePen);
}

void DrawGingerbread(Graphics& g, float x, float y, float scale, float angle) {
    At at(g, x, y, scale, angle);
    Push bob(g, Translate(0.f, Bob()));
    FillStroke(g, Path(GingerShape), Ginger, OutlinePen);
    Ellipse(g, -1.3f, -10.3f, 0.6f, 0.6f, White);
    Ellipse(g, 1.3f, -10.3f, 0.6f, 0.6f, White);
    Stroke(g, Path(GingerSmile), IcingPen);
    Ellipse(g, 0.f, -3.5f, 0.9f, 0.9f, Red);
    Ellipse(g, 0.f, -0.5f, 0.9f, 0.9f, GreenDark);
    Stroke(g, Path(GingerIcing), IcingPen);
}

void DrawSnowflake(Graphics& g, float x, float y, float scale, bool spin) {
    At at(g, x, y, scale);
    if (spin) g.PushTransform(Rotate(Spin()));
    Stroke(g, FlakeShape(), FlakePen());
    if (spin) g.PopTransform();
}

void DrawLights(Graphics& g, float x0, float x1, float y, float scale) {
    const float span = x1 - x0;
    if (span < 30.f) return;
    const float spacing = 22.f * scale;
    const int segments = std::max(1, static_cast<int>(std::round(span / (spacing * 4.f))));
    const float segWidth = span / segments;
    const float sag = 7.f * scale;

    auto wire = Geometry::Build([&](ID2D1GeometrySink* sink) {
        sink->BeginFigure(D2D1::Point2F(x0, y), D2D1_FIGURE_BEGIN_HOLLOW);
        for (int s = 0; s < segments; ++s)
            sink->AddQuadraticBezier(D2D1::QuadraticBezierSegment(D2D1::Point2F(x0 + segWidth * (s + 0.5f), y + sag * 2.f),
                                                                  D2D1::Point2F(x0 + segWidth * (s + 1), y)));
        sink->EndFigure(D2D1_FIGURE_END_OPEN);
    });
    Stroke(g, wire.Get(), WirePen);

    const int count = static_cast<int>(span / spacing);
    for (int i = 1; i < count; ++i) {
        const float bx = x0 + i * span / count;
        // Quadratic sag within the bulb's segment
        const float t = std::fmod(bx - x0, segWidth) / segWidth;
        const float by = y + sag * 2.f * 2.f * t * (1.f - t);
        Push at(g, Translate(bx, by));
        Push size(g, Scale(scale));
        Rectangle(g, { -1.3f, -0.5f, 2.6f, 2.f }, Coal);
        Ellipse(g, 0.f, 4.f, 2.2f, 3.2f, TwinkleBrush(i));
    }
}

} // namespace SonicRacingSaveManager::Seasonal::Art::Christmas
