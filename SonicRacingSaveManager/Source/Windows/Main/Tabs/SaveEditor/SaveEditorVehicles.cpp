#include <algorithm>
#include "Core/Text.h"
#include "Data/SaveNames.h"
#include "Localization/Localization.h"
#include "UI/Material/Motion.h"
#include "UI/Material/Theme.h"
#include "UI/Widgets/ScrollArea.h"
#include "UI/Material/Buttons.h"
#include "Windows/Main/Tabs/SaveEditor/SaveEditorBuild.h"
#include "Windows/Main/Tabs/SaveEditor/SaveEditorTab.h"
#include "Windows/Main/Tabs/TabKit.h"

// The vehicles section: the garage (the machines built in the game, the one raced with first, each
// with its three parts, colour, horn, aura and stickers; another can be picked to race with) over
// tags for the lists: the complete machines, the front / rear parts and tyres, colours and auras.
namespace SonicRacingSaveManager::Main {

namespace M = UI::Material;
namespace SF = Services::SaveFile;
using namespace SaveEditorKit;
using TabKit::Style;

namespace {

constexpr float kGarageH = 206.f;
constexpr float kBuildW = 330.f, kSelectedW = 470.f, kNewW = 250.f;

// The lists the section's tags open, in order
constexpr SF::ListId kVehicleLists[] = { SF::ListId::Machines, SF::ListId::FrontParts, SF::ListId::RearParts,
                                         SF::ListId::TireParts, SF::ListId::ColorPresets, SF::ListId::Auras };

std::wstring Named(const std::string& key, const std::wstring& fallback) {
    const wchar_t* name = Data::SaveNames::Find(key, Loc::CurrentLang());
    return name ? name : fallback;
}

} // namespace

SaveEditorTab::Stat SaveEditorTab::VehicleStat(const std::vector<Stat>& all) {
    Stat sum;
    for (const SF::ListId list : kVehicleLists) {
        const Stat& s = all[static_cast<size_t>(CategoryOf(list))];
        sum.done += s.done;
        sum.total += s.total;
    }
    return sum;
}

const std::vector<SF::MachineBuild>& SaveEditorTab::Garage() {
    if (garageFor == version) return garage;
    garageFor = version;
    garage.clear();
    if (save) {
        for (auto& b : save->Garage())
            if (b.used) garage.push_back(std::move(b));
    }
    return garage;
}

// One machine of the garage: its parts' pictures side by side, its name and type, then its colour,
// horn (with its sound), aura and stickers; the selected one says so, another can be raced with
void SaveEditorTab::BuildCard(UI::Ui& ui, UI::Id id, const Rect& r, const SF::MachineBuild& b) {
    const M::Palette& p = M::P();
    const Color tone = b.selected ? kGreen : GroupColor(1);
    const auto it = ui.Interact(id, r);
    const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, false);
    const Rect card = r.Offset(0.f, -3.f * fb.hover);
    Card(ui, card, kRadius, tone, fb.hover * 0.7f);
    if (b.selected) ui.g.StrokeRound(card.Inset(0.5f), kRadius - 0.5f, kGreen.Fade(0.5f), 1.5f);

    // The three parts (two for the Boost machines: they float, their tyres slot is unused)
    const int partCount = b.slot.starts_with(L"DSH") ? 2 : 3;
    const float ph = 84.f, gap = 8.f;
    const float pw = (card.w - 28.f - (partCount - 1) * gap) / partCount;
    const struct {
        SF::ListId list;
        const std::wstring& part;
        const char* key;
    } parts[3] = { { SF::ListId::FrontParts, b.front, "saveeditor_part_front" },
                   { SF::ListId::RearParts, b.rear, "saveeditor_part_rear" },
                   { SF::ListId::TireParts, b.tire, "saveeditor_part_tire" } };
    TextStyle partName = Style(11.f, DWRITE_FONT_WEIGHT_SEMI_BOLD, false, HAlign::Center);
    partName.valign = VAlign::Center;
    partName.ellipsis = true;
    for (int k = 0; k < partCount; ++k) {
        const Rect pr{ card.x + 14.f + k * (pw + gap), card.y + 14.f, pw, ph };
        ui.g.FillRound(pr, 10.f, tone.Fade(0.1f));
        if (const ImagePtr image = Picture(parts[k].list, parts[k].part)) ui.g.DrawImage(*image, pr.Inset(6.f), 1.f, Stretch::Uniform);
        const std::wstring name = Named((k == 0 ? "front:" : k == 1 ? "rear:" : "tire:") + Text::Narrow(parts[k].part), parts[k].part);
        ui.g.Text(name, { pr.x, pr.B() + 2.f, pr.w, 18.f }, p.foreground.Fade(0.75f), partName);
        const UI::Id pid = UI::HashId(id, 0x70 + k);
        ui.Interact(pid, pr);
        ui.Tooltip(pid, Loc::Get(parts[k].key) + L": " + name);
    }

