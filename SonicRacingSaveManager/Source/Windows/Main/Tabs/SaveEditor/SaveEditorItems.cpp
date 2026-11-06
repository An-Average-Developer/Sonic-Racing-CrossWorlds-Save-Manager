#include <algorithm>
#include <cmath>
#include <functional>
#include <unordered_map>
#include "Data/ColorCatalog.h"
#include "Data/SaveCatalog.h"
#include "Data/SaveNames.h"
#include "Data/StickerCatalog.h"
#include "App/Paths.h"
#include "Core/Text.h"
#include "Graphics/Images.h"
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "UI/Material/Buttons.h"
#include "UI/Material/Fields.h"
#include "UI/Material/Motion.h"
#include "UI/Material/Theme.h"
#include "UI/Material/Toggles.h"
#include "UI/Widgets/ScrollArea.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Tabs/Backup/BackupKit.h"
#include "Windows/Main/Tabs/SaveEditor/SaveEditorTab.h"
#include "Windows/Main/Tabs/SaveEditor/SaveEditorVariants.h"
#include "Windows/Main/Tabs/TabKit.h"

// A category of the Save Editor: its bar (title, count, search, filter, Unlock all) over its items,
// as picture tiles when the game's pictures are cached, as rows otherwise, or the profile's values.
// The versions of a character share one tile / row ("unlocked / total"), which opens them in a card;
// clicking an item changes it, and it plays a little burst.
namespace SonicRacingSaveManager::Main {

namespace M = UI::Material;
namespace SF = Services::SaveFile;
using namespace SaveEditorKit;
using TabKit::Style;
using SF::ItemState;
using SF::ListId;

const std::vector<SaveEditorTab::Category>& SaveEditorTab::Categories() {
    using I = M::Icon;
    static const std::vector<Category> list = {
        { 0, true, ListId::Count, I::AccountStar, "saveeditor_cat_profile" },
        { 1, false, ListId::Drivers, I::Account, "saveeditor_cat_drivers" },
        { 1, false, ListId::Machines, I::CarSports, "saveeditor_cat_machines" },
        { 1, false, ListId::FrontParts, I::CarCog, "saveeditor_cat_front" },
        { 1, false, ListId::RearParts, I::Wrench, "saveeditor_cat_rear" },
        { 1, false, ListId::TireParts, I::Tire, "saveeditor_cat_tires" },
        { 2, false, ListId::ColorPresets, I::Palette, "saveeditor_cat_colors" },
        { 2, false, ListId::Stickers, I::Sticker, "saveeditor_cat_stickers" },
        { 2, false, ListId::Horns, I::Bullhorn, "saveeditor_cat_horns" },
        { 2, false, ListId::Auras, I::Creation, "saveeditor_cat_auras" },
        { 2, false, ListId::Gadgets, I::Flash, "saveeditor_cat_gadgets" },
        { 3, false, ListId::GrandPrix, I::Trophy, "saveeditor_cat_grandprix" },
        { 3, false, ListId::TimeTrial, I::TimerOutline, "saveeditor_cat_timetrial" },
        { 3, false, ListId::RedStarRings, I::StarFourPoints, "saveeditor_cat_redrings" },
        { 3, false, ListId::Challenges, I::FlagCheckered, "saveeditor_cat_challenges" },
        { 3, false, ListId::HonorTitles, I::Medal, "saveeditor_cat_titles" },
        { 3, false, ListId::Albums, I::Album, "saveeditor_cat_albums" },
        { 3, false, ListId::Tracks, I::MusicNote, "saveeditor_cat_tracks" },
        { 3, false, ListId::Tutorials, I::School, "saveeditor_cat_tutorials" },
    };
    return list;
}

int SaveEditorTab::CategoryOf(ListId list) {
    for (size_t k = 0; k < Categories().size(); ++k)
        if (!Categories()[k].numbers && Categories()[k].list == list) return static_cast<int>(k);
    return 0;
}

const std::vector<SF::ItemRow>& SaveEditorTab::Rows() {
    const std::string key = std::to_string(category) + "|" + std::to_string(version);
    if (key == rowsKey) return rows;
    rowsKey = key;
    rows.clear();
    if (save && category >= 0 && !Current().numbers) rows = save->Rows(Current().list);
    return rows;
}

const std::vector<size_t>& SaveEditorTab::Visible() {
    const auto& all = Rows();
    const std::string key = rowsKey + "|" + std::to_string(filter) + "|" + Text::Narrow(search);
    if (key == visibleKey) return visible;
    visibleKey = key;
    visible.clear();
    const std::wstring q = Text::Trim(search);
    for (size_t i = 0; i < all.size(); ++i) {
        const auto& r = all[i];
        if (filter == 1 && r.state != ItemState::Unlocked) continue;
        if (filter == 2 && r.state != ItemState::Locked) continue;
        if (filter == 3 && r.state != ItemState::Missing) continue;
        if (!q.empty() && !Text::Contains(r.name, q, true) && !Text::Contains(r.id, q, true)) continue;
        visible.push_back(i);
    }
    return visible;
}

const std::vector<SaveEditorTab::Family>& SaveEditorTab::Families() {
    const auto& all = Rows();
    if (familiesKey == rowsKey) return families;
    familiesKey = rowsKey;
    families.clear();
    const bool characters = category >= 0 && !Current().numbers && Current().list == ListId::Drivers;
    const bool titles = category >= 0 && !Current().numbers && Current().list == ListId::HonorTitles;
    const bool stickers = category >= 0 && !Current().numbers && Current().list == ListId::Stickers;
    const auto titleOf = [](const std::wstring& id) { return static_cast<int>(Text::ToInt(id).value_or(-1)); };
    // A sticker pack's key: "pack:<n>" (no sticker has that id); the base game's that share a name (the
    // 51 "SONIC ART") go together too, as "name:<name>"
    std::unordered_map<std::wstring, int> sameName;
    if (stickers)
        for (const auto& row : all)
            if (!Data::StickerCatalog::Pack(titleOf(row.id))) ++sameName[row.name];
    const auto stickerKey = [&](const SF::ItemRow& row) {
        const int pack = Data::StickerCatalog::Pack(titleOf(row.id));
        if (pack) return L"pack:" + std::to_wstring(pack);
        return sameName[row.name] >= 3 ? L"name:" + row.name : row.id;
    };
    std::unordered_map<std::wstring, size_t> at;
    for (size_t i = 0; i < all.size(); ++i) {
        const std::wstring key = characters ? Data::SaveCatalog::DriverFamily(all[i].id)
                                 : titles   ? std::to_wstring(Data::SaveCatalog::TitleFamily(titleOf(all[i].id)))
                                 : stickers ? stickerKey(all[i])
                                            : all[i].id;
        const auto [it, added] = at.try_emplace(key, families.size());
        if (added) families.push_back({ key, {}, 0 });
        Family& f = families[it->second];
        // The character itself first: it names the family and gives its picture
        if (all[i].id == key) f.members.insert(f.members.begin(), i);
        else f.members.push_back(i);
        if (all[i].state == ItemState::Unlocked) ++f.done;
    }
    // The titles by character first, then the festivals, the World Championship and the rest
    if (titles)
        std::stable_sort(families.begin(), families.end(), [&](const Family& a, const Family& b) {
            return Data::SaveCatalog::TitleGroup(titleOf(a.id)) < Data::SaveCatalog::TitleGroup(titleOf(b.id));
        });
    // The stickers by kind: characters, designs, events, competitions, brands
    if (stickers)
        std::stable_sort(families.begin(), families.end(), [&](const Family& a, const Family& b) {
            const auto kind = [&](const Family& f) { return static_cast<int>(Data::StickerCatalog::GroupOf(titleOf(all[f.members.front()].id))); };
            return kind(a) < kind(b);
        });
    return families;
}

std::wstring SaveEditorTab::FamilyName(const Family& f) {
    if (f.id.starts_with(L"name:")) return f.id.substr(5);
    if (f.id.starts_with(L"pack:"))
        if (const wchar_t* name = Data::SaveNames::Find("sticker" + Text::Narrow(f.id), Loc::CurrentLang())) return name;
    // Season 2's title groups have no title that names them
    if (f.members.size() > 1 && Current().list == ListId::HonorTitles) {
        if (f.id == L"9001") return Loc::Get("saveeditor_titles_shop");
        if (f.id == L"9900") return Loc::Get("saveeditor_titles_friends");
        if (f.id == L"6001") return Loc::Get("saveeditor_titles_season2");
    }
    return Rows()[f.members.front()].name;
}

const std::vector<size_t>& SaveEditorTab::ShownFamilies() {
    const auto& fams = Families();
    const auto& shown = Visible();
    if (visibleKey == shownKey) return shownFamilies;
    shownKey = visibleKey;
    shownFamilies.clear();
    std::vector<bool> on(Rows().size(), false);
    for (size_t i : shown) on[i] = true;
    for (size_t k = 0; k < fams.size(); ++k)
        if (std::any_of(fams[k].members.begin(), fams[k].members.end(), [&](size_t i) { return on[i]; })) shownFamilies.push_back(k);
    return shownFamilies;
}

SaveEditorTab::TileLook SaveEditorTab::FamilyLook(const std::vector<SF::ItemRow>& all, const Family& f, double now) const {
    const SF::ItemRow& first = all[f.members.front()];
    const int total = static_cast<int>(f.members.size());
    TileLook look;
    look.state = f.done > 0 ? ItemState::Unlocked : first.state;
    look.chip = std::to_wstring(f.done) + L" / " + std::to_wstring(total);
    look.chipTone = f.done == total ? kGreen : f.done > 0 ? kAmber : M::P().foreground.Fade(0.55f);
    // DLC (Steam gives it from your purchases): said on the tile too, as in the cards; one that is all
    // DLC is just "DLC", a family with some ("Sonic" and the Werehog) adds it to its count
    const size_t dlc = static_cast<size_t>(std::count_if(f.members.begin(), f.members.end(), [&](size_t i) { return all[i].dlc; }));
    if (dlc == f.members.size()) {
        look.chip = total > 1 ? Loc::Get("savefile_dlc") + L"  " + look.chip : Loc::Get("savefile_dlc");
        look.chipTone = kAmber;
    } else if (dlc > 0) look.chip += L"  \u00B7  " + Loc::Get("savefile_dlc");
    // Several versions: a click opens them; one: a click changes it (never a DLC one)
    look.can = total > 1 || (CanToggle(first) && !first.dlc && !busy);
    for (size_t i : f.members) look.burst = std::max(look.burst, bursts.Progress(all[i].id, now));
    return look;
}

const std::vector<SaveEditorTab::Stat>& SaveEditorTab::Stats() {
    if (statsFor == version) return stats;
    statsFor = version;
    stats.assign(Categories().size(), {});
    if (!save) return stats;
    for (size_t k = 0; k < Categories().size(); ++k) {
        const Category& c = Categories()[k];
        if (c.numbers) continue;
        for (const auto& r : save->Rows(c.list)) {
            ++stats[k].total;
            if (r.state != ItemState::Unlocked) continue;
            ++stats[k].done;
            // The first unlocked one with a picture stands for the category
            if (stats[k].cover.empty() && Picture(c.list, r.id)) stats[k].cover = r.id;
        }
    }
    return stats;
}

void SaveEditorTab::ToggleItem(UI::Ui& ui, const SF::ItemRow& row) {
    if (!CanToggle(row) || busy) return;
    const std::wstring id = row.id;
    const bool unlocked = row.state == ItemState::Unlocked;
    const ListId list = Current().list;
    bool changed = false;
    Edit([&] { changed = unlocked ? save->Lock(list, id) : save->Unlock(list, id); });
    if (changed) bursts.Fire(id, ui.Time());
}

void SaveEditorTab::UnlockCategory(UI::Ui& ui) {
    if (busy || category < 0 || Current().numbers) return;
    // The ones about to change, in the order shown, for the wave
    std::vector<std::wstring> waves;
    for (size_t i : Visible())
        if (Rows()[i].state != ItemState::Unlocked && Rows()[i].canUnlock) waves.push_back(Rows()[i].id);
    int changed = 0;
    const ListId list = Current().list;
    Edit([&] { changed = save->UnlockAll(list); });
    const double now = ui.Time();
    for (size_t k = 0; k < waves.size(); ++k) bursts.Fire(waves[k], now + std::min<size_t>(k, 60) * 0.018);
    animateUntil = now + std::min<size_t>(waves.size(), 60) * 0.018 + kBurstSeconds;
    Status(Loc::Fmt("saveeditor_unlocked_n", std::to_wstring(changed)));
}

void SaveEditorTab::CategoryView(UI::Ui& ui, const Rect& r) {
    if (ui.Time() < animateUntil) ui.KeepAnimating();
    const Rect bar{ r.x, r.y, r.w, kBarH };
    Bar(ui, bar);
    const Rect content{ r.x, bar.B() + kGap, r.w, r.B() - bar.B() - kGap };
    if (Current().numbers) Numbers(ui, content);
    else if (Current().list == ListId::HonorTitles) Banners(ui, content);
    else if (HasPictures(Current().list)) Tiles(ui, content);
    else List(ui, content);
}

// ── Bar ─────────────────────────────────────────────────────────────────────────────────────────

// The category's icon, name and count (with a bar), then on the right the search box, the filter (a segmented control whose highlight slides) and Unlock all
void SaveEditorTab::Bar(UI::Ui& ui, const Rect& r) {
    const M::Palette& p = M::P();
    const Category& c = Current();
    const Color tone = GroupColor(c.group);
    Card(ui, r, kRadius, tone, 0.f);
    ui.g.PushRoundClip(r, kRadius);
    ui.g.FillLinear(r, 0.f, { { 0.f, tone.Fade(0.16f) }, { 0.45f, Colors::Transparent } }, { r.x, r.y }, { r.R(), r.y });
    ui.g.PopLayer();
    const float cy = r.CY();

    float x = r.x + 14.f;
    ui.g.FillEllipse(x + 18.f, cy, 18.f, 18.f, tone.Fade(0.2f));
    M::DrawIcon(ui.g, c.icon, x + 18.f, cy, 20.f, TabKit::Readable(tone));
    x += 46.f;

    // Right side, from the right
    float right = r.R() - 12.f;
    if (c.numbers) {
        const std::wstring& maxAll = Loc::Get("saveeditor_max_all");
        const float mw = TabKit::PillWidth(ui, maxAll) + 10.f;
        right -= mw;
        if (TabKit::PillButton(ui, ui.MakeId("se-max-all"), { right, cy - 19.f, mw, 38.f }, M::Icon::ArrowUp, maxAll, tone, true, !busy))
            Edit([&] {
                for (const auto& f : save->Numbers())
                    if (f.suggested > 0) save->SetNumber(f.key, f.suggested);
            });
    } else {
        const auto& all = Rows();
        int n[3] = {};
        for (const auto& row : all) ++n[static_cast<int>(row.state)];
        const std::wstring& unlockAll = Loc::Get("saveeditor_unlock_all_short");
        const float uw = TabKit::PillWidth(ui, unlockAll) + 10.f;
        right -= uw;
        bool anyLeft = false;
        for (const auto& row : all) anyLeft |= row.state != ItemState::Unlocked && row.canUnlock;
        if (TabKit::PillButton(ui, ui.MakeId("se-unlock-here"), { right, cy - 19.f, uw, 38.f }, M::Icon::LockOpenVariant, unlockAll, tone, true, !busy && anyLeft))
            UnlockCategory(ui);
        right -= 12.f;
        // The challenges: every one back to not done, its count at 0
        if (c.list == ListId::Challenges) {
            const std::wstring& resetAll = Loc::Get("saveeditor_reset_challenges");
            const float rw = TabKit::PillWidth(ui, resetAll) + 10.f;
            right -= rw;
            bool anyDone = false;
            for (const auto& row : all) anyDone |= row.state == ItemState::Unlocked || row.progress > 0;
            const UI::Id resetId = ui.MakeId("se-reset-challenges");
            if (TabKit::PillButton(ui, resetId, { right, cy - 19.f, rw, 38.f }, M::Icon::Restore, resetAll, kAmber, false, !busy && anyDone)) {
                int reset = 0;
                Edit([&] { reset = save->ResetChallenges(); });
                if (reset) Status(Loc::Fmt("saveeditor_reset_challenges_done", std::to_wstring(reset)));
            }
            ui.Tooltip(resetId, Loc::Get("saveeditor_reset_challenges_hint"));
            right -= 12.f;
        }

        // Filter: segmented, the highlight slides to the picked one
        const std::wstring labels[4] = { Loc::Get("saveeditor_seg_all"), Loc::Get("saveeditor_seg_unlocked"), Loc::Get("saveeditor_seg_locked"),
                                         Loc::Get("saveeditor_seg_missing") };
        const int counts[4] = { static_cast<int>(all.size()), n[0], n[1], n[2] };
        const Color tones[4] = { tone, kGreen, kAmber, kBlue };
        TextStyle seg = Style(11.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD, false, HAlign::Center);
        seg.valign = VAlign::Center;
        float widths[4], total = 8.f;
        for (int k = 0; k < 4; ++k) {
            widths[k] = std::ceil(ui.g.Measure(labels[k] + L"  " + std::to_wstring(counts[k]), seg).w) + 24.f;
            total += widths[k];
        }
        right -= total;
        const Rect segR{ right, cy - 18.f, total, 36.f };
        ui.g.FillRound(segR, 18.f, p.foreground.Fade(0.06f));
        float offs[4], ox = segR.x + 4.f;
        for (int k = 0; k < 4; ++k) {
            offs[k] = ox;
            ox += widths[k];
        }
        const UI::Id segId = ui.MakeId("se-seg");
        // Animate eases by distance (Tween's fixed speed suits 0..1 values, not pixels)
        const float hx = ui.Animate(segId, offs[filter], 16.f, offs[filter]);
        const float hw = ui.Animate(UI::HashId(segId, 1), widths[filter], 16.f, widths[filter]);
        const Color hc = Lerp(ui.Data<Color>(UI::HashId(segId, 2)), tones[filter], 0.25f);
        ui.Data<Color>(UI::HashId(segId, 2)) = hc;
        if (std::abs(hx - offs[filter]) > 0.3f) ui.KeepAnimating();
        ui.g.FillRound({ hx, segR.y + 4.f, hw, segR.h - 8.f }, 14.f, hc);
        for (int k = 0; k < 4; ++k) {
            const Rect cell{ offs[k], segR.y, widths[k], segR.h };
            const UI::Id cid = ui.MakeId("se-seg-cell", k);
            const auto it = ui.Interact(cid, cell);
            const bool on = filter == k;
            ui.g.Text(labels[k] + L"  " + std::to_wstring(counts[k]), cell, on ? Colors::White : p.foreground.Fade(it.hovered ? 0.95f : 0.7f), seg);
            if (it.hovered && !on) ui.SetCursor(IDC_HAND);
            if (it.clicked) filter = k;
        }
        right -= 12.f;

        // Search
        M::FieldStyle field;
        field.outlined = true;
        field.fontSize = 13.f;
        field.hint = Loc::Get("saveeditor_search");
        const float fh = M::FieldHeight(ui, field);
        const float sw = std::clamp(right - x - 260.f, 140.f, 260.f);
        right -= sw;
        M::TextField(ui, ui.MakeId("se-search", category), { right, cy - fh * 0.5f, sw, fh }, search, field, !busy);
        right -= 16.f;
    }

    // Title and count
    TextStyle title = Style(18.f, DWRITE_FONT_WEIGHT_BOLD);
    title.valign = VAlign::Center;
    title.ellipsis = true;
    const float tw = std::max(0.f, right - x);
    if (c.numbers) {
        ui.g.Text(Loc::Get(c.key), { x, cy - 13.f, tw, 26.f }, p.foreground, title);
        return;
    }
    const Stat& st = Stats()[static_cast<size_t>(category)];
    ui.g.Text(Loc::Get(c.key), { x, cy - 20.f, tw, 24.f }, p.foreground, title);
    const long long n = Counter(ui, ui.MakeId("se-bar-count", category), st.done, 0.5f);
    TextStyle sub = Style(11.5f);
    sub.valign = VAlign::Center;
    const std::wstring line = Loc::Fmt("saveeditor_unlocked_of", Text::FromInt(n), std::to_wstring(st.total));
    const float lw = std::min(tw, std::ceil(ui.g.Measure(line, sub).w));
    ui.g.Text(line, { x, cy + 4.f, lw, 18.f }, p.foreground.Fade(0.6f), sub);
    const float barW = std::min(140.f, tw - lw - 12.f);
    if (barW > 30.f && st.total) {
        const float v = static_cast<float>(n) / static_cast<float>(st.total);
        ui.g.FillRound({ x + lw + 12.f, cy + 11.f, barW, 5.f }, 2.5f, p.foreground.Fade(0.08f));
        ui.g.FillRound({ x + lw + 12.f, cy + 11.f, std::max(5.f, barW * v), 5.f }, 2.5f, st.done == st.total ? kGreen : tone);
    }
}

// ── Tiles (categories with pictures) ────────────────────────────────────────────────────────────

// A card: the picture (full colour when unlocked, faded and covered when locked, faint when
// missing), a state badge, the name, and a chip. Hover lifts it; it bounces with its burst.
UI::Interaction SaveEditorTab::Tile(UI::Ui& ui, UI::Id id, const Rect& tile, const SF::ItemRow& row, const TileLook& look) {
    const M::Palette& p = M::P();
    const ListId list = Current().list;
    const Color group = GroupColor(Current().group);
    TextStyle name = Style(12.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD, true, HAlign::Center);
    name.valign = VAlign::Top;
    name.ellipsis = true;
    const auto it = ui.Interact(id, tile);
    const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, it.held);
    const Color tone = StateColor(look.state);
    const bool unlocked = look.state == ItemState::Unlocked;
    const float scale = BurstScale(look.burst) * (1.f - 0.03f * fb.press);
    ui.g.PushTransform(D2D1::Matrix3x2F::Scale(scale, scale, D2D1::Point2F(tile.CX(), tile.CY())));
    const Rect lifted = tile.Offset(0.f, -4.f * fb.hover);
    Card(ui, lifted, 14.f, unlocked ? group : tone, look.can ? fb.hover : 0.f);
    if (unlocked) ui.g.StrokeRound(lifted.Inset(0.5f), 13.5f, kGreen.Fade(0.35f), 1.f);

