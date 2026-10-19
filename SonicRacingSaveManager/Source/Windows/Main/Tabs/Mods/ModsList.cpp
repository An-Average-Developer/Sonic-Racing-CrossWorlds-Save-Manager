#include <algorithm>
#include <cmath>
#include <functional>
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "UI/Material/Elevation.h"
#include "UI/Material/Motion.h"
#include "UI/Material/PackIcon.h"
#include "UI/Material/Theme.h"
#include "UI/Widgets/ScrollArea.h"
#include "Windows/Main/Tabs/Backup/BackupKit.h"
#include "Windows/Main/Tabs/Mods/ModsTab.h"
#include "Windows/Main/Tabs/TabKit.h"

// The mods list: one card per mod (the DataGrid's #, Status, Mod Name, Version, Update, Files and
// Total Size columns), selectable; in priority mode a row can be dragged to a new position (the
// others make room) and dropped there (HandleDrop).
namespace SonicRacingSaveManager::Main {

namespace {

namespace M = UI::Material;
using TabKit::Style;

constexpr float kRowH = 72.f, kRowGap = 8.f, kDragThreshold = 6.f;

// A coloured pill with white text (the Status / Update cells)
void Pill(UI::Ui& ui, const Rect& r, const std::wstring& text, Color color) {
    ui.g.FillRound(r, r.h * 0.5f, color);
    TextStyle t = Style(11.f, DWRITE_FONT_WEIGHT_MEDIUM, false, HAlign::Center);
    t.valign = VAlign::Center;
    ui.g.Text(text, r, Colors::White, t);
}

float PillW(UI::Ui& ui, const std::wstring& text) {
    TextStyle t = Style(11.f, DWRITE_FONT_WEIGHT_MEDIUM);
    return std::ceil(ui.g.Measure(text, t).w) + 22.f;
}

} // namespace

// Draws mod `index` in `r`; `dragLift` (0..1) is the lift of the row being dragged. True when the
// row was clicked (not dragged).
bool ModsTab::ModRow(UI::Ui& ui, const Rect& r, int index, float dragLift) {
    ModsState& s = *state;
    const M::Palette& p = M::P();
    const Color fg = p.foreground;
    const Models::ModInfo& mod = s.Mods()[static_cast<size_t>(index)];
    const bool selected = mod.name == s.selectedMod;
    const UI::Id id = ui.MakeId("md-row", static_cast<int64_t>(std::hash<std::wstring>{}(mod.name)));
    const UI::Interaction it = ui.Interact(id, r);
    if (it.hovered) ui.SetCursor(s.CanDrag() ? IDC_SIZEALL : IDC_HAND);
    const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered || dragLift > 0.f, it.held);
    const float sel = ui.Tween(UI::HashId(id, 0x5E1), selected ? 1.f : 0.f, 0.2f);
    const float shift = 5.f * Anim::EaseOutCubic(sel) * (1.f - dragLift);
    const Rect rr{ r.x + shift, r.y, r.w - shift, r.h };
    const Color accent = p.primaryMid.color;
    if (dragLift > 0.f) {
        M::Elevation(ui.g, rr, 12.f, 8.f * dragLift);
        ui.g.FillRound(rr, 12.f, p.cardBackground);
    }
    if (sel > 0.f) ui.g.GlowRound(rr, 12.f, accent, 12.f, 0.35f * sel);
    ui.g.FillRound(rr, 12.f, Lerp(Lerp(fg.Fade(0.04f), fg.Fade(0.08f), fb.hover), accent.Fade(0.2f), sel));
    if (sel > 0.f) ui.g.StrokeRound(rr.Inset(0.75f), 11.25f, accent.Fade(0.55f * sel), 1.5f);

