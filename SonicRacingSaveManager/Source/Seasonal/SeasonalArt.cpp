#include "Seasonal/SeasonalArt.h"
#include <climits>
#include <cmath>
#include <cstdlib>
#include <unordered_map>
#include "Graphics/Animation.h"

namespace SonicRacingSaveManager::Seasonal::Art {

// ── Clock ───────────────────────────────────────────────────────────────────

namespace {
double clockSeconds = 0.0;
bool lightMode = false;
}

void SetClock(double seconds) { clockSeconds = seconds; }
double Clock() { return clockSeconds; }
void SetLightMode(bool light) { lightMode = light; }
bool LightMode() { return lightMode; }

float Wave(float from, float to, double seconds, bool eased, double delay) {
    const double t = clockSeconds - delay;
    if (t <= 0.0) return from;
    float p = Anim::PingPong(t, seconds * 2.0);
    if (eased) p = Anim::EaseInOutSine(p);
    return from + (to - from) * p;
}

float Loop(float from, float to, double seconds) {
    const double p = std::fmod(clockSeconds, seconds) / seconds;
    return from + (to - from) * static_cast<float>(p);
}

float KeyframesAt(const std::vector<Key>& keys, double p) {
    if (keys.empty()) return 0.f;
    if (p <= keys.front().time) return keys.front().value;
    for (size_t i = 1; i < keys.size(); ++i) {
        if (p > keys[i].time) continue;
        const double span = keys[i].time - keys[i - 1].time;
        float local = span > 0.0 ? static_cast<float>((p - keys[i - 1].time) / span) : 1.f;
        if (keys[i].eased) local = Anim::EaseInOutSine(local);
        return keys[i - 1].value + (keys[i].value - keys[i - 1].value) * local;
    }
    return keys.back().value;
}

float Keyframes(const std::vector<Key>& keys) {
    if (keys.empty()) return 0.f;
    const double period = keys.back().time;
    return KeyframesAt(keys, period > 0.0 ? std::fmod(clockSeconds, period) : 0.0);
}

// ── Seeded subtractive generator (Knuth) ─────────────────────────────────────

SeededRandom::SeededRandom(int seed) {
    constexpr int MBIG = INT_MAX;
    constexpr int MSEED = 161803398;
    const int subtraction = seed == INT_MIN ? INT_MAX : std::abs(seed);
    int mj = MSEED - subtraction;
    seeds[55] = mj;
    int mk = 1;
    int ii = 0;
    for (int i = 1; i < 55; ++i) {
        if ((ii += 21) >= 55) ii -= 55;
        seeds[ii] = mk;
        mk = mj - mk;
        if (mk < 0) mk += MBIG;
        mj = seeds[ii];
    }
    for (int k = 1; k < 5; ++k) {
        for (int i = 1; i < 56; ++i) {
            int n = i + 30;
            if (n >= 55) n -= 55;
            seeds[i] -= seeds[1 + n];
            if (seeds[i] < 0) seeds[i] += MBIG;
        }
    }
}

int SeededRandom::Sample() {
    int locINext = inext;
    if (++locINext >= 56) locINext = 1;
    int locINextp = inextp;
    if (++locINextp >= 56) locINextp = 1;
    int retVal = seeds[locINext] - seeds[locINextp];
    if (retVal == INT_MAX) retVal--;
    if (retVal < 0) retVal += INT_MAX;
    seeds[locINext] = retVal;
    inext = locINext;
    inextp = locINextp;
    return retVal;
}

double SeededRandom::NextDouble() { return Sample() * (1.0 / INT_MAX); }
int SeededRandom::Next(int maxValue) { return static_cast<int>(NextDouble() * maxValue); }
int SeededRandom::Next(int minValue, int maxValue) {
    return static_cast<int>(NextDouble() * (static_cast<long long>(maxValue) - minValue)) + minValue;
}

// ── Drawing helpers ─────────────────────────────────────────────────────────

At::At(Graphics& graphics, float x, float y, float scale, float angle) : g(graphics) {
    g.PushTransform(Translate(x, y));
    g.PushTransform(Rotate(angle));
    g.PushTransform(Scale(scale));
}

At::~At() {
    g.PopTransform();
    g.PopTransform();
    g.PopTransform();
}

ID2D1PathGeometry* Path(const char* data) {
    static std::unordered_map<const char*, ComPtr<ID2D1PathGeometry>> cache;
    auto it = cache.find(data);
    if (it == cache.end()) it = cache.emplace(data, Geometry::FromPathData(data)).first;
    return it->second.Get();
}

void Fill(Graphics& g, ID2D1Geometry* geo, Color fill) { g.FillGeometry(geo, fill); }

void Stroke(Graphics& g, ID2D1Geometry* geo, const Pen& pen) { g.StrokeGeometry(geo, pen.color, pen.width, pen.round); }

void FillStroke(Graphics& g, ID2D1Geometry* geo, Color fill, const Pen& pen) {
    g.FillGeometry(geo, fill);
    g.StrokeGeometry(geo, pen.color, pen.width, pen.round);
}

void Ellipse(Graphics& g, float cx, float cy, float rx, float ry, Color fill) { g.FillEllipse(cx, cy, rx, ry, fill); }

void Ellipse(Graphics& g, float cx, float cy, float rx, float ry, Color fill, const Pen& pen) {
    g.FillEllipse(cx, cy, rx, ry, fill);
    g.StrokeEllipse(cx, cy, rx, ry, pen.color, pen.width);
}

void StrokeEllipse(Graphics& g, float cx, float cy, float rx, float ry, const Pen& pen) {
    g.StrokeEllipse(cx, cy, rx, ry, pen.color, pen.width);
}

void Rectangle(Graphics& g, const Rect& r, Color fill) { g.FillRect(r, fill); }

void Rectangle(Graphics& g, const Rect& r, Color fill, const Pen& pen) {
    g.FillRect(r, fill);
    StrokeRectangle(g, r, pen);
}

void StrokeRectangle(Graphics& g, const Rect& r, const Pen& pen) {
    // WPF centres the pen on the outline (Graphics::StrokeRect insets it)
    if (pen.color.Transparent()) return;
    g.Context()->DrawRectangle(r.D2D(), g.Brush(pen.color), pen.width);
}

void Line(Graphics& g, float x1, float y1, float x2, float y2, const Pen& pen) {
    g.Line(x1, y1, x2, y2, pen.color, pen.width, pen.round);
}

void GlowEllipse(Graphics& g, float cx, float cy, float rx, float ry, Color color) {
    if (rx <= 0.f || ry <= 0.f) return;
    g.FillRadial({ cx - rx, cy - ry, rx * 2.f, ry * 2.f }, { { 0.f, color }, { 1.f, color.WithAlpha(0.f) } });
}

void GlowRect(Graphics& g, const Rect& r, Color color) {
    if (r.Empty()) return;
    auto* brush = g.RadialBrush({ { 0.f, color }, { 1.f, color.WithAlpha(0.f) } }, { r.CX(), r.CY() }, r.w * 0.5f, r.h * 0.5f);
    if (brush) g.Context()->FillRectangle(r.D2D(), brush);
}

void PushRectClip(Graphics& g, const Rect& r) {
    auto geometry = Geometry::Rectangle(r.D2D());
    g.PushLayer(1.f, geometry.Get());
}

} // namespace SonicRacingSaveManager::Seasonal::Art
