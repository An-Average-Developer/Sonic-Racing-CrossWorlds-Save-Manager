#include <algorithm>
#include <cmath>
#include "App/Paths.h"
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "UI/Material/Buttons.h"
#include "UI/Material/Card.h"
#include "UI/Material/Motion.h"
#include "UI/Material/PackIcon.h"
#include "UI/Material/Theme.h"
#include "UI/Material/Toggles.h"
#include "UI/Widgets/ScrollArea.h"
#include "UI/Widgets/TextBox.h"
#include "Windows/Main/Tabs/TabKit.h"
#include "Windows/Main/Tabs/Tickets/TicketsTab.h"

// The control deck under the banner, as in the Inazuma editor's Tool Menu: the search box and the
// quick tiles (Auto-Attach, your tickets) side by side, and the manual process picker
// unfolding under them while Auto-Attach is off. The deck's code measures (draw = false) and draws.
namespace SonicRacingSaveManager::Main {

namespace {

namespace M = UI::Material;
using TabKit::LineH;
using TabKit::Style;

constexpr float kBlockPad = 15.f;
const Color kGreen = Argb(0xFF4CAF50), kAmber = Argb(0xFFFFB300);

M::ButtonLook RaisedOn(Color background) {
    M::ButtonLook l = M::RaisedStatic();
    l.background = background;
    l.text.weight = DWRITE_FONT_WEIGHT_MEDIUM;
    l.radius = 16.f;
    l.padding = Edges(12.f, 0.f);
    return l;
}

} // namespace

float TicketsTab::ControlDeck(UI::Ui& ui, const Rect& area, bool draw) {
    const M::Palette& p = M::P();
    constexpr float kPad = 12.f, kRowH = 60.f, kSearchW = 300.f;
    const float innerW = area.w - kPad * 2.f;
    const float pickerH = pickerOpen > 0.f ? (12.f + ProcessPicker(ui, { 0.f, 0.f, innerW - kBlockPad * 2.f, 0.f }, false) + kBlockPad * 2.f) : 0.f;
    const float h = kPad * 2.f + kRowH + pickerH * pickerOpen;
    if (!draw) return h;

    M::CardLook look;
    look.radius = 16.f;
    M::Card card(ui, ui.MakeId("tk-deck"), { area.x, area.y, area.w, h }, Edges(kPad), false, 1.f, look);
    const Rect c = card.content;
    SearchBox(ui, { c.x, c.y + (kRowH - 44.f) * 0.5f, kSearchW, 44.f });
    QuickTiles(ui, { c.x + kSearchW + 12.f, c.y, c.w - kSearchW - 12.f, kRowH });

    if (pickerOpen > 0.f) {
        const Rect box{ c.x, c.y + kRowH + 12.f, c.w, pickerH - 12.f };
        ui.PushClip({ c.x - 4.f, c.y + kRowH, c.w + 8.f, pickerH * pickerOpen });
        {
            M::Motion::Enter enter(ui, pickerOpen, -10.f);
            ui.g.FillRound(box, 12.f, p.divider);
            ProcessPicker(ui, box.Inset(kBlockPad), true);
        }
        ui.PopClip();
    }
    return h;
}

// Filled pill with a magnifier, the text box and a clear button. Ctrl+F focuses it, Esc clears it.
void TicketsTab::SearchBox(UI::Ui& ui, const Rect& r) {
    const M::Palette& p = M::P();
    const UI::Id id = ui.MakeId("tk-menu-search");
    if (ui.In().ctrl && ui.KeyPressed('F')) ui.SetFocus(id);
    const bool focused = ui.Focus() == id;
    const float focus = ui.Tween(UI::HashId(id, 0xF0C5), focused ? 1.f : 0.f, 0.15f);
    const float hover = M::Motion::Hover(ui, id, ui.MouseOver(r), false).hover;

    if (focus > 0.f) ui.g.GlowRound(r, r.h * 0.5f, p.primaryMid.color, 12.f, 0.45f * focus);
    ui.g.FillRound(r, r.h * 0.5f, Lerp(p.divider, p.foreground.Fade(0.12f), 0.35f * hover));
    if (focus > 0.f) ui.g.StrokeRound(r.Inset(1.f), r.h * 0.5f - 1.f, p.primaryMid.color.Fade(focus), 2.f);
    // The magnifier wiggles while typing
    const float typing = 1.f - Anim::Clamp01(static_cast<float>((ui.Time() - searchChangedAt) / 0.45));
    if (typing > 0.f) ui.KeepAnimating();
    const float wiggle = 14.f * typing * std::sin(static_cast<float>(ui.Time() - searchChangedAt) * 30.f);
    ui.g.PushTransform(D2D1::Matrix3x2F::Rotation(wiggle, D2D1::Point2F(r.x + 24.f, r.CY())));
    M::DrawIcon(ui.g, M::Icon::Magnify, { r.x + 14.f, r.CY() - 10.f, 20.f, 20.f }, Lerp(p.foreground.Fade(0.55f), p.primaryMid.color, focus));
    ui.g.PopTransform();

    UI::TextBoxStyle box;
    box.text = M::Text(13.f);
    box.text.valign = VAlign::Top;
    box.foreground = p.foreground;
    box.background = Colors::Transparent;
    box.border = Colors::Transparent;
    box.focusBorder = Rgba(0, 0, 0, 1);   // invisible: the pill's outline shows the focus
    box.caret = p.primaryMid.color;
    box.selection = p.primaryLight.color.Fade(0.4f);
    box.placeholderColor = p.foreground.Fade(0.45f);
    box.placeholder = Loc::Get("tickets_search_placeholder");
    box.padding = Edges{ 42.f, (r.h - LineH(ui, box.text)) * 0.5f, search.empty() ? 16.f : 40.f, 0.f };
    const std::wstring before = search;
    UI::TextBox(ui, id, r, search, box);

    bool clear = focused && ui.KeyPressed(VK_ESCAPE);
    if (!search.empty()) {
        M::ButtonLook look = M::IconButtonLook(p.foreground.Fade(0.6f));
        look.padding = Edges(0.f);
        const UI::Id clearId = ui.MakeId("tk-menu-search-clear");
        if (M::IconButton(ui, clearId, { r.R() - 36.f, r.CY() - 14.f, 28.f, 28.f }, M::Icon::Close, 16.f, look)) clear = true;
        ui.Tooltip(clearId, Loc::Get("tickets_search_clear"));
    }
    if (clear) {
        search.clear();
        ui.ClearFocus();
    }
    if (search != before) {
        searchChangedAt = ui.Time();
        UI::ScrollArea::ScrollTo(ui, ui.MakeId("tk-scroll"), 0.f);
    }
}

// Auto-Attach | your tickets, side by side
void TicketsTab::QuickTiles(UI::Ui& ui, const Rect& area) {
    TicketsState& s = *state;
    const M::Palette& p = M::P();
    const Color fg = p.foreground;
    constexpr float kGap = 12.f;
    const float tileW = (area.w - kGap) / 2.f;

    const TextStyle name = Style(14.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    const TextStyle desc = Style(11.f);
    // A tile: rounded divider box, hover tint + tilting icon (or picture), accent + outline while
    // `on`; returns the inner rect
    const auto Tile = [&](const char* key, int column, M::Icon icon, const wchar_t* image, bool on) {
        const Rect r{ area.x + (tileW + kGap) * static_cast<float>(column), area.y, tileW, area.h };
        const UI::Id id = ui.MakeId(key);
        const float hover = M::Motion::Hover(ui, id, ui.MouseOver(r), false).hover;
        const float lit = ui.Tween(UI::HashId(id, 0x0071), on ? 1.f : 0.f, 0.25f);
        const float pulse = M::Motion::Pulse(ui, id, on);
        if (pulse > 0.f) ui.g.GlowRound(r, 12.f, p.primaryMid.color, 16.f, 0.6f * pulse);
        ui.g.FillRound(r, 12.f, Lerp(p.divider, p.foreground.Fade(0.14f), 0.3f * hover));
        if (lit > 0.f) {
            ui.g.StrokeRound(r.Inset(0.75f), 11.25f, p.primaryMid.color.Fade(0.6f * lit), 1.5f);
            ui.g.FillRound({ r.x, r.y + 12.f, 4.f, r.h - 24.f }, 2.f, p.primaryMid.color.Fade(lit));
        }
        const Rect in = r.Inset(Edges(16.f, 12.f));
        const float icx = in.x + 13.f, icy = in.CY();
        ui.g.PushTransform(D2D1::Matrix3x2F::Rotation(-12.f * hover + 10.f * pulse, D2D1::Point2F(icx, icy)));
        const ImagePtr picture = image ? Images::Load(Paths::Asset(image)) : nullptr;
        if (picture) {
            const float size = 30.f * (1.f + 0.1f * hover);
            ui.g.DrawImage(*picture, { icx - size * 0.5f, icy - size * 0.5f, size, size }, 1.f, Stretch::Uniform);
        } else {
            M::DrawIcon(ui.g, icon, icx, icy, 26.f * (1.f + 0.1f * hover), Lerp(fg, p.primaryMid.color, lit));
        }
        ui.g.PopTransform();
        return in;
    };
    const auto Texts = [&](const Rect& in, const std::wstring& title, const std::wstring& text, float right) {
        const float tx = in.x + 26.f + 12.f, tw = std::max(0.f, right - tx - 8.f);
        const float stackH = LineH(ui, name) + 2.f + LineH(ui, desc);
        const float ty = in.CY() - stackH * 0.5f;
        TextStyle n = name;
        n.ellipsis = true;
        ui.g.Text(title, { tx, ty, tw, LineH(ui, name) }, fg, n);
        TextStyle d = desc;
        d.ellipsis = true;
        ui.g.Text(text, { tx, ty + LineH(ui, name) + 2.f, tw, LineH(ui, desc) }, fg.Fade(0.65f), d);
    };

    // Auto-Attach; a scan line runs along the tile while it waits for the game
    {
        const Rect in = Tile("tk-tile-auto", 0, M::Icon::AutoFix, nullptr, s.isAutoAttachEnabled);
        const Rect sw{ in.R() - M::kSwitchWidth, in.CY() - M::kSwitchHeight * 0.5f, M::kSwitchWidth, M::kSwitchHeight };
        Texts(in, Loc::Get("tickets_auto_attach"), Loc::Get("tickets_auto_attach_hint"), sw.x);
        if (s.isAutoAttachEnabled && !s.isAttached) {
            const Rect tile{ area.x, area.y, tileW, area.h };
            const float k = static_cast<float>(std::fmod(ui.Time(), 1.8) / 1.8);
            const float sx = tile.x + 12.f + (tile.w - 24.f - 60.f) * Anim::EaseInOutSine(k < 0.5f ? k * 2.f : 2.f - k * 2.f);
            ui.g.FillLinear({ sx, tile.B() - 3.f, 60.f, 2.f }, 0.f, { { 0.f, kAmber.Fade(0.f) }, { 0.5f, kAmber }, { 1.f, kAmber.Fade(0.f) } }, { 0.f, 0.f },
                            { 1.f, 0.f });
            ui.KeepAnimatingThrottled();
        }
        const UI::Id id = ui.MakeId("tk-autoattach");
        if (M::Switch(ui, id, sw, s.isAutoAttachEnabled)) s.ToggleAutoAttach();
        ui.Tooltip(id, Loc::Get("tickets_auto_hint"));
    }
    // Your tickets: the amount once located, Find opens the Ticket Editor
    {
        const bool located = s.IsTicketLocated();
        const Rect in = Tile("tk-tile-count", 1, M::Icon::Counter, L"Tickets.png", located);
        const std::wstring& label = Loc::Get("tickets_counter_find");
        M::ButtonLook look = RaisedOn(p.secondaryMid.color);
        look.foreground = p.secondaryMid.foreground;
        const float bw = M::ButtonWidth(ui, label, look, 16.f, 6.f);
        const Rect button{ in.R() - bw, in.CY() - 16.f, bw, 32.f };
        Texts(in, Loc::Get("tickets_counter_title"),
              located ? Loc::Fmt("tickets_counter_value", s.tickets.currentValue) : Loc::Get(s.isAttached ? "tickets_counter_not_located" : "tickets_counter_attach"),
              button.x);
        if (M::IconTextButton(ui, ui.MakeId("tk-count-find"), button, M::Icon::Magnify, label, look, true, 16.f, 6.f)) s.selectedTool = L"tickets";
    }
}

// The manual mode (only before attaching, with Auto-Attach off): the detected processes, Refresh and
// Attach to Selected
float TicketsTab::ProcessPicker(UI::Ui& ui, const Rect& area, bool draw) {
    TicketsState& s = *state;
    const M::Palette& p = M::P();
    const Color fg = p.foreground;
    constexpr float kProcH = 40.f;
    float y = area.y;

    if (draw) {
        TextStyle head = Style(12.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        head.valign = VAlign::Center;
        ui.g.Text(Loc::Get("tickets_detected_processes"), { area.x, y, area.w - 30.f, 28.f }, fg, head);
        const UI::Id refresh = ui.MakeId("tk-proc-refresh");
        if (M::IconButton(ui, refresh, { area.R() - 28.f, y, 28.f, 28.f }, M::Icon::Refresh, 16.f, M::IconButtonLook())) s.RefreshDetectedProcesses();
        ui.Tooltip(refresh, Loc::Get("tickets_refresh_processes"));
    }
    y += 28.f + 6.f;

    if (!s.HasDetectedProcesses()) {
        TextStyle none = Style(11.f, DWRITE_FONT_WEIGHT_NORMAL, true);
        none.italic = true;
        const std::wstring& text = Loc::Get("tickets_no_processes");
        const float h = TabKit::TextH(ui, text, none, area.w);
        if (draw) ui.g.Text(text, { area.x, y, area.w, h }, fg.Fade(0.6f), none);
        y += h + 6.f;
    } else {
        for (size_t i = 0; i < s.detectedProcesses.size() && i < 3; ++i) {
            if (draw) {
                const auto& proc = s.detectedProcesses[i];
                const Rect row{ area.x, y, area.w, kProcH };
                const UI::Id id = ui.MakeId("tk-proc", static_cast<int64_t>(proc.id));
                const UI::Interaction it = ui.Interact(id, row);
                const bool selected = s.selectedGameProcess == proc.id;
                if (it.hovered) ui.SetCursor(IDC_HAND);
                const float hover = M::Motion::Hover(ui, id, it.hovered, it.held).hover;
                const float sel = ui.Tween(UI::HashId(id, 0x5E1), selected ? 1.f : 0.f, 0.2f);
                ui.g.FillRound(row, 10.f, Lerp(Lerp(fg.Fade(0.04f), fg.Fade(0.08f), hover), p.primaryMid.color.Fade(0.2f), sel));
                if (sel > 0.f) ui.g.StrokeRound(row.Inset(0.75f), 9.25f, p.primaryMid.color.Fade(0.6f * sel), 1.5f);
                M::DrawIcon(ui.g, M::Icon::Memory, row.x + 20.f, row.CY(), 15.f, fg.Fade(0.7f));
                TextStyle text = Style(12.f);
                text.valign = VAlign::Center;
                text.ellipsis = true;
                ui.g.Text(proc.displayName, { row.x + 38.f, row.y, row.w - 46.f, row.h }, fg, text);
                if (it.clicked) s.selectedGameProcess = proc.id;
            }
            y += kProcH + 6.f;
        }
    }
    y += 2.f;

    // Attach to Selected (right aligned, green)
    const std::wstring& label = Loc::Get("tickets_attach_selected");
    const float w = TabKit::PillWidth(ui, label);
    if (draw && TabKit::PillButton(ui, ui.MakeId("tk-attach-selected"), { area.R() - w, y, w, 40.f }, M::Icon::Link, label, kGreen, true,
                                   s.CanAttachToSelectedProcess()))
        s.AttachToSelectedProcess();
    y += 40.f;
    return y - area.y;
}

} // namespace SonicRacingSaveManager::Main
