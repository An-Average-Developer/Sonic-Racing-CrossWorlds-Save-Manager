#include <algorithm>
#include "Core/Text.h"
#include "Data/GadgetCatalog.h"
#include "Localization/Localization.h"
#include "UI/Material/Buttons.h"
#include "UI/Material/Motion.h"
#include "UI/Material/Theme.h"
#include "UI/Widgets/ScrollArea.h"
#include "Windows/Main/Tabs/SaveEditor/SaveEditorTab.h"
#include "Windows/Main/Tabs/TabKit.h"

// The gadgets section: a local player's 5 gadget presets (the one raced with marked), each a plate of
// 6 slots where a gadget takes 1..4 side by side, beside the gadgets to put on the preset being edited
// (by kind; a click puts one in the first place it fits, or takes it off). A tag switches to the list of
// every gadget (unlock / lock).
namespace SonicRacingSaveManager::Main {

namespace M = UI::Material;
namespace SF = Services::SaveFile;
using namespace SaveEditorKit;
using TabKit::Style;

namespace {

constexpr float kGadgetBarH = 38.f, kPresetH = 124.f, kPresetsW = 600.f;
constexpr float kPickW = 118.f, kPickH = 136.f;

// A gadget's kind by its id's start: 0 Run (driving), 1 Ring, 2 Item, 3 Various (kits)
int KindOf(const std::wstring& id) {
    if (id.starts_with(L"Run")) return 0;
    if (id.starts_with(L"Ring")) return 1;
    if (id.starts_with(L"Item")) return 2;
    return 3;
}
constexpr Color kKindTone[4] = { Rgba(59, 130, 246), Rgba(234, 179, 8), Rgba(168, 85, 247), Rgba(20, 184, 166) };
constexpr M::Icon kKindIcon[5] = { M::Icon::Flash, M::Icon::CarSports, M::Icon::StarFourPoints, M::Icon::Creation, M::Icon::Wrench };
constexpr const char* kKindKey[4] = { "saveeditor_gadget_kind_run", "saveeditor_gadget_kind_ring", "saveeditor_gadget_kind_item",
                                      "saveeditor_gadget_kind_kit" };

// The first slot from which `size` free, unlocked slots follow; -1 when it doesn't fit
int FirstFit(const SF::GadgetPreset& p, int size, int unlocked) {
    bool used[SF::kGadgetPlate] = {};
    for (const auto& g : p.gadgets)
        for (int s = g.slot; s < g.slot + g.size && s < SF::kGadgetPlate; ++s)
            if (s >= 0) used[s] = true;
    for (int s = 0; s + size <= unlocked; ++s) {
        bool free = true;
        for (int k = s; k < s + size; ++k) free &= !used[k];
        if (free) return s;
    }
    return -1;
}
int FreeSlots(const SF::GadgetPreset& p, int unlocked) {
    int used = 0;
    for (const auto& g : p.gadgets) used += g.size;
    return std::max(0, unlocked - used);
}

// A preset by its letter, as the game names them: A..J
std::wstring PresetName(int index) { return Loc::Fmt("saveeditor_gadget_preset", std::wstring(1, static_cast<wchar_t>(L'A' + index))); }

constexpr Color kSpecialTone = Rgba(234, 179, 8);   // the special slot: gold

// Its slots as little squares (filled), `size` of them
void SlotPips(UI::Ui& ui, float x, float cy, int size, Color tone) {
    for (int k = 0; k < size; ++k) ui.g.FillRound({ x + k * 11.f, cy - 4.f, 8.f, 8.f }, 2.f, tone);
}

} // namespace

const std::vector<SF::GadgetPreset>& SaveEditorTab::GadgetPresets() {
    const int key = version * 8 + gadgetPlayer;
    if (gadgetPresetsFor == key) return gadgetPresets;
    gadgetPresetsFor = key;
    gadgetPresets = save ? save->GadgetPresets(gadgetPlayer) : std::vector<SF::GadgetPreset>{};
    return gadgetPresets;
}

// One preset: its name, In use / Use, Empty; its plate (locked slots with a lock, each gadget over its
// slots: a click takes it off). A click on the card edits it.
void SaveEditorTab::GadgetPresetCard(UI::Ui& ui, UI::Id id, const Rect& r, const SF::GadgetPreset& preset, bool editing) {
    const M::Palette& p = M::P();
    const Color tone = preset.selected ? kGreen : GroupColor(2);
    const auto it = ui.Interact(id, r, !busy);
    const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, false);
    Card(ui, r, kRadius, tone, editing ? 0.f : fb.hover * 0.7f);
    if (editing) ui.g.StrokeRound(r.Inset(0.5f), kRadius - 0.5f, kAmber.Fade(0.85f), 2.f);
    else if (preset.selected) ui.g.StrokeRound(r.Inset(0.5f), kRadius - 0.5f, kGreen.Fade(0.5f), 1.5f);
    if (it.hovered && !editing) ui.SetCursor(IDC_HAND);
    if (it.clicked) {
        gadgetPreset = preset.index;
        gadgetSpecial = false;
    }