    // Picture
    const Rect pic{ lifted.x + 12.f, lifted.y + 12.f, lifted.w - 24.f, 104.f };
    // A soft plate behind it; a lighter one for the "SONIC ART" stickers, black line art that the dark
    // one hides
    if (list == ListId::Stickers && row.name == L"SONIC ART") ui.g.FillRound(pic, 10.f, Rgba(150, 154, 164).Fade(unlocked ? 0.6f : 0.5f));
    else ui.g.FillRound(pic, 10.f, (unlocked ? group : p.foreground).Fade(unlocked ? 0.12f : 0.1f));
    const float grow = 4.f * fb.hover;
    // What you don't have is dimmed, but still easy to make out
    const float alpha = unlocked ? 1.f : look.state == ItemState::Locked ? 0.75f : 0.6f;
    if (const ImagePtr image = Picture(list, row.id)) {
        if (list == ListId::Horns || (list == ListId::Auras && image->w > image->h * 1.2f)) {
            // The horns' pictures are framed screenshots: they fill the picture, the frame trimmed off (the
            // exchange's auras' too: a machine wearing it)
            ui.g.PushRoundClip(pic, 10.f);
            ui.g.DrawImage(*image, pic.Inset(-6.f - grow), alpha, Stretch::UniformToFill);
            ui.g.PopLayer();
        } else {
            ui.g.DrawImage(*image, pic.Inset(4.f - grow), alpha, Stretch::Uniform);
        }
    } else M::DrawIcon(ui.g, Current().icon, pic.CX(), pic.CY(), 40.f + grow, unlocked ? TabKit::Readable(group) : p.foreground.Fade(0.45f));
    if (!unlocked) {
        // A light cover with a small state icon in the corner of the picture (the badge says it too), gone on hover
        ui.g.FillRound(pic, 10.f, p.background.Fade(0.12f * (1.f - fb.hover)));
    }
    // State badge on the corner
    const float bx = pic.R() - 4.f, by = pic.y + 4.f;
    ui.g.FillEllipse(bx, by, 11.f, 11.f, tone);
    ui.g.StrokeEllipse(bx, by, 11.f, 11.f, p.background, 2.f);
    M::DrawIcon(ui.g, StateIcon(look.state), bx, by, 13.f, Colors::White);
    BurstRing(ui, bx, by, 16.f, look.burst, tone);

