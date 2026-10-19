#pragma once
#include <windows.h>
#include <functional>
#include <string>
#include "Graphics/Graphics.h"
#include "UI/Input.h"
#include "UI/Ui.h"

namespace SonicRacingSaveManager::UI {

// A top-level window rendered with Direct2D and driven by the immediate-mode UI.
// Subclasses implement OnFrame(); the shared message loop (App/MessageLoop) calls Frame()
// whenever the window was invalidated or is animating.
class Window {
public:
    struct Options {
        std::wstring title;
        DWORD style = WS_OVERLAPPEDWINDOW;
        DWORD exStyle = 0;
        int x = CW_USEDEFAULT, y = CW_USEDEFAULT;
        int width = 800, height = 600;     // DIPs (scaled by the monitor DPI)
        HWND owner = nullptr;
        bool transparent = false;          // per-pixel alpha (DirectComposition)
        bool clickThrough = false;         // mouse goes to the window below (game overlays)
        bool topmost = false;
        bool noActivate = false;
        int showCommand = SW_SHOW;
    };

    Window();
    virtual ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool Create(const Options& options);
    HWND Hwnd() const { return hwnd; }
    bool IsOpen() const { return hwnd != nullptr; }
    bool IsVisible() const { return hwnd && IsWindowVisible(hwnd); }
    bool IsMinimized() const { return hwnd && IsIconic(hwnd); }
    void Show(int command = SW_SHOW);
    void Hide();
    void Close();          // like Window.Close(): WM_CLOSE (can be cancelled by OnClosing)
    void Destroy();        // immediate
    void Activate();
    void Invalidate() { dirty = true; }
    // Render every frame even without input / animations. `throttled` caps those idle frames at
    // about 60 fps (input and running animations still render at the full refresh rate).
    void SetContinuous(bool on, bool throttled = false) {
        continuous = on;
        continuousThrottled = throttled;
    }
    // Rate of those idle frames for a throttled continuous window (default ~60 fps).
    void SetIdleFps(double fps) { idleInterval = fps >= 60.0 ? 0.0 : 1.0 / fps; }
    void SetIcon(HICON bigIcon, HICON smallIcon);
    void SetTitle(const std::wstring& title);
    void MoveTo(int x, int y);                 // physical pixels
    void Resize(int width, int height);        // DIPs
    float DpiScale() const { return g.Dpi() / 96.f; }

    // Runs after the window was destroyed (posted to the UI thread, so the owner may delete it).
    std::function<void()> onClosed;

    bool NeedsFrame() const;
    void Frame();
    // Milliseconds until a throttled animation wants its next frame (INFINITE when none).
    unsigned long NextFrameDelay() const;

    // All live windows (for the message loop).
    static std::vector<Window*>& All();

protected:
    virtual void OnFrame() = 0;
    // Return true to swallow the message; `result` is then returned from the window procedure.
    virtual bool OnMessage(UINT msg, WPARAM wp, LPARAM lp, LRESULT& result) { (void)msg; (void)wp; (void)lp; (void)result; return false; }
    virtual void OnClosing(bool& cancel) { (void)cancel; }
    virtual void OnDestroyed() {}
    virtual Color ClearColor() const { return Colors::Black; }

    Graphics g;
    Ui ui{ g };
    Input input;
    HWND hwnd = nullptr;

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT Handle(UINT msg, WPARAM wp, LPARAM lp);
    void UpdateModifiers();
    double ThrottledInterval() const;

    bool dirty = true;
    bool inFrame = false;        // between Begin and End: a resize has to wait for the frame to finish
    bool resizePending = false;
    bool continuous = false;
    bool continuousThrottled = false;
    double idleInterval = 0.0;   // 0 = same as throttled animations
    bool trackingMouse = false;
    bool clickThrough = false;
    double lastFrame = 0.0;
    int captureCount = 0;
};

} // namespace SonicRacingSaveManager::UI
