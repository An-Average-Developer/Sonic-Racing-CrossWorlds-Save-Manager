#include "Windows/Main/Tabs/SaveEditor/SaveEditorBuild.h"
#include <algorithm>
#include "Core/Text.h"
#include "Data/ColorCatalog.h"
#include "Localization/Localization.h"
#include "UI/Material/Buttons.h"
#include "UI/Material/Motion.h"
#include "UI/Material/Theme.h"
#include "UI/Widgets/ScrollArea.h"
#include "Windows/Main/Tabs/SaveEditor/SaveEditorTab.h"
#include "Windows/Main/Tabs/TabKit.h"

namespace SonicRacingSaveManager::Main {

namespace {
namespace M = UI::Material;
namespace SF = Services::SaveFile;
using namespace SaveEditorKit;
using TabKit::Style;

constexpr float kTop = 112.f, kChoiceW = 96.f, kChoiceH = 104.f, kRowTitleH = 26.f;

// A base machine's own colours ("machine" 01..10): the game lets every machine wear them
bool BaseLivery(const std::wstring& presetId) {
    const int number = static_cast<int>(Text::ToInt(presetId.substr(std::min<size_t>(presetId.size(), 11))).value_or(-1));
    for (const auto& c : Data::ColorCatalog::Presets())
        if (c.id == number && *c.machine) {
            const std::string m = c.machine;
            return Text::ToInt(m.substr(m.find('_') + 1)).value_or(99) <= 10;
        }
    return false;
}
} // namespace

void SaveEditorBuild::Show(const std::wstring& garageSlot) {
    slot = garageSlot;
    rowsFor = -1;
    Open();
}

float SaveEditorBuild::CardHeight(UI::Ui&, float) { return 700.f; }

const std::vector<SaveEditorBuild::Row>& SaveEditorBuild::Rows(const SF::MachineBuild& b) {
    if (rowsFor == tab.version) return rows;
    rowsFor = tab.version;
    rows.clear();
    const std::wstring type = slot.substr(0, slot.find(L'_')) + L"_";
    const bool boost = type == L"DSH_";
    // A custom machine (custom cars on and made with "Custom", or already mixing types): every type's parts
    const bool mixed = tab.konami && (tab.mixedSlots.contains(slot) || MixedBuild(b));
    // Everything the game has (the parts of the machine's own type), the one on it first: what the save
    // has, then what putting on would unlock; not what can't be unlocked (DLC)
    const auto choices = [&](SF::ListId list, const std::wstring& current, auto keep) {
        std::vector<Choice> have, more;
        std::wstring currentName = current;
        for (const auto& r : tab.save->Rows(list)) {
            if (r.id == current) {
                currentName = r.name;
                continue;
            }
            if (!keep(r)) continue;
            // (a base machine's own colours need no unlocking: every machine can wear them)
            const bool owned = r.state == SF::ItemState::Unlocked || (list == SF::ListId::ColorPresets && BaseLivery(r.id));
            // DLC only when the save has it: Steam gives it, the editor never does
            if (r.dlc && !owned) continue;
            if (owned) have.push_back({ r.id, r.name, true, r.dlc });
            else if (r.canUnlock) more.push_back({ r.id, r.name, false, false });
        }
        if (!current.empty()) have.insert(have.begin(), { current, currentName, true });
        have.insert(have.end(), more.begin(), more.end());
        return have;
    };
    const auto unlocked = [](const SF::ItemRow&) { return true; };
    const auto ownType = [&](const SF::ItemRow& r) { return mixed || r.id.starts_with(type); };
    rows.push_back({ SF::BuildPart::Front, SF::ListId::FrontParts, "saveeditor_part_front", choices(SF::ListId::FrontParts, b.front, ownType) });
    rows.push_back({ SF::BuildPart::Rear, SF::ListId::RearParts, "saveeditor_part_rear", choices(SF::ListId::RearParts, b.rear, ownType) });
    if (!boost) rows.push_back({ SF::BuildPart::Tire, SF::ListId::TireParts, "saveeditor_part_tire", choices(SF::ListId::TireParts, b.tire, ownType) });
    rows.push_back({ SF::BuildPart::Color, SF::ListId::ColorPresets, "saveeditor_build_color",
                     choices(SF::ListId::ColorPresets, b.colorPreset, unlocked) });
    rows.push_back({ SF::BuildPart::Horn, SF::ListId::Horns, "saveeditor_build_horn", choices(SF::ListId::Horns, b.horn, unlocked) });
    rows.push_back({ SF::BuildPart::Aura, SF::ListId::Auras, "saveeditor_build_aura", choices(SF::ListId::Auras, b.aura, unlocked) });
    return rows;
}

void SaveEditorBuild::DrawCard(UI::Ui& ui, const Rect& card) {
    Frame(ui, card);
    // The save was closed, read again, or the slot emptied
    const SF::MachineBuild* found = nullptr;
    if (tab.save)
        for (const auto& b : tab.Garage())
            if (b.slot == slot) found = &b;
    if (!found) {
        Close();
        return;
    }
    const SF::MachineBuild b = *found;   // (a copy: an edit reads the garage again)
    const M::Palette& p = M::P();

    // Header: name and type; Race with it / In use, Empty the slot, Close
    {
        M::Motion::Enter enter(ui, Reveal(ui, 0), 0.f, -10.f);
        const Rect head{ card.x + kPad, card.y + 22.f, card.w - kPad * 2.f, kTop - 30.f };
        float right = head.R();
        const UI::Id closeId = ui.MakeId("seb-close");
        right -= 40.f;
        if (M::IconButton(ui, closeId, { right, head.y, 40.f, 40.f }, M::Icon::Close, 20.f, M::IconButtonLook())) {
            Close();
            return;
        }
        if (!b.selected) {
            const std::wstring& clear = Loc::Get("saveeditor_build_clear");
            const float cw = TabKit::PillWidth(ui, clear);
            right -= cw + 10.f;
            if (TabKit::PillButton(ui, ui.MakeId("seb-clear"), { right, head.y + 1.f, cw, 38.f }, M::Icon::Delete, clear, kAmber, false, !tab.busy)) {
                const std::wstring s = slot;
                tab.Edit([&] { tab.save->ClearBuild(s); });
                tab.mixedSlots.erase(s);
                tab.Status(Loc::Get("saveeditor_build_cleared"));
                Close();
                return;
            }
            const std::wstring& use = Loc::Get("saveeditor_build_use");
            const float uw = TabKit::PillWidth(ui, use);
            right -= uw + 10.f;
            if (TabKit::PillButton(ui, ui.MakeId("seb-use"), { right, head.y + 1.f, uw, 38.f }, M::Icon::CarSports, use, GroupColor(1), true, !tab.busy)) {
                const std::wstring s = slot;
                tab.Edit([&] { tab.save->SetSelectedMachine(s); });
                tab.Status(Loc::Fmt("saveeditor_build_now", BuildName(b)));
            }
        } else {
            const std::wstring& inUse = Loc::Get("saveeditor_build_selected");
            right -= ChipWidth(ui, inUse, true) + 10.f;
            Chip(ui, right, head.y + 20.f, inUse, kGreen, true, true);
        }
        TextStyle title = Style(22.f, DWRITE_FONT_WEIGHT_BOLD);
        title.valign = VAlign::Center;
        title.ellipsis = true;
        const std::wstring name = BuildName(b);
        const float tw = std::min(std::ceil(ui.g.Measure(name, title).w), std::max(0.f, right - head.x - 140.f));
        ui.g.Text(name, { head.x, head.y, tw + 2.f, 40.f }, p.foreground, title);
        Chip(ui, head.x + tw + 12.f, head.y + 20.f, BuildTypeLabel(b), TabKit::Readable(MixedBuild(b) ? kAmber : GroupColor(1)));
        TextStyle hint = Style(12.f, DWRITE_FONT_WEIGHT_NORMAL, true);
        ui.g.Text(Loc::Get(tab.konami && (tab.mixedSlots.contains(slot) || MixedBuild(b)) ? "saveeditor_build_hint_mixed" : "saveeditor_build_hint"), { head.x, head.y + 44.f, head.w, 36.f }, p.foreground.Fade(0.55f), hint);
    }

    // The rows
    const Rect body{ card.x + kPad, card.y + kTop + 8.f, card.w - kPad * 2.f, card.h - kTop - 8.f - 18.f };
    UI::ScrollArea scroll(ui, ui.MakeId("seb-body"), body, false, 70.f, UI::ScrollArea::Bar::Thin);
    float y = body.y;
    const auto& all = Rows(b);
    for (size_t k = 0; k < all.size(); ++k) {
        const Row& row = all[k];
        const std::wstring& current = row.part == SF::BuildPart::Front   ? b.front
                                      : row.part == SF::BuildPart::Rear  ? b.rear
                                      : row.part == SF::BuildPart::Tire  ? b.tire
                                      : row.part == SF::BuildPart::Color ? b.colorPreset
                                      : row.part == SF::BuildPart::Horn  ? b.horn
                                                                         : b.aura;
        M::Motion::Enter enter(ui, Reveal(ui, static_cast<int>(k) + 1), 0.f, 14.f);
        y += DrawRow(ui, body.x, y, body.w - 10.f, row, current, static_cast<int>(k)) + 10.f;
    }
    scroll.SetContentSize(y - body.y);
}

// A row: its title and count, then its choices side by side (scrolling sideways), the one on the
// machine marked; a click puts another on
float SaveEditorBuild::DrawRow(UI::Ui& ui, float x, float y, float w, const Row& row, const std::wstring& current, int index) {
    const M::Palette& p = M::P();
    TextStyle title = Style(13.f, DWRITE_FONT_WEIGHT_BOLD);
    title.valign = VAlign::Center;
    ui.g.Text(Loc::Get(row.key) + L"   " + std::to_wstring(row.choices.size()), { x, y, w, kRowTitleH }, p.foreground.Fade(0.85f), title);
    const Rect strip{ x, y + kRowTitleH + 4.f, w, kChoiceH + 12.f };
    if (row.choices.empty()) {
        TextStyle note = Style(12.f);
        note.valign = VAlign::Center;
        ui.g.Text(Loc::Get("saveeditor_build_none"), strip, p.foreground.Fade(0.5f), note);
        return kRowTitleH + 4.f + strip.h;
    }
    UI::ScrollArea scroll(ui, ui.MakeId("seb-row", index), strip, true, 80.f, UI::ScrollArea::Bar::Thin);
    TextStyle name = Style(11.f, DWRITE_FONT_WEIGHT_SEMI_BOLD, false, HAlign::Center);
    name.valign = VAlign::Center;
    name.ellipsis = true;
    float cx = strip.x;
    for (const Choice& c : row.choices) {
        const Rect tile{ cx, strip.y + 2.f, kChoiceW, kChoiceH };
        cx += kChoiceW + 8.f;
        if (tile.R() < strip.x + scroll.Offset() - 10.f || tile.x > strip.R() + scroll.Offset() + 10.f) continue;
        const bool on = c.id == current;
        const UI::Id id = ui.MakeId("seb-choice", static_cast<int64_t>(index) * 100000 + static_cast<int64_t>(std::hash<std::wstring>{}(c.id) % 99991));
        const auto it = ui.Interact(id, tile, !tab.busy);
        const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, it.held);
        const Color tone = on ? kGreen : GroupColor(1);
        Card(ui, tile, 12.f, tone, on ? 0.f : fb.hover);
        if (on) ui.g.StrokeRound(tile.Inset(0.5f), 11.5f, kGreen.Fade(0.7f), 2.f);
        const Rect pic{ tile.x + 8.f, tile.y + 8.f, tile.w - 16.f, 66.f };
        ui.g.FillRound(pic, 8.f, tone.Fade(0.1f));
        const float grow = 3.f * fb.hover;
        // What the save doesn't have yet: dimmed (clearer on hover), with a lock
        const float alpha = c.owned ? 1.f : 0.55f + 0.45f * fb.hover;
        if (const ImagePtr image = Picture(row.list, c.id)) {
            if (row.list == SF::ListId::Horns) {
                ui.g.PushRoundClip(pic, 8.f);
                ui.g.DrawImage(*image, pic.Inset(-4.f - grow), alpha, Stretch::UniformToFill);
                ui.g.PopLayer();
            } else ui.g.DrawImage(*image, pic.Inset(4.f - grow), alpha, Stretch::Uniform);
        } else M::DrawIcon(ui.g, row.list == SF::ListId::Horns ? M::Icon::Bullhorn : M::Icon::CarSports, pic.CX(), pic.CY(), 28.f + grow,
                           p.foreground.Fade(0.45f));
        if (on) {
            ui.g.FillEllipse(pic.R() - 2.f, pic.y + 2.f, 10.f, 10.f, kGreen);
            M::DrawIcon(ui.g, M::Icon::Check, pic.R() - 2.f, pic.y + 2.f, 12.f, Colors::White);
        } else if (c.dlc) {
            const std::wstring& tag = Loc::Get("savefile_dlc");
            Chip(ui, pic.x + 2.f, pic.y + 10.f, tag, kAmber, true, false);
        } else if (!c.owned) {
            ui.g.FillEllipse(pic.R() - 2.f, pic.y + 2.f, 10.f, 10.f, kAmber);
            M::DrawIcon(ui.g, StateIcon(SF::ItemState::Locked), pic.R() - 2.f, pic.y + 2.f, 12.f, Colors::White);
        }
        ui.g.Text(c.name, { tile.x + 4.f, pic.B() + 4.f, tile.w - 8.f, 22.f }, on ? p.foreground : p.foreground.Fade(c.owned ? 0.75f : 0.5f), name);
        {
            std::wstring tip = c.name;
            // (parts: their type)
            if (row.list == SF::ListId::FrontParts || row.list == SF::ListId::RearParts || row.list == SF::ListId::TireParts)
                tip += L" \x2022 " + Loc::Get(MachineTypeKey(c.id));
            if (!c.owned) tip += L"\n" + Loc::Get("saveeditor_build_will_unlock");
            ui.Tooltip(id, tip);
        }
        if (it.hovered && !on) ui.SetCursor(IDC_HAND);
        if (it.clicked) {
            // A horn plays when picked (and when clicked again)
            if (row.part == SF::BuildPart::Horn)
                if (const std::wstring sound = HornSound(c.id); !sound.empty()) PlayWav(sound);
            if (!on) {
                // What the save doesn't have is unlocked first (a part, colour, horn or aura), then put on
                const std::wstring s = slot, value = c.id;
                const SF::BuildPart part = row.part;
                const SF::ListId list = row.list;
                const bool owned = c.owned;
                tab.Edit([&] {
                    if (!owned) tab.save->Unlock(list, value);
                    tab.save->SetBuild(s, part, value);
                });
                if (!owned) {
                    tab.bursts.Fire(value, ui.Time());
                    tab.Status(Loc::Fmt("saveeditor_build_unlocked", c.name));
                }
            }
        }
    }
    scroll.SetContentSize(cx - strip.x - 8.f);
    return kRowTitleH + 4.f + strip.h;
}

} // namespace SonicRacingSaveManager::Main
