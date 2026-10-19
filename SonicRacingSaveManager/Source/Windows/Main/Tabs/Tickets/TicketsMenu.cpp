#include <algorithm>
#include <cmath>
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "UI/Material/Card.h"
#include "UI/Material/Motion.h"
#include "UI/Material/PackIcon.h"
#include "UI/Material/Theme.h"
#include "UI/Widgets/ScrollArea.h"
#include "Windows/Main/Tabs/TabKit.h"
#include "Windows/Main/Tabs/Tickets/TicketsTab.h"

// The menu view, laid out like the Inazuma editor's Tool Menu: the control deck under the banner,
// then a category rail that follows the scroll next to the tool tiles. The tools: the three patches
// (Freeze / Multiply / x500; Multiply and x500 lock each other) and the Ticket Editor with its guide.
namespace SonicRacingSaveManager::Main {

namespace {

namespace M = UI::Material;
namespace K = TicketsKit;
using TabKit::LineH;
using TabKit::Style;

constexpr float kRailW = 236.f;
constexpr float kItemH = 54.f, kItemGap = 4.f;
const Color kGreen = Argb(0xFF4CAF50);

} // namespace

void TicketsTab::MenuView(UI::Ui& ui, const Rect& area) {
    pickerOpen = M::Motion::Toggle(ui, ui.MakeId("tk-picker"), !state->isAutoAttachEnabled && !state->isAttached);

    // Control deck: search, quick tiles, process picker
    const Rect deck{ area.x, area.y, area.w, ControlDeck(ui, { 0.f, 0.f, area.w, 0.f }, false) };
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.08, 0, 0.f, 0.45f), 14.f);
        ControlDeck(ui, deck, true);
    }

    // Sections of tools (filtered by the search box)
    Section sections[kSections] = {
        { Loc::Get("tickets_cat_patches"), M::Icon::Flash, PatchTools(ui) },
        { Loc::Get("tickets_cat_editor"), M::Icon::Counter, EditorTools(ui) },
    };
    bool shown[kSections] = {};
    int firstShown = -1, lastShown = -1;
    for (int i = 0; i < kSections; ++i) {
        Section& section = sections[i];
        for (auto& tool : section.tools) {
            if (!search.empty() && !K::Matches(tool, search)) tool.visible = false;
            if (!tool.visible) continue;
            shown[i] = true;
            if (!tool.counted) continue;
            ++section.count;
            if (tool.active) ++section.active;
        }
        if (!shown[i]) continue;
        if (firstShown < 0) firstShown = i;
        lastShown = i;
    }

    // Scroll-spy: the section under the top of the list (the last one once the end is reached)
    int current = firstShown;
    for (int i = 0; i < kSections; ++i)
        if (shown[i] && sectionTop[static_cast<size_t>(i)] <= lastScroll + 60.f) current = i;
    if (lastMaxScroll > 0.f && lastScroll >= lastMaxScroll - 2.f) current = lastShown;

    const float top = deck.B() + 16.f;
    const Rect rail{ area.x, top, kRailW, area.B() - top };
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.16, 0, 0.f, 0.5f), 0.f, -28.f);
        Rail(ui, rail, sections, current);
    }

    // The tool list
    const Rect viewport{ rail.R() + 16.f, top, area.R() - rail.R() - 16.f, area.B() - top };
    const UI::Id scrollId = ui.MakeId("tk-scroll");
    UI::ScrollArea scroll(ui, scrollId, viewport, false, 48.f, UI::ScrollArea::Bar::Material);
    if (scrollToSection >= 0) {
        UI::ScrollArea::ScrollBy(ui, scrollId, sectionTop[static_cast<size_t>(scrollToSection)] - scroll.Offset());
        scrollToSection = -1;
    }
    lastScroll = scroll.Offset();
    const float x = viewport.x, w = scroll.ContentWidth() - 10.f;
    float y = viewport.y + 4.f;
    int index = 0, number = 0;
    const double reveal = std::max(loadedAt + 0.22, searchChangedAt);
    for (int i = 0; i < kSections; ++i) {
        Section& section = sections[i];
        if (!shown[i]) continue;
        sectionTop[static_cast<size_t>(i)] = y - viewport.y - 4.f;
        y += K::SectionTitle(ui, x, y, w, ++number, section.title, section.count, section.active, reveal + 0.06 * number);
        y += K::ToolGrid(ui, x, y, w, section.tools, reveal, index) + 16.f;
    }
    if (firstShown < 0) {
        // Empty search state: a bobbing magnifier
        const M::Palette& p = M::P();
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, searchChangedAt, 0));
        const float bob = 6.f * std::sin(static_cast<float>(ui.Time()) * 2.4f);
        ui.KeepAnimatingThrottled();
        ui.g.FillEllipse(x + w * 0.5f, y + 70.f, 46.f, 46.f, p.primaryMid.color.Fade(0.1f));
        M::DrawIcon(ui.g, M::Icon::Magnify, { x + w * 0.5f - 28.f, y + 42.f + bob, 56.f, 56.f }, p.foreground.Fade(0.35f));
        ui.g.Text(Loc::Fmt("tickets_no_match", search), { x, y + 130.f, w, 24.f }, p.foreground.Fade(0.8f),
                  Style(16.f, DWRITE_FONT_WEIGHT_SEMI_BOLD, false, HAlign::Center));
        ui.g.Text(Loc::Get("tickets_no_match_hint"), { x, y + 158.f, w, 20.f }, p.foreground.Fade(0.55f), Style(12.f, DWRITE_FONT_WEIGHT_NORMAL, false, HAlign::Center));
        y += 200.f;
    }
    scroll.SetContentSize(y - viewport.y);
    lastMaxScroll = std::max(0.f, y - viewport.y - viewport.h);
}