    // Edit: its card with every part, colour, horn and aura
    {
        const UI::Id eid = UI::HashId(id, 0xED);
        const Rect er{ card.R() - 14.f - 34.f, card.y + 14.f + 4.f, 30.f, 30.f };
        ui.g.FillEllipse(er.CX(), er.CY(), 17.f, 17.f, p.background.Fade(0.85f));
        if (M::IconButton(ui, eid, er, M::Icon::Pencil, 17.f, M::IconButtonLook(GroupColor(1)), !busy)) build->Show(b.slot);
        ui.Tooltip(eid, Loc::Get("saveeditor_build_edit"));
    }

    // Name, type and what it is raced with
    float y = card.y + 14.f + ph + 24.f;
    TextStyle title = Style(16.f, DWRITE_FONT_WEIGHT_BOLD);
    title.valign = VAlign::Center;
    title.ellipsis = true;
    float right = card.R() - 14.f;
    if (b.selected) {
        const std::wstring& inUse = Loc::Get("saveeditor_build_selected");
        right -= ChipWidth(ui, inUse, true);
        Chip(ui, right, y + 13.f, inUse, kGreen, true, true);
    } else {
        const std::wstring& use = Loc::Get("saveeditor_build_use");
        const float bw = TabKit::PillWidth(ui, use);
        right -= bw;
        if (TabKit::PillButton(ui, UI::HashId(id, 0x05E), { right, y - 4.f, bw, 34.f }, M::Icon::CarSports, use, GroupColor(1), false, !busy)) {
            const std::wstring slot = b.slot;
            Edit([&] { save->SetSelectedMachine(slot); });
            Status(Loc::Fmt("saveeditor_build_now", BuildName(b)));
        }
    }
    const std::wstring& typeLabel = BuildTypeLabel(b);
    const float typeW = ChipWidth(ui, typeLabel);
    ui.g.Text(BuildName(b), { card.x + 16.f, y, std::max(0.f, right - card.x - 32.f - typeW), 26.f }, p.foreground, title);
    const float nameW = std::min(std::ceil(ui.g.Measure(BuildName(b), title).w), std::max(0.f, right - card.x - 32.f - typeW));
    Chip(ui, card.x + 16.f + nameW + 8.f, y + 13.f, typeLabel, TabKit::Readable(MixedBuild(b) ? kAmber : GroupColor(1)));
    y += 34.f;

    // Colour, horn (its sound on a click), aura, stickers
    float x = card.x + 16.f;
    const float maxX = card.R() - 14.f;
    const auto chip = [&](const std::wstring& text, Color c) {
        const float w = ChipWidth(ui, text, true);
        if (x + w > maxX) return false;
        Chip(ui, x, y + 11.f, text, c, false, true);
        x += w + 6.f;
        return true;
    };
    // The colour: its swatch, then its name
    if (!b.colorPreset.empty())
        if (const ImagePtr swatch = Picture(SF::ListId::ColorPresets, b.colorPreset)) {
            ui.g.DrawImage(*swatch, { x, y, 22.f, 22.f }, 1.f, Stretch::Uniform);
            x += 26.f;
        }
    chip(b.colorPreset.empty() ? Loc::Get("saveeditor_build_color_custom") : b.colorName, GroupColor(2));
    const std::wstring horn = Named("horn:" + Text::Narrow(b.horn), b.horn);
    const float hx = x;
    if (chip(L"\x266A " + horn, Rgba(249, 115, 22))) {
        const std::wstring sound = HornSound(b.horn);
        const Rect hr{ hx, y, x - hx - 6.f, 22.f };
        const UI::Id hid = UI::HashId(id, 0x40);
        const auto hit = ui.Interact(hid, hr);
        if (!sound.empty()) {
            if (hit.hovered) ui.SetCursor(IDC_HAND);
            ui.Tooltip(hid, Loc::Get("saveeditor_horn_play"));
            if (hit.clicked) PlayWav(sound);
        }
    }
    if (b.aura != L"MACHINE_AURA_NONE" && !b.aura.empty()) chip(Named("aura:" + Text::Narrow(b.aura), b.aura), Rgba(168, 85, 247));
    if (b.stickers) chip(Loc::Fmt("saveeditor_build_stickers", std::to_wstring(b.stickers)), Rgba(236, 72, 153));
}

