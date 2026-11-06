#include <algorithm>
#include <cmath>
#include <optional>
#include "Core/Text.h"
#include "Core/Time.h"
#include "Data/MissionCatalog.h"
#include "Data/SaveNames.h"
#include "Localization/Localization.h"
#include "UI/Material/Motion.h"
#include "UI/Material/PackIcon.h"
#include "UI/Material/Theme.h"
#include "UI/Material/Buttons.h"
#include "UI/Material/Fields.h"
#include "UI/Widgets/ScrollArea.h"
#include "Windows/Main/Tabs/Backup/BackupKit.h"
#include "Windows/Main/Tabs/SaveEditor/SaveEditorTab.h"
#include "Windows/Main/Tabs/TabKit.h"

// The Save Editor's statistics and Play Missions sections.
//   Statistics: a strip saying what they are, the race totals (_StatsData.RaceDetails) as number cards, then
//   the characters' races and wins and the machines' races, a row each with boxes to change them.
//   Play Missions: a strip with how many are done and Complete all, then each period (a week) with its end
//   and its missions as cards: the reward, the text, the count as a bar, a box for it, Complete and Reset.
namespace SonicRacingSaveManager::Main {

namespace M = UI::Material;
namespace SF = Services::SaveFile;
using namespace SaveEditorKit;
using TabKit::Style;

namespace {
constexpr Color kStatsTone = Rgba(99, 102, 241);     // the statistics: indigo
constexpr Color kMissionTone = Rgba(244, 63, 94);    // the missions: rose
constexpr float kStripH = 64.f, kUsageH = 64.f, kMissionH = 150.f;
constexpr float kBoxW = 92.f, kLabelW = 54.f;   // a usage row's boxes and their labels

// The section's strip: icon, title, what it is, and room on the right (returned) for its buttons
Rect Strip(UI::Ui& ui, const Rect& r, M::Icon icon, Color tone, const std::wstring& title, const std::wstring& hint, float buttonsW) {
    const M::Palette& p = M::P();
    Card(ui, r, kRadius, tone, 0.f);
    ui.g.FillEllipse(r.x + 36.f, r.CY(), 20.f, 20.f, tone.Fade(0.2f));
    M::DrawIcon(ui.g, icon, r.x + 36.f, r.CY(), 22.f, TabKit::Readable(tone));
    TextStyle t = Style(16.f, DWRITE_FONT_WEIGHT_BOLD);
    t.ellipsis = true;
    TextStyle h = Style(12.f, DWRITE_FONT_WEIGHT_NORMAL, true);
    h.ellipsis = true;
    const float tx = r.x + 70.f, tw = std::max(0.f, r.R() - 20.f - buttonsW - tx);
    ui.g.Text(title, { tx, r.y + 10.f, tw, 22.f }, p.foreground, t);
    ui.g.Text(hint, { tx, r.y + 32.f, tw, 26.f }, p.foreground.Fade(0.6f), h);
    return { r.R() - 14.f - buttonsW, r.y, buttonsW, r.h };
}

std::wstring GameNameOf(const std::string& key) {
    const wchar_t* n = Data::SaveNames::Find(key, Loc::CurrentLang());
    return n ? n : L"";
}

M::FieldStyle NumberField() {
    M::FieldStyle field;
    field.outlined = true;
    field.fontSize = 13.f;
    field.align = HAlign::Right;
    return field;
}

// Digits typed (with or without separators); nothing when there are none
std::optional<int64_t> Digits(const std::wstring& text) {
    std::wstring digits;
    for (wchar_t ch : text)
        if (ch >= L'0' && ch <= L'9') digits.push_back(ch);
    return Text::ToInt(digits);
}
} // namespace

// ── Statistics ──────────────────────────────────────────────────────────────────────────────────

void SaveEditorTab::StatsView(UI::Ui& ui, const Rect& r) {
    const M::Palette& p = M::P();
    const auto totals = save->StatNumbers();
    const auto drivers = save->Usage(false);
    const auto machines = save->Usage(true);
    if (totals.empty() && drivers.empty() && machines.empty()) {
        BackupKit::EmptyState(ui, r, M::Icon::TrendingUp, Loc::Get("saveeditor_empty_title"), Loc::Get("saveeditor_stats_none"));
        return;
    }
    const double reveal = std::max(loadedAt, screenAt) + 0.08;
    UI::ScrollArea scroll(ui, ui.MakeId("se-items"), r, false, 60.f, UI::ScrollArea::Bar::Material);
    const float w = scroll.ContentWidth() - (scroll.BarShown() ? 10.f : 0.f);
    float y = r.y;
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, reveal, 0, 0.f, 0.4f), 10.f);
        Strip(ui, { r.x, y, w, kStripH }, M::Icon::TrendingUp, kStatsTone, Loc::Get("saveeditor_section_stats"), Loc::Get("saveeditor_stats_hint"), 0.f);
        y += kStripH + kGap + 6.f;
    }
    TextStyle group = Style(12.f, DWRITE_FONT_WEIGHT_BOLD);
    group.valign = VAlign::Center;
    int order = 1;

    // ── The race totals ──
    if (!totals.empty()) {
        ui.g.Text(Loc::Get("saveeditor_stats_totals"), { r.x + 4.f, y, w, 22.f }, p.foreground.Fade(0.55f), group);
        y += 26.f;
        const int cols = std::clamp(static_cast<int>((w + kGap) / (240.f + kGap)), 1, static_cast<int>(totals.size()));
        const float cw = (w - (cols - 1) * kGap) / cols, ch = 136.f;
        for (size_t k = 0; k < totals.size(); ++k)
            NumberCard(ui, totals[k], { r.x + (k % cols) * (cw + kGap), y + (k / cols) * (ch + kGap), cw, ch }, false, reveal, order++);
        y += ((totals.size() + cols - 1) / cols) * (ch + kGap) + 8.f;
    }

    // ── A row per character / machine: picture, name, Races (and Wins) ──
    const M::FieldStyle field = NumberField();
    const float fieldH = M::FieldHeight(ui, field);
    TextStyle name = Style(14.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    name.valign = VAlign::Center;
    name.ellipsis = true;
    TextStyle boxLabel = Style(11.f, DWRITE_FONT_WEIGHT_SEMI_BOLD, false, HAlign::Right);
    boxLabel.valign = VAlign::Center;
    const auto usageRows = [&](const char* titleKey, const std::vector<SF::StatUsage>& list, bool isMachine) {
        if (list.empty()) return;
        ui.g.Text(Loc::Get(titleKey), { r.x + 4.f, y, w, 22.f }, p.foreground.Fade(0.55f), group);
        y += 26.f;
        const int cols = std::max(1, static_cast<int>((w + kGap) / (440.f + kGap)));
        const float cw = (w - (cols - 1) * kGap) / cols;
        const Color tone = isMachine ? Rgba(14, 165, 233) : GroupColor(1);
        for (size_t k = 0; k < list.size(); ++k) {
            const SF::StatUsage& u = list[k];
            const Rect cell{ r.x + (k % cols) * (cw + kGap), y + (k / cols) * (kUsageH + kGap), cw, kUsageH };
            M::Motion::Enter enter(ui, M::Motion::Stagger(ui, reveal + 0.1, order++, 0.03f, 0.35f), 12.f);
            const UI::Id id = ui.MakeId(isMachine ? "se-usage-m" : "se-usage-d", static_cast<int64_t>(k));
            const auto it = ui.Interact(id, cell);
            const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, false);
            Card(ui, cell, kRadius, tone, fb.hover * 0.6f);
            // Picture: a character's, a machine's (a garage slot shows its front part)
            ImagePtr image;
            std::wstring label;
            if (isMachine) {
                for (const auto& b : Garage())
                    if (b.slot == u.id) {
                        image = Picture(SF::ListId::FrontParts, b.front);
                        label = BuildName(b);
                    }
                if (!image) image = Picture(SF::ListId::Machines, u.id);
                if (!image) image = Picture(SF::ListId::FrontParts, u.id);
                if (label.empty()) label = GameNameOf("machine:" + Text::Narrow(u.id));
            } else {
                image = Picture(SF::ListId::Drivers, u.id);
                label = GameNameOf("driver:" + Text::Narrow(u.id));
            }
            if (label.empty()) label = u.id;
            const float pr = 22.f, pcx = cell.x + 14.f + pr, pcy = cell.CY();
            ui.g.FillEllipse(pcx, pcy, pr + 2.f, pr + 2.f, tone.Fade(0.15f));
            if (image) {
                ui.g.PushRoundClip({ pcx - pr, pcy - pr, pr * 2.f, pr * 2.f }, pr);
                ui.g.DrawImage(*image, { pcx - pr, pcy - pr, pr * 2.f, pr * 2.f }, 1.f, isMachine ? Stretch::Uniform : Stretch::UniformToFill);
                ui.g.PopLayer();
            } else {
                M::DrawIcon(ui.g, isMachine ? M::Icon::CarSports : M::Icon::Account, pcx, pcy, 22.f, TabKit::Readable(tone));
            }
            // The boxes on the right: Races, then Wins (characters)
            float right = cell.R() - 14.f;
            const auto box = [&](bool wins, int64_t value) {
                const Rect b{ right - kBoxW, pcy - fieldH * 0.5f, kBoxW, fieldH };
                ui.g.Text(Loc::Get(wins ? "saveeditor_stats_wins" : "saveeditor_stats_races"), { b.x - kLabelW - 6.f, b.y, kLabelW, b.h }, p.foreground.Fade(0.55f), boxLabel);
                const std::wstring uid = u.id;
                M::BoundTextField(
                    ui, UI::HashId(id, wins ? 0xB1 : 0xB0), b, Text::FromInt(value),
                    [this, uid, wins, isMachine](const std::wstring& text) {
                        const auto v = Digits(text);
                        if (!v) return;
                        Edit([&] { save->SetUsage(isMachine, uid, wins, *v); });
                    },
                    field, !busy);
                right = b.x - kLabelW - 14.f;
            };
            if (u.hasWins) box(true, u.wins);
            box(false, u.used);
            const float tx = pcx + pr + 14.f;
            ui.g.Text(label, { tx, cell.y, std::max(0.f, right - tx), cell.h }, p.foreground, name);
        }
        y += ((list.size() + cols - 1) / cols) * (kUsageH + kGap) + 8.f;
    };
    usageRows("saveeditor_stats_characters", drivers, false);
    usageRows("saveeditor_stats_machines", machines, true);
    scroll.SetContentSize(y - r.y);
}

