#pragma once
#include <string>
#include <vector>

namespace SonicRacingSaveManager::UI {

enum class MouseButton { Left = 0, Right = 1, Middle = 2, X1 = 3, X2 = 4 };

struct KeyEvent {
    unsigned vk = 0;
    bool down = true;
    bool repeat = false;
    bool system = false;     // WM_SYSKEYDOWN (Alt held / F10)
    bool handled = false;
};

// Everything that happened to a window since the previous frame.
struct Input {
    float mouseX = -1.f, mouseY = -1.f;   // DIPs
    bool mouseInside = false;
    bool mouseMoved = false;
    bool down[5]{};
    bool pressed[5]{};                    // went down this frame
    bool released[5]{};                   // went up this frame
    bool doubleClick = false;
    float wheel = 0.f;                    // WHEEL_DELTA units (120 per notch), positive = away from user
    std::vector<KeyEvent> keys;
    std::wstring chars;                   // WM_CHAR text typed this frame
    bool ctrl = false, shift = false, alt = false;

    void BeginFrame() {
        for (bool& p : pressed) p = false;
        for (bool& r : released) r = false;
        doubleClick = false;
        mouseMoved = false;
        wheel = 0.f;
        keys.clear();
        chars.clear();
    }
};

} // namespace SonicRacingSaveManager::UI