    // Name, and the chip
    ui.g.Text(row.name, { lifted.x + 8.f, pic.B() + 8.f, lifted.w - 16.f, 34.f }, unlocked ? p.foreground : p.foreground.Fade(0.7f), name);
    if (!look.chip.empty()) {
        const float cw = std::min(ChipWidth(ui, look.chip), lifted.w - 16.f);
        Chip(ui, lifted.CX() - cw * 0.5f, lifted.B() - 16.f, look.chip, look.chipTone, false, false, cw);
    }
    ui.g.PopTransform();
    if (it.hovered) ui.SetCursor(look.can ? IDC_HAND : IDC_NO);
    return it;
}

// A tile per character ("unlocked / total" of its versions under it; with several, a click opens
// them) or per item; a click on a single one changes it and it bursts
void SaveEditorTab::Tiles(UI::Ui& ui, const Rect& full) {
    const auto& all = Rows();
    const auto& fams = Families();
    Rect area = full;
    std::vector<size_t> shown = ShownFamilies();
    // Stickers: tags by kind
    if (Current().list == ListId::Stickers) {
        using G = Data::StickerCatalog::Group;
        const auto kindOf = [](const SF::ItemRow& row) { return static_cast<int>(Data::StickerCatalog::GroupOf(static_cast<int>(Text::ToInt(row.id).value_or(-1)))); };
        area = TagRow(ui, full, stickerTag,
                      { { -1, "saveeditor_tag_all", M::Icon::Sticker },
                        { static_cast<int>(G::Characters), "saveeditor_tag_characters", M::Icon::Account },
                        { static_cast<int>(G::Designs), "saveeditor_tag_designs", M::Icon::Palette },
                        { static_cast<int>(G::Events), "saveeditor_tag_events", M::Icon::StarFourPoints },
                        { static_cast<int>(G::Competitions), "saveeditor_tag_competitions", M::Icon::Trophy },
                        { static_cast<int>(G::Brands), "saveeditor_tag_brands", M::Icon::Medal } },
                      kindOf);
        if (stickerTag >= 0) std::erase_if(shown, [&](size_t k) { return !FamilyHasTag(fams[k], stickerTag, kindOf); });
    }
    // Horns: the base game's and the festivals' (HORNTYPE_EXTND_..)
    if (Current().list == ListId::Horns) {
        const auto kindOf = [](const SF::ItemRow& row) { return Text::Contains(row.id, L"EXTND") ? 1 : 0; };
        area = TagRow(ui, full, hornTag,
                      { { -1, "saveeditor_tag_all", M::Icon::Bullhorn },
                        { 0, "saveeditor_tag_basic", M::Icon::Bullhorn },
                        { 1, "saveeditor_tag_festivals", M::Icon::StarFourPoints },
                        { kShopTag, "saveeditor_tag_shop", M::Icon::Storefront } },
                      kindOf);
        if (hornTag >= 0) std::erase_if(shown, [&](size_t k) { return !FamilyHasTag(fams[k], hornTag, kindOf); });
    }
    // Characters, auras, gadgets: all, or what the shop sells
    if (Current().list == ListId::Drivers || Current().list == ListId::Auras || Current().list == ListId::Gadgets) {
        const auto kindOf = [](const SF::ItemRow&) { return 0; };
        const M::Icon icon = Current().list == ListId::Drivers ? M::Icon::Account : Current().list == ListId::Auras ? M::Icon::Creation : M::Icon::Flash;
        area = TagRow(ui, full, shopTag, { { -1, "saveeditor_tag_all", icon }, { kShopTag, "saveeditor_tag_shop", M::Icon::Storefront } }, kindOf);
        if (shopTag >= 0) std::erase_if(shown, [&](size_t k) { return !FamilyHasTag(fams[k], shopTag, kindOf); });
    }
    // Colour presets: the machines' own colours and the festival teams'
    if (Current().list == ListId::ColorPresets) {
        const auto kindOf = [](const SF::ItemRow& row) {
            const int number = static_cast<int>(Text::ToInt(row.id.substr(std::min<size_t>(row.id.size(), 11))).value_or(-1));
            for (const auto& c : Data::ColorCatalog::Presets())
                if (c.id == number) return *c.machine ? 0 : c.festival ? 1 : 2;
            return 2;
        };
        area = TagRow(ui, full, colorTag,
                      { { -1, "saveeditor_tag_all", M::Icon::Palette },
                        { 0, "saveeditor_tag_machine_colors", M::Icon::CarSports },
                        { 1, "saveeditor_tag_festivals", M::Icon::StarFourPoints } },
                      kindOf);
        if (colorTag >= 0) std::erase_if(shown, [&](size_t k) { return !FamilyHasTag(fams[k], colorTag, kindOf); });
    }
    if (shown.empty()) {
        BackupKit::EmptyState(ui, area, M::Icon::Magnify, Loc::Get("saveeditor_empty_title"), Loc::Get("saveeditor_empty_message"));
        return;
    }
    UI::ScrollArea scroll(ui, ui.MakeId("se-items"), area, false, 70.f, UI::ScrollArea::Bar::Material);
    const float w = scroll.ContentWidth() - (scroll.BarShown() ? 10.f : 0.f);
    const int cols = std::max(1, static_cast<int>((w + kGap) / (kTileW + kGap)));
    const float tileW = (w - (cols - 1) * kGap) / cols;
    const double reveal = std::max(loadedAt, screenAt) + 0.12;
    const double now = ui.Time();

    for (size_t k = 0; k < shown.size(); ++k) {
        const float ty = area.y + 4.f + (k / cols) * (kTileH + kGap);
        if (ty + kTileH < area.y + scroll.Offset() - 20.f || ty > area.B() + scroll.Offset() + 20.f) continue;
        const Family& f = fams[shown[k]];
        const bool many = f.members.size() > 1;
        // A group shows by its name (a sticker pack's own)
        SF::ItemRow row = all[f.members.front()];
        if (many) row.name = FamilyName(f);
        const Rect tile{ area.x + (k % cols) * (tileW + kGap), ty, tileW, kTileH };
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, reveal, static_cast<int>(std::min<size_t>(k, 24)), 0.018f, 0.35f), 16.f);
        const UI::Id id = ui.MakeId("se-tile", static_cast<int64_t>(std::hash<std::wstring>{}(f.id) & 0x7FFFFFFF));
        const TileLook look = FamilyLook(all, f, now);
        const auto it = Tile(ui, id, tile, row, look);
        if (std::any_of(f.members.begin(), f.members.end(), [&](size_t m) { return all[m].shop; })) ShopBadge(ui, tile.x + 16.f, tile.y + 16.f);   // top left: the state is on the right
        // A horn with its sound: a play button on its picture's corner
        bool overPlay = false;
        if (Current().list == ListId::Horns && !many)
            if (const std::wstring sound = HornSound(row.id); !sound.empty()) {
                const float r = 16.f, cx = tile.x + 12.f + r + 6.f, cy = tile.y + 12.f + 104.f - r - 6.f;
                const UI::Id pid = UI::HashId(id, 0x50);
                const auto pit = ui.Interact(pid, { cx - r, cy - r, r * 2.f, r * 2.f });
                const M::Motion::Feedback pfb = M::Motion::Hover(ui, pid, pit.hovered, pit.held);
                const float pr = r * (1.f + 0.12f * pfb.hover - 0.06f * pfb.press);
                ui.g.GlowRound({ cx - pr, cy - pr, pr * 2.f, pr * 2.f }, pr, GroupColor(Current().group), 8.f, 0.25f + 0.35f * pfb.hover);
                ui.g.FillEllipse(cx, cy, pr, pr, Lerp(GroupColor(Current().group), Colors::White, 0.1f * pfb.hover));
                M::DrawIcon(ui.g, M::Icon::Play, cx + 1.5f, cy, 18.f, Colors::White);
                overPlay = pit.hovered;
                if (pit.hovered) ui.SetCursor(IDC_HAND);
                ui.Tooltip(pid, Loc::Get("saveeditor_horn_play"));
                if (pit.clicked) PlayWav(sound);
            }
        const char* openKey = Current().list == ListId::Stickers ? "saveeditor_click_stickers" : "saveeditor_click_versions";
        const bool sold = std::any_of(f.members.begin(), f.members.end(), [&](size_t m) { return all[m].shop; });
        ui.Tooltip(id, row.name + (sold ? L"  \u00B7  " + Loc::Get("saveeditor_tag_shop") : L"") + L"\n" +
                           (many ? Loc::Fmt(openKey, std::to_wstring(f.members.size()))
                                 : ActionText(row) + (row.hint.empty() ? L"" : L"\n" + row.hint)));
        if (it.clicked && look.can && !overPlay) {
            if (many) variants->Show(f.id);
            else ToggleItem(ui, row);
        }
    }
    const size_t lines = (shown.size() + cols - 1) / cols;
    scroll.SetContentSize(lines * (kTileH + kGap) - kGap + 8.f);
}