// Category rail: one entry per section with its icon, tool count and options on; a highlight that
// stretches and glides to the section in view; the "options on" gauge at the bottom
void TicketsTab::Rail(UI::Ui& ui, const Rect& r, const Section* sections, int current) {
    const M::Palette& p = M::P();
    const Color fg = p.foreground;
    M::CardLook look;
    look.radius = 16.f;
    M::Card card(ui, ui.MakeId("tk-rail"), r, Edges(12.f, 14.f), false, 1.f, look);
    const Rect c = card.content;

    ui.g.Text(Loc::Get("tickets_categories"), { c.x + 6.f, c.y, c.w, 16.f }, fg.Fade(0.5f), Style(10.f, DWRITE_FONT_WEIGHT_BOLD));
    const float listY = c.y + 24.f;

    // Highlight: the leading edge moves faster than the trailing one, so it stretches as it goes
    if (current >= 0) {
        const float targetTop = listY + static_cast<float>(current) * (kItemH + kItemGap);
        float& topEdge = ui.Slot(ui.MakeId("tk-rail-top"), targetTop);
        float& bottomEdge = ui.Slot(ui.MakeId("tk-rail-bottom"), targetTop + kItemH);
        const bool down = targetTop > topEdge + 0.5f;
        topEdge = Anim::Approach(topEdge, targetTop, down ? 10.f : 22.f, ui.Dt());
        bottomEdge = Anim::Approach(bottomEdge, targetTop + kItemH, down ? 22.f : 10.f, ui.Dt());
        if (std::fabs(topEdge - targetTop) > 0.3f || std::fabs(bottomEdge - targetTop - kItemH) > 0.3f) ui.KeepAnimating();
        const Rect hl{ c.x, topEdge, c.w, bottomEdge - topEdge };
        ui.g.FillRound(hl, 12.f, p.primaryMid.color.Fade(0.14f));
        ui.g.FillRound({ hl.x, hl.y + 12.f, 4.f, std::max(0.f, hl.h - 24.f) }, 2.f, p.primaryMid.color);
    }

    const TextStyle name = Style(13.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    const TextStyle info = Style(10.f);
    int total = 0, on = 0;
    for (int i = 0; i < kSections; ++i) {
        const Section& s = sections[i];
        total += s.count;
        on += s.active;
        const Rect item{ c.x, listY + static_cast<float>(i) * (kItemH + kItemGap), c.w, kItemH };
        const UI::Id id = ui.MakeId("tk-rail-item", i);
        const bool enabled = s.count > 0;
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.2, i, 0.05f, 0.4f), 0.f, -16.f);
        const UI::Interaction it = ui.Interact(id, item, enabled);
        if (it.hovered) ui.SetCursor(IDC_HAND);
        if (it.clicked) scrollToSection = i;
        const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, it.held);
        const float sel = ui.Tween(UI::HashId(id, 0x5E1), i == current ? 1.f : 0.f, 0.2f);
        if (!enabled) ui.g.PushOpacity(0.35f);
        if (fb.hover > 0.f && i != current) ui.g.FillRound(item, 12.f, fg.Fade(0.05f * fb.hover));

        const float nudge = 4.f * fb.hover - 2.f * fb.press;
        const float bcx = item.x + 14.f + 18.f + nudge, bcy = item.CY();
        const float br = 18.f * (1.f + 0.08f * fb.hover);
        ui.g.FillEllipse(bcx, bcy, br, br, Lerp(fg.Fade(0.07f), p.primaryMid.color, sel));
        ui.g.PushTransform(D2D1::Matrix3x2F::Rotation(-12.f * fb.hover, D2D1::Point2F(bcx, bcy)));
        M::DrawIcon(ui.g, s.icon, bcx, bcy, 20.f, Lerp(fg.Fade(0.8f), p.primaryMid.foreground, sel));
        ui.g.PopTransform();

        const float tx = item.x + 14.f + 36.f + 12.f + nudge;
        const float stackH = LineH(ui, name) + 2.f + LineH(ui, info);
        const float ty = item.CY() - stackH * 0.5f;
        TextStyle n = name;
        n.ellipsis = true;
        ui.g.Text(s.title, { tx, ty, item.R() - tx - 30.f, LineH(ui, name) }, Lerp(fg.Fade(0.85f), p.sectionHeader, sel), n);
        const std::wstring tools = Loc::Fmt(s.count == 1 ? "tools_count_one" : "tools_count_many", s.count);
        ui.g.Text(tools, { tx, ty + LineH(ui, name) + 2.f, 120.f, LineH(ui, info) }, fg.Fade(0.55f), info);
        if (s.active > 0) {
            // "N on" + a pulsing green dot
            const float toolsW = std::ceil(ui.g.Measure(tools, info).w);
            ui.g.Text(L"\x00B7 " + Loc::Fmt("tools_on_short", s.active), { tx + toolsW + 4.f, ty + LineH(ui, name) + 2.f, 80.f, LineH(ui, info) }, kGreen, info);
            const float t = static_cast<float>(std::fmod(ui.Time() + i * 0.3, 1.6) / 1.6);
            const float dx = item.R() - 16.f, dy = item.CY();
            ui.g.StrokeEllipse(dx, dy, 4.f + 7.f * t, 4.f + 7.f * t, kGreen.Fade(0.6f * (1.f - t)), 1.5f);
            ui.g.FillEllipse(dx, dy, 4.f, 4.f, kGreen);
            ui.KeepAnimatingThrottled();
        }
        if (!enabled) ui.g.PopLayer();
    }

    // "Options on" gauge, when there is room for it under the entries
    const float listB = listY + kSections * (kItemH + kItemGap);
    constexpr float kGaugeH = 132.f;
    if (c.B() - listB < kGaugeH + 8.f) return;
    const Rect gauge{ c.x, c.B() - kGaugeH, c.w, kGaugeH };
    ui.g.FillRect({ gauge.x + 8.f, gauge.y - 4.f, gauge.w - 16.f, 1.f }, p.divider);
    const float cx = gauge.CX(), cy = gauge.y + 52.f, radius = 34.f;
    const float fraction = ui.Animate(ui.MakeId("tk-gauge"), total > 0 ? static_cast<float>(on) / static_cast<float>(total) : 0.f, 6.f, 0.f);
    const float shownOn = ui.Animate(ui.MakeId("tk-gauge-n"), static_cast<float>(on), 8.f, 0.f);
    if (std::fabs(shownOn - static_cast<float>(on)) > 0.01f) ui.KeepAnimating();
    ui.g.StrokeEllipse(cx, cy, radius, radius, fg.Fade(0.08f), 7.f);
    TabKit::Arc(ui.g, cx, cy, radius, 0.f, std::max(0.f, 360.f * fraction), p.primaryMid.color, 7.f);
    if (on > 0) {
        // A spark circling the ring
        const float a = static_cast<float>(std::fmod(ui.Time() * 1.4, 2.0 * 3.14159265));
        ui.g.FillEllipse(cx + (radius + 9.f) * std::sin(a), cy - (radius + 9.f) * std::cos(a), 2.5f, 2.5f, p.secondaryMid.color);
        ui.KeepAnimatingThrottled();
    }
    TextStyle number = Style(24.f, DWRITE_FONT_WEIGHT_BOLD, false, HAlign::Center);
    number.valign = VAlign::Center;
    ui.g.Text(std::to_wstring(static_cast<int>(std::lround(shownOn))), { cx - radius, cy - 16.f, radius * 2.f, 32.f }, fg, number);
    ui.g.Text(Loc::Get(on == 1 ? "tools_option_on" : "tools_options_on"), { gauge.x, cy + radius + 12.f, gauge.w, 16.f }, fg.Fade(0.65f),
              Style(11.f, DWRITE_FONT_WEIGHT_SEMI_BOLD, false, HAlign::Center));
    ui.g.Text(Loc::Fmt("tools_of_total", total), { gauge.x, cy + radius + 28.f, gauge.w, 14.f }, fg.Fade(0.45f), Style(10.f, DWRITE_FONT_WEIGHT_NORMAL, false, HAlign::Center));
}

