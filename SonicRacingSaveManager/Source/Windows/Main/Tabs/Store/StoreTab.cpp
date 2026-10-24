#include "Windows/Main/Tabs/Store/StoreTab.h"
#include <algorithm>
#include <cmath>
#include "App/Config.h"
#include "Core/Async.h"
#include "Core/Files.h"
#include "Core/Process.h"
#include "Core/Text.h"
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "Services/App/Log.h"
#include "Services/Mods/GameBananaApi.h"
#include "Services/Mods/ModManager.h"
#include "Settings/AppSettings.h"
#include "UI/Material/Buttons.h"
#include "UI/Material/Card.h"
#include "UI/Material/Motion.h"
#include "UI/Material/PackIcon.h"
#include "UI/Material/Theme.h"
#include "UI/Widgets/ScrollArea.h"
#include "UI/Widgets/TextBox.h"
#include "Windows/Dialogs/MessageDialog.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Tabs/Mods/FileSelectionOverlay.h"
#include "Windows/Main/Tabs/Store/ModInfoOverlay.h"
#include "Windows/Main/Tabs/TabBanner.h"
#include "Windows/Main/Tabs/TabKit.h"

namespace SonicRacingSaveManager::Main {

namespace {

namespace M = UI::Material;
namespace Mods = Services::Mods;
namespace Store = Services::Mods::Store;
using TabKit::LineH;
using TabKit::Style;

constexpr float kGap = 16.f, kRadius = 16.f, kChipH = 34.f, kSearchH = 44.f, kPagesH = 52.f;
constexpr int kMaxPictureLoads = 6;
const Color kGreen = Argb(0xFF4CAF50), kRed = Argb(0xFFF4511E);

struct SortOption {
    Store::Sort sort;
    const char* key;
    M::Icon icon;
};
constexpr SortOption kSorts[] = {
    { Store::Sort::Popular, "store_sort_popular", M::Icon::TrendingUp },
    { Store::Sort::Newest, "store_sort_newest", M::Icon::Star },
    { Store::Sort::Updated, "store_sort_updated", M::Icon::Refresh },
    { Store::Sort::MostLiked, "store_sort_liked", M::Icon::Heart },
};

// "2195" → "2,195"
std::wstring Thousands(long long n) {
    std::wstring digits = std::to_wstring(n), out;
    for (size_t i = 0; i < digits.size(); ++i) {
        if (i > 0 && (digits.size() - i) % 3 == 0) out += L',';
        out += digits[i];
    }
    return out;
}

} // namespace

StoreTab::StoreTab(MainWindow& window) : Tab(window), files(std::make_unique<FileSelectionOverlay>(window)) {
    info = std::make_unique<ModInfoOverlay>(window, *this);
    // The file picker goes over the info card (Install from there)
    window.AddOverlay(files.get());
    window.AddOverlay(info.get());
}

StoreTab::~StoreTab() = default;

void StoreTab::OnSelected() {
    loadedAt = -1.0;
    ScanInstalled();
    if (!loadedOnce && !loading) Load();
    if (categories.empty()) LoadCategories();
}

// ── Requests ────────────────────────────────────────────────────────────────

void StoreTab::Load() {
    const unsigned gen = ++generation;
    loading = true;
    error.clear();
    struct Outcome {
        Store::Page page;
        std::wstring error;
    };
    Async::Run(
        [p = page, s = sort, c = categoryId, q = appliedSearch] {
            Outcome o;
            try {
                o.page = Store::FetchPage(p, s, c, q);
            } catch (const Mods::Error& e) {
                o.error = e.Message();
            }
            return o;
        },
        [this, gen](Outcome o) {
            if (gen != generation) return;   // the filters changed meanwhile
            loading = false;
            loadedOnce = true;
            if (!o.error.empty()) {
                error = o.error;
                Log::Warning(L"Mod Store: " + error);
                return;
            }
            mods = std::move(o.page.mods);
            total = o.page.total;
            perPage = std::max(1, o.page.perPage);
            pageShownAt = -1.0;   // the cards come in (and the list goes back up) on the next frame
        });
}

void StoreTab::LoadCategories() {
    Async::Run(
        [] {
            try {
                return Store::FetchCategories();
            } catch (const Mods::Error&) {
                return std::vector<Store::Category>{};
            }
        },
        [this](std::vector<Store::Category> result) { categories = std::move(result); });
}

void StoreTab::ScanInstalled() {
    modsDirectory = Settings::AppSettings::GetModsDirectory().value_or(L"");
    if (modsDirectory.empty() || !Files::DirectoryExists(modsDirectory)) {
        installed.clear();
        return;
    }
    Async::Run([dir = modsDirectory] { return Mods::ModManager(dir).ScanForMods(); },
               [this, dir = modsDirectory](std::vector<Models::ModInfo> result) {
                   if (dir == modsDirectory) installed = std::move(result);
               });
}

ImagePtr StoreTab::Picture(const std::wstring& url, unsigned maxSide) {
    if (url.empty()) return nullptr;
    PictureSlot& slot = pictures[url];
    if (slot.image || slot.failed || slot.loading || picturesLoading >= kMaxPictureLoads) return slot.image;
    slot.loading = true;
    ++picturesLoading;
    struct Decoded {
        unsigned width = 0, height = 0;
        std::vector<uint32_t> pixels;
        bool ok = false;
    };
    Async::Run(
        [url, maxSide] {
            Decoded d;
            const std::string bytes = Store::FetchImage(url);
            d.ok = !bytes.empty() && Images::DecodeMemory(bytes, maxSide, d.width, d.height, d.pixels);
            return d;
        },
        [this, url](Decoded d) {
            --picturesLoading;
            PictureSlot& s = pictures[url];
            s.loading = false;
            if (d.ok) s.image = Images::FromPixels(d.width, d.height, d.pixels.data());
            s.failed = !s.image;
        });
    return nullptr;
}

// ── Installation ────────────────────────────────────────────────────────────

void StoreTab::ShowMod(long long id, const std::wstring& name) { info->ShowById(id, name); }

void StoreTab::Install(const StoreMod& mod) {
    ChooseFiles(mod, [this, mod](std::vector<Models::GameBananaFile> selected) { Download(mod, std::move(selected), false); });
}

// The game keeps its .pak files open: they can't be replaced while it runs
void StoreTab::Reinstall(const StoreMod& mod) {
    if (!Process::FindByName(Config::Game::ProcessName).empty()) {
        Dialogs::Message(main.Hwnd(), Loc::Get("mods_game_running_message"), Loc::Get("mods_game_running_title"), Dialogs::MessageButtons::Ok,
                         Dialogs::MessageIcon::Warning);
        return;
    }
    ChooseFiles(mod, [this, mod](std::vector<Models::GameBananaFile> selected) { Download(mod, std::move(selected), true); });
}

// The mod's files on GameBanana: the only one, or the ones picked when there are several
void StoreTab::ChooseFiles(const StoreMod& mod, std::function<void(std::vector<Models::GameBananaFile>)> then) {
    if (installingId != 0) return;
    const auto fail = [this](const std::wstring& message) {
        installingId = 0;
        notice = message;
        noticeError = true;
        noticeAt = -1.0;
    };
    if (modsDirectory.empty() || !Files::DirectoryExists(modsDirectory)) return fail(Loc::Get("store_need_folder"));
    installingId = mod.id;
    installProgress = 0.0;
    installStatus = Loc::Get("mod_update_fetching");
    struct Outcome {
        std::vector<Models::GameBananaFile> files;
        std::wstring error;
    };
    Async::Run(
        [url = mod.profileUrl] {
            Outcome o;
            try {
                if (auto list = Mods::GameBanana::GetAvailableFiles(url)) o.files = std::move(*list);
            } catch (const Mods::Error& e) {
                o.error = e.Message();
            }
            return o;
        },
        [this, mod, fail, then](Outcome o) {
            if (!o.error.empty()) return fail(Loc::Fmt("store_install_failed", mod.name, o.error));
            o.files.erase(std::remove_if(o.files.begin(), o.files.end(), [](const auto& f) { return !f.downloadUrl || f.downloadUrl->empty(); }), o.files.end());
            if (o.files.empty()) return fail(Loc::Fmt("store_install_failed", mod.name, Loc::Get("mod_update_no_files")));
            if (o.files.size() == 1) return then(std::move(o.files));
            // Several files (versions, variants): pick which ones, the newest (first) checked
            std::vector<FileSelectionOverlay::Item> items;
            for (const auto& f : o.files) items.push_back({ f.fileName.value_or(L"Unknown"), f.fileSize.value_or(0), items.empty() });
            files->Show(mod.name, std::move(items), [this, then, available = o.files](const std::vector<FileSelectionOverlay::Item>* picked) {
                std::vector<Models::GameBananaFile> selected;
                if (picked)
                    for (size_t i = 0; i < picked->size() && i < available.size(); ++i)
                        if ((*picked)[i].isSelected) selected.push_back(available[i]);
                if (selected.empty()) {
                    installingId = 0;   // cancelled
                    return;
                }
                then(std::move(selected));
            });
        });
}

// Installs into a new folder, or (`reinstall`) replaces the files of the installed one
void StoreTab::Download(const StoreMod& mod, std::vector<Models::GameBananaFile> selected, bool reinstall) {
    installProgress = 0.0;
    installStatus = Loc::Get("mod_update_preparing");
    struct Outcome {
        std::wstring folder, error;
    };
    Async::Run(
        [this, mod, reinstall, selected = std::move(selected), dir = modsDirectory, list = installed] {
            Outcome o;
            const auto progress = [this](double percentage, const std::wstring& status) {
                Async::Post([this, percentage, status] {
                    installProgress = percentage;
                    installStatus = status;
                });
            };
            try {
                if (!reinstall) {
                    o.folder = Store::Install(mod, selected, dir, list, progress);
                } else {
                    const int index = Store::FindInstalled(mod, list);
                    if (index < 0) throw Mods::Error(Loc::Fmt("mods_error_folder_not_found", mod.name));
                    o.folder = list[static_cast<size_t>(index)].folderPath;
                    Store::Reinstall(mod, selected, list[static_cast<size_t>(index)], progress);
                }
            } catch (const Mods::Error& e) {
                o.error = e.Message();
            }
            return o;
        },
        [this, mod, reinstall](Outcome o) {
            installingId = 0;
            noticeAt = -1.0;
            if (!o.error.empty()) {
                notice = Loc::Fmt(reinstall ? "store_reinstall_failed" : "store_install_failed", mod.name, o.error);
                noticeError = true;
                Log::Warning(L"Mod Store: " + mod.name + L": " + o.error);
                ScanInstalled();
                return;
            }
            notice = Loc::Fmt(reinstall ? "store_reinstalled" : "store_installed", mod.name, Files::FileName(o.folder));
            noticeError = false;
            ScanInstalled();
            main.ModsChanged();
        });
}

// ── Layout ──────────────────────────────────────────────────────────────────

void StoreTab::Draw(UI::Ui& ui, const Rect& area) {
    if (loadedAt < 0.0) loadedAt = ui.Time();
    // The search applies once the typing stops
    if (searchPending && ui.Time() - searchChangedAt > 0.6) {
        searchPending = false;
        if (Text::Trim(search) != appliedSearch) {
            appliedSearch = Text::Trim(search);
            page = 1;
            Load();
        }
    }
    const Rect grid = area.Inset(20.f);
    const Rect banner{ grid.x, grid.y, grid.w, kTabBannerHeight };
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt, 0, 0.f, 0.5f), 14.f);
        TabBannerSpec spec;
        spec.icon = M::Icon::Storefront;
        spec.motion = TabBannerSpec::Motion::Float;
        spec.title = Loc::Get("store_title");
        spec.subtitle = Loc::Get("store_subtitle");
        spec.pill = total > 0 && appliedSearch.empty() && categoryId == 0 ? Loc::Fmt("store_pill_count", Thousands(total)) : Loc::Get("store_pill");
        spec.pillIcon = M::Icon::Web;
        TabBanner(ui, banner, spec, loadedAt);
    }
    const float toolbarH = 20.f + kSearchH + 12.f + kChipH + 20.f;
    const Rect toolbar{ grid.x, banner.B() + kGap, grid.w, toolbarH };
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.08, 0, 0.f, 0.5f), 16.f);
        M::CardLook look;
        look.radius = kRadius;
        M::Card card(ui, ui.MakeId("st-toolbar"), toolbar, Edges(22.f, 20.f), false, 1.f, look);
        Toolbar(ui, card.content);
    }
    const Rect content{ grid.x, toolbar.B() + kGap, grid.w, grid.B() - toolbar.B() - kGap };
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.16, 0, 0.f, 0.5f), 18.f);
        M::CardLook look;
        look.radius = kRadius;
        M::Card card(ui, ui.MakeId("st-content"), content, Edges(20.f, 18.f), false, 1.f, look);
        Rect c = card.content;
        if (!notice.empty()) {
            Notice(ui, { c.x, c.y, c.w, 44.f });
            c = { c.x, c.y + 56.f, c.w, c.h - 56.f };
        }
        const bool paged = total > perPage;
        Grid(ui, { c.x, c.y, c.w, c.h - (paged ? kPagesH + 8.f : 0.f) });
        if (paged) Pages(ui, { c.x, c.B() - kPagesH, c.w, kPagesH });
    }
}

