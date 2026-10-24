#include "Windows/Main/MainScreen/Bars.h"
#include <cmath>
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "UI/Material/Elevation.h"
#include "UI/Material/Motion.h"
#include "UI/Material/PackIcon.h"
#include "UI/Material/Theme.h"
#include "Windows/Main/MainState.h"
#include "Windows/Main/MainWindow.h"

namespace SonicRacingSaveManager::Main::NavRail {

namespace {

namespace M = UI::Material;

struct Item {
    M::Icon icon;
    const char* text;   // translation key
};

// The TabItem headers, one per group (MainWindow::GroupIndex): PackIcon 20 (Margin 0,0,0,8) over a
// FontSize 10 / LineHeight 12 caption (translation keys; "\n" in the text splits the caption in two
// lines)
constexpr Item kItems[MainWindow::GroupCount] = {
    { M::Icon::BackupRestore, "rail_backups" },
    { M::Icon::Memory, "rail_tickets" },
    { M::Icon::Puzzle, "rail_mods_group" },
    { M::Icon::CloudDownload, "rail_updates" },
    { M::Icon::FileDocumentOutline, "rail_changelogs" },
    { M::Icon::Bug, "rail_reports" },
    { M::Icon::Steam, "rail_steam" },
};

TextStyle CaptionStyle() {
    TextStyle t = M::Medium(10.f);   // the Ripple presenter sets FontWeight Medium for the header
    t.lineHeight = 12.f;
    t.align = HAlign::Center;
    t.valign = VAlign::Top;
    return t;
}

void DrawHeader(UI::Ui& ui, MainWindow& main, int index, const Rect& r, Color fg) {
    const TextStyle caption = CaptionStyle();
    const std::wstring& text = Loc::Get(kItems[index].text);
    const int lines = text.find(L'\n') != std::wstring::npos ? 2 : 1;
    const float textH = 12.f * lines;
    if (index == MainWindow::UpdatesGroup) {
        // Grid 32x32 (Margin 0,-6,0,2) holding the breathing ring and the icon
        const float h = 32.f - 6.f + 2.f + textH;
        const float top = r.CY() - h * 0.5f - 6.f;   // the Grid starts 6 px above the stack
        const float cx = r.CX(), cy = top + 16.f;
        if (main.State().HasAnyUpdateAvailable()) {
            // Opacity 0.2 -> 1 over 0.8 s, AutoReverse, Forever
            const float o = Anim::Lerp(0.2f, 1.f, Anim::PingPong(ui.Time(), 1.6));
            ui.g.StrokeEllipse(cx, cy, 15.f, 15.f, Rgba(255, 0, 0).Fade(o), 2.f);
            ui.KeepAnimatingThrottled();
        }
        M::DrawIcon(ui.g, kItems[index].icon, cx, cy, 20.f, fg);
        ui.g.Text(text, { r.x, top + 32.f + 2.f, r.w, textH + 2.f }, fg, caption);
        return;
    }
    const float h = 20.f + 8.f + textH;
    const float top = r.CY() - h * 0.5f;
    M::DrawIcon(ui.g, kItems[index].icon, r.CX(), top + 10.f, 20.f, fg);
    ui.g.Text(text, { r.x, top + 28.f, r.w, textH + 2.f }, fg, caption);
}

} // namespace

void Draw(UI::Ui& ui, MainWindow& main, const Rect& rail) {
    const M::Palette& p = M::P();
    // The strip's ColorZone (Mode Standard) paints the paper color behind the items
    ui.g.FillRect(rail, p.background);
    ui.Block(rail);

    // One 72 px row per group
    Rect rows[MainWindow::GroupCount]{};
    for (int i = 0; i < MainWindow::GroupCount; ++i) rows[i] = { rail.x, rail.y + 72.f * static_cast<float>(i), rail.w, 72.f };

    // Selection indicator: a pill that slides to the selected row, with an accent on the edge
    const int current = main.SelectedGroup();
    const Rect& target = rows[current];
    const float iy = ui.Animate(ui.MakeId("rail-indicator"), target.y, 16.f);
    if (std::abs(iy - target.y) > 0.5f) ui.KeepAnimating();
    const Rect pill{ rail.x + 6.f, iy + 5.f, rail.w - 12.f, 62.f };
    ui.g.FillRound(pill, 14.f, p.primaryMid.color.Fade(0.22f));
    ui.g.FillRound({ rail.x, iy + 20.f, 3.f, 32.f }, 1.5f, p.primaryMid.color);

    for (int i = 0; i < MainWindow::GroupCount; ++i) {
        const Rect& r = rows[i];
        const UI::Id id = ui.MakeId("rail", i);
        const UI::Interaction it = ui.Interact(id, r);
        const bool selected = current == i;
        // TabItem selects on mouse down (the group's last shown tab)
        if (it.hovered && ui.In().pressed[0] && !selected) main.SelectGroup(i);
        if (it.hovered && !selected) ui.SetCursor(IDC_HAND);

        const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered && !selected, it.held);
        if (fb.hover > 0.f) ui.g.FillRound({ rail.x + 6.f, r.y + 5.f, rail.w - 12.f, 62.f }, 14.f, p.foreground.Fade(0.045f * fb.hover));

        // Unselected headers at .52 opacity (brighter on hover); the selected one in the primary
        // color, its icon bouncing once when it gets selected
        const float lit = ui.Tween(UI::HashId(id, 0x5E1E), selected ? 1.f : 0.f, 0.2f);
        const Color fg = Lerp(p.foreground.Fade(0.52f + 0.3f * fb.hover), p.primaryMid.color, lit);
        const float pulse = M::Motion::Pulse(ui, id, selected, 0.45f);
        const float bounce = pulse > 0.f ? 1.f + 0.18f * std::sin(3.14159265f * (1.f - pulse)) : 1.f;
        ui.g.PushTransform(D2D1::Matrix3x2F::Scale(bounce, bounce, D2D1::Point2F(r.CX(), r.CY() - 8.f)));
        DrawHeader(ui, main, i, r, fg);
        ui.g.PopTransform();
    }
}

} // namespace SonicRacingSaveManager::Main::NavRail
