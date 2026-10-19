#include "Windows/Main/Tabs/Mods/FileSelectionOverlay.h"
#include <algorithm>
#include <cstdio>
#include "Localization/Localization.h"
#include "UI/Material/Motion.h"
#include "UI/Material/PackIcon.h"
#include "UI/Material/Theme.h"
#include "UI/Material/Toggles.h"
#include "UI/Widgets/ScrollArea.h"
#include "Windows/Main/Tabs/TabKit.h"

namespace SonicRacingSaveManager::Main {

namespace {

namespace M = UI::Material;
using TabKit::Style;
using TabKit::TextH;

constexpr float kHeaderH = 84.f, kRowH = 52.f, kRowGap = 6.f, kFooterH = 76.f;

// DownloadFileItemViewModel.FileSizeFormatted: "512 B", "1.50 KB", "12.34 MB", "1.02 GB"
std::wstring SizeText(long long size) {
    wchar_t text[32];
    if (size < 1024) swprintf_s(text, L"%lld B", size);
    else if (size < 1024 * 1024) swprintf_s(text, L"%.2f KB", size / 1024.0);
    else if (size < 1024LL * 1024 * 1024) swprintf_s(text, L"%.2f MB", size / 1024.0 / 1024.0);
    else swprintf_s(text, L"%.2f GB", size / 1024.0 / 1024.0 / 1024.0);
    return text;
}

} // namespace

void FileSelectionOverlay::Show(const std::wstring& name, std::vector<Item> files, std::function<void(const std::vector<Item>*)> callback) {
    modName = name;
    items = std::move(files);
    done = std::move(callback);
    // Closed by Escape / a click outside: the C#'s window close button (no files)
    onClosed = [this] { Finish(false); };
    Open();
}

void FileSelectionOverlay::Finish(bool confirmed) {
    auto callback = std::move(done);
    done = nullptr;
    if (callback) callback(confirmed ? &items : nullptr);
}

float FileSelectionOverlay::CardHeight(UI::Ui& ui, float width) {
    const float intro = TextH(ui, Loc::Get("mods_files_intro"), Style(13.f, DWRITE_FONT_WEIGHT_NORMAL, true), width - kPad * 2.f);
    const float list = std::min(5.f, std::max(1.f, static_cast<float>(items.size()))) * (kRowH + kRowGap);
    return kHeaderH + kPad + intro + 16.f + 40.f + 12.f + list + kFooterH;
}

void FileSelectionOverlay::DrawCard(UI::Ui& ui, const Rect& card) {
    Frame(ui, card);
    const M::Palette& p = M::P();
    const Color fg = p.foreground;
    // Header: the files icon and the title (Mod name under it)
    {
        M::Motion::Enter enter(ui, Reveal(ui, 0), 0.f, -10.f);
        const Rect header{ card.x, card.y, card.w, kHeaderH };
        ui.g.PushClip(header);
        ui.g.FillLinear({ header.x, header.y, header.w, header.h + kRadius }, kRadius,
                        { { 0.f, p.primaryDark.color }, { 1.f, p.primaryMid.color } }, { 0.f, 0.f }, { 1.f, 1.f });
        ui.g.PopClip();
        M::DrawIcon(ui.g, M::Icon::FileMultiple, header.x + kPad + 16.f, header.CY(), 32.f, p.primaryMid.foreground);
        TextStyle title = Style(20.f, DWRITE_FONT_WEIGHT_BOLD);
        title.valign = VAlign::Center;
        TextStyle sub = Style(12.f);
        sub.valign = VAlign::Center;
        sub.ellipsis = true;
        const float tx = header.x + kPad + 46.f, tw = header.w - kPad * 2.f - 46.f;
        ui.g.Text(Loc::Get("mods_files_title"), { tx, header.CY() - 24.f, tw, 28.f }, p.primaryMid.foreground, title);
        ui.g.Text(modName, { tx, header.CY() + 4.f, tw, 20.f }, p.primaryMid.foreground.Fade(0.8f), sub);
    }
    const Rect in{ card.x + kPad, card.y + kHeaderH + kPad, card.w - kPad * 2.f, card.h - kHeaderH - kPad - kFooterH };
    float y = in.y;
    {
        M::Motion::Enter enter(ui, Reveal(ui, 1), 10.f);
        const TextStyle intro = Style(13.f, DWRITE_FONT_WEIGHT_NORMAL, true);
        const float ih = TextH(ui, Loc::Get("mods_files_intro"), intro, in.w);
        ui.g.Text(Loc::Get("mods_files_intro"), { in.x, y, in.w, ih }, fg.Fade(0.8f), intro);
        y += ih + 16.f;
        // Select All / Deselect All
        const std::wstring& all = Loc::Get("mods_files_select_all");
        const std::wstring& none = Loc::Get("mods_files_deselect_all");
        const float aw = TabKit::PillWidth(ui, all) - 20.f, nw = TabKit::PillWidth(ui, none) - 20.f;
        if (TabKit::PillButton(ui, ui.MakeId("fs-all"), { in.x, y, aw, 36.f }, M::Icon::CheckAll, all, p.primaryMid.color, false))
            for (auto& item : items) item.isSelected = true;
        if (TabKit::PillButton(ui, ui.MakeId("fs-none"), { in.x + aw + 10.f, y, nw, 36.f }, M::Icon::CheckboxBlankOutline, none, p.primaryMid.color, false))
            for (auto& item : items) item.isSelected = false;
        y += 40.f + 12.f;
    }
    // The files
    {
        M::Motion::Enter enter(ui, Reveal(ui, 2), 10.f);
        const Rect list{ in.x, y, in.w, in.B() - y };
        UI::ScrollArea scroll(ui, ui.MakeId("fs-list"), list, false, 48.f, UI::ScrollArea::Bar::Material);
        float ry = list.y;
        for (size_t i = 0; i < items.size(); ++i, ry += kRowH + kRowGap) {
            Item& item = items[i];
            const Rect row{ list.x, ry, scroll.ContentWidth() - 4.f, kRowH };
            const UI::Id id = ui.MakeId("fs-file", static_cast<int64_t>(i));
            const UI::Interaction it = ui.Interact(id, row);
            if (it.hovered) ui.SetCursor(IDC_HAND);
            const float hover = M::Motion::Hover(ui, id, it.hovered, it.held).hover;
            const float sel = ui.Tween(UI::HashId(id, 0x5E1), item.isSelected ? 1.f : 0.f, 0.2f);
            ui.g.FillRound(row, 12.f, Lerp(Lerp(fg.Fade(0.04f), fg.Fade(0.08f), hover), p.primaryMid.color.Fade(0.18f), sel));
            M::DrawIcon(ui.g, item.isSelected ? M::Icon::CheckboxMarked : M::Icon::CheckboxBlankOutline, row.x + 22.f, row.CY(), 22.f,
                        item.isSelected ? TabKit::Readable(p.primaryMid.color) : fg.Fade(0.5f));
            M::DrawIcon(ui.g, M::Icon::File, row.x + 54.f, row.CY(), 20.f, fg.Fade(0.6f));
            TextStyle name = Style(13.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
            name.valign = VAlign::Center;
            name.ellipsis = true;
            TextStyle size = Style(12.f, DWRITE_FONT_WEIGHT_NORMAL, false, HAlign::Right);
            size.valign = VAlign::Center;
            ui.g.Text(item.fileName, { row.x + 74.f, row.y, row.w - 74.f - 110.f, row.h }, fg, name);
            ui.g.Text(SizeText(item.fileSize), { row.R() - 110.f, row.y, 96.f, row.h }, fg.Fade(0.6f), size);
            if (it.clicked) item.isSelected = !item.isSelected;
        }
        scroll.SetContentSize(items.empty() ? 0.f : items.size() * (kRowH + kRowGap) - kRowGap);
    }
    // Cancel / Continue (CanExecute: something is checked)
    {
        M::Motion::Enter enter(ui, Reveal(ui, 3), 10.f);
        const float by = card.B() - kFooterH + 14.f;
        const std::wstring& cont = Loc::Get("mods_files_continue");
        const std::wstring& cancel = Loc::Get("cancel");
        const float cw = TabKit::PillWidth(ui, cont), xw = TabKit::PillWidth(ui, cancel);
        const bool any = std::any_of(items.begin(), items.end(), [](const auto& i) { return i.isSelected; });
        if (TabKit::PillButton(ui, ui.MakeId("fs-cancel"), { card.R() - kPad - cw - 10.f - xw, by, xw, 44.f }, M::Icon::Close, cancel, fg.Fade(0.8f), false))
            Close();   // onClosed reports the cancel
        if (TabKit::PillButton(ui, ui.MakeId("fs-continue"), { card.R() - kPad - cw, by, cw, 44.f }, M::Icon::Download, cont, p.primaryMid.color, true, any)) {
            Finish(true);
            Close();
        }
    }
}

} // namespace SonicRacingSaveManager::Main
