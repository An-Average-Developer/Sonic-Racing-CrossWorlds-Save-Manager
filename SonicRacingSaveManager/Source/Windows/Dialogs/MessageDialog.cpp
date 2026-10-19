#include "Windows/Dialogs/MessageDialog.h"
#include <algorithm>
#include <future>
#include <memory>
#include <vector>
#include "Core/Async.h"
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "Services/App/Log.h"
#include "UI/Material/Buttons.h"
#include "UI/Material/Elevation.h"
#include "UI/Material/PackIcon.h"
#include "UI/Material/Theme.h"
#include "UI/Window.h"

namespace SonicRacingSaveManager::Dialogs {

namespace {

namespace M = UI::Material;

constexpr float kShadowMargin = 28.f;   // transparent border holding the Dp24 shadow
constexpr float kWidth = 440.f;         // card width
constexpr float kPadding = 24.f;
constexpr float kIconSize = 36.f;

struct ButtonSpec {
    std::wstring text;
    MessageResult result;
};

std::vector<ButtonSpec> ButtonsFor(MessageButtons b) {
    switch (b) {
        case MessageButtons::OkCancel: return { { Loc::Get("cancel"), MessageResult::Cancel }, { Loc::Get("ok"), MessageResult::Ok } };
        case MessageButtons::YesNo: return { { Loc::Get("no"), MessageResult::No }, { Loc::Get("yes"), MessageResult::Yes } };
        case MessageButtons::YesNoCancel:
            return { { Loc::Get("cancel"), MessageResult::Cancel }, { Loc::Get("no"), MessageResult::No }, { Loc::Get("yes"), MessageResult::Yes } };
        default: return { { Loc::Get("ok"), MessageResult::Ok } };
    }
}

// What closing with Esc / the title bar's X answered in a MessageBox
MessageResult CancelResult(MessageButtons b) {
    switch (b) {
        case MessageButtons::OkCancel:
        case MessageButtons::YesNoCancel: return MessageResult::Cancel;
        case MessageButtons::YesNo: return MessageResult::No;
        default: return MessageResult::Ok;
    }
}

class MessageWindow : public UI::Window {
public:
    MessageWindow(HWND ownerWindow, std::wstring body, std::wstring heading, MessageButtons kind, MessageIcon glyph,
                  std::function<void(MessageResult)> onDone)
        : owner(ownerWindow), text(std::move(body)), title(std::move(heading)), buttons(kind), icon(glyph), done(std::move(onDone)) {}

    bool Open() {
        UI::Window::Options w;
        w.title = title;
        w.style = WS_POPUP;
        w.topmost = true;
        w.owner = owner;
        w.transparent = true;
        w.width = static_cast<int>(kWidth + kShadowMargin * 2.f);
        w.height = 240;
        w.showCommand = SW_HIDE;
        if (!Create(w)) return false;
        if (owner) EnableWindow(owner, FALSE);
        Layout(240.f);
        Show(SW_SHOW);
        Activate();
        MessageBeep(BeepFor());
        return true;
    }

    void Dismiss(MessageResult result) {
        if (finished) return;
        finished = true;
        if (owner) {
            EnableWindow(owner, TRUE);
            SetForegroundWindow(owner);
        }
        auto cb = done;
        Async::Post([cb, result] {
            if (cb) cb(result);
        });
        Close();
    }

protected:
    Color ClearColor() const override { return Colors::Transparent; }

