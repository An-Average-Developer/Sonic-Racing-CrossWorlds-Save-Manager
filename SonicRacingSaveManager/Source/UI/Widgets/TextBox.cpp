#include "UI/Widgets/TextBox.h"
#include <algorithm>
#include <cmath>
#include "Core/System.h"

namespace SonicRacingSaveManager::UI {

namespace {

struct EditState {
    int caret = 0;
    int anchor = 0;           // selection start (== caret when nothing selected)
    float scroll = 0.f;       // horizontal (single line) or vertical (multi line)
    bool dragging = false;
};

std::wstring Shown(const std::wstring& value, bool password) {
    return password ? std::wstring(value.size(), L'\x2022') : value;
}

bool IsWordChar(wchar_t c) { return iswalnum(c) || c == L'_'; }

} // namespace

void SetTextBoxSelection(Ui& ui, Id id, int anchor, int caret) {
    auto& st = ui.Data<EditState>(id);
    st.anchor = anchor;
    st.caret = caret;
}

float TextBoxHeight(Ui& ui, const TextBoxStyle& style) {
    return ui.g.Measure(L"Ag", style.text).h + style.padding.V() + 2.f;
}

bool TextBox(Ui& ui, Id id, const Rect& r, std::wstring& value, const TextBoxStyle& style, bool* enterPressed) {
    if (enterPressed) *enterPressed = false;
    auto& st = ui.Data<EditState>(id);
    const bool focused = ui.Focus() == id;
    const Interaction it = ui.Interact(id, r, true, false);
    if (it.hovered) ui.SetCursor(IDC_IBEAM);

    const Rect inner = r.Inset(1.f).Inset(style.padding);
    TextStyle ts = style.text;
    ts.valign = VAlign::Top;
    ts.align = HAlign::Left;
    ts.wrap = style.multiline;
    ts.ellipsis = false;
    const float layoutWidth = style.multiline ? std::max(10.f, inner.w - 8.f) : 100000.f;

    st.caret = std::clamp(st.caret, 0, static_cast<int>(value.size()));
    st.anchor = std::clamp(st.anchor, 0, static_cast<int>(value.size()));
    const auto alignOffset = [&] {
        if (style.multiline || style.align != HAlign::Center) return 0.f;
        const float w = ui.g.Measure(Shown(value, style.password), ts).w;
        return w < inner.w ? (inner.w - w) * 0.5f : 0.f;
    };

    // Mouse → caret
    const auto hitIndex = [&]() {
        const float lx = ui.MouseX() - inner.x - alignOffset() + (style.multiline ? 0.f : st.scroll);
        const float ly = ui.MouseY() - inner.y + (style.multiline ? st.scroll : 0.f);
        return ui.g.HitTestText(Shown(value, style.password), ts, layoutWidth, lx, ly);
    };
    if (it.hovered && ui.In().pressed[0]) {
        if (!focused) ui.SetFocus(id);
        const int index = hitIndex();
        if (ui.In().doubleClick) {
            int a = index, b = index;
            while (a > 0 && IsWordChar(value[static_cast<size_t>(a - 1)])) --a;
            while (b < static_cast<int>(value.size()) && IsWordChar(value[static_cast<size_t>(b)])) ++b;
            st.anchor = a;
            st.caret = b;
        } else {
            st.caret = index;
            if (!ui.In().shift) st.anchor = index;
            st.dragging = true;
        }
        ui.ResetCaret();
    }
    if (st.dragging) {
        if (!ui.In().down[0]) st.dragging = false;
        else if (ui.In().mouseMoved) st.caret = hitIndex();
    }

    bool changed = false;
    if (ui.Focus() == id) {
        auto& in = ui.In();
        const auto selMin = [&] { return std::min(st.caret, st.anchor); };
        const auto selMax = [&] { return std::max(st.caret, st.anchor); };
        const auto deleteSelection = [&] {
            if (st.caret == st.anchor) return false;
            value.erase(static_cast<size_t>(selMin()), static_cast<size_t>(selMax() - selMin()));
            st.caret = st.anchor = selMin();
            return true;
        };
        const auto insert = [&](std::wstring text) {
            if (!style.multiline) {
                text.erase(std::remove_if(text.begin(), text.end(), [](wchar_t c) { return c == L'\r' || c == L'\n'; }), text.end());
            } else {
                text.erase(std::remove(text.begin(), text.end(), L'\r'), text.end());
            }
            deleteSelection();
            if (style.maxLength && value.size() + text.size() > style.maxLength)
                text.resize(style.maxLength > value.size() ? style.maxLength - value.size() : 0);
            value.insert(static_cast<size_t>(st.caret), text);
            st.caret += static_cast<int>(text.size());
            st.anchor = st.caret;
            changed = true;
        };
        const auto lineStart = [&](int i) {
            while (i > 0 && value[static_cast<size_t>(i - 1)] != L'\n') --i;
            return i;
        };
        const auto lineEnd = [&](int i) {
            while (i < static_cast<int>(value.size()) && value[static_cast<size_t>(i)] != L'\n') ++i;
            return i;
        };

        for (auto& k : in.keys) {
            if (!k.down || k.handled) continue;
            bool handled = true;
            const int len = static_cast<int>(value.size());
            switch (k.vk) {
                case VK_LEFT:
                    if (!in.shift && st.caret != st.anchor) st.caret = selMin();
                    else if (in.ctrl) {
                        int i = st.caret;
                        while (i > 0 && !IsWordChar(value[static_cast<size_t>(i - 1)])) --i;
                        while (i > 0 && IsWordChar(value[static_cast<size_t>(i - 1)])) --i;
                        st.caret = i;
                    } else st.caret = std::max(0, st.caret - 1);
                    if (!in.shift) st.anchor = st.caret;
                    break;
                case VK_RIGHT:
                    if (!in.shift && st.caret != st.anchor) st.caret = selMax();
                    else if (in.ctrl) {
                        int i = st.caret;
                        while (i < len && IsWordChar(value[static_cast<size_t>(i)])) ++i;
                        while (i < len && !IsWordChar(value[static_cast<size_t>(i)])) ++i;
                        st.caret = i;
                    } else st.caret = std::min(len, st.caret + 1);
                    if (!in.shift) st.anchor = st.caret;
                    break;
                case VK_HOME:
                    st.caret = in.ctrl || !style.multiline ? 0 : lineStart(st.caret);
                    if (!in.shift) st.anchor = st.caret;
                    break;
                case VK_END:
                    st.caret = in.ctrl || !style.multiline ? len : lineEnd(st.caret);
                    if (!in.shift) st.anchor = st.caret;
                    break;
                case VK_UP:
                case VK_DOWN:
                    if (style.multiline) {
                        float cx, cy, ch;
                        ui.g.CaretPosition(Shown(value, style.password), ts, layoutWidth, st.caret, cx, cy, ch);
                        st.caret = ui.g.HitTestText(Shown(value, style.password), ts, layoutWidth, cx,
                                                    k.vk == VK_UP ? cy - ch * 0.5f : cy + ch * 1.5f);
                        if (!in.shift) st.anchor = st.caret;
                    } else handled = false;
                    break;
                case VK_BACK:
                    if (!deleteSelection() && st.caret > 0) {
                        int from = st.caret - 1;
                        if (in.ctrl) {
                            while (from > 0 && !IsWordChar(value[static_cast<size_t>(from)])) --from;
                            while (from > 0 && IsWordChar(value[static_cast<size_t>(from - 1)])) --from;
                        }
                        value.erase(static_cast<size_t>(from), static_cast<size_t>(st.caret - from));
                        st.caret = st.anchor = from;
                    }
                    changed = true;
                    break;
                case VK_DELETE:
                    if (!deleteSelection() && st.caret < len) value.erase(static_cast<size_t>(st.caret), 1);
                    changed = true;
                    break;
                case 'A':
                    if (in.ctrl) {
                        st.anchor = 0;
                        st.caret = len;
                    } else handled = false;
                    break;
                case 'C':
                case 'X':
                    if (in.ctrl && st.caret != st.anchor && !style.password) {
                        System::SetClipboardText(value.substr(static_cast<size_t>(selMin()), static_cast<size_t>(selMax() - selMin())));
                        if (k.vk == 'X') changed = deleteSelection() || changed;
                    } else handled = in.ctrl;
                    break;
                case 'V':
                    if (in.ctrl) insert(System::GetClipboardText());
                    else handled = false;
                    break;
                case VK_RETURN:
                    if (!style.multiline) {
                        if (enterPressed) *enterPressed = true;
                    } else handled = false;  // newline arrives as WM_CHAR
                    break;
                case VK_ESCAPE:
                    ui.ClearFocus();
                    break;
                case VK_TAB:
                    handled = false;
                    break;
                default:
                    handled = false;
            }
            if (handled) {
                k.handled = true;
                ui.ResetCaret();
            }
        }
        for (wchar_t c : in.chars) {
            if (c == 8 || c == L'\t' || (in.ctrl && !in.alt)) continue;
            if (c == L'\r') {
                if (style.multiline) insert(L"\n");
                continue;
            }
            insert(std::wstring(1, c));
        }
        if (!in.chars.empty() && ui.Focus() == id) in.chars.clear();
    }

    // ── Draw ──
    const bool hasFocus = ui.Focus() == id;
    Color border = style.border;
    if (hasFocus) border = style.focusBorder.Transparent() ? style.caret : style.focusBorder;
    ui.g.Border(r, style.radius, style.background, border, 1.f);

    const std::wstring shown = Shown(value, style.password);
    float cx = 0, cy = 0, ch = 0;
    ui.g.CaretPosition(shown, ts, layoutWidth, st.caret, cx, cy, ch);
    // Keep the caret visible
    if (style.multiline) {
        if (cy - st.scroll < 0) st.scroll = cy;
        if (cy + ch - st.scroll > inner.h) st.scroll = cy + ch - inner.h;
        const float total = ui.g.Measure(shown.empty() ? L" " : shown, ts, layoutWidth).h;
        st.scroll = std::clamp(st.scroll, 0.f, std::max(0.f, total - inner.h));
        if (it.hovered && ui.In().wheel != 0.f && total > inner.h) {
            st.scroll = std::clamp(st.scroll - ui.In().wheel / 120.f * 32.f, 0.f, total - inner.h);
            ui.In().wheel = 0.f;
        }
    } else {
        if (cx - st.scroll > inner.w - 2.f) st.scroll = cx - inner.w + 2.f;
        if (cx - st.scroll < 0) st.scroll = cx;
        st.scroll = std::max(0.f, st.scroll);
    }
    const float ox = inner.x - (style.multiline ? 0.f : st.scroll) + alignOffset();
    const float oy = inner.y - (style.multiline ? st.scroll : 0.f);

    ui.PushClip(r.Inset(1.f));
    if (value.empty() && !style.placeholder.empty() && !hasFocus) {
        ui.g.Text(style.placeholder, { inner.x, inner.y, inner.w, inner.h }, style.placeholderColor, ts);
    }
    // Selection
    if (hasFocus && st.caret != st.anchor) {
        IDWriteTextLayout* layout = nullptr;
        (void)layout;
        const int a = std::min(st.caret, st.anchor), b = std::max(st.caret, st.anchor);
        // Walk the range one character at a time per line segment (simple and good enough for short fields)
        float ax, ay, ah, bx, by, bh;
        ui.g.CaretPosition(shown, ts, layoutWidth, a, ax, ay, ah);
        ui.g.CaretPosition(shown, ts, layoutWidth, b, bx, by, bh);
        if (std::fabs(ay - by) < 0.5f) {
            ui.g.FillRect({ ox + ax, oy + ay, bx - ax, ah }, style.selection);
        } else {
            ui.g.FillRect({ ox + ax, oy + ay, layoutWidth - ax, ah }, style.selection);
            for (float y = ay + ah; y < by - 0.5f; y += ah) ui.g.FillRect({ ox, oy + y, layoutWidth, ah }, style.selection);
            ui.g.FillRect({ ox, oy + by, bx, bh }, style.selection);
        }
    }
    const float textW = style.multiline ? layoutWidth : std::max(inner.w, ui.g.Measure(shown, ts).w + 4.f);
    ui.g.Text(shown, { ox, oy, textW, 100000.f }, style.foreground, ts);
    // Caret (blinks every 0.5 s)
    if (hasFocus) {
        const double phase = std::fmod(ui.Time() - ui.CaretStart(), 1.06);
        if (phase < 0.53) ui.g.FillRect({ ox + cx, oy + cy, 1.2f, ch }, style.caret);
        ui.KeepAnimatingThrottled();
    }
    ui.PopClip();
    return changed;
}

} // namespace SonicRacingSaveManager::UI
