#include "UI/Window.h"
#include <windowsx.h>
#include <algorithm>
#include "Core/Async.h"

namespace SonicRacingSaveManager::UI {

namespace {

const wchar_t* kClassName = L"SonicRacingSaveManager.Window";

void RegisterClassOnce() {
    static bool registered = false;
    if (registered) return;
    WNDCLASSEXW wc{ sizeof(wc) };
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = DefWindowProcW;  // replaced per window in Create
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kClassName;
    wc.hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(101));
    wc.hIconSm = wc.hIcon;
    RegisterClassExW(&wc);
    registered = true;
}

} // namespace

std::vector<Window*>& Window::All() {
    static std::vector<Window*> windows;
    return windows;
}

Window::Window() { All().push_back(this); }

Window::~Window() {
    auto& all = All();
    all.erase(std::remove(all.begin(), all.end(), this), all.end());
    if (hwnd) {
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        DestroyWindow(hwnd);
        hwnd = nullptr;
    }
    g.Shutdown();
}

bool Window::Create(const Options& o) {
    RegisterClassOnce();
    clickThrough = o.clickThrough;
    DWORD ex = o.exStyle;
    if (o.transparent) ex |= WS_EX_NOREDIRECTIONBITMAP;
    if (o.clickThrough) ex |= WS_EX_TRANSPARENT | WS_EX_LAYERED;
    if (o.topmost) ex |= WS_EX_TOPMOST;
    if (o.noActivate) ex |= WS_EX_NOACTIVATE;

    // Size is given in DIPs: scale by the DPI of the monitor the window will appear on
    POINT probe{ o.x == CW_USEDEFAULT ? 0 : o.x, o.y == CW_USEDEFAULT ? 0 : o.y };
    HMONITOR monitor = MonitorFromPoint(probe, MONITOR_DEFAULTTOPRIMARY);
    UINT dpiX = 96, dpiY = 96;
    using GetDpiForMonitorFn = HRESULT(WINAPI*)(HMONITOR, int, UINT*, UINT*);
    if (HMODULE shcore = LoadLibraryW(L"shcore.dll")) {
        if (auto fn = reinterpret_cast<GetDpiForMonitorFn>(GetProcAddress(shcore, "GetDpiForMonitor"))) fn(monitor, 0, &dpiX, &dpiY);
    }
    const float scale = dpiX / 96.f;
    RECT rc{ 0, 0, static_cast<LONG>(o.width * scale), static_cast<LONG>(o.height * scale) };
    AdjustWindowRectExForDpi(&rc, o.style, FALSE, ex, dpiX);

    hwnd = CreateWindowExW(ex, kClassName, o.title.c_str(), o.style, o.x, o.y, rc.right - rc.left, rc.bottom - rc.top, o.owner,
                           nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!hwnd) return false;
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&Window::WndProc));
    if (o.clickThrough) SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
    if (!g.Init(hwnd, o.transparent)) {
        DestroyWindow(hwnd);
        hwnd = nullptr;
        return false;
    }
    if (o.showCommand != SW_HIDE) ShowWindow(hwnd, o.showCommand);
    dirty = true;
    return true;
}

void Window::Show(int command) {
    if (!hwnd) return;
    ShowWindow(hwnd, command);
    dirty = true;
}

void Window::Hide() {
    if (hwnd) ShowWindow(hwnd, SW_HIDE);
}

void Window::Close() {
    if (hwnd) PostMessageW(hwnd, WM_CLOSE, 0, 0);
}

void Window::Destroy() {
    if (hwnd) DestroyWindow(hwnd);
}

void Window::Activate() {
    if (!hwnd) return;
    if (IsIconic(hwnd)) ShowWindow(hwnd, SW_RESTORE);
    SetForegroundWindow(hwnd);
}

void Window::SetIcon(HICON bigIcon, HICON smallIcon) {
    if (!hwnd) return;
    SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(bigIcon));
    SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(smallIcon ? smallIcon : bigIcon));
}

void Window::SetTitle(const std::wstring& title) {
    if (hwnd) SetWindowTextW(hwnd, title.c_str());
}

