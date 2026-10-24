#include <algorithm>
#include <cmath>
#include <cstdio>
#include "Core/Process.h"
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "UI/Material/Buttons.h"
#include "UI/Material/Elevation.h"
#include "UI/Material/Motion.h"
#include "UI/Material/PackIcon.h"
#include "UI/Material/Theme.h"
#include "UI/Widgets/ScrollArea.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Tabs/Backup/BackupKit.h"
#include "Windows/Main/Tabs/Store/ModInfoOverlay.h"
#include "Windows/Main/Tabs/Store/StoreTab.h"
#include "Windows/Main/Tabs/TabKit.h"

// The Mod Store's grid: a card per mod, the loading placeholders, the empty and error states.
namespace SonicRacingSaveManager::Main {

namespace {

namespace M = UI::Material;
namespace Store = Services::Mods::Store;
using TabKit::Style;

constexpr float kCardGap = 16.f, kMinCardW = 290.f, kInfoH = 154.f, kInset = 8.f;
const Color kGreen = Argb(0xFF4CAF50), kOrange = Argb(0xFFFF9800);

// "1234" → "1.2K", "1234567" → "1.2M"
std::wstring Short(long long n) {
    wchar_t text[32];
    if (n >= 1000000) swprintf_s(text, L"%.1fM", static_cast<double>(n) / 1000000.0);
    else if (n >= 1000) swprintf_s(text, L"%.1fK", static_cast<double>(n) / 1000.0);
    else swprintf_s(text, L"%lld", n);
    return text;
}

// Small pill over the picture: icon + text on a solid colour
float Badge(UI::Ui& ui, float x, float y, M::Icon icon, const std::wstring& text, Color bg, Color fg, bool alignRight = false) {
    TextStyle t = Style(11.f, DWRITE_FONT_WEIGHT_BOLD);
    t.valign = VAlign::Center;
    const float w = std::ceil(ui.g.Measure(text, t).w) + 34.f;
    const Rect r{ alignRight ? x - w : x, y, w, 24.f };
    ui.g.FillRound(r, 12.f, bg);
    M::DrawIcon(ui.g, icon, r.x + 14.f, r.CY(), 14.f, fg);
    ui.g.Text(text, { r.x + 26.f, r.y, w - 30.f, r.h }, fg, t);
    return w;
}

// A shimmering placeholder (loading)
void Shimmer(UI::Ui& ui, const Rect& r, float radius) {
    const M::Palette& p = M::P();
    ui.g.FillRound(r, radius, p.foreground.Fade(0.07f));
    const float k = static_cast<float>(std::fmod(ui.Time(), 1.4) / 1.4);
    const float x = r.x - r.w * 0.5f + (r.w * 2.f) * k;
    ui.g.PushClip(r);
    ui.g.FillLinear({ x - 80.f, r.y, 160.f, r.h }, 0.f,
                    { { 0.f, p.foreground.Fade(0.f) }, { 0.5f, p.foreground.Fade(0.07f) }, { 1.f, p.foreground.Fade(0.f) } }, { 0.f, 0.5f }, { 1.f, 0.5f });
    ui.g.PopClip();
    ui.KeepAnimatingThrottled();
}

} // namespace

void StoreTab::Grid(UI::Ui& ui, const Rect& r) {
    const M::Palette& p = M::P();
    if (mods.empty()) {
        if (loading || !loadedOnce) {
            // Placeholders while the first page comes
            const int cols = std::max(2, static_cast<int>((r.w + kCardGap) / (kMinCardW + kCardGap)));
            const float w = (r.w - kCardGap * static_cast<float>(cols - 1)) / static_cast<float>(cols);
            const float h = w * 9.f / 16.f + kInfoH;
            ui.g.PushClip(r);
            for (int i = 0; i < cols * 2; ++i) {
                const Rect c{ r.x + (w + kCardGap) * static_cast<float>(i % cols), r.y + (h + kCardGap) * static_cast<float>(i / cols), w, h };
                ui.g.FillRound(c, 14.f, p.foreground.Fade(0.035f));
                Shimmer(ui, { c.x + kInset, c.y + kInset, c.w - kInset * 2.f, w * 9.f / 16.f - kInset }, 10.f);
                Shimmer(ui, { c.x + 14.f, c.y + w * 9.f / 16.f + 14.f, c.w * 0.7f, 16.f }, 8.f);
                Shimmer(ui, { c.x + 14.f, c.y + w * 9.f / 16.f + 40.f, c.w * 0.4f, 12.f }, 6.f);
            }
            ui.g.PopClip();
            return;
        }
        if (!error.empty()) {
            BackupKit::EmptyState(ui, { r.x, r.y, r.w, r.h - 70.f }, M::Icon::AlertCircle, Loc::Get("store_error_title"), error);
            const std::wstring& retry = Loc::Get("store_retry");
            const float w = TabKit::PillWidth(ui, retry);
            if (TabKit::PillButton(ui, ui.MakeId("st-retry"), { r.CX() - w * 0.5f, r.B() - 110.f, w, 42.f }, M::Icon::Refresh, retry, p.primaryMid.color, true)) Load();
            return;
        }
        BackupKit::EmptyState(ui, r, M::Icon::Magnify, Loc::Get("store_empty_title"), Loc::Get("store_empty_hint"));
        return;
    }

    const UI::Id scrollId = ui.MakeId("st-scroll");
    if (pageShownAt == -1.0) {
        pageShownAt = ui.Time();
        UI::ScrollArea::ScrollTo(ui, scrollId, 0.f);
    }
    UI::ScrollArea scroll(ui, scrollId, r, false, 64.f, UI::ScrollArea::Bar::Material);
    const float width = scroll.ContentWidth() - (scroll.BarShown() ? 12.f : 0.f);
    const int cols = std::max(2, static_cast<int>((width + kCardGap) / (kMinCardW + kCardGap)));
    const float w = (width - kCardGap * static_cast<float>(cols - 1)) / static_cast<float>(cols);
    const float h = w * 9.f / 16.f + kInfoH;
    const int count = static_cast<int>(mods.size());
    for (int i = 0; i < count; ++i) {
        const Rect c{ r.x + (w + kCardGap) * static_cast<float>(i % cols), r.y + (h + kCardGap) * static_cast<float>(i / cols), w, h };
        if (c.B() < r.y + scroll.Offset() - 20.f || c.y > r.B() + scroll.Offset() + 20.f) continue;
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, pageShownAt, i, 0.035f, 0.4f), 16.f);
        Card(ui, c, i);
    }
    const int rows = (count + cols - 1) / cols;
    scroll.SetContentSize(static_cast<float>(rows) * (h + kCardGap) - kCardGap);

    // Another page or filter on its way: the cards dim under a spinner
    if (loading) {
        ui.g.FillRound(r, 12.f, p.cardBackground.Fade(0.55f));
        TabKit::Arc(ui.g, r.CX(), r.y + std::min(r.h * 0.5f, 220.f), 22.f, static_cast<float>(ui.Time()) * 360.f, 260.f, p.primaryMid.color, 4.f);
        ui.Block(r);
        ui.KeepAnimating();
    }
}

