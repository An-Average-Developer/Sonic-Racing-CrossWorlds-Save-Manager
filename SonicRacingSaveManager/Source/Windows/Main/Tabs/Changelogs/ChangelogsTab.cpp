#include "Windows/Main/Tabs/Changelogs/ChangelogsTab.h"
#include <algorithm>
#include <cmath>
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "Services/App/ChangelogData.h"
#include "UI/Material/Card.h"
#include "UI/Material/Motion.h"
#include "UI/Material/Theme.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Tabs/TabKit.h"
#include "Windows/Main/Tabs/TabBanner.h"

namespace SonicRacingSaveManager::Main {

namespace M = UI::Material;
using namespace TabKit;

namespace {

constexpr float kRightW = 360.f, kGap = 16.f, kRadius = 16.f;

TextStyle Caps() {
    TextStyle t = Style(11.f, DWRITE_FONT_WEIGHT_BOLD);
    t.valign = VAlign::Center;
    return t;
}

} // namespace

// Counts the changes of every "## " section across the history
void ChangelogsTab::Summarize() {
    summarized = true;
    const auto& entries = Services::Changelog::Entries();
    versions = static_cast<int>(entries.size());
    if (!entries.empty()) {
        latestVersion = entries.front().version;
        latestLabel = entries.front().label;
        firstVersion = entries.back().version;
    }
    for (const auto& entry : entries) {
        std::wstring section;
        for (const auto& line : entry.lines) {
            if (line.rfind(L"## ", 0) == 0) {
                section = line.substr(3);
                continue;
            }
            if (line.rfind(L"- ", 0) != 0 || section.empty()) continue;
            ++changes;
            auto it = std::find_if(types.begin(), types.end(), [&](const TypeCount& t) { return t.title == section; });
            if (it == types.end()) types.push_back({ section, 1 });
            else ++it->count;
        }
    }
    std::stable_sort(types.begin(), types.end(), [](const TypeCount& a, const TypeCount& b) { return a.count > b.count; });
}

void ChangelogsTab::Draw(UI::Ui& ui, const Rect& area) {
    if (loadedAt < 0.0) loadedAt = ui.Time();
    if (!summarized) Summarize();
    if (text.empty()) text = Services::Changelog::EmbeddedText();
    const Rect grid = area.Inset(20.f);

    const Rect banner{ grid.x, grid.y, grid.w, kTabBannerHeight };
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt, 0, 0.f, 0.5f), 14.f);
        TabBannerSpec spec;
        spec.icon = M::Icon::FileDocumentOutline;
        spec.motion = TabBannerSpec::Motion::Float;
        spec.title = Loc::Get("changelogs_title");
        spec.subtitle = Loc::Get("changelogs_subtitle");
        spec.pill = Loc::Fmt(versions == 1 ? "changelogs_pill_one" : "changelogs_pill", versions, changes);
        spec.pillIcon = M::Icon::ClipboardTextClockOutline;
        TabBanner(ui, banner, spec, loadedAt);
    }

    const float top = banner.B() + kGap;
    const Rect left{ grid.x, top, grid.w - kRightW - kGap, grid.B() - top };
    const Rect right{ left.R() + kGap, top, kRightW, grid.B() - top };
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.1, 0, 0.f, 0.5f), 18.f);
        M::CardLook look;
        look.radius = kRadius;
        M::Card card(ui, ui.MakeId("chl-card"), left, Edges(20.f), false, 1.f, look);
        cards.DrawHistory(ui, card.content, text, main.State().currentVersion);
    }
    const Rect glance{ right.x, right.y, right.w, Glance(ui, { right.x, right.y, right.w, 0.f }, false) };
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.18, 0, 0.f, 0.5f), 0.f, 28.f);
        Glance(ui, glance, true);
    }
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.26, 0, 0.f, 0.5f), 0.f, 28.f);
        ByType(ui, { right.x, glance.B() + kGap, right.w, right.B() - glance.B() - kGap });
    }
}

