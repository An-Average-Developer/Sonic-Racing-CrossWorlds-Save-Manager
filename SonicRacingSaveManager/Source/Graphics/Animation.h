#pragma once
#include <cmath>

// Easing helpers (WPF CubicEase / BackEase / QuadraticEase equivalents).
namespace SonicRacingSaveManager::Anim {

inline float Clamp01(float t) { return t < 0.f ? 0.f : (t > 1.f ? 1.f : t); }
inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }

inline float SmoothStep(float edge0, float edge1, float x) {
    const float t = Clamp01((x - edge0) / (edge1 - edge0));
    return t * t * (3.f - 2.f * t);
}

inline float EaseInCubic(float t) {
    t = Clamp01(t);
    return t * t * t;
}

inline float EaseOutCubic(float t) {
    t = 1.f - Clamp01(t);
    return 1.f - t * t * t;
}

inline float EaseInOutCubic(float t) {
    t = Clamp01(t);
    return t < 0.5f ? 4.f * t * t * t : 1.f - std::pow(-2.f * t + 2.f, 3.f) * 0.5f;
}

inline float EaseOutQuad(float t) {
    t = Clamp01(t);
    return 1.f - (1.f - t) * (1.f - t);
}

inline float EaseInOutSine(float t) {
    t = Clamp01(t);
    return -(std::cos(3.14159265f * t) - 1.f) * 0.5f;
}

// WPF BackEase (EaseOut) with Amplitude
inline float EaseOutBack(float t, float amplitude = 0.3f) {
    t = 1.f - Clamp01(t);
    return 1.f - (t * t * t - t * amplitude * std::sin(t * 3.14159265f));
}

// Frame-rate independent exponential approach towards a target.
inline float Approach(float current, float target, float speed, float dt) {
    return target + (current - target) * std::exp(-speed * dt);
}

// Linear move towards a target at a fixed rate (units per second).
inline float MoveTowards(float current, float target, float rate, float dt) {
    const float step = rate * dt;
    if (current < target) return current + step > target ? target : current + step;
    return current - step < target ? target : current - step;
}

// 0..1..0 triangle wave with a period of `period` seconds (auto-reversing animations).
inline float PingPong(double time, double period) {
    const double t = std::fmod(time, period) / period;
    return static_cast<float>(t < 0.5 ? t * 2.0 : 2.0 - t * 2.0);
}

} // namespace SonicRacingSaveManager::Anim
