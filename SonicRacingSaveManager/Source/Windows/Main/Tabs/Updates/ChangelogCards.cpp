#include "Windows/Main/Tabs/Updates/ChangelogCards.h"
#include <algorithm>
#include <cmath>
#include <cwctype>
#include "Core/Text.h"
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "Services/App/VersionCompare.h"
#include "UI/Material/Motion.h"
#include "UI/Material/Theme.h"
#include "UI/Widgets/ScrollArea.h"

namespace SonicRacingSaveManager::Main {

namespace {

namespace M = UI::Material;

constexpr float kPadX = 16.f, kHeaderH = 46.f, kGap = 10.f;
constexpr float kTagH = 22.f, kIndent = 18.f, kLineGap = 6.f, kSectionGap = 14.f;
constexpr float kTextSize = 12.5f;

TextStyle Body() {
    TextStyle t = M::Text(kTextSize);
    t.wrap = true;
    t.valign = VAlign::Top;
    return t;
}

TextStyle Centered(float size, DWRITE_FONT_WEIGHT weight) {
    TextStyle t = M::Text(size, weight);
    t.align = HAlign::Center;
    t.valign = VAlign::Center;
    return t;
}

TextStyle Middle(float size, DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL) {
    TextStyle t = M::Text(size, weight);
    t.valign = VAlign::Center;
    return t;
}

// "3.0.1", "2.1.4" (the first word of a version line)
bool SameVersion(const std::wstring& a, const std::wstring& b) { return !Versions::IsNewer(a, b) && !Versions::IsNewer(b, a); }

bool IsVersionNumber(const std::wstring& s) {
    int groups = 0;
    bool digit = false;
    for (wchar_t c : s) {
        if (c >= L'0' && c <= L'9') {
            digit = true;
        } else if (c == L'.' && digit) {
            ++groups;
            digit = false;
        } else {
            return false;
        }
    }
    return digit && groups >= 1 && groups <= 3;
}

std::wstring WithV(const std::wstring& version) {
    if (!version.empty() && (version[0] == L'v' || version[0] == L'V')) return L"v" + version.substr(1);
    return L"v" + version;
}

// Colour and icon of each section: the kinds of change ("## Additions", as in the launchers) and the
// "=====X=====" lists; changes outside any section use the primary colour
struct SectionLook {
    Color color;
    M::Icon icon;
};

SectionLook LookOf(const std::wstring& title) {
    const std::wstring t = Text::Upper(title);
    if (t.empty()) return { M::P().primaryMid.color, M::Icon::CheckCircle };
    if (t == L"ADDITIONS") return { Rgba(46, 204, 140), M::Icon::PlusCircle };
    if (t == L"CHANGES") return { Rgba(110, 168, 240), M::Icon::SwapHorizontal };
    if (t == L"REDESIGNS") return { Rgba(186, 140, 255), M::Icon::Palette };
    if (t == L"SEASONAL THEMES") return { Rgba(255, 196, 64), M::Icon::WeatherSunny };
    if (t == L"COMPATIBILITY") return { Rgba(77, 182, 172), M::Icon::Refresh };
    if (t == L"IMPROVEMENTS") return { Rgba(240, 98, 146), M::Icon::TrendingUp };
    if (t == L"PERFORMANCE") return { Rgba(80, 206, 236), M::Icon::Flash };
    if (t == L"BUG FIXES") return { Rgba(255, 152, 72), M::Icon::Wrench };
    if (t == L"MAINTENANCE") return { Rgba(144, 164, 174), M::Icon::Lock };
    if (t == L"OTHER") return { Rgba(110, 168, 240), M::Icon::InformationOutline };
    static const Color others[] = { Rgba(38, 198, 218), Rgba(236, 64, 122), Rgba(141, 110, 99), Rgba(120, 144, 156) };
    size_t h = 0;
    for (wchar_t c : t) h = h * 31 + c;
    return { others[h % 4], M::Icon::StarCircle };
}

// The section colour made readable as text on the current cards
Color Readable(Color c) { return M::P().baseDark ? Lerp(c, Colors::White, 0.2f) : Lerp(c, Colors::Black, 0.45f); }

// Bold prefix of a change: a special move id ("4178DB91 - ...") or a short "Topic:" (0 = none)
size_t PrefixLength(const std::wstring& line) {
    if (line.size() > 11 && line.compare(8, 3, L" - ") == 0 &&
        std::all_of(line.begin(), line.begin() + 8, [](wchar_t c) { return std::iswxdigit(c) != 0; }))
        return 8;
    const size_t colon = line.find(L": ");
    if (colon == std::wstring::npos || colon == 0 || colon > 28) return 0;
    return colon + 1;
}

} // namespace

Color ChangelogCards::SectionColor(const std::wstring& title) { return LookOf(title).color; }
M::Icon ChangelogCards::SectionIcon(const std::wstring& title) { return LookOf(title).icon; }

std::wstring ChangelogCards::SectionName(const std::wstring& title) {
    static const struct {
        const wchar_t* title;
        const char* key;
    } kSections[] = {
        { L"ADDITIONS", "changelog_section_additions" },     { L"REDESIGNS", "changelog_section_redesigns" },
        { L"SEASONAL THEMES", "changelog_section_seasonal" }, { L"COMPATIBILITY", "changelog_section_compatibility" },
        { L"IMPROVEMENTS", "changelog_section_improvements" }, { L"PERFORMANCE", "changelog_section_performance" },
        { L"BUG FIXES", "changelog_section_bug_fixes" },     { L"MAINTENANCE", "changelog_section_maintenance" },
        { L"OTHER", "changelog_section_other" },         { L"CHANGES", "changelog_section_changes" },
    };
    const std::wstring upper = Text::Upper(title);
    for (const auto& s : kSections)
        if (upper == s.title) return Loc::Get(s.key);
    return title;
}
Color ChangelogCards::ReadableColor(Color c) { return Readable(c); }

// Version lines ("3.0.1", "2.1.4 [7.0.2 UPDATE!]" or "3.0.1 | 19/10/2026") start an entry, "## X" and
// "=====X=====" lines a section, anything else is a change ("- " dropped). Text without any version
// line is one entry for `latest`.
void ChangelogCards::Build(UI::Ui& ui, const std::wstring& changelog, const std::wstring& current, const std::wstring& latest, float width) {
    struct Raw {
        std::wstring version, label, date;
        std::vector<std::wstring> lines;
    };
    std::vector<Raw> all;
    for (const auto& raw : Text::SplitLines(changelog)) {
        const std::wstring line = Text::Trim(raw);
        if (line.empty()) continue;
        const size_t space = line.find(L' ');
        const std::wstring first = line.substr(0, space);
        if (IsVersionNumber(first)) {
            std::wstring label = space == std::wstring::npos ? L"" : Text::Trim(line.substr(space + 1));
            std::wstring date;
            if (!label.empty() && label.front() == L'|') {
                date = Text::Trim(label.substr(1));
                label.clear();
            }
            if (label.size() >= 2 && label.front() == L'[' && label.back() == L']') label = label.substr(1, label.size() - 2);
            all.push_back({ first, label, date, {} });
            continue;
        }
        if (all.empty()) all.push_back({ latest, L"", L"", {} });
        all.back().lines.push_back(line);
    }

    std::vector<Raw> kept;
    if (history) kept = all;
    else
        for (const auto& e : all)
        if (Versions::IsNewer(current, e.version) && !Versions::IsNewer(latest, e.version)) kept.push_back(e);
    if (kept.empty())
        for (const auto& e : all)
            if (SameVersion(e.version, latest)) kept.push_back(e);
    if (kept.empty() && !all.empty()) kept.push_back(all.front());

    const float textW = width - kPadX * 2.f - kIndent;
    const TextStyle body = Body();
    cards.clear();
    for (const auto& e : kept) {
        Card card;
        card.version = e.version;
        card.label = e.label;
        card.date = e.date;
        float cy = 12.f;
        for (const auto& raw : e.lines) {
            const bool heading = raw.rfind(L"## ", 0) == 0;
            if (heading || (raw.size() > 2 && raw.front() == L'=' && raw.back() == L'=')) {
                const size_t a = raw.find_first_not_of(L'='), b = raw.find_last_not_of(L'=');
                const std::wstring title = heading ? Text::Trim(raw.substr(3)) : a == std::wstring::npos ? L"" : Text::Trim(raw.substr(a, b - a + 1));
                if (!card.sections.empty()) cy += kSectionGap - kLineGap;
                card.sections.push_back({ title, cy, {} });
                cy += kTagH + 8.f;
                continue;
            }
            if (card.sections.empty()) card.sections.push_back({ L"", cy, {} });
            Line line;
            line.text = raw.rfind(L"- ", 0) == 0 || raw.rfind(L"* ", 0) == 0 || raw.rfind(L"\x2022 ", 0) == 0 ? Text::Trim(raw.substr(2)) : raw;
            const size_t prefix = PrefixLength(line.text);
            if (prefix) line.spans.push_back({ 0, prefix, DWRITE_FONT_WEIGHT_BOLD, Readable(LookOf(card.sections.back().title).color) });
            line.y = cy;
            line.h = std::ceil(ui.g.MeasureRich(line.text, body, line.spans, textW).h);
            cy += line.h + kLineGap;
            card.sections.back().lines.push_back(std::move(line));
            ++card.changes;
        }
        card.bodyH = cy - kLineGap + 16.f;
        cards.push_back(std::move(card));
    }
    // A new changelog: only the newest version starts open
    if (changelog != source || current != sourceCurrent || latest != sourceLatest || history != builtHistory) {
        open.assign(cards.size(), false);
        if (!open.empty()) open[0] = true;
        shownAt = ui.Time();
    }
    source = changelog;
    sourceCurrent = current;
    sourceLatest = latest;
    builtWidth = width;
    builtHistory = history;
    builtRevision = M::Revision();
}

void ChangelogCards::Draw(UI::Ui& ui, const Rect& r, const std::wstring& changelog, const std::wstring& current, const std::wstring& latest) {
    history = false;
    DrawList(ui, r, changelog, current, latest);
}

void ChangelogCards::DrawHistory(UI::Ui& ui, const Rect& r, const std::wstring& changelog, const std::wstring& installed) {
    history = true;
    DrawList(ui, r, changelog, installed, installed);
}

void ChangelogCards::DrawList(UI::Ui& ui, const Rect& r, const std::wstring& changelog, const std::wstring& current, const std::wstring& latest) {
    const M::Palette& p = M::P();
    const Color fg = p.foreground;

    // "WHAT'S NEW" · "2 new versions · v3.0.0 → v3.0.1"
    TextStyle overline = M::Text(11.f, DWRITE_FONT_WEIGHT_BOLD);
    overline.valign = VAlign::Center;
    const Rect label{ r.x + 4.f, r.y, r.w - 8.f, 22.f };
    M::DrawIcon(ui.g, history ? M::Icon::ClipboardTextClockOutline : M::Icon::Star, { label.x, label.CY() - 8.f, 16.f, 16.f }, p.primaryMid.color);
    ui.g.Text(Loc::Get(history ? "changelog_version_history" : "changelog_whats_new"), { label.x + 22.f, label.y, label.w - 22.f, label.h }, p.primaryMid.color, overline);

    const Rect list{ r.x, label.B() + 10.f, r.w, std::max(0.f, r.B() - label.B() - 10.f) };
    const UI::Id scrollId = ui.MakeId(key + "-scroll");
    const bool changed = changelog != source || current != sourceCurrent || latest != sourceLatest || history != builtHistory;
    if (changed) UI::ScrollArea::ScrollTo(ui, scrollId, 0.f);
    {
        UI::ScrollArea scroll(ui, scrollId, list, false, 48.f, UI::ScrollArea::Bar::Material);
        const float listW = scroll.ContentWidth() - 6.f;
        if (changed || std::fabs(listW - builtWidth) > 0.5f ||
            builtRevision != M::Revision())
            Build(ui, changelog, current, latest, listW);
        // Cards outside the viewport only advance their fold (the history is long)
        viewTop = list.y + scroll.Offset();
        viewBottom = viewTop + list.h;
        float y = list.y + 2.f;
        for (size_t i = 0; i < cards.size(); ++i) {
            M::Motion::Enter enter(ui, M::Motion::Stagger(ui, shownAt, static_cast<int>(i), 0.06f, 0.4f), 14.f);
            y += DrawCard(ui, i, list.x + 2.f, y, listW - 2.f) + kGap;
        }
        scroll.SetContentSize(y - list.y);
    }
    // The summary, next to the title (outside the list's clip)
    if (!cards.empty()) {
        const std::wstring to = WithV(cards.front().version);
        std::wstring summary;
        if (history) summary = Loc::Fmt(cards.size() == 1 ? "changelog_versions_one" : "changelog_versions", cards.size(), WithV(cards.back().version), to);
        else if (cards.size() > 1) summary = Loc::Fmt("changelog_new_versions", cards.size(), WithV(current), to);
        else if (!SameVersion(current, cards.front().version)) summary = WithV(current) + L"  \x2192  " + to;
        TextStyle right = M::Text(11.f);
        right.align = HAlign::Right;
        right.valign = VAlign::Center;
        ui.g.Text(summary, label, fg.Fade(0.55f), right);
    }
}

// Header (chevron · version · LATEST · label · per-section counts ······ N changes), click to fold /
// unfold; the body grows open with the sections and their changes. Returns the card's height.
float ChangelogCards::DrawCard(UI::Ui& ui, size_t index, float x, float y, float w) {
    const M::Palette& p = M::P();
    const Color fg = p.foreground;
    const Card& card = cards[index];
    const bool newest = index == 0;
    const int id = static_cast<int>(index);
    const float o = ui.Animate(ui.MakeId(key + "-card-open", id), open[index] ? 1.f : 0.f, 12.f);
    const bool folding = o > 0.001f && o < 0.999f;
    if (folding) ui.KeepAnimating();
    const float h = kHeaderH + card.bodyH * o;
    if (y > viewBottom || y + h < viewTop) return h;
    const Rect r{ x, y, w, h };
    const Rect header{ x, y, w, kHeaderH };

    const UI::Interaction it = ui.Interact(ui.MakeId(key + "-card-head", id), header);
    if (it.hovered) ui.SetCursor(IDC_HAND);
    if (it.clicked) open[index] = !open[index];
    const float hover = M::Motion::Hover(ui, ui.MakeId(key + "-card-head", id), it.hovered, it.held).hover;
    const Color accent = newest ? p.primaryMid.color : fg.Fade(0.45f);
    const Color bg = newest ? Lerp(p.cardBackground, p.primaryMid.color, 0.08f + 0.05f * hover) : Lerp(p.cardBackground, fg, 0.035f + 0.035f * hover);
    const Color border = newest ? p.primaryMid.color.Fade(0.45f + 0.3f * hover) : Lerp(p.divider, fg.Fade(0.3f), hover);
    if (newest && !folding) ui.g.GlowRound(r, 10.f, p.primaryMid.color, 18.f, 0.16f + 0.12f * hover);
    ui.g.Border(r, 10.f, bg, border, 1.f);

    // ── Header ──
    const float cy = header.CY();
    {
        const float cx = x + 22.f;
        ui.g.PushTransform(D2D1::Matrix3x2F::Rotation(90.f * o, D2D1::Point2F(cx, cy)));
        M::DrawIcon(ui.g, M::Icon::ChevronRight, cx, cy, 18.f, Lerp(accent, fg, 0.3f * hover));
        ui.g.PopTransform();
    }
    float px = x + 40.f;
    const std::wstring version = WithV(card.version);
    const TextStyle versionStyle = Middle(15.f, DWRITE_FONT_WEIGHT_BOLD);
    const float versionW = std::ceil(ui.g.Measure(version, versionStyle).w) + 2.f;
    ui.g.Text(version, { px, header.y, versionW, header.h }, newest ? p.primaryMid.color : fg.Fade(0.85f), versionStyle);
    px += versionW + 10.f;
    const auto Pill = [&](const std::wstring& text, Color color) {
        const TextStyle style = Centered(9.f, DWRITE_FONT_WEIGHT_BOLD);
        const float pw = std::ceil(ui.g.Measure(text, style).w) + 16.f;
        const Rect pr{ px, cy - 10.f, pw, 20.f };
        ui.g.Border(pr, 10.f, color.Fade(0.16f), color.Fade(0.55f), 1.f);
        ui.g.Text(text, pr, Readable(color), style);
        px += pw + 8.f;
    };
    if (newest) Pill(Loc::Get("changelog_latest"), p.primaryMid.color);
    if (history && SameVersion(card.version, sourceCurrent)) Pill(Loc::Get("changelog_installed"), Argb(0xFF4CAF50));
    if (!card.label.empty()) Pill(card.label, p.secondaryMid.color);

    // "N changes" on the right
    float right = header.R() - kPadX;
    {
        const std::wstring text = Loc::Fmt(card.changes == 1 ? "changelog_change_one" : "changelog_change_many", card.changes);
        const TextStyle style = Middle(11.f);
        const float tw = std::ceil(ui.g.Measure(text, style).w) + 2.f;
        ui.g.Text(text, { right - tw, header.y, tw, header.h }, fg.Fade(0.5f), style);
        right -= tw + 12.f;
    }
    // Release date (when the changelog has one), with a clock before it
    if (!card.date.empty()) {
        const TextStyle style = Middle(11.f);
        const float dw = std::ceil(ui.g.Measure(card.date, style).w) + 2.f;
        ui.g.Text(card.date, { right - dw, header.y, dw, header.h }, fg.Fade(0.55f), style);
        M::DrawIcon(ui.g, M::Icon::ClockOutline, right - dw - 12.f, cy, 13.f, fg.Fade(0.55f));
        right -= dw + 32.f;
    }
    // How many changes of each section, as far as they fit
    const TextStyle countStyle = Middle(10.f, DWRITE_FONT_WEIGHT_BOLD);
    for (size_t s = 0; s < card.sections.size(); ++s) {
        const Section& section = card.sections[s];
        if (section.lines.empty()) continue;
        const SectionLook look = LookOf(section.title);
        const std::wstring count = std::to_wstring(section.lines.size());
        const float chipW = 8.f + 12.f + 4.f + std::ceil(ui.g.Measure(count, countStyle).w) + 8.f;
        if (px + chipW > right) break;
        const Rect chip{ px, cy - 10.f, chipW, 20.f };
        ui.g.Border(chip, 10.f, look.color.Fade(0.12f), look.color.Fade(0.4f), 1.f);
        M::DrawIcon(ui.g, look.icon, chip.x + 7.f + 6.f, chip.CY(), 12.f, Readable(look.color));
        ui.g.Text(count, { chip.x + 23.f, chip.y, chipW - 23.f, chip.h }, Readable(look.color), countStyle);
        const UI::Id chipId = UI::HashId(static_cast<int64_t>(s), ui.MakeId(key + "-card-chip", id));
        if (ui.Interact(chipId, chip, true, false).clicked) open[index] = !open[index];
        ui.Tooltip(chipId, section.title.empty() ? Loc::Get("changelog_changes") : SectionName(section.title));
        px += chipW + 6.f;
    }

    // ── Body (clipped while it folds) ──
    if (o > 0.01f) {
        const Rect bodyR{ x, header.B(), w, h - kHeaderH };
        ui.PushClip(bodyR);
        ui.g.PushOpacity(std::min(1.f, o * 1.4f));
        ui.g.FillRect({ x + kPadX, header.B(), w - kPadX * 2.f, 1.f }, accent.Fade(0.25f));
        const float textX = x + kPadX + kIndent;
        const float textW = w - kPadX * 2.f - kIndent;
        const TextStyle body = Body();
        const float lineH = std::ceil(ui.g.Measure(L"Ag", body).h);
        for (const Section& section : card.sections) {
            const SectionLook look = LookOf(section.title);
            if (!section.title.empty()) {
                const TextStyle tagStyle = Middle(9.5f, DWRITE_FONT_WEIGHT_BOLD);
                const std::wstring tagText = Text::Upper(SectionName(section.title));
                const float tw = std::ceil(ui.g.Measure(tagText, tagStyle).w);
                const Rect tag{ x + kPadX, header.B() + section.y, tw + 34.f, kTagH };
                ui.g.Border(tag, 6.f, look.color.Fade(0.14f), look.color.Fade(0.45f), 1.f);
                M::DrawIcon(ui.g, look.icon, tag.x + 8.f + 7.f, tag.CY(), 13.f, Readable(look.color));
                ui.g.Text(tagText, { tag.x + 25.f, tag.y, tw + 4.f, tag.h }, Readable(look.color), tagStyle);
            }
            for (const Line& line : section.lines) {
                const float ly = header.B() + line.y;
                if (ly > bodyR.B() || ly > viewBottom) break;
                if (ly + line.h < viewTop) continue;
                ui.g.FillEllipse(x + kPadX + 5.f, ly + lineH * 0.5f + 0.5f, 2.6f, 2.6f, look.color.Fade(0.85f));
                ui.g.RichText(line.text, { textX, ly, textW, line.h + 2.f }, fg.Fade(0.88f), body, line.spans);
            }
        }
        ui.g.PopLayer();
        ui.PopClip();
    }
    return h;
}

} // namespace SonicRacingSaveManager::Main
