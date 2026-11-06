#include <algorithm>
#include <cmath>
#include "Core/Text.h"
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "UI/Material/Motion.h"
#include "UI/Material/PackIcon.h"
#include "UI/Material/Theme.h"
#include "UI/Widgets/ScrollArea.h"
#include "Windows/Main/Tabs/SaveEditor/SaveEditorTab.h"
#include "Windows/Main/Tabs/TabKit.h"

// The Save Editor's profile section: the whole save's progress (a big ring, a bar per group) with the
// tools that touch everything (Max out the profile, Unlock EVERYTHING), then the profile's numbers
// (level, races, records) as cards, then the tutorials and hints as switches.
namespace SonicRacingSaveManager::Main {

namespace M = UI::Material;
namespace SF = Services::SaveFile;
using namespace SaveEditorKit;
using TabKit::Style;

namespace {
const char* kGroupKeys[] = { "saveeditor_group_profile", "saveeditor_group_garage", "saveeditor_group_custom", "saveeditor_group_progress" };
constexpr float kSummaryH = 168.f, kToolsW = 250.f, kTutorialH = 50.f;
constexpr Color kProfileTone = Rgba(59, 130, 246);   // the profile: blue
} // namespace

SaveEditorTab::Stat SaveEditorTab::OverallStat() {
    Stat all;
    for (const auto& s : Stats()) {
        all.done += s.done;
        all.total += s.total;
    }
    return all;
}

void SaveEditorTab::ProfileView(UI::Ui& ui, const Rect& r) {
    const M::Palette& p = M::P();
    if (ui.Time() < animateUntil) ui.KeepAnimating();
    const auto& cats = Categories();
    const auto& st = Stats();
    const double reveal = std::max(loadedAt, screenAt);
    UI::ScrollArea scroll(ui, ui.MakeId("se-items"), r, false, 60.f, UI::ScrollArea::Bar::Material);
    const float w = scroll.ContentWidth() - (scroll.BarShown() ? 10.f : 0.f);
    float y = r.y;

    // ── The whole save, and the tools ──
    int groupDone[4] = {}, groupTotal[4] = {};
    for (size_t k = 0; k < cats.size(); ++k) {
        groupDone[cats[k].group] += st[k].done;
        groupTotal[cats[k].group] += st[k].total;
    }
    const Stat all = OverallStat();
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, reveal, 0, 0.f, 0.45f), 14.f);
        const Rect card{ r.x, y, w, kSummaryH };
        Card(ui, card, kRadius, kProfileTone, 0.f);
        // The ring fills as it appears
        const float value = all.total ? static_cast<float>(all.done) / static_cast<float>(all.total) : 0.f;
        const float shown = ui.Tween(ui.MakeId("se-total-ring"), value * Anim::EaseOutCubic(M::Motion::Stagger(ui, reveal + 0.1, 0, 0.f, 0.9f)), 0.6f, 0.f);
        if (std::abs(shown - value) > 0.002f) ui.KeepAnimating();
        const float cx = card.x + 90.f, cy = card.CY();
        Ring(ui, cx, cy, 56.f, 10.f, shown, all.done == all.total ? kGreen : kProfileTone);
        TextStyle pct = Style(24.f, DWRITE_FONT_WEIGHT_BOLD, false, HAlign::Center);
        pct.valign = VAlign::Center;
        ui.g.Text(std::to_wstring(static_cast<int>(std::floor(shown * 100.f))) + L"%", { cx - 50.f, cy - 16.f, 100.f, 32.f }, p.foreground, pct);

        // The tools, on the right
        const Rect tools{ card.R() - 20.f - kToolsW, card.y + 20.f, kToolsW, card.h - 40.f };
        const float bh = 40.f;
        const std::wstring& maxAll = Loc::Get("saveeditor_max_all");
        const std::wstring& everything = Loc::Get("saveeditor_unlock_everything");
        bool anyBelow = false;
        for (const auto& f : save->Numbers()) anyBelow |= f.suggested > 0 && f.value < f.suggested;
        const UI::Id maxId = ui.MakeId("se-max-all");
        if (TabKit::PillButton(ui, maxId, { tools.x, tools.y + 8.f, tools.w, bh }, M::Icon::ArrowUp, maxAll, kProfileTone, false, !busy && anyBelow)) {
            int changed = 0;
            Edit([&] {
                for (const auto& f : save->Numbers())
                    if (f.suggested > 0 && f.value < f.suggested) {
                        save->SetNumber(f.key, f.suggested);
                        ++changed;
                    }
            });
            Status(Loc::Fmt("saveeditor_unlocked_n", std::to_wstring(changed)));
        }
        ui.Tooltip(maxId, Loc::Get("saveeditor_max_all_tip"));
        const UI::Id allId = ui.MakeId("se-unlock-everything");
        if (TabKit::PillButton(ui, allId, { tools.x, tools.y + 8.f + bh + 12.f, tools.w, bh }, M::Icon::LockOpenVariant, everything, kAmber, true,
                               !busy && all.done < all.total))
            UnlockEverything();
        ui.Tooltip(allId, Loc::Get("saveeditor_tools_hint"));

        // Title, counter and what it is, then a bar per group (when there is room for them)
        TextStyle title = Style(18.f, DWRITE_FONT_WEIGHT_BOLD);
        title.valign = VAlign::Center;
        title.ellipsis = true;
        TextStyle sub = Style(12.f);
        sub.valign = VAlign::Center;
        const float tx = card.x + 180.f;
        const bool bars = tools.x - tx > 660.f;
        const float textW = bars ? 340.f : std::max(0.f, tools.x - 24.f - tx);
        ui.g.Text(Loc::Get("saveeditor_overall"), { tx, card.y + 28.f, textW, 26.f }, p.foreground, title);
        const long long n = Counter(ui, ui.MakeId("se-total-count"), all.done);
        ui.g.Text(Loc::Fmt("saveeditor_unlocked_of", Text::FromInt(n), std::to_wstring(all.total)), { tx, card.y + 56.f, textW, 20.f }, p.foreground.Fade(0.6f), sub);
        ui.g.Text(Loc::Get("saveeditor_profile_hint"), { tx, card.y + 80.f, textW, 64.f }, p.foreground.Fade(0.45f), Style(11.5f, DWRITE_FONT_WEIGHT_NORMAL, true));
        if (bars) {
            const float bx = tx + textW + 40.f, bw = tools.x - 36.f - bx;
            TextStyle label = Style(12.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
            label.valign = VAlign::Center;
            label.ellipsis = true;
            TextStyle right = Style(12.f, DWRITE_FONT_WEIGHT_SEMI_BOLD, false, HAlign::Right);
            right.valign = VAlign::Center;
            for (int g = 1; g < 4; ++g) {
                const float gy = card.y + 34.f + (g - 1) * 38.f;
                const Color tone = GroupColor(g);
                const float v = groupTotal[g] ? static_cast<float>(groupDone[g]) / static_cast<float>(groupTotal[g]) : 0.f;
                const float sv = v * Anim::EaseOutCubic(M::Motion::Stagger(ui, reveal + 0.2, g, 0.08f, 0.8f));
                ui.g.FillEllipse(bx + 5.f, gy + 8.f, 5.f, 5.f, tone);
                ui.g.Text(Loc::Get(kGroupKeys[g]), { bx + 18.f, gy, bw - 100.f, 16.f }, p.foreground.Fade(0.85f), label);
                ui.g.Text(std::to_wstring(groupDone[g]) + L" / " + std::to_wstring(groupTotal[g]), { bx, gy, bw, 16.f }, p.foreground.Fade(0.55f), right);
                ui.g.FillRound({ bx, gy + 20.f, bw, 6.f }, 3.f, p.foreground.Fade(0.08f));
                if (sv > 0.f) ui.g.FillRound({ bx, gy + 20.f, std::max(6.f, bw * sv), 6.f }, 3.f, groupDone[g] == groupTotal[g] ? kGreen : tone);
            }
        }
    }
    y += kSummaryH + kGap + 6.f;

    TextStyle group = Style(12.f, DWRITE_FONT_WEIGHT_BOLD);
    group.valign = VAlign::Center;
    int order = 1;

    // ── The profile's numbers (the currencies have their own section) ──
    std::vector<SF::NumberField> fields;
    for (const auto& f : save->Numbers())
        if (!f.currency) fields.push_back(f);
    if (!fields.empty()) {
        ui.g.Text(Loc::Get("saveeditor_profile_numbers"), { r.x + 4.f, y, w, 22.f }, p.foreground.Fade(0.55f), group);
        y += 26.f;
        const int cols = std::clamp(static_cast<int>((w + kGap) / (260.f + kGap)), 1, static_cast<int>(fields.size()));
        const float cw = (w - (cols - 1) * kGap) / cols, ch = 136.f;
        for (size_t k = 0; k < fields.size(); ++k)
            NumberCard(ui, fields[k], { r.x + (k % cols) * (cw + kGap), y + (k / cols) * (ch + kGap), cw, ch }, false, reveal + 0.1, order++);
        y += ((fields.size() + cols - 1) / cols) * (ch + kGap) + 8.f;
    }

    // ── Tutorials and hints: a switch each, and Unlock all ──
    const auto tutorials = save->Rows(SF::ListId::Tutorials);
    if (!tutorials.empty()) {
        int done = 0;
        bool anyLeft = false;
        for (const auto& row : tutorials) {
            done += row.state == SF::ItemState::Unlocked;
            anyLeft |= row.state != SF::ItemState::Unlocked && row.canUnlock;
        }
        const std::wstring heading = Loc::Get("saveeditor_profile_tutorials") + L"   " + std::to_wstring(done) + L" / " + std::to_wstring(tutorials.size());
        ui.g.Text(heading, { r.x + 4.f, y, w - 200.f, 30.f }, p.foreground.Fade(0.55f), group);
        const std::wstring& unlockAll = Loc::Get("saveeditor_unlock_all_short");
        const float uw = TabKit::PillWidth(ui, unlockAll) + 10.f;
        if (TabKit::PillButton(ui, ui.MakeId("se-tutorials-all"), { r.x + w - uw, y - 3.f, uw, 36.f }, M::Icon::CheckAll, unlockAll, GroupColor(3), false,
                               !busy && anyLeft)) {
            int changed = 0;
            Edit([&] { changed = save->UnlockAll(SF::ListId::Tutorials); });
            const double now = ui.Time();
            for (const auto& row : tutorials)
                if (row.state != SF::ItemState::Unlocked) bursts.Fire(L"tutorial:" + row.id, now);
            animateUntil = now + kBurstSeconds;
            Status(Loc::Fmt("saveeditor_unlocked_n", std::to_wstring(changed)));
        }
        y += 40.f;
        const int cols = std::max(1, static_cast<int>((w + kGap) / (230.f + kGap)));
        const float cw = (w - (cols - 1) * kGap) / cols;
        TextStyle name = Style(13.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        name.valign = VAlign::Center;
        name.ellipsis = true;
        for (size_t k = 0; k < tutorials.size(); ++k) {
            const SF::ItemRow& row = tutorials[k];
            const Rect cell{ r.x + (k % cols) * (cw + kGap), y + (k / cols) * (kTutorialH + kGap), cw, kTutorialH };
            M::Motion::Enter enter(ui, M::Motion::Stagger(ui, reveal + 0.15, order++, 0.03f, 0.35f), 12.f);
            const UI::Id id = ui.MakeId("se-tutorial", static_cast<int64_t>(k));
            const auto it = ui.Interact(id, cell);
            const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, it.held);
            const bool on = row.state == SF::ItemState::Unlocked;
            const float sel = ui.Tween(UI::HashId(id, 0x5E), on ? 1.f : 0.f, 0.2f);
            const Color tone = GroupColor(3);
            Card(ui, cell, kTutorialH * 0.5f, tone, fb.hover);
            ui.g.FillRound(cell, kTutorialH * 0.5f, tone.Fade(0.18f * sel));
            // A tick when read, the state's icon when not
            ui.g.FillEllipse(cell.x + 26.f, cell.CY(), 13.f, 13.f, on ? kGreen : StateColor(row.state).Fade(0.2f));
            M::DrawIcon(ui.g, on ? M::Icon::Check : StateIcon(row.state), cell.x + 26.f, cell.CY(), 15.f, on ? Colors::White : TabKit::Readable(StateColor(row.state)));
            ui.g.Text(row.name, { cell.x + 48.f, cell.y, cell.w - 60.f, cell.h }, on ? p.foreground : p.foreground.Fade(0.7f), name);
            if (const float burst = bursts.Progress(L"tutorial:" + row.id, ui.Time()); burst >= 0.f) BurstRing(ui, cell.x + 26.f, cell.CY(), 13.f, burst, kGreen);
            if (CanToggle(row)) {
                ui.Tooltip(id, ActionText(row));
                if (it.hovered) ui.SetCursor(IDC_HAND);
                if (it.clicked && !busy) {
                    const std::wstring rid = row.id;
                    bool changed = false;
                    Edit([&] { changed = on ? save->Lock(SF::ListId::Tutorials, rid) : save->Unlock(SF::ListId::Tutorials, rid); });
                    if (changed) {
                        bursts.Fire(L"tutorial:" + rid, ui.Time());
                        animateUntil = ui.Time() + kBurstSeconds;
                    }
                }
            }
        }
        y += ((tutorials.size() + cols - 1) / cols) * (kTutorialH + kGap);
    }
    scroll.SetContentSize(y - r.y);
}

} // namespace SonicRacingSaveManager::Main