// ── Toolbar: the search, the sort and the categories ────────────────────────

void StoreTab::Toolbar(UI::Ui& ui, const Rect& r) {
    const M::Palette& p = M::P();
    const float searchW = std::min(440.f, r.w * 0.42f);
    SearchBox(ui, { r.x, r.y, searchW, kSearchH });

    // Sort (the search orders by best match: the chips fade while it's on)
    const bool searching = !appliedSearch.empty();
    float x = r.R();
    for (int i = static_cast<int>(std::size(kSorts)) - 1; i >= 0; --i) {
        const std::wstring& text = Loc::Get(kSorts[i].key);
        const float w = ChipWidth(ui, text) + 22.f;
        x -= w;
        const Rect chip{ x, r.y + (kSearchH - kChipH) * 0.5f, w, kChipH };
        ui.g.PushOpacity(searching ? 0.5f : 1.f);
        const bool on = sort == kSorts[i].sort;
        if (Chip(ui, ui.MakeId("st-sort", i), chip, text, on) && (!on || searching)) {
            sort = kSorts[i].sort;
            search.clear();
            appliedSearch.clear();
            page = 1;
            Load();
        }
        M::DrawIcon(ui.g, kSorts[i].icon, chip.x + 18.f, chip.CY(), 15.f, on ? TabKit::Readable(p.primaryMid.color) : p.foreground.Fade(0.55f));
        ui.g.PopLayer();
        x -= 8.f;
    }
    TextStyle label = Style(11.f, DWRITE_FONT_WEIGHT_SEMI_BOLD, false, HAlign::Right);
    label.valign = VAlign::Center;
    ui.g.Text(Loc::Get("store_sort_by"), { searchW + r.x + 12.f, r.y, x - searchW - r.x - 20.f, kSearchH }, p.foreground.Fade(0.5f), label);

    // Categories: All, then GameBanana's (biggest first)
    float cx = r.x;
    const float cy = r.y + kSearchH + 12.f;
    const auto category = [&](int index, long long id, const std::wstring& text) {
        const float w = ChipWidth(ui, text);
        if (cx + w > r.R()) return;
        const bool on = categoryId == id && !searching;
        if (Chip(ui, ui.MakeId("st-cat", index), { cx, cy, w, kChipH }, text, on) && (!on || searching)) {
            categoryId = id;
            search.clear();
            appliedSearch.clear();
            page = 1;
            Load();
        }
        cx += w + 8.f;
    };
    category(0, 0, Loc::Get("store_category_all"));
    for (size_t i = 0; i < categories.size(); ++i) category(static_cast<int>(i) + 1, categories[i].id, categories[i].name);
}

