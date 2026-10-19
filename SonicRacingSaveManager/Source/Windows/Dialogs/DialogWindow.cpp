#include "Windows/Dialogs/DialogWindow.h"
#include <algorithm>
#include <future>
#include <memory>
#include <vector>
#include "Core/Async.h"
#include "UI/Style.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Controls.h"
#include "UI/Window.h"

namespace SonicRacingSaveManager::Dialogs {

namespace {

constexpr float kTitleH = 48.f, kButtonsH = 52.f;
constexpr Edges kContentPadding{ 20.f, 16.f, 20.f, 16.f };

class DialogWindow : public UI::Window {
public:
    DialogWindow(HWND ownerWindow, Dialogs::Options opts, Content drawContent, std::function<void(Result)> onDone)
        : owner(ownerWindow), options(std::move(opts)), content(std::move(drawContent)), done(std::move(onDone)) {}

    bool Open() {
        UI::Window::Options w;
        w.title = options.title;
        w.style = WS_POPUP;
        w.topmost = true;
        w.owner = owner;
        w.width = static_cast<int>(options.contentWidth + kContentPadding.H() + 2.f);
        w.height = 200;
        w.showCommand = SW_HIDE;
        if (!Create(w)) return false;
        if (owner) EnableWindow(owner, FALSE);
        SetContinuous(options.live);
        Layout(200.f);
        Show(SW_SHOW);
        Activate();
        return true;
    }

protected:
    Color ClearColor() const override { return Hex(0x09141E); }

    void OnFrame() override { DrawDialog(g.Width(), g.Height()); }

    void DrawDialog(float w, float h) {
        const bool hasButtons = !options.primary.empty() || !options.secondary.empty() || !options.close.empty();
        g.FillRect({ 0, 0, w, h }, Hex(0x09141E));

        // Title bar
        const Rect title{ 1.f, 1.f, w - 2.f, kTitleH };
        g.FillRect(title, Hex(0x060F18));
        g.FillRect({ title.x, title.B() - 1.f, title.w, 1.f }, Hex(0x0D2030));
        g.Text(options.title, title.Inset(Edges{ 20.f, 0.f }), Hex(0x00CCAA), UI::Mono(14.f, true));


        // Content
        const Rect area{ kContentPadding.l, kTitleH + 1.f + kContentPadding.t, options.contentWidth, 10000.f };
        const float contentH = content ? content(ui, area) : 0.f;

        // Buttons (right aligned: Close, Secondary, Primary)
        if (hasButtons) {
            const Rect bar{ 1.f, h - kButtonsH - 1.f, w - 2.f, kButtonsH };
            g.FillRect(bar, Hex(0x060F18));
            g.FillRect({ bar.x, bar.y, bar.w, 1.f }, Hex(0x0D2030));
            struct Btn {
                const std::wstring* text;
                bool primary;
                Result result;
            };
            std::vector<Btn> buttons;
            if (!options.close.empty()) buttons.push_back({ &options.close, false, Result::Close });
            if (!options.secondary.empty()) buttons.push_back({ &options.secondary, false, Result::Secondary });
            if (!options.primary.empty()) buttons.push_back({ &options.primary, true, Result::Primary });
            float x = bar.R() - 14.f;
            const TextStyle text = UI::Mono(11.f, true).Center();
            for (auto it = buttons.rbegin(); it != buttons.rend(); ++it) {
                UI::ButtonStyle style{ it->primary ? Rgba(0, 80, 60) : Rgba(10, 24, 37),
                                       it->primary ? Rgba(0, 180, 130, 180) : Rgba(13, 37, 53), 1.f, 3.f, { 12.f, 0.f },
                                       it->primary ? options.primaryMinWidth : 100.f, 36.f };
                const float bw = UI::ButtonWidth(ui, *it->text, text, style);
                x -= bw;
                const bool enabled = !it->primary || !options.primaryEnabled || options.primaryEnabled();
                if (UI::TextButton(ui, ui.MakeId("dlgbtn", static_cast<int>(it->result)), { x, bar.CY() - 18.f, bw, 36.f }, *it->text, text,
                                   it->primary ? Rgba(0, 204, 170) : Rgba(58, 106, 138, 200), style, enabled)) {
                    Finish(it->result);
                    return;
                }
                x -= 8.f;
            }
        }
        if (ui.KeyPressed(VK_ESCAPE)) {
            Finish(Result::Close);
            return;
        }
        // Outer border
        g.StrokeRect({ 0, 0, w, h }, Rgba(21, 48, 72), 1.f);

        const float wanted = kTitleH + 1.f + kContentPadding.V() + contentH + (hasButtons ? kButtonsH : 0.f) + 2.f;
        if (std::abs(wanted - g.Height()) > 0.5f) {
            Layout(wanted);
            Invalidate();
        }
    }

