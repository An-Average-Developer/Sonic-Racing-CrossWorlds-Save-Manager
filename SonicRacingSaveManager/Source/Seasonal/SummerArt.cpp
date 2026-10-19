#include <algorithm>
#include <cmath>
#include <cstdlib>
#include "Seasonal/SeasonalArt.h"

// Vector summer decorations (sun, palm trees, beach balls, crabs, starfish, shells, ice creams,
// umbrellas, seagulls, clouds).
namespace SonicRacingSaveManager::Seasonal::Art::Summer {

namespace {

constexpr Color SunCore = Rgba(255, 213, 79);
constexpr Color SunRay = Rgba(255, 183, 39);
constexpr Color PalmLeaf = Rgba(46, 160, 67);
constexpr Color PalmLeafDk = Rgba(29, 115, 48);
constexpr Color Trunk = Rgba(160, 110, 60);
constexpr Color Coconut = Rgba(110, 70, 35);
constexpr Color BallRed = Rgba(239, 83, 80);
constexpr Color BallBlue = Rgba(41, 182, 246);
constexpr Color BallYellow = Rgba(255, 213, 79);
constexpr Color White = Rgba(255, 255, 255);
constexpr Color CrabBrush = Rgba(244, 81, 30);
constexpr Color StarBrush = Rgba(255, 138, 101);
constexpr Color ShellBrush = Rgba(255, 204, 188);
constexpr Color Cone = Rgba(214, 160, 90);
constexpr Color ScoopPink = Rgba(248, 140, 180);
constexpr Color ScoopMint = Rgba(150, 230, 190);
constexpr Color UmbrellaA = Rgba(255, 112, 67);
constexpr Color UmbrellaB = Rgba(255, 245, 225);
constexpr Color SandBrush = Rgba(240, 210, 150);
constexpr Color CloudBrush = Rgba(255, 255, 255, 230);
constexpr Color Dark = Rgba(40, 30, 30);

const Pen OutlinePen{ Rgba(90, 60, 30, 170), 0.8f };
const Pen StemPen{ Rgba(120, 90, 60), 1.2f };
const Pen GullPenDark{ Rgba(255, 255, 255, 230), 1.6f };
const Pen GullPenLite{ Rgba(55, 71, 79, 220), 1.6f };
const Pen& GullPen() { return LightMode() ? GullPenLite : GullPenDark; }
constexpr Color WaveBrush = Rgba(38, 198, 218, 150);
constexpr Color WaveFoam = Rgba(255, 255, 255, 170);
const Pen ShellLine{ Rgba(200, 120, 100, 200), 0.6f };
const Pen CrabLeg{ CrabBrush, 1.1f, false };

// ── Shared animations ──
float Spin() { return Loop(0.f, 360.f, 24.0); }                // sun rays, beach balls
float Bob() { return Wave(0.f, -3.f, 1.6); }                    // balls, ice creams, clouds
float Sway() { return Wave(-4.f, 4.f, 2.4); }                   // palm leaves
float Flap() { return Wave(1.f, 0.4f, 0.32); }                  // seagull wing beat
float Scuttle() {                                               // crabs scuttling sideways
    return Keyframes({ { 0.0, 0.f, false }, { 0.6, 6.f, true }, { 1.8, 6.f, false },
                       { 2.6, -4.f, true }, { 3.6, -4.f, false }, { 4.2, 0.f, true } });
}

constexpr const char* PalmTrunk = "M -2,0 C -1,-10 0,-20 3,-30 L 5.5,-29.5 C 3,-20 2,-10 2.5,0 Z";
constexpr const char* PalmFrond = "M 0,0 C 5,-5 12,-5 17,0 C 12,-2 6,-1 0,0 Z";
constexpr const char* PalmIsland = "M -14,1 C -8,-4 10,-4 16,1 Z";
constexpr const char* CrabBody = "M -7,0 C -7,-6 7,-6 7,0 C 5,2 -5,2 -7,0 Z";
constexpr const char* CrabClaws = "M -8,-4 C -12,-7 -12,-11 -9,-11 L -9.5,-8.5 L -7.5,-9.5 C -7,-7 -7,-5.5 -8,-4 Z M 8,-4 C 12,-7 12,-11 9,-11 L 9.5,-8.5 L 7.5,-9.5 C 7,-7 7,-5.5 8,-4 Z";
constexpr const char* ShellShape = "M 0,4 C -9,2 -9,-7 0,-8 C 9,-7 9,2 0,4 Z M -2.5,4.5 L 2.5,4.5 L 1.5,6.5 L -1.5,6.5 Z";
constexpr const char* ConeShape = "M -4.5,0 L 4.5,0 L 0,12 Z";
constexpr const char* UmbrellaTop = "M -15,0 C -14,-12 14,-12 15,0 C 12,-2 9,-2 7.5,0 C 5,-2 2.5,-2 0,0 C -2.5,-2 -5,-2 -7.5,0 C -9,-2 -12,-2 -15,0 Z";
constexpr const char* UmbrellaStripes = "M -7.5,0 C -6,-6 -3,-9.5 0,-10 C -2,-7 -3,-4 0,0 C -2.5,-2 -5,-2 -7.5,0 Z M 7.5,0 C 6,-6 3,-9.5 0,-10 C 2,-7 3,-4 0,0 C 2.5,-2 5,-2 7.5,0 Z";
constexpr const char* GullShape = "M -9,-1 Q -4.5,-6 0,0 Q 4.5,-6 9,-1";
constexpr const char* BallSegA = "M 0,0 L 0,-9 A 9,9 0 0 1 7.8,-4.5 Z";
constexpr const char* BallSegB = "M 0,0 L 7.8,4.5 A 9,9 0 0 1 0,9 Z";
constexpr const char* BallSegC = "M 0,0 L -7.8,4.5 A 9,9 0 0 1 -7.8,-4.5 Z";
constexpr const char* CloudShape = "M -14,4 C -18,4 -18,-3 -12,-3 C -12,-9 -3,-10 -1,-5 C 1,-11 11,-10 10,-3 C 16,-4 17,4 12,4 Z";

ID2D1PathGeometry* SunRays() {
    static ComPtr<ID2D1PathGeometry> geo = Geometry::Build([](ID2D1GeometrySink* sink) {
        for (int i = 0; i < 12; ++i) {
            const double a = i * 3.14159265358979 / 6, b = a + 0.14, c = a - 0.14;
            sink->BeginFigure(D2D1::Point2F(static_cast<float>(std::cos(c) * 11), static_cast<float>(std::sin(c) * 11)),
                              D2D1_FIGURE_BEGIN_FILLED);
            sink->AddLine(D2D1::Point2F(static_cast<float>(std::cos(a) * 17), static_cast<float>(std::sin(a) * 17)));
            sink->AddLine(D2D1::Point2F(static_cast<float>(std::cos(b) * 11), static_cast<float>(std::sin(b) * 11)));
            sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        }
    }, D2D1_FILL_MODE_ALTERNATE);
    return geo.Get();
}

ID2D1PathGeometry* Star() {
    static ComPtr<ID2D1PathGeometry> geo = Geometry::Build([](ID2D1GeometrySink* sink) {
        for (int i = 0; i < 10; ++i) {
            const double r = i % 2 == 0 ? 9 : 3.8;
            const double a = -3.14159265358979 / 2 + i * 3.14159265358979 / 5;
            const auto p = D2D1::Point2F(static_cast<float>(std::cos(a) * r), static_cast<float>(std::sin(a) * r));
            if (i == 0) sink->BeginFigure(p, D2D1_FIGURE_BEGIN_FILLED);
            else sink->AddLine(p);
        }
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    }, D2D1_FILL_MODE_ALTERNATE);
    return geo.Get();
}

} // namespace

void DrawSun(Graphics& g, float x, float y, float scale) {
    At at(g, x, y, scale);
    GlowEllipse(g, 0.f, 0.f, 24.f, 24.f, Rgba(255, 214, 90, 110));
    {
        Push spin(g, Rotate(Spin()));
        Fill(g, SunRays(), SunRay);
    }
    Ellipse(g, 0.f, 0.f, 9.5f, 9.5f, SunCore);
}

void DrawPalm(Graphics& g, float x, float y, float scale) {
    At at(g, x, y, scale);
    Fill(g, Path(PalmIsland), SandBrush);
    FillStroke(g, Path(PalmTrunk), Trunk, OutlinePen);
    Push top(g, Translate(4.2f, -30.f));
    Push sway(g, Rotate(Sway()));
    const struct { float angle; Color color; } fronds[] = {
        { -165.f, PalmLeafDk }, { -120.f, PalmLeaf }, { -60.f, PalmLeaf }, { -15.f, PalmLeafDk }, { 20.f, PalmLeaf }, { 160.f, PalmLeaf },
    };
    for (const auto& f : fronds) {
        Push rotate(g, Rotate(f.angle));
        Fill(g, Path(PalmFrond), f.color);
    }
    Ellipse(g, -1.5f, 1.5f, 1.8f, 1.8f, Coconut);
    Ellipse(g, 1.5f, 2.f, 1.8f, 1.8f, Coconut);
}

void DrawBeachBall(Graphics& g, float x, float y, float scale) {
    At at(g, x, y, scale);
    Push bob(g, Translate(0.f, Bob()));
    Push spin(g, Rotate(Spin()));
    Ellipse(g, 0.f, 0.f, 9.f, 9.f, White, OutlinePen);
    Fill(g, Path(BallSegA), BallRed);
    Fill(g, Path(BallSegB), BallBlue);
    Fill(g, Path(BallSegC), BallYellow);
    Ellipse(g, 0.f, 0.f, 2.f, 2.f, White);
}

void DrawCrab(Graphics& g, float x, float y, float scale) {
    At at(g, x, y, scale);
    Push scuttle(g, Translate(Scuttle(), 0.f));
    for (int i = 0; i < 3; ++i) {
        Line(g, -5.f + i, 0.f, -9.f + i * 0.5f, 3.5f, CrabLeg);
        Line(g, 5.f - i, 0.f, 9.f - i * 0.5f, 3.5f, CrabLeg);
    }
    Fill(g, Path(CrabClaws), CrabBrush);
    Fill(g, Path(CrabBody), CrabBrush);
    Line(g, -2.f, -4.f, -2.5f, -7.f, CrabLeg);
    Line(g, 2.f, -4.f, 2.5f, -7.f, CrabLeg);
    Ellipse(g, -2.5f, -7.5f, 1.3f, 1.3f, White);
    Ellipse(g, 2.5f, -7.5f, 1.3f, 1.3f, White);
    Ellipse(g, -2.5f, -7.5f, 0.6f, 0.6f, Dark);
    Ellipse(g, 2.5f, -7.5f, 0.6f, 0.6f, Dark);
}

void DrawStarfish(Graphics& g, float x, float y, float scale, float angle) {
    At at(g, x, y, scale, angle);
    FillStroke(g, Star(), StarBrush, OutlinePen);
    Ellipse(g, 0.f, -4.5f, 0.7f, 0.7f, ShellBrush);
    Ellipse(g, 0.f, 0.f, 0.9f, 0.9f, ShellBrush);
}

void DrawShell(Graphics& g, float x, float y, float scale, float angle) {
    At at(g, x, y, scale, angle);
    FillStroke(g, Path(ShellShape), ShellBrush, OutlinePen);
    for (int i = -2; i <= 2; ++i) Line(g, 0.f, 4.f, i * 3.f, -6.5f + std::abs(i) * 0.8f, ShellLine);
}

void DrawIceCream(Graphics& g, float x, float y, float scale) {
    At at(g, x, y, scale);
    Push bob(g, Translate(0.f, Bob()));
    FillStroke(g, Path(ConeShape), Cone, OutlinePen);
    Line(g, -2.5f, 2.f, 1.5f, 8.f, StemPen);
    Line(g, 2.5f, 2.f, -1.5f, 8.f, StemPen);
    Ellipse(g, 0.f, -3.f, 5.5f, 5.f, ScoopMint);
    Ellipse(g, 0.f, -8.5f, 5.f, 4.6f, ScoopPink);
    Ellipse(g, 0.5f, -13.5f, 1.6f, 1.6f, BallRed);
}

void DrawUmbrella(Graphics& g, float x, float y, float scale, float angle) {
    At at(g, x, y, scale, angle);
    Line(g, 0.f, 0.f, 0.f, -22.f, StemPen);
    {
        Push top(g, Translate(0.f, -20.f));
        FillStroke(g, Path(UmbrellaTop), UmbrellaB, OutlinePen);
        Fill(g, Path(UmbrellaStripes), UmbrellaA);
    }
    Ellipse(g, 0.f, 0.5f, 8.f, 1.8f, SandBrush);
}

void DrawSeagull(Graphics& g, float x, float y, float scale) {
    At at(g, x, y, scale);
    Push flap(g, Scale(1.f, Flap()));
    Stroke(g, Path(GullShape), GullPen());
}

void DrawCloud(Graphics& g, float x, float y, float scale) {
    At at(g, x, y, scale);
    Push bob(g, Translate(0.f, Bob()));
    Fill(g, Path(CloudShape), CloudBrush);
}

void DrawWaves(Graphics& g, float x0, float x1, float y, float scale) {
    constexpr float period = 16.f;
    // Extra period on each side so the rolling animation never shows an edge
    const float start = x0 - period * scale, end = x1 + period * scale;
    const auto crests = [&](ID2D1GeometrySink* sink) {
        for (float x = start; x < end; x += period * scale)
            sink->AddQuadraticBezier(D2D1::QuadraticBezierSegment(D2D1::Point2F(x + period * scale / 2.f, y - 9.f * scale),
                                                                  D2D1::Point2F(x + period * scale, y - 4.f * scale)));
    };
    const auto water = Geometry::Build([&](ID2D1GeometrySink* sink) {
        sink->BeginFigure(D2D1::Point2F(start, y), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(start, y - 4.f * scale));
        crests(sink);
        sink->AddLine(D2D1::Point2F(end, y));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    });
    const auto foam = Geometry::Build([&](ID2D1GeometrySink* sink) {
        sink->BeginFigure(D2D1::Point2F(start, y - 4.f * scale), D2D1_FIGURE_BEGIN_HOLLOW);
        crests(sink);
        sink->EndFigure(D2D1_FIGURE_END_OPEN);
    });
    PushRectClip(g, { x0, y - 12.f * scale, std::max(0.f, x1 - x0), 12.f * scale });
    {
        Push roll(g, Translate(Wave(-5.f, 5.f, 2.2), 0.f));
        Fill(g, water.Get(), WaveBrush);
        Stroke(g, foam.Get(), Pen{ WaveFoam, 1.2f * scale });
    }
    g.PopLayer();
}

} // namespace SonicRacingSaveManager::Seasonal::Art::Summer
