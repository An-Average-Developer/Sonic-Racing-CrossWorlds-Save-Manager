#include <algorithm>
#include <cmath>
#include "App/Paths.h"
#include "Localization/Localization.h"
#include "UI/Material/Motion.h"
#include "UI/Material/PackIcon.h"
#include "UI/Material/Theme.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Overlays/HeaderPopups.h"

namespace SonicRacingSaveManager::Main {

namespace {

namespace M = UI::Material;

constexpr float kPadding = 10.f, kHeadingH = 20.f;
constexpr float kBannerW = 72.f, kBannerH = 44.f, kGap = 8.f;   // close to the 3:2 of the flags

} // namespace

const LanguagePopup::Language LanguagePopup::kLanguages[] = {
    { L"en", L"gb.png", L"English" },
    { L"es", L"es.png", L"Espa\x00F1ol" },
    { L"fr", L"fr.png", L"Fran\x00E7" L"ais" },
    { L"it", L"it.png", L"Italiano" },
};

ImagePtr LanguagePopup::Flag(const std::wstring& lang) {
    for (const auto& l : kLanguages)
        if (lang == l.code) return Images::Load(Paths::Asset(std::wstring(L"Flags\\") + l.flag));
    return Images::Load(Paths::Asset(L"Flags\\gb.png"));
}

// The whole flag wrapped around a cylinder lying on its side: stretched over the full capsule, dark at the
// top and bottom edges where it turns away, a bright band near the top where the light hits it, and
// a glint that rolls across on hover
void LanguagePopup::Banner(UI::Ui& ui, const Rect& r, const ImagePtr& flag, float hover, float glint) {
    const float radius = r.h * 0.5f;
    if (flag) ui.g.DrawImageRounded(*flag, r, radius, 1.f, Stretch::Fill);
    else ui.g.FillRound(r, radius, Colors::Black.Fade(0.2f));
    ui.g.FillLinear(r, radius,
                    { { 0.f, Colors::Black.Fade(0.32f) },
                      { 0.16f, Colors::White.Fade(0.38f + 0.12f * hover) },
                      { 0.34f, Colors::White.Fade(0.f) },
                      { 0.68f, Colors::Black.Fade(0.f) },
                      { 1.f, Colors::Black.Fade(0.45f) } },
                    { 0.f, 0.f }, { 0.f, 1.f });
    if (glint > 0.f && glint < 1.f) {
        const float k = -0.2f + 1.4f * glint;
        ui.g.FillLinear(r, radius,
                        { { std::clamp(k - 0.14f, 0.f, 1.f), Colors::White.Fade(0.f) },
                          { std::clamp(k, 0.f, 1.f), Colors::White.Fade(0.45f) },
                          { std::clamp(k + 0.14f, 0.f, 1.f), Colors::White.Fade(0.f) } },
                        { 0.f, 0.f }, { 1.f, 0.35f });
    }
    ui.g.StrokeRound(r.Inset(0.5f), radius - 0.5f, Colors::White.Fade(0.3f), 1.f);
}

float LanguagePopup::Width(UI::Ui&) { return kBannerW + kPadding * 2.f + 4.f; }

float LanguagePopup::Height(UI::Ui&, float) {
    const float n = static_cast<float>(std::size(kLanguages));
    return kPadding * 2.f + kHeadingH + n * kBannerH + (n - 1.f) * kGap + 4.f;
}

void LanguagePopup::DrawContent(UI::Ui& ui, const Rect& box) {
    const M::Palette& p = M::P();
    const Rect inner = box.Inset(kPadding);
    {
        M::Motion::Enter enter(ui, RowReveal(ui, 0), 0.f, 16.f);
        TextStyle heading = M::Text(11.f, DWRITE_FONT_WEIGHT_BOLD);
        heading.valign = VAlign::Top;
        ui.g.Text(Loc::Get("language_title"), { inner.x + 2.f, inner.y, inner.w, 16.f }, p.primaryMid.color, heading);
    }
    float y = inner.y + kHeadingH;
    int index = 0;
    for (const auto& l : kLanguages) {
        const Rect r{ inner.x, y, kBannerW, kBannerH };
        y += kBannerH + kGap;
        M::Motion::Enter enter(ui, RowReveal(ui, ++index), 0.f, 24.f);
        const UI::Id id = ui.MakeId("lang", index);
        const UI::Interaction it = ui.Interact(id, r);
        if (it.hovered) ui.SetCursor(IDC_HAND);
        const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, it.held);
        const bool selected = Loc::CurrentLang() == l.code;

        // The glint rolls across once each time the pointer comes in
        double& enteredAt = ui.Data<double>(UI::HashId(id, 0x611));
        bool& wasHovered = ui.Data<bool>(UI::HashId(id, 0x612));
        if (it.hovered && !wasHovered) enteredAt = ui.Time();
        wasHovered = it.hovered;
        const float glint = static_cast<float>((ui.Time() - enteredAt) / 0.6);
        if (glint < 1.f) ui.KeepAnimating();

        const float scale = 1.f + 0.04f * fb.hover - 0.04f * fb.press;
        ui.PushScale(scale, r.CX(), r.CY());
        if (selected) ui.g.GlowRound(r, r.h * 0.5f, p.primaryMid.color, 12.f, 0.6f);
        else if (fb.hover > 0.f) ui.g.GlowRound(r, r.h * 0.5f, p.primaryMid.color, 10.f, 0.35f * fb.hover);
        ui.g.PushOpacity(selected ? 1.f : 0.9f + 0.1f * fb.hover);
        Banner(ui, r, Images::Load(Paths::Asset(std::wstring(L"Flags\\") + l.flag)), fb.hover, glint);
        ui.g.PopLayer();
        if (selected) {
            ui.g.StrokeRound(r.Inset(-3.f), r.h * 0.5f + 3.f, p.primaryMid.color, 2.f);
            // Check badge on the right end
            const float cx = r.R() - 2.f, cy = r.y + 4.f;
            ui.g.FillEllipse(cx, cy, 8.f, 8.f, p.primaryMid.color);
            ui.g.StrokeEllipse(cx, cy, 8.f, 8.f, p.background, 1.5f);
            M::DrawIcon(ui.g, M::Icon::Check, cx, cy, 11.f, p.primaryMid.foreground);
        }
        ui.PopTransform();
        ui.Tooltip(id, l.name);
        if (it.clicked) {
            if (!selected) Loc::Load(l.code);
            Close();
        }
    }
}

} // namespace SonicRacingSaveManager::Main