void Window::MoveTo(int x, int y) {
    if (hwnd) SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void Window::Resize(int width, int height) {
    if (!hwnd) return;
    const float scale = DpiScale();
    RECT rc{ 0, 0, static_cast<LONG>(width * scale), static_cast<LONG>(height * scale) };
    AdjustWindowRectExForDpi(&rc, static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE)), FALSE,
                             static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_EXSTYLE)), GetDpiForWindow(hwnd));
    SetWindowPos(hwnd, nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

namespace {
// Throttled animations render when this much time has passed. A bit under 1/60 s so the frame
// lands on the next vblank (DwmFlush) instead of skipping one.
constexpr double kThrottledInterval = 1.0 / 60.0 - 1.0 / 240.0;
}

// Input (dirty), running animations and plain continuous windows render at the full refresh rate;
// throttled continuous windows and throttled animations at about 60 fps.
bool Window::NeedsFrame() const {
    if (!hwnd || !IsWindowVisible(hwnd) || IsIconic(hwnd)) return false;
    if (dirty || ui.Animating() || (continuous && !continuousThrottled)) return true;
    const double interval = ThrottledInterval();
    return interval > 0.0 && Async::Now() - lastFrame >= interval;
}

// Interval of the next throttled frame, or 0 when nothing asks for one
double Window::ThrottledInterval() const {
    if (ui.AnimatingThrottled()) return kThrottledInterval;
    if (continuous && continuousThrottled) return idleInterval > 0.0 ? idleInterval : kThrottledInterval;
    return 0.0;
}

unsigned long Window::NextFrameDelay() const {
    const double interval = ThrottledInterval();
    if (!hwnd || !IsWindowVisible(hwnd) || IsIconic(hwnd) || interval <= 0.0) return INFINITE;
    const double ms = (lastFrame + interval - Async::Now()) * 1000.0;
    return ms <= 0.0 ? 0 : static_cast<unsigned long>(ms) + 1;
}

void Window::Frame() {
    if (!hwnd) return;
    const double now = Async::Now();
    // After an idle pause, don't make animations jump
    const float dt = std::clamp(lastFrame > 0.0 ? static_cast<float>(now - lastFrame) : 1.f / 60.f, 0.f, 0.05f);
    lastFrame = now;
    dirty = false;
    if (!g.Begin()) {
        dirty = true;
        return;
    }
    const bool hadInput = input.mouseMoved || input.wheel != 0.f || !input.keys.empty() || !input.chars.empty() ||
                          std::any_of(std::begin(input.pressed), std::end(input.pressed), [](bool b) { return b; }) ||
                          std::any_of(std::begin(input.released), std::end(input.released), [](bool b) { return b; });
    const Id hotBefore = ui.HotId();
    g.Clear(ClearColor());
    UpdateModifiers();
    ui.BeginFrame(input, dt, now);
    inFrame = true;
    OnFrame();
    ui.EndFrame();
    g.End();
    inFrame = false;
    input.BeginFrame();
    // The window was resized while drawing (e.g. a dialog growing to fit its content): resize the
    // swap chain now, or its old size would stay and the new area would show black
    if (resizePending) {
        resizePending = false;
        g.Resize();
        dirty = true;
    }
    // Immediate mode reacts to input while drawing, and hover resolves one frame late:
    // draw once more so state changes and hover highlights show right away.
    if (hadInput || ui.HotId() != hotBefore) dirty = true;
}

void Window::UpdateModifiers() {
    input.ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    input.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    input.alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
}

LRESULT CALLBACK Window::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* self = reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!self) return DefWindowProcW(hwnd, msg, wp, lp);
    return self->Handle(msg, wp, lp);
}