// The patches (the C#'s green Freeze, orange Multiply and purple x500 banners). The switches need the
// game; Multiply and x500 patch the same spot, so each one locks while the other is on.
std::vector<K::ToolSpec> TicketsTab::PatchTools(UI::Ui& ui) {
    TicketsState& s = *state;
    std::vector<K::ToolSpec> tools;
    const auto Patch = [&](const char* key, M::Icon icon, const char* title, const char* hint, const char* tooltip, bool on, bool canToggle,
                           const char* lockedNote, std::function<void()> toggle, const wchar_t* words) {
        const bool locked = s.isAttached && !canToggle && lockedNote;
        K::ToolSpec t;
        t.key = key;
        t.icon = icon;
        t.title = Loc::Get(title);
        t.subtitle = locked ? Loc::Get(lockedNote) : Loc::Get(hint);
        t.active = on;
        t.search = words;
        t.trailing = K::SwitchRow(ui, ui.MakeId(std::string(key) + "-switch"), on, canToggle, std::move(toggle), locked,
                                  Loc::Get(s.isAttached ? tooltip : "tickets_patches_attach_first"));
        tools.push_back(std::move(t));
    };
    Patch("tk-freeze", M::Icon::Snowflake, "tickets_freeze", "tickets_freeze_hint", "tickets_freeze_tooltip", s.IsTicketsFrozen(), s.CanToggleFreeze(), nullptr,
          [&s] { s.ToggleTicketsFreeze(); }, L"freeze unlimited");
    Patch("tk-multiply", M::Icon::TrendingUp, "tickets_multiply", "tickets_multiply_hint", "tickets_multiply_tooltip", s.IsTicketsMultiplied(),
          s.CanToggleMultiply(), "tickets_multiply_locked", [&s] { s.ToggleTicketsMultiply(); }, L"multiply");
    Patch("tk-x500", M::Icon::Rocket, "tickets_x500", "tickets_x500_hint", "tickets_x500_tooltip", s.IsTicketsMultiply500(), s.CanToggleMultiply500(),
          "tickets_x500_locked", [&s] { s.ToggleTicketsMultiply500(); }, L"multiply 500");
    return tools;
}