void StoreTab::SearchBox(UI::Ui& ui, const Rect& r) {
    const M::Palette& p = M::P();
    const UI::Id id = ui.MakeId("st-search");
    if (ui.In().ctrl && ui.KeyPressed('F')) ui.SetFocus(id);
    const bool focused = ui.Focus() == id;
    const float focus = ui.Tween(UI::HashId(id, 0xF0C5), focused ? 1.f : 0.f, 0.15f);
    const float hover = M::Motion::Hover(ui, id, ui.MouseOver(r), false).hover;
    if (focus > 0.f) ui.g.GlowRound(r, r.h * 0.5f, p.primaryMid.color, 12.f, 0.45f * focus);
    ui.g.FillRound(r, r.h * 0.5f, Lerp(p.divider, p.foreground.Fade(0.12f), 0.35f * hover));
    if (focus > 0.f) ui.g.StrokeRound(r.Inset(1.f), r.h * 0.5f - 1.f, p.primaryMid.color.Fade(focus), 2.f);
    // The magnifier wiggles while typing, and spins while the results load
    const float typing = 1.f - Anim::Clamp01(static_cast<float>((ui.Time() - searchChangedAt) / 0.45));
    if (typing > 0.f) ui.KeepAnimating();
    const float wiggle = 14.f * typing * std::sin(static_cast<float>(ui.Time() - searchChangedAt) * 30.f);
    ui.g.PushTransform(D2D1::Matrix3x2F::Rotation(wiggle, D2D1::Point2F(r.x + 24.f, r.CY())));
    M::DrawIcon(ui.g, M::Icon::Magnify, { r.x + 14.f, r.CY() - 10.f, 20.f, 20.f }, Lerp(p.foreground.Fade(0.55f), p.primaryMid.color, focus));
    ui.g.PopTransform();

    UI::TextBoxStyle box;
    box.text = M::Text(13.f);
    box.text.valign = VAlign::Top;
    box.foreground = p.foreground;
    box.background = Colors::Transparent;
    box.border = Colors::Transparent;
    box.focusBorder = Rgba(0, 0, 0, 1);   // invisible: the pill's outline shows the focus
    box.caret = p.primaryMid.color;
    box.selection = p.primaryLight.color.Fade(0.4f);
    box.placeholderColor = p.foreground.Fade(0.45f);
    box.placeholder = Loc::Get("store_search_placeholder");
    box.padding = Edges{ 42.f, (r.h - LineH(ui, box.text)) * 0.5f, search.empty() ? 16.f : 40.f, 0.f };
    const std::wstring before = search;
    UI::TextBox(ui, id, r, search, box);

    bool clear = focused && ui.KeyPressed(VK_ESCAPE);
    if (!search.empty()) {
        M::ButtonLook look = M::IconButtonLook(p.foreground.Fade(0.6f));
        look.padding = Edges(0.f);
        const UI::Id clearId = ui.MakeId("st-search-clear");
        if (M::IconButton(ui, clearId, { r.R() - 36.f, r.CY() - 14.f, 28.f, 28.f }, M::Icon::Close, 16.f, look)) clear = true;
        ui.Tooltip(clearId, Loc::Get("tickets_search_clear"));
    }
    if (clear) {
        search.clear();
        ui.ClearFocus();
    }
    if (search != before) {
        searchChangedAt = ui.Time();
        searchPending = true;
    }
    // Enter searches right away
    if (focused && ui.KeyPressed(VK_RETURN)) searchChangedAt = -10.0;
}