// ── Tags ────────────────────────────────────────────────────────────────────────────────────────

Rect SaveEditorTab::TagRow(UI::Ui& ui, const Rect& r, int& current, const std::vector<TagInfo>& tags,
                           const std::function<int(const SF::ItemRow&)>& kindOf) {
    const Color tone = GroupColor(Current().group);
    std::vector<int> done(tags.size(), 0), total(tags.size(), 0);
    for (const auto& row : Rows()) {
        const int kind = kindOf(row);
        for (size_t t = 0; t < tags.size(); ++t)
            if (tags[t].kind < 0 || (tags[t].kind == kShopTag ? row.shop : tags[t].kind == kind)) {
                ++total[t];
                done[t] += row.state == ItemState::Unlocked;
            }
    }
    float x = r.x;
    for (size_t t = 0; t < tags.size(); ++t) {
        const std::wstring text = Loc::Get(tags[t].key) + L"   " + std::to_wstring(done[t]) + L" / " + std::to_wstring(total[t]);
        const float tw = TabKit::PillWidth(ui, text);
        const bool on = current == tags[t].kind;
        if (TabKit::PillButton(ui, ui.MakeId("se-tag", static_cast<int64_t>(category) * 16 + tags[t].kind + 1), { x, r.y, tw, 36.f }, tags[t].icon, text, tone, on, !on)) {
            current = tags[t].kind;
            UI::ScrollArea::ScrollTo(ui, ui.MakeId("se-items"), 0.f);
        }
        x += tw + 10.f;
    }
    return { r.x, r.y + 36.f + kGap, r.w, r.h - 36.f - kGap };
}

bool SaveEditorTab::FamilyHasTag(const Family& f, int tag, const std::function<int(const SF::ItemRow&)>& kindOf) {
    const auto& all = Rows();
    if (tag < 0) return true;
    if (tag == kShopTag) return std::any_of(f.members.begin(), f.members.end(), [&](size_t m) { return all[m].shop; });
    return kindOf(all[f.members.front()]) == tag;
}

// ── Banners (honor titles) ──────────────────────────────────────────────────────────────────────

