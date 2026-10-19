#include "Seasonal/SeasonalOverlay.h"
#include <algorithm>
#include <cmath>
#include <random>
#include <vector>
#include "Seasonal/SeasonalArt.h"
#include "Seasonal/SeasonalTheme.h"

// Click-through ambient animations over the whole main screen (Halloween: bats and ghosts crossing
// the screen plus rising embers, Summer: seagulls plus floating sun motes, Christmas: snowfall).
namespace SonicRacingSaveManager::Seasonal::Ambient {

namespace {

using namespace Art;

constexpr int MaxSnowflakes = 110;
constexpr int MaxParticles = 30;
constexpr double SnowInterval = 0.19;
constexpr double ParticleInterval = 0.42;

enum class Kind { Bat, Seagull, Ghost, Ember, Snow };

// One animated element. Everything is a function of (now - start) / duration.
struct Sprite {
    Kind kind;
    double start = 0.0, duration = 1.0;
    float scale = 1.f;          // critters: art scale, particles: radius
    bool leftToRight = true;
    float fromX = 0.f, toX = 0.f;
    float x = 0.f, y = 0.f;     // base position (wave / drift around it)
    float amplitude = 0.f;
    float phase = 1.f;
    float opacity = 1.f;
    float endY = 0.f;           // particles rising / snow falling
    Color color;
};

struct State {
    Season season = Season::Classic;
    bool started = false;
    double nextFlock = 0.0, nextParticle = 0.0, nextSnow = 0.0;
    std::vector<Sprite> sprites;
    std::mt19937 rng{ std::random_device{}() };

