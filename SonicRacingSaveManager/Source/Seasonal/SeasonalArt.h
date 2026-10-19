#pragma once
#include <d2d1_3.h>
#include <string_view>
#include <vector>
#include "Graphics/Geometry.h"
#include "Graphics/Graphics.h"

// Shared pieces of the seasonal art (scenery, chapter card decorations, ambient particles).
// Every piece is drawn in small "unit" coordinates around the origin and positioned with
// transforms, like the old WPF DrawingContext code. Animations are pure functions of the clock
// (no per-object state), so the same piece drawn twice in a frame moves in sync.
namespace SonicRacingSaveManager::Seasonal::Art {

// ── Clock ───────────────────────────────────────────────────────────────────
void SetClock(double seconds);   // called once per frame by the scenery / ambient layers
double Clock();
// LightMode (SeasonalDecorations.SyncArt): drawing on light backgrounds, webs / threads / gulls /
// snowflakes switch to dark pens and the snowman gets shaded snow.
void SetLightMode(bool light);
bool LightMode();

// DoubleAnimation(from, to, seconds) { AutoReverse, Forever }, optionally with SineEase InOut.
float Wave(float from, float to, double seconds, bool eased = true, double delay = 0.0);
// DoubleAnimation(from, to, seconds) { Forever } without auto-reverse (linear).
float Loop(float from, float to, double seconds);

struct Key {
    double time;   // seconds (or 0..1 for KeyframesAt)
    float value;
    bool eased;    // SineEase InOut on the segment that ends at this key
};
// Repeating key-frame animation; the last key's time is the period.
float Keyframes(const std::vector<Key>& keys);
// Key frames evaluated at progress p (0..1), keys use percent times.
float KeyframesAt(const std::vector<Key>& keys, double p);

// ── Seeded subtractive generator (Knuth), so the scenery layouts stay the same between versions ──
class SeededRandom {
public:
    explicit SeededRandom(int seed);
    double NextDouble();
    int Next(int maxValue);
    int Next(int minValue, int maxValue);

private:
    int Sample();
    int seeds[56]{};
    int inext = 0, inextp = 21;
};

// ── Drawing helpers ─────────────────────────────────────────────────────────
struct Pen {
    Color color;
    float width = 1.f;
    bool round = true;   // round caps / joins (FrozenPen)
};

using M = D2D1::Matrix3x2F;
inline M Translate(float x, float y) { return M::Translation(x, y); }
inline M Rotate(float degrees) { return M::Rotation(degrees); }
inline M Scale(float sx, float sy) { return M::Scale(sx, sy); }
inline M Scale(float s) { return M::Scale(s, s); }
inline M SkewX(float degrees) { return M::Skew(degrees, 0.f); }

// Pushes a transform for the lifetime of the object (inner transforms are pushed later).
class Push {
public:
    Push(Graphics& graphics, const M& m) : g(graphics) { g.PushTransform(m); }
    ~Push() { g.PopTransform(); }
    Push(const Push&) = delete;
    Push& operator=(const Push&) = delete;

private:
    Graphics& g;
};

// Translate(x, y) · Rotate(angle) · Scale(scale) — the art's "At" helper.
class At {
public:
    At(Graphics& graphics, float x, float y, float scale, float angle = 0.f);
    ~At();
    At(const At&) = delete;
    At& operator=(const At&) = delete;

private:
    Graphics& g;
};

// Path markup parsed once and kept for the lifetime of the app (keyed by the literal's address).
ID2D1PathGeometry* Path(const char* data);

void Fill(Graphics& g, ID2D1Geometry* geo, Color fill);
void Stroke(Graphics& g, ID2D1Geometry* geo, const Pen& pen);
void FillStroke(Graphics& g, ID2D1Geometry* geo, Color fill, const Pen& pen);
void Ellipse(Graphics& g, float cx, float cy, float rx, float ry, Color fill);
void Ellipse(Graphics& g, float cx, float cy, float rx, float ry, Color fill, const Pen& pen);
void StrokeEllipse(Graphics& g, float cx, float cy, float rx, float ry, const Pen& pen);
void Rectangle(Graphics& g, const Rect& r, Color fill);
void Rectangle(Graphics& g, const Rect& r, Color fill, const Pen& pen);
void StrokeRectangle(Graphics& g, const Rect& r, const Pen& pen);
void Line(Graphics& g, float x1, float y1, float x2, float y2, const Pen& pen);
// RadialGradientBrush(color, transparent) filling an ellipse (WPF "Glow" brush).
void GlowEllipse(Graphics& g, float cx, float cy, float rx, float ry, Color color);
// Same brush filling a rectangle (ellipse inscribed in the rectangle, transparent corners).
void GlowRect(Graphics& g, const Rect& r, Color color);
// Clip to a rectangle in the current (possibly rotated) coordinate space.
void PushRectClip(Graphics& g, const Rect& r);

// ── Halloween pieces ────────────────────────────────────────────────────────
namespace Halloween {
void DrawPumpkin(Graphics& g, float x, float y, float scale);
void DrawWitchHat(Graphics& g, float x, float y, float scale, float angle);
void DrawBat(Graphics& g, float x, float y, float scale, bool bob = true);
void DrawSpider(Graphics& g, float x, float y, float length, float scale);
void DrawGhost(Graphics& g, float x, float y, float scale);
void DrawTree(Graphics& g, float x, float y, float scale);
void DrawCobweb(Graphics& g, float x, float y, float scale, bool flipX, bool flipY);
void DrawMoon(Graphics& g, float x, float y, float radius);
} // namespace Halloween

// ── Summer pieces ───────────────────────────────────────────────────────────
namespace Summer {
void DrawSun(Graphics& g, float x, float y, float scale);
void DrawPalm(Graphics& g, float x, float y, float scale);
void DrawBeachBall(Graphics& g, float x, float y, float scale);
void DrawCrab(Graphics& g, float x, float y, float scale);
void DrawStarfish(Graphics& g, float x, float y, float scale, float angle);
void DrawShell(Graphics& g, float x, float y, float scale, float angle);
void DrawIceCream(Graphics& g, float x, float y, float scale);
void DrawUmbrella(Graphics& g, float x, float y, float scale, float angle);
void DrawSeagull(Graphics& g, float x, float y, float scale);
void DrawCloud(Graphics& g, float x, float y, float scale);
// Rolling waves along the bottom edge of a card, from x0 to x1 at baseline y.
void DrawWaves(Graphics& g, float x0, float x1, float y, float scale);
} // namespace Summer

// ── Christmas pieces ────────────────────────────────────────────────────────
namespace Christmas {
inline constexpr Color OrnRed = Rgba(211, 47, 47);
inline constexpr Color OrnGold = Rgba(255, 202, 40);
inline constexpr Color OrnBlue = Rgba(66, 165, 245);
inline constexpr Color OrnPurple = Rgba(171, 71, 188);

void DrawSantaHat(Graphics& g, float x, float y, float scale, float angle);
void DrawTree(Graphics& g, float x, float y, float scale);
void DrawGift(Graphics& g, float x, float y, float scale, Color box);
void DrawCandyCane(Graphics& g, float x, float y, float scale, float angle);
void DrawSnowman(Graphics& g, float x, float y, float scale);
void DrawOrnament(Graphics& g, float x, float y, float length, float scale, Color ball);
void DrawHolly(Graphics& g, float x, float y, float scale, float angle);
void DrawGingerbread(Graphics& g, float x, float y, float scale, float angle);
void DrawSnowflake(Graphics& g, float x, float y, float scale, bool spin = true);
void DrawLights(Graphics& g, float x0, float x1, float y, float scale);
} // namespace Christmas

} // namespace SonicRacingSaveManager::Seasonal::Art
