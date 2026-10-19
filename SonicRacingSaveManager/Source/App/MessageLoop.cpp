#include "App/MessageLoop.h"
#include <windows.h>
#include <dwmapi.h>
#include <algorithm>
#include "Core/Async.h"
#include "UI/Window.h"

#pragma comment(lib, "dwmapi.lib")

namespace SonicRacingSaveManager::App {

int RunMessageLoop() {
    MSG msg{};
    while (true) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) return static_cast<int>(msg.wParam);
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        Async::Pump();

        bool rendered = false;
        // Copy: a frame may open or close windows
        const auto windows = UI::Window::All();
        for (UI::Window* w : windows) {
            auto& all = UI::Window::All();
            if (std::find(all.begin(), all.end(), w) == all.end()) continue;
            if (w->NeedsFrame()) {
                w->Frame();
                rendered = true;
            }
        }

        if (rendered) {
            // Pace to the compositor (one frame per refresh, every window presents with interval 0)
            if (FAILED(DwmFlush())) Sleep(8);
        } else {
            HANDLE wake = static_cast<HANDLE>(Async::WakeEvent());
            DWORD timeout = Async::NextTimerDelay();
            for (UI::Window* w : UI::Window::All()) timeout = std::min<DWORD>(timeout, w->NextFrameDelay());
            MsgWaitForMultipleObjectsEx(wake ? 1 : 0, wake ? &wake : nullptr, timeout, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        }
    }
}

} // namespace SonicRacingSaveManager::App