    double NextDouble() { return std::uniform_real_distribution<double>(0.0, 1.0)(rng); }
    int Next(int minValue, int maxValue) { return std::uniform_int_distribution<int>(minValue, maxValue - 1)(rng); }
    int Next(int maxValue) { return Next(0, maxValue); }
} state;

int Count(Kind a, Kind b) {
    return static_cast<int>(std::count_if(state.sprites.begin(), state.sprites.end(), [&](const Sprite& s) { return s.kind == a || s.kind == b; }));
}

void Reset(Season season, double now) {
    state.season = season;
    state.sprites.clear();
    state.nextFlock = now + 2.0;
    state.nextParticle = now + ParticleInterval;
    state.nextSnow = now + SnowInterval;
}

void SpawnBat(double now, float w, bool leftToRight, float y, double delay) {
    Sprite s;
    s.kind = state.season == Season::Summer ? Kind::Seagull : Kind::Bat;
    s.scale = static_cast<float>(1.4 + state.NextDouble() * 0.8);
    s.leftToRight = leftToRight;
    s.opacity = 0.9f;
    s.duration = 5 + state.NextDouble() * 3;
    s.start = now + delay;
    s.fromX = leftToRight ? -60.f : w + 60.f;
    s.toX = leftToRight ? w + 60.f : -60.f;
    s.y = y;
    s.amplitude = static_cast<float>(18 + state.NextDouble() * 22);
    state.sprites.push_back(s);
}

// A faint ghost drifting slowly across the screen, fading in and out
void SpawnGhost(double now, float w, float h, bool leftToRight) {
    Sprite s;
    s.kind = Kind::Ghost;
    s.scale = static_cast<float>(2.2 + state.NextDouble() * 1.2);
    s.leftToRight = leftToRight;
    s.duration = 16 + state.NextDouble() * 8;
    s.start = now;
    s.fromX = leftToRight ? -60.f : w + 60.f;
    s.toX = leftToRight ? w + 60.f : -60.f;
    s.y = static_cast<float>(h * (0.12 + state.NextDouble() * 0.5));
    state.sprites.push_back(s);
}

void SpawnFlock(double now, float w, float h) {
    if (w < 100.f || h < 100.f) {
        state.nextFlock = now + 3.0;
        return;
    }
    const bool leftToRight = state.Next(2) == 0;
    if (state.season == Season::Halloween && state.Next(3) == 0) {
        SpawnGhost(now, w, h, leftToRight);
        state.nextFlock = now + state.Next(8, 14);
        return;
    }
    const int count = state.Next(1, 4);
    const float baseY = static_cast<float>(h * (0.15 + state.NextDouble() * 0.6));
    for (int i = 0; i < count; ++i) SpawnBat(now, w, leftToRight, baseY + i * 26.f - 20.f, i * 0.35);
    state.nextFlock = now + state.Next(8, 16);
}

// Glowing ember (Halloween) or sun mote (Summer) rising from the bottom and fading out
void SpawnParticle(double now, float w, float h) {
    if (w < 100.f || h < 100.f || Count(Kind::Ember, Kind::Ember) >= MaxParticles) return;
    const bool halloween = state.season == Season::Halloween;
    Sprite s;
    s.kind = Kind::Ember;
    s.color = halloween ? (state.Next(4) == 0 ? Rgba(190, 120, 255) : Rgba(255, 140, 40))
                        : (state.Next(3) == 0 ? Rgba(255, 255, 255) : Rgba(255, 214, 110));
    s.scale = static_cast<float>(1.6 + state.NextDouble() * 2.4);
    s.x = static_cast<float>(state.NextDouble() * w);
    s.y = static_cast<float>(h * (0.82 + state.NextDouble() * 0.15));
    s.endY = s.y - static_cast<float>(h * (0.25 + state.NextDouble() * 0.35));
    s.duration = 6 + state.NextDouble() * 6;
    s.amplitude = static_cast<float>(12 + state.NextDouble() * 30);
    s.start = now;
    state.sprites.push_back(s);
}

void SpawnSnowflake(double now, float w, float h) {
    if (w < 100.f || h < 100.f || static_cast<int>(state.sprites.size()) >= MaxSnowflakes) return;
    Sprite s;
    s.kind = Kind::Snow;
    s.scale = static_cast<float>(1.6 + state.NextDouble() * state.NextDouble() * 5);   // mostly small flakes, a few big ones
    s.opacity = static_cast<float>(0.45 + state.NextDouble() * 0.45);
    // A bit wider than the window: gusts push flakes in from the sides
    s.x = static_cast<float>(-0.1 * w + state.NextDouble() * w * 1.2);
    s.duration = 9 + state.NextDouble() * 7 - s.scale;   // bigger flakes fall a bit faster
    s.y = -10.f;
    s.endY = h + 10.f;
    s.amplitude = static_cast<float>(10 + state.NextDouble() * 25);
    s.phase = state.Next(2) == 0 ? 1.f : -1.f;
    s.start = now;
    state.sprites.push_back(s);
}

// Wind: sideways position of the air (px), whose rate of change is the wind speed. A flake has
// moved Wind(now) - Wind(spawn), so every flake in the air drifts with the same gusts.
float Wind(double t) {
    return static_cast<float>(140.0 * std::sin(t * 0.21) + 60.0 * std::sin(t * 0.53 + 1.7) + 25.0 * std::sin(t * 1.3 + 0.4));
}

// Alternating 0, -a, 0, +a, 0... key frames (SineEase on each segment), like the old wave animations
float WaveOffset(double p, int segments, float first, float second) {
    std::vector<Key> keys;
    for (int k = 0; k <= segments; ++k)
        keys.push_back({ static_cast<double>(k) / segments, k % 2 == 0 ? 0.f : (k % 4 == 1 ? first : second), true });
    return KeyframesAt(keys, p);
}

void DrawSprite(Graphics& g, const Sprite& s, double now) {
    const double p = std::clamp((now - s.start) / s.duration, 0.0, 1.0);
    const bool begun = now >= s.start;
    switch (s.kind) {
        case Kind::Bat:
        case Kind::Seagull:
        case Kind::Ghost: {
            const float x = begun ? s.fromX + (s.toX - s.fromX) * static_cast<float>(p) : s.fromX;
            float y = s.y, opacity = s.opacity;
            if (s.kind == Kind::Ghost) {
                y += WaveOffset(p, 6, -30.f, 30.f);
                opacity = KeyframesAt({ { 0.0, 0.f, false }, { 0.2, 0.5f, false }, { 0.5, 0.15f, false }, { 0.75, 0.5f, false }, { 1.0, 0.f, false } }, p);
            } else if (begun) {
                y += WaveOffset(p, 8, -s.amplitude, s.amplitude);
            }
            if (opacity <= 0.f) return;
            g.PushOpacity(opacity);
            {
                Push at(g, Translate(x, y));
                Push face(g, Scale(s.leftToRight ? 1.f : -1.f, 1.f));
                if (s.kind == Kind::Seagull) Art::Summer::DrawSeagull(g, 0, 0, s.scale);
                else if (s.kind == Kind::Ghost) Art::Halloween::DrawGhost(g, 0, 0, s.scale);
                else Art::Halloween::DrawBat(g, 0, 0, s.scale, false);
            }
            g.PopLayer();
            break;
        }
        case Kind::Ember: {
            const float y = s.y + (s.endY - s.y) * static_cast<float>(p);
            const float x = s.x + WaveOffset(p, 4, s.amplitude, -s.amplitude);
            const float opacity = KeyframesAt({ { 0.0, 0.f, false }, { 0.25, 0.9f, false }, { 0.6, 0.5f, false }, { 1.0, 0.f, false } }, p);
            if (opacity <= 0.f) return;
            const float r = s.scale;
            // Soft halo + bright core, each faded (cheaper than a layer per particle; the core covers
            // the only place where they overlap)
            const float halo = r * 3.5f;
            auto* brush = g.RadialBrush({ { 0.f, s.color.WithAlpha(150 / 255.f) }, { 1.f, s.color.WithAlpha(0.f) } }, { x, y }, halo, halo, opacity);
            if (brush) g.Context()->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x, y), halo, halo), brush);
            const Color core{ std::min(1.f, s.color.r + 40 / 255.f), std::min(1.f, s.color.g + 40 / 255.f), std::min(1.f, s.color.b + 40 / 255.f), opacity };
            Ellipse(g, x, y, r * 0.6f, r * 0.6f, core);
            break;
        }
        case Kind::Snow: {
            const float y = s.y + (s.endY - s.y) * static_cast<float>(p);
            // Bigger (closer) flakes are pushed a little more by the wind
            const float gust = (Wind(std::min(now, s.start + s.duration)) - Wind(s.start)) * (0.6f + s.scale * 0.12f);
            const float x = s.x + WaveOffset(p, 6, s.amplitude * s.phase, -s.amplitude * s.phase) + gust;
            const float r = s.scale;
            // White flakes on the dark palette, light blue ones on the light palette
            const Color flake = DarkMode() ? Colors::White : Rgba(140, 185, 225);
            auto* brush = g.RadialBrush({ { 0.55f, flake }, { 1.f, flake.WithAlpha(0.f) } }, { x, y }, r, r, s.opacity);
            if (brush) g.Context()->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x, y), r, r), brush);
            break;
        }
    }
}

} // namespace