void StoreTab::Card(UI::Ui& ui, const Rect& r, int index) {
    const M::Palette& p = M::P();
    const Color fg = p.foreground;
    const StoreMod& mod = mods[static_cast<size_t>(index)];
    const UI::Id id = ui.MakeId("st-card", mod.id);
    const UI::Interaction it = ui.Interact(id, r);
    const float hover = M::Motion::Hover(ui, id, it.hovered, false).hover;
    const int installedIndex = Store::FindInstalled(mod, installed);
    const Models::ModInfo* local = installedIndex >= 0 ? &installed[static_cast<size_t>(installedIndex)] : nullptr;
    const bool outdated = local && Store::IsOutdated(mod, *local);
    const bool installingThis = installingId == mod.id;
    const Color state = outdated ? kOrange : kGreen;

    // The card lifts on hover; installed ones keep a green (orange: outdated) edge
    const Rect c{ r.x, r.y - 3.f * hover, r.w, r.h };
    if (hover > 0.f) M::Elevation(ui.g, c, 14.f, 6.f * hover);
    ui.g.FillRound(c, 14.f, Lerp(Lerp(fg.Fade(0.04f), p.cardBackground, 0.4f), Lerp(fg.Fade(0.07f), p.cardBackground, 0.2f), hover));
    if (local) ui.g.StrokeRound(c.Inset(0.75f), 13.25f, state.Fade(0.55f + 0.25f * hover), 1.5f);
    else if (hover > 0.f) ui.g.StrokeRound(c.Inset(0.75f), 13.25f, p.primaryMid.color.Fade(0.45f * hover), 1.5f);

    const float picH = r.w * 9.f / 16.f - kInset;
    const Rect pic{ c.x + kInset, c.y + kInset, c.w - kInset * 2.f, picH };
    CardPicture(ui, pic, mod, hover);
    // Badges: installed / update on the left, the category on the right
    if (local)
        Badge(ui, pic.x + 8.f, pic.y + 8.f, outdated ? M::Icon::Download : M::Icon::CheckCircle, Loc::Get(outdated ? "store_badge_update" : "store_badge_installed"),
              state, Colors::White);
    if (!mod.category.empty()) Badge(ui, pic.R() - 8.f, pic.y + 8.f, M::Icon::Puzzle, mod.category, Colors::Black.Fade(0.6f), Colors::White, true);

    // Name, author, the character (skins), likes / views / version
    float y = pic.B() + 10.f;
    const float tx = c.x + 14.f, tw = c.w - 28.f;
    TextStyle name = Style(14.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    name.valign = VAlign::Center;
    name.ellipsis = true;
    ui.g.Text(mod.name, { tx, y, tw, 22.f }, fg, name);
    y += 22.f;
    TextStyle sub = Style(11.5f);
    sub.valign = VAlign::Center;
    sub.ellipsis = true;
    std::wstring by = Loc::Fmt("store_by", mod.author.empty() ? L"?" : mod.author);
    float byW = tw;
    if (!mod.subCategory.empty()) {
        // "→ Shadow" (what a skin goes over) after the author
        TextStyle tag = Style(11.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD, false, HAlign::Right);
        tag.valign = VAlign::Center;
        tag.ellipsis = true;
        const std::wstring text = L"\x2192 " + mod.subCategory;
        const float w = std::min(std::ceil(ui.g.Measure(text, tag).w) + 2.f, tw * 0.5f);
        ui.g.Text(text, { tx + tw - w, y, w, 20.f }, TabKit::Readable(p.primaryMid.color), tag);
        byW = tw - w - 8.f;
    }
    ui.g.Text(by, { tx, y, byW, 20.f }, fg.Fade(0.6f), sub);
    y += 26.f;
    {
        TextStyle stat = Style(11.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        stat.valign = VAlign::Center;
        float x = tx;
        const auto Stat = [&](M::Icon icon, const std::wstring& text, Color color) {
            M::DrawIcon(ui.g, icon, x + 7.f, y + 10.f, 14.f, color);
            const float w = std::ceil(ui.g.Measure(text, stat).w);
            ui.g.Text(text, { x + 18.f, y, w + 2.f, 20.f }, fg.Fade(0.75f), stat);
            x += 18.f + w + 14.f;
        };
        Stat(M::Icon::Heart, Short(mod.likes), Argb(0xFFE91E63));
        Stat(M::Icon::Eye, Short(mod.views), fg.Fade(0.55f));
        if (!mod.version.empty()) {
            TextStyle ver = Style(11.f, DWRITE_FONT_WEIGHT_NORMAL, false, HAlign::Right);
            ver.valign = VAlign::Center;
            ver.ellipsis = true;
            const std::wstring text = (local && outdated ? local->version + L" \x2192 " : L"v") + mod.version;
            ui.g.Text(text, { x, y, tx + tw - x, 20.f }, fg.Fade(0.5f), ver);
        }
    }

    // Install / installed / the progress, and the GameBanana page
    const Rect row{ tx, c.B() - 12.f - 40.f, tw, 40.f };
    const Rect pageR{ row.R() - 40.f, row.y, 40.f, 40.f };
    const Rect infoR{ pageR.x - 44.f, row.y, 40.f, 40.f };
    const Rect action{ row.x, row.y, row.w - 94.f, row.h };
    if (installingThis) {
        const float shown = ui.Animate(ui.MakeId("st-progress", mod.id), static_cast<float>(installProgress / 100.0), 8.f, 0.f);
        ui.g.FillRound(action, 20.f, p.primaryMid.color.Fade(0.15f));
        ui.g.PushClip({ action.x, action.y, action.w * std::max(0.04f, shown), action.h });
        ui.g.FillRound(action, 20.f, p.primaryMid.color.Fade(0.55f));
        ui.g.PopClip();
        TabKit::Arc(ui.g, action.x + 20.f, action.CY(), 8.f, static_cast<float>(ui.Time()) * 360.f, 260.f, p.primaryMid.foreground, 2.5f);
        TextStyle t = Style(11.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        t.valign = VAlign::Center;
        t.ellipsis = true;
        wchar_t pct[16];
        swprintf_s(pct, L"%d%%  ", static_cast<int>(installProgress));
        ui.g.Text(pct + installStatus, { action.x + 36.f, action.y, action.w - 44.f, action.h }, fg, t);
        ui.KeepAnimating();
    } else if (local) {
        const UI::Id lid = ui.MakeId("st-local", mod.id);
        if (TabKit::PillButton(ui, lid, action, outdated ? M::Icon::Download : M::Icon::CheckCircle, Loc::Get(outdated ? "store_update" : "store_installed_button"),
                               state, outdated))
            main.ShowInModManager(local->name);
        ui.Tooltip(lid, Loc::Get(outdated ? "store_update_tooltip" : "store_installed_tooltip"));
    } else {
        const UI::Id iid = ui.MakeId("st-install", mod.id);
        const bool can = mod.hasFiles && installingId == 0;
        if (TabKit::PillButton(ui, iid, action, M::Icon::PackageDown, Loc::Get(mod.hasFiles ? "store_install" : "store_no_files"), p.primaryMid.color, true, can))
            Install(mod);
        if (mod.obsolete) ui.Tooltip(iid, Loc::Get("store_obsolete"));
    }
    M::ButtonLook look = M::IconButtonLook(fg.Fade(0.75f));
    look.padding = Edges(0.f);
    // More about it: the info card (description, every picture, credits, files)
    const UI::Id infoId = ui.MakeId("st-info", mod.id);
    {
        M::ButtonLook infoLook = M::IconButtonLook(TabKit::Readable(p.primaryMid.color));
        infoLook.padding = Edges(0.f);
        ui.g.FillEllipse(infoR.CX(), infoR.CY(), 19.f, 19.f, p.primaryMid.color.Fade(0.12f + 0.1f * hover));
        if (M::IconButton(ui, infoId, infoR, M::Icon::InformationOutline, 22.f, infoLook)) info->Show(mod);
        ui.Tooltip(infoId, Loc::Get("store_info"));
    }
    const UI::Id pid = ui.MakeId("st-page", mod.id);
    if (M::IconButton(ui, pid, pageR, M::Icon::OpenInNew, 20.f, look)) Process::OpenUrl(mod.profileUrl);
    ui.Tooltip(pid, Loc::Get("store_open_page"));
}

void StoreTab::CardPicture(UI::Ui& ui, const Rect& r, const StoreMod& mod, float hover) {
    const M::Palette& p = M::P();
    // Mature content (GameBanana's content ratings): hidden until it's asked for
    if (mod.rated && !revealed[mod.id]) {
        ui.g.FillRound(r, 10.f, Lerp(Colors::Black.Fade(0.75f), p.primaryDark.color, 0.25f));
        M::DrawIcon(ui.g, M::Icon::Eye, r.CX(), r.CY() - 14.f, 28.f, Colors::White.Fade(0.85f));
        TextStyle t = Style(11.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD, false, HAlign::Center);
        t.valign = VAlign::Center;
        ui.g.Text(Loc::Get("store_rated"), { r.x + 10.f, r.CY() + 6.f, r.w - 20.f, 20.f }, Colors::White.Fade(0.85f), t);
        const UI::Id id = ui.MakeId("st-reveal", mod.id);
        const UI::Interaction it = ui.Interact(id, r);
        if (it.hovered) ui.SetCursor(IDC_HAND);
        if (it.clicked) revealed[mod.id] = true;
        return;
    }
    // A click on the picture opens the info card
    const UI::Id openId = ui.MakeId("st-pic-open", mod.id);
    const UI::Interaction open = ui.Interact(openId, r);
    if (open.hovered) ui.SetCursor(IDC_HAND);
    if (open.clicked) info->Show(mod);
    if (const ImagePtr image = Picture(mod.imageUrl)) {
        const float shown = ui.Tween(ui.MakeId("st-pic", mod.id), 1.f, 0.3f, 0.f);
        if (shown < 1.f) ui.g.FillRound(r, 10.f, p.foreground.Fade(0.07f));
        ui.g.DrawImageRounded(*image, r, 10.f, shown, Stretch::UniformToFill);
        // A light sweep across the picture on hover, and a hint that it opens the info
        if (hover > 0.f) ui.g.FillRound(r, 10.f, Colors::White.Fade(0.06f * hover));
        const float h = M::Motion::Hover(ui, openId, open.hovered, open.held).hover;
        if (h > 0.f) {
            TextStyle t = Style(11.5f, DWRITE_FONT_WEIGHT_BOLD);
            t.valign = VAlign::Center;
            const std::wstring& text = Loc::Get("store_info");
            const float w = std::ceil(ui.g.Measure(text, t).w) + 40.f;
            const Rect pill{ r.CX() - w * 0.5f, r.B() - 38.f + 6.f * (1.f - h), w, 28.f };
            ui.g.FillRound(pill, 14.f, Colors::Black.Fade(0.6f * h));
            M::DrawIcon(ui.g, M::Icon::InformationOutline, pill.x + 16.f, pill.CY(), 16.f, Colors::White.Fade(h));
            ui.g.Text(text, { pill.x + 30.f, pill.y, w - 34.f, pill.h }, Colors::White.Fade(h), t);
        }
        return;
    }
    const auto slot = pictures.find(mod.imageUrl);
    const bool failed = mod.imageUrl.empty() || (slot != pictures.end() && slot->second.failed);
    if (!failed) {
        Shimmer(ui, r, 10.f);
        return;
    }
    ui.g.FillRound(r, 10.f, p.foreground.Fade(0.06f));
    M::DrawIcon(ui.g, M::Icon::ImageOff, r.CX(), r.CY(), 34.f, p.foreground.Fade(0.3f));
}

} // namespace SonicRacingSaveManager::Main
