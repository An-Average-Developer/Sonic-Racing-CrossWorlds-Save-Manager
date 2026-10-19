#pragma once
#include "Graphics/Rect.h"
#include "UI/Ui.h"

namespace SonicRacingSaveManager::Main {

class MainWindow;

// An in-window layer drawn over the main content (the header popups, the developer card, the
// loading screen...).
class Overlay {
public:
    explicit Overlay(MainWindow& window) : main(window) {}
    virtual ~Overlay() = default;
    virtual bool Visible() const = 0;
    virtual void Draw(UI::Ui& ui, const Rect& screen) = 0;

protected:
    MainWindow& main;
};

} // namespace SonicRacingSaveManager::Main