UI::Interaction SaveEditorTab::Banner(UI::Ui& ui, UI::Id id, const Rect& tile, float bannerH, const SF::ItemRow& row, const TileLook& look, bool on) {
    const M::Palette& p = M::P();
    const bool unlocked = look.state == ItemState::Unlocked;
    const Rect banner{ tile.x + 10.f, tile.y + 10.f, tile.w - 20.f, bannerH };
    const auto it = ui.Interact(id, banner);
    const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, it.held);
    const Color tone = on ? kGold : StateColor(look.state);
    Card(ui, tile, 14.f, unlocked ? GroupColor(3) : tone, look.can ? fb.hover : 0.f);
    if (on) ui.g.StrokeRound(tile.Inset(0.5f), 13.5f, kGold.Fade(0.6f), 1.5f);

    // The banner (or its name on a plain plate when the pictures aren't cached)
    const float scale = BurstScale(look.burst) * (1.f + 0.015f * fb.hover) * (1.f - 0.02f * fb.press);
    ui.g.PushTransform(D2D1::Matrix3x2F::Scale(scale, scale, D2D1::Point2F(banner.CX(), banner.CY())));
    const float alpha = unlocked ? 1.f : (look.state == ItemState::Locked ? 0.4f : 0.25f) + 0.35f * fb.hover;
    if (const ImagePtr image = Picture(ListId::HonorTitles, row.id)) {
        // The rect is already the plates' 8:1, and Uniform never scales a picture up
        ui.g.DrawImage(*image, banner, alpha, Stretch::Fill);
    } else {
        ui.g.FillRound(banner.Inset(banner.w * 0.08f, banner.h * 0.12f), 8.f, GroupColor(3).Fade(0.25f * alpha + 0.1f));
        TextStyle plate = Style(std::max(12.f, bannerH * 0.32f), DWRITE_FONT_WEIGHT_BOLD, false, HAlign::Center);
        plate.valign = VAlign::Center;
        plate.ellipsis = true;
        ui.g.Text(row.name, banner.Inset(banner.w * 0.12f, 0.f), p.foreground.Fade(alpha), plate);
    }
    // The state on the corner, off the name
    const float bx = banner.x + 4.f, by = banner.y + 4.f;
    ui.g.FillEllipse(bx, by, 11.f, 11.f, tone);
    ui.g.StrokeEllipse(bx, by, 11.f, 11.f, p.background, 2.f);
    M::DrawIcon(ui.g, on ? M::Icon::AccountStar : StateIcon(look.state), bx, by, 13.f, Colors::White);
    BurstRing(ui, bx, by, 16.f, look.burst, tone);
    ui.g.PopTransform();
    if (it.hovered) ui.SetCursor(look.can ? IDC_HAND : IDC_NO);
    return it;
}

void SaveEditorTab::BannerLine(UI::Ui& ui, UI::Id id, const Rect& tile, float bannerH, const SF::ItemRow& row, bool on) {
    const M::Palette& p = M::P();
    const bool unlocked = row.state == ItemState::Unlocked;
    TextStyle name = Style(12.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    name.valign = VAlign::Center;
    name.ellipsis = true;
    const float ly = tile.y + 10.f + bannerH + 24.f;
    float right = tile.R() - 12.f;
    if (on) {
        const std::wstring& text = Loc::Get("saveeditor_title_on");
        right -= Chip(ui, right - ChipWidth(ui, text, true), ly, text, kGold, true, true) + 8.f;
    } else if (unlocked) {
        const std::wstring& text = Loc::Get("saveeditor_title_put_on");
        const float pw = TabKit::PillWidth(ui, text) - 10.f;
        right -= pw;
        if (TabKit::PillButton(ui, UI::HashId(id, 0xE0), { right, ly - 15.f, pw, 30.f }, M::Icon::AccountStar, text, kGold, false, !busy)) {
            const int title = static_cast<int>(Text::ToInt(row.id).value_or(-1));
            Edit([&] { save->SetSelectedTitle(title); });
            bursts.Fire(row.id, ui.Time());
        }
        right -= 8.f;
    }
    const std::wstring state = Loc::Get(StateKey(row.state));
    const float sw = ChipWidth(ui, state, true);
    Chip(ui, tile.x + 12.f, ly, state, StateColor(row.state), false, true);
    ui.g.Text(row.name, { tile.x + 22.f + sw, ly - 10.f, std::max(0.f, right - tile.x - 30.f - sw), 20.f }, unlocked ? p.foreground : p.foreground.Fade(0.65f), name);
}

// Two columns of banners: a character's titles (or a festival's, the World Championship's) share one,
// its first, with "unlocked / total" under it, and a click opens them all in a card; the rest have
// their own, which a click unlocks or locks. The one on the player's card is in gold.
void SaveEditorTab::Banners(UI::Ui& ui, const Rect& full) {
    const M::Palette& p = M::P();
    const auto& all = Rows();
    const auto& fams = Families();
    const auto titleOf = [](const std::wstring& id) { return static_cast<int>(Text::ToInt(id).value_or(-1)); };

    // Tags: All, Racers, Festivals, Competitions; one picked shows only its titles
    const Rect area = TagRow(ui, full, titleTag,
                             { { -1, "saveeditor_tag_all", M::Icon::Medal },
                               { 0, "saveeditor_tag_racers", M::Icon::Account },
                               { 1, "saveeditor_tag_festivals", M::Icon::StarFourPoints },
                               { 2, "saveeditor_tag_competitions", M::Icon::Trophy },
                               { kShopTag, "saveeditor_tag_shop", M::Icon::Storefront } },
                             [&](const SF::ItemRow& row) { return Data::SaveCatalog::TitleGroup(titleOf(row.id)); });
    std::vector<size_t> shown;
    const auto titleKind = [&](const SF::ItemRow& row) { return Data::SaveCatalog::TitleGroup(titleOf(row.id)); };
    for (size_t k : ShownFamilies())
        if (FamilyHasTag(fams[k], titleTag, titleKind)) shown.push_back(k);
    if (shown.empty()) {
        BackupKit::EmptyState(ui, area, M::Icon::Magnify, Loc::Get("saveeditor_empty_title"), Loc::Get("saveeditor_empty_message"));
        return;
    }
    UI::ScrollArea scroll(ui, ui.MakeId("se-items"), area, false, 70.f, UI::ScrollArea::Bar::Material);
    const float w = area.w - 10.f;
    const int cols = w >= 900.f ? 2 : 1;
    const float tileW = (w - (cols - 1) * kGap) / cols;
    const float bannerH = std::round((tileW - 20.f) / 8.f);   // the plates are 8:1
    const float tileH = 10.f + bannerH + 48.f;
    const double reveal = std::max(loadedAt, screenAt) + 0.12;
    const double now = ui.Time();
    const std::wstring selected = save ? std::to_wstring(save->SelectedTitle()) : L"";
    TextStyle name = Style(12.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    name.valign = VAlign::Center;
    name.ellipsis = true;
    TextStyle more = Style(12.f, DWRITE_FONT_WEIGHT_SEMI_BOLD, false, HAlign::Right);
    more.valign = VAlign::Center;

    for (size_t k = 0; k < shown.size(); ++k) {
        const float ty = area.y + 4.f + (k / cols) * (tileH + kGap);
        if (ty + tileH < area.y + scroll.Offset() - 20.f || ty > area.B() + scroll.Offset() + 20.f) continue;
        const Family& f = fams[shown[k]];
        const SF::ItemRow& row = all[f.members.front()];
        const bool many = f.members.size() > 1;
        const Rect tile{ area.x + (k % cols) * (tileW + kGap), ty, tileW, tileH };
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, reveal, static_cast<int>(std::min<size_t>(k, 20)), 0.02f, 0.35f), 14.f);
        const UI::Id id = ui.MakeId("se-banner", static_cast<int64_t>(std::hash<std::wstring>{}(f.id) & 0x7FFFFFFF));
        bool on = false;
        for (size_t i : f.members) on |= all[i].id == selected;
        TileLook look = FamilyLook(all, f, now);
        const auto it = Banner(ui, id, tile, bannerH, row, look, on);
        if (many) {
            // "unlocked / total", the name, and how many it holds
            const float ly = tile.y + 10.f + bannerH + 24.f;
            const float cw = ChipWidth(ui, look.chip, true);
            Chip(ui, tile.x + 12.f, ly, look.chip, look.chipTone, false, true);
            const std::wstring count = Loc::Fmt("saveeditor_titles_n", std::to_wstring(f.members.size()));
            M::DrawIcon(ui.g, M::Icon::ChevronRight, tile.R() - 22.f, ly, 20.f, p.foreground.Fade(0.5f));
            ui.g.Text(count, { tile.R() - 150.f, ly - 10.f, 116.f, 20.f }, p.foreground.Fade(0.6f), more);
            ui.g.Text(row.name, { tile.x + 22.f + cw, ly - 10.f, std::max(0.f, tile.R() - 160.f - tile.x - 22.f - cw), 20.f }, p.foreground, name);
            ui.Tooltip(id, row.name + L"\n" + Loc::Fmt("saveeditor_click_titles", std::to_wstring(f.members.size())));
            if (it.clicked) variants->Show(f.id);
        } else {
            BannerLine(ui, id, tile, bannerH, row, on);
            ui.Tooltip(id, row.name + L"\n" + (on ? Loc::Get("saveeditor_title_on_tip") : ActionText(row)) + (row.hint.empty() ? L"" : L"\n" + row.hint));
            if (it.clicked && look.can) ToggleItem(ui, row);
        }
    }
    const size_t lines = (shown.size() + cols - 1) / cols;
    scroll.SetContentSize(lines * (tileH + kGap) - kGap + 8.f);
}