    // Header
    TextStyle title = Style(15.f, DWRITE_FONT_WEIGHT_BOLD);
    title.valign = VAlign::Center;
    const float hy = r.y + 12.f;
    const std::wstring name = PresetName(preset.index);
    const float nw = std::ceil(ui.g.Measure(name, title).w);
    ui.g.Text(name, { r.x + 16.f, hy, nw + 2.f, 30.f }, p.foreground, title);
    float x = r.x + 16.f + nw + 10.f;
    if (editing) x += Chip(ui, x, hy + 15.f, Loc::Get("saveeditor_gadget_editing"), kAmber, false, true) + 6.f;
    const int unlocked = save->GadgetSlotCount();
    const int freeSlots = FreeSlots(preset, unlocked);
    Chip(ui, x, hy + 15.f, Loc::Fmt("saveeditor_gadget_free", std::to_wstring(freeSlots)), p.foreground.Fade(0.6f));

    float right = r.R() - 14.f;
    if (!preset.gadgets.empty() || !preset.special.empty()) {
        const UI::Id cid = UI::HashId(id, 0xC1);
        right -= 32.f;
        if (M::IconButton(ui, cid, { right, hy - 1.f, 32.f, 32.f }, M::Icon::Delete, 17.f, M::IconButtonLook(kAmber), !busy)) {
            const int player = gadgetPlayer, index = preset.index;
            Edit([&] { save->ClearGadgetPreset(player, index); });
            Status(Loc::Fmt("saveeditor_gadget_cleared", name));
        }
        ui.Tooltip(cid, Loc::Get("saveeditor_gadget_clear"));
        right -= 6.f;
    }
    if (preset.selected) {
        const std::wstring& inUse = Loc::Get("saveeditor_build_selected");
        right -= ChipWidth(ui, inUse, true);
        Chip(ui, right, hy + 15.f, inUse, kGreen, true, true);
    } else {
        const std::wstring& use = Loc::Get("saveeditor_build_use");
        const float bw = TabKit::PillWidth(ui, use);
        right -= bw;
        if (TabKit::PillButton(ui, UI::HashId(id, 0x05E), { right, hy - 2.f, bw, 34.f }, M::Icon::Flash, use, GroupColor(2), false, !busy)) {
            const int player = gadgetPlayer, index = preset.index;
            Edit([&] { save->SelectGadgetPreset(player, index); });
            Status(Loc::Fmt("saveeditor_gadget_now", name));
        }
    }

