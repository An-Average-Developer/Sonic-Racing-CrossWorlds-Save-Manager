#include <cmath>
#include "Graphics/Animation.h"
#include "UI/Material/Elevation.h"
#include "UI/Material/Feedback.h"
#include "UI/Material/Motion.h"
#include "UI/Material/PackIcon.h"
#include "UI/Material/Theme.h"
#include "Windows/Main/MainScreen/Bars.h"
#include "Windows/Main/MainState.h"
#include "Windows/Main/MainWindow.h"

// The status bar, redesigned like the header: the PrimaryLight ColorZone as a soft gradient with an
// accent line on top, the info icon in a small glass dot that pulses while something is loading,
// the status message sliding in whenever it changes, and the indeterminate progress bar (Width 200)
// on the right while IsLoading.
namespace SonicRacingSaveManager::Main::StatusBar {

namespace {
namespace M = UI::Material;
}

void Draw(UI::Ui& ui, MainWindow& main, const Rect& bar) {
    const M::Palette& p = M::P();
    MainState& state = main.State();
    const Color zone = p.primaryLight.color, fg = p.primaryLight.foreground;
    const float t = static_cast<float>(ui.Time());
    ui.Block(bar);

    // ColorZone Mode="PrimaryLight" (Dp2): gradient towards the mid tone, accent line on the top edge
    M::Elevation(ui.g, bar, 0.f, 2.f);
    ui.g.FillLinear(bar, 0.f, { { 0.f, zone }, { 1.f, Lerp(zone, p.primaryMid.color, 0.35f) } }, { 0.f, 0.f }, { 1.f, 0.f });
    ui.g.FillRect({ bar.x, bar.y, bar.w, 1.f }, p.secondaryMid.color.Fade(0.45f));

    // Info icon (the dot breathes while loading)
    const float busy = ui.Tween(ui.MakeId("status-busy"), state.isLoading ? 1.f : 0.f, 0.25f);
    const float cx = bar.x + 16.f + 11.f, cy = bar.CY();
    const float breathe = busy * (0.5f + 0.5f * std::sin(t * 5.f));
    if (busy > 0.f) ui.KeepAnimating();
    ui.g.FillEllipse(cx, cy, 13.f, 13.f, fg.Fade(0.1f + 0.12f * breathe));
    M::DrawIcon(ui.g, M::Icon::Information, cx, cy, 18.f, fg);

    // StatusMessage: each new message slides in from below
    const std::wstring& text = state.statusMessage;
    auto& shown = ui.Data<std::wstring>(ui.MakeId("status-text"));
    double& changedAt = ui.Data<double>(ui.MakeId("status-changed"));
    if (shown != text) {
        shown = text;
        changedAt = ui.Time();
    }
    const float k = Anim::EaseOutCubic(Anim::Clamp01(static_cast<float>((ui.Time() - changedAt) / 0.3)));
    if (k < 1.f) ui.KeepAnimating();
    TextStyle style = M::Medium(13.f);
    style.valign = VAlign::Center;
    const float textX = cx + 22.f;
    const float barW = 200.f;
    const Rect textR{ textX, bar.y + 8.f * (1.f - k), bar.R() - textX - barW - 40.f, bar.h };
    ui.g.PushOpacity(k);
    ui.g.Text(text, textR, fg, style);
    ui.g.PopLayer();

    // <ProgressBar IsIndeterminate="True" Width="200" Height="4"/> while IsLoading
    if (busy > 0.01f) {
        ui.g.PushOpacity(busy);
        M::ProgressBar(ui, { bar.R() - 16.f - barW, bar.CY() - 2.f, barW, 4.f }, 0.f, true, fg);
        ui.g.PopLayer();
    }
}

} // namespace SonicRacingSaveManager::Main::StatusBar