float StoreTab::ChipWidth(UI::Ui& ui, const std::wstring& text) const {
    TextStyle t = Style(12.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    return std::ceil(ui.g.Measure(text, t).w) + 30.f;
}

// A rounded toggle: filled in the primary colour when on. True when clicked.
bool StoreTab::Chip(UI::Ui& ui, UI::Id id, const Rect& r, const std::wstring& text, bool on) {
    const M::Palette& p = M::P();
    const UI::Interaction it = ui.Interact(id, r);
    if (it.hovered) ui.SetCursor(IDC_HAND);
    const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, it.held);
    const float lit = ui.Tween(UI::HashId(id, 0xC41), on ? 1.f : 0.f, 0.18f);
    ui.PushScale(1.f - 0.04f * fb.press, r.CX(), r.CY());
    ui.g.FillRound(r, r.h * 0.5f, Lerp(Lerp(p.foreground.Fade(0.06f), p.foreground.Fade(0.11f), fb.hover), p.primaryMid.color.Fade(0.22f), lit));
    if (lit > 0.f) ui.g.StrokeRound(r.Inset(0.75f), r.h * 0.5f - 0.75f, p.primaryMid.color.Fade(0.7f * lit), 1.5f);
    TextStyle t = Style(12.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD, false, HAlign::Right);
    t.valign = VAlign::Center;
    ui.g.Text(text, { r.x, r.y, r.w - 15.f, r.h }, Lerp(p.foreground.Fade(0.75f), TabKit::Readable(p.primaryMid.color), lit), t);
    ui.PopTransform();
    return it.clicked;
}