    // The plate
    const float gap = 6.f;
    const Rect area{ r.x + 14.f, r.y + 52.f, r.w - 28.f, r.h - 52.f - 14.f };
    // The special slot (Season 2): a gold cell past the plate, the size of one of its slots
    const float slotW = (area.w - 12.f - SF::kGadgetPlate * gap) / (SF::kGadgetPlate + 1);
    const Rect plate{ area.x, area.y, area.w - slotW - 12.f, area.h };
    {
        const Rect sp{ plate.R() + 12.f, area.y, slotW, area.h };
        const bool open = save->SpecialSlotOpen();
        const bool picking = editing && gadgetSpecial;
        const UI::Id sid = UI::HashId(id, 0x5B);
        const auto sit = ui.Interact(sid, sp, !busy && open);
        const M::Motion::Feedback sfb = M::Motion::Hover(ui, sid, sit.hovered, sit.held);
        ui.g.FillRound(sp, 10.f, open ? kSpecialTone.Fade(0.12f + 0.1f * sfb.hover) : p.foreground.Fade(0.06f));
        ui.g.StrokeRound(sp.Inset(0.75f), 9.25f, open ? kSpecialTone.Fade(picking ? 0.95f : 0.55f) : p.foreground.Fade(0.12f), picking ? 2.f : 1.5f);
        if (!open) {
            M::DrawIcon(ui.g, StateIcon(SF::ItemState::Locked), sp.CX(), sp.CY(), 18.f, p.foreground.Fade(0.3f));
            ui.Tooltip(sid, Loc::Get("saveeditor_gadget_special_locked"));
        } else if (preset.special.empty()) {
            M::DrawIcon(ui.g, M::Icon::PlusCircle, sp.CX(), sp.CY(), 22.f, TabKit::Readable(kSpecialTone).Fade(0.7f + 0.3f * sfb.hover));
            ui.Tooltip(sid, Loc::Get("saveeditor_gadget_special") + L"\n" + Loc::Get("saveeditor_gadget_special_pick"));
        } else {
            const float side = std::min(sp.w, sp.h) - 8.f;
            if (const ImagePtr image = Picture(SF::ListId::Gadgets, preset.special))
                ui.g.DrawImage(*image, { sp.CX() - side * 0.5f, sp.CY() - side * 0.5f, side, side }, 1.f, Stretch::Uniform);
            if (picking && sfb.hover > 0.01f) {   // (being filled: a click takes it out)
                ui.g.FillEllipse(sp.R() - 9.f, sp.y + 9.f, 8.f, 8.f, kAmber.Fade(sfb.hover));
                M::DrawIcon(ui.g, M::Icon::Close, sp.R() - 9.f, sp.y + 9.f, 11.f, Colors::White.Fade(sfb.hover));
            }
            ui.Tooltip(sid, Loc::Get("saveeditor_gadget_special") + L": " + GadgetName(preset.special) + L"\n" +
                                Data::GadgetCatalog::SpecialAbout(preset.special, Loc::CurrentLang()) + L"\n" + Loc::Get("saveeditor_gadget_take_off"));
        }
        if (sit.hovered) ui.SetCursor(IDC_HAND);
        if (sit.clicked) {
            gadgetPreset = preset.index;
            if (!preset.special.empty() && picking) {
                // On the slot being filled: a click takes its gadget out
                const int player = gadgetPlayer, index = preset.index;
                const std::wstring was = GadgetName(preset.special);
                Edit([&] { save->SetSpecialGadget(player, index, L""); });
                Status(Loc::Fmt("saveeditor_gadget_removed", was));
            }
            gadgetSpecial = true;
        }
    }
    const float cw = (plate.w - (SF::kGadgetPlate - 1) * gap) / SF::kGadgetPlate;
    const auto cellX = [&](int s) { return plate.x + s * (cw + gap); };
    for (int s = 0; s < SF::kGadgetPlate; ++s) {
        const Rect cell{ cellX(s), plate.y, cw, plate.h };
        if (s >= unlocked) {
            ui.g.FillRound(cell, 10.f, p.foreground.Fade(0.06f));
            M::DrawIcon(ui.g, StateIcon(SF::ItemState::Locked), cell.CX(), cell.CY(), 18.f, p.foreground.Fade(0.3f));
        } else {
            ui.g.FillRound(cell, 10.f, tone.Fade(0.05f));
            ui.g.StrokeRound(cell.Inset(0.5f), 9.5f, p.foreground.Fade(0.12f), 1.f);
        }
    }
    TextStyle gname = Style(11.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    gname.valign = VAlign::Center;
    gname.ellipsis = true;
    for (const auto& g : preset.gadgets) {
        if (g.slot < 0 || g.slot >= SF::kGadgetPlate) continue;
        const int span = std::min(g.size, SF::kGadgetPlate - g.slot);
        const Rect gr{ cellX(g.slot), plate.y, span * cw + (span - 1) * gap, plate.h };
        const UI::Id gid = UI::HashId(id, 0x100 + g.slot);
        const auto git = ui.Interact(gid, gr, !busy);
        const M::Motion::Feedback gfb = M::Motion::Hover(ui, gid, git.hovered, git.held);
        const Color kt = kKindTone[KindOf(g.id)];
        const float burst = bursts.Progress(g.id, ui.Time());
        ui.g.FillRound(gr, 10.f, kt.Fade(0.2f + 0.12f * gfb.hover));
        ui.g.StrokeRound(gr.Inset(0.5f), 9.5f, kt.Fade(0.6f), 1.5f);
        const float side = std::min(gr.h - 8.f, 56.f) * (burst >= 0.f ? BurstScale(burst) : 1.f);
        const std::wstring gn = GadgetName(g.id);
        // Wider than one slot: as the game draws it, on the frame as wide as its slots (its name in the tooltip)
        const ImagePtr plateImage = g.size >= 2 ? GadgetPlatePicture(g.id) : nullptr;
        const bool wide = !plateImage && gr.w > side + 50.f;
        const Rect pic = plateImage ? gr.Inset(3.f) : wide ? Rect{ gr.x + 4.f, gr.CY() - side * 0.5f, side, side }
                                                           : Rect{ gr.CX() - side * 0.5f, gr.CY() - side * 0.5f, side, side };
        if (plateImage) ui.g.DrawImage(*plateImage, pic, 1.f, Stretch::Uniform);
        else if (const ImagePtr image = Picture(SF::ListId::Gadgets, g.id)) ui.g.DrawImage(*image, pic, 1.f, Stretch::Uniform);
        else M::DrawIcon(ui.g, M::Icon::Flash, pic.CX(), pic.CY(), 22.f, TabKit::Readable(kt));
        if (burst >= 0.f) BurstRing(ui, pic.CX(), pic.CY(), side * 0.6f, burst, kt);
        if (wide) ui.g.Text(gn, { pic.R() + 6.f, gr.y, gr.R() - pic.R() - 12.f, gr.h }, p.foreground, gname);
        // A cross on hover: a click takes it off
        if (gfb.hover > 0.01f) {
            ui.g.FillEllipse(gr.R() - 9.f, gr.y + 9.f, 8.f, 8.f, kAmber.Fade(gfb.hover));
            M::DrawIcon(ui.g, M::Icon::Close, gr.R() - 9.f, gr.y + 9.f, 11.f, Colors::White.Fade(gfb.hover));
        }
        if (git.hovered) ui.SetCursor(IDC_HAND);
        ui.Tooltip(gid, gn + L"  (" + Loc::Fmt("saveeditor_gadget_slots", std::to_wstring(g.size)) + L")\n" +
                            Data::GadgetCatalog::About(g.id, Loc::CurrentLang()) + L"\n" + Loc::Get("saveeditor_gadget_take_off"));
        if (git.clicked) {
            const int player = gadgetPlayer, index = preset.index, slot = g.slot;
            gadgetPreset = preset.index;
            Edit([&] { save->RemoveGadget(player, index, slot); });
            Status(Loc::Fmt("saveeditor_gadget_removed", gn));
        }
    }
}

std::wstring SaveEditorTab::GadgetName(const std::wstring& id) {
    for (const auto& row : GadgetRows())
        if (row.id == id) return row.name;
    return id;
}

const std::vector<SF::ItemRow>& SaveEditorTab::GadgetRows() {
    if (gadgetRowsFor == version) return gadgetRows;
    gadgetRowsFor = version;
    gadgetRows.clear();
    if (!save) return gadgetRows;
    for (auto& row : save->Rows(SF::ListId::Gadgets)) {
        const bool owned = row.state == SF::ItemState::Unlocked;
        // Not the empty slot, nor what isn't in the game yet, nor what can't be had (DLC Steam gives)
        if (row.id == L"NonGadget" || row.unreleased || (!owned && (row.dlc || !row.canUnlock))) continue;
        gadgetRows.push_back(std::move(row));
    }
    // By kind, then the game's order
    std::stable_sort(gadgetRows.begin(), gadgetRows.end(), [](const auto& a, const auto& b) { return KindOf(a.id) < KindOf(b.id); });
    return gadgetRows;
}

// The gadgets to put on the edited preset: tags by kind, then a tile each (picture, name, its slots);
// on the plate already: marked (a click takes it off); not fitting: faded; not had yet: a lock (putting
// it on unlocks it)
void SaveEditorTab::GadgetPicker(UI::Ui& ui, const Rect& r, const SF::GadgetPreset& preset) {
    const M::Palette& p = M::P();
    TextStyle head = Style(15.f, DWRITE_FONT_WEIGHT_BOLD);
    head.valign = VAlign::Center;
    const std::wstring presetName = PresetName(preset.index);
    const bool special = gadgetSpecial && save->SpecialSlotOpen();
    float headRight = r.R();
    if (special) {
        // Back to the plate
        const std::wstring& back = Loc::Get("saveeditor_gadget_back_plate");
        const float bw = TabKit::PillWidth(ui, back);
        headRight -= bw;
        if (TabKit::PillButton(ui, ui.MakeId("se-gadget-back-plate"), { headRight, r.y - 4.f, bw, 34.f }, M::Icon::ChevronLeft, back, GroupColor(2), false, !busy))
            gadgetSpecial = false;
        headRight -= 10.f;
    }
    ui.g.Text(Loc::Fmt(special ? "saveeditor_gadget_special_add_to" : "saveeditor_gadget_add_to", presetName), { r.x + 4.f, r.y, headRight - r.x - 4.f, 28.f },
              special ? TabKit::Readable(kSpecialTone) : p.foreground, head);
    TextStyle hint = Style(12.f, DWRITE_FONT_WEIGHT_NORMAL, true);
    ui.g.Text(Loc::Get(special ? "saveeditor_gadget_special_hint" : "saveeditor_gadget_hint"), { r.x + 4.f, r.y + 28.f, r.w - 8.f, 36.f },
              p.foreground.Fade(0.55f), hint);

    // Tags: all, then each kind (had / total). They fit the picker's width: without their counts when
    // they don't (the counts go to the tooltip), and as icons only when even that is too wide
    const auto& all = GadgetRows();
    const float ty = r.y + 68.f;
    std::wstring names[5], counted[5];
    for (int k = -1; k < 4; ++k) {
        int done = 0, total = 0;
        for (const auto& row : all)
            if (k < 0 || KindOf(row.id) == k) {
                ++total;
                done += row.state == SF::ItemState::Unlocked;
            }
        names[k + 1] = Loc::Get(k < 0 ? "saveeditor_tag_all" : kKindKey[k]);
        counted[k + 1] = names[k + 1] + L"   " + std::to_wstring(done) + L" / " + std::to_wstring(total);
    }
    const auto rowWidth = [&](const std::wstring* texts) {
        float w = 0.f;
        for (int i = 0; i < 5; ++i) w += (texts ? TabKit::PillWidth(ui, texts[i]) : 46.f) + 8.f;
        return w - 8.f;
    };
    const std::wstring* shown = rowWidth(counted) <= r.w ? counted : rowWidth(names) <= r.w ? names : nullptr;
    float tx = r.x;
    for (int k = -1; k < 4; ++k) {
        const std::wstring text = shown ? shown[k + 1] : L"";
        const float tw = shown ? TabKit::PillWidth(ui, text) : 46.f;
        const bool on = gadgetTag == k;
        const UI::Id tagId = ui.MakeId("se-gadget-tag", k + 1);
        if (TabKit::PillButton(ui, tagId, { tx, ty, tw, 34.f }, kKindIcon[k + 1], text, k < 0 ? GroupColor(2) : kKindTone[k], on, !on)) {
            gadgetTag = k;
            UI::ScrollArea::ScrollTo(ui, ui.MakeId("se-gadget-pick"), 0.f);
        }
        if (shown != counted) ui.Tooltip(tagId, counted[k + 1]);
        tx += tw + 8.f;
    }

    const int unlocked = save->GadgetSlotCount();
    const Rect grid{ r.x, ty + 34.f + kGap, r.w, r.B() - ty - 34.f - kGap };
    UI::ScrollArea scroll(ui, ui.MakeId("se-gadget-pick"), grid, false, 70.f, UI::ScrollArea::Bar::Thin);
    const float avail = scroll.ContentWidth() - 4.f;
    const int cols = std::max(1, static_cast<int>((avail + 10.f) / (kPickW + 10.f)));
    const float tw = (avail - (cols - 1) * 10.f) / cols;
    TextStyle name = Style(11.f, DWRITE_FONT_WEIGHT_SEMI_BOLD, false, HAlign::Center);
    name.valign = VAlign::Center;
    name.ellipsis = true;
    int n = 0;
    for (const auto& row : all) {
        if (gadgetTag >= 0 && KindOf(row.id) != gadgetTag) continue;
        if (special && !Data::GadgetCatalog::IsSpecial(row.id)) continue;
        const Rect tile{ grid.x + (n % cols) * (tw + 10.f), grid.y + (n / cols) * (kPickH + 10.f) + 2.f, tw, kPickH };
        ++n;
        if (tile.B() < grid.y + scroll.Offset() - 10.f || tile.y > grid.B() + scroll.Offset() + 10.f) continue;
        const int size = Data::GadgetCatalog::Slots(row.id);
        // On: in the place being filled; elsewhere: in the preset's other place (the plate / the special slot)
        bool onPlate = false;
        for (const auto& g : preset.gadgets) onPlate |= g.id == row.id;
        const bool inSpecial = preset.special == row.id;
        const bool on = special ? inSpecial : onPlate;
        const bool elsewhere = special ? onPlate : inSpecial;
        const int fit = on || elsewhere ? -1 : special ? 0 : FirstFit(preset, size, unlocked);
        const bool owned = row.state == SF::ItemState::Unlocked;
        const UI::Id id = ui.MakeId("se-gadget-pick-tile", static_cast<int64_t>(std::hash<std::wstring>{}(row.id) & 0x7FFFFFFF));
        const auto it = ui.Interact(id, tile, !busy);
        const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, it.held);
        const Color kt = kKindTone[KindOf(row.id)];
        const Color tone = on ? kGreen : kt;
        const float alpha = on || fit >= 0 ? 1.f : 0.4f + 0.3f * fb.hover;
        Card(ui, tile, 12.f, tone, on ? 0.f : fb.hover, alpha);
        if (on) ui.g.StrokeRound(tile.Inset(0.5f), 11.5f, kGreen.Fade(0.7f), 2.f);
        const Rect pic{ tile.x + 10.f, tile.y + 10.f, tile.w - 20.f, 70.f };
        ui.g.FillRound(pic, 8.f, tone.Fade(0.1f * alpha));
        const float burst = bursts.Progress(row.id, ui.Time());
        const float grow = 3.f * fb.hover;
        const Rect img = pic.Inset(6.f - grow);
        const float pa = (owned ? 1.f : 0.55f + 0.45f * fb.hover) * alpha;
        if (const ImagePtr image = Picture(SF::ListId::Gadgets, row.id)) {
            const float s = burst >= 0.f ? BurstScale(burst) : 1.f;
            const Rect scaled{ img.CX() - img.w * s * 0.5f, img.CY() - img.h * s * 0.5f, img.w * s, img.h * s };
            ui.g.DrawImage(*image, scaled, pa, Stretch::Uniform);
        } else M::DrawIcon(ui.g, M::Icon::Flash, pic.CX(), pic.CY(), 28.f + grow, p.foreground.Fade(0.45f * alpha));
        if (burst >= 0.f) BurstRing(ui, pic.CX(), pic.CY(), pic.h * 0.5f, burst, tone);
        if (on) {
            ui.g.FillEllipse(pic.R() - 2.f, pic.y + 2.f, 10.f, 10.f, kGreen);
            M::DrawIcon(ui.g, M::Icon::Check, pic.R() - 2.f, pic.y + 2.f, 12.f, Colors::White);
        } else if (!owned) {
            ui.g.FillEllipse(pic.R() - 2.f, pic.y + 2.f, 10.f, 10.f, kAmber.Fade(alpha));
            M::DrawIcon(ui.g, StateIcon(SF::ItemState::Locked), pic.R() - 2.f, pic.y + 2.f, 12.f, Colors::White.Fade(alpha));
        }
        ui.g.Text(row.name, { tile.x + 4.f, pic.B() + 6.f, tile.w - 8.f, 20.f }, p.foreground.Fade((on ? 1.f : owned ? 0.8f : 0.55f) * alpha), name);
        if (special) SlotPips(ui, tile.CX() - 4.f, pic.B() + 36.f, 1, kSpecialTone.Fade(alpha));
        else SlotPips(ui, tile.CX() - (size * 11.f - 3.f) * 0.5f, pic.B() + 36.f, size, kt.Fade(alpha));

        std::wstring tip = row.name + (special ? L"" : L"  (" + Loc::Fmt("saveeditor_gadget_slots", std::to_wstring(size)) + L")");
        const std::wstring about = special ? Data::GadgetCatalog::SpecialAbout(row.id, Loc::CurrentLang()) : Data::GadgetCatalog::About(row.id, Loc::CurrentLang());
        if (!about.empty()) tip += L"\n" + about;
        if (on) tip += L"\n" + Loc::Get("saveeditor_gadget_take_off");
        else if (elsewhere) tip += L"\n" + Loc::Get(special ? "saveeditor_gadget_on_plate" : "saveeditor_gadget_in_special");
        else if (fit < 0) tip += L"\n" + Loc::Fmt("saveeditor_gadget_no_room", std::to_wstring(size), std::to_wstring(FreeSlots(preset, unlocked)));
        else if (!owned) tip += L"\n" + Loc::Get("saveeditor_build_will_unlock");
        ui.Tooltip(id, tip);
        if (it.hovered && (on || fit >= 0)) ui.SetCursor(IDC_HAND);
        if (it.clicked) {
            const int player = gadgetPlayer, index = preset.index;
            const std::wstring gid = row.id;
            if (on && special) {
                Edit([&] { save->SetSpecialGadget(player, index, L""); });
                Status(Loc::Fmt("saveeditor_gadget_removed", row.name));
            } else if (on) {
                int slot = 0;
                for (const auto& g : preset.gadgets)
                    if (g.id == gid) slot = g.slot;
                Edit([&] { save->RemoveGadget(player, index, slot); });
                Status(Loc::Fmt("saveeditor_gadget_removed", row.name));
            } else if (elsewhere) {
                Status(Loc::Get(special ? "saveeditor_gadget_on_plate" : "saveeditor_gadget_in_special"), true);
            } else if (special) {
                Edit([&] {
                    if (!owned) save->Unlock(SF::ListId::Gadgets, gid);
                    save->SetSpecialGadget(player, index, gid);
                });
                bursts.Fire(gid, ui.Time());
                animateUntil = std::max(animateUntil, ui.Time() + kBurstSeconds);
                Status(Loc::Fmt("saveeditor_gadget_special_added", row.name));
            } else if (fit < 0) {
                Status(Loc::Fmt("saveeditor_gadget_no_room", std::to_wstring(size), std::to_wstring(FreeSlots(preset, unlocked))), true);
            } else {
                // What the save doesn't have is unlocked first, then put on
                Edit([&] {
                    if (!owned) save->Unlock(SF::ListId::Gadgets, gid);
                    save->PlaceGadget(player, index, gid, fit);
                });
                bursts.Fire(gid, ui.Time());
                animateUntil = std::max(animateUntil, ui.Time() + kBurstSeconds);
                Status(Loc::Fmt(owned ? "saveeditor_gadget_added" : "saveeditor_build_unlocked", row.name));
            }
        }
    }
    const int lines = (n + cols - 1) / cols;
    scroll.SetContentSize(lines * (kPickH + 10.f) + 4.f);
}