// ── Rows (categories without pictures) ──────────────────────────────────────────────────────────

// One line per item: its state badge, name, id and how to get it, a chip, its state, and a switch;
// the whole line is clickable
void SaveEditorTab::List(UI::Ui& ui, const Rect& area) {
    const M::Palette& p = M::P();
    const auto& all = Rows();
    const auto& fams = Families();
    const auto& shown = ShownFamilies();
    if (shown.empty()) {
        BackupKit::EmptyState(ui, area, M::Icon::Magnify, Loc::Get("saveeditor_empty_title"), Loc::Get("saveeditor_empty_message"));
        return;
    }
    UI::ScrollArea scroll(ui, ui.MakeId("se-items"), area, false, 60.f, UI::ScrollArea::Bar::Material);
    const float w = scroll.ContentWidth() - (scroll.BarShown() ? 10.f : 0.f);
    const double reveal = std::max(loadedAt, screenAt) + 0.12;
    const double now = ui.Time();
    TextStyle name = Style(14.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    name.valign = VAlign::Center;
    name.ellipsis = true;
    TextStyle meta = Style(11.f);
    meta.valign = VAlign::Center;
    meta.ellipsis = true;
    const ListId list = Current().list;

    float ry = area.y + 2.f;
    for (size_t k = 0; k < shown.size(); ++k, ry += kRowH + 6.f) {
        if (ry + kRowH < area.y + scroll.Offset() - 20.f || ry > area.B() + scroll.Offset() + 20.f) continue;
        const Family& family = fams[shown[k]];
        const SF::ItemRow& row = all[family.members.front()];
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, reveal, static_cast<int>(std::min<size_t>(k, 14)), 0.022f, 0.3f), 0.f, -18.f);
        const Rect rr{ area.x, ry, w, kRowH };
        const UI::Id id = ui.MakeId("se-line", static_cast<int64_t>(std::hash<std::wstring>{}(family.id) & 0x7FFFFFFF));
        if (family.members.size() > 1) {
            FamilyLine(ui, id, rr, all, family, now);
            continue;
        }
        const bool can = CanToggle(row) && !busy;
        const auto it = ui.Interact(id, rr);
        const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, it.held);
        const Color tone = StateColor(row.state);
        const bool unlocked = row.state == ItemState::Unlocked;
        Card(ui, rr, 12.f, tone, can ? fb.hover * 0.6f : 0.f);
        // State badge, bouncing with its burst
        const float burst = bursts.Progress(row.id, now);
        const float s = BurstScale(burst);
        const float bx = rr.x + 30.f, by = rr.CY();
        ui.g.FillEllipse(bx, by, 15.f * s, 15.f * s, tone.Fade(unlocked ? 0.9f : 0.18f));
        M::DrawIcon(ui.g, StateIcon(row.state), bx, by, 16.f * s, unlocked ? Colors::White : TabKit::Readable(tone));
        BurstRing(ui, bx, by, 18.f, burst, tone);

        // From the right: switch, state, chip
        const Rect sw{ rr.R() - 14.f - M::kSwitchWidth, rr.CY() - M::kSwitchHeight * 0.5f, M::kSwitchWidth, M::kSwitchHeight };
        const bool flipped = M::Switch(ui, UI::HashId(id, 0x5C), sw, unlocked, can);
        float right = sw.x - 12.f;
        // A course's parts one by one (its 5 Red Star Rings, its 2 Time Trial rewards): a token each,
        // filled when you have it; a click flips just that one
        if (!row.flags.empty()) {
            const Color on = list == ListId::RedStarRings ? kRing : kGold;
            constexpr float d = 30.f, gap = 6.f;
            const float width = row.flags.size() * d + (row.flags.size() - 1) * gap;
            float fx = right - width;
            for (size_t f = 0; f < row.flags.size(); ++f, fx += d + gap) {
                const Rect tr{ fx, rr.CY() - d * 0.5f, d, d };
                const UI::Id tid = UI::HashId(id, 0x7000 + static_cast<int64_t>(f));
                const auto tit = ui.Interact(tid, tr, !busy);
                const M::Motion::Feedback tfb = M::Motion::Hover(ui, tid, tit.hovered, tit.held);
                const bool set = row.flags[f];
                const std::wstring bid = row.id + L"#" + std::to_wstring(f);
                const float bt = bursts.Progress(bid, now);
                const float ts = BurstScale(bt) * (1.f + 0.12f * tfb.hover);
                const float cx = tr.CX(), cyy = tr.CY(), radius = d * 0.5f * ts;
                if (set) {
                    ui.g.GlowRound({ cx - radius, cyy - radius, radius * 2.f, radius * 2.f }, radius, on, 8.f, 0.35f);
                    ui.g.FillEllipse(cx, cyy, radius, radius, on);
                } else {
                    ui.g.FillEllipse(cx, cyy, radius, radius, p.foreground.Fade(0.05f + 0.06f * tfb.hover));
                    ui.g.StrokeEllipse(cx, cyy, radius - 1.f, radius - 1.f, on.Fade(0.45f + 0.4f * tfb.hover), 1.5f);
                }
                M::DrawIcon(ui.g, M::Icon::StarFourPoints, cx, cyy, 16.f * ts, set ? Colors::White : on.Fade(0.55f + 0.3f * tfb.hover));
                BurstRing(ui, cx, cyy, d * 0.55f, bt, on);
                ui.Tooltip(tid, Loc::Fmt(list == ListId::RedStarRings ? "saveeditor_ring_n" : "saveeditor_reward_n", std::to_wstring(f + 1)) + L"\n" +
                                    Loc::Get(set ? "saveeditor_click_unmark" : "saveeditor_click_mark"));
                if (tit.clicked) {
                    bool changed = false;
                    const std::wstring rowId = row.id;
                    Edit([&] { changed = save->SetFlag(list, rowId, f, !set); });
                    if (changed) bursts.Fire(bid, now);
                }
            }
            right -= width + 14.f;
            // Complete: one chip; otherwise the count says enough
            if (unlocked) {
                const std::wstring done = Loc::Get("saveeditor_complete");
                right -= Chip(ui, right - ChipWidth(ui, done, true), rr.CY(), done, kGreen, false, true) + 8.f;
            }
        } else {
            const std::wstring state = Loc::Get(StateKey(row.state));
            right -= Chip(ui, right - ChipWidth(ui, state, true), rr.CY(), state, tone, false, true) + 8.f;
        }
        if (!row.detail.empty()) {
            const float cw = ChipWidth(ui, row.detail);
            Chip(ui, right - cw, rr.CY(), row.detail, row.unreleased || !row.canUnlock ? kAmber : row.goal > 0 && row.progress >= row.goal ? kGreen : p.foreground.Fade(0.55f));
            right -= cw + 8.f;
        }
        // A challenge: how far it is, as a bar
        if (row.goal > 0) {
            constexpr float bw = 120.f;
            const float v = std::clamp(static_cast<float>(row.progress) / static_cast<float>(row.goal), 0.f, 1.f);
            const float shownV = ui.Tween(UI::HashId(id, 0x9A), v, 0.4f);
            const Rect bar{ right - bw, rr.CY() - 3.f, bw, 6.f };
            ui.g.FillRound(bar, 3.f, p.foreground.Fade(0.08f));
            if (shownV > 0.f) ui.g.FillRound({ bar.x, bar.y, std::max(6.f, bar.w * shownV), bar.h }, 3.f, v >= 1.f ? kGreen : tone);
            right -= bw + 12.f;
        }
        const float tx = rr.x + 58.f, tw = std::max(0.f, right - 10.f - tx);
        ui.g.Text(row.name, { tx, rr.CY() - 18.f, tw, 20.f }, unlocked ? p.foreground : p.foreground.Fade(0.8f), name);
        ui.g.Text(row.hint.empty() ? row.id : row.id + L"   \x00B7   " + row.hint, { tx, rr.CY() + 3.f, tw, 16.f }, p.foreground.Fade(0.48f), meta);

        if (it.hovered) ui.SetCursor(can ? IDC_HAND : IDC_NO);
        ui.Tooltip(id, ActionText(row));
        // A click on the switch is a click on the line too: one change either way
        if ((flipped || it.clicked) && can) ToggleItem(ui, row);
    }
    scroll.SetContentSize(shown.size() * (kRowH + 6.f) - 6.f + 4.f);
}