// ── Pages and the result of the last installation ───────────────────────────

void StoreTab::Pages(UI::Ui& ui, const Rect& r) {
    const M::Palette& p = M::P();
    const int pages = static_cast<int>(std::min<long long>((total + perPage - 1) / perPage, 9999));
    ui.g.FillRect({ r.x, r.y, r.w, 1.f }, p.divider);
    TextStyle results = Style(12.f);
    results.valign = VAlign::Center;
    ui.g.Text(Loc::Fmt(appliedSearch.empty() ? "store_results" : "store_results_search", Thousands(total), appliedSearch), { r.x, r.y, r.w * 0.3f, r.h },
              p.foreground.Fade(0.6f), results);

    const std::wstring label = Loc::Fmt("store_page_of", page, pages);
    TextStyle mid = Style(13.f, DWRITE_FONT_WEIGHT_SEMI_BOLD, false, HAlign::Center);
    mid.valign = VAlign::Center;
    const float lw = std::ceil(ui.g.Measure(label, mid).w) + 30.f;
    const std::wstring& prev = Loc::Get("store_previous");
    const std::wstring& next = Loc::Get("store_next");
    const float pw = TabKit::PillWidth(ui, prev), nw = TabKit::PillWidth(ui, next);
    const float x0 = r.CX() - (pw + lw + nw + 20.f) * 0.5f, by = r.CY() - 18.f + 4.f;
    if (TabKit::PillButton(ui, ui.MakeId("st-prev"), { x0, by, pw, 36.f }, M::Icon::ChevronLeft, prev, p.primaryMid.color, false, page > 1 && !loading)) {
        --page;
        Load();
    }
    ui.g.Text(label, { x0 + pw + 10.f, r.y + 4.f, lw, r.h }, p.foreground, mid);
    if (TabKit::PillButton(ui, ui.MakeId("st-next"), { x0 + pw + 10.f + lw + 10.f, by, nw, 36.f }, M::Icon::ChevronRight, next, p.primaryMid.color, false,
                           page < pages && !loading)) {
        ++page;
        Load();
    }
}

