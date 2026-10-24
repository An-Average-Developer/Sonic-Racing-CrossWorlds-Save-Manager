#include "Windows/Main/Tabs/Mods/ModsTab.h"
#include <cmath>
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "UI/Material/Card.h"
#include "UI/Material/Motion.h"
#include "UI/Material/PackIcon.h"
#include "UI/Material/Theme.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Tabs/Mods/FileSelectionOverlay.h"
#include "Windows/Main/Tabs/Mods/ModUpdateOverlay.h"
#include "Windows/Main/Tabs/TabBanner.h"
#include "Windows/Main/Tabs/TabKit.h"

namespace SonicRacingSaveManager::Main {

namespace {
namespace M = UI::Material;
using TabKit::Style;
constexpr float kRightW = 360.f, kGap = 16.f, kRadius = 16.f;
const Color kGreen = Argb(0xFF4CAF50), kOrange = Argb(0xFFFF6B00);
} // namespace

ModsTab::ModsTab(MainWindow& window)
    : Tab(window), files(std::make_unique<FileSelectionOverlay>(window)), update(std::make_unique<ModUpdateOverlay>(window, *files)) {
    // The file picker goes over the update card
    window.AddOverlay(files.get());
    window.AddOverlay(update.get());
}

ModsTab::~ModsTab() = default;

void ModsTab::OnWindowOpened() {
    state = std::make_unique<ModsState>(main.Hwnd());
    state->onShowUpdate = [this](const Models::ModInfo& mod, Services::Mods::ModManager& service) { update->Show(mod, service); };
    update->onUpdated = [this](const Models::ModInfo& mod) { state->UpdateFinished(mod); };
}

// ModManagerViewModel was created (and scanned the folder) when the tab's bindings first asked for it
void ModsTab::OnSelected() {
    loadedAt = -1.0;
    state->EnsureInitialized();
}

void ModsTab::Rescan() {
    if (state && state->Initialized()) state->RefreshMods();
}

void ModsTab::Select(const std::wstring& name) {
    if (state) state->selectedMod = name;
}

void ModsTab::Draw(UI::Ui& ui, const Rect& area) {
    ModsState& s = *state;
    if (loadedAt < 0.0) loadedAt = ui.Time();
    const Rect grid = area.Inset(20.f);
    const Rect banner{ grid.x, grid.y, grid.w, kTabBannerHeight };
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt, 0, 0.f, 0.5f), 14.f);
        TabBannerSpec spec;
        spec.icon = M::Icon::Puzzle;
        spec.motion = TabBannerSpec::Motion::Wobble;
        spec.title = Loc::Get("mods_title");
        spec.subtitle = Loc::Get("mods_subtitle");
        spec.pill = s.ModsDirectory().empty() ? Loc::Get("mods_no_folder") : s.ModsDirectory();
        spec.pillIcon = M::Icon::FolderCog;
        TabBanner(ui, banner, spec, loadedAt);
    }
    const float top = banner.B() + kGap;
    const Rect left{ grid.x, top, grid.w - kRightW - kGap, grid.B() - top };
    const Rect right{ left.R() + kGap, top, kRightW, grid.B() - top };
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.1, 0, 0.f, 0.5f), 18.f);
        M::CardLook look;
        look.radius = kRadius;
        M::Card card(ui, ui.MakeId("md-card"), left, Edges(24.f, 20.f), false, 1.f, look);
        const Rect c = card.content;
        const float statsH = Stats(ui, c);
        float y = c.y + statsH + 14.f;
        // The game-running lock (CanExecute of every change)
        const float lock = ui.Tween(ui.MakeId("md-lock"), s.isGameRunning ? 1.f : 0.f, 0.25f);
        if (lock > 0.f) {
            const Rect bar{ c.x, y, c.w, 40.f * lock };
            ui.g.PushClip(bar);
            ui.g.FillRound({ c.x, y, c.w, 40.f }, 12.f, kOrange.Fade(0.14f));
            M::DrawIcon(ui.g, M::Icon::Lock, c.x + 20.f, y + 20.f, 18.f, TabKit::Readable(kOrange));
            TextStyle t = Style(12.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
            t.valign = VAlign::Center;
            ui.g.Text(Loc::Get("mods_status_game_running"), { c.x + 40.f, y, c.w - 50.f, 40.f }, TabKit::Readable(kOrange), t);
            ui.g.PopClip();
            y += (40.f + 12.f) * lock;
        }
        List(ui, { c.x, y, c.w, c.B() - y });
        ReorderVeil(ui, { c.x, y, c.w, c.B() - y });
    }
    const float actionsH = ActionsCard(ui, right, false);
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.18, 0, 0.f, 0.5f), 0.f, 28.f);
        ActionsCard(ui, { right.x, right.y, right.w, actionsH }, true);
    }
    const float py = right.y + actionsH + kGap;
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.26, 0, 0.f, 0.5f), 0.f, 28.f);
        PriorityCard(ui, { right.x, py, right.w, std::min(PriorityCard(ui, right, false), right.B() - py) }, true);
    }
}

// Stats Banner: Total Mods (primary), Enabled (#4CAF50), Disabled (#FF6B00); the numbers roll
float ModsTab::Stats(UI::Ui& ui, const Rect& c) {
    const ModsState& s = *state;
    const M::Palette& p = M::P();
    constexpr float kH = 76.f, kStatGap = 12.f;
    const float w = (c.w - kStatGap * 2.f) / 3.f;
    const struct {
        const char* key;
        int value;
        Color color;
        M::Icon icon;
    } stats[] = { { "mods_total", s.TotalModCount(), p.primaryMid.color, M::Icon::Puzzle },
                  { "mods_enabled", s.EnabledModCount(), kGreen, M::Icon::CheckCircle },
                  { "mods_disabled", s.DisabledModCount(), kOrange, M::Icon::CloseCircle } };
    for (int i = 0; i < 3; ++i) {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.2, i, 0.07f, 0.4f), 12.f);
        const Rect r{ c.x + (w + kStatGap) * static_cast<float>(i), c.y, w, kH };
        ui.g.FillRound(r, 14.f, stats[i].color.Fade(0.1f));
        ui.g.FillRound({ r.x, r.y + 14.f, 4.f, r.h - 28.f }, 2.f, stats[i].color);
        M::DrawIcon(ui.g, stats[i].icon, r.R() - 26.f, r.y + 24.f, 20.f, stats[i].color.Fade(0.7f));
        TextStyle label = Style(11.f);
        label.valign = VAlign::Center;
        ui.g.Text(Loc::Get(stats[i].key), { r.x + 18.f, r.y + 10.f, r.w - 50.f, 18.f }, p.foreground.Fade(0.7f), label);
        const float shown = ui.Animate(ui.MakeId("md-stat", i), static_cast<float>(stats[i].value), 10.f, 0.f);
        if (std::fabs(shown - static_cast<float>(stats[i].value)) > 0.05f) ui.KeepAnimating();
        TextStyle big = Style(28.f, DWRITE_FONT_WEIGHT_BOLD);
        big.valign = VAlign::Center;
        ui.g.Text(std::to_wstring(static_cast<int>(std::lround(shown))), { r.x + 18.f, r.y + 30.f, r.w - 30.f, 38.f }, TabKit::Readable(stats[i].color), big);
    }
    return kH;
}

} // namespace SonicRacingSaveManager::Main