// A character with versions as a line: its state badge, name, how many versions, "unlocked / total"
// and an arrow; a click opens them
void SaveEditorTab::FamilyLine(UI::Ui& ui, UI::Id id, const Rect& rr, const std::vector<SF::ItemRow>& all, const Family& family, double now) {
    const M::Palette& p = M::P();
    const SF::ItemRow& row = all[family.members.front()];
    const TileLook look = FamilyLook(all, family, now);
    const auto it = ui.Interact(id, rr);
    const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, it.held);
    const Color tone = StateColor(look.state);
    const bool unlocked = look.state == ItemState::Unlocked;
    Card(ui, rr, 12.f, tone, fb.hover * 0.6f);
    const float s = BurstScale(look.burst);
    const float bx = rr.x + 30.f, by = rr.CY();
    ui.g.FillEllipse(bx, by, 15.f * s, 15.f * s, tone.Fade(unlocked ? 0.9f : 0.18f));
    M::DrawIcon(ui.g, StateIcon(look.state), bx, by, 16.f * s, unlocked ? Colors::White : TabKit::Readable(tone));
    BurstRing(ui, bx, by, 18.f, look.burst, tone);
    M::DrawIcon(ui.g, M::Icon::ChevronRight, rr.R() - 26.f + 3.f * fb.hover, rr.CY(), 22.f, p.foreground.Fade(0.5f + 0.4f * fb.hover));
    const float right = rr.R() - 48.f;
    const float cw = ChipWidth(ui, look.chip, true);
    Chip(ui, right - cw, rr.CY(), look.chip, look.chipTone, false, true);
    TextStyle name = Style(14.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    name.valign = VAlign::Center;
    name.ellipsis = true;
    TextStyle meta = Style(11.f);
    meta.valign = VAlign::Center;
    meta.ellipsis = true;
    const float tx = rr.x + 58.f, tw = std::max(0.f, right - cw - 18.f - tx);
    const std::wstring count = std::to_wstring(family.members.size());
    ui.g.Text(row.name, { tx, rr.CY() - 18.f, tw, 20.f }, unlocked ? p.foreground : p.foreground.Fade(0.8f), name);
    ui.g.Text(Loc::Fmt("saveeditor_versions_n", count), { tx, rr.CY() + 3.f, tw, 16.f }, p.foreground.Fade(0.48f), meta);
    if (it.hovered) ui.SetCursor(IDC_HAND);
    ui.Tooltip(id, Loc::Fmt("saveeditor_click_versions", count));
    if (it.clicked) variants->Show(family.id);
}

// ── Profile values ──────────────────────────────────────────────────────────────────────────────

// A card per value in three columns: its name, the number (counting to its new value), the box
// (written when it loses the focus) and Max
void SaveEditorTab::Numbers(UI::Ui& ui, const Rect& r, bool currencies) {
    const M::Palette& p = M::P();
    const Color tone = GroupColor(0);
    auto fields = save->Numbers();
    if (currencies) std::erase_if(fields, [](const SF::NumberField& f) { return !f.currency; });
    UI::ScrollArea scroll(ui, ui.MakeId("se-items"), r, false, 60.f, UI::ScrollArea::Bar::Material);
    constexpr int kCols = 3;
    const float colW = (scroll.ContentWidth() - (scroll.BarShown() ? 10.f : 0.f) - (kCols - 1) * kGap) / kCols;
    M::FieldStyle field;
    field.outlined = true;
    field.fontSize = 13.f;
    field.align = HAlign::Right;
    const float fieldH = M::FieldHeight(ui, field);
    TextStyle label = Style(12.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    label.valign = VAlign::Center;
    label.ellipsis = true;
    TextStyle big = Style(28.f, DWRITE_FONT_WEIGHT_BOLD);
    big.valign = VAlign::Center;
    const double reveal = std::max(loadedAt, screenAt) + 0.12;

    for (size_t k = 0; k < fields.size(); ++k) {
        const SF::NumberField& f = fields[k];
        const Rect card{ r.x + (k % kCols) * (colW + kGap), r.y + 2.f + (k / kCols) * (kStatH + kGap), colW, kStatH };
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, reveal, static_cast<int>(k), 0.03f, 0.35f), 16.f);
        const UI::Id id = ui.MakeId("se-stat", static_cast<int64_t>(k));
        const auto it = ui.Interact(id, card);
        const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, false);
        Card(ui, card, kRadius, tone, fb.hover * 0.7f);
        ui.g.Text(Loc::Get("savefile_num_" + f.key), { card.x + 18.f, card.y + 14.f, card.w - 36.f, 20.f }, p.foreground.Fade(0.7f), label);
        const long long shown = Counter(ui, UI::HashId(id, 0xC0), f.value, 0.6f);
        ui.g.Text(Text::FromInt(shown), { card.x + 18.f, card.y + 36.f, card.w - 36.f, 38.f }, p.foreground, big);
        // Box and Max
        const std::wstring maxText = Loc::Get("saveeditor_max");
        const float mw = TabKit::PillWidth(ui, maxText);
        const Rect maxR{ card.R() - 16.f - mw, card.B() - 14.f - 32.f, mw, 32.f };
        const Rect box{ card.x + 18.f, card.B() - 14.f - fieldH, std::min(180.f, maxR.x - 12.f - card.x - 18.f), fieldH };
        const std::string key = f.key;
        const int64_t lo = f.min, hi = f.max;
        const UI::Id boxId = ui.MakeId("se-num", static_cast<int64_t>(k));
        M::BoundTextField(
            ui, boxId, box, Text::FromInt(f.value),
            [this, key, lo, hi](const std::wstring& text) {
                const auto v = Text::ToInt(Text::Trim(text));
                if (!v) return;
                Edit([&] { save->SetNumber(key, std::clamp<int64_t>(*v, lo, hi)); });
            },
            field, !busy);
        ui.Tooltip(boxId, Loc::Fmt("saveeditor_range", Text::FromInt(f.min), Text::FromInt(f.max)));
        if (TabKit::PillButton(ui, ui.MakeId("se-num-max", static_cast<int64_t>(k)), maxR, M::Icon::ArrowUp, maxText, tone, false,
                               !busy && f.suggested > 0 && f.value != f.suggested)) {
            const int64_t v = f.suggested;
            Edit([&] { save->SetNumber(key, v); });
        }
    }
    const size_t lines = (fields.size() + kCols - 1) / kCols;
    scroll.SetContentSize(lines ? lines * (kStatH + kGap) - kGap + 4.f : 0.f);
}

namespace {

// A number with its thousands apart, as the language writes them: 2,512,363 / 2.512.363
std::wstring Grouped(int64_t n) {
    std::wstring digits = Text::FromInt(n < 0 ? -n : n), out;
    const wchar_t sep = Loc::CurrentLang() == L"en" ? L',' : L'.';
    for (size_t k = 0; k < digits.size(); ++k) {
        if (k && (digits.size() - k) % 3 == 0) out.push_back(sep);
        out.push_back(digits[k]);
    }
    return n < 0 ? L"-" + out : out;
}

// How each currency looks: the game's picture for it (GameImages\<kind>\<id>), else an icon; its colour
struct CurrencyLook {
    const wchar_t* kind;
    const wchar_t* image;
    M::Icon icon;
    Color tone;
};
CurrencyLook LookOf(const std::string& key) {
    if (key == "donpa_tickets" || key == "total_donpa_tickets" || key == "used_donpa_tickets")
        return { L"currency", L"tickets", M::Icon::Ticket, Rgba(239, 68, 68) };
    if (key == "duel_medals") return { L"currency", L"medals", M::Icon::Medal, Rgba(234, 179, 8) };
    if (key == "red_star_rings" || key == "total_red_star_rings") return { L"rsr", L"on", M::Icon::StarFourPoints, Rgba(244, 63, 94) };
    if (key == "gadget_fragments") return { L"gadget", L"unknown", M::Icon::Flash, Rgba(59, 130, 246) };
    // The rest of the profile
    if (key == "level") return { nullptr, nullptr, M::Icon::AccountStar, Rgba(59, 130, 246) };
    if (key == "race_count") return { nullptr, nullptr, M::Icon::FlagCheckered, Rgba(16, 185, 129) };
    if (key == "rate_high") return { nullptr, nullptr, M::Icon::TrendingUp, Rgba(14, 165, 233) };
    if (key == "gp_wins") return { nullptr, nullptr, M::Icon::Trophy, Rgba(234, 179, 8) };
    if (key == "gp_finishes") return { nullptr, nullptr, M::Icon::TrophyAward, Rgba(249, 115, 22) };
    if (key == "gadget_slots") return { nullptr, nullptr, M::Icon::Flash, Rgba(236, 72, 153) };
    if (key == "player_icon") return { nullptr, nullptr, M::Icon::Account, Rgba(100, 116, 139) };
    // The statistics' race totals
    if (key.rfind("stat_", 0) == 0) {
        static const std::unordered_map<std::string, std::pair<M::Icon, Color>> stats = {
            { "stat_TotalDrivingDistance", { M::Icon::Steering, Rgba(14, 165, 233) } },
            { "stat_TotalPlayingTime", { M::Icon::ClockOutline, Rgba(99, 102, 241) } },
            { "stat_TotalRacesCount", { M::Icon::FlagCheckered, Rgba(16, 185, 129) } },
            { "stat_TotalVictoryCount", { M::Icon::Trophy, Rgba(234, 179, 8) } },
            { "stat_StartDashCount", { M::Icon::Rocket, Rgba(249, 115, 22) } },
            { "stat_DriftDashCount", { M::Icon::Tire, Rgba(59, 130, 246) } },
            { "stat_AirTrickCount", { M::Icon::StarFourPoints, Rgba(168, 85, 247) } },
            { "stat_DashPanelCount", { M::Icon::Flash, Rgba(234, 179, 8) } },
            { "stat_SafetyDriverCount", { M::Icon::ShieldLock, Rgba(20, 184, 166) } },
            { "stat_SpecialItemBoxCount", { M::Icon::Star, Rgba(236, 72, 153) } },
            { "stat_UsedItemCount", { M::Icon::Puzzle, Rgba(139, 92, 246) } },
            { "stat_AttackItemCount", { M::Icon::Bullhorn, Rgba(239, 68, 68) } },
            { "stat_BackAttackItemCount", { M::Icon::SwapHorizontal, Rgba(244, 63, 94) } },
            { "stat_RingsCount", { M::Icon::StarCircle, Rgba(234, 179, 8) } },
            { "stat_ShortCutRingCount", { M::Icon::TrendingUp, Rgba(16, 185, 129) } },
            { "stat_SelectedTravelRingCount", { M::Icon::Web, Rgba(14, 165, 233) } },
            { "stat_TimeTrialCount", { M::Icon::TimerOutline, Rgba(99, 102, 241) } },
        };
        const auto it = stats.find(key);
        return it != stats.end() ? CurrencyLook{ nullptr, nullptr, it->second.first, it->second.second }
                                 : CurrencyLook{ nullptr, nullptr, M::Icon::Counter, Rgba(99, 102, 241) };
    }
    return { nullptr, nullptr, M::Icon::StarFourPoints, Rgba(168, 85, 247) };   // festival points
}

} // namespace

