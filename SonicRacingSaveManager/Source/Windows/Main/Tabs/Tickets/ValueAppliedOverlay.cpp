#include "Windows/Main/Tabs/Tickets/ValueAppliedOverlay.h"
#include <cmath>
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "UI/Material/Motion.h"
#include "UI/Material/PackIcon.h"
#include "UI/Material/Theme.h"
#include "Windows/Main/Tabs/TabKit.h"

namespace SonicRacingSaveManager::Main {

namespace {
namespace M = UI::Material;
using TabKit::Style;
const Color kGreen = Argb(0xFF4CAF50), kRed = Argb(0xFFFF1744);
constexpr float kHeaderH = 84.f, kWarningH = 104.f, kFooterH = 76.f;
} // namespace

Color ValueAppliedOverlay::Accent() const { return kGreen; }

float ValueAppliedOverlay::CardHeight(UI::Ui&, float) { return kHeaderH + kPad + kWarningH + kFooterH; }

void ValueAppliedOverlay::DrawCard(UI::Ui& ui, const Rect& card) {
    Frame(ui, card);
    const float t = static_cast<float>(ui.Time());
    // Header (the PrimaryMid ColorZone, now green): the check pops in
    {
        M::Motion::Enter enter(ui, Reveal(ui, 0), 0.f, -10.f);
        const Rect header{ card.x, card.y, card.w, kHeaderH };
        ui.g.PushClip(header);
        ui.g.FillLinear({ header.x, header.y, header.w, header.h + kRadius }, kRadius,
                        { { 0.f, Lerp(kGreen, Colors::Black, 0.25f) }, { 1.f, Lerp(kGreen, Colors::White, 0.1f) } }, { 0.f, 0.f }, { 1.f, 1.f });
        ui.g.PopClip();
        const float pop = M::Motion::Stagger(ui, OpenedAt() + 0.25, 0, 0.f, 0.5f);
        const float s = Anim::EaseOutBack(pop, 2.f);
        M::DrawIcon(ui.g, M::Icon::CheckCircle, header.x + kPad + 18.f, header.CY(), 36.f * s, Colors::White);
        TextStyle title = Style(22.f, DWRITE_FONT_WEIGHT_BOLD);
        title.valign = VAlign::Center;
        ui.g.Text(Loc::Get("tickets_applied_title"), { header.x + kPad + 48.f, header.y, header.w - kPad * 2.f - 48.f, header.h }, Colors::White, title);
    }
    // The warning, in big red letters
    {
        M::Motion::Enter enter(ui, Reveal(ui, 1), 12.f);
        const Rect box{ card.x + kPad, card.y + kHeaderH + kPad, card.w - kPad * 2.f, kWarningH };
        const float pulse = 0.5f + 0.5f * std::sin(t * 3.f);
        ui.KeepAnimatingThrottled();
        ui.g.FillRound(box, 14.f, kRed.Fade(0.08f + 0.04f * pulse));
        ui.g.StrokeRound(box.Inset(0.75f), 13.25f, kRed.Fade(0.35f + 0.25f * pulse), 1.5f);
        TextStyle big = Style(24.f, DWRITE_FONT_WEIGHT_BOLD, false, HAlign::Center);
        big.valign = VAlign::Center;
        ui.g.GlowText(Loc::Get("tickets_applied_line1"), { box.x, box.y + 14.f, box.w, 36.f }, big, kRed, 10.f, 0.25f * pulse);
        ui.g.Text(Loc::Get("tickets_applied_line1"), { box.x, box.y + 14.f, box.w, 36.f }, kRed, big);
        ui.g.GlowText(Loc::Get("tickets_applied_line2"), { box.x, box.y + 52.f, box.w, 36.f }, big, kRed, 10.f, 0.25f * pulse);
        ui.g.Text(Loc::Get("tickets_applied_line2"), { box.x, box.y + 52.f, box.w, 36.f }, kRed, big);
    }
    // OK (Background #4CAF50)
    {
        M::Motion::Enter enter(ui, Reveal(ui, 2), 10.f);
        const float w = 160.f;
        const Rect ok{ card.CX() - w * 0.5f, card.B() - kFooterH + 14.f, w, 44.f };
        if (TabKit::PillButton(ui, ui.MakeId("value-applied-ok"), ok, M::Icon::Check, Loc::Get("ok"), kGreen, true)) Close();
    }
}

} // namespace SonicRacingSaveManager::Main