void SaveEditorTab::GadgetsView(UI::Ui& ui, const Rect& r) {
    const M::Palette& p = M::P();
    // The bar: the players (presets view), the views, and the plate's slots
    float x = r.x;
    const int cat = CategoryOf(SF::ListId::Gadgets);
    const Stat& st = Stats()[static_cast<size_t>(cat)];
    {
        const std::wstring presets = Loc::Get("saveeditor_gadget_presets");
        const std::wstring list = Loc::Get("saveeditor_gadget_list") + L"   " + std::to_wstring(st.done) + L" / " + std::to_wstring(st.total);
        const std::wstring* texts[2] = { &presets, &list };
        for (int k = 0; k < 2; ++k) {
            const float w = TabKit::PillWidth(ui, *texts[k]);
            const bool on = gadgetView == k;
            if (TabKit::PillButton(ui, ui.MakeId("se-gadget-view", k), { x, r.y, w, kGadgetBarH }, k == 0 ? M::Icon::Wrench : M::Icon::Flash, *texts[k],
                                   GroupColor(2), on, !on)) {
                gadgetView = k;
                if (k == 1) {
                    category = cat;
                    search.clear();
                    filter = 0;
                    shopTag = -1;
                    UI::ScrollArea::ScrollTo(ui, ui.MakeId("se-items"), 0.f);
                }
            }
            x += w + 10.f;
        }
    }
    if (gadgetView == 1) {
        category = cat;
        CategoryView(ui, { r.x, r.y + kGadgetBarH + kGap, r.w, r.h - kGadgetBarH - kGap });
        return;
    }
    // The players (split screen): each has its own presets
    x += 14.f;
    for (int k = 0; k < SF::kGadgetPlayers; ++k) {
        const std::wstring text = Loc::Fmt("saveeditor_gadget_player", std::to_wstring(k + 1));
        const float w = TabKit::PillWidth(ui, text);
        const bool on = gadgetPlayer == k;
        if (TabKit::PillButton(ui, ui.MakeId("se-gadget-player", k), { x, r.y, w, kGadgetBarH }, M::Icon::Account, text, GroupColor(1), on, !on)) {
            gadgetPlayer = k;
            gadgetPreset = -1;
            gadgetSpecial = false;
        }
        x += w + 8.f;
    }
    // The plate's unlocked slots, and unlocking them all
    {
        const int unlocked = save->GadgetSlotCount() + (save->SpecialSlotOpen() ? 1 : 0);
        float right = r.R();
        if (unlocked < SF::kGadgetSlotsAll) {
            const std::wstring& more = Loc::Get("saveeditor_gadget_unlock_slots");
            const float w = TabKit::PillWidth(ui, more);
            right -= w;
            if (TabKit::PillButton(ui, ui.MakeId("se-gadget-slots"), { right, r.y, w, kGadgetBarH }, StateIcon(SF::ItemState::Unlocked), more, kAmber, false, !busy)) {
                Edit([&] { save->SetNumber("gadget_slots", SF::kGadgetSlotsAll); });
                Status(Loc::Get("saveeditor_gadget_slots_done"));
            }
            right -= 10.f;
        }
        const std::wstring slots = Loc::Fmt("saveeditor_gadget_plate", std::to_wstring(unlocked), std::to_wstring(SF::kGadgetSlotsAll));
        right -= ChipWidth(ui, slots);
        if (right > x) Chip(ui, right, r.y + kGadgetBarH * 0.5f, slots, unlocked < SF::kGadgetSlotsAll ? kAmber : p.foreground.Fade(0.65f));
    }

    const Rect body{ r.x, r.y + kGadgetBarH + kGap, r.w, r.h - kGadgetBarH - kGap };
    const auto& presets = GadgetPresets();
    if (presets.empty()) {
        TextStyle note = Style(13.f);
        note.valign = VAlign::Center;
        ui.g.Text(Loc::Get("saveeditor_gadget_none"), body, p.foreground.Fade(0.55f), note);
        return;
    }
    // The preset edited: the one picked, else the one raced with
    if (gadgetPreset < 0 || gadgetPreset >= static_cast<int>(presets.size())) {
        gadgetPreset = 0;
        for (const auto& pr : presets)
            if (pr.selected) gadgetPreset = pr.index;
    }

    const float lw = std::min(kPresetsW, std::max(420.f, body.w * 0.5f));
    const Rect left{ body.x, body.y, lw, body.h };
    {
        UI::ScrollArea scroll(ui, ui.MakeId("se-gadget-presets"), left, false, 70.f, UI::ScrollArea::Bar::Thin);
        const float w = scroll.ContentWidth() - 6.f;
        const double reveal = std::max(loadedAt, screenAt) + 0.08;
        float y = left.y + 4.f;
        for (size_t k = 0; k < presets.size(); ++k) {
            M::Motion::Enter enter(ui, M::Motion::Stagger(ui, reveal, static_cast<int>(k), 0.05f, 0.35f), 12.f);
            GadgetPresetCard(ui, ui.MakeId("se-gadget-preset", gadgetPlayer * 10 + static_cast<int64_t>(k)), { left.x, y, w, kPresetH }, presets[k],
                             presets[k].index == gadgetPreset);
            y += kPresetH + 10.f;
        }
        scroll.SetContentSize(y - left.y);
    }
    GadgetPicker(ui, { left.R() + kGap + 6.f, body.y, body.R() - left.R() - kGap - 6.f, body.h }, presets[static_cast<size_t>(gadgetPreset)]);
}

} // namespace SonicRacingSaveManager::Main