LRESULT Window::Handle(UINT msg, WPARAM wp, LPARAM lp) {
    LRESULT custom = 0;
    if (OnMessage(msg, wp, lp, custom)) return custom;

    const auto mouseButton = [&](int index, bool down) {
        input.mouseX = GET_X_LPARAM(lp) / DpiScale();
        input.mouseY = GET_Y_LPARAM(lp) / DpiScale();
        input.mouseInside = true;
        if (down) {
            input.down[index] = true;
            input.pressed[index] = true;
            if (captureCount++ == 0) SetCapture(hwnd);
        } else {
            if (input.down[index]) input.released[index] = true;
            input.down[index] = false;
            if (captureCount > 0 && --captureCount == 0) ReleaseCapture();
        }
        dirty = true;
    };

    switch (msg) {
        case WM_SIZE:
            if (wp != SIZE_MINIMIZED) {
                if (inFrame) resizePending = true;
                else g.Resize();
            }
            dirty = true;
            return 0;
        case WM_DPICHANGED: {
            const RECT* suggested = reinterpret_cast<const RECT*>(lp);
            SetWindowPos(hwnd, nullptr, suggested->left, suggested->top, suggested->right - suggested->left,
                         suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
            g.SetDpi(static_cast<float>(HIWORD(wp)));
            dirty = true;
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
            dirty = true;
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_NCHITTEST:
            if (clickThrough) return HTTRANSPARENT;
            break;
        case WM_MOUSEMOVE:
            input.mouseX = GET_X_LPARAM(lp) / DpiScale();
            input.mouseY = GET_Y_LPARAM(lp) / DpiScale();
            input.mouseInside = true;
            input.mouseMoved = true;
            if (!trackingMouse) {
                TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, hwnd, 0 };
                TrackMouseEvent(&tme);
                trackingMouse = true;
            }
            dirty = true;
            return 0;
        case WM_MOUSELEAVE:
            trackingMouse = false;
            if (captureCount == 0) input.mouseInside = false;
            dirty = true;
            return 0;
        case WM_LBUTTONDOWN: mouseButton(0, true); return 0;
        case WM_LBUTTONDBLCLK: mouseButton(0, true); input.doubleClick = true; return 0;
        case WM_LBUTTONUP: mouseButton(0, false); return 0;
        case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK: mouseButton(1, true); return 0;
        case WM_RBUTTONUP: mouseButton(1, false); return 0;
        case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK: mouseButton(2, true); return 0;
        case WM_MBUTTONUP: mouseButton(2, false); return 0;
        case WM_XBUTTONDOWN: case WM_XBUTTONDBLCLK: mouseButton(GET_XBUTTON_WPARAM(wp) == XBUTTON1 ? 3 : 4, true); return TRUE;
        case WM_XBUTTONUP: mouseButton(GET_XBUTTON_WPARAM(wp) == XBUTTON1 ? 3 : 4, false); return TRUE;
        case WM_CAPTURECHANGED:
            if (reinterpret_cast<HWND>(lp) != hwnd) {
                captureCount = 0;
                for (int i = 0; i < 5; ++i) {
                    if (input.down[i]) input.released[i] = true;
                    input.down[i] = false;
                }
            }
            return 0;
        case WM_MOUSEWHEEL:
            input.wheel += static_cast<float>(GET_WHEEL_DELTA_WPARAM(wp));
            dirty = true;
            return 0;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            input.keys.push_back({ static_cast<unsigned>(wp), true, (lp & (1 << 30)) != 0, msg == WM_SYSKEYDOWN });
            dirty = true;
            if (msg == WM_SYSKEYDOWN && wp != VK_F4) return 0;  // don't let Alt / F10 open the system menu
            break;
        case WM_KEYUP:
        case WM_SYSKEYUP:
            input.keys.push_back({ static_cast<unsigned>(wp), false, false, msg == WM_SYSKEYUP });
            dirty = true;
            if (msg == WM_SYSKEYUP) return 0;
            break;
        case WM_CHAR:
            if (wp >= 32 || wp == L'\r' || wp == L'\t' || wp == 8) input.chars += static_cast<wchar_t>(wp);
            dirty = true;
            return 0;
        case WM_SETCURSOR:
            if (LOWORD(lp) == HTCLIENT) {
                SetCursor(LoadCursorW(nullptr, ui.Cursor() ? ui.Cursor() : IDC_ARROW));
                return TRUE;
            }
            break;
        case WM_ACTIVATE:
            dirty = true;
            break;
        case WM_CLOSE: {
            bool cancel = false;
            OnClosing(cancel);
            if (!cancel) DestroyWindow(hwnd);
            return 0;
        }
        case WM_DESTROY: {
            OnDestroyed();
            g.Shutdown();
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            hwnd = nullptr;
            if (onClosed) {
                auto callback = onClosed;
                Async::Post([callback] { callback(); });
            }
            return 0;
        }
        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace SonicRacingSaveManager::UI
