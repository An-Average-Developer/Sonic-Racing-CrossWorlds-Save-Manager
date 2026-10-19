#pragma once
#include "Graphics/Rect.h"

namespace SonicRacingSaveManager::Main {

// MainWindow.xaml geometry, redesigned like the other C++ ports: a 1475x1000 borderless window, the
// header ColorZone on top, the 72 px navigation rail on the left, the selected tab filling the rest
// and the status bar ColorZone (row 2) along the bottom edge.
struct MainLayout {
    static constexpr float WindowWidth = 1475.f, WindowHeight = 1000.f;
    static constexpr float HeaderHeight = 80.f;
    static constexpr float RailWidth = 72.f;        // MaterialDesignNavigationRailTabItem Width/Height 72
    static constexpr float StatusBarHeight = 36.f;  // Padding 16,8 around a single text line

    Rect window, header, rail, content, statusBar;

    static MainLayout Compute(const Rect& window) {
        MainLayout l;
        l.window = window;
        l.header = { window.x, window.y, window.w, HeaderHeight };
        l.statusBar = { window.x, window.B() - StatusBarHeight, window.w, StatusBarHeight };
        l.rail = { window.x, l.header.B(), RailWidth, l.statusBar.y - l.header.B() };
        l.content = { l.rail.R(), l.header.B(), window.w - RailWidth, l.rail.h };
        return l;
    }
};

} // namespace SonicRacingSaveManager::Main
