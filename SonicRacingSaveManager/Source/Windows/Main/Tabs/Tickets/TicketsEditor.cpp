#include <algorithm>
#include <cmath>
#include <cwctype>
#include <optional>
#include "App/Paths.h"
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "UI/Material/Card.h"
#include "UI/Material/Feedback.h"
#include "UI/Material/Fields.h"
#include "UI/Material/Motion.h"
#include "UI/Material/PackIcon.h"
#include "UI/Material/Theme.h"
#include "UI/Material/Toggles.h"
#include "Windows/Main/Tabs/TabKit.h"
#include "Windows/Main/Tabs/Tickets/TicketsTab.h"

// The Ticket Editor view (SelectedTool "tickets"): Back to Menu, Game Values (Auto-refresh, the guided
// Find Tickets, the Tickets entry, Refresh Now) on the left and Edit Value on the right.
namespace SonicRacingSaveManager::Main {

namespace {

namespace M = UI::Material;
using TabKit::Style;
using TabKit::TextH;

constexpr float kPad = 20.f;
const Color kPink = Argb(0xFFE91E63), kGreen = Argb(0xFF4CAF50);

// The int binding of the C#'s text boxes (Int32.Parse, NumberStyles.Integer): surrounding spaces and
// a leading sign; anything else leaves the bound value as it was
std::optional<int32_t> ParseInt32(const std::wstring& text) {
    size_t a = 0, b = text.size();
    while (a < b && std::iswspace(text[a])) ++a;
    while (b > a && std::iswspace(text[b - 1])) --b;
    if (a == b) return std::nullopt;
    bool negative = false;
    if (text[a] == L'+' || text[a] == L'-') negative = text[a++] == L'-';
    if (a == b) return std::nullopt;
    long long value = 0;
    for (size_t i = a; i < b; ++i) {
        if (!std::iswdigit(text[i])) return std::nullopt;
        value = value * 10 + (text[i] - L'0');
        if (value > 2147483648LL) return std::nullopt;
    }
    if (negative) value = -value;
    if (value > INT32_MAX || value < INT32_MIN) return std::nullopt;
    return static_cast<int32_t>(value);
}

TextStyle Caps() {
    TextStyle t = Style(11.f, DWRITE_FONT_WEIGHT_BOLD);
    t.valign = VAlign::Center;
    return t;
}

} // namespace

void TicketsTab::FindTicketsCard(UI::Ui& ui, const Rect& r) {
    TicketsState& s = *state;
    const M::Palette& p = M::P();
    const Color fg = p.foreground;
    const float t = static_cast<float>(ui.Time());
    M::CardLook look;
    look.radius = 16.f;
    M::Card card(ui, ui.MakeId("tk-find"), r, Edges(24.f, kPad), false, 1.f, look);
    const Rect c = card.content;
    float y = c.y;

    // < Back to Menu
    {
        const std::wstring& label = Loc::Get("tickets_back_to_menu");
        if (TabKit::PillButton(ui, ui.MakeId("tk-back"), { c.x, y, TabKit::PillWidth(ui, label) - 16.f, 36.f }, M::Icon::ChevronLeft, label,
                               p.primaryMid.color, false))
            s.selectedTool = L"menu";
        y += 36.f + 18.f;
    }
    // Game Values + Auto-refresh (1s)
    ui.g.Text(Loc::Get("tickets_game_values"), { c.x, y, c.w, 20.f }, p.primaryMid.color, Caps());
    const std::wstring& refresh = Loc::Get("tickets_auto_refresh");
    const float cbW = M::CheckBoxWidth(ui, refresh, Style(12.f));
    if (M::CheckBox(ui, ui.MakeId("tk-autorefresh"), { c.R() - cbW, y + 1.f, cbW, 18.f }, s.autoRefresh, refresh, Style(12.f), fg, s.isAttached))
        s.SetAutoRefresh(!s.autoRefresh);
    y += 30.f;

    // ── Find Tickets ──
    const float inputH = M::FieldHeight(ui, 13.f);
    const std::wstring stepText = s.TicketStepText();
    const TextStyle stepStyle = Style(13.f, DWRITE_FONT_WEIGHT_NORMAL, true);
    const TextStyle hintStyle = Style(11.f, DWRITE_FONT_WEIGHT_NORMAL, true);
    const float boxW = c.w - 36.f;
    const float stepH = TextH(ui, stepText, stepStyle, boxW);
    const float hintH = s.ticketSearchHint.empty() ? 0.f : TextH(ui, s.ticketSearchHint, hintStyle, boxW) + 8.f;
    const float actionH = s.IsTicketNextStep() ? 40.f : s.IsTicketSearchInputStep() ? std::max(inputH, 40.f) : 0.f;
    const float boxH = 18.f + 22.f + 16.f + 14.f + stepH + 12.f + actionH + (s.isTicketSearching ? 12.f : 0.f) + hintH + 44.f;
    const Rect box{ c.x, y, c.w, boxH };
    ui.g.FillRound(box, 14.f, fg.Fade(0.05f));
    ui.g.StrokeRound(box.Inset(0.5f), 13.5f, p.primaryMid.color.Fade(0.25f), 1.f);
    {
        const Rect in = box.Inset(18.f);
        float by = in.y;
        TextStyle head = Style(15.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        head.valign = VAlign::Center;
        M::DrawIcon(ui.g, M::Icon::Magnify, in.x + 9.f, by + 11.f, 18.f, TabKit::Readable(p.primaryMid.color));
        ui.g.Text(Loc::Get("tickets_find"), { in.x + 26.f, by, in.w, 22.f }, fg, head);
        TextStyle step = Style(11.f, DWRITE_FONT_WEIGHT_NORMAL, false, HAlign::Right);
        step.valign = VAlign::Center;
        ui.g.Text(s.TicketStepTitle(), { in.x, by, in.w, 22.f }, fg.Fade(0.6f), step);
        by += 22.f + 16.f;

        // Five step dots joined by a line; the current one pulses, the done ones are filled
        const float dotsW = in.w;
        const float gap = dotsW / 4.f;
        ui.g.FillRect({ in.x, by + 6.f, dotsW, 2.f }, fg.Fade(0.1f));
        const float done = ui.Animate(ui.MakeId("tk-steps-progress"), static_cast<float>(std::min(s.ticketSearchStep - 1, 4)) / 4.f, 8.f);
        ui.g.FillRect({ in.x, by + 6.f, dotsW * done, 2.f }, p.primaryMid.color);
        for (int i = 0; i < 5; ++i) {
            const float dx = in.x + gap * static_cast<float>(i);
            const bool reached = i + 1 < s.ticketSearchStep || s.ticketSearchStep == 6;
            const bool current = i + 1 == s.ticketSearchStep;
            const float r2 = current ? 7.f + 1.5f * std::sin(t * 5.f) : 6.f;
            if (current) ui.KeepAnimatingThrottled();
            ui.g.FillEllipse(dx, by + 7.f, r2, r2, reached || current ? p.primaryMid.color : p.background);
            ui.g.StrokeEllipse(dx, by + 7.f, r2, r2, reached || current ? p.primaryMid.color : fg.Fade(0.3f), 1.5f);
            if (reached) M::DrawIcon(ui.g, M::Icon::Check, dx, by + 7.f, 9.f, p.primaryMid.foreground);
        }
        by += 14.f + 14.f;

        // The step's instructions slide in whenever the step changes
        struct Shown {
            int step = 0;
            double at = -10.0;
        };
        Shown& shown = ui.Data<Shown>(ui.MakeId("tk-step-shown"));
        if (shown.step != s.ticketSearchStep) {
            shown.step = s.ticketSearchStep;
            shown.at = ui.Time();
        }
        const float k = Anim::EaseOutCubic(Anim::Clamp01(static_cast<float>((ui.Time() - shown.at) / 0.35)));
        if (k < 1.f) ui.KeepAnimating();
        {
            M::Motion::Enter enter(ui, k, 0.f, 18.f);
            ui.g.Text(stepText, { in.x, by, in.w, stepH }, s.ticketSearchStep == 6 ? TabKit::Readable(kGreen) : fg, stepStyle);
        }
        by += stepH + 12.f;

        if (s.IsTicketNextStep()) {
            // Steps 1, 3, 4: change menu in-game, then Next
            const std::wstring& next = Loc::Get("tickets_next");
            const float w = TabKit::PillWidth(ui, next);
            if (TabKit::PillButton(ui, ui.MakeId("tk-next"), { in.R() - w, by, w, 40.f }, M::Icon::ChevronRight, next, p.primaryMid.color, true))
                s.NextTicketStep();
            by += 40.f;
        } else if (s.IsTicketSearchInputStep()) {
            // Steps 2, 5: type the ticket amount and Search
            const std::wstring& searchLabel = Loc::Get("tickets_search");
            const float sw = TabKit::PillWidth(ui, searchLabel);
            M::FieldStyle field;
            field.outlined = true;
            field.fontSize = 13.f;
            field.hint = Loc::Get("tickets_search_hint");
            const float fy = by + (std::max(inputH, 40.f) - inputH) * 0.5f;
            if (M::TextField(ui, ui.MakeId("tk-search-value"), { in.x, fy, in.w - sw - 10.f, inputH }, searchText, field))
                if (const auto v = ParseInt32(searchText)) s.ticketSearchValue = *v;
            if (TabKit::PillButton(ui, ui.MakeId("tk-search"), { in.R() - sw, by, sw, 40.f }, M::Icon::Magnify, searchLabel, p.primaryMid.color, true,
                                   s.CanSearchTickets()))
                s.SearchTickets();
            by += std::max(inputH, 40.f);
        }
        if (s.isTicketSearching) {
            M::ProgressBar(ui, { in.x, by + 8.f, in.w, 4.f }, 0.f, true);
            by += 12.f;
        }
        if (!s.ticketSearchHint.empty()) {
            ui.g.Text(s.ticketSearchHint, { in.x, by + 8.f, in.w, hintH - 8.f }, fg.Fade(0.7f), hintStyle);
            by += hintH;
        }
        // New Search (flat)
        TextStyle link = Style(12.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        link.valign = VAlign::Center;
        const std::wstring& again = Loc::Get("tickets_new_search");
        const float lw = std::ceil(ui.g.Measure(again, link).w) + 28.f;
        const Rect lr{ in.x - 4.f, by + 6.f, lw, 28.f };
        const UI::Id lid = ui.MakeId("tk-new-search");
        const bool canNew = s.CanNewTicketSearch();
        const UI::Interaction it = ui.Interact(lid, lr, canNew);
        if (it.hovered && canNew) ui.SetCursor(IDC_HAND);
        const float hover = M::Motion::Hover(ui, lid, it.hovered && canNew, it.held).hover;
        ui.g.FillRound(lr, 8.f, p.primaryMid.color.Fade(0.12f * hover));
        const Color linkColor = canNew ? TabKit::Readable(p.primaryMid.color) : fg.Fade(0.35f);
        M::DrawIcon(ui.g, M::Icon::Refresh, lr.x + 14.f, lr.CY(), 14.f, linkColor);
        ui.g.Text(again, { lr.x + 26.f, lr.y, lw, lr.h }, linkColor, link);
        if (it.clicked && canNew) s.ResetTicketSearch();
    }
    y = box.B() + 16.f;

    // ── The values list: the Tickets entry (image + pink count badge), selectable ──
    constexpr float kRefreshH = 40.f;
    const float rowH = std::min(80.f, std::max(0.f, c.B() - kRefreshH - 14.f - y));
    if (rowH >= 50.f) {
        const Rect row{ c.x, y, c.w, rowH };
        const UI::Id id = ui.MakeId("tk-value-tickets");
        const UI::Interaction it = ui.Interact(id, row);
        if (it.hovered) ui.SetCursor(IDC_HAND);
        const float hover = M::Motion::Hover(ui, id, it.hovered, it.held).hover;
        const float sel = ui.Tween(UI::HashId(id, 0x5E1), s.valueSelected ? 1.f : 0.f, 0.2f);
        const float shift = 5.f * Anim::EaseOutCubic(sel);   // the selected ListBoxItem slides 5 px right
        const Rect rr{ row.x + shift, row.y, row.w - shift, row.h };
        if (sel > 0.f) ui.g.GlowRound(rr, 12.f, p.primaryMid.color, 12.f, 0.35f * sel);
        ui.g.FillRound(rr, 12.f, Lerp(Lerp(fg.Fade(0.04f), fg.Fade(0.08f), hover), p.primaryMid.color.Fade(0.2f), sel));
        if (const ImagePtr ticket = Images::Load(Paths::Asset(L"Tickets.png")))
            ui.g.DrawImage(*ticket, { rr.x + 10.f, rr.CY() - (rowH - 16.f) * 0.5f, rowH - 16.f, rowH - 16.f }, 1.f);
        TextStyle count = Style(16.f, DWRITE_FONT_WEIGHT_BOLD);
        count.valign = VAlign::Center;
        const std::wstring value = std::to_wstring(s.tickets.currentValue);
        const float bw = std::ceil(ui.g.Measure(value, count).w) + 44.f;
        const Rect badge{ rr.x + rowH + 4.f, rr.CY() - 15.f, bw, 30.f };
        ui.g.FillRound(badge, 15.f, kPink);
        M::DrawIcon(ui.g, M::Icon::Counter, badge.x + 18.f, badge.CY(), 16.f, Colors::White);
        ui.g.Text(value, { badge.x + 30.f, badge.y, bw - 30.f, badge.h }, Colors::White, count);
        TextStyle name = Style(12.f);
        name.valign = VAlign::Center;
        ui.g.Text(Loc::Get(s.IsTicketLocated() ? "tickets_value_located" : "tickets_value_not_located"), { badge.R() + 14.f, rr.y, rr.R() - badge.R() - 50.f, rr.h },
                  fg.Fade(0.6f), name);
        if (sel > 0.f) M::DrawIcon(ui.g, M::Icon::CheckCircle, rr.R() - 24.f, rr.CY(), 22.f * Anim::EaseOutBack(sel, 1.2f), p.primaryMid.color);
        if (it.clicked) s.valueSelected = true;
    }
    // Refresh Now (outlined)
    {
        const std::wstring& label = Loc::Get("tickets_refresh_now");
        if (TabKit::PillButton(ui, ui.MakeId("tk-refresh-now"), { c.x, c.B() - kRefreshH, c.w, kRefreshH }, M::Icon::Refresh, label, p.primaryMid.color, false,
                               s.isAttached))
            s.RefreshValues();
    }
}

void TicketsTab::EditValueCard(UI::Ui& ui, const Rect& r) {
    TicketsState& s = *state;
    const M::Palette& p = M::P();
    const Color fg = p.foreground;
    M::CardLook look;
    look.radius = 16.f;
    M::Card card(ui, ui.MakeId("tk-edit"), r, Edges(kPad), false, 1.f, look);
    const Rect c = card.content;

    if (!s.valueSelected) {
        // No Selection Placeholder
        const float t = static_cast<float>(ui.Time());
        const float cy = c.CY() - 40.f;
        M::DrawIcon(ui.g, M::Icon::CursorPointer, c.CX() + 4.f * std::sin(t * 1.5f), cy + 4.f * std::cos(t * 1.5f), 64.f, fg.Fade(0.3f));
        ui.KeepAnimatingThrottled();
        TextStyle head = Style(20.f, DWRITE_FONT_WEIGHT_MEDIUM, false, HAlign::Center);
        ui.g.Text(Loc::Get("tickets_select_value"), { c.x, cy + 52.f, c.w, 28.f }, fg.Fade(0.5f), head);
        TextStyle body = Style(14.f, DWRITE_FONT_WEIGHT_NORMAL, true, HAlign::Center);
        ui.g.Text(Loc::Get("tickets_select_value_hint"), { c.x + 10.f, cy + 88.f, c.w - 20.f, 40.f }, fg.Fade(0.4f), body);
        return;
    }

    float y = c.y;
    ui.g.Text(Loc::Get("tickets_edit_value"), { c.x, y, c.w, 20.f }, p.primaryMid.color, Caps());
    y += 32.f;
    // Current Value: big, in the primary colour, bumping whenever it changes
    {
        const Rect box{ c.x, y, c.w, 84.f };
        ui.g.FillRound(box, 14.f, fg.Fade(0.05f));
        TextStyle label = Style(11.f);
        label.valign = VAlign::Center;
        ui.g.Text(Loc::Get("tickets_current_value"), { box.x + 16.f, box.y + 10.f, box.w - 32.f, 18.f }, fg.Fade(0.7f), label);
        const float bump = M::Motion::Pulse(ui, UI::HashId(ui.MakeId("tk-current"), static_cast<int64_t>(s.tickets.currentValue)), true, 0.5f);
        TextStyle big = Style(30.f, DWRITE_FONT_WEIGHT_BOLD);
        big.valign = VAlign::Center;
        ui.g.PushTransform(D2D1::Matrix3x2F::Scale(1.f + 0.08f * bump, 1.f + 0.08f * bump, D2D1::Point2F(box.x + 16.f, box.y + 52.f)));
        ui.g.Text(std::to_wstring(s.tickets.currentValue), { box.x + 16.f, box.y + 32.f, box.w - 32.f, 40.f }, TabKit::Readable(p.primaryMid.color), big);
        ui.g.PopTransform();
        y += 84.f + 18.f;
    }
    // New Value
    {
        if (s.tickets.newValue != syncedNewValue || newValueText.empty()) {
            syncedNewValue = s.tickets.newValue;
            newValueText = std::to_wstring(s.tickets.newValue);
        }
        TextStyle label = Style(11.f);
        label.valign = VAlign::Center;
        ui.g.Text(Loc::Get("tickets_new_value"), { c.x, y, c.w, 18.f }, fg.Fade(0.7f), label);
        y += 22.f;
        M::FieldStyle field;
        field.outlined = true;
        field.fontSize = 16.f;
        const float h = M::FieldHeight(ui, field);
        if (M::TextField(ui, ui.MakeId("tk-new-value"), { c.x, y, c.w, h }, newValueText, field)) {
            if (const auto v = ParseInt32(newValueText)) {
                s.tickets.newValue = *v;
                syncedNewValue = *v;
            }
        }
        y += h + 18.f;
    }
    // Apply New Value
    if (TabKit::PillButton(ui, ui.MakeId("tk-apply"), { c.x, y, c.w, 46.f }, M::Icon::Check, Loc::Get("tickets_apply"), p.primaryMid.color, true,
                           s.CanApplyValue()))
        s.ApplyValue();
    y += 46.f + 14.f;
    if (!s.IsTicketLocated()) {
        TextStyle note = Style(12.f, DWRITE_FONT_WEIGHT_NORMAL, true);
        const float nh = TextH(ui, Loc::Get("tickets_locate_first"), note, c.w - 34.f);
        const Rect nr{ c.x, y, c.w, nh + 20.f };
        ui.g.FillRound(nr, 12.f, p.primaryMid.color.Fade(0.08f));
        M::DrawIcon(ui.g, M::Icon::Information, nr.x + 16.f, nr.y + 18.f, 16.f, p.primaryMid.color);
        ui.g.Text(Loc::Get("tickets_locate_first"), { nr.x + 32.f, nr.y + 10.f, nr.w - 40.f, nh }, fg.Fade(0.75f), note);
    }
}

} // namespace SonicRacingSaveManager::Main