    void OnClosing(bool& cancel) override {
        cancel = false;
        if (!finished) {
            finished = true;
            Reenable();
            if (done) {
                auto cb = done;
                Async::Post([cb] { cb(Result::Close); });
            }
        }
    }

private:
    void Layout(float heightDips) {
        // Centre on the owner (CenterOwner), or on the screen without one
        const float scale = DpiScale();
        const int w = static_cast<int>((options.contentWidth + kContentPadding.H() + 2.f) * scale);
        const int h = static_cast<int>(heightDips * scale);
        RECT area{};
        if (owner && IsWindowVisible(owner) && !IsIconic(owner)) GetWindowRect(owner, &area);
        else SystemParametersInfoW(SPI_GETWORKAREA, 0, &area, 0);
        SetWindowPos(hwnd, HWND_TOPMOST, area.left + (area.right - area.left - w) / 2, area.top + (area.bottom - area.top - h) / 2, w, h,
                     SWP_NOACTIVATE);
    }

    void Reenable() {
        if (owner) {
            EnableWindow(owner, TRUE);
            SetForegroundWindow(owner);
        }
    }

public:
    int id = 0;
    void Dismiss() {
        if (finished) return;
        finished = true;
        Reenable();
        Close();
    }

private:
    void Finish(Result result) {
        if (finished) return;
        finished = true;
        Reenable();
        auto cb = done;
        Async::Post([cb, result] {
            if (cb) cb(result);
        });
        Close();
    }

    HWND owner;
    Dialogs::Options options;
    Content content;
    std::function<void(Result)> done;
    bool finished = false;
};

std::vector<std::unique_ptr<DialogWindow>>& Open() {
    static std::vector<std::unique_ptr<DialogWindow>> open;
    return open;
}

} // namespace

int Show(HWND owner, const Options& options, Content content, std::function<void(Result)> done) {
    static int nextId = 0;
    auto window = std::make_unique<DialogWindow>(owner, options, std::move(content), std::move(done));
    DialogWindow* raw = window.get();
    raw->id = ++nextId;
    raw->onClosed = [raw] {
        auto& list = Open();
        list.erase(std::remove_if(list.begin(), list.end(), [raw](const auto& p) { return p.get() == raw; }), list.end());
    };
    if (raw->Open()) Open().push_back(std::move(window));
    return raw->id;
}

void Close(int id) {
    for (auto& w : Open())
        if (w->id == id) w->Dismiss();
}

Content Text(const std::wstring& text, Color color, float fontSize) {
    return [text, color, fontSize](UI::Ui& ui, const Rect& area) {
        return UI::Paragraph(ui, area.x, area.y, area.w, text, UI::Mono(fontSize), color);
    };
}

void ShowText(HWND owner, const Options& options, const std::wstring& text, Color color, std::function<void(Result)> done) {
    Show(owner, options, Text(text, color), std::move(done));
}

Result ShowBlocking(HWND owner, const Options& options, Content content) {
    auto promise = std::make_shared<std::promise<Result>>();
    auto future = promise->get_future();
    Async::Post([owner, options, content, promise] { Show(owner, options, content, [promise](Result r) { promise->set_value(r); }); });
    return future.get();
}

bool AnyOpen() { return !Open().empty(); }

} // namespace SonicRacingSaveManager::Dialogs
