#include "UI/Material/Fields.h"
#include <algorithm>
#include <cmath>
#include <cwctype>
#include <utility>
#include "Graphics/Animation.h"
#include "UI/Material/Elevation.h"
#include "UI/Material/PackIcon.h"
#include "UI/Widgets/ScrollArea.h"
#include "UI/Widgets/TextBox.h"

namespace SonicRacingSaveManager::UI::Material {

namespace {

constexpr float kPadV = 4.f;   // Constants.TextBoxDefaultPadding (0,4,0,4)
const Edges kOutlinedPadding{ 16.f, 16.f, 12.f, 16.f };   // Constants.OutlinedTextBoxDefaultPadding

TextStyle FieldText(float size) {
    TextStyle t = Text(size);
    t.valign = VAlign::Center;
    return t;
}

Edges PaddingOf(const FieldStyle& s) {
    if (s.padding.l >= 0.f) return s.padding;
    return s.outlined ? kOutlinedPadding : Edges{ 0.f, kPadV, 0.f, kPadV };
}

float LineH(Ui& ui, float size) { return std::ceil(ui.g.Measure(L"Ag", FieldText(size)).h); }

// Underline: 1 px TextBox.Border, hover TextBox.HoverBorder, focused 2 px Primary growing from the center.
// Outline: 1 px OutlineBorder (hover HoverBorder), 2 px Primary while focused.
// `gap` (left, right): where the floating hint interrupts the outline's top edge
void Frame(Ui& ui, Id id, const Rect& r, const FieldStyle& style, bool hovered, bool focused, bool enabled, std::pair<float, float> gap = {}) {
    const Palette& p = P();
    if (style.outlined) {
        const auto stroke = [&] {
            if (focused) ui.g.StrokeRound(r.Inset(1.f), 4.f, p.primaryMid.color, 2.f);
            else ui.g.StrokeRound(r.Inset(0.5f), 4.f, hovered && enabled ? p.textBoxHoverBorder : p.textBoxOutlineBorder, 1.f);
        };
        if (gap.second <= gap.first) {
            stroke();
            return;
        }
        // The top edge band left and right of the gap, and everything under it
        const Rect clips[] = { { r.x - 2.f, r.y + 2.f, r.w + 4.f, r.h }, { r.x - 2.f, r.y - 2.f, gap.first - (r.x - 2.f), 4.f },
                               { gap.second, r.y - 2.f, r.R() + 2.f - gap.second, 4.f } };
        for (const Rect& clip : clips) {
            if (clip.w <= 0.f) continue;
            ui.PushClip(clip);
            stroke();
            ui.PopClip();
        }
        return;
    }
    ui.g.FillRect({ r.x, r.B() - 1.f, r.w, 1.f }, hovered && enabled ? p.textBoxHoverBorder : p.foregroundLight);
    const float grow = ui.Tween(HashId(id, 0x0F0C), focused ? 1.f : 0.f, 0.3f, 0.f);
    if (grow > 0.f) {
        const float w = r.w * Anim::EaseOutCubic(grow);
        ui.g.FillRect({ r.CX() - w * 0.5f, r.B() - 2.f, w, 2.f }, p.primaryMid.color);
    }
}

TextBoxStyle BoxStyle(const FieldStyle& style) {
    const Palette& p = P();
    TextBoxStyle box;
    box.text = FieldText(style.fontSize);
    box.text.valign = VAlign::Top;
    box.align = style.align;
    box.foreground = p.foreground;
    box.background = Colors::Transparent;
    box.border = Colors::Transparent;
    box.focusBorder = Rgba(0, 0, 0, 1);   // invisible: the underline / outline shows the focus
    box.caret = p.primaryMid.color;
    box.selection = p.primaryLight.color.Fade(0.4f);
    const Edges pad = PaddingOf(style);
    // TextBox() insets 1 px for its own border; TextBoxViewMargin adds 1 px left / right
    box.padding = Edges{ pad.l, pad.t - 1.f, pad.r, pad.b - 1.f };
    box.multiline = style.multiline;
    box.text.font = style.font;
    if (!style.foreground.Transparent()) box.foreground = style.foreground;
    if (style.multiline) box.text.wrap = true;
    return box;
}

// HintAssist.Hint (HintOpacity .56, FloatingScale .75). Returns the outline gap for Frame.
std::pair<float, float> Hint(Ui& ui, Id id, const Rect& r, const FieldStyle& style, bool hasValue, bool focused) {
    if (style.hint.empty()) return {};
    const Palette& p = P();
    const Edges pad = PaddingOf(style);
    const float lineH = LineH(ui, style.fontSize);
    const float x = r.x + pad.l + 1.f;
    // At rest: where the text goes (top line of a multiline box, centered in a single-line one)
    const float restY = style.multiline ? r.y + pad.t : r.y + pad.t + (r.h - pad.V() - lineH) * 0.5f;
    TextStyle t = FieldText(style.fontSize);
    t.valign = VAlign::Top;
    if (!style.outlined) {
        if (!hasValue) ui.g.Text(style.hint, { x, restY, r.R() - pad.r - x, lineH }, p.foreground.Fade(0.56f), t);
        return {};
    }
    const float target = focused || hasValue ? 1.f : 0.f;
    const float f = ui.Tween(HashId(id, 0x41A7), target, 0.15f, target);
    t.size = style.fontSize * (1.f - 0.25f * f);
    const float h = lineH * (1.f - 0.25f * f);
    const float y = restY + (r.y - h * 0.5f - restY) * f;
    const Color color = focused ? Lerp(p.foreground.Fade(0.56f), p.primaryMid.color, f) : p.foreground.Fade(0.56f);
    const float w = std::ceil(ui.g.Measure(style.hint, t).w);
    ui.g.Text(style.hint, { x, y, std::max(w, r.R() - pad.r - x), h + 2.f }, color, t);
    if (f <= 0.01f) return {};
    return { x - 4.f, x + w + 4.f };
}

struct ComboState {
    bool open = false;
    int picked = -1;
    bool scrollToSelected = false;
    std::wstring text;        // editable combos
    int textFor = -2;         // selection the text was last synced to
};

struct BoundState {
    std::wstring buffer;
    bool editing = false;
};

bool StartsWithNoCase(const std::wstring& s, const std::wstring& prefix) {
    if (prefix.size() > s.size()) return false;
    for (size_t i = 0; i < prefix.size(); ++i)
        if (std::towlower(s[i]) != std::towlower(prefix[i])) return false;
    return true;
}

} // namespace

float FieldHeight(Ui& ui, float fontSize) { return LineH(ui, fontSize) + kPadV * 2.f + 1.f; }

float FieldHeight(Ui& ui, const FieldStyle& style) {
    const Edges pad = PaddingOf(style);
    return LineH(ui, style.fontSize) + pad.V() + (style.outlined ? 2.f : 1.f);
}

bool TextField(Ui& ui, Id id, const Rect& r, std::wstring& text, const FieldStyle& style, bool enabled) {
    const Palette& p = P();
    if (!enabled) ui.g.PushOpacity(0.56f);   // Constants.TextBoxNotEnabledOpacity
    if (!style.background.Transparent()) ui.g.FillRound(r, style.outlined ? 4.f : 0.f, style.background);
    bool changed = false;
    const TextBoxStyle box = BoxStyle(style);
    if (enabled) {
        changed = TextBox(ui, id, r, text, box);
    } else {
        ui.Interact(id, r, false);
        const Edges pad = PaddingOf(style);
        TextStyle t = FieldText(style.fontSize);
        t.align = style.align;
        ui.g.Text(text, { r.x + pad.l + 1.f, r.y + pad.t, r.w - pad.H() - 2.f, LineH(ui, style.fontSize) }, p.foreground, t);
    }
    const bool focused = enabled && ui.Focus() == id;
    const auto gap = Hint(ui, id, r, style, !text.empty(), focused);
    Frame(ui, id, r, style, enabled && ui.Hot(id), focused, enabled, gap);
    if (!style.tooltip.empty()) ui.Tooltip(id, style.tooltip);
    if (!enabled) ui.g.PopLayer();
    return changed;
}

void BoundTextField(Ui& ui, Id id, const Rect& r, const std::wstring& current, const std::function<void(const std::wstring&)>& commit,
                    const FieldStyle& style, bool enabled) {
    BoundState& s = ui.Data<BoundState>(HashId(id, 0xB0DE));
    const bool focused = ui.Focus() == id;
    if (focused && !s.editing) {
        s.editing = true;
        s.buffer = current;
    }
    if (!focused && s.editing) {
        // LostFocus: push the edited text to the source
        s.editing = false;
        commit(s.buffer);
    }
    std::wstring shown = s.editing ? s.buffer : current;
    TextField(ui, id, r, shown, style, enabled);
    if (s.editing) s.buffer = shown;
}

bool ComboBox(Ui& ui, Id id, const Rect& r, int& selected, const ComboItems& items, const FieldStyle& style, bool enabled, float maxDropHeight,
              bool editable) {
    const Palette& p = P();
    ComboState& state = ui.Data<ComboState>(HashId(id, 0xC0B0));
    bool changed = false;
    if (state.picked >= 0) {
        changed = state.picked != selected;
        selected = state.picked;
        state.picked = -1;
    }
    if (!enabled) {
        state.open = false;
        ui.g.PushOpacity(0.56f);
    }
    const Edges pad = PaddingOf(style);
    const Rect inner{ r.x + pad.l, r.y + pad.t, r.w - pad.l - pad.r, r.h - pad.V() };
    const Rect arrowArea{ r.R() - pad.r - 22.f, r.y, 22.f + pad.r, r.h };
    if (!style.background.Transparent()) ui.g.FillRound(r, style.outlined ? 4.f : 0.f, style.background);

    bool focused = state.open;
    if (editable) {
        // The text part is a TextBox; the arrow toggles the list
        if (state.textFor != selected) {
            state.text = selected >= 0 && selected < items.count ? items.text(selected) : L"";
            state.textFor = selected;
        }
        const Id boxId = HashId(id, 0xC0B2);
        std::wstring text = state.text;
        TextBoxStyle box = BoxStyle(style);
        box.padding.r += 22.f;
        const size_t before = text.size();
        if (enabled && TextBox(ui, boxId, r, text, box)) {
            // TextSearch: the first item starting with the typed text (completed and selected), else none
            int match = -1;
            if (!text.empty())
                for (int i = 0; i < items.count && match < 0; ++i)
                    if (StartsWithNoCase(items.text(i), text)) match = i;
            if (match >= 0 && text.size() > before) {
                const std::wstring full = items.text(match);
                SetTextBoxSelection(ui, boxId, static_cast<int>(text.size()), static_cast<int>(full.size()));
                text = full;
            }
            if (match != selected) {
                selected = match;
                changed = true;
            }
            state.textFor = selected;
        } else if (!enabled) {
            ui.g.Text(text, { inner.x + 1.f, inner.y, inner.w - 22.f, inner.h }, p.foreground, FieldText(style.fontSize));
        }
        state.text = text;
        focused = focused || ui.Focus() == boxId;
        const Interaction arrow = ui.Interact(HashId(id, 0xC0B3), arrowArea, enabled);
        if (arrow.clicked) {
            state.open = !state.open;
            state.scrollToSelected = state.open;
        }
    } else {
        const Interaction it = ui.Interact(id, r, enabled);
        if (it.clicked) {
            state.open = !state.open;
            state.scrollToSelected = state.open;
        }
        if (selected >= 0 && selected < items.count) {
            const Rect shown{ inner.x + 1.f, inner.y, inner.w - 24.f, inner.h };
            if (items.draw) items.draw(selected, shown, false);
            else ui.g.Text(items.text(selected), shown, p.foreground, FieldText(style.fontSize));
        }
    }
    // Toggle arrow (M7,10L12,15L17,10H7Z = MenuDown)
    DrawIcon(ui.g, Icon::MenuDown, r.R() - pad.r - 10.f, r.CY(), 20.f, p.foreground.Fade(0.7f));
    const bool hasValue = editable ? !state.text.empty() : selected >= 0 && selected < items.count;
    const auto gap = Hint(ui, id, r, style, hasValue, focused);
    Frame(ui, id, r, style, enabled && ui.MouseOver(r), focused, enabled, gap);
    if (!style.tooltip.empty()) ui.Tooltip(id, style.tooltip);
    if (!enabled) ui.g.PopLayer();

    if (state.open) {
        // The list draws on top of everything (in window coordinates)
        const Rect anchor = ui.ToScreenRect(r);
        const int sel = selected;
        ComboState* s = &state;
        const float fontSize = style.fontSize;
        const ComboItems list = items;
        ui.Defer([&ui, id, anchor, sel, s, list, fontSize, maxDropHeight] {
            const Palette& pal = P();
            const TextStyle t = FieldText(fontSize);
            const float rowH = (list.itemHeight > 0.f ? list.itemHeight : LineH(ui, fontSize)) + 16.f;   // item Padding 16,8
            const float contentH = rowH * static_cast<float>(list.count);
            const float listH = std::min(maxDropHeight, contentH + 16.f);
            Rect box{ anchor.x, anchor.B(), std::max(anchor.w, 120.f), listH };
            if (box.B() > ui.g.Height()) box.y = std::max(0.f, anchor.y - listH);
            ui.Block(box);
            Elevation(ui.g, box, 4.f, 8.f);
            ui.g.FillRound(box, 4.f, pal.background);
            const Rect view = box.Inset(Edges{ 0.f, 8.f });
            const Id scrollId = HashId(id, 0xC0B4);
            if (s->scrollToSelected) {
                s->scrollToSelected = false;
                ScrollArea::ScrollTo(ui, scrollId, sel >= 0 ? std::max(0.f, rowH * sel - (view.h - rowH) * 0.5f) : 0.f);
            }
            {
                ScrollArea scroll(ui, scrollId, view, false, rowH * 3.f, ScrollArea::Bar::Material);
                // Only the visible rows are laid out (the special moves list has ~1300 items)
                const int first = std::max(0, static_cast<int>(scroll.Offset() / rowH));
                const int last = std::min(list.count, first + static_cast<int>(view.h / rowH) + 2);
                for (int i = first; i < last; ++i) {
                    const Rect row{ view.x, view.y + rowH * static_cast<float>(i), scroll.ContentWidth(), rowH };
                    const Interaction ri = ui.Interact(HashId(static_cast<int64_t>(i), HashId(id, 0xC0B1)), row);
                    if (i == sel) ui.g.FillRect(row, pal.foreground.Fade(0.18f));
                    else if (ri.hovered) ui.g.FillRect(row, pal.foreground.Fade(0.1f));
                    const Rect content = row.Inset(Edges{ 16.f, 8.f });
                    if (list.draw) list.draw(i, content, true);
                    else ui.g.Text(list.text(i), content, pal.foreground, t);
                    if (ri.clicked) {
                        s->picked = i;
                        s->open = false;
                    }
                }
                scroll.SetContentSize(contentH);
            }
            // Clicking anywhere else closes it
            if (ui.In().pressed[0] && !ui.MouseOver(box) && !ui.MouseOver(anchor)) s->open = false;
        });
    }
    return changed;
}

bool ComboBox(Ui& ui, Id id, const Rect& r, int& selected, const std::vector<std::wstring>& items, const FieldStyle& style, bool enabled,
              float maxDropHeight) {
    ComboItems list;
    list.count = static_cast<int>(items.size());
    list.text = [items](int i) { return items[static_cast<size_t>(i)]; };   // copied: the list may outlive the call (deferred dropdown)
    return ComboBox(ui, id, r, selected, list, style, enabled, maxDropHeight, false);
}

} // namespace SonicRacingSaveManager::UI::Material
