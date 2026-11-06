#include "Windows/Main/Tabs/SaveEditor/SaveEditorCourse.h"
#include <algorithm>
#include <cmath>
#include "Core/Text.h"
#include "Data/SaveNames.h"
#include "Localization/Localization.h"
#include "UI/Material/Buttons.h"
#include "UI/Material/Fields.h"
#include "UI/Material/Motion.h"
#include "UI/Material/Theme.h"
#include "UI/Material/Toggles.h"
#include "UI/Widgets/ScrollArea.h"
#include "Windows/Main/Tabs/SaveEditor/SaveEditorTab.h"
#include "Windows/Main/Tabs/TabKit.h"

namespace SonicRacingSaveManager::Main {

namespace {
namespace M = UI::Material;
namespace SF = Services::SaveFile;
using namespace SaveEditorKit;
using TabKit::Style;

constexpr float kTop = 132.f, kSpeedRowH = 62.f, kBlockPad = 16.f;

std::wstring SpeedName(int speed) {
    const wchar_t* name = Data::SaveNames::Find("speed:" + std::to_string(speed), Loc::CurrentLang());
    return name ? name : std::to_wstring(speed);
}

// The Time Trial record of a speed class: 0 Sonic, 1 Super Sonic, -1 none
int TrialOf(int speed) {
    for (int t = 0; t < 2; ++t)
        if (SF::kTrialSpeeds[t] == speed) return t;
    return -1;
}

// A Grand Prix result: 6 won (1st), 5 2nd, 4 3rd, below that finished, 0 not raced
std::wstring CupResult(int rank) {
    if (rank <= 0) return Loc::Get("saveeditor_cup_none");
    if (rank >= SF::kCupWon) return Loc::Get("saveeditor_cup_1st");
    if (rank == 5) return Loc::Get("saveeditor_cup_2nd");
    if (rank == 4) return Loc::Get("saveeditor_cup_3rd");
    return Loc::Get("saveeditor_cup_finished");
}

Color MedalColor(const std::wstring& rank) {
    if (rank == L"Platinum") return Rgba(125, 211, 252);
    if (rank == L"Gold") return kGold;
    if (rank == L"Silver") return Rgba(148, 163, 184);
    if (rank == L"Bronze") return Rgba(180, 83, 9);
    return M::P().foreground.Fade(0.5f);
}

// The speed class's icon (white in the game) on a pill of its colour
void SpeedPill(UI::Ui& ui, const Rect& r, int speed) {
    ui.g.FillRound(r, r.h * 0.5f, SpeedColor(speed));
    if (const ImagePtr icon = GameImage(L"speed", std::to_wstring(speed), 192)) ui.g.DrawImage(*icon, r.Inset(r.h * 0.22f, r.h * 0.2f), 1.f, Stretch::Uniform);
    else M::DrawIcon(ui.g, M::Icon::FlagCheckered, r.CX(), r.CY(), r.h * 0.55f, Colors::White);
}

// A titled block: soft card, the title on top; returns the content's top
float Block(UI::Ui& ui, const Rect& r, M::Icon icon, const std::wstring& title, Color tone) {
    Card(ui, r, 14.f, tone, 0.f);
    TextStyle t = Style(13.f, DWRITE_FONT_WEIGHT_BOLD);
    t.valign = VAlign::Center;
    t.ellipsis = true;
    M::DrawIcon(ui.g, icon, r.x + kBlockPad + 10.f, r.y + kBlockPad + 10.f, 18.f, TabKit::Readable(tone));
    ui.g.Text(title, { r.x + kBlockPad + 28.f, r.y + kBlockPad, r.w - kBlockPad * 2.f - 28.f - 150.f, 20.f }, M::P().foreground, t);
    return r.y + kBlockPad + 30.f;
}

} // namespace

void SaveEditorCourse::Show(const std::wstring& course) {
    id = course;
    speed = -1;
    stepAt = -10.0;
    Open();
}

Color SaveEditorCourse::Accent() const { return speed >= 0 ? SpeedColor(speed) : kRing; }

float SaveEditorCourse::CardHeight(UI::Ui&, float) { return 660.f; }

void SaveEditorCourse::Pick(UI::Ui& ui, int speedClass) {
    speed = speedClass;
    stepAt = ui.Time();
    UI::ScrollArea::ScrollTo(ui, ui.MakeId("sec-body"), 0.f);
}

void SaveEditorCourse::DrawCard(UI::Ui& ui, const Rect& card) {
    Frame(ui, card);
    // The save was closed or read again without it
    const SF::Course* found = nullptr;
    for (const auto& c : tab.Courses())
        if (c.id == id) found = &c;
    if (!tab.save || !found) {
        Close();
        return;
    }
    const SF::Course& c = *found;
    if (ui.Time() < tab.animateUntil) ui.KeepAnimating();
    {
        M::Motion::Enter enter(ui, Reveal(ui, 0), 0.f, -10.f);
        Header(ui, { card.x, card.y, card.w, kTop }, c);
    }
    const Rect body{ card.x + kPad, card.y + kTop + 18.f, card.w - kPad * 2.f, card.h - kTop - 18.f - 18.f };
    // A step slides in from the side it was opened from
    const double at = std::max(OpenedAt() + 0.15, stepAt);
    M::Motion::Enter enter(ui, M::Motion::Stagger(ui, at, 0, 0.f, 0.32f), 0.f, speed < 0 ? -28.f : 28.f);
    if (speed < 0) Speeds(ui, body, c);
    else Detail(ui, body, c);
}

// The photo as a banner, darkened to the bottom, with the emblem, the name and the Grand Prix (and
// the speed class once one is picked); Back and Close on it
void SaveEditorCourse::Header(UI::Ui& ui, const Rect& r, const SF::Course& c) {
    ui.g.PushRoundClip({ r.x, r.y, r.w, r.h + kRadius }, kRadius);
    ui.g.PushClip(r);
    if (const ImagePtr photo = GameImage(L"stage", c.id, 400)) ui.g.DrawImage(*photo, r, 1.f, Stretch::UniformToFill);
    else ui.g.FillLinear(r, 0.f, { { 0.f, kRing.Fade(0.85f) }, { 1.f, SpeedColor(4).Fade(0.85f) } }, { r.x, r.y }, { r.R(), r.B() });
    ui.g.FillLinear(r, 0.f, { { 0.f, Colors::Black.Fade(0.15f) }, { 1.f, Colors::Black.Fade(0.78f) } }, { r.x, r.y }, { r.x, r.B() });
    ui.g.PopClip();
    ui.g.PopLayer();

    const M::ButtonLook light = M::IconButtonLook(Colors::White);
    const float bh = 38.f;
    if (M::IconButton(ui, ui.MakeId("sec-close"), { r.R() - 14.f - bh, r.y + 12.f, bh, bh }, M::Icon::Close, 20.f, light, true)) Close();
    float x = r.x + kPad;
    if (speed >= 0) {
        const UI::Id back = ui.MakeId("sec-back");
        if (M::IconButton(ui, back, { r.x + 12.f, r.y + 12.f, bh, bh }, M::Icon::ArrowLeft, 20.f, light, true)) {
            speed = -1;
            stepAt = ui.Time();
        }
        ui.Tooltip(back, Loc::Get("saveeditor_course_back"));
    }
    const float cy = r.B() - 42.f;
    if (const ImagePtr emblem = GameImage(L"stageicon", c.id, 128)) {
        ui.g.DrawImage(*emblem, { x, cy - 30.f, 60.f, 60.f }, 1.f, Stretch::Uniform);
        x += 72.f;
    }
    TextStyle title = Style(22.f, DWRITE_FONT_WEIGHT_BOLD);
    title.valign = VAlign::Center;
    title.ellipsis = true;
    const float tw = r.R() - kPad - x;
    ui.g.Text(c.name, { x, cy - 26.f, tw, 30.f }, Colors::White, title);
    TextStyle sub = Style(12.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    sub.valign = VAlign::Center;
    sub.ellipsis = true;
    std::wstring line = c.cup >= 0 ? c.cupName : Loc::Get("saveeditor_crossworlds");
    if (speed >= 0) line += L"   \x00B7   " + SpeedName(speed);
    ui.g.Text(line, { x, cy + 6.f, tw, 20.f }, Colors::White.Fade(0.85f), sub);
}

float SaveEditorCourse::Rings(UI::Ui& ui, float x, float y, float w, const SF::Course& c) {
    const M::Palette& p = M::P();
    const float h = 112.f;
    const Rect r{ x, y, w, h };
    const int got = static_cast<int>(std::count(c.rings.begin(), c.rings.end(), true));
    const float top = Block(ui, r, M::Icon::StarFourPoints, Loc::Get("saveeditor_course_rings"), kRing);
    TextStyle right = Style(12.f, DWRITE_FONT_WEIGHT_SEMI_BOLD, false, HAlign::Right);
    right.valign = VAlign::Center;
    ui.g.Text(std::to_wstring(got) + L" / " + std::to_wstring(c.rings.size()), { r.x, r.y + kBlockPad, r.w - kBlockPad, 20.f },
              got == static_cast<int>(c.rings.size()) ? kGreen : p.foreground.Fade(0.6f), right);
    const double now = ui.Time();
    constexpr float d = 46.f, gap = 14.f;
    for (size_t i = 0; i < c.rings.size(); ++i) {
        const float cx = r.x + kBlockPad + d * 0.5f + i * (d + gap), cyy = top + d * 0.5f + 2.f;
        const std::wstring key = c.id + L"#" + std::to_wstring(i);
        const UI::Id rid = ui.MakeId("sec-ring", static_cast<int64_t>(i));
        const bool on = c.rings[i];
        if (RingToken(ui, rid, cx, cyy, d, on, tab.bursts.Progress(key, now), !tab.busy)) {
            bool changed = false;
            tab.Edit([&] { changed = tab.save->SetRing(c, i, !on); });
            if (changed) tab.bursts.Fire(key, now);
        }
        ui.Tooltip(rid, Loc::Fmt("saveeditor_ring_n", std::to_wstring(i + 1)) + L"\n" + Loc::Get(on ? "saveeditor_click_unmark" : "saveeditor_click_mark"));
    }
    TextStyle note = Style(11.5f, DWRITE_FONT_WEIGHT_NORMAL, true);
    const float nx = r.x + kBlockPad + 5.f * (d + gap) + 4.f;
    ui.g.Text(Loc::Get("saveeditor_course_rings_note"), { nx, top + 4.f, std::max(0.f, r.R() - kBlockPad - nx), d }, p.foreground.Fade(0.55f), note);
    return h;
}

// The rings, then "Pick a speed class" and a row per class: its icon, name, Time Trial time (Sonic
// and Super Sonic) and an arrow
void SaveEditorCourse::Speeds(UI::Ui& ui, const Rect& r, const SF::Course& c) {
    const M::Palette& p = M::P();
    UI::ScrollArea scroll(ui, ui.MakeId("sec-body"), r, false, 60.f, UI::ScrollArea::Bar::Thin);
    const float w = r.w - 8.f;
    float y = r.y;
    y += Rings(ui, r.x, y, w, c) + 18.f;
    TextStyle label = Style(12.f, DWRITE_FONT_WEIGHT_BOLD);
    label.valign = VAlign::Center;
    ui.g.Text(Loc::Get("saveeditor_course_pick_speed"), { r.x + 4.f, y, w, 20.f }, p.foreground.Fade(0.7f), label);
    y += 28.f;
    TextStyle name = Style(15.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    name.valign = VAlign::Center;
    name.ellipsis = true;
    for (int s = 0; s < SF::kSpeedCount; ++s, y += kSpeedRowH + 8.f) {
        const Rect row{ r.x, y, w, kSpeedRowH };
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, std::max(OpenedAt() + 0.2, stepAt), s, 0.04f, 0.3f), 0.f, -14.f);
        const UI::Id rowId = ui.MakeId("sec-speed", s);
        const auto it = ui.Interact(rowId, row);
        const M::Motion::Feedback fb = M::Motion::Hover(ui, rowId, it.hovered, it.held);
        const Color tone = SpeedColor(s);
        Card(ui, row.Offset(2.f * fb.hover, 0.f), 14.f, tone, fb.hover);
        SpeedPill(ui, { row.x + 12.f, row.CY() - 20.f, 92.f, 40.f }, s);
        // From the right: arrow, Time Trial (the Grand Prix result is in the speed's own page)
        M::DrawIcon(ui.g, M::Icon::ChevronRight, row.R() - 24.f + 3.f * fb.hover, row.CY(), 22.f, p.foreground.Fade(0.45f + 0.4f * fb.hover));
        float right = row.R() - 46.f;
        const int t = TrialOf(s);
        if (t >= 0) {
            const SF::TrialRecord& rec = c.trials[t];
            const std::wstring text = rec.valid ? CourseTime(rec.time) : Loc::Get("saveeditor_course_tt_none_short");
            const float cw = ChipWidth(ui, text, true);
            Chip(ui, right - cw, row.CY(), text, rec.valid ? MedalColor(rec.rank) : p.foreground.Fade(0.45f), false, true);
            right -= cw + 8.f;
        }
        const float tx = row.x + 118.f;
        ui.g.Text(SpeedName(s), { tx, row.y, std::max(0.f, right - 8.f - tx), row.h }, p.foreground, name);
        if (it.hovered) ui.SetCursor(IDC_HAND);
        if (it.clicked) Pick(ui, s);
    }
    scroll.SetContentSize(y - r.y);
}

// A speed class: its Grand Prix (opened, the result here) and its Time Trial (the record and its
// reward); the rings are on the speed classes' page
void SaveEditorCourse::Detail(UI::Ui& ui, const Rect& r, const SF::Course& c) {
    const M::Palette& p = M::P();
    UI::ScrollArea scroll(ui, ui.MakeId("sec-body"), r, false, 60.f, UI::ScrollArea::Bar::Thin);
    const float w = r.w - 8.f;
    float y = r.y;
    const double now = ui.Time();
    TextStyle text = Style(13.f, DWRITE_FONT_WEIGHT_NORMAL, true);
    TextStyle meta = Style(12.f);
    meta.valign = VAlign::Center;
    meta.ellipsis = true;

    // The speed class
    {
        SpeedPill(ui, { r.x, y, 104.f, 44.f }, speed);
        TextStyle big = Style(19.f, DWRITE_FONT_WEIGHT_BOLD);
        big.valign = VAlign::Center;
        big.ellipsis = true;
        ui.g.Text(SpeedName(speed), { r.x + 118.f, y, w - 118.f, 44.f }, p.foreground, big);
        y += 44.f + 16.f;
    }

    // Grand Prix
    {
        const float h = c.cup >= 0 ? 132.f : 84.f;
        const Rect b{ r.x, y, w, h };
        const float top = Block(ui, b, M::Icon::Trophy, Loc::Get("saveeditor_course_gp") + (c.cup >= 0 ? L"   \x00B7   " + c.cupName : L""), kGold);
        if (c.cup < 0) {
            ui.g.Text(Loc::Get("saveeditor_course_no_gp"), { b.x + kBlockPad, top, b.w - kBlockPad * 2.f, 36.f }, p.foreground.Fade(0.6f), text);
        } else {
            // Opened: a switch on the title's line
            const Rect sw{ b.R() - kBlockPad - M::kSwitchWidth, b.y + kBlockPad - 2.f, M::kSwitchWidth, M::kSwitchHeight };
            TextStyle swLabel = Style(12.f, DWRITE_FONT_WEIGHT_SEMI_BOLD, false, HAlign::Right);
            swLabel.valign = VAlign::Center;
            ui.g.Text(Loc::Get("saveeditor_course_open"), { sw.x - 160.f, sw.y, 150.f, sw.h }, p.foreground.Fade(0.7f), swLabel);
            if (M::Switch(ui, ui.MakeId("sec-cup-open"), sw, c.cupOpen, !tab.busy)) {
                const int cup = c.cup;
                const bool opened = !c.cupOpen;
                tab.Edit([&] { tab.save->SetCupOpen(cup, opened); });
            }
            // The result at this speed: a pill per result, the one it has filled
            ui.g.Text(Loc::Get("saveeditor_course_result"), { b.x + kBlockPad, top, b.w, 18.f }, p.foreground.Fade(0.6f), meta);
            struct Option {
                int rank;
                const char* key;
                M::Icon icon;
            };
            static constexpr Option kOptions[] = { { 0, "saveeditor_cup_none", M::Icon::Close },
                                                   { 4, "saveeditor_cup_3rd", M::Icon::Medal },
                                                   { 5, "saveeditor_cup_2nd", M::Icon::Medal },
                                                   { SF::kCupWon, "saveeditor_cup_1st", M::Icon::Trophy } };
            float x = b.x + kBlockPad;
            const int current = c.cupRank[speed];
            for (const Option& o : kOptions) {
                const std::wstring& label = Loc::Get(o.key);
                const float pw = TabKit::PillWidth(ui, label);
                const bool on = current == o.rank;
                const Color tone = o.rank >= SF::kCupWon ? kGold : o.rank > 0 ? kAmber : p.foreground.Fade(0.6f);
                if (TabKit::PillButton(ui, ui.MakeId("sec-rank", o.rank), { x, top + 26.f, pw, 36.f }, o.icon, label, tone, on, !tab.busy && !on)) {
                    const int cup = c.cup, sp = speed, rank = o.rank;
                    bool changed = false;
                    tab.Edit([&] { changed = tab.save->SetCupRank(cup, sp, rank); });
                    if (changed) tab.bursts.Fire(c.id + L"#cup", now);
                }
                x += pw + 8.f;
            }
            if (current > 0 && current < 4)   // finished, not on the podium
                ui.g.Text(CupResult(current), { x + 4.f, top + 26.f, b.R() - kBlockPad - x, 36.f }, kAmber, meta);
        }
        y += h + 14.f;
    }

    // Time Trial: only Sonic and Super Sonic have one
    if (const int t = TrialOf(speed); t >= 0) {
        const SF::TrialRecord& rec = c.trials[t];
        const float h = rec.valid ? 262.f : 116.f;
        const Rect b{ r.x, y, w, h };
        const float top = Block(ui, b, M::Icon::TimerOutline, Loc::Get("saveeditor_course_tt"), SpeedColor(speed));
        // Its reward: a switch on the title's line
        const Rect sw{ b.R() - kBlockPad - M::kSwitchWidth, b.y + kBlockPad - 2.f, M::kSwitchWidth, M::kSwitchHeight };
        TextStyle swLabel = Style(12.f, DWRITE_FONT_WEIGHT_SEMI_BOLD, false, HAlign::Right);
        swLabel.valign = VAlign::Center;
        ui.g.Text(Loc::Get("saveeditor_course_tt_reward"), { sw.x - 200.f, sw.y, 190.f, sw.h }, p.foreground.Fade(0.7f), swLabel);
        if (M::Switch(ui, ui.MakeId("sec-tt-reward"), sw, rec.reward, !tab.busy)) {
            const bool on = !rec.reward;
            const size_t trial = static_cast<size_t>(t);
            tab.Edit([&] { tab.save->SetTrialReward(c, trial, on); });
        }
        if (!rec.valid) {
            ui.g.Text(Loc::Get("saveeditor_course_tt_none"), { b.x + kBlockPad, top, b.w - kBlockPad * 2.f, 44.f }, p.foreground.Fade(0.6f), text);
        } else {
            float ty = top;
            // The total (the laps' sum) and its medal
            TextStyle time = Style(30.f, DWRITE_FONT_WEIGHT_BOLD);
            time.valign = VAlign::Center;
            const std::wstring total = CourseTime(rec.time);
            const float timeW = std::ceil(ui.g.Measure(total, time).w);
            ui.g.Text(total, { b.x + kBlockPad, ty, timeW + 4.f, 40.f }, p.foreground, time);
            Chip(ui, b.x + kBlockPad + timeW + 16.f, ty + 20.f, Loc::Get("saveeditor_tt_rank_" + Text::Narrow(rec.rank)), MedalColor(rec.rank), true, true);
            ty += 48.f;

            // A box per lap, written when it loses the focus
            M::FieldStyle field;
            field.outlined = true;
            field.fontSize = 14.f;
            field.align = HAlign::Center;
            const float fh = M::FieldHeight(ui, field);
            TextStyle caption = Style(11.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
            caption.valign = VAlign::Center;
            constexpr float kLapW = 132.f;
            for (size_t l = 0; l < 3; ++l) {
                const float lx = b.x + kBlockPad + l * (kLapW + 12.f);
                ui.g.Text(Loc::Fmt("saveeditor_course_tt_lap", std::to_wstring(l + 1)), { lx + 2.f, ty, kLapW, 16.f }, p.foreground.Fade(0.6f), caption);
                const UI::Id fid = ui.MakeId("sec-tt-lap", static_cast<int64_t>(l));
                const int course = c.index;
                const size_t trial = static_cast<size_t>(t);
                M::BoundTextField(
                    ui, fid, { lx, ty + 20.f, kLapW, fh }, CourseTime(rec.laps[l]),
                    [this, course, trial, l](const std::wstring& value) {
                        // Anything that isn't a time puts the old one back
                        if (const auto v = ParseCourseTime(value)) tab.Edit([&] { tab.save->SetTrialLap(course, trial, l, *v); });
                    },
                    field, !tab.busy);
                ui.Tooltip(fid, Loc::Get("saveeditor_course_tt_edit_hint"));
            }
            ty += 20.f + fh + 14.f;

            // The medal
            ui.g.Text(Loc::Get("saveeditor_course_tt_medal"), { b.x + kBlockPad, ty, 200.f, 16.f }, p.foreground.Fade(0.6f), caption);
            float mx = b.x + kBlockPad;
            for (const wchar_t* rank : { L"Bronze", L"Silver", L"Gold", L"Platinum" }) {
                const std::wstring& label = Loc::Get("saveeditor_tt_rank_" + Text::Narrow(rank));
                const float pw = TabKit::PillWidth(ui, label);
                const bool on = rec.rank == rank;
                if (TabKit::PillButton(ui, ui.MakeId("sec-tt-medal", static_cast<int64_t>(std::hash<std::wstring>{}(rank) & 0x7FFFFFFF)), { mx, ty + 20.f, pw, 34.f },
                                       M::Icon::Medal, label, MedalColor(rank), on, !tab.busy && !on)) {
                    const int course = c.index;
                    const size_t trial = static_cast<size_t>(t);
                    const std::wstring value = rank;
                    tab.Edit([&] { tab.save->SetTrialRank(course, trial, value); });
                }
                mx += pw + 8.f;
            }
            ty += 20.f + 34.f + 12.f;

            // Who and when
            const wchar_t* driver = Data::SaveNames::Find("driver:" + Text::Narrow(rec.driver), Loc::CurrentLang());
            std::wstring who = driver ? driver : rec.driver;
            if (rec.date.size() >= 10) who += L"   \x00B7   " + rec.date.substr(0, 10);
            ui.g.Text(who, { b.x + kBlockPad, ty, b.w - kBlockPad * 2.f, 20.f }, p.foreground.Fade(0.5f), meta);
        }
        y += h + 14.f;
    }

    scroll.SetContentSize(y - r.y);
}

} // namespace SonicRacingSaveManager::Main