// A new machine: one of each type's buttons builds it in that type's first free slot and opens it. With
// custom cars on (the Konami code), "Custom" builds one in the first free slot of any type and lets its
// editor mix every type's parts
void SaveEditorTab::NewBuildCard(UI::Ui& ui, const Rect& r) {
    const M::Palette& p = M::P();
    Card(ui, r, kRadius, GroupColor(1), 0.f);
    ui.g.StrokeRound(r.Inset(1.f), kRadius - 1.f, GroupColor(1).Fade(0.35f), 1.5f);
    M::DrawIcon(ui.g, M::Icon::Plus, r.x + 30.f, r.y + 30.f, 24.f, TabKit::Readable(GroupColor(1)));
    TextStyle title = Style(15.f, DWRITE_FONT_WEIGHT_BOLD);
    title.valign = VAlign::Center;
    title.ellipsis = true;
    ui.g.Text(Loc::Get("saveeditor_build_new"), { r.x + 50.f, r.y + 16.f, r.w - 64.f, 28.f }, p.foreground, title);
    static const wchar_t* kTypes[] = { L"SPD", L"ACL", L"HND", L"POW", L"DSH" };
    const float bw = (r.w - 28.f - 8.f) / 2.f, bh = 32.f;
    const int buttons = konami ? 6 : 5;
    for (int k = 0; k < buttons; ++k) {
        // (two per row; a fifth alone on the last row takes it whole)
        const bool alone = k == 4 && buttons == 5;
        const Rect br{ r.x + 14.f + (k % 2) * (bw + 8.f), r.y + 56.f + (k / 2) * (bh + 8.f), alone ? bw * 2.f + 8.f : bw, bh };
        const bool custom = k == 5;
        const std::wstring& label = Loc::Get(custom ? "saveeditor_build_custom" : MachineTypeKey(kTypes[k]));
        const UI::Id bid = ui.MakeId("se-new-build", k);
        if (TabKit::PillButton(ui, bid, br, custom ? M::Icon::Wrench : M::Icon::CarSports, label, custom ? kAmber : GroupColor(1), false, !busy)) {
            std::wstring made;
            Edit([&] {
                // Custom: the first type with a free slot (Boost last: its machines have no tyres)
                const int from = custom ? 0 : k, to = custom ? 4 : k;
                for (int t = from; t <= to && made.empty(); ++t) made = save->NewBuild(kTypes[t]);
            });
            if (made.empty()) Status(custom ? Loc::Get("saveeditor_build_full_any") : Loc::Fmt("saveeditor_build_full", label), true);
            else {
                if (custom) mixedSlots.insert(made);
                else mixedSlots.erase(made);
                Status(custom ? Loc::Get("saveeditor_build_created_custom") : Loc::Fmt("saveeditor_build_created", label));
                build->Show(made);
            }
        }
        if (custom) ui.Tooltip(bid, Loc::Get("saveeditor_build_custom_tip"));
    }
}

void SaveEditorTab::VehiclesView(UI::Ui& ui, const Rect& r) {
    // The garage, side by side (the selected one wider), scrolling sideways when they don't fit
    const Rect strip{ r.x, r.y, r.w, kGarageH };
    const auto& builds = Garage();
    {
        UI::ScrollArea scroll(ui, ui.MakeId("se-garage"), strip, true, 80.f, UI::ScrollArea::Bar::Thin);
        float x = strip.x;
        const double reveal = std::max(loadedAt, screenAt) + 0.08;
        // A new machine first, then the garage
        {
            M::Motion::Enter enter(ui, M::Motion::Stagger(ui, reveal, 0, 0.05f, 0.35f), 12.f);
            NewBuildCard(ui, { x, strip.y + 4.f, kNewW, kGarageH - 16.f });
            x += kNewW + kGap;
        }
        for (size_t k = 0; k < builds.size(); ++k) {
            const float w = builds[k].selected ? kSelectedW : kBuildW;
            M::Motion::Enter enter(ui, M::Motion::Stagger(ui, reveal, static_cast<int>(k) + 1, 0.05f, 0.35f), 12.f);
            BuildCard(ui, ui.MakeId("se-build", static_cast<int64_t>(std::hash<std::wstring>{}(builds[k].slot) & 0x7FFFFFFF)),
                      { x, strip.y + 4.f, w, kGarageH - 16.f }, builds[k]);
            x += w + kGap;
        }
        scroll.SetContentSize(x - strip.x - kGap);
    }

    // The lists: a tag each (unlocked / total), opening it under them
    const Rect tags{ r.x, strip.B() + 6.f, r.w, 36.f };
    float tx = tags.x;
    const auto& counts = Stats();
    for (int k = 0; k < static_cast<int>(std::size(kVehicleLists)); ++k) {
        const int cat = CategoryOf(kVehicleLists[k]);
        const Category& c = Categories()[static_cast<size_t>(cat)];
        const Stat& st = counts[static_cast<size_t>(cat)];
        const std::wstring text = Loc::Get(k == 0 ? "saveeditor_cat_complete_machines" : c.key) + L"   " + std::to_wstring(st.done) + L" / " +
                                  std::to_wstring(st.total);
        const float tw = TabKit::PillWidth(ui, text);
        const bool on = category == cat;
        if (TabKit::PillButton(ui, ui.MakeId("se-vehicle-tag", k), { tx, tags.y, tw, 36.f }, c.icon, text, GroupColor(c.group), on, !on)) {
            category = cat;
            search.clear();
            filter = 0;
            shopTag = -1;
            UI::ScrollArea::ScrollTo(ui, ui.MakeId("se-items"), 0.f);
        }
        tx += tw + 10.f;
    }
    CategoryView(ui, { r.x, tags.B() + kGap, r.w, r.B() - tags.B() - kGap });
}

} // namespace SonicRacingSaveManager::Main
