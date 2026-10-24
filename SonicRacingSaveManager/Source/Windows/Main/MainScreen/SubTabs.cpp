#include <cmath>
#include <cwctype>
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "UI/Material/Motion.h"
#include "UI/Material/PackIcon.h"
#include "UI/Material/Theme.h"
#include "Windows/Main/MainScreen/Bars.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Tabs/TabKit.h"

namespace SonicRacingSaveManager::Main::SubTabs {

namespace {

namespace M = UI::Material;

struct Item {
    M::Icon icon;
    const char* text;   // the rail's caption keys (their line break becomes a space)
};
// Per tab (MainWindow::TabIndex); only the tabs of groups with several are used
constexpr Item kItems[MainWindow::TabCount] = {
    { M::Icon::BackupRestore, "rail_create_backup" },
    { M::Icon::Restore, "rail_restore" },
    { M::Icon::FolderCog, "rail_manage" },
    { M::Icon::Memory, "rail_tickets" },
    { M::Icon::Puzzle, "rail_mods" },
    { M::Icon::Storefront, "rail_store" },
    { M::Icon::CloudDownload, "rail_updates" },
    { M::Icon::FileDocumentOutline, "rail_changelogs" },
    { M::Icon::Bug, "rail_reports" },
    { M::Icon::Steam, "rail_steam" },
};

// A rail caption on one line: "Create\nBackup" → "Create Backup", "Sauve-\ngardes" → "Sauvegardes"
std::wstring OneLine(const char* key) {
    std::wstring text = Loc::Get(key);
    for (size_t at; (at = text.find(L"-\n")) != std::wstring::npos;) text.erase(at, 2);
    for (auto& c : text)
        if (c == L'\n') c = L' ';
    return text;
}

std::wstring Label(int tab) { return OneLine(kItems[tab].text); }

} // namespace

void Draw(UI::Ui& ui, MainWindow& main, const Rect& bar) {
    const M::Palette& p = M::P();
    const int group = main.SelectedGroup();
    const int first = MainWindow::kGroupFirst[group], count = MainWindow::kGroupSize[group];
    const int selected = main.SelectedTab();

    // Ctrl+Tab / Ctrl+Shift+Tab: the next / previous tab of the group
    if (ui.In().ctrl && ui.KeyPressed(VK_TAB)) {
        const int step = ui.In().shift ? count - 1 : 1;
        main.SelectTab(first + (selected - first + step) % count);
    }

    TextStyle t = TabKit::Style(13.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    t.valign = VAlign::Center;
    // The segments: icon + label, side by side in a rounded track
    constexpr float kPad = 4.f, kIconW = 30.f, kSidePad = 20.f;
    float widths[MainWindow::TabCount]{};
    float trackW = kPad * 2.f;
    for (int i = 0; i < count; ++i) {
        widths[i] = std::ceil(ui.g.Measure(Label(first + i), t).w) + kIconW + kSidePad * 2.f;
        trackW += widths[i];
    }
    const Rect track{ bar.x, bar.y, std::min(trackW, bar.w), bar.h };
    ui.g.FillRound(track, bar.h * 0.5f, p.foreground.Fade(0.06f));
    ui.g.StrokeRound(track.Inset(0.5f), bar.h * 0.5f - 0.5f, p.foreground.Fade(0.08f), 1.f);

    // The highlight slides (and stretches) to the shown tab
    float x = track.x + kPad, selX = x, selW = widths[0];
    for (int i = 0; i < count; ++i) {
        if (first + i == selected) {
            selX = x;
            selW = widths[i];
        }
        x += widths[i];
    }
    const UI::Id hid = ui.MakeId("subtabs-highlight", group);
    const float hx = ui.Animate(UI::HashId(hid, 1), selX, 16.f), hw = ui.Animate(UI::HashId(hid, 2), selW, 16.f);
    if (std::fabs(hx - selX) > 0.5f || std::fabs(hw - selW) > 0.5f) ui.KeepAnimating();
    const Rect highlight{ hx, track.y + kPad, hw, track.h - kPad * 2.f };
    ui.g.GlowRound(highlight, highlight.h * 0.5f, p.primaryMid.color, 10.f, 0.35f);
    ui.g.FillRound(highlight, highlight.h * 0.5f, p.primaryMid.color);

    x = track.x + kPad;
    for (int i = 0; i < count; ++i) {
        const int tab = first + i;
        const Rect r{ x, track.y + kPad, widths[i], track.h - kPad * 2.f };
        x += widths[i];
        const bool on = tab == selected;
        const UI::Id id = ui.MakeId("subtab", tab);
        const UI::Interaction it = ui.Interact(id, r, !on);
        if (it.hovered && !on) ui.SetCursor(IDC_HAND);
        const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered && !on, it.held);
        if (fb.hover > 0.f) ui.g.FillRound(r, r.h * 0.5f, p.foreground.Fade(0.06f * fb.hover));
        // TabItem behaviour: selects on mouse down
        if (it.hovered && !on && ui.In().pressed[0]) main.SelectTab(tab);
        const float lit = ui.Tween(UI::HashId(id, 0x11), on ? 1.f : 0.f, 0.18f);
        const Color fg = Lerp(p.foreground.Fade(0.6f + 0.3f * fb.hover), p.primaryMid.foreground, lit);
        // The icon bounces once when its tab gets shown
        const float pulse = M::Motion::Pulse(ui, id, on, 0.45f);
        const float bounce = pulse > 0.f ? 1.f + 0.2f * std::sin(3.14159265f * (1.f - pulse)) : 1.f;
        M::DrawIcon(ui.g, kItems[tab].icon, r.x + kSidePad + 9.f, r.CY(), 18.f * bounce, fg);
        ui.g.Text(Label(tab), { r.x + kSidePad + kIconW - 4.f, r.y, r.w - kSidePad * 2.f - kIconW + 8.f, r.h }, fg, t);
    }

    // The group's name and where the shown tab is in it ("BACKUPS · 2 / 3"), on the right
    TextStyle caps = TabKit::Style(11.f, DWRITE_FONT_WEIGHT_BOLD, false, HAlign::Right);
    caps.valign = VAlign::Center;
    const std::wstring where = OneLine(group == MainWindow::BackupsGroup ? "rail_backups" : "rail_mods_group") + L"  \x00B7  " +
                               std::to_wstring(selected - first + 1) + L" / " + std::to_wstring(count);
    std::wstring upper = where;
    for (auto& c : upper) c = static_cast<wchar_t>(std::towupper(c));
    if (bar.R() - track.R() > 160.f) ui.g.Text(upper, { track.R() + 16.f, bar.y, bar.R() - track.R() - 16.f, bar.h }, p.foreground.Fade(0.45f), caps);
}

} // namespace SonicRacingSaveManager::Main::SubTabs
