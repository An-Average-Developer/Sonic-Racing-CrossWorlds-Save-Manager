#include <algorithm>
#include <cmath>
#include <functional>
#include "Core/Text.h"
#include "Localization/Localization.h"
#include "UI/Material/Fields.h"
#include "UI/Material/Motion.h"
#include "UI/Material/Theme.h"
#include "UI/Widgets/ScrollArea.h"
#include "Windows/Main/Tabs/Backup/BackupKit.h"
#include "Windows/Main/Tabs/SaveEditor/SaveEditorCourse.h"
#include "Windows/Main/Tabs/SaveEditor/SaveEditorTab.h"
#include "Windows/Main/Tabs/TabKit.h"

// The Save Editor's courses: a bar (the Red Star Rings got, search, every ring) over a tile per course
// (its photo, emblem, name and five rings) under the name of its Grand Prix, the CrossWorlds last.
// A tile opens the course's card (SaveEditorCourse.cpp).
namespace SonicRacingSaveManager::Main {

namespace M = UI::Material;
namespace SF = Services::SaveFile;
using namespace SaveEditorKit;
using TabKit::Style;

namespace {

constexpr float kGroupH = 36.f;

int RingsOf(const SF::Course& c) { return static_cast<int>(std::count(c.rings.begin(), c.rings.end(), true)); }

// The speed classes a cup was won at
int WonSpeeds(const SF::Course& c) {
    int n = 0;
    for (int r : c.cupRank) n += r >= SF::kCupWon;
    return n;
}

} // namespace

const std::vector<SF::Course>& SaveEditorTab::Courses() {
    if (coursesFor == version) return courses;
    coursesFor = version;
    courses = save ? save->Courses() : std::vector<SF::Course>{};
    return courses;
}

// The section's icon, name and Red Star Rings (with a bar), then on the right the search box and
// "Every ring"
void SaveEditorTab::CoursesBar(UI::Ui& ui, const Rect& r) {
    const M::Palette& p = M::P();
    const Color tone = kRing;
    Card(ui, r, kRadius, tone, 0.f);
    ui.g.PushRoundClip(r, kRadius);
    ui.g.FillLinear(r, 0.f, { { 0.f, tone.Fade(0.16f) }, { 0.45f, Colors::Transparent } }, { r.x, r.y }, { r.R(), r.y });
    ui.g.PopLayer();
    const float cy = r.CY();
    float x = r.x + 14.f;
    ui.g.FillEllipse(x + 18.f, cy, 18.f, 18.f, tone.Fade(0.2f));
    M::DrawIcon(ui.g, M::Icon::FlagCheckered, x + 18.f, cy, 20.f, TabKit::Readable(tone));
    x += 46.f;

    int done = 0, total = 0;
    for (const auto& c : Courses()) {
        done += RingsOf(c);
        total += static_cast<int>(c.rings.size());
    }
    // Right side, from the right
    float right = r.R() - 12.f;
    const std::wstring& all = Loc::Get("saveeditor_courses_all_rings");
    const float aw = TabKit::PillWidth(ui, all) + 10.f;
    right -= aw;
    if (TabKit::PillButton(ui, ui.MakeId("se-all-rings"), { right, cy - 19.f, aw, 38.f }, M::Icon::StarFourPoints, all, tone, true, !busy && done < total)) {
        const double now = ui.Time();
        int k = 0;
        Edit([&] {
            for (const auto& c : Courses())
                for (size_t i = 0; i < c.rings.size(); ++i)
                    if (!c.rings[i] && save->SetRing(c, i, true)) bursts.Fire(c.id + L"#" + std::to_wstring(i), now + std::min(k++, 60) * 0.012);
        });
        animateUntil = now + std::min(k, 60) * 0.012 + kBurstSeconds;
        Status(Loc::Fmt("saveeditor_unlocked_n", std::to_wstring(k)));
    }
    right -= 16.f;
    M::FieldStyle field;
    field.outlined = true;
    field.fontSize = 13.f;
    field.hint = Loc::Get("saveeditor_search");
    const float fh = M::FieldHeight(ui, field);
    const float sw = std::clamp(right - x - 260.f, 140.f, 260.f);
    right -= sw;
    M::TextField(ui, ui.MakeId("se-course-search"), { right, cy - fh * 0.5f, sw, fh }, courseSearch, field, !busy);
    right -= 16.f;

    // Title and count
    TextStyle title = Style(18.f, DWRITE_FONT_WEIGHT_BOLD);
    title.valign = VAlign::Center;
    title.ellipsis = true;
    const float tw = std::max(0.f, right - x);
    ui.g.Text(Loc::Get("saveeditor_section_courses"), { x, cy - 20.f, tw, 24.f }, p.foreground, title);
    const long long n = Counter(ui, ui.MakeId("se-rings-count"), done, 0.5f);
    TextStyle sub = Style(11.5f);
    sub.valign = VAlign::Center;
    const std::wstring line = Loc::Fmt("saveeditor_courses_rings", Text::FromInt(n), std::to_wstring(total));
    const float lw = std::min(tw, std::ceil(ui.g.Measure(line, sub).w));
    ui.g.Text(line, { x, cy + 4.f, lw, 18.f }, p.foreground.Fade(0.6f), sub);
    const float barW = std::min(140.f, tw - lw - 12.f);
    if (barW > 30.f && total) {
        const float v = static_cast<float>(n) / static_cast<float>(total);
        ui.g.FillRound({ x + lw + 12.f, cy + 11.f, barW, 5.f }, 2.5f, p.foreground.Fade(0.08f));
        ui.g.FillRound({ x + lw + 12.f, cy + 11.f, std::max(5.f, barW * v), 5.f }, 2.5f, done == total ? kGreen : tone);
    }
}

// By Grand Prix (its name, and the speed classes it was won at), a tile per course: its photo with
// its emblem on the corner, its name and its five rings. A click opens its card.
void SaveEditorTab::CoursesView(UI::Ui& ui, const Rect& r) {
    if (ui.Time() < animateUntil) ui.KeepAnimating();
    const M::Palette& p = M::P();
    const Rect bar{ r.x, r.y, r.w, kBarH };
    CoursesBar(ui, bar);
    const Rect area{ r.x, bar.B() + kGap, r.w, r.B() - bar.B() - kGap };
    const auto& all = Courses();
    const std::wstring q = Text::Trim(courseSearch);
    std::vector<size_t> shown;
    for (size_t i = 0; i < all.size(); ++i)
        if (q.empty() || Text::Contains(all[i].name, q, true) || Text::Contains(all[i].cupName, q, true)) shown.push_back(i);
    if (shown.empty()) {
        BackupKit::EmptyState(ui, area, M::Icon::Magnify, Loc::Get("saveeditor_empty_title"), Loc::Get("saveeditor_empty_message"));
        return;
    }
    UI::ScrollArea scroll(ui, ui.MakeId("se-courses"), area, false, 70.f, UI::ScrollArea::Bar::Material);
    const float w = area.w - 10.f;   // the bar's room is always kept: no reflow when it shows
    const int cols = std::max(1, static_cast<int>((w + kGap) / (kCourseW + kGap)));
    const float tileW = (w - (cols - 1) * kGap) / cols;
    const double reveal = std::max(loadedAt, screenAt) + 0.12;
    const double now = ui.Time();
    const float top = area.y + scroll.Offset() - 20.f, bottom = area.B() + scroll.Offset() + 20.f;

    TextStyle group = Style(13.f, DWRITE_FONT_WEIGHT_BOLD);
    group.valign = VAlign::Center;
    group.ellipsis = true;
    TextStyle groupRight = Style(11.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD, false, HAlign::Right);
    groupRight.valign = VAlign::Center;
    TextStyle name = Style(13.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    name.valign = VAlign::Center;
    name.ellipsis = true;
    TextStyle count = Style(11.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD, false, HAlign::Right);
    count.valign = VAlign::Center;

    float y = area.y + 2.f;
    int index = 0;
    for (size_t s = 0; s < shown.size();) {
        // A group: the courses of one Grand Prix (or the CrossWorlds)
        const int cup = all[shown[s]].cup;
        size_t e = s;
        while (e < shown.size() && all[shown[e]].cup == cup) ++e;
        {
            const SF::Course& first = all[shown[s]];
            const Color tone = cup >= 0 ? GroupColor(3) : SpeedColor(4);
            ui.g.FillRound({ area.x + 2.f, y + 10.f, 4.f, 16.f }, 2.f, tone);
            const std::wstring label = cup >= 0 ? first.cupName : Loc::Get("saveeditor_crossworlds");
            ui.g.Text(label, { area.x + 14.f, y, w - 220.f, kGroupH }, p.foreground.Fade(0.85f), group);
            if (cup >= 0) {
                const int won = WonSpeeds(first);
                ui.g.Text(Loc::Fmt("saveeditor_cup_won_n", std::to_wstring(won), std::to_wstring(SF::kSpeedCount)), { area.x, y, w, kGroupH },
                          won == SF::kSpeedCount ? kGreen : p.foreground.Fade(0.5f), groupRight);
            }
            y += kGroupH;
        }
        for (size_t k = s; k < e; ++k) {
            const size_t col = (k - s) % cols;
            if (col == 0 && k > s) y += kCourseH + kGap;
            const SF::Course& c = all[shown[k]];
            const Rect tile{ area.x + col * (tileW + kGap), y, tileW, kCourseH };
            if (tile.B() < top || tile.y > bottom) continue;
            M::Motion::Enter enter(ui, M::Motion::Stagger(ui, reveal, std::min(index++, 24), 0.018f, 0.35f), 16.f);
            const UI::Id id = ui.MakeId("se-course", static_cast<int64_t>(std::hash<std::wstring>{}(c.id) & 0x7FFFFFFF));
            const auto it = ui.Interact(id, tile);
            const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, it.held);
            const int got = RingsOf(c);
            const bool full = got == static_cast<int>(c.rings.size()) && got > 0;
            // The tile bounces when one of its rings changes
            float burst = -1.f;
            for (size_t i = 0; i < c.rings.size(); ++i) burst = std::max(burst, bursts.Progress(c.id + L"#" + std::to_wstring(i), now));
            const float scale = BurstScale(burst) * (1.f - 0.03f * fb.press);
            ui.g.PushTransform(D2D1::Matrix3x2F::Scale(scale, scale, D2D1::Point2F(tile.CX(), tile.CY())));
            const Rect lifted = tile.Offset(0.f, -4.f * fb.hover);
            Card(ui, lifted, 14.f, full ? kGreen : kRing, fb.hover);
            if (full) ui.g.StrokeRound(lifted.Inset(0.5f), 13.5f, kGreen.Fade(0.35f), 1.f);

            // Photo (or emblem), the emblem on its corner
            const Rect pic{ lifted.x + 10.f, lifted.y + 10.f, lifted.w - 20.f, 118.f };
            const ImagePtr photo = GameImage(L"stage", c.id, 400);
            const ImagePtr emblem = GameImage(L"stageicon", c.id, 128);
            const float grow = 4.f * fb.hover;
            if (photo) {
                ui.g.PushRoundClip(pic, 10.f);
                ui.g.DrawImage(*photo, pic.Inset(-grow), 1.f, Stretch::UniformToFill);
                ui.g.PopLayer();
            } else {
                ui.g.FillRound(pic, 10.f, kRing.Fade(0.08f));
                if (emblem) ui.g.DrawImage(*emblem, pic.Inset(14.f - grow), 1.f, Stretch::Uniform);
                else M::DrawIcon(ui.g, M::Icon::FlagCheckered, pic.CX(), pic.CY(), 34.f, p.foreground.Fade(0.2f));
            }
            if (photo && emblem) ui.g.DrawImage(*emblem, { pic.x + 6.f, pic.B() - 46.f, 40.f, 40.f }, 1.f, Stretch::Uniform);
            if (full) {
                const float bx = pic.R() - 4.f, by = pic.y + 4.f;
                ui.g.FillEllipse(bx, by, 11.f, 11.f, kGreen);
                ui.g.StrokeEllipse(bx, by, 11.f, 11.f, p.background, 2.f);
                M::DrawIcon(ui.g, M::Icon::Check, bx, by, 13.f, Colors::White);
            }
            // Name, then its five rings and the count
            ui.g.Text(c.name, { lifted.x + 12.f, pic.B() + 6.f, lifted.w - 24.f, 24.f }, p.foreground, name);
            const float ry = lifted.B() - 22.f;
            for (size_t i = 0; i < c.rings.size(); ++i) {
                const float cx = lifted.x + 22.f + i * 22.f;
                if (const ImagePtr ring = GameImage(L"rsr", c.rings[i] ? L"on" : L"off", 128))
                    ui.g.DrawImage(*ring, { cx - 9.f, ry - 9.f, 18.f, 18.f }, 1.f, Stretch::Uniform);
                else ui.g.FillEllipse(cx, ry, 8.f, 8.f, c.rings[i] ? kRing : p.foreground.Fade(0.12f));
            }
            ui.g.Text(std::to_wstring(got) + L" / " + std::to_wstring(c.rings.size()), { lifted.x, ry - 10.f, lifted.w - 12.f, 20.f },
                      full ? kGreen : p.foreground.Fade(0.6f), count);
            ui.g.PopTransform();

            if (it.hovered) ui.SetCursor(IDC_HAND);
            ui.Tooltip(id, c.name + L"\n" + Loc::Get("saveeditor_course_open_tip"));
            if (it.clicked) this->course->Show(c.id);
        }
        y += kCourseH + kGap + 6.f;
        s = e;
    }
    scroll.SetContentSize(y - area.y);
}

} // namespace SonicRacingSaveManager::Main
