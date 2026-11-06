#include "Windows/Main/Tabs/SaveEditor/SaveEditorVariants.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include "Core/Text.h"
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
using SF::ItemState;

constexpr float kTop = 96.f, kFooterH = 72.f;
constexpr int kMaxCols = 5, kMaxLines = 2;   // more versions than that scroll
constexpr float kBannerTileW = 420.f;        // honor titles: two banners a line
constexpr int kMaxBannerLines = 4;
} // namespace

void SaveEditorVariants::Show(const std::wstring& id) {
    family = id;
    titles = tab.Current().list == SF::ListId::HonorTitles;
    count = 0;
    for (const auto& f : tab.Families())
        if (f.id == id) count = f.members.size();
    Open();
}

Color SaveEditorVariants::Accent() const { return titles ? kGold : GroupColor(1); }

int SaveEditorVariants::Columns() const { return titles ? std::min(static_cast<int>(count), 2) : std::clamp(static_cast<int>(count), 2, kMaxCols); }

float SaveEditorVariants::TileW() const { return titles ? kBannerTileW : kTileW; }

// A banner tile: the 8:1 banner and the line under it
float SaveEditorVariants::TileH(float tileW) const { return titles ? 10.f + std::round((tileW - 20.f) / 8.f) + 48.f : kTileH; }

float SaveEditorVariants::CardWidth() const { return kPad * 2.f + Columns() * TileW() + (Columns() - 1) * kGap + 10.f; }

float SaveEditorVariants::CardHeight(UI::Ui&, float width) {
    const int cols = std::max(1, static_cast<int>((width - kPad * 2.f - 10.f + kGap) / (TileW() + kGap)));
    const float tileW = (width - kPad * 2.f - 10.f - (cols - 1) * kGap) / cols;
    const int lines = std::clamp(static_cast<int>((count + cols - 1) / cols), 1, titles ? kMaxBannerLines : kMaxLines);
    return kTop + lines * (TileH(tileW) + kGap) - kGap + 12.f + kFooterH;
}