// ── Play Missions ───────────────────────────────────────────────────────────────────────────────

void SaveEditorTab::MissionsView(UI::Ui& ui, const Rect& r) {
    const M::Palette& p = M::P();
    const auto missions = save->Missions();
    if (missions.empty()) {
        BackupKit::EmptyState(ui, r, M::Icon::FormatListNumbered, Loc::Get("saveeditor_empty_title"), Loc::Get("saveeditor_missions_none"));
        return;
    }
    const double reveal = std::max(loadedAt, screenAt) + 0.08;
    int done = 0;
    for (const auto& m : missions) done += m.state == SF::MissionState::Cleared;

    UI::ScrollArea scroll(ui, ui.MakeId("se-items"), r, false, 60.f, UI::ScrollArea::Bar::Material);
    const float w = scroll.ContentWidth() - (scroll.BarShown() ? 10.f : 0.f);
    float y = r.y;

    // ── The strip: how many are done, Complete all ──
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, reveal, 0, 0.f, 0.4f), 10.f);
        const std::wstring& all = Loc::Get("saveeditor_missions_complete_all");
        const float aw = TabKit::PillWidth(ui, all) + 10.f;
        const std::wstring title = Loc::Get("saveeditor_section_missions") + L"   " + std::to_wstring(done) + L" / " + std::to_wstring(missions.size());
        const Rect buttons = Strip(ui, { r.x, y, w, kStripH }, M::Icon::FormatListNumbered, kMissionTone, title, Loc::Get("saveeditor_missions_hint"), aw);
        if (TabKit::PillButton(ui, ui.MakeId("se-missions-all"), { buttons.x, buttons.CY() - 19.f, aw, 38.f }, M::Icon::CheckAll, all, kMissionTone, true,
                               !busy && done < static_cast<int>(missions.size()))) {
            int changed = 0;
            Edit([&] { changed = save->CompleteMissions(); });
            const double now = ui.Time();
            for (const auto& m : missions)
                if (m.state != SF::MissionState::Cleared) bursts.Fire(L"mission:" + std::to_wstring(m.period) + L":" + std::to_wstring(m.index), now);
            animateUntil = now + kBurstSeconds;
            Status(Loc::Fmt("saveeditor_missions_completed", std::to_wstring(changed)));
        }
        y += kStripH + kGap + 6.f;
    }
    if (ui.Time() < animateUntil) ui.KeepAnimating();

    TextStyle group = Style(12.f, DWRITE_FONT_WEIGHT_BOLD);
    group.valign = VAlign::Center;
    TextStyle text = Style(14.f, DWRITE_FONT_WEIGHT_SEMI_BOLD, true);
    TextStyle reward = Style(14.f, DWRITE_FONT_WEIGHT_BOLD, false, HAlign::Center);
    reward.valign = VAlign::Center;
    TextStyle count = Style(12.f, DWRITE_FONT_WEIGHT_SEMI_BOLD, false, HAlign::Right);
    count.valign = VAlign::Center;
    const M::FieldStyle field = NumberField();
    const float fieldH = M::FieldHeight(ui, field);
    const int cols = std::max(1, static_cast<int>((w + kGap) / (460.f + kGap)));
    const float cw = (w - (cols - 1) * kGap) / cols;
    const int64_t now = Time::Now();
    int order = 1;

    for (size_t start = 0; start < missions.size();) {
        // A period: its missions, under when it ends
        size_t end = start;
        while (end < missions.size() && missions[end].period == missions[start].period) ++end;
        std::wstring heading = Loc::Get("saveeditor_missions_period");
        if (const int64_t ft = missions[start].endsAt) {
            const Time::Ticks local = Time::FromFileTimeUtc(ft);
            heading = Loc::Fmt(local < now ? "saveeditor_missions_ended" : "saveeditor_missions_ends", Time::Display(local).substr(0, 16));
        }
        ui.g.Text(heading, { r.x + 4.f, y, w, 22.f }, p.foreground.Fade(0.55f), group);
        y += 26.f;
        for (size_t k = start; k < end; ++k) {
            const SF::Mission& m = missions[k];
            const size_t at = k - start;
            const Rect card{ r.x + (at % cols) * (cw + kGap), y + (at / cols) * (kMissionH + kGap), cw, kMissionH };
            M::Motion::Enter enter(ui, M::Motion::Stagger(ui, reveal + 0.1, order++, 0.03f, 0.35f), 12.f);
            const UI::Id id = ui.MakeId("se-mission", static_cast<int64_t>(m.period) * 1000 + m.index);
            const auto it = ui.Interact(id, card);
            const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, false);
            const bool cleared = m.state == SF::MissionState::Cleared;
            Card(ui, card, kRadius, cleared ? kGreen : kMissionTone, fb.hover * 0.6f);
            const std::wstring burstId = L"mission:" + std::to_wstring(m.period) + L":" + std::to_wstring(m.index);

            // The reward on the left: the game's picture of it and how many
            const Rect rw{ card.x + 14.f, card.y + 14.f, 74.f, 74.f };
            ui.g.FillRound(rw, 14.f, (cleared ? kGreen : kMissionTone).Fade(0.12f));
            const wchar_t* coin = m.rewardType == SF::kRewardDuelMedals ? L"medals" : m.rewardType == SF::kRewardDonpaTickets ? L"tickets" : nullptr;
            if (const ImagePtr image = coin ? GameImage(L"currency", coin, 128) : nullptr)
                ui.g.DrawImage(*image, { rw.x + 12.f, rw.y + 6.f, rw.w - 24.f, rw.h - 30.f }, 1.f, Stretch::Uniform);
            else M::DrawIcon(ui.g, M::Icon::Star, rw.CX(), rw.y + 26.f, 28.f, TabKit::Readable(kMissionTone));
            ui.g.Text(L"\x00D7" + Text::FromInt(m.rewardCount), { rw.x, rw.B() - 26.f, rw.w, 22.f }, p.foreground, reward);
            if (const float burst = bursts.Progress(burstId, ui.Time()); burst >= 0.f) BurstRing(ui, rw.CX(), rw.CY(), 37.f, burst, kGreen);

            // Its text and state
            const float tx = rw.R() + 14.f, tw = card.R() - 16.f - tx;
            const std::wstring chipText = Loc::Get(m.claimed ? "saveeditor_missions_received"
                                                   : cleared ? "saveeditor_missions_cleared"
                                                   : m.state == SF::MissionState::InProgress ? "saveeditor_missions_in_progress"
                                                                                             : "saveeditor_missions_not_started");
            const Color chipTone = m.claimed ? kGold : cleared ? kGreen : m.state == SF::MissionState::InProgress ? kBlue : p.foreground.Fade(0.5f);
            Chip(ui, tx, card.y + 24.f, chipText, chipTone, false, true);
            ui.g.Text(Data::MissionCatalog::Text(m.type, m.params, Loc::CurrentLang()), { tx, card.y + 38.f, tw, 44.f }, p.foreground, text);

            // The count as a bar
            const float v = static_cast<float>(std::clamp<double>(static_cast<double>(m.progress) / static_cast<double>(m.goal), 0.0, 1.0));
            const float sv = ui.Tween(UI::HashId(id, 0xBA), v, 0.4f, 0.f);
            if (std::abs(sv - v) > 0.002f) ui.KeepAnimating();
            const Rect bar{ tx, card.y + 90.f, tw - 70.f, 6.f };
            ui.g.FillRound(bar, 3.f, p.foreground.Fade(0.08f));
            if (sv > 0.f) ui.g.FillRound({ bar.x, bar.y, std::max(6.f, bar.w * sv), bar.h }, 3.f, cleared ? kGreen : kMissionTone);
            ui.g.Text(Text::FromInt(m.progress) + L" / " + Text::FromInt(m.goal), { bar.R() + 6.f, bar.CY() - 9.f, 64.f, 18.f }, p.foreground.Fade(0.7f), count);

            // Along the bottom: the box, Reset and Complete
            const float by = card.B() - 12.f - 34.f;
            const std::wstring& completeText = Loc::Get("saveeditor_missions_complete");
            const float compW = TabKit::PillWidth(ui, completeText);
            const Rect comp{ card.R() - 16.f - compW, by, compW, 34.f };
            const int period = m.period, index = m.index;
            const int64_t goal = m.goal;
            if (TabKit::PillButton(ui, UI::HashId(id, 0xC1), comp, M::Icon::Check, completeText, kGreen, !cleared, !busy && !cleared)) {
                bool changed = false;
                Edit([&] { changed = save->SetMissionProgress(period, index, goal); });
                if (changed) {
                    bursts.Fire(burstId, ui.Time());
                    animateUntil = ui.Time() + kBurstSeconds;
                }
            }
            const UI::Id resetId = UI::HashId(id, 0xC2);
            const Rect reset{ comp.x - 8.f - 34.f, by, 34.f, 34.f };
            if (M::IconButton(ui, resetId, reset, M::Icon::Restore, 18.f, M::IconButtonLook(), !busy && (m.progress > 0 || m.state != SF::MissionState::NotStarted)))
                Edit([&] { save->SetMissionProgress(period, index, 0); });
            ui.Tooltip(resetId, Loc::Get("saveeditor_missions_reset"));
            const Rect box{ tx, by + (34.f - fieldH) * 0.5f, std::min(110.f, reset.x - 10.f - tx), fieldH };
            const UI::Id boxId = UI::HashId(id, 0xB0);
            M::BoundTextField(
                ui, boxId, box, Text::FromInt(m.progress),
                [this, period, index](const std::wstring& t) {
                    const auto n = Digits(t);
                    if (!n) return;
                    Edit([&] { save->SetMissionProgress(period, index, *n); });
                },
                field, !busy);
            ui.Tooltip(boxId, Loc::Fmt("saveeditor_range", L"0", Text::FromInt(m.goal)));
        }
        y += ((end - start + cols - 1) / cols) * (kMissionH + kGap) + 8.f;
        start = end;
    }
    scroll.SetContentSize(y - r.y);
}

} // namespace SonicRacingSaveManager::Main