void StoreTab::Notice(UI::Ui& ui, const Rect& r) {
    const M::Palette& p = M::P();
    if (noticeAt == -1.0) noticeAt = ui.Time();
    const float k = Anim::EaseOutCubic(Anim::Clamp01(static_cast<float>((ui.Time() - noticeAt) / 0.35)));
    if (k < 1.f) ui.KeepAnimating();
    M::Motion::Enter enter(ui, k, -8.f);
    const Color c = noticeError ? kRed : kGreen;
    ui.g.FillRound(r, 12.f, c.Fade(0.13f));
    ui.g.StrokeRound(r.Inset(0.5f), 11.5f, c.Fade(0.45f), 1.f);
    M::DrawIcon(ui.g, noticeError ? M::Icon::AlertCircle : M::Icon::CheckCircle, r.x + 22.f, r.CY(), 20.f, TabKit::Readable(c));
    TextStyle t = Style(12.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    t.valign = VAlign::Center;
    t.ellipsis = true;
    float right = r.R() - 44.f;
    // A successful installation offers the Mod Manager
    if (!noticeError && !installed.empty()) {
        const std::wstring& open = Loc::Get("store_show_in_manager");
        const float w = TabKit::PillWidth(ui, open);
        right -= w + 8.f;
        if (TabKit::PillButton(ui, ui.MakeId("st-notice-open"), { right + 8.f, r.CY() - 15.f, w, 30.f }, M::Icon::Puzzle, open, kGreen, false))
            main.SelectTab(MainWindow::Mods);
    }
    ui.g.Text(notice, { r.x + 42.f, r.y, right - r.x - 50.f, r.h }, p.foreground, t);
    M::ButtonLook look = M::IconButtonLook(p.foreground.Fade(0.6f));
    look.padding = Edges(0.f);
    if (M::IconButton(ui, ui.MakeId("st-notice-close"), { r.R() - 38.f, r.CY() - 14.f, 28.f, 28.f }, M::Icon::Close, 16.f, look)) notice.clear();
}

} // namespace SonicRacingSaveManager::Main
