#include <cmath>
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "UI/Material/Card.h"
#include "UI/Material/Motion.h"
#include "UI/Material/PackIcon.h"
#include "UI/Material/Theme.h"
#include "Windows/Main/Tabs/Backup/BackupKit.h"
#include "Windows/Main/Tabs/Mods/ModsTab.h"
#include "Windows/Main/Tabs/TabKit.h"

// The right column of the Mod Manager: the Main Actions row and the Priority Mode Controls row of
// the C# (now two cards), plus the mod manager's own status line (its StatusMessage was never shown
// in the C#).
namespace SonicRacingSaveManager::Main {

namespace {

namespace M = UI::Material;
using TabKit::PillButton;
using TabKit::Style;

constexpr float kPad = 20.f, kButtonH = 44.f, kRowGap = 10.f;
const Color kGreen = Argb(0xFF4CAF50), kOrange = Argb(0xFFFF6B00), kPurple = Argb(0xFF9C27B0), kGrey = Argb(0xFF808080);

void Title(UI::Ui& ui, const Rect& c, const std::wstring& text) {
    TextStyle caps = Style(11.f, DWRITE_FONT_WEIGHT_BOLD);
    caps.valign = VAlign::Center;
    ui.g.Text(text, { c.x, c.y, c.w, 20.f }, M::P().primaryMid.color, caps);
}

} // namespace

float ModsTab::ActionsCard(UI::Ui& ui, const Rect& r, bool draw) {
    const float h = kPad * 2.f + 26.f + (kButtonH + kRowGap) * 3.f + 40.f + 12.f + 34.f;
    if (!draw) return h;
    ModsState& s = *state;
    const M::Palette& p = M::P();
    M::CardLook look;
    look.radius = 16.f;
    M::Card card(ui, ui.MakeId("md-actions"), r, Edges(kPad), false, 1.f, look);
    const Rect c = card.content;
    Title(ui, c, Loc::Get("mods_actions"));
    float y = c.y + 26.f;
    const float half = (c.w - kRowGap) * 0.5f;
    if (PillButton(ui, ui.MakeId("md-toggle"), { c.x, y, c.w, kButtonH }, M::Icon::SwapHorizontal, Loc::Get("mods_toggle_selected"), p.primaryMid.color, true,
                   s.CanToggleMod()))
        s.ToggleSelectedMod();
    y += kButtonH + kRowGap;
    if (PillButton(ui, ui.MakeId("md-enable-all"), { c.x, y, half, kButtonH }, M::Icon::CheckAll, Loc::Get("mods_enable_all"), kGreen, true, s.CanEnableAll()))
        s.EnableAllMods();
    if (PillButton(ui, ui.MakeId("md-disable-all"), { c.x + half + kRowGap, y, half, kButtonH }, M::Icon::CloseCircle, Loc::Get("mods_disable_all"), kOrange,
                   true, s.CanDisableAll()))
        s.DisableAllMods();
    y += kButtonH + kRowGap;
    if (PillButton(ui, ui.MakeId("md-open"), { c.x, y, half, kButtonH }, M::Icon::FolderOpen, Loc::Get("manage_open_folder"), p.primaryMid.color, false))
        s.OpenModsFolder();
    if (PillButton(ui, ui.MakeId("md-check"), { c.x + half + kRowGap, y, half, kButtonH }, M::Icon::CloudDownload, Loc::Get("mods_check_updates"),
                   p.primaryMid.color, false))
        s.CheckForUpdates();
    y += kButtonH + kRowGap;
    {
        const std::wstring& label = Loc::Get("refresh");
        const float w = TabKit::PillWidth(ui, label);
        const float angle = s.isLoading ? std::fmod(static_cast<float>(ui.Time()) * 360.f, 360.f) : 0.f;
        if (s.isLoading) ui.KeepAnimating();
        if (PillButton(ui, ui.MakeId("md-refresh"), { c.CX() - w * 0.5f, y, w, 40.f }, M::Icon::Refresh, label, p.primaryMid.color, false, true, angle))
            s.RefreshMods();
        y += 40.f + 12.f;
    }
    // The mod manager's status, sliding in when it changes
    {
        struct Shown {
            std::wstring text;
            double at = -10.0;
        };
        Shown& shown = ui.Data<Shown>(ui.MakeId("md-status"));
        if (shown.text != s.statusMessage) {
            shown.text = s.statusMessage;
            shown.at = ui.Time();
        }
        const float k = Anim::EaseOutCubic(Anim::Clamp01(static_cast<float>((ui.Time() - shown.at) / 0.35)));
        if (k < 1.f) ui.KeepAnimating();
        const Rect box{ c.x, y, c.w, 34.f };
        ui.g.FillRound(box, 10.f, p.primaryMid.color.Fade(0.08f));
        M::DrawIcon(ui.g, M::Icon::InformationOutline, box.x + 16.f, box.CY(), 16.f, TabKit::Readable(p.primaryMid.color));
        TextStyle t = Style(11.5f);
        t.valign = VAlign::Center;
        t.ellipsis = true;
        ui.g.PushOpacity(k);
        ui.g.Text(s.statusMessage, { box.x + 32.f + 8.f * (1.f - k), box.y, box.w - 40.f, box.h }, p.foreground.Fade(0.8f), t);
        ui.g.PopLayer();
        ui.Tooltip(ui.MakeId("md-status-tip"), s.statusMessage);
    }
    return h;
}

float ModsTab::PriorityCard(UI::Ui& ui, const Rect& r, bool draw) {
    ModsState& s = *state;
    const float open = draw ? ui.Tween(ui.MakeId("md-priority-open"), s.isPriorityMode ? 1.f : 0.f, 0.25f) : (s.isPriorityMode ? 1.f : 0.f);
    const float h = kPad * 2.f + 26.f + 52.f + kRowGap + (kButtonH + kRowGap) * 2.f - kRowGap + Anim::EaseInOutCubic(open) * (kRowGap + kButtonH + 50.f);
    if (!draw) return h;
    const M::Palette& p = M::P();
    M::CardLook look;
    look.radius = 16.f;
    M::Card card(ui, ui.MakeId("md-priority"), r, Edges(kPad), false, 1.f, look);
    const Rect c = card.content;
    Title(ui, c, Loc::Get("mods_folder_priority"));
    float y = c.y + 26.f;
    BackupKit::InfoChip(ui, { c.x, y, c.w, 52.f }, M::Icon::FolderCog, Loc::Get("mods_folder"),
                        s.ModsDirectory().empty() ? Loc::Get("mods_no_folder") : s.ModsDirectory());
    y += 52.f + kRowGap;
    // Select Mods Folder (Background #9C27B0)
    if (PillButton(ui, ui.MakeId("md-select-folder"), { c.x, y, c.w, kButtonH }, M::Icon::FolderCog, Loc::Get("mods_select_folder"), kPurple, true))
        s.SelectModsFolder();
    y += kButtonH + kRowGap;
    // Enable / Disable Priority (#808080, #9C27B0 while priority mode is on)
    if (PillButton(ui, ui.MakeId("md-priority-toggle"), { c.x, y, c.w, kButtonH }, M::Icon::FormatListNumbered,
                   Loc::Get(s.isPriorityMode ? "mods_disable_priority" : "mods_enable_priority"), Lerp(kGrey, kPurple, open), true, s.CanTogglePriorityMode()))
        s.TogglePriorityMode();
    y += kButtonH;
    if (open > 0.01f) {
        ui.PushClip({ c.x - 6.f, y, c.w + 12.f, c.B() - y + 6.f });
        {
            M::Motion::Enter enter(ui, open, -10.f);
            y += kRowGap;
            const float half = (c.w - kRowGap) * 0.5f;
            if (PillButton(ui, ui.MakeId("md-up"), { c.x, y, half, kButtonH }, M::Icon::ArrowUp, Loc::Get("mods_move_up"), p.primaryMid.color, false, s.CanMove()))
                s.MoveModUp();
            if (PillButton(ui, ui.MakeId("md-down"), { c.x + half + kRowGap, y, half, kButtonH }, M::Icon::ArrowDown, Loc::Get("mods_move_down"),
                           p.primaryMid.color, false, s.CanMove()))
                s.MoveModDown();
            y += kButtonH + 8.f;
            TextStyle note = Style(11.5f, DWRITE_FONT_WEIGHT_NORMAL, true);
            note.italic = true;
            ui.g.Text(Loc::Get("mods_priority_hint"), { c.x, y, c.w, 40.f }, p.foreground.Fade(0.7f), note);
        }
        ui.PopClip();
    }
    return h;
}

} // namespace SonicRacingSaveManager::Main