    float x = rr.x + 14.f;
    // # (PriorityDisplay), in a circle, or the drag grip in priority mode
    {
        const bool has = mod.HasPriority();
        ui.g.FillEllipse(x + 18.f, rr.CY(), 18.f, 18.f, has ? accent.Fade(0.2f) : fg.Fade(0.06f));
        TextStyle num = Style(13.f, DWRITE_FONT_WEIGHT_BOLD, false, HAlign::Center);
        num.valign = VAlign::Center;
        ui.g.Text(mod.PriorityDisplay(), { x, rr.y, 36.f, rr.h }, has ? TabKit::Readable(accent) : fg.Fade(0.5f), num);
        x += 50.f;
    }
    // Status (Enabled #4CAF50 / Disabled #FF6B00)
    {
        const std::wstring status = mod.StatusText();
        const float w = std::max(84.f, PillW(ui, status));
        Pill(ui, { x, rr.CY() - 13.f, w, 26.f }, status, mod.StatusColor());
        x += w + 14.f;
    }
    // Update (the button of the Update column: enabled with an update, opens the update card)
    const std::wstring updateText = mod.UpdateStatusText();
    const float uw = std::max(110.f, PillW(ui, updateText) + 8.f);
    const Rect ur{ rr.R() - 14.f - uw - (s.CanDrag() ? 30.f : 0.f), rr.CY() - 15.f, uw, 30.f };
    bool updateClicked = false;
    {
        const UI::Id uid = UI::HashId(id, 0x0B);
        const bool enabled = mod.hasUpdate && s.CanShowUpdateDialog();
        const UI::Interaction ui2 = ui.Interact(uid, ur, enabled);
        if (ui2.hovered && enabled) ui.SetCursor(IDC_HAND);
        const float h2 = M::Motion::Hover(ui, uid, ui2.hovered && enabled, ui2.held).hover;
        const Color c = mod.UpdateStatusColor();
        if (h2 > 0.f) ui.g.GlowRound(ur, 15.f, c, 10.f, 0.5f * h2);
        if (mod.hasUpdate) {
            // The update pill breathes
            const float k = 0.5f + 0.5f * std::sin(static_cast<float>(ui.Time()) * 3.f);
            ui.g.GlowRound(ur, 15.f, c, 8.f, 0.25f * k);
            ui.KeepAnimatingThrottled();
        }
        Pill(ui, ur, updateText, Lerp(c, Colors::White, 0.12f * h2));
        if (enabled) ui.Tooltip(uid, Loc::Get("mods_update_tooltip"));
        updateClicked = ui2.clicked && enabled;
    }
    // Mod Name + version, files and size
    {
        const float w = std::max(0.f, ur.x - 14.f - x);
        TextStyle name = Style(14.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        name.valign = VAlign::Center;
        name.ellipsis = true;
        TextStyle sub = Style(11.5f);
        sub.valign = VAlign::Center;
        sub.ellipsis = true;
        ui.g.Text(mod.name, { x, rr.y + 13.f, w, 24.f }, fg, name);
        const std::wstring details = Loc::Fmt("mods_row_details", mod.VersionDisplay(), BackupKit::FilesText(mod.fileCount), mod.FileSizeFormatted());
        ui.g.Text(details, { x, rr.y + 37.f, w, 20.f }, fg.Fade(0.6f), sub);
    }
    // The grip of a draggable row
    if (s.CanDrag()) M::DrawIcon(ui.g, M::Icon::DragVertical, rr.R() - 24.f, rr.CY(), 20.f, fg.Fade(0.4f + 0.4f * fb.hover));

    if (updateClicked) {
        s.ShowUpdateDialog(mod.name);
        return false;
    }
    return it.clicked;
}

void ModsTab::List(UI::Ui& ui, const Rect& r) {
    ModsState& s = *state;
    const int count = static_cast<int>(s.Mods().size());
    if (count == 0) {
        if (s.isLoading) BackupKit::EmptyState(ui, r, M::Icon::Refresh, Loc::Get("mods_status_scanning"), L"");
        else
            BackupKit::EmptyState(ui, r, M::Icon::Puzzle, Loc::Get(s.ModsDirectoryExists() ? "mods_status_none" : "mods_status_no_directory"),
                                  Loc::Get(s.ModsDirectoryExists() ? "mods_empty_hint" : "mods_no_directory_hint"));
        return;
    }
    const double reveal = BackupKit::ListReveal(ui, "md-reveal", s.revision, loadedAt + 0.2);
    const UI::Id scrollId = ui.MakeId("md-list");
    UI::ScrollArea scroll(ui, scrollId, r, false, 48.f, UI::ScrollArea::Bar::Material);
    const float w = scroll.ContentWidth() - (scroll.BarShown() ? 10.f : 4.f);
    const float step = kRowH + kRowGap;
    const float mx = ui.MouseX(), my = ui.MouseY();
    const UI::Input& in = ui.In();

    // A drag ends (drop) or is abandoned when it's no longer possible
    if (pressedIndex >= 0 && (!in.down[0] || !s.CanDrag() || pressedIndex >= count)) {
        if (dragging && in.released[0] && s.CanDrag() && pressedIndex < count) s.HandleDrop(pressedIndex, dropIndex);
        pressedIndex = -1;
        dragging = false;
    }
    // Starts once the mouse moved a few pixels from where it went down
    if (pressedIndex >= 0 && !dragging && (std::fabs(mx - pressX) > kDragThreshold || std::fabs(my - pressY) > kDragThreshold)) {
        dragging = true;
        dragOffsetY = pressY - (r.y + step * static_cast<float>(pressedIndex));
    }
    if (dragging) {
        dropIndex = std::clamp(static_cast<int>(std::lround((my - dragOffsetY - r.y) / step)), 0, count - 1);
        ui.SetCursor(IDC_SIZEALL);
        ui.KeepAnimating();
    }

    for (int i = 0; i < count; ++i) {
        if (dragging && i == pressedIndex) continue;   // drawn last, under the mouse
        // The rows between the dragged one and its drop slot move one place to make room
        int slot = i;
        if (dragging) {
            if (pressedIndex < dropIndex && i > pressedIndex && i <= dropIndex) slot = i - 1;
            else if (pressedIndex > dropIndex && i >= dropIndex && i < pressedIndex) slot = i + 1;
        }
        const float y = ui.Animate(ui.MakeId("md-row-y", static_cast<int64_t>(std::hash<std::wstring>{}(s.Mods()[static_cast<size_t>(i)].name))),
                                   r.y + step * static_cast<float>(slot), 18.f);
        if (std::fabs(y - (r.y + step * static_cast<float>(slot))) > 0.5f) ui.KeepAnimating();
        if (y + kRowH < r.y + scroll.Offset() - 20.f || y > r.B() + scroll.Offset() + 20.f) continue;
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, reveal, i, 0.04f, 0.35f), 0.f, -20.f);
        const Rect row{ r.x, y, w, kRowH };
        // A press on a draggable row may become a drag
        if (s.CanDrag() && !dragging && pressedIndex < 0 && in.pressed[0] && ui.MouseOver(row)) {
            pressedIndex = i;
            pressX = mx;
            pressY = my;
        }
        if (ModRow(ui, row, i, 0.f) && !dragging) s.selectedMod = s.Mods()[static_cast<size_t>(i)].name;
    }
    if (dragging && pressedIndex >= 0 && pressedIndex < count) {
        // The dragged row follows the mouse, lifted, and the drop slot shows as a dashed outline
        const Rect slotR{ r.x, r.y + step * static_cast<float>(dropIndex), w, kRowH };
        ui.g.StrokeRound(slotR.Inset(1.f), 11.f, M::P().primaryMid.color.Fade(0.6f), 2.f);
        const Rect row{ r.x, my - dragOffsetY, w, kRowH };
        ui.PushScale(1.02f, row.CX(), row.CY());
        ModRow(ui, row, pressedIndex, 1.f);
        ui.PopTransform();
    }
    scroll.SetContentSize(count * step - kRowGap);
}

} // namespace SonicRacingSaveManager::Main
