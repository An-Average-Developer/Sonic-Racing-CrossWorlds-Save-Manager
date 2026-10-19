#include "Windows/Splash/LoadingScreen.h"
#include <algorithm>
#include <cmath>
#include "App/Paths.h"
#include "Core/Process.h"
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "Services/App/AppVersion.h"
#include "UI/Material/Motion.h"
#include "UI/Material/PackIcon.h"
#include "UI/Material/Theme.h"
#include "Windows/Main/MainWindow.h"

namespace SonicRacingSaveManager::Main {

namespace {

namespace M = UI::Material;

constexpr float kPi = 3.14159265f;

// Pictures cycling in the orb (relative to Assets)
constexpr const wchar_t* kCards[] = { L"Steam\\icons\\iconHD.png", L"Tickets.png", L"Steam\\Logo.png" };
constexpr int kCardCount = static_cast<int>(std::size(kCards));
constexpr double kCardTime = 0.6;

// MainWindow_Loaded's simulated loading steps (seconds from the start)
struct Step {
    double at;
    double progress;
    const char* status;
};
constexpr Step kSteps[] = {
    { 1.2, 20, "loading_step_resources" }, { 2.7, 45, "loading_step_components" }, { 4.2, 70, "loading_step_data" },
    { 5.7, 90, "loading_step_finalizing" }, { 6.7, 100, "loading_step_ready" },
};
constexpr int kStepCount = static_cast<int>(std::size(kSteps));
constexpr double kComplete = 7.9;           // CompleteLoading
constexpr double kFadeStart = kComplete + 0.5, kFadeEnd = kFadeStart + 0.5;

constexpr float kCardW = 640.f, kCardPad = 32.f, kOrb = 64.f;
constexpr Color kRed = Argb(0xFFFF5252);

ImagePtr Card(int index) { return Images::Load(Paths::Asset(kCards[index])); }

TextStyle Centered(TextStyle t) {
    t.align = HAlign::Center;
    t.valign = VAlign::Center;
    return t;
}

// Sparks drifting up the screen, each on its own column, speed and phase
void Sparks(UI::Ui& ui, const Rect& screen, Color color, float t) {
    for (int i = 0; i < 26; ++i) {
        const float seed = static_cast<float>(i) * 12.9898f;
        const float fx = std::fmod(std::sin(seed) * 43758.5453f, 1.f);
        const float speed = 0.04f + 0.05f * std::fmod(std::fabs(std::cos(seed)) * 9.1f, 1.f);
        const float k = std::fmod(t * speed + static_cast<float>(i) * 0.137f, 1.f);
        const float x = screen.x + screen.w * std::fabs(fx) + 18.f * std::sin(t * 0.8f + seed);
        const float y = screen.B() - (screen.h + 40.f) * k;
        const float size = 1.5f + 2.f * std::fabs(std::sin(seed * 3.f));
        const float alpha = 0.55f * std::sin(k * kPi);
        ui.g.GlowRound({ x - size, y - size, size * 2.f, size * 2.f }, size, color, 6.f, alpha * 0.8f);
        ui.g.FillEllipse(x, y, size, size, color.Fade(alpha));
    }
}

} // namespace

void LoadingOverlay::Start(double now) {
    started = true;
    startedAt = now;
    progressClock = now;
}

void LoadingOverlay::Advance(double now) {
    const double t = now - startedAt;
    while (step < kStepCount && t >= kSteps[step].at) {
        targetProgress = kSteps[step].progress;
        ++step;
        stepAt = now;
    }
    // _progressTimer: every 16 ms the bar closes 10% of the remaining distance
    while (now - progressClock >= 0.016) {
        progressClock += 0.016;
        if (currentProgress < targetProgress) {
            currentProgress += (targetProgress - currentProgress) * 0.1;
            if (targetProgress - currentProgress < 0.5) currentProgress = targetProgress;
        }
    }
}

// The red glass box: alert icon that shakes every few seconds, "WARNING", the two lines and the
// GitHub link
float LoadingOverlay::DrawWarning(UI::Ui& ui, float centerX, float y, bool measureOnly) {
    const TextStyle headline = Centered(M::Bold(14.f));
    const TextStyle line = Centered(M::SemiBold(13.f));
    const std::wstring& title = Loc::Get("loading_warning_title");
    const std::wstring& free = Loc::Get("loading_warning_free");
    const std::wstring& scam = Loc::Get("loading_warning_scam");
    // "Download the original version {0}" with {0} = the link
    const std::wstring& here = Loc::Get("loading_warning_here");
    const std::wstring& download = Loc::Get("loading_warning_download");
    const size_t at = download.find(L"{0}");
    const std::wstring lead = at == std::wstring::npos ? download : download.substr(0, at);
    const std::wstring tail = at == std::wstring::npos ? L"" : download.substr(at + 3);
    TextStyle left = line;
    left.align = HAlign::Left;
    TextStyle link = left;
    link.underline = true;
    const float leadW = std::ceil(ui.g.Measure(lead, left).w), hereW = std::ceil(ui.g.Measure(here, link).w),
                tailW = tail.empty() ? 0.f : std::ceil(ui.g.Measure(tail, left).w);
    const float contentW = std::min(760.f, std::max({ 30.f + ui.g.Measure(title, headline).w, ui.g.Measure(free, line).w,
                                                       ui.g.Measure(scam, line).w, leadW + hereW + tailW }));
    const float lineH = 20.f;
    const float h = 14.f * 2.f + 24.f + 6.f + lineH * 3.f;
    if (measureOnly) return h;

    const float t = static_cast<float>(ui.Time());
    const Rect box{ centerX - (contentW + 56.f) * 0.5f, y, contentW + 56.f, h };
    ui.g.GlowRound(box, 16.f, kRed, 18.f, 0.25f + 0.1f * std::sin(t * 2.f));
    ui.g.Border(box, 16.f, kRed.Fade(0.12f), kRed.Fade(0.55f), 1.5f);
    float cy = box.y + 14.f;
    // Shake for 0.5 s every 4 s
    const float phase = std::fmod(t, 4.f);
    const float shake = phase < 0.5f ? 8.f * (1.f - phase / 0.5f) * std::sin(phase * 40.f) : 0.f;
    const float headW = 30.f + ui.g.Measure(title, headline).w;
    const float hx = box.CX() - headW * 0.5f;
    ui.g.PushTransform(D2D1::Matrix3x2F::Rotation(shake, D2D1::Point2F(hx + 11.f, cy + 12.f)));
    M::DrawIcon(ui.g, M::Icon::AlertCircle, hx + 11.f, cy + 12.f, 22.f, kRed);
    ui.g.PopTransform();
    TextStyle headLeft = headline;
    headLeft.align = HAlign::Left;
    ui.g.Text(title, { hx + 30.f, cy, headW, 24.f }, kRed, headLeft);
    cy += 24.f + 6.f;
    const Color text = Lerp(kRed, Colors::White, 0.45f);
    ui.g.Text(free, { box.x, cy, box.w, lineH }, text, line);
    cy += lineH;
    ui.g.Text(scam, { box.x, cy, box.w, lineH }, text, line);
    cy += lineH;
    const float lx = box.CX() - (leadW + hereW + tailW) * 0.5f;
    ui.g.Text(lead, { lx, cy, leadW + 1.f, lineH }, text, left);
    const Rect hereR{ lx + leadW, cy, hereW + 1.f, lineH };
    const UI::Id id = ui.MakeId("loading-here");
    const UI::Interaction it = ui.Interact(id, hereR);
    if (it.hovered) ui.SetCursor(IDC_HAND);
    const float hover = M::Motion::Hover(ui, id, it.hovered, false).hover;
    ui.g.Text(here, hereR, Lerp(Argb(0xFF66B3FF), Colors::White, 0.4f * hover), link);
    if (!tail.empty()) ui.g.Text(tail, { hereR.R(), cy, tailW + 1.f, lineH }, text, left);
    if (it.clicked) Process::OpenUrl(AppVersion::GitHubRepoUrl() + L"/releases/latest");
    return h;
}

float LoadingOverlay::CardHeight() const {
    // Orb + 34, title 34, tagline 34, bar 12 + 22, dots 10 + 14, status 22
    return kCardPad * 2.f + kOrb * 2.f + 34.f + 34.f + 34.f + 34.f + 24.f + 22.f;
}

void LoadingOverlay::Draw(UI::Ui& ui, const Rect& screen) {
    if (!visible) return;
    const double now = ui.Time();
    if (!started) Start(now);
    Advance(now);
    const double t = now - startedAt;
    if (t >= kFadeEnd) {
        // fadeOut.Completed: collapse, stop the progress timer, check for updates
        visible = false;
        main.LoadingFinished();
        return;
    }
    ui.KeepAnimating();
    const float ft = static_cast<float>(t);
    const float opacity = t > kFadeStart ? 1.f - static_cast<float>((t - kFadeStart) / (kFadeEnd - kFadeStart)) : 1.f;
    if (opacity < 1.f) ui.g.PushOpacity(opacity);
    ui.Block(screen);
    const M::Palette& p = M::P();
    const Color accent = p.primaryMid.color, accent2 = p.secondaryMid.color;

    // Background: the theme's dark background, two wandering glows, sparks
    ui.g.FillRect(screen, p.background);
    ui.g.FillLinear(screen, 0.f, { { 0.f, p.primaryDark.color.Fade(0.55f) }, { 0.55f, p.background.Fade(0.f) }, { 1.f, accent2.Fade(0.12f) } },
                    { 0.f, 0.f }, { 1.f, 1.f });
    const float g1x = screen.x + screen.w * (0.3f + 0.12f * std::sin(ft * 0.35f)), g1y = screen.y + screen.h * (0.35f + 0.08f * std::cos(ft * 0.3f));
    ui.g.FillRadial({ g1x - 420.f, g1y - 320.f, 840.f, 640.f }, { { 0.f, accent.Fade(0.3f) }, { 1.f, accent.Fade(0.f) } });
    const float g2x = screen.x + screen.w * (0.72f + 0.1f * std::cos(ft * 0.27f)), g2y = screen.y + screen.h * (0.68f + 0.07f * std::sin(ft * 0.4f));
    ui.g.FillRadial({ g2x - 360.f, g2y - 280.f, 720.f, 560.f }, { { 0.f, accent2.Fade(0.18f) }, { 1.f, accent2.Fade(0.f) } });
    Sparks(ui, screen, Lerp(accent2, Colors::White, 0.3f), ft);

    const bool complete = t >= kComplete;
    const float doneK = complete ? Anim::Clamp01(static_cast<float>((t - kComplete) / 0.5)) : 0.f;
    const float warningH = DrawWarning(ui, 0.f, 0.f, true);
    const float cardH = CardHeight();
    const float total = cardH + (warningH > 0.f ? 24.f + warningH : 0.f);
    float y = screen.CY() - total * 0.5f;

    // The card rises in, and grows a little once loading is complete
    const float rise = Anim::EaseOutBack(Anim::Clamp01(ft / 0.7f), 0.8f);
    const Rect card{ screen.CX() - kCardW * 0.5f, y, kCardW, cardH };
    ui.g.PushOpacity(Anim::Clamp01(ft / 0.4f));
    ui.PushTranslate(0.f, 30.f * (1.f - rise));
    ui.PushScale(Anim::Lerp(0.92f, 1.f, rise) * (1.f + 0.04f * Anim::EaseOutBack(doneK, 1.f)), card.CX(), card.CY());
    ui.g.GlowRound(card, 24.f, accent, 40.f, 0.35f + 0.1f * std::sin(ft * 1.6f));
    ui.g.FillRound(card, 24.f, Lerp(p.background, Colors::Black, 0.15f).Fade(0.82f));
    ui.g.FillLinear(card, 24.f, { { 0.f, Colors::White.Fade(0.08f) }, { 0.5f, Colors::White.Fade(0.02f) }, { 1.f, Colors::White.Fade(0.f) } },
                    { 0.f, 0.f }, { 0.f, 1.f });
    ui.g.StrokeRound(card.Inset(0.5f), 23.5f, Colors::White.Fade(0.14f), 1.f);
    float cy = card.y + kCardPad;

    // Orb: the card image cycling in a glowing circle, two arcs turning around it
    {
        const float ocx = card.CX(), ocy = cy + kOrb;
        const int index = static_cast<int>((complete ? kComplete : t) / kCardTime) % kCardCount;
        const float since = complete ? 1.f : static_cast<float>(std::fmod(t, kCardTime) / 0.25);
        const float pop = Anim::EaseOutBack(Anim::Clamp01(since), 1.4f);
        const float orb = kOrb * (1.f + 0.1f * Anim::EaseOutBack(doneK, 1.f));
        const float ring = static_cast<float>(std::fmod(t, 1.6) / 1.6);
        ui.g.StrokeEllipse(ocx, ocy, orb + 8.f + 22.f * ring, orb + 8.f + 22.f * ring, accent2.Fade(0.35f * (1.f - ring)), 2.f);
        ui.g.GlowRound({ ocx - orb, ocy - orb, orb * 2.f, orb * 2.f }, orb, accent, 30.f, 0.6f);
        ui.g.FillRadial({ ocx - orb, ocy - orb, orb * 2.f, orb * 2.f }, { { 0.f, Lerp(accent, Colors::White, 0.25f) }, { 1.f, p.primaryDark.color } });
        const float speed = complete ? 40.f : 140.f;
        for (int i = 0; i < 2; ++i) {
            const float start = (ft * speed + 180.f * static_cast<float>(i)) * kPi / 180.f, sweep = 70.f * kPi / 180.f;
            const float radius = orb + 8.f;
            for (int s = 0; s < 12; ++s) {
                const float a0 = start + sweep * s / 12.f, a1 = start + sweep * (s + 1) / 12.f;
                ui.g.Line(ocx + radius * std::sin(a0), ocy - radius * std::cos(a0), ocx + radius * std::sin(a1), ocy - radius * std::cos(a1),
                          accent2.Fade(0.3f + 0.7f * s / 12.f), 3.f, true);
            }
        }
        if (const ImagePtr image = Card(index)) {
            const float box = orb * 1.25f * (0.7f + 0.3f * pop);
            const float fit = std::min(box / image->w, box / image->h);
            const float iw = image->w * fit, ih = image->h * fit;
            const float sway = complete ? 0.f : 6.f * std::sin(ft * 2.2f);
            const Rect ir{ ocx - iw * 0.5f, ocy - ih * 0.5f + 2.f * std::sin(ft * 3.f), iw, ih };
            ui.g.PushTransform(D2D1::Matrix3x2F::Rotation(sway, { ocx, ocy }));
            ui.g.GlowImage(*image, ir, Argb(0xFF66B3FF), 16.f + 8.f * std::sin(ft * 4.f), 0.7f);
            ui.g.DrawImage(*image, ir, 1.f, Stretch::Fill);
            ui.g.PopTransform();
        }
        cy += kOrb * 2.f + 34.f;
    }

    // Title: "SONIC RACING CROSSWORLDS" + "SAVE MANAGER" in the accent colour, glowing
    {
        TextStyle title = M::Text(26.f, DWRITE_FONT_WEIGHT_BOLD);
        title.valign = VAlign::Center;
        {
            const std::wstring first = L"SONIC RACING CROSSWORLDS", second = L"SAVE MANAGER";
            const float fw = std::ceil(ui.g.Measure(first, title).w), sw = std::ceil(ui.g.Measure(second, title).w);
            const float x = card.CX() - (fw + 10.f + sw) * 0.5f;
            ui.g.Text(first, { x, cy, fw + 2.f, 34.f }, Colors::White, title);
            const Rect sr{ x + fw + 10.f, cy, sw + 2.f, 34.f };
            ui.g.GlowText(second, sr, title, accent2, 12.f, 0.35f + 0.2f * std::sin(ft * 2.f));
            ui.g.Text(second, sr, Lerp(p.secondaryLight.color, Colors::White, 0.2f), title);
        }
        cy += 34.f;
        {
            TextStyle caption = M::Text(12.f);
            caption.valign = VAlign::Center;
            const std::wstring& tagline = Loc::Get("app_tagline");
            const std::wstring version = AppVersion::DisplayVersion();
            TextStyle pill = M::Text(10.f, DWRITE_FONT_WEIGHT_BOLD);
            pill.align = HAlign::Center;
            pill.valign = VAlign::Center;
            const float tw = std::ceil(ui.g.Measure(tagline, caption).w) + 2.f, pw = std::ceil(ui.g.Measure(version, pill).w) + 16.f;
            const float x = card.CX() - (tw + 8.f + pw) * 0.5f;
            ui.g.Text(tagline, { x, cy + 6.f, tw, 18.f }, Colors::White.Fade(0.7f), caption);
            const Rect pr{ x + tw + 8.f, cy + 7.f, pw, 16.f };
            ui.g.FillRound(pr, 8.f, accent.Fade(0.3f));
            ui.g.StrokeRound(pr.Inset(0.5f), 7.5f, Colors::White.Fade(0.25f), 1.f);
            ui.g.Text(version, pr, Colors::White, pill);
        }
        cy += 34.f;
    }

    // Progress: track, gradient fill with a shimmer running over it, the percentage on the right
    {
        const float barW = kCardW - kCardPad * 2.f - 52.f;
        const Rect track{ card.x + kCardPad, cy, barW, 12.f };
        ui.g.FillRound(track, 6.f, Colors::White.Fade(0.08f));
        const float fillW = barW * static_cast<float>(currentProgress / 100.0);
        if (fillW > 0.5f) {
            const Rect bar{ track.x, track.y, std::max(12.f, fillW), track.h };
            ui.g.GlowRound(bar, 6.f, accent2, 12.f, 0.7f);
            ui.g.FillLinear(bar, 6.f, { { 0.f, accent }, { 1.f, Lerp(accent2, Colors::White, 0.15f) } }, { 0.f, 0.f }, { 1.f, 0.f });
            const float k = std::fmod(ft / 1.3f, 1.f);
            const float c = -0.3f + 1.6f * k;
            ui.g.FillLinear(bar, 6.f,
                            { { std::clamp(c - 0.2f, 0.f, 1.f), Colors::White.Fade(0.f) },
                              { std::clamp(c, 0.f, 1.f), Colors::White.Fade(0.45f) },
                              { std::clamp(c + 0.2f, 0.f, 1.f), Colors::White.Fade(0.f) } },
                            { 0.f, 0.f }, { 1.f, 0.f });
        }
        TextStyle pct = M::Text(13.f, DWRITE_FONT_WEIGHT_BOLD);
        pct.align = HAlign::Right;
        pct.valign = VAlign::Center;
        ui.g.Text(std::to_wstring(static_cast<int>(std::lround(currentProgress))) + L"%", { track.R(), track.CY() - 10.f, 52.f, 20.f }, Colors::White, pct);
        cy += 12.f + 22.f;
    }

    // Step dots (the newest one pops) and the status
    {
        constexpr float kDot = 10.f, kDotGap = 14.f;
        const float dotsW = kStepCount * kDot + (kStepCount - 1) * kDotGap;
        float dx = card.CX() - dotsW * 0.5f + kDot * 0.5f;
        const float popK = Anim::Clamp01(static_cast<float>((now - stepAt) / 0.4));
        for (int i = 0; i < kStepCount; ++i, dx += kDot + kDotGap) {
            const bool reached = i < step;
            const bool newest = i == step - 1;
            const float r = kDot * 0.5f * (newest ? 1.f + 0.5f * (1.f - Anim::EaseOutCubic(popK)) : 1.f);
            if (newest && popK < 1.f) ui.g.StrokeEllipse(dx, cy + kDot * 0.5f, r + 8.f * popK, r + 8.f * popK, accent2.Fade(1.f - popK), 1.5f);
            ui.g.FillEllipse(dx, cy + kDot * 0.5f, r, r, reached ? Lerp(accent2, Colors::White, 0.2f) : Colors::White.Fade(0.15f));
        }
        cy += kDot + 14.f;
        const std::wstring& status = Loc::Get(step == 0 ? "loading_initializing" : kSteps[step - 1].status);
        M::Motion::Enter enter(ui, Anim::EaseOutCubic(Anim::Clamp01(static_cast<float>((now - stepAt) / 0.3))), 0.f, 8.f);
        ui.g.Text(status, { card.x, cy, card.w, 22.f }, step >= kStepCount ? Lerp(accent2, Colors::White, 0.3f) : Colors::White.Fade(0.75f),
                  Centered(M::Text(13.f, DWRITE_FONT_WEIGHT_SEMI_BOLD)));
    }
    ui.PopTransform();
    ui.PopTransform();
    ui.g.PopLayer();

    {
        const float k = M::Motion::Stagger(ui, startedAt + 0.35, 0, 0.f, 0.6f);
        M::Motion::Enter enter(ui, k, 18.f);
        DrawWarning(ui, screen.CX(), card.B() + 24.f, false);
    }
    if (opacity < 1.f) ui.g.PopLayer();
}

} // namespace SonicRacingSaveManager::Main