// Versions and changes (counting up), installed / latest version pills
float ChangelogsTab::Glance(UI::Ui& ui, const Rect& r, bool draw) {
    constexpr float kPad = 20.f, kH = kPad * 2.f + 20.f + 12.f + 64.f + 14.f + 2.f * 40.f + 8.f;
    if (!draw) return kH;
    const M::Palette& p = M::P();
    const Color fg = p.foreground;
    M::CardLook look;
    look.radius = kRadius;
    M::Card card(ui, ui.MakeId("chl-glance"), r, Edges(kPad), false, 1.f, look);
    const Rect c = card.content;
    ui.g.Text(Loc::Get("changelogs_glance"), { c.x, c.y, c.w, 20.f }, p.primaryMid.color, Caps());
    float y = c.y + 32.f;

    // Two big numbers counting up
    const float k = M::Motion::Stagger(ui, loadedAt + 0.3, 0, 0.f, 1.f);
    const auto Stat = [&](float x, float w, int value, const std::wstring& label, M::Icon icon) {
        const Rect box{ x, y, w, 64.f };
        ui.g.FillRound(box, 12.f, p.primaryMid.color.Fade(0.08f));
        M::DrawIcon(ui.g, icon, box.x + 22.f, box.CY(), 22.f, p.primaryMid.color);
        TextStyle number = Style(24.f, DWRITE_FONT_WEIGHT_BOLD);
        number.valign = VAlign::Center;
        ui.g.Text(std::to_wstring(static_cast<int>(std::lround(value * k))), { box.x + 44.f, box.y + 6.f, w - 50.f, 32.f }, fg, number);
        TextStyle caption = Style(11.f);
        caption.valign = VAlign::Center;
        ui.g.Text(label, { box.x + 44.f, box.y + 36.f, w - 50.f, 18.f }, fg.Fade(0.6f), caption);
    };
    const float half = (c.w - 10.f) * 0.5f;
    Stat(c.x, half, versions, Loc::Get(versions == 1 ? "changelogs_versions_one" : "changelogs_versions"), M::Icon::TrophyAward);
    Stat(c.x + half + 10.f, half, changes, Loc::Get("changelogs_changes"), M::Icon::FileMultiple);
    y += 64.f + 14.f;

    // Installed / latest rows
    const auto Row = [&](const std::wstring& label, const std::wstring& value, const std::wstring& tag, Color tagColor) {
        TextStyle name = Style(12.f);
        name.valign = VAlign::Center;
        ui.g.Text(label, { c.x, y, 120.f, 32.f }, fg.Fade(0.65f), name);
        TextStyle v = Style(14.f, DWRITE_FONT_WEIGHT_BOLD);
        v.valign = VAlign::Center;
        const float vw = std::ceil(ui.g.Measure(value, v).w) + 2.f;
        float x = c.R() - vw;
        if (!tag.empty()) {
            TextStyle t = Style(9.f, DWRITE_FONT_WEIGHT_BOLD, false, HAlign::Center);
            t.valign = VAlign::Center;
            const float tw = std::ceil(ui.g.Measure(tag, t).w) + 14.f;
            x -= tw + 8.f;
            const Rect pill{ x, y + 7.f, tw, 18.f };
            ui.g.Border(pill, 9.f, tagColor.Fade(0.16f), tagColor.Fade(0.55f), 1.f);
            ui.g.Text(tag, pill, ChangelogCards::ReadableColor(tagColor), t);
            x = pill.R() + 8.f;
        }
        ui.g.Text(value, { x, y, vw, 32.f }, fg, v);
        y += 40.f;
    };
    std::wstring installed = main.State().currentVersion;
    Row(Loc::Get("updates_installed"), installed, Loc::Get("changelog_installed"), Argb(0xFF4CAF50));
    std::wstring label = latestLabel;
    if (label.size() >= 2 && label.front() == L'[' && label.back() == L']') label = label.substr(1, label.size() - 2);
    Row(Loc::Get("changelogs_latest"), L"v" + latestVersion, label.empty() ? Loc::Get("changelog_latest") : label, p.primaryMid.color);
    if (k < 1.f) ui.KeepAnimating();
    return kH;
}

// One bar per type of change, growing in turn from the left
void ChangelogsTab::ByType(UI::Ui& ui, const Rect& r) {
    const M::Palette& p = M::P();
    const Color fg = p.foreground;
    M::CardLook look;
    look.radius = kRadius;
    M::Card card(ui, ui.MakeId("chl-types"), r, Edges(20.f), false, 1.f, look);
    const Rect c = card.content;
    ui.g.Text(Loc::Get("changelogs_by_type"), { c.x, c.y, c.w, 20.f }, p.primaryMid.color, Caps());
    float y = c.y + 34.f;
    const int most = types.empty() ? 1 : std::max(1, types.front().count);
    TextStyle name = Style(12.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    name.valign = VAlign::Center;
    TextStyle count = Style(12.f, DWRITE_FONT_WEIGHT_BOLD, false, HAlign::Right);
    count.valign = VAlign::Center;
    for (size_t i = 0; i < types.size() && y + 42.f <= c.B(); ++i) {
        const TypeCount& type = types[i];
        const Color color = ChangelogCards::SectionColor(type.title);
        const float grow = M::Motion::Stagger(ui, loadedAt + 0.4, static_cast<int>(i), 0.08f, 0.7f);
        const UI::Id id = ui.MakeId("chl-type", static_cast<int64_t>(i));
        const Rect row{ c.x, y, c.w, 38.f };
        const float hover = M::Motion::Hover(ui, id, ui.MouseOver(row), false).hover;
        if (hover > 0.f) ui.g.FillRound(row.Outset(Edges(6.f, 2.f)), 10.f, color.Fade(0.08f * hover));
        M::DrawIcon(ui.g, ChangelogCards::SectionIcon(type.title), c.x + 10.f, y + 10.f, 16.f * (1.f + 0.15f * hover), ChangelogCards::ReadableColor(color));
        ui.g.Text(ChangelogCards::SectionName(type.title), { c.x + 26.f, y, c.w - 80.f, 20.f }, fg.Fade(0.9f), name);
        ui.g.Text(std::to_wstring(static_cast<int>(std::lround(type.count * grow))), { c.R() - 60.f, y, 60.f, 20.f }, ChangelogCards::ReadableColor(color), count);
        const Rect track{ c.x, y + 24.f, c.w, 8.f };
        ui.g.FillRound(track, 4.f, fg.Fade(0.07f));
        const float w = track.w * static_cast<float>(type.count) / static_cast<float>(most) * grow;
        if (w > 0.5f) ui.g.FillRound({ track.x, track.y, std::max(8.f, w), track.h }, 4.f, color);
        y += 44.f;
    }
}

} // namespace SonicRacingSaveManager::Main