// The character on top (picture, name, "unlocked / total", close), a tile per version, Unlock all
void SaveEditorVariants::DrawCard(UI::Ui& ui, const Rect& card) {
    Frame(ui, card);
    // The save was closed or read again without it
    const SaveEditorTab::Family* found = nullptr;
    for (const auto& f : tab.Families())
        if (f.id == family) found = &f;
    if (!tab.save || !found) {
        Close();
        return;
    }
    const SaveEditorTab::Family& f = *found;
    const auto& all = tab.Rows();
    const M::Palette& p = M::P();
    const Color tone = Accent();
    const SF::ItemRow& head = all[f.members.front()];
    const double now = ui.Time();
    if (now < tab.animateUntil) ui.KeepAnimating();

    // Header
    {
        M::Motion::Enter enter(ui, Reveal(ui, 0), 0.f, -10.f);
        const Rect header{ card.x, card.y, card.w, kTop - 14.f };
        ui.g.PushRoundClip(card, kRadius);
        ui.g.FillLinear(header, 0.f, { { 0.f, tone.Fade(0.22f) }, { 0.7f, Colors::Transparent } }, { header.x, header.y }, { header.R(), header.y });
        ui.g.PopLayer();
        const float cy = header.CY();
        float ax = header.x + kPad + 26.f;
        if (titles) {
            // Its first banner, small
            const Rect thumb{ header.x + kPad, cy - 14.f, 224.f, 28.f };
            if (const ImagePtr image = Picture(SF::ListId::HonorTitles, head.id)) ui.g.DrawImage(*image, thumb, 1.f, Stretch::Fill);
            ax = thumb.R() - 26.f;
        } else {
            const SF::ListId list = tab.Current().list;
            if (const ImagePtr image = Picture(list, head.id))
                ui.g.DrawImageRounded(*image, { ax - 26.f, cy - 26.f, 52.f, 52.f }, 26.f, 1.f,
                                      list == SF::ListId::Drivers ? Stretch::UniformToFill : Stretch::Uniform);
            else {
                ui.g.FillEllipse(ax, cy, 26.f, 26.f, tone.Fade(0.16f));
                M::DrawIcon(ui.g, M::Icon::Account, ax, cy, 28.f, TabKit::Readable(tone));
            }
            ui.g.StrokeEllipse(ax, cy, 28.5f, 28.5f, f.done == static_cast<int>(f.members.size()) ? kGreen : tone, 2.f);
        }
        // Close
        const float bh = 38.f;
        const Rect closeR{ header.R() - kPad - bh + 8.f, cy - bh * 0.5f, bh, bh };
        if (M::IconButton(ui, ui.MakeId("sev-close"), closeR, M::Icon::Close, 20.f, M::IconButtonLook(), true)) Close();
        TextStyle title = Style(20.f, DWRITE_FONT_WEIGHT_BOLD);
        title.valign = VAlign::Center;
        title.ellipsis = true;
        TextStyle sub = Style(12.f);
        sub.valign = VAlign::Center;
        sub.ellipsis = true;
        const float tx = ax + 42.f, tw = std::max(0.f, closeR.x - 12.f - tx);
        ui.g.Text(tab.FamilyName(f), { tx, cy - 24.f, tw, 28.f }, p.foreground, title);
        const long long n = Counter(ui, ui.MakeId("sev-count"), f.done, 0.4f);
        ui.g.Text(Loc::Fmt("saveeditor_unlocked_of", Text::FromInt(n), std::to_wstring(f.members.size())), { tx, cy + 6.f, tw, 18.f },
                  p.foreground.Fade(0.6f), sub);
    }

    // A tile per version
    const Rect list{ card.x + kPad, card.y + kTop, card.w - kPad * 2.f, card.h - kTop - kFooterH };
    {
        M::Motion::Enter enter(ui, Reveal(ui, 1), 12.f);
        UI::ScrollArea scroll(ui, ui.MakeId("sev-list"), list, false, 70.f, UI::ScrollArea::Bar::Material);
        // The bar's room is always kept (CardWidth counts it): measuring without it while the bar shows
        // gave one column less, so more lines, so the bar stayed
        const float w = list.w - 10.f;
        const int cols = std::max(1, static_cast<int>((w + kGap) / (TileW() + kGap)));
        const float tileW = (w - (cols - 1) * kGap) / cols;
        const float tileH = TileH(tileW);
        const std::wstring selected = titles ? std::to_wstring(tab.save->SelectedTitle()) : L"";
        for (size_t k = 0; k < f.members.size(); ++k) {
            const SF::ItemRow& row = all[f.members[k]];
            const Rect tile{ list.x + (k % cols) * (tileW + kGap), list.y + (k / cols) * (tileH + kGap), tileW, tileH };
            M::Motion::Enter item(ui, M::Motion::Stagger(ui, OpenedAt() + 0.15, static_cast<int>(k), 0.04f, 0.35f), 14.f);
            const UI::Id id = ui.MakeId("sev-tile", static_cast<int64_t>(std::hash<std::wstring>{}(row.id) & 0x7FFFFFFF));
            SaveEditorTab::TileLook look;
            look.state = row.state;
            look.chip = row.detail;
            look.chipTone = row.dlc || row.unreleased || !row.canUnlock ? kAmber : p.foreground.Fade(0.55f);
            look.can = CanToggle(row) && !tab.busy;
            look.burst = tab.bursts.Progress(row.id, now);
            if (titles) {
                // As the tab's banners: the line under it puts it on the player's card
                const bool on = row.id == selected;
                const auto it = tab.Banner(ui, id, tile, tileH - 58.f, row, look, on);
                tab.BannerLine(ui, id, tile, tileH - 58.f, row, on);
                ui.Tooltip(id, row.name + L"\n" + (on ? Loc::Get("saveeditor_title_on_tip") : ActionText(row)) + (row.hint.empty() ? L"" : L"\n" + row.hint));
                if (it.clicked && look.can) tab.ToggleItem(ui, row);
                continue;
            }
            const auto it = tab.Tile(ui, id, tile, row, look);
            ui.Tooltip(id, row.name + L"\n" + ActionText(row) + (row.hint.empty() ? L"" : L"\n" + row.hint));
            if (it.clicked && look.can) tab.ToggleItem(ui, row);
        }
        const size_t lines = (f.members.size() + cols - 1) / cols;
        scroll.SetContentSize(lines * (tileH + kGap) - kGap);
    }

    // Unlock all of its versions
    {
        M::Motion::Enter enter(ui, Reveal(ui, 2), 10.f);
        std::vector<std::wstring> left;
        for (size_t i : f.members)
            if (all[i].state != ItemState::Unlocked && all[i].canUnlock) left.push_back(all[i].id);
        const std::wstring& text = Loc::Get("saveeditor_unlock_all_short");
        const float bw = TabKit::PillWidth(ui, text) + 10.f;
        const Rect btn{ card.R() - kPad - bw, card.B() - kFooterH * 0.5f - 19.f, bw, 38.f };
        if (TabKit::PillButton(ui, ui.MakeId("sev-unlock"), btn, M::Icon::LockOpenVariant, text, tone, true, !tab.busy && !left.empty())) {
            const SF::ListId target = tab.Current().list;
            int changed = 0;
            tab.Edit([&] {
                for (const auto& id : left) changed += tab.save->Unlock(target, id) ? 1 : 0;
            });
            for (size_t k = 0; k < left.size(); ++k) tab.bursts.Fire(left[k], now + k * 0.05);
            tab.animateUntil = now + left.size() * 0.05 + kBurstSeconds;
            tab.Status(Loc::Fmt("saveeditor_unlocked_n", std::to_wstring(changed)));
        }
    }
}

} // namespace SonicRacingSaveManager::Main
