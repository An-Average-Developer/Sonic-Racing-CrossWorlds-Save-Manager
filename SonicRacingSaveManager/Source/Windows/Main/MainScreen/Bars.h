#pragma once
#include "Graphics/Rect.h"
#include "UI/Ui.h"

namespace SonicRacingSaveManager::Main {

class MainWindow;

// HeaderGrid: the PrimaryMid ColorZone (icon, title, version) and the floating buttons on its right.
namespace Header {
void Draw(UI::Ui& ui, MainWindow& main, const Rect& header);
}

// MainTabControl's tab strip: MaterialDesignNavigationRailTabControl with ShowSelectionBackground.
namespace NavRail {
void Draw(UI::Ui& ui, MainWindow& main, const Rect& rail);
}

// The bar on top of a group of tabs (Backups: Create / Restore / Manage; Mods: Manager / Store): a
// segment per tab with a highlight that slides to the shown one. Ctrl+Tab / Ctrl+Shift+Tab cycle.
namespace SubTabs {
inline constexpr float kHeight = 44.f;
void Draw(UI::Ui& ui, MainWindow& main, const Rect& bar);
}

// The Status Bar ColorZone (PrimaryLight, row 2): StatusMessage on the left, the indeterminate
// progress bar on the right while IsLoading.
namespace StatusBar {
void Draw(UI::Ui& ui, MainWindow& main, const Rect& bar);
}

} // namespace SonicRacingSaveManager::Main
