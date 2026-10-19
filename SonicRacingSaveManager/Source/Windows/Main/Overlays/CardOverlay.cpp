#include "Windows/Main/Overlays/CardOverlay.h"
#include <algorithm>
#include "Graphics/Animation.h"
#include "UI/Material/Elevation.h"
#include "UI/Material/Motion.h"
#include "UI/Material/Theme.h"

namespace SonicRacingSaveManager::Main {

namespace {
namespace M = UI::Material;
constexpr double kOpen = 0.45, kClose = 0.2;
} // namespace

void CardOverlay::Open() {
    if (open) return;
    open = true;
    changedAt = -1.0;
}

void CardOverlay::Close() {
    if (!open) return;
    open = false;
    changedAt = -2.0;
}

Color CardOverlay::Accent() const { return M::P().primaryMid.color; }

float CardOverlay::Reveal(UI::Ui& ui, int index) const { return open ? M::Motion::Stagger(ui, changedAt + 0.15, index, 0.07f, 0.4f) : 1.f; }

void CardOverlay::Frame(UI::Ui& ui, const Rect& card) const {
    M::Elevation(ui.g, card, kRadius, 18.f);
    ui.g.GlowRound(card, kRadius, Accent(), 22.f, 0.35f);
    ui.g.FillRound(card, kRadius, M::P().background);
}

void CardOverlay::Draw(UI::Ui& ui, const Rect& screen) {
    const double now = ui.Time();
    if (changedAt == -1.0 || changedAt == -2.0) changedAt = now;
    const float raw = Anim::Clamp01(static_cast<float>((now - changedAt) / (open ? kOpen : kClose)));
    if (!open && raw >= 1.f) {
        changedAt = -10.0;
        if (onClosed) onClosed();
        return;
    }
    ui.KeepAnimating();
    const float fade = open ? Anim::EaseOutCubic(raw) : 1.f - Anim::EaseInCubic(raw);
    const float scale = open ? Anim::Lerp(0.86f, 1.f, Anim::EaseOutBack(raw, 1.2f)) : Anim::Lerp(1.f, 0.94f, raw);
    const float rise = open ? 40.f * (1.f - Anim::EaseOutCubic(raw)) : 16.f * raw;

    ui.Block(screen);
    ui.g.FillRect(screen, Colors::Black.Fade(0.55f * fade));
    const float w = std::min(CardWidth(), screen.w - 80.f);
    const float h = std::min(CardHeight(ui, w), screen.h - 60.f);
    const Rect card{ screen.CX() - w * 0.5f, screen.CY() - h * 0.5f, w, h };
    const bool covered = cover && cover->Visible();
    if (open && !covered && Closable() && (ui.KeyPressed(VK_ESCAPE) || (ui.In().pressed[0] && !card.Contains(ui.MouseX(), ui.MouseY())))) Close();

    ui.g.PushOpacity(std::max(0.f, fade));
    ui.PushTranslate(0.f, rise);
    ui.PushScale(scale, card.CX(), card.CY());
    ui.Block(card);
    DrawCard(ui, card);
    ui.PopTransform();
    ui.PopTransform();
    ui.g.PopLayer();
}

} // namespace SonicRacingSaveManager::Main
