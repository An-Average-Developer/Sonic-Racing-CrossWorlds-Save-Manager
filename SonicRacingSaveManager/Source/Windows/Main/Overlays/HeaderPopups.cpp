#include "Windows/Main/Overlays/HeaderPopups.h"
#include <algorithm>
#include "Graphics/Animation.h"
#include "Graphics/Geometry.h"
#include "UI/Material/Elevation.h"
#include "UI/Material/Motion.h"
#include "UI/Material/Theme.h"

namespace SonicRacingSaveManager::Main {

namespace {
constexpr double kOpen = 0.32, kClose = 0.18;
constexpr float kCaret = 8.f;   // the arrow pointing at the button
}

void AnchoredPopup::Toggle(const Rect& at) {
    if (open) {
        Close();
        return;
    }
    anchor = at;
    open = true;
    changedAt = -1.0;   // stamped on the next frame
    OnOpened();
}

void AnchoredPopup::Close() {
    if (!open) return;
    open = false;
    changedAt = -2.0;
}

bool AnchoredPopup::Visible() const { return open || changedAt > -10.0; }

float AnchoredPopup::RowReveal(UI::Ui& ui, int index) const {
    return open ? UI::Material::Motion::Stagger(ui, changedAt + 0.08, index, 0.035f, 0.3f) : 1.f;
}

// Opens with a little bounce, growing out of the button (arrow on top pointing at it) while its
// height unfolds; closes with a quick fade and shrink
void AnchoredPopup::Draw(UI::Ui& ui, const Rect& screen) {
    const double now = ui.Time();
    if (changedAt == -1.0 || changedAt == -2.0) changedAt = now;
    const float raw = Anim::Clamp01(static_cast<float>((now - changedAt) / (open ? kOpen : kClose)));
    if (!open && raw >= 1.f) {
        changedAt = -10.0;
        return;
    }
    if (raw < 1.f) ui.KeepAnimating();
    const float t = open ? raw : 1.f - Anim::EaseInCubic(raw);
    const float fade = open ? Anim::EaseOutCubic(raw) : t;
    const float scale = open ? Anim::Lerp(0.88f, 1.f, Anim::EaseOutBack(raw, 0.9f)) : Anim::Lerp(0.94f, 1.f, t);
    const float unfold = open ? Anim::EaseOutCubic(Anim::Clamp01(raw * 1.3f)) : 1.f;

    const float w = Width(ui);
    const float h = Height(ui, w);
    // Under the button (left edges aligned when it fits), kept inside the window
    Rect box{ anchor.x, anchor.B() + kCaret + 14.f, w, h };   // clear of the header, the caret reaching up to it
    box.x = std::clamp(box.x, screen.x + 8.f, screen.R() - w - 8.f);
    box.y = std::min(box.y, screen.B() - h);
    const float caretX = std::clamp(anchor.CX(), box.x + 16.f, box.R() - 16.f);

    if (open) {
        // StaysOpen="False": the popup takes the mouse; a press outside only closes it
        ui.Block(screen);
        if (ui.In().pressed[0] && !box.Contains(ui.MouseX(), ui.MouseY())) {
            Close();
            ui.Block(screen);
        }
    }

    const auto& p = UI::Material::P();
    ui.g.PushOpacity(std::max(0.f, fade));
    ui.PushTranslate(0.f, -10.f * (1.f - fade));
    ui.PushScale(scale, caretX, box.y);
    ui.Block(box);
    // The box unfolds from the top while it opens
    const Rect shown{ box.x - 30.f, box.y - kCaret - 4.f, box.w + 60.f, (box.h + kCaret + 34.f) * unfold };
    ui.PushClip(shown);
    UI::Material::Elevation(ui.g, box, 10.f, 8.f);
    if (open && raw < 1.f) ui.g.GlowRound(box, 10.f, p.primaryMid.color, 16.f, 0.5f * (1.f - raw));
    ui.g.Border(box, 10.f, p.background, p.primaryMid.color, 2.f);
    // Caret: a small triangle on the top edge, under the button (the border's colour around the
    // paper, merging into the box)
    {
        const auto Triangle = [&](float size, float baseY) {
            return Geometry::Build([&](ID2D1GeometrySink* sink) {
                sink->BeginFigure(D2D1::Point2F(caretX - size, baseY), D2D1_FIGURE_BEGIN_FILLED);
                sink->AddLine(D2D1::Point2F(caretX, baseY - size));
                sink->AddLine(D2D1::Point2F(caretX + size, baseY));
                sink->EndFigure(D2D1_FIGURE_END_CLOSED);
            });
        };
        auto outer = Triangle(kCaret + 2.f, box.y + 2.f);
        auto inner = Triangle(kCaret - 0.5f, box.y + 2.5f);
        ui.g.FillGeometry(outer.Get(), p.primaryMid.color);
        ui.g.FillGeometry(inner.Get(), p.background);
    }
    DrawContent(ui, box.Inset(2.f));
    ui.PopClip();
    ui.PopTransform();
    ui.PopTransform();
    ui.g.PopLayer();
}

} // namespace SonicRacingSaveManager::Main
