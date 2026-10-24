#include "Windows/Main/Tabs/Store/ModInfoOverlay.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include "Core/Async.h"
#include "Core/Process.h"
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "Services/Mods/ModManager.h"
#include "UI/Material/Buttons.h"
#include "UI/Material/Motion.h"
#include "UI/Material/PackIcon.h"
#include "UI/Material/Theme.h"
#include "UI/Widgets/ScrollArea.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Tabs/Mods/FileSelectionOverlay.h"
#include "Windows/Main/Tabs/Store/StoreTab.h"
#include "Windows/Main/Tabs/TabKit.h"

namespace SonicRacingSaveManager::Main {

namespace {

namespace M = UI::Material;
namespace Mods = Services::Mods;
namespace Store = Services::Mods::Store;
using TabKit::Style;
using TabKit::TextH;

constexpr float kCardH = 730.f, kLeftW = 560.f, kColumnGap = 26.f, kThumbW = 96.f, kThumbH = 54.f, kThumbGap = 8.f;
const Color kGreen = Argb(0xFF4CAF50), kOrange = Argb(0xFFFF9800), kPink = Argb(0xFFE91E63);

// Unix seconds → "2026-10-23"
std::wstring Date(long long seconds) {
    if (seconds <= 0) return L"-";
    const std::time_t t = static_cast<std::time_t>(seconds);
    std::tm local{};
    if (localtime_s(&local, &t) != 0) return L"-";
    wchar_t text[16];
    swprintf_s(text, L"%04d-%02d-%02d", local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
    return text;
}

// "1234" → "1,234"
std::wstring Thousands(long long n) {
    std::wstring digits = std::to_wstring(n), out;
    for (size_t i = 0; i < digits.size(); ++i) {
        if (i > 0 && (digits.size() - i) % 3 == 0) out += L',';
        out += digits[i];
    }
    return out;
}

// "8.4 MB"
std::wstring Size(long long bytes) {
    wchar_t text[32];
    if (bytes >= 1024ll * 1024) swprintf_s(text, L"%.1f MB", static_cast<double>(bytes) / 1024.0 / 1024.0);
    else swprintf_s(text, L"%.0f KB", static_cast<double>(bytes) / 1024.0);
    return text;
}

// Caps heading of a section of the details, returns its height
float Heading(UI::Ui& ui, const Rect& r, const std::wstring& text) {
    TextStyle caps = Style(11.f, DWRITE_FONT_WEIGHT_BOLD);
    caps.valign = VAlign::Center;
    ui.g.Text(text, { r.x, r.y, r.w, 20.f }, M::P().primaryMid.color, caps);
    ui.g.FillRect({ r.x, r.y + 24.f, r.w, 1.f }, M::P().divider);
    return 34.f;
}

} // namespace

ModInfoOverlay::ModInfoOverlay(MainWindow& window, StoreTab& tab) : CardOverlay(window), store(tab) { SetCover(tab.files.get()); }

void ModInfoOverlay::Show(const Store::StoreMod& info) {
    mod = info;
    partial = false;
    details.reset();
    error.clear();
    picture = 0;
    pictureChangedAt = -10.0;
    loading = true;
    const unsigned gen = ++generation;
    struct Outcome {
        std::optional<Store::ModDetails> details;
        std::wstring error;
    };
    Async::Run(
        [id = mod.id] {
            Outcome o;
            try {
                o.details = Store::FetchDetails(id);
            } catch (const Mods::Error& e) {
                o.error = e.Message();
            }
            return o;
        },
        [this, gen](Outcome o) {
            if (gen != generation) return;   // another mod was opened meanwhile
            loading = false;
            details = std::move(o.details);
            error = o.error;
            if (partial && details) {
                mod = details->mod;
                partial = false;
            }
        });
    Open();
}

void ModInfoOverlay::ShowById(long long id, const std::wstring& name) {
    Store::StoreMod m;
    m.id = id;
    m.name = name;
    m.profileUrl = L"https://gamebanana.com/mods/" + std::to_wstring(id);
    Show(m);
    partial = true;   // the details arrive on a later frame
}

float ModInfoOverlay::CardHeight(UI::Ui&, float) { return kCardH; }

void ModInfoOverlay::DrawCard(UI::Ui& ui, const Rect& card) {
    Frame(ui, card);
    const M::Palette& p = M::P();
    // Close (top right)
    {
        M::ButtonLook look = M::IconButtonLook(p.foreground.Fade(0.7f));
        look.padding = Edges(0.f);
        const UI::Id id = ui.MakeId("mi-close");
        if (M::IconButton(ui, id, { card.R() - 50.f, card.y + 14.f, 36.f, 36.f }, M::Icon::Close, 20.f, look)) Close();
        ui.Tooltip(id, Loc::Get("store_info_close"));
    }
    const Rect in = card.Inset(kPad);
    const Rect left{ in.x, in.y, kLeftW, in.h };
    const Rect right{ left.R() + kColumnGap, in.y, in.R() - left.R() - kColumnGap - 30.f, in.h };
    {
        M::Motion::Enter enter(ui, Reveal(ui, 0), 10.f);
        Gallery(ui, left);
    }
    {
        M::Motion::Enter enter(ui, Reveal(ui, 2), 10.f);
        Actions(ui, { left.x, left.B() - 196.f, left.w, 196.f });
    }
    {
        M::Motion::Enter enter(ui, Reveal(ui, 1), 0.f, 18.f);
        Details(ui, right);
    }
}

// ── Left: the pictures ──────────────────────────────────────────────────────

void ModInfoOverlay::Gallery(UI::Ui& ui, const Rect& r) {
    const M::Palette& p = M::P();
    // The profile's pictures, the card's one until they're here
    std::vector<Store::ModDetails::Picture> pictures;
    if (details) pictures = details->pictures;
    if (pictures.empty() && !mod.imageUrl.empty()) pictures.push_back({ mod.imageUrl, mod.imageUrl, L"" });
    const int count = static_cast<int>(pictures.size());
    picture = std::clamp(picture, 0, std::max(0, count - 1));
    const auto go = [&](int to) {
        if (count < 2) return;
        to = (to + count) % count;
        if (to == picture) return;
        pictureDirection = to > picture ? 1.f : -1.f;
        picture = to;
        pictureChangedAt = ui.Time();
    };
    if (count > 1 && ui.KeyPressed(VK_RIGHT)) go(picture + 1);
    if (count > 1 && ui.KeyPressed(VK_LEFT)) go(picture - 1);

    const Rect big{ r.x, r.y, r.w, std::floor(r.w * 9.f / 16.f) };
    ui.g.PushRoundClip(big, 14.f);
    ui.g.FillRect(big, Lerp(Colors::Black, p.primaryDark.color, 0.25f).Fade(0.85f));
    const bool hidden = mod.rated && !store.revealed[mod.id];
    if (hidden) {
        M::DrawIcon(ui.g, M::Icon::Eye, big.CX(), big.CY() - 16.f, 34.f, Colors::White.Fade(0.85f));
        TextStyle t = Style(13.f, DWRITE_FONT_WEIGHT_SEMI_BOLD, false, HAlign::Center);
        t.valign = VAlign::Center;
        ui.g.Text(Loc::Get("store_rated"), { big.x, big.CY() + 8.f, big.w, 24.f }, Colors::White.Fade(0.85f), t);
        const UI::Id id = ui.MakeId("mi-reveal");
        const UI::Interaction it = ui.Interact(id, big);
        if (it.hovered) ui.SetCursor(IDC_HAND);
        if (it.clicked) store.revealed[mod.id] = true;
    } else if (count == 0) {
        M::DrawIcon(ui.g, loading ? M::Icon::CloudSearch : M::Icon::ImageOff, big.CX(), big.CY(), 44.f, Colors::White.Fade(0.4f));
    } else if (const ImagePtr image = store.Picture(pictures[static_cast<size_t>(picture)].url, 1024)) {
        // The new picture slides in a little and fades in
        const float k = Anim::EaseOutCubic(Anim::Clamp01(static_cast<float>((ui.Time() - pictureChangedAt) / 0.3)));
        if (k < 1.f) ui.KeepAnimating();
        ui.g.PushTransform(D2D1::Matrix3x2F::Translation(24.f * (1.f - k) * pictureDirection, 0.f));
        ui.g.DrawImage(*image, big, k, Stretch::Uniform);
        ui.g.PopTransform();
    } else {
        TabKit::Arc(ui.g, big.CX(), big.CY(), 18.f, static_cast<float>(ui.Time()) * 360.f, 260.f, Colors::White.Fade(0.7f), 3.f);
        ui.KeepAnimating();
    }
    // Arrows and "3 / 11"
    if (count > 1 && !hidden) {
        const auto arrow = [&](const char* key, float cx, M::Icon icon, int step) {
            const UI::Id id = ui.MakeId(key);
            const Rect a{ cx - 20.f, big.CY() - 20.f, 40.f, 40.f };
            const UI::Interaction it = ui.Interact(id, a);
            if (it.hovered) ui.SetCursor(IDC_HAND);
            const float h = M::Motion::Hover(ui, id, it.hovered, it.held).hover;
            ui.g.FillEllipse(a.CX(), a.CY(), 20.f, 20.f, Colors::Black.Fade(0.45f + 0.25f * h));
            M::DrawIcon(ui.g, icon, a.CX(), a.CY(), 26.f, Colors::White);
            if (it.clicked) go(picture + step);
        };
        arrow("mi-prev", big.x + 32.f, M::Icon::ChevronLeft, -1);
        arrow("mi-next", big.R() - 32.f, M::Icon::ChevronRight, 1);
        TextStyle t = Style(11.f, DWRITE_FONT_WEIGHT_BOLD, false, HAlign::Center);
        t.valign = VAlign::Center;
        const std::wstring text = std::to_wstring(picture + 1) + L" / " + std::to_wstring(count);
        const float w = std::ceil(ui.g.Measure(text, t).w) + 22.f;
        const Rect pill{ big.R() - 12.f - w, big.B() - 34.f, w, 24.f };
        ui.g.FillRound(pill, 12.f, Colors::Black.Fade(0.55f));
        ui.g.Text(text, pill, Colors::White, t);
    }
    ui.g.PopLayer();

    float y = big.B() + 8.f;
    // The caption
    if (count > 0 && !pictures[static_cast<size_t>(picture)].caption.empty()) {
        TextStyle t = Style(12.f, DWRITE_FONT_WEIGHT_NORMAL, false, HAlign::Center);
        t.valign = VAlign::Center;
        t.ellipsis = true;
        ui.g.Text(pictures[static_cast<size_t>(picture)].caption, { r.x, y, r.w, 20.f }, p.foreground.Fade(0.7f), t);
    }
    y += 26.f;
    // The thumbnails, the shown one in the middle when there are more than fit
    if (count > 1 && !hidden) {
        const int fit = std::max(1, static_cast<int>((r.w + kThumbGap) / (kThumbW + kThumbGap)));
        const int first = std::clamp(picture - fit / 2, 0, std::max(0, count - fit));
        for (int i = first; i < std::min(count, first + fit); ++i) {
            const Rect t{ r.x + static_cast<float>(i - first) * (kThumbW + kThumbGap), y, kThumbW, kThumbH };
            const UI::Id id = ui.MakeId("mi-thumb", i);
            const UI::Interaction it = ui.Interact(id, t);
            if (it.hovered) ui.SetCursor(IDC_HAND);
            const float h = M::Motion::Hover(ui, id, it.hovered, it.held).hover;
            const bool on = i == picture;
            if (const ImagePtr image = store.Picture(pictures[static_cast<size_t>(i)].thumb)) ui.g.DrawImageRounded(*image, t, 8.f, on ? 1.f : 0.55f + 0.35f * h);
            else ui.g.FillRound(t, 8.f, p.foreground.Fade(0.07f));
            if (on) ui.g.StrokeRound(t.Inset(1.f), 7.f, p.primaryMid.color, 2.5f);
            if (it.clicked) go(i);
        }
    }
}

// ── Left, bottom: the stats and the buttons ─────────────────────────────────

void ModInfoOverlay::Actions(UI::Ui& ui, const Rect& r) {
    const M::Palette& p = M::P();
    const Color fg = p.foreground;
    const long long likes = details ? details->likes : mod.likes, views = details ? details->views : mod.views;
    struct Stat {
        M::Icon icon;
        Color color;
        std::wstring value, label;
    };
    const Stat stats[] = {
        { M::Icon::Heart, kPink, Thousands(likes), Loc::Get("store_info_likes") },
        { M::Icon::Eye, fg.Fade(0.6f), Thousands(views), Loc::Get("store_info_views") },
        { M::Icon::Download, kGreen, details ? Thousands(details->downloads) : L"-", Loc::Get("store_info_downloads") },
        { M::Icon::Star, p.primaryMid.color, mod.version.empty() ? L"-" : mod.version, Loc::Get("store_info_version") },
        { M::Icon::PlusCircle, fg.Fade(0.6f), Date(mod.dateAdded), Loc::Get("store_info_added") },
        { M::Icon::Refresh, fg.Fade(0.6f), Date(mod.dateUpdated), Loc::Get("store_info_updated") },
    };
    const float w = (r.w - 16.f) / 3.f, h = 50.f;
    for (int i = 0; i < 6; ++i) {
        const Rect c{ r.x + static_cast<float>(i % 3) * (w + 8.f), r.y + static_cast<float>(i / 3) * (h + 8.f), w, h };
        ui.g.FillRound(c, 12.f, fg.Fade(0.05f));
        M::DrawIcon(ui.g, stats[i].icon, c.x + 22.f, c.CY(), 18.f, stats[i].color);
        TextStyle value = Style(13.5f, DWRITE_FONT_WEIGHT_BOLD);
        value.valign = VAlign::Center;
        value.ellipsis = true;
        TextStyle label = Style(10.5f);
        label.valign = VAlign::Center;
        label.ellipsis = true;
        ui.g.Text(stats[i].value, { c.x + 40.f, c.y + 6.f, c.w - 46.f, 22.f }, fg, value);
        ui.g.Text(stats[i].label, { c.x + 40.f, c.y + 26.f, c.w - 46.f, 18.f }, fg.Fade(0.55f), label);
    }

    // Install / installed / the progress, and the GameBanana page
    const Rect row{ r.x, r.B() - 48.f, r.w, 48.f };
    const std::wstring& page = Loc::Get("store_info_open_page");
    const float pw = TabKit::PillWidth(ui, page);
    const Rect action{ row.x, row.y, row.w - pw - 10.f, row.h };
    if (TabKit::PillButton(ui, ui.MakeId("mi-page"), { action.R() + 10.f, row.y, pw, row.h }, M::Icon::OpenInNew, page, p.primaryMid.color, false))
        Process::OpenUrl(mod.profileUrl);
    const int installedIndex = Store::FindInstalled(mod, store.installed);
    const Models::ModInfo* local = installedIndex >= 0 ? &store.installed[static_cast<size_t>(installedIndex)] : nullptr;
    const bool outdated = local && Store::IsOutdated(mod, *local);
    if (store.installingId == mod.id) {
        const float shown = ui.Animate(ui.MakeId("mi-progress"), static_cast<float>(store.installProgress / 100.0), 8.f, 0.f);
        ui.g.FillRound(action, 24.f, p.primaryMid.color.Fade(0.15f));
        ui.g.PushClip({ action.x, action.y, action.w * std::max(0.04f, shown), action.h });
        ui.g.FillRound(action, 24.f, p.primaryMid.color.Fade(0.55f));
        ui.g.PopClip();
        TabKit::Arc(ui.g, action.x + 24.f, action.CY(), 9.f, static_cast<float>(ui.Time()) * 360.f, 260.f, p.primaryMid.foreground, 2.5f);
        TextStyle t = Style(12.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        t.valign = VAlign::Center;
        t.ellipsis = true;
        wchar_t pct[16];
        swprintf_s(pct, L"%d%%  ", static_cast<int>(store.installProgress));
        ui.g.Text(pct + store.installStatus, { action.x + 42.f, action.y, action.w - 52.f, action.h }, fg, t);
        ui.KeepAnimating();
    } else if (local) {
        // Installed / Update, and Reinstall: the files on GameBanana replace the installed ones
        const std::wstring& again = Loc::Get("store_reinstall");
        const float rw = TabKit::PillWidth(ui, again);
        const Rect status{ action.x, action.y, action.w - rw - 10.f, action.h };
        if (TabKit::PillButton(ui, ui.MakeId("mi-local"), status, outdated ? M::Icon::Download : M::Icon::CheckCircle,
                               Loc::Get(outdated ? "store_update" : "store_installed_button"), outdated ? kOrange : kGreen, outdated)) {
            Close();
            store.main.ShowInModManager(local->name);
        }
        const UI::Id rid = ui.MakeId("mi-reinstall");
        if (TabKit::PillButton(ui, rid, { status.R() + 10.f, action.y, rw, action.h }, M::Icon::Restore, again, p.primaryMid.color, true,
                               details && !details->files.empty() && store.installingId == 0 && !partial))
            store.Reinstall(mod);
        ui.Tooltip(rid, Loc::Get("store_reinstall_tooltip"));
    } else if (TabKit::PillButton(ui, ui.MakeId("mi-install"), action, M::Icon::PackageDown, Loc::Get(mod.hasFiles ? "store_install" : "store_no_files"),
                                  p.primaryMid.color, true, mod.hasFiles && store.installingId == 0)) {
        store.Install(mod);
    }
}

// ── Right: name, description, credits, files ───────────────────────────────

void ModInfoOverlay::Details(UI::Ui& ui, const Rect& r) {
    const M::Palette& p = M::P();
    const Color fg = p.foreground;
    // Name, author and what it goes over
    TextStyle name = Style(22.f, DWRITE_FONT_WEIGHT_BOLD, true);
    const float nameH = std::min(TextH(ui, mod.name, name, r.w), 60.f);
    ui.g.Text(mod.name, { r.x, r.y, r.w, nameH }, fg, name);
    float y = r.y + nameH + 6.f;
    {
        TextStyle by = Style(13.f);
        by.valign = VAlign::Center;
        by.ellipsis = true;
        const std::wstring author = Loc::Fmt("store_by", mod.author.empty() ? L"?" : mod.author);
        const float aw = std::min(std::ceil(ui.g.Measure(author, by).w) + 2.f, r.w * 0.5f);
        ui.g.Text(author, { r.x, y, aw, 22.f }, fg.Fade(0.65f), by);
        float x = r.x + aw + 12.f;
        TextStyle tag = Style(11.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        tag.valign = VAlign::Center;
        for (const std::wstring& text : { mod.category, mod.subCategory.empty() ? std::wstring() : L"\x2192 " + mod.subCategory }) {
            if (text.empty()) continue;
            const float w = std::ceil(ui.g.Measure(text, tag).w) + 20.f;
            if (x + w > r.R()) break;
            ui.g.FillRound({ x, y, w, 22.f }, 11.f, p.primaryMid.color.Fade(0.16f));
            ui.g.Text(text, { x + 10.f, y, w - 12.f, 22.f }, TabKit::Readable(p.primaryMid.color), tag);
            x += w + 8.f;
        }
        y += 34.f;
    }
    const Rect body{ r.x, y, r.w, r.B() - y };
    if (loading) {
        TabKit::Arc(ui.g, body.CX(), body.y + 80.f, 20.f, static_cast<float>(ui.Time()) * 360.f, 260.f, p.primaryMid.color, 3.5f);
        TextStyle t = Style(12.5f, DWRITE_FONT_WEIGHT_NORMAL, false, HAlign::Center);
        ui.g.Text(Loc::Get("store_info_loading"), { body.x, body.y + 112.f, body.w, 20.f }, fg.Fade(0.6f), t);
        ui.KeepAnimating();
        return;
    }
    if (!details) {
        M::DrawIcon(ui.g, M::Icon::AlertCircle, body.CX(), body.y + 70.f, 34.f, Argb(0xFFF4511E));
        TextStyle t = Style(12.5f, DWRITE_FONT_WEIGHT_NORMAL, true, HAlign::Center);
        const std::wstring text = Loc::Fmt("store_info_failed", error);
        ui.g.Text(text, { body.x + 20.f, body.y + 100.f, body.w - 40.f, TextH(ui, text, t, body.w - 40.f) }, fg.Fade(0.75f), t);
        const std::wstring& retry = Loc::Get("store_retry");
        const float w = TabKit::PillWidth(ui, retry);
        if (TabKit::PillButton(ui, ui.MakeId("mi-retry"), { body.CX() - w * 0.5f, body.y + 170.f, w, 40.f }, M::Icon::Refresh, retry, p.primaryMid.color, true)) {
            if (partial) ShowById(mod.id, mod.name);
            else Show(mod);
        }
        return;
    }

    const Store::ModDetails& d = *details;
    UI::ScrollArea scroll(ui, ui.MakeId("mi-scroll", mod.id), body, false, 56.f, UI::ScrollArea::Bar::Material);
    const float w = scroll.ContentWidth() - (scroll.BarShown() ? 14.f : 0.f);
    float cy = body.y;
    // The one-liner, then the description
    if (!d.summary.empty()) {
        TextStyle t = Style(14.f, DWRITE_FONT_WEIGHT_SEMI_BOLD, true);
        const float h = TextH(ui, d.summary, t, w - 18.f);
        ui.g.FillRound({ body.x, cy, 4.f, h + 4.f }, 2.f, p.primaryMid.color);
        ui.g.Text(d.summary, { body.x + 16.f, cy + 2.f, w - 18.f, h }, fg.Fade(0.9f), t);
        cy += h + 18.f;
    }
    cy += Heading(ui, { body.x, cy, w, 0.f }, Loc::Get("store_info_description"));
    {
        const std::wstring& text = d.text.empty() ? Loc::Get("store_info_no_description") : d.text;
        TextStyle t = Style(13.f, DWRITE_FONT_WEIGHT_NORMAL, true);
        t.lineHeight = 20.f;
        const float h = TextH(ui, text, t, w);
        ui.g.Text(text, { body.x, cy, w, h }, fg.Fade(d.text.empty() ? 0.5f : 0.85f), t);
        cy += h + 22.f;
    }
    // Credits: "FloofBiscuit · i created the whole thing" under their group
    if (!d.credits.empty()) {
        cy += Heading(ui, { body.x, cy, w, 0.f }, Loc::Get("store_info_credits"));
        std::wstring group;
        for (const auto& c : d.credits) {
            if (c.group != group) {
                group = c.group;
                TextStyle g = Style(11.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
                g.valign = VAlign::Center;
                ui.g.Text(group, { body.x, cy, w, 20.f }, fg.Fade(0.55f), g);
                cy += 22.f;
            }
            TextStyle n = Style(13.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
            n.valign = VAlign::Center;
            const float nw = std::min(std::ceil(ui.g.Measure(c.name, n).w) + 2.f, w * 0.5f);
            M::DrawIcon(ui.g, M::Icon::Account, body.x + 9.f, cy + 11.f, 16.f, p.primaryMid.color);
            ui.g.Text(c.name, { body.x + 24.f, cy, nw, 22.f }, fg, n);
            if (!c.role.empty()) {
                TextStyle role = Style(12.f);
                role.valign = VAlign::Center;
                role.ellipsis = true;
                ui.g.Text(L"\x00B7  " + c.role, { body.x + 32.f + nw, cy, w - nw - 32.f, 22.f }, fg.Fade(0.6f), role);
            }
            cy += 26.f;
        }
        cy += 14.f;
    }
    // Files: name, size, downloads and what it is
    if (!d.files.empty()) {
        cy += Heading(ui, { body.x, cy, w, 0.f }, Loc::Fmt("store_info_files", static_cast<int>(d.files.size())));
        for (const auto& f : d.files) {
            TextStyle desc = Style(11.5f, DWRITE_FONT_WEIGHT_NORMAL, true);
            const float dh = f.description.empty() ? 0.f : TextH(ui, f.description, desc, w - 60.f);
            const Rect row{ body.x, cy, w, 52.f + (dh > 0.f ? dh + 4.f : 0.f) };
            ui.g.FillRound(row, 12.f, fg.Fade(0.045f));
            M::DrawIcon(ui.g, M::Icon::PackageDown, row.x + 24.f, row.y + 26.f, 22.f, p.primaryMid.color);
            TextStyle n = Style(13.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
            n.valign = VAlign::Center;
            n.ellipsis = true;
            ui.g.Text(f.name, { row.x + 46.f, row.y + 8.f, row.w - 56.f, 20.f }, fg, n);
            TextStyle meta = Style(11.5f);
            meta.valign = VAlign::Center;
            ui.g.Text(Size(f.size) + L"  \x00B7  " + Loc::Fmt("store_info_file_downloads", Thousands(f.downloads)), { row.x + 46.f, row.y + 28.f, row.w - 56.f, 18.f },
                      fg.Fade(0.55f), meta);
            if (dh > 0.f) ui.g.Text(f.description, { row.x + 46.f, row.y + 50.f, w - 60.f, dh }, fg.Fade(0.75f), desc);
            cy += row.h + 8.f;
        }
        cy += 6.f;
    }
    if (!d.license.empty()) {
        cy += Heading(ui, { body.x, cy, w, 0.f }, Loc::Get("store_info_license"));
        TextStyle t = Style(12.f, DWRITE_FONT_WEIGHT_NORMAL, true);
        const std::wstring license = d.license.size() > 1500 ? d.license.substr(0, 1500) + L"\x2026" : d.license;
        const float h = TextH(ui, license, t, w);
        ui.g.Text(license, { body.x, cy, w, h }, fg.Fade(0.65f), t);
        cy += h + 10.f;
    }
    scroll.SetContentSize(cy - body.y);
}

} // namespace SonicRacingSaveManager::Main