    void OnFrame() override {
        const M::Palette& p = M::P();
        const float cardW = kWidth;
        const Rect card{ kShadowMargin, kShadowMargin, cardW, g.Height() - kShadowMargin * 2.f };

        // Fade + rise + a slight zoom in (EaseOutBack), like the DialogHost open animation
        const float t = ui.Tween(ui.MakeId("open"), 1.f, 0.22f, 0.f);
        if (t < 1.f) {
            const float e = Anim::EaseOutBack(t, 0.6f);
            g.PushOpacity(Anim::Clamp01(t * 1.6f));
            ui.PushTranslate(0.f, (1.f - e) * 12.f);
            ui.PushScale(0.94f + 0.06f * e, card.CX(), card.CY());
        }
        M::Elevation(g, card, 16.f, 24.f);
        g.FillRound(card, 16.f, p.toolkitCard);

        float y = card.y + kPadding;
        const float textX = card.x + kPadding + (icon != MessageIcon::None ? kIconSize + 16.f : 0.f);
        const float textW = card.R() - kPadding - textX;
        if (icon != MessageIcon::None) M::DrawIcon(g, IconKind(), { card.x + kPadding, y, kIconSize, kIconSize }, IconColor());
        if (!title.empty()) {
            TextStyle h = M::Headline6();
            h.wrap = true;
            h.valign = VAlign::Top;
            const float th = g.Measure(title, h, textW).h;
            g.Text(title, { textX, y, textW, th }, p.foreground, h);
            y += th + 12.f;
        }
        TextStyle body = M::Body1();
        body.wrap = true;
        body.valign = VAlign::Top;
        const float bh = g.Measure(text, body, textW).h;
        g.Text(text, { textX, y, textW, bh }, p.foreground.Fade(0.85f), body);
        y += bh;
        if (icon != MessageIcon::None) y = std::max(y, card.y + kPadding + kIconSize);

        // Actions: flat buttons, right aligned (DialogHost "MaterialDesignFlatButton" row)
        y += 24.f;
        const auto specs = ButtonsFor(buttons);
        const M::ButtonLook look = M::Flat();
        float x = card.R() - 8.f;
        bool closed = false;
        for (auto it = specs.rbegin(); it != specs.rend() && !closed; ++it) {
            const float bw = std::max(64.f, M::ButtonWidth(ui, it->text, look));
            x -= bw;
            if (M::TextButton(ui, ui.MakeId("msgbtn", static_cast<int>(it->result)), { x, y, bw, 36.f }, it->text, look)) {
                Dismiss(it->result);
                closed = true;
            }
            x -= 8.f;
        }
        y += 36.f + 8.f;
        if (t < 1.f) {
            ui.PopTransform();
            ui.PopTransform();
            g.PopLayer();
        }
        if (closed) return;
        if (ui.KeyPressed(VK_ESCAPE)) {
            Dismiss(CancelResult(buttons));
            return;
        }
        if (ui.KeyPressed(VK_RETURN)) {
            Dismiss(specs.back().result);
            return;
        }
        // Grow to the content (measured this frame)
        const float wanted = (y - card.y) + kShadowMargin * 2.f;
        if (std::abs(wanted - g.Height()) > 0.5f) Layout(wanted);
    }

private:
    M::Icon IconKind() const {
        switch (icon) {
            case MessageIcon::Warning: return M::Icon::AlertOutline;
            case MessageIcon::Error: return M::Icon::AlertCircle;
            case MessageIcon::Question: return M::Icon::InformationOutline;
            default: return M::Icon::Information;
        }
    }
    Color IconColor() const {
        switch (icon) {
            case MessageIcon::Warning: return Rgba(255, 152, 0);
            case MessageIcon::Error: return M::P().validationError;
            default: return M::P().primaryMid.color;
        }
    }
    UINT BeepFor() const {
        switch (icon) {
            case MessageIcon::Warning: return MB_ICONWARNING;
            case MessageIcon::Error: return MB_ICONERROR;
            case MessageIcon::Question: return MB_ICONQUESTION;
            case MessageIcon::Information: return MB_ICONINFORMATION;
            default: return MB_OK;
        }
    }

    void Layout(float heightDips) {
        const float scale = DpiScale();
        const int w = static_cast<int>((kWidth + kShadowMargin * 2.f) * scale);
        const int h = static_cast<int>(heightDips * scale);
        RECT area{};
        if (owner && IsWindowVisible(owner) && !IsIconic(owner)) GetWindowRect(owner, &area);
        else SystemParametersInfoW(SPI_GETWORKAREA, 0, &area, 0);
        SetWindowPos(hwnd, HWND_TOPMOST, area.left + (area.right - area.left - w) / 2, area.top + (area.bottom - area.top - h) / 2, w, h,
                     SWP_NOACTIVATE);
    }

    HWND owner;
    std::wstring text, title;
    MessageButtons buttons;
    MessageIcon icon;
    std::function<void(MessageResult)> done;
    bool finished = false;
};

std::vector<std::unique_ptr<MessageWindow>>& OpenWindows() {
    static std::vector<std::unique_ptr<MessageWindow>> open;
    return open;
}

} // namespace

void Message(HWND owner, const std::wstring& text, const std::wstring& title, MessageButtons buttons, MessageIcon icon,
             std::function<void(MessageResult)> done) {
    // Every message shown ends up in the log (bug reports can attach it)
    const Log::Level level = icon == MessageIcon::Error ? Log::Level::Error : icon == MessageIcon::Warning ? Log::Level::Warning : Log::Level::Info;
    Log::Write(level, L"Message \"" + title + L"\": " + text);
    auto window = std::make_unique<MessageWindow>(owner, text, title, buttons, icon, std::move(done));
    MessageWindow* raw = window.get();
    raw->onClosed = [raw] {
        auto& list = OpenWindows();
        list.erase(std::remove_if(list.begin(), list.end(), [raw](const auto& p) { return p.get() == raw; }), list.end());
    };
    if (raw->Open()) OpenWindows().push_back(std::move(window));
}

MessageResult MessageBlocking(HWND owner, const std::wstring& text, const std::wstring& title, MessageButtons buttons, MessageIcon icon) {
    auto promise = std::make_shared<std::promise<MessageResult>>();
    auto future = promise->get_future();
    Async::Post([=] { Message(owner, text, title, buttons, icon, [promise](MessageResult r) { promise->set_value(r); }); });
    return future.get();
}

bool AnyMessageOpen() { return !OpenWindows().empty(); }

} // namespace SonicRacingSaveManager::Dialogs