void Draw(Graphics& g, const Rect& screen, double time) {
    const Season season = Active();
    if (!state.started || season != state.season) {
        state.started = true;
        Reset(season, time);
    }
    if (season == Season::Classic) return;
    SetClock(time);
    const float w = screen.w, h = screen.h;

    // Timers
    if (season == Season::Christmas) {
        while (time >= state.nextSnow) {
            SpawnSnowflake(state.nextSnow, w, h);
            state.nextSnow += SnowInterval;
            if (time - state.nextSnow > 2.0) state.nextSnow = time;   // after a long stall, don't burst
        }
    } else {
        if (time >= state.nextFlock) SpawnFlock(time, w, h);
        while (time >= state.nextParticle) {
            SpawnParticle(state.nextParticle, w, h);
            state.nextParticle += ParticleInterval;
            if (time - state.nextParticle > 2.0) state.nextParticle = time;
        }
    }

    // Finished animations are removed
    std::erase_if(state.sprites, [time](const Sprite& s) { return time >= s.start + s.duration; });

    Push origin(g, Translate(screen.x, screen.y));
    g.PushClip({ 0, 0, w, h });
    for (const auto& s : state.sprites) DrawSprite(g, s, time);
    g.PopClip();
}

} // namespace SonicRacingSaveManager::Seasonal::Ambient