// A number's card: its picture (the game's, else an icon) and name, the amount (counting to its new value),
// and along the bottom a box to type a new one (written when it loses the focus) with Max. `big`: a big
// picture and amount (the currencies you spend).
void SaveEditorTab::NumberCard(UI::Ui& ui, const SF::NumberField& f, const Rect& c, bool big, double reveal, int order) {
    const M::Palette& p = M::P();
    M::FieldStyle field;
    field.outlined = true;
    field.fontSize = 13.f;
    field.align = HAlign::Right;
    const float fieldH = M::FieldHeight(ui, field);
    TextStyle label = Style(13.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    label.valign = VAlign::Center;
    label.ellipsis = true;
    const CurrencyLook look = LookOf(f.key);
    M::Motion::Enter enter(ui, M::Motion::Stagger(ui, reveal, order, 0.03f, 0.35f), 14.f);
    const UI::Id id = ui.MakeId("se-cur", static_cast<int64_t>(std::hash<std::string>{}(f.key) & 0x7FFFFFFF));
    const auto it = ui.Interact(id, c);
    const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, false);
    Card(ui, c, kRadius, look.tone, fb.hover * 0.6f);
    // Picture, name and amount
    const float pic = big ? 56.f : 36.f;
    const Rect pr{ c.x + 16.f, c.y + 16.f, pic, pic };
    ui.g.FillEllipse(pr.CX(), pr.CY(), pic * 0.5f + 4.f, pic * 0.5f + 4.f, look.tone.Fade(0.14f));
    const ImagePtr image = look.kind ? GameImage(look.kind, look.image, 128) : nullptr;
    if (image) ui.g.DrawImage(*image, pr.Inset(-2.f * fb.hover), 1.f, Stretch::Uniform);
    else M::DrawIcon(ui.g, look.icon, pr.CX(), pr.CY(), pic * 0.55f, TabKit::Readable(look.tone));
    const float tx = pr.R() + 14.f;
    ui.g.Text(Loc::Get("savefile_num_" + f.key), { tx, c.y + 14.f, c.R() - tx - 16.f, 20.f }, p.foreground.Fade(0.7f), label);
    TextStyle amount = Style(big ? 30.f : 22.f, DWRITE_FONT_WEIGHT_BOLD);
    amount.valign = VAlign::Center;
    amount.ellipsis = true;
    const long long n = Counter(ui, UI::HashId(id, 0xC0), f.value, 0.6f);
    ui.g.Text(Grouped(n), { tx, c.y + 34.f, c.R() - tx - 16.f, big ? 40.f : 28.f }, p.foreground, amount);
    // The box and Max, along the bottom
    const std::wstring maxText = Loc::Get("saveeditor_max");
    const float mw = TabKit::PillWidth(ui, maxText);
    const Rect maxR{ c.R() - 16.f - mw, c.B() - 14.f - 34.f, mw, 34.f };
    const Rect box{ c.x + 16.f, c.B() - 14.f - fieldH, maxR.x - 10.f - c.x - 16.f, fieldH };
    const std::string key = f.key;
    const int64_t lo = f.min, hi = f.max;
    const UI::Id boxId = UI::HashId(id, 0xB0);
    M::BoundTextField(
        ui, boxId, box, Text::FromInt(f.value),
        [this, key, lo, hi](const std::wstring& text) {
            std::wstring digits;
            for (wchar_t ch : text)
                if (ch >= L'0' && ch <= L'9') digits.push_back(ch);   // (typed with its separators too)
            const auto v = Text::ToInt(digits);
            if (!v) return;
            Edit([&] { save->SetNumber(key, std::clamp<int64_t>(*v, lo, hi)); });
        },
        field, !busy);
    ui.Tooltip(boxId, Loc::Fmt("saveeditor_range", Grouped(f.min), Grouped(f.max)));
    if (TabKit::PillButton(ui, UI::HashId(id, 0x3A), maxR, M::Icon::ArrowUp, maxText, look.tone, false,
                           !busy && f.suggested > 0 && f.value != f.suggested)) {
        const int64_t v = f.suggested;
        Edit([&] { save->SetNumber(key, v); });
    }
}

// The Currencies section: a strip saying what they are, then what you spend (tickets, Duel Medals, Red Star
// Rings, gadget fragments) as big cards and what the game counts (earned / spent, festival points) as
// smaller ones. A card: the currency's picture and name, its amount, and a box to type a new one with Max.
void SaveEditorTab::CurrencyView(UI::Ui& ui, const Rect& r) {
    const M::Palette& p = M::P();
    const Color gold = Rgba(234, 179, 8);
    UI::ScrollArea scroll(ui, ui.MakeId("se-items"), r, false, 60.f, UI::ScrollArea::Bar::Material);
    const float w = scroll.ContentWidth() - (scroll.BarShown() ? 10.f : 0.f);
    float y = r.y;

    // What they are
    {
        const Rect strip{ r.x, y, w, 64.f };
        Card(ui, strip, kRadius, gold, 0.f);
        float tx = strip.x + 22.f;
        if (const ImagePtr ticket = GameImage(L"currency", L"tickets", 128)) {
            ui.g.DrawImage(*ticket, { strip.x + 14.f, strip.y + 8.f, 72.f, 48.f }, 1.f, Stretch::Uniform);
            tx = strip.x + 98.f;
        }
        TextStyle title = Style(16.f, DWRITE_FONT_WEIGHT_BOLD);
        title.ellipsis = true;
        TextStyle hint = Style(12.f, DWRITE_FONT_WEIGHT_NORMAL, true);
        hint.ellipsis = true;
        ui.g.Text(Loc::Get("saveeditor_section_currency"), { tx, strip.y + 10.f, strip.R() - tx - 20.f, 22.f }, p.foreground, title);
        ui.g.Text(Loc::Get("saveeditor_currency_hint"), { tx, strip.y + 32.f, strip.R() - tx - 20.f, 24.f }, p.foreground.Fade(0.6f), hint);
        y = strip.B() + kGap + 6.f;
    }

    std::vector<SF::NumberField> spend, history;
    for (const auto& f : save->Numbers()) {
        if (!f.currency) continue;
        const bool spent = f.key == "donpa_tickets" || f.key == "duel_medals" || f.key == "red_star_rings" || f.key == "gadget_fragments";
        (spent ? spend : history).push_back(f);
    }
    TextStyle group = Style(12.f, DWRITE_FONT_WEIGHT_BOLD);
    group.valign = VAlign::Center;
    const double reveal = std::max(loadedAt, screenAt) + 0.12;
    int shown = 0;

    // A group: its title, then its cards in as many columns as fit
    const auto groupOf = [&](const char* titleKey, const std::vector<SF::NumberField>& fields, bool big) {
        if (fields.empty()) return;
        ui.g.Text(Loc::Get(titleKey), { r.x + 4.f, y, w, 22.f }, p.foreground.Fade(0.55f), group);
        y += 26.f;
        const float minW = big ? 300.f : 260.f;
        const int cols = std::clamp(static_cast<int>((w + kGap) / (minW + kGap)), 1, static_cast<int>(fields.size()));
        const float cw = (w - (cols - 1) * kGap) / cols;
        const float ch = big ? 168.f : 136.f;
        for (size_t k = 0; k < fields.size(); ++k)
            NumberCard(ui, fields[k], { r.x + (k % cols) * (cw + kGap), y + (k / cols) * (ch + kGap), cw, ch }, big, reveal, shown++);
        y += ((fields.size() + cols - 1) / cols) * (ch + kGap) + 8.f;
    };
    groupOf("saveeditor_currency_spend", spend, true);
    groupOf("saveeditor_currency_history", history, false);
    scroll.SetContentSize(y - r.y);
}

} // namespace SonicRacingSaveManager::Main
