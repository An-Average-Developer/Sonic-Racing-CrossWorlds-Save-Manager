#include "Windows/Main/Tabs/Tickets/TicketsTab.h"
#include <algorithm>
#include "Localization/Localization.h"
#include "UI/Material/Motion.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Tabs/Tickets/ValueAppliedOverlay.h"

namespace SonicRacingSaveManager::Main {

namespace {
namespace M = UI::Material;
constexpr float kRightW = 360.f, kGap = 16.f;
} // namespace

TicketsTab::TicketsTab(MainWindow& window) : Tab(window), applied(std::make_unique<ValueAppliedOverlay>(window)) { window.AddOverlay(applied.get()); }

TicketsTab::~TicketsTab() = default;

// MemoryEditorViewModel was created with the main view model: Auto-Attach starts with the app
void TicketsTab::OnWindowOpened() {
    state = std::make_unique<TicketsState>(main.Hwnd());
    state->onValueApplied = [this] { applied->Open(); };
}

void TicketsTab::Draw(UI::Ui& ui, const Rect& area) {
    TicketsState& s = *state;
    // Switching between the menu and the editor plays the entrance again
    if (s.selectedTool != shownTool) {
        shownTool = s.selectedTool;
        loadedAt = -1.0;
    }
    if (loadedAt < 0.0) loadedAt = ui.Time();
    const Rect grid = area.Inset(20.f);
    const Rect banner{ grid.x, grid.y, grid.w, kTicketsBannerH };
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt, 0, 0.f, 0.5f), 14.f);
        Banner(ui, banner);
    }
    const float top = banner.B() + kGap;

    if (s.selectedTool == L"tickets") {
        const Rect left{ grid.x, top, grid.w - kRightW - kGap, grid.B() - top };
        const Rect right{ left.R() + kGap, top, kRightW, grid.B() - top };
        {
            M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.1, 0, 0.f, 0.5f), 0.f, -28.f);
            FindTicketsCard(ui, left);
        }
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.18, 0, 0.f, 0.5f), 0.f, 28.f);
        EditValueCard(ui, right);
        return;
    }
    MenuView(ui, { grid.x, top, grid.w, grid.B() - top });
}

} // namespace SonicRacingSaveManager::Main
