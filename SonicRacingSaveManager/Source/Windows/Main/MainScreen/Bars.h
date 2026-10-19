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

// The Status Bar ColorZone (PrimaryLight, row 2): StatusMessage on the left, the indeterminate
// progress bar on the right while IsLoading.
namespace StatusBar {
void Draw(UI::Ui& ui, MainWindow& main, const Rect& bar);
}

} // namespace SonicRacingSaveManager::Main
