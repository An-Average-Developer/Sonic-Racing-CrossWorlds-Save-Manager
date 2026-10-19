#pragma once
#include "Graphics/Rect.h"
#include "UI/Ui.h"

namespace SonicRacingSaveManager::Main {

class MainWindow;

// One page of the navigation rail's TabControl (the TabItem's content).
class Tab {
public:
    explicit Tab(MainWindow& window) : main(window) {}
    virtual ~Tab() = default;
    // `area` is the selected-content area right of the rail and under the header
    virtual void Draw(UI::Ui& ui, const Rect& area) = 0;
    // The tab became the selected one / stopped being it
    virtual void OnSelected() {}
    virtual void OnDeselected() {}
    // The main window exists (its HWND can own dialogs): start what the C# view models started in
    // their constructors
    virtual void OnWindowOpened() {}
    // The main window is closing
    virtual void OnClosing() {}

protected:
    MainWindow& main;
};

} // namespace SonicRacingSaveManager::Main