// The Ticket Editor tile (TicketEditorCard_Click: the whole tile opens it) and the how-to guide
std::vector<K::ToolSpec> TicketsTab::EditorTools(UI::Ui& ui) {
    TicketsState& s = *state;
    std::vector<K::ToolSpec> tools;
    {
        K::ToolSpec t;
        t.key = "tk-editor";
        t.image = L"Tickets.png";
        t.title = Loc::Get("tickets_editor_tile");
        t.subtitle = s.IsTicketLocated() ? Loc::Fmt("tickets_counter_value", s.tickets.currentValue) : Loc::Get("tickets_editor_tile_hint");
        t.clickable = true;
        t.onClick = [&s] { s.selectedTool = L"tickets"; };
        t.trailing = K::OpenButtonRow(ui, ui.MakeId("tk-editor-open"), Loc::Get("tickets_open"), [&s] { s.selectedTool = L"tickets"; });
        t.search = L"find search edit value";
        tools.push_back(std::move(t));
    }
    {
        K::ToolSpec t;
        t.key = "tk-guide";
        t.icon = M::Icon::InformationOutline;
        t.title = Loc::Get("tickets_guide_title");
        t.subtitle = Loc::Get("tickets_guide_subtitle");
        t.counted = false;
        t.wide = true;
        t.details = [&ui](float width) {
            K::Rows rows;
            const char* steps[] = { "tickets_how_step1", "tickets_how_step2", "tickets_how_step3" };
            const TextStyle text = Style(12.f, DWRITE_FONT_WEIGHT_NORMAL, true);
            for (int i = 0; i < 3; ++i) {
                const std::wstring line = Loc::Get(steps[i]);
                const float textW = width - 32.f;
                K::Row row;
                row.height = std::max(22.f, TabKit::TextH(ui, line, text, textW));
                row.margin = Edges{ 0.f, i == 0 ? 0.f : 6.f, 0.f, 0.f };
                row.draw = [&ui, line, text, i, textW](const Rect& r) {
                    // A numbered dot, then the step
                    const M::Palette& p = M::P();
                    ui.g.FillEllipse(r.x + 11.f, r.y + 11.f, 11.f, 11.f, p.primaryMid.color.Fade(0.85f));
                    TextStyle n = Style(11.f, DWRITE_FONT_WEIGHT_BOLD, false, HAlign::Center);
                    n.valign = VAlign::Center;
                    ui.g.Text(std::to_wstring(i + 1), { r.x, r.y, 22.f, 22.f }, p.primaryMid.foreground, n);
                    ui.g.Text(line, { r.x + 32.f, r.y + 3.f, textW, r.h }, p.foreground.Fade(0.8f), text);
                };
                rows.push_back(std::move(row));
            }
            return rows;
        };
        tools.push_back(std::move(t));
    }
    return tools;
}

} // namespace SonicRacingSaveManager::Main
