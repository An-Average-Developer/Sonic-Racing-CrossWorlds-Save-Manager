#include "Windows/Main/Tabs/SaveEditor/SaveEditorTab.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <set>
#include "App/Config.h"
#include "Core/Async.h"
#include "Core/Files.h"
#include "Core/Process.h"
#include "Core/Zip.h"
#include "Core/System.h"
#include "App/Paths.h"
#include "Core/Text.h"
#include "Graphics/Images.h"
#include "Core/Time.h"
#include "Data/SaveCatalog.h"
#include "Data/StickerCatalog.h"
#include "Graphics/Geometry.h"
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "Services/App/Log.h"
#include "Services/Backup/BackupService.h"
#include "UI/Material/Buttons.h"
#include "UI/Material/Motion.h"
#include "UI/Material/Theme.h"
#include "UI/Widgets/ScrollArea.h"
#include "Windows/Dialogs/MessageDialog.h"
#include "Windows/Main/MainState.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Tabs/Backup/BackupKit.h"
#include "Windows/Main/Tabs/Backup/BackupState.h"
#include "Windows/Main/Tabs/SaveEditor/SaveEditorBuild.h"
#include "Windows/Main/Tabs/SaveEditor/SaveEditorCourse.h"
#include "Windows/Main/Tabs/SaveEditor/SaveEditorSwitch.h"
#include "Windows/Main/Tabs/SaveEditor/SaveEditorVariants.h"
#include "Windows/Main/Tabs/TabKit.h"
#include "Windows/Main/Tabs/Konami.h"

// The Save Editor tab: opening, saving and closing app.sav, the header, the account picker and the
// characters (SaveEditorItems.cpp, their versions' card in SaveEditorVariants.cpp).
namespace SonicRacingSaveManager::Main {

namespace M = UI::Material;
namespace SF = Services::SaveFile;
using namespace SaveEditorKit;
using TabKit::Style;

namespace {
const Models::SaveAccount* FindAccount(MainWindow& main, const std::wstring& id) {
    for (const auto& a : main.Backups().Accounts())
        if (a.accountId == id) return &a;
    return nullptr;
}
std::wstring AccountName(const Models::SaveAccount* a, const std::wstring& id) { return a && !a->accountName.empty() ? a->accountName : id; }

// A round picture (or icon) with a ring around it
void Avatar(UI::Ui& ui, float cx, float cy, float radius, const ImagePtr& picture, M::Icon icon, Color ring) {
    const M::Palette& p = M::P();
    if (picture) ui.g.DrawImageRounded(*picture, { cx - radius, cy - radius, radius * 2.f, radius * 2.f }, radius, 1.f, Stretch::UniformToFill);
    else {
        ui.g.FillEllipse(cx, cy, radius, radius, p.primaryMid.color.Fade(0.16f));
        M::DrawIcon(ui.g, icon, cx, cy, radius * 1.05f, TabKit::Readable(p.primaryMid.color));
    }
    ui.g.StrokeEllipse(cx, cy, radius + 2.5f, radius + 2.5f, ring, 2.f);
}
} // namespace

SaveEditorTab::SaveEditorTab(MainWindow& window)
    : Tab(window), variants(std::make_unique<SaveEditorVariants>(window, *this)), course(std::make_unique<SaveEditorCourse>(window, *this)),
      build(std::make_unique<SaveEditorBuild>(window, *this)), switchHelp(std::make_unique<SaveEditorSwitch>(window, *this)) {
    window.AddOverlay(variants.get());
    window.AddOverlay(course.get());
    window.AddOverlay(build.get());
    window.AddOverlay(switchHelp.get());
}

SaveEditorTab::~SaveEditorTab() = default;

void SaveEditorTab::OnDeselected() {
    variants->Close();
    course->Close();
    build->Close();
    switchHelp->Close();
}

void SaveEditorTab::Status(const std::wstring& text, bool error) {
    main.State().statusMessage = text;
    if (error) {
        Log::Warning(L"Save Editor: " + text);
        Dialogs::Message(main.Hwnd(), text, Loc::Get("saveeditor_error_title"), Dialogs::MessageButtons::Ok, Dialogs::MessageIcon::Error);
    } else {
        Log::Info(L"Save Editor: " + text);
    }
}

void SaveEditorTab::Open(const std::wstring& id, const std::wstring& path) {
    const auto load = [this, id, path] {
        try {
            save = SF::AppSave::Open(path);
            accountId = id;
            category = CharactersCategory();
            section = -1;   // the menu
            courseSearch.clear();
            variants->Close();
            course->Close();
    build->Close();
            search.clear();
            filter = 0;
            shopTag = -1;
            bursts.Clear();
            ++version;
            screenAt = -10.0;
            loadedAt = -1.0;   // the editor plays its entrance
            const Models::SaveAccount* account = FindAccount(main, id);
            Status(id == kSwitchAccount ? Loc::Fmt("saveeditor_switch_opened", path)
                                        : Loc::Fmt("saveeditor_opened", account ? account->ShortDisplay() : id));
        } catch (const SF::SaveError& e) {
            Status(e.Message(), true);
        }
    };
    if (save && save->Dirty()) {
        Dialogs::Message(main.Hwnd(), Loc::Get("saveeditor_discard"), Loc::Get("saveeditor_title"), Dialogs::MessageButtons::YesNo,
                         Dialogs::MessageIcon::Warning, [load](Dialogs::MessageResult r) {
                             if (r == Dialogs::MessageResult::Yes) load();
                         });
        return;
    }
    load();
}

void SaveEditorTab::OpenSwitchSave() {
    if (!busy) switchHelp->Open();
}

void SaveEditorTab::PickSwitchSave() {
    if (busy) return;
    const std::wstring path = System::PickFile(main.Hwnd(), Loc::Get("saveeditor_switch_pick"),
                                               { { Loc::Get("saveeditor_switch_filter"), L"app" }, { Loc::Get("filter_all"), L"*.*" } });
    if (path.empty()) return;
    // The whole backup folder is needed (the zip takes all its files): a lone "app" is most likely not it
    const auto files = Files::ListFiles(Files::DirectoryName(path));
    if (files.size() < 2)
        Dialogs::Message(main.Hwnd(), Loc::Get("saveeditor_switch_lonely"), Loc::Get("saveeditor_switch_title"), Dialogs::MessageButtons::Ok,
                         Dialogs::MessageIcon::Warning);
    Open(kSwitchAccount, path);
}

void SaveEditorTab::Reload() {
    if (!save || busy) return;
    const std::wstring path = save->Path();
    const auto load = [this, path] {
        try {
            save = SF::AppSave::Open(path);
            ++version;
            changedOnDisk = false;
            Status(Loc::Get("saveeditor_reloaded"));
        } catch (const SF::SaveError& e) {
            Status(e.Message(), true);
        }
    };
    if (save->Dirty()) {
        Dialogs::Message(main.Hwnd(), Loc::Get("saveeditor_discard"), Loc::Get("saveeditor_title"), Dialogs::MessageButtons::YesNo,
                         Dialogs::MessageIcon::Warning, [load](Dialogs::MessageResult r) {
                             if (r == Dialogs::MessageResult::Yes) load();
                         });
        return;
    }
    load();
}

void SaveEditorTab::CloseSave() {
    if (busy) return;
    const auto close = [this] {
        variants->Close();
        course->Close();
    build->Close();
        save.reset();
        accountId.clear();
        category = -1;
        ++version;
        loadedAt = -1.0;
        main.Backups().RefreshAccounts();
    };
    if (save && save->Dirty()) {
        Dialogs::Message(main.Hwnd(), Loc::Get("saveeditor_discard"), Loc::Get("saveeditor_title"), Dialogs::MessageButtons::YesNo,
                         Dialogs::MessageIcon::Warning, [close](Dialogs::MessageResult r) {
                             if (r == Dialogs::MessageResult::Yes) close();
                         });
        return;
    }
    close();
}

void SaveEditorTab::UnlockEverything() {
    if (!save || busy) return;
    Dialogs::Message(main.Hwnd(), Loc::Get("saveeditor_unlock_everything_confirm"), Loc::Get("saveeditor_unlock_everything"), Dialogs::MessageButtons::YesNo,
                     Dialogs::MessageIcon::Warning, [this](Dialogs::MessageResult res) {
                         if (res != Dialogs::MessageResult::Yes) return;
                         int changed = 0;
                         Edit([&] { changed = save->UnlockEverything(); });
                         Status(Loc::Fmt("saveeditor_unlocked_n", std::to_wstring(changed)));
                     });
}

void SaveEditorTab::SaveToDisk(bool overwrite) {
    if (!save || busy) return;
    // The game keeps its own copy and writes it back: a save written now would be lost (a Switch save isn't the PC game's)
    if (!IsSwitch() && !Process::FindByName(Config::Game::ProcessName).empty()) {
        Dialogs::Message(main.Hwnd(), Loc::Get("saveeditor_game_running"), Loc::Get("saveeditor_title"), Dialogs::MessageButtons::Ok,
                         Dialogs::MessageIcon::Warning);
        return;
    }
    // The game saved since this was opened: writing now would put the old state back over it
    if (!overwrite && save->ChangedOnDisk()) {
        Dialogs::Message(main.Hwnd(), Loc::Get("saveeditor_changed_on_disk"), Loc::Get("saveeditor_title"), Dialogs::MessageButtons::YesNoCancel,
                         Dialogs::MessageIcon::Warning, [this](Dialogs::MessageResult r) {
                             if (!save) return;
                             if (r == Dialogs::MessageResult::Yes) {
                                 const std::wstring path = save->Path();
                                 save = std::nullopt;   // read again, dropping the edits (they were asked about)
                                 Open(accountId, path);
                             } else if (r == Dialogs::MessageResult::No) {
                                 SaveToDisk(true);
                             }
                         });
        return;
    }
    struct Result {
        bool ok = false;
        std::wstring error;
        uint64_t size = 0;
        std::wstring zip;          // a Switch save: the zip of its folder, for JKSV
        std::wstring zipError;     // (the save itself was written)
    };
    const std::string bytes = save->Encoded();
    const std::wstring path = save->Path(), id = accountId, backupName = Loc::Get("saveeditor_backup_name");
    // A Switch save is backed up outside its JKSV folder: JKSV puts everything in it back on the console
    const std::wstring switchBackup = IsSwitch() ? Files::Combine(Files::Combine(Paths::DataDir(), L"SwitchBackups"), Time::Stamp(Time::Now())) : L"";
    busy = true;
    main.State().statusMessage = Loc::Get("saveeditor_saving");
    // The backup first; nothing is written when it fails
    Async::Run(
        [bytes, path, id, backupName, switchBackup] {
            Result r;
            try {
                if (!switchBackup.empty()) {
                    if (!Files::CreateDirectories(switchBackup) || !Files::Copy(path, Files::Combine(switchBackup, Files::FileName(path))))
                        throw SF::SaveError(Loc::Fmt("saveeditor_switch_backup_failed", switchBackup));
                } else {
                    Services::Backup::BackupService().CreateBackup(id, backupName);
                }
                SF::AppSave::WriteFile(path, bytes);
                r.ok = true;
                r.size = bytes.size();
                // A Switch save: every file of its JKSV backup folder (the edited "app" with them) in <folder>.zip
                // beside the folder, which JKSV restores as it is
                if (!switchBackup.empty()) {
                    const std::wstring folder = Files::DirectoryName(path);
                    const std::wstring zipPath = Files::Combine(Files::DirectoryName(folder), Files::FileName(folder) + L".zip");
                    Zip::Writer zip;
                    bool read = true;
                    for (const auto& file : Files::ListFiles(folder, L"*", true)) {
                        const auto data = Files::ReadBytes(file);
                        if (!data) {
                            read = false;
                            break;
                        }
                        zip.Add(Text::Replace(Files::RelativePath(folder, file), L"\\", L"/"), *data, true, Files::LastWriteTime(file).value_or(0));
                    }
                    if (read && Files::WriteBytes(zipPath, zip.Finish())) r.zip = zipPath;
                    else r.zipError = zipPath;
                }
            } catch (const Services::Backup::Error& e) {
                r.error = e.Message();
            } catch (const SF::SaveError& e) {
                r.error = e.Message();
            }
            return r;
        },
        [this](Result r) {
            busy = false;
            if (!r.ok) {
                Status(Loc::Fmt("saveeditor_save_failed", r.error), true);
                return;
            }
            if (save) save->Written(r.size);
            changedOnDisk = false;
            main.Backups().RefreshBackups();
            if (!r.zipError.empty()) {
                Status(Loc::Fmt("saveeditor_switch_zip_failed", r.zipError), true);
            } else if (!r.zip.empty()) {
                Status(Loc::Fmt("saveeditor_switch_saved", r.zip));
                Process::ShowInExplorer(r.zip);
            } else {
                Status(Loc::Get("saveeditor_saved"));
            }
        });
}

void SaveEditorTab::OpenCategory(UI::Ui& ui, int index) {
    if (index < 0) index = CharactersCategory();   // only the characters are shown
    if (index == category) return;
    category = index;
    screenAt = ui.Time();
    search.clear();
    filter = 0;
    shopTag = -1;
    UI::ScrollArea::ScrollTo(ui, ui.MakeId("se-items"), 0.f);
}

void SaveEditorTab::KonamiCode(UI::Ui& ui) {
    if (KonamiTyped(ui, konamiStep)) {
        SF::SetKonamiExtras(!SF::KonamiExtras());
        Status(Loc::Get(SF::KonamiExtras() ? "saveeditor_konami_on" : "saveeditor_konami_off"));
    }
    // Typed here or in the Tickets Menu
    if (konami != SF::KonamiExtras()) {
        konami = SF::KonamiExtras();
        if (!konami) mixedSlots.clear();
        ++version;   // (the machine editor offers other parts now)
    }
}

void SaveEditorTab::Draw(UI::Ui& ui, const Rect& area) {
    if (loadedAt < 0.0) loadedAt = ui.Time();
    KonamiCode(ui);
    const Rect grid = area.Inset(22.f, 18.f);
    const Rect header{ grid.x, grid.y, grid.w, kHeaderH };
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt, 0, 0.f, 0.45f), 12.f);
        Header(ui, header);
    }
    if (!save) {
        Picker(ui, { grid.x, header.B() + kGap + 4.f, grid.w, grid.B() - header.B() - kGap - 4.f });
        return;
    }
    if (category < 0) category = CharactersCategory();
    const double at = std::max(loadedAt + 0.08, screenAt);
    if (section < 0) {
        // The menu rises in; a section slides in from the right
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, at, 0, 0.f, 0.4f), 20.f);
        Menu(ui, { grid.x, header.B() + kGap + 4.f, grid.w, grid.B() - header.B() - kGap - 4.f });
        return;
    }
    const Rect tabs{ grid.x, header.B() + kGap, grid.w, kSectionsH };
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, at, 0, 0.f, 0.4f), 10.f);
        Sections(ui, tabs);
        if (section < 0) return;   // back to the menu: it draws from the next frame
    }
    const Rect body{ grid.x, tabs.B() + kGap, grid.w, grid.B() - tabs.B() - kGap };
    M::Motion::Enter enter(ui, M::Motion::Stagger(ui, at, 0, 0.f, 0.35f), 0.f, 36.f);
    if (section == 1) CoursesView(ui, body);
    else if (section == 5) VehiclesView(ui, body);
    else if (section == 6) GadgetsView(ui, body);
    else if (section == 7) CurrencyView(ui, body);
    else if (section == kProfileSection) ProfileView(ui, body);
    else if (section == kMusicSection) MusicView(ui, body);
    else if (section == kStatsSection) StatsView(ui, body);
    else if (section == kMissionsSection) MissionsView(ui, body);
    else CategoryView(ui, body);
}

// ── Header ──────────────────────────────────────────────────────────────────────────────────────

// The account (picture, name, id · file · size) and the save's state on the left; Save, Reload and
// Close on the right. Without a save, the tab's name and what it does.
void SaveEditorTab::Header(UI::Ui& ui, const Rect& r) {
    const M::Palette& p = M::P();
    const Color accent = p.primaryMid.color;
    Card(ui, r, kRadius, accent, 0.f);
    // A slow drifting light along the top
    {
        const float t = static_cast<float>(std::fmod(ui.Time() * 0.08, 1.0));
        ui.KeepAnimatingThrottled();
        ui.g.PushRoundClip(r, kRadius);
        const float cx = r.x + r.w * (t * 1.4f - 0.2f);
        ui.g.FillLinear({ cx - 260.f, r.y, 520.f, 2.f }, 0.f, { { 0.f, Colors::Transparent }, { 0.5f, accent.Fade(0.8f) }, { 1.f, Colors::Transparent } },
                        { cx - 260.f, r.y }, { cx + 260.f, r.y });
        ui.g.PopLayer();
    }
    const float cy = r.CY();
    TextStyle title = Style(19.f, DWRITE_FONT_WEIGHT_BOLD);
    title.valign = VAlign::Center;
    title.ellipsis = true;
    TextStyle meta = Style(11.5f);
    meta.valign = VAlign::Center;
    meta.ellipsis = true;

    if (!save) {
        Avatar(ui, r.x + 44.f, cy, 24.f, nullptr, M::Icon::ContentSaveEdit, accent.Fade(0.5f));
        ui.g.Text(Loc::Get("saveeditor_title"), { r.x + 84.f, cy - 22.f, r.w - 100.f, 26.f }, p.foreground, title);
        ui.g.Text(Loc::Get("saveeditor_subtitle"), { r.x + 84.f, cy + 4.f, r.w - 100.f, 18.f }, p.foreground.Fade(0.6f), meta);
        return;
    }

    // Actions, from the right
    const float bh = 40.f, by = cy - bh * 0.5f;
    float right = r.R() - 18.f;
    M::ButtonLook ghost = M::IconButtonLook();
    right -= bh;
    const UI::Id closeId = ui.MakeId("se-close");
    if (M::IconButton(ui, closeId, { right, by, bh, bh }, M::Icon::Close, 20.f, ghost, !busy)) {
        CloseSave();
        if (!save) return;
    }
    ui.Tooltip(closeId, Loc::Get("saveeditor_close_tip"));
    // Reload: amber once the game has written the file since it was read (looked at once a second)
    if (ui.Time() - diskCheckedAt >= 1.0) {
        diskCheckedAt = ui.Time();
        changedOnDisk = !busy && save->ChangedOnDisk();
    }
    right -= bh + 4.f;
    const UI::Id rid = ui.MakeId("se-reload");
    if (M::IconButton(ui, rid, { right, by, bh, bh }, M::Icon::Refresh, 20.f, changedOnDisk ? M::IconButtonLook(kAmber) : ghost, !busy)) Reload();
    ui.Tooltip(rid, Loc::Get(changedOnDisk ? "saveeditor_reload_changed" : "saveeditor_reload_tip"));
    const bool dirty = save->Dirty();
    const std::wstring& saveText = Loc::Get(busy ? "saveeditor_saving_short" : "saveeditor_save");
    const float sw = TabKit::PillWidth(ui, saveText) + 16.f;
    right -= sw + 10.f;
    const Rect saveR{ right, by, sw, bh };
    // While there is something to save, a light sweeps over the button now and then
    if (dirty && !busy) {
        const float t = static_cast<float>(std::fmod(ui.Time(), 2.4) / 2.4);
        ui.g.GlowRound(saveR, bh * 0.5f, accent, 14.f, 0.25f + 0.2f * std::sin(t * 6.2831853f));
        ui.KeepAnimatingThrottled();
    }
    const UI::Id sid = ui.MakeId("se-save");
    if (TabKit::PillButton(ui, sid, saveR, M::Icon::ContentSave, saveText, accent, true, dirty && !busy)) SaveToDisk();
    if (dirty && !busy) {
        const float t = static_cast<float>(std::fmod(ui.Time(), 2.4) / 2.4);
        if (t < 0.45f) {
            const float sx = saveR.x + saveR.w * (t / 0.45f * 1.4f - 0.2f);
            ui.g.PushRoundClip(saveR, bh * 0.5f);
            ui.g.FillLinear({ sx - 24.f, saveR.y, 48.f, saveR.h }, 0.f,
                            { { 0.f, Colors::Transparent }, { 0.5f, Colors::White.Fade(0.35f) }, { 1.f, Colors::Transparent } }, { sx - 24.f, saveR.y },
                            { sx + 24.f, saveR.y });
            ui.g.PopLayer();
        }
    }
    ui.Tooltip(sid, Loc::Get("saveeditor_tools_hint"));

    // The account (a Switch save: the console's icon, and its file's folder)
    const Models::SaveAccount* account = FindAccount(main, accountId);
    const Color state = dirty ? kAmber : kGreen;
    const bool isSwitch = IsSwitch();
    Avatar(ui, r.x + 44.f, cy, 24.f, isSwitch ? nullptr : BackupKit::SteamAvatar(accountId), isSwitch ? M::Icon::NintendoSwitch : M::Icon::Account, state);
    const float x = r.x + 84.f, w = std::max(0.f, right - 20.f - x);
    ui.g.Text(isSwitch ? Loc::Get("saveeditor_switch_name") : AccountName(account, accountId), { x, cy - 23.f, w, 26.f }, p.foreground, title);
    float mx = x;
    const float pulse = dirty ? 0.65f + 0.35f * std::sin(static_cast<float>(ui.Time()) * 4.f) : 1.f;
    mx += Chip(ui, mx, cy + 13.f, Loc::Get(dirty ? "saveeditor_unsaved" : "saveeditor_saved_state"), state.Fade(pulse), false, true) + 10.f;
    const std::wstring where = isSwitch ? Files::FileName(Files::DirectoryName(save->Path())) + L"   \x00B7   " + Files::FileName(save->Path())
                                  : L"ID " + accountId + L"   \x00B7   app.sav";
    ui.g.Text(where + L"   \x00B7   " + Text::FormatBytes(save->Size()), { mx, cy + 4.f, std::max(0.f, x + w - mx), 18.f }, p.foreground.Fade(0.55f), meta);
}

// ── Sections ────────────────────────────────────────────────────────────────────────────────────

void SaveEditorTab::SwitchSection(UI::Ui& ui, int index) {
    if (index == section) return;
    section = index;
    screenAt = ui.Time();
    variants->Close();
    course->Close();
    build->Close();
    // Every section but the courses, the currencies and the profile is a category (its bar, filter and search)
    if (index == 7 || index == kProfileSection || index == kMusicSection || index == kStatsSection || index == kMissionsSection) {
        category = 0;   // the profile's numbers
        UI::ScrollArea::ScrollTo(ui, ui.MakeId("se-items"), 0.f);
    } else if (index >= 0 && index != 1) {
        static const SF::ListId kLists[] = { SF::ListId::Drivers, SF::ListId::Drivers, SF::ListId::HonorTitles, SF::ListId::Stickers, SF::ListId::Horns,
                                             SF::ListId::Machines, SF::ListId::Gadgets, SF::ListId::Drivers, SF::ListId::Challenges,
                                             SF::ListId::Auras };
        category = CategoryOf(kLists[index]);
        search.clear();
        filter = 0;
        shopTag = -1;
        UI::ScrollArea::ScrollTo(ui, ui.MakeId("se-items"), 0.f);
    }
    if (index == 6) {
        gadgetView = 0;
        gadgetPreset = -1;
    }
}

namespace {

// A cover cut in slanted panels, like a strip of screenshots: `count` panels side by side (vertical
// cuts leaning by `slant`) or stacked (horizontal cuts), each drawn by `panel` inside its bounds
// while clipped to its shape, with a line of `tone` between them and along the long edges
void SlantedCover(UI::Ui& ui, const Rect& r, int count, bool stacked, float slant, Color tone, const std::function<void(int, const Rect&)>& panel) {
    if (count <= 0) return;
    const float span = (stacked ? r.h : r.w) + slant;
    const float step = span / count;
    // Edge k: on the panels' far side it is shifted by `slant`
    const auto edge = [&](int k) { return (stacked ? r.y : r.x) + k * step; };
    for (int k = 0; k < count; ++k) {
        const float a0 = edge(k), a1 = edge(k + 1);
        const auto shape = Geometry::Build([&](ID2D1GeometrySink* s) {
            if (stacked) {
                // Left side at a, right side at a - slant
                s->BeginFigure({ r.x, a0 }, D2D1_FIGURE_BEGIN_FILLED);
                s->AddLine({ r.R(), a0 - slant });
                s->AddLine({ r.R(), a1 - slant });
                s->AddLine({ r.x, a1 });
            } else {
                // Top at a, bottom at a - slant
                s->BeginFigure({ a0, r.y }, D2D1_FIGURE_BEGIN_FILLED);
                s->AddLine({ a1, r.y });
                s->AddLine({ a1 - slant, r.B() });
                s->AddLine({ a0 - slant, r.B() });
            }
            s->EndFigure(D2D1_FIGURE_END_CLOSED);
        });
        ui.g.PushLayer(1.f, shape.Get());
        const Rect bounds = stacked ? Rect{ r.x, a0 - slant, r.w, a1 - a0 + slant } : Rect{ a0 - slant, r.y, a1 - a0 + slant, r.h };
        panel(k, bounds);
        ui.g.PopLayer();
    }
    // The cuts, then the accent along the long edges
    for (int k = 1; k < count; ++k) {
        const float a = edge(k);
        if (stacked) ui.g.Line(r.x, a, r.R(), a - slant, tone, 3.f);
        else ui.g.Line(a, r.y, a - slant, r.B(), tone, 3.f);
    }
    if (stacked) {
        ui.g.FillRect({ r.x, r.y, 3.f, r.h }, tone);
        ui.g.FillRect({ r.R() - 3.f, r.y, 3.f, r.h }, tone);
    } else {
        ui.g.FillRect({ r.x, r.y, r.w, 3.f }, tone);
        ui.g.FillRect({ r.x, r.B() - 3.f, r.w, 3.f }, tone);
    }
}

constexpr Color kHornTone = Rgba(249, 115, 22);   // the horns: orange
constexpr Color kVehicleTone = Rgba(14, 165, 233);   // the vehicles: sky blue
constexpr Color kGadgetTone = Rgba(236, 72, 153);    // the gadgets: pink
constexpr Color kCurrencyTone = Rgba(234, 179, 8);   // the currencies: gold
constexpr Color kChallengeTone = Rgba(16, 185, 129);  // the challenges: emerald
constexpr Color kAuraTone = Rgba(139, 92, 246);       // the auras: violet
constexpr Color kProfileTone = Rgba(59, 130, 246);    // the profile: blue
// The sections in the order the menu and the bar show them (the profile, added last, first)
constexpr Color kMusicTone = Rgba(20, 184, 166);      // the music: teal
constexpr Color kStatsTone = Rgba(99, 102, 241);     // the statistics: indigo
constexpr Color kMissionTone = Rgba(244, 63, 94);    // the Play Missions: rose
constexpr int kSectionCount = 14;
// (10 the profile; 9 the auras, after the horns; 11 the music; 13 the Play Missions after the challenges, 12 the statistics last)
constexpr int kSectionOrder[kSectionCount] = { 10, 0, 1, 2, 3, 4, 9, 11, 5, 6, 7, 8, 13, 12 };
// The honor titles' and challenges' covers: this many plates, each covering its strip end to end; with
// more the strips are too low for the names (a plate that wide is ~100 px tall)
constexpr size_t kTitleStrips = 3;

// A section of the menu: its name, icon, colour and unlocked / total (or, for the currencies, what
// it shows instead: your tickets)
struct SectionInfo {
    M::Icon icon;
    const char* key;
    const char* hint;
    Color tone;
    int done = 0, total = 0;
    std::wstring value;
};

// A number with its thousands apart, as the language writes them: 2,512,363 / 2.512.363
std::wstring Grouped(int64_t n) {
    const std::wstring digits = Text::FromInt(n < 0 ? -n : n);
    std::wstring out;
    const wchar_t sep = Loc::CurrentLang() == L"en" ? L',' : L'.';
    for (size_t k = 0; k < digits.size(); ++k) {
        if (k && (digits.size() - k) % 3 == 0) out.push_back(sep);
        out.push_back(digits[k]);
    }
    return n < 0 ? L"-" + out : out;
}

// The currencies' card and switch: your tickets and Duel Medals
std::wstring CurrencySummary(const Services::SaveFile::AppSave& save) {
    int64_t tickets = 0, medals = 0;
    for (const auto& f : save.Numbers()) {
        if (f.key == "donpa_tickets") tickets = f.value;
        else if (f.key == "duel_medals") medals = f.value;
    }
    return Loc::Fmt("saveeditor_currency_summary", Grouped(tickets), Grouped(medals));
}

// The statistics' card and switch: races and wins
std::wstring StatsSummary(const Services::SaveFile::AppSave& save) {
    int64_t races = 0, wins = 0;
    for (const auto& f : save.StatNumbers()) {
        if (f.key == "stat_TotalRacesCount") races = f.value;
        else if (f.key == "stat_TotalVictoryCount") wins = f.value;
    }
    return Loc::Fmt("saveeditor_stats_summary", Grouped(races), Grouped(wins));
}

// The Play Missions' done / total
std::pair<int, int> MissionCount(const Services::SaveFile::AppSave& save) {
    int done = 0, total = 0;
    for (const auto& m : save.Missions()) {
        ++total;
        done += m.state == Services::SaveFile::MissionState::Cleared;
    }
    return { done, total };
}

// A number in a few characters, for a narrow switch: 950, 12.3 k, 2.5 M
std::wstring Abbreviated(int64_t n) {
    const auto one = [](double v, const wchar_t* unit) {
        wchar_t buf[32];
        swprintf(buf, 32, v < 100.0 ? L"%.1f %ls" : L"%.0f %ls", v, unit);
        return std::wstring(buf);
    };
    if (n < 10000) return Text::FromInt(n);
    if (n < 1000000) return one(n / 1000.0, L"k");
    return one(n / 1000000.0, L"M");
}

// The currencies' switch: your tickets, short
std::wstring CurrencyShort(const Services::SaveFile::AppSave& save) {
    for (const auto& f : save.Numbers())
        if (f.key == "donpa_tickets") return Abbreviated(f.value);
    return L"0";
}
} // namespace

// Back to the menu, then the three sections as wide switches (the open one filled, with its count)
void SaveEditorTab::Sections(UI::Ui& ui, const Rect& r) {
    const M::Palette& p = M::P();
    int ringsDone = 0, ringsTotal = 0;
    for (const auto& c : Courses())
        for (bool b : c.rings) {
            ++ringsTotal;
            ringsDone += b;
        }
    const Stat& chars = Stats()[static_cast<size_t>(CharactersCategory())];
    const Stat& titles = Stats()[static_cast<size_t>(CategoryOf(SF::ListId::HonorTitles))];
    const Stat& stickers = Stats()[static_cast<size_t>(CategoryOf(SF::ListId::Stickers))];
    const Stat& horns = Stats()[static_cast<size_t>(CategoryOf(SF::ListId::Horns))];
    const Stat vehicles = VehicleStat(Stats());
    const Stat& gadgets = Stats()[static_cast<size_t>(CategoryOf(SF::ListId::Gadgets))];
    const Stat& challenges = Stats()[static_cast<size_t>(CategoryOf(SF::ListId::Challenges))];
    const Stat& auras = Stats()[static_cast<size_t>(CategoryOf(SF::ListId::Auras))];
    const Stat overall = OverallStat();
    const Stat& albums = Stats()[static_cast<size_t>(CategoryOf(SF::ListId::Albums))];
    const Stat& tracks = Stats()[static_cast<size_t>(CategoryOf(SF::ListId::Tracks))];
    const Stat music{ albums.done + tracks.done, albums.total + tracks.total, L"" };
    const std::pair<int, int> missions = save ? MissionCount(*save) : std::pair<int, int>{};
    int64_t statRaces = 0;
    if (save)
        for (const auto& f : save->StatNumbers())
            if (f.key == "stat_TotalRacesCount") statRaces = f.value;
    constexpr int kSections = kSectionCount;
    const SectionInfo items[kSections] = {
        { M::Icon::Account, "saveeditor_section_characters", "", GroupColor(1), chars.done, chars.total },
        { M::Icon::FlagCheckered, "saveeditor_section_courses", "", kRing, ringsDone, ringsTotal },
        { M::Icon::Medal, "saveeditor_section_titles", "", kGold, titles.done, titles.total },
        { M::Icon::Sticker, "saveeditor_section_stickers", "", GroupColor(2), stickers.done, stickers.total },
        { M::Icon::Bullhorn, "saveeditor_section_horns", "", kHornTone, horns.done, horns.total },
        { M::Icon::CarSports, "saveeditor_section_vehicles", "", kVehicleTone, vehicles.done, vehicles.total },
        { M::Icon::Flash, "saveeditor_section_gadgets", "", kGadgetTone, gadgets.done, gadgets.total },
        { M::Icon::Ticket, "saveeditor_section_currency", "", kCurrencyTone, 0, 0, save ? CurrencyShort(*save) : std::wstring{} },
        { M::Icon::TrophyAward, "saveeditor_section_challenges", "", kChallengeTone, challenges.done, challenges.total },
        { M::Icon::Creation, "saveeditor_section_auras", "", kAuraTone, auras.done, auras.total },
        { M::Icon::AccountStar, "saveeditor_section_profile", "", kProfileTone, overall.done, overall.total },
        { M::Icon::MusicNote, "saveeditor_section_music", "", kMusicTone, music.done, music.total },
        { M::Icon::TrendingUp, "saveeditor_section_stats", "", kStatsTone, 0, 0, save ? Abbreviated(statRaces) : std::wstring{} },
        { M::Icon::FormatListNumbered, "saveeditor_section_missions", "", kMissionTone, missions.first, missions.second },
    };
    TextStyle name = Style(14.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    name.valign = VAlign::Center;
    name.ellipsis = true;
    TextStyle count = Style(12.f, DWRITE_FONT_WEIGHT_SEMI_BOLD, false, HAlign::Right);
    count.valign = VAlign::Center;

    // Menu: with its name, or its arrow alone when the bar is narrow
    const std::wstring& menu = Loc::Get("saveeditor_menu");
    const bool narrow = r.w < 1100.f;
    const float mw = narrow ? 46.f : TabKit::PillWidth(ui, menu);
    const UI::Id menuId = ui.MakeId("se-menu");
    if (TabKit::PillButton(ui, menuId, { r.x, r.CY() - 19.f, mw, 38.f }, M::Icon::ChevronLeft, narrow ? L"" : menu, p.primaryMid.color, false)) {
        SwitchSection(ui, -1);
        return;
    }
    if (narrow) ui.Tooltip(menuId, menu);

    // Each switch as wide as it needs: its icon, name and count. When they don't all fit the counts go
    // (to the tooltip), then the names of all but the open one: those show their icon alone
    std::wstring names[kSections], counts[kSections];
    float nameW[kSections], countW[kSections];
    for (int k = 0; k < kSections; ++k) {
        names[k] = Loc::Get(items[k].key);
        counts[k] = items[k].value.empty() ? std::to_wstring(items[k].done) + L" / " + std::to_wstring(items[k].total) : items[k].value;
        nameW[k] = std::ceil(ui.g.Measure(names[k], name).w);
        countW[k] = std::ceil(ui.g.Measure(counts[k], count).w);
    }
    constexpr float kIconOnly = 52.f;
    const auto full = [&](int k) { return 44.f + nameW[k] + 12.f + countW[k] + 18.f; };
    const auto named = [&](int k) { return 44.f + nameW[k] + 18.f; };
    const float x0 = r.x + mw + kGap;
    const float avail = r.R() - x0 - (kSections - 1) * kGap;
    int mode = 0;   // 0 name and count, 1 names, 2 icons (the open one with its name and count)
    float widths[kSections];
    for (; mode < 3; ++mode) {
        float sum = 0.f;
        for (int k = 0; k < kSections; ++k) {
            widths[k] = mode == 0 ? full(k) : mode == 1 ? named(k) : (section == k ? full(k) : kIconOnly);
            sum += widths[k];
        }
        if (sum <= avail || mode == 2) {
            // What is left is shared out, so the switches fill the bar; one too wide still (the open one,
            // icons only) gives up what is missing
            const float spare = (avail - sum) / kSections;
            for (int k = 0; k < kSections; ++k) widths[k] = std::max(kIconOnly, widths[k] + spare);
            break;
        }
    }

    float x = x0;
    for (const int k : kSectionOrder) {
        const UI::Id id = ui.MakeId("se-section", k);
        // Slide to the new widths when the open section changes (icons only)
        const float cx = ui.Animate(UI::HashId(id, 0x51), x, 14.f, x);
        const float cw = ui.Animate(UI::HashId(id, 0x52), widths[k], 14.f, widths[k]);
        x += widths[k] + kGap;
        const Rect cell{ cx, r.y, cw, r.h };
        const auto it = ui.Interact(id, cell);
        const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, it.held);
        const bool on = section == k;
        const float sel = ui.Tween(UI::HashId(id, 0x5E), on ? 1.f : 0.f, 0.2f);
        const Color tone = items[k].tone;
        Card(ui, cell, r.h * 0.5f, tone, on ? 0.f : fb.hover);
        ui.g.FillRound(cell, r.h * 0.5f, tone.Fade(0.85f * sel));
        const Color fg = Lerp(p.foreground, Colors::White, sel);
        const bool iconOnly = mode == 2 && !on;
        const bool withCount = mode == 0 || (mode == 2 && on);
        ui.g.PushRoundClip(cell, r.h * 0.5f);
        M::DrawIcon(ui.g, items[k].icon, iconOnly ? cell.CX() : cell.x + 26.f, cell.CY(), 20.f, Lerp(TabKit::Readable(tone), Colors::White, sel));
        if (!iconOnly) {
            // The count takes what it needs, the name the rest
            const float cwText = withCount ? countW[k] : 0.f;
            ui.g.Text(names[k], { cell.x + 44.f, cell.y, std::max(0.f, cell.w - 44.f - cwText - (withCount ? 30.f : 18.f)), cell.h }, fg, name);
            if (withCount) ui.g.Text(counts[k], { cell.R() - 16.f - cwText - 2.f, cell.y, cwText + 2.f, cell.h }, fg.Fade(0.75f), count);
        }
        ui.g.PopLayer();
        // What doesn't show goes to the tooltip (the currencies': the tickets and Duel Medals in full)
        const std::wstring detail = k == 7 && save ? CurrencySummary(*save) : k == kStatsSection && save ? StatsSummary(*save) : counts[k];
        if (!withCount || k == 7 || k == kStatsSection) ui.Tooltip(id, iconOnly ? names[k] + L"\n" + detail : detail);
        if (it.hovered && !on) ui.SetCursor(IDC_HAND);
        if (it.clicked) SwitchSection(ui, k);
    }
}

// A big card per section: its picture (the character on the card, a course, the title on the card),
// name, what it holds, and its progress (a bar and unlocked / total). A card opens its section.
void SaveEditorTab::Menu(UI::Ui& ui, const Rect& r) {
    const M::Palette& p = M::P();
    int ringsDone = 0, ringsTotal = 0;
    for (const auto& c : Courses())
        for (bool b : c.rings) {
            ++ringsTotal;
            ringsDone += b;
        }
    const Stat& chars = Stats()[static_cast<size_t>(CharactersCategory())];
    const Stat& titles = Stats()[static_cast<size_t>(CategoryOf(SF::ListId::HonorTitles))];
    const Stat& stickers = Stats()[static_cast<size_t>(CategoryOf(SF::ListId::Stickers))];
    const Stat& horns = Stats()[static_cast<size_t>(CategoryOf(SF::ListId::Horns))];
    const Stat vehicles = VehicleStat(Stats());
    const Stat& gadgets = Stats()[static_cast<size_t>(CategoryOf(SF::ListId::Gadgets))];
    const Stat& challenges = Stats()[static_cast<size_t>(CategoryOf(SF::ListId::Challenges))];
    const Stat& auras = Stats()[static_cast<size_t>(CategoryOf(SF::ListId::Auras))];
    const Stat overall = OverallStat();
    const Stat& albums = Stats()[static_cast<size_t>(CategoryOf(SF::ListId::Albums))];
    const Stat& tracks = Stats()[static_cast<size_t>(CategoryOf(SF::ListId::Tracks))];
    const Stat music{ albums.done + tracks.done, albums.total + tracks.total, L"" };
    const std::pair<int, int> missions = save ? MissionCount(*save) : std::pair<int, int>{};
    int64_t statRaces = 0;
    if (save)
        for (const auto& f : save->StatNumbers())
            if (f.key == "stat_TotalRacesCount") statRaces = f.value;
    constexpr int kSections = kSectionCount;
    const SectionInfo items[kSections] = {
        { M::Icon::Account, "saveeditor_section_characters", "saveeditor_menu_characters", GroupColor(1), chars.done, chars.total },
        { M::Icon::FlagCheckered, "saveeditor_section_courses", "saveeditor_menu_courses", kRing, ringsDone, ringsTotal },
        { M::Icon::Medal, "saveeditor_section_titles", "saveeditor_menu_titles", kGold, titles.done, titles.total },
        { M::Icon::Sticker, "saveeditor_section_stickers", "saveeditor_menu_stickers", GroupColor(2), stickers.done, stickers.total },
        { M::Icon::Bullhorn, "saveeditor_section_horns", "saveeditor_menu_horns", kHornTone, horns.done, horns.total },
        { M::Icon::CarSports, "saveeditor_section_vehicles", "saveeditor_menu_vehicles", kVehicleTone, vehicles.done, vehicles.total },
        { M::Icon::Flash, "saveeditor_section_gadgets", "saveeditor_menu_gadgets", kGadgetTone, gadgets.done, gadgets.total },
        { M::Icon::Ticket, "saveeditor_section_currency", "saveeditor_menu_currency", kCurrencyTone, 0, 0, save ? CurrencySummary(*save) : std::wstring{} },
        { M::Icon::TrophyAward, "saveeditor_section_challenges", "saveeditor_menu_challenges", kChallengeTone, challenges.done, challenges.total },
        { M::Icon::Creation, "saveeditor_section_auras", "saveeditor_menu_auras", kAuraTone, auras.done, auras.total },
        { M::Icon::AccountStar, "saveeditor_section_profile", "saveeditor_menu_profile", kProfileTone, overall.done, overall.total },
        { M::Icon::MusicNote, "saveeditor_section_music", "saveeditor_menu_music", kMusicTone, music.done, music.total },
        { M::Icon::TrendingUp, "saveeditor_section_stats", "saveeditor_menu_stats", kStatsTone, 0, 0, save ? StatsSummary(*save) : std::wstring{} },
        { M::Icon::FormatListNumbered, "saveeditor_section_missions", "saveeditor_menu_missions", kMissionTone, missions.first, missions.second },
    };
    // The covers: the characters you have (the one on your card first) and your honor titles (the one
    // on your card first), one of each family, read again after edits
    if (menuFor != version && save) {
        menuFor = version;
        menuDrivers.clear();
        menuTitles.clear();
        std::vector<std::wstring> drivers, locked;
        for (const auto& row : save->Rows(SF::ListId::Drivers)) {
            if (row.dlc || Data::SaveCatalog::DriverFamily(row.id) != row.id || !Picture(SF::ListId::Drivers, row.id)) continue;
            if (row.state != SF::ItemState::Unlocked) locked.push_back(row.id);
            else if (!row.canLock) drivers.insert(drivers.begin(), row.id);   // the selected one can't be locked
            else drivers.push_back(row.id);
        }
        drivers.insert(drivers.end(), locked.begin(), locked.end());
        drivers.resize(std::min<size_t>(drivers.size(), 6));
        menuDrivers = drivers;
        const std::wstring on = std::to_wstring(save->SelectedTitle());
        std::vector<std::pair<int, std::wstring>> picks;   // (group, id)
        std::set<int> seen;
        for (const auto& row : save->Rows(SF::ListId::HonorTitles)) {
            const int id = static_cast<int>(Text::ToInt(row.id).value_or(-1));
            if (row.state != SF::ItemState::Unlocked || !Picture(SF::ListId::HonorTitles, row.id)) continue;
            if (!seen.insert(Data::SaveCatalog::TitleFamily(id)).second && row.id != on) continue;
            picks.push_back({ row.id == on ? -1 : Data::SaveCatalog::TitleGroup(id), row.id });
        }
        std::stable_sort(picks.begin(), picks.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        for (const auto& [group, id] : picks)
            if (menuTitles.size() < kTitleStrips) menuTitles.push_back(id);
        // Stickers: one per pack / kind you have, characters first
        menuStickers.clear();
        std::set<int> packs;
        std::vector<std::pair<int, std::wstring>> stickerPicks;
        for (const auto& row : save->Rows(SF::ListId::Stickers)) {
            const int id = static_cast<int>(Text::ToInt(row.id).value_or(-1));
            // (the "SONIC ART" line art would vanish on the cover's dark panels)
            if (row.state != SF::ItemState::Unlocked || !Picture(SF::ListId::Stickers, row.id) || row.name == L"SONIC ART") continue;
            const int pack = Data::StickerCatalog::Pack(id);
            // One per pack; of the base game's, every tenth (Sonic, then others further on)
            if (pack ? !packs.insert(pack).second : id % 10 != 0) continue;
            stickerPicks.push_back({ static_cast<int>(Data::StickerCatalog::GroupOf(id)), row.id });
        }
        std::stable_sort(stickerPicks.begin(), stickerPicks.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        for (const auto& [group, id] : stickerPicks)
            if (menuStickers.size() < 6) menuStickers.push_back(id);
        // Horns: the festivals' pictures, the ones you have first
        menuHorns.clear();
        std::vector<std::wstring> lockedHorns;
        for (const auto& row : save->Rows(SF::ListId::Horns)) {
            if (!Picture(SF::ListId::Horns, row.id)) continue;
            (row.state == SF::ItemState::Unlocked ? menuHorns : lockedHorns).push_back(row.id);
        }
        menuHorns.insert(menuHorns.end(), lockedHorns.begin(), lockedHorns.end());
        menuHorns.resize(std::min<size_t>(menuHorns.size(), 6));
        // Vehicles: the garage's machines (by their front part), then the complete machines you have
        menuVehicles.clear();
        for (const auto& b : Garage())
            if (Picture(SF::ListId::FrontParts, b.front)) menuVehicles.push_back({ SF::ListId::FrontParts, b.front });
        for (const auto& row : save->Rows(SF::ListId::Machines))
            if (row.state == SF::ItemState::Unlocked && Picture(SF::ListId::Machines, row.id)) menuVehicles.push_back({ SF::ListId::Machines, row.id });
        menuVehicles.resize(std::min<size_t>(menuVehicles.size(), 6));
        // Gadgets: the first player's preset raced with, then the ones you have
        menuGadgets.clear();
        for (const auto& preset : save->GadgetPresets(0))
            if (preset.selected)
                for (const auto& g : preset.gadgets)
                    if (Picture(SF::ListId::Gadgets, g.id)) menuGadgets.push_back(g.id);
        for (const auto& row : save->Rows(SF::ListId::Gadgets))
            if (row.state == SF::ItemState::Unlocked && row.id != L"NonGadget" && Picture(SF::ListId::Gadgets, row.id) &&
                std::find(menuGadgets.begin(), menuGadgets.end(), row.id) == menuGadgets.end())
                menuGadgets.push_back(row.id);
        menuGadgets.resize(std::min<size_t>(menuGadgets.size(), 6));
        // Challenges: the titles they give (challenge N, title N), the ones done first
        menuChallenges.clear();
        std::vector<std::wstring> notDone;
        for (const auto& row : save->Rows(SF::ListId::Challenges)) {
            const std::wstring title = std::to_wstring(Text::ToInt(Text::Replace(row.id, L"CHALLENGE", L"")).value_or(0));
            if (!Picture(SF::ListId::HonorTitles, title)) continue;
            (row.state == SF::ItemState::Unlocked ? menuChallenges : notDone).push_back(title);
        }
        menuChallenges.insert(menuChallenges.end(), notDone.begin(), notDone.end());
        menuChallenges.resize(std::min<size_t>(menuChallenges.size(), kTitleStrips));
        menuAuras.clear();
        std::vector<std::wstring> tinted, lockedAuras;
        for (const auto& row : save->Rows(SF::ListId::Auras)) {
            const ImagePtr image = Picture(SF::ListId::Auras, row.id);
            if (!image || row.unreleased) continue;
            if (row.state != SF::ItemState::Unlocked) lockedAuras.push_back(row.id);
            else (image->w > image->h * 1.2f ? menuAuras : tinted).push_back(row.id);
        }
        menuAuras.insert(menuAuras.end(), tinted.begin(), tinted.end());
        menuAuras.insert(menuAuras.end(), lockedAuras.begin(), lockedAuras.end());
        menuAuras.resize(std::min<size_t>(menuAuras.size(), 6));
        // Music: the jackets of the albums you have, then the others
        menuAlbums.clear();
        std::vector<std::wstring> lockedAlbums;
        for (const auto& row : save->Rows(SF::ListId::Albums)) {
            if (!Picture(SF::ListId::Albums, row.id)) continue;
            (row.state == SF::ItemState::Unlocked ? menuAlbums : lockedAlbums).push_back(row.id);
        }
        // (the base game's first: they share the main album's jacket, so one of them is enough)
        menuAlbums.insert(menuAlbums.end(), lockedAlbums.begin(), lockedAlbums.end());
        menuAlbums.resize(std::min<size_t>(menuAlbums.size(), 6));
        // Statistics: the characters raced with most
        menuStatDrivers.clear();
        for (const auto& u : save->Usage(false))
            if (menuStatDrivers.size() < 6 && Picture(SF::ListId::Drivers, u.id)) menuStatDrivers.push_back(u.id);
    }
    const ImagePtr courseCover = Courses().empty() ? nullptr : GameImage(L"stage", Courses().front().id, 400);
    TextStyle title = Style(20.f, DWRITE_FONT_WEIGHT_BOLD);
    title.valign = VAlign::Center;
    title.ellipsis = true;
    TextStyle hint = Style(12.f, DWRITE_FONT_WEIGHT_NORMAL, true);
    TextStyle count = Style(13.f, DWRITE_FONT_WEIGHT_SEMI_BOLD, false, HAlign::Right);
    count.valign = VAlign::Center;

    UI::ScrollArea scroll(ui, ui.MakeId("se-menu-scroll"), r, false, 60.f, UI::ScrollArea::Bar::Thin);
    const float w = r.w - 10.f;
    // Two by two: the covers need their width (a banner's name fits its strip)
    const int cols = w >= 560.f ? 2 : 1;
    const float cw = (w - (cols - 1) * kGap) / cols;
    const float ch = 330.f;
    const double reveal = std::max(loadedAt, screenAt) + 0.1;
    for (int j = 0; j < kSections; ++j) {
        const int k = kSectionOrder[j];
        const SectionInfo& s = items[k];
        const Rect card{ r.x + (j % cols) * (cw + kGap), r.y + (j / cols) * (ch + kGap), cw, ch };
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, reveal, j, 0.07f, 0.45f), 22.f);
        const UI::Id id = ui.MakeId("se-menu-card", k);
        const auto it = ui.Interact(id, card);
        const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, it.held);
        const Rect lifted = card.Offset(0.f, -5.f * fb.hover + 1.f * fb.press);
        Card(ui, lifted, kRadius + 4.f, s.tone, fb.hover);

        // The picture: the characters in slanted panels, the honor titles in slanted strips, a course
        const Rect pic{ lifted.x + 14.f, lifted.y + 14.f, lifted.w - 28.f, 190.f };
        ui.g.PushRoundClip(pic, 14.f);
        ui.g.FillLinear(pic, 0.f, { { 0.f, s.tone.Fade(0.35f) }, { 1.f, s.tone.Fade(0.08f) } }, { pic.x, pic.y }, { pic.R(), pic.B() });
        const float grow = 6.f * fb.hover;
        const auto shade = [&](const Rect& b) {
            // The bottom darkens a little, like the reference strip's panels
            ui.g.FillLinear(b, 0.f, { { 0.55f, Colors::Transparent }, { 1.f, Colors::Black.Fade(0.35f) } }, { b.x, b.y }, { b.x, b.B() });
        };
        if (k == 0 && !menuDrivers.empty()) {
            SlantedCover(ui, pic, static_cast<int>(menuDrivers.size()), false, pic.h * 0.3f, s.tone, [&](int i, const Rect& b) {
                ui.g.FillRect(b, Lerp(s.tone, Colors::Black, 0.55f + 0.1f * (i % 2)));
                if (const ImagePtr image = Picture(SF::ListId::Drivers, menuDrivers[static_cast<size_t>(i)]))
                    ui.g.DrawImage(*image, b.Inset(-grow), 1.f, Stretch::UniformToFill);
                shade(b);
            });
        } else if (k == 2 && !menuTitles.empty()) {
            SlantedCover(ui, pic, static_cast<int>(menuTitles.size()), true, 14.f, s.tone, [&](int i, const Rect& b) {
                ui.g.FillRect(b, Lerp(s.tone, Colors::Black, 0.6f + 0.1f * (i % 2)));
                const std::wstring& titleId = menuTitles[static_cast<size_t>(i)];
                if (const ImagePtr image = Picture(SF::ListId::HonorTitles, titleId)) {
                    // Straight, and covering its whole slanted strip: at least as tall as the strip's bounds
                    // (the plates' art fills about 3/4 of their height)
                    // (wide enough to cover the strip end to end, its rounded ends past the cut: taller than the
                    // strip then, the plate's top and bottom trimmed, its name in the middle whole)
                    const Rect art = TitleArt(titleId);
                    const float bw = std::max(b.h / std::max(0.1f, art.h), b.w * 1.06f / std::max(0.1f, art.w) / 8.f) * 8.f * (1.f + 0.03f * fb.hover), bh = bw / 8.f;
                    // The first and last strips reach past the picture: their banner moves in so the name
                    // shows whole (its art still covers what shows of the strip)
                    const float cy = std::clamp(b.CY(), pic.y + bh * 0.26f, pic.B() - bh * 0.26f);
                    ui.g.DrawImage(*image, { b.CX() - bw * art.CX(), cy - bh * art.CY(), bw, bh }, 1.f, Stretch::Fill);
                }
            });
        } else if (k == 3 && !menuStickers.empty()) {
            SlantedCover(ui, pic, static_cast<int>(menuStickers.size()), false, pic.h * 0.3f, s.tone, [&](int i, const Rect& b) {
                ui.g.FillRect(b, Lerp(s.tone, Colors::Black, 0.5f + 0.12f * (i % 2)));
                // A sticker in the middle of its panel (they have see-through backgrounds)
                if (const ImagePtr image = Picture(SF::ListId::Stickers, menuStickers[static_cast<size_t>(i)])) {
                    const float side = std::min(b.w * 1.2f, b.h * 0.85f) + grow;
                    ui.g.DrawImage(*image, { b.CX() - side * 0.5f, b.CY() - side * 0.5f, side, side }, 1.f, Stretch::Uniform);
                }
                shade(b);
            });
        } else if (k == 4 && !menuHorns.empty()) {
            SlantedCover(ui, pic, static_cast<int>(menuHorns.size()), false, pic.h * 0.3f, s.tone, [&](int i, const Rect& b) {
                ui.g.FillRect(b, Lerp(s.tone, Colors::Black, 0.55f + 0.1f * (i % 2)));
                if (const ImagePtr image = Picture(SF::ListId::Horns, menuHorns[static_cast<size_t>(i)]))
                    ui.g.DrawImage(*image, b.Inset(-grow), 1.f, Stretch::UniformToFill);
                shade(b);
            });
        } else if (k == 5 && !menuVehicles.empty()) {
            SlantedCover(ui, pic, static_cast<int>(menuVehicles.size()), false, pic.h * 0.3f, s.tone, [&](int i, const Rect& b) {
                ui.g.FillRect(b, Lerp(s.tone, Colors::Black, 0.55f + 0.1f * (i % 2)));
                const auto& [list, id] = menuVehicles[static_cast<size_t>(i)];
                if (const ImagePtr image = Picture(list, id)) {
                    const float side = std::min(b.w * 1.25f, b.h * 0.8f) + grow;
                    ui.g.DrawImage(*image, { b.CX() - side * 0.5f, b.CY() - side * 0.5f, side, side }, 1.f, Stretch::Uniform);
                }
                shade(b);
            });
        } else if (k == 6 && !menuGadgets.empty()) {
            SlantedCover(ui, pic, static_cast<int>(menuGadgets.size()), false, pic.h * 0.3f, s.tone, [&](int i, const Rect& b) {
                ui.g.FillRect(b, Lerp(s.tone, Colors::Black, 0.55f + 0.1f * (i % 2)));
                if (const ImagePtr image = Picture(SF::ListId::Gadgets, menuGadgets[static_cast<size_t>(i)])) {
                    const float side = std::min(b.w * 1.1f, b.h * 0.7f) + grow;
                    ui.g.DrawImage(*image, { b.CX() - side * 0.5f, b.CY() - side * 0.5f, side, side }, 1.f, Stretch::Uniform);
                }
                shade(b);
            });
        } else if (k == 1 && courseCover) {
            ui.g.DrawImage(*courseCover, pic.Inset(-grow), 1.f, Stretch::UniformToFill);
        } else if (k == 7 && save) {
            // Currencies: a panel each, the game's picture of it over how many you have
            struct Coin {
                const wchar_t* kind;
                const wchar_t* image;
                const char* key;
            };
            static constexpr Coin coins[] = { { L"currency", L"tickets", "donpa_tickets" }, { L"currency", L"medals", "duel_medals" },
                                              { L"rsr", L"on", "red_star_rings" }, { L"gadget", L"unknown", "gadget_fragments" } };
            const auto fields = save->Numbers();
            TextStyle amount = Style(15.f, DWRITE_FONT_WEIGHT_BOLD, false, HAlign::Center);
            amount.valign = VAlign::Center;
            amount.ellipsis = true;
            SlantedCover(ui, pic, 4, false, pic.h * 0.3f, s.tone, [&](int i, const Rect& b) {
                ui.g.FillRect(b, Lerp(s.tone, Colors::Black, 0.5f + 0.1f * (i % 2)));
                const Coin& c = coins[i];
                const float side = std::min(b.w * 0.75f, b.h * 0.5f) + grow;
                if (const ImagePtr image = GameImage(c.kind, c.image, 128))
                    ui.g.DrawImage(*image, { b.CX() - side * 0.5f, b.y + b.h * 0.42f - side * 0.5f, side, side }, 1.f, Stretch::Uniform);
                for (const auto& f : fields)
                    if (f.key == c.key) ui.g.Text(Abbreviated(f.value), { b.x + 8.f, b.y + b.h * 0.72f, b.w - 16.f, 26.f }, Colors::White, amount);
                shade(b);
            });
        } else if (k == 9 && !menuAuras.empty()) {
            // Auras: in slanted panels, a machine wearing it (the exchange's pictures fill their panel)
            SlantedCover(ui, pic, static_cast<int>(menuAuras.size()), false, pic.h * 0.3f, s.tone, [&](int i, const Rect& b) {
                ui.g.FillRect(b, Lerp(s.tone, Colors::Black, 0.6f + 0.1f * (i % 2)));
                if (const ImagePtr image = Picture(SF::ListId::Auras, menuAuras[static_cast<size_t>(i)])) {
                    if (image->w > image->h * 1.2f) ui.g.DrawImage(*image, b.Inset(-grow), 1.f, Stretch::UniformToFill);
                    else {
                        const float side = std::min(b.w * 1.2f, b.h * 0.75f) + grow;
                        ui.g.DrawImage(*image, { b.CX() - side * 0.5f, b.CY() - side * 0.5f, side, side }, 1.f, Stretch::Uniform);
                    }
                }
                shade(b);
            });
        } else if (k == 8 && !menuChallenges.empty()) {
            // Challenges: the titles they give, in strips (as the honor titles' cover)
            SlantedCover(ui, pic, static_cast<int>(menuChallenges.size()), true, 14.f, s.tone, [&](int i, const Rect& b) {
                ui.g.FillRect(b, Lerp(s.tone, Colors::Black, 0.6f + 0.1f * (i % 2)));
                const std::wstring& titleId = menuChallenges[static_cast<size_t>(i)];
                if (const ImagePtr image = Picture(SF::ListId::HonorTitles, titleId)) {
                    // (wide enough to cover the strip end to end, its rounded ends past the cut: taller than the
                    // strip then, the plate's top and bottom trimmed, its name in the middle whole)
                    const Rect art = TitleArt(titleId);
                    const float bw = std::max(b.h / std::max(0.1f, art.h), b.w * 1.06f / std::max(0.1f, art.w) / 8.f) * 8.f * (1.f + 0.03f * fb.hover), bh = bw / 8.f;
                    const float cy = std::clamp(b.CY(), pic.y + bh * 0.26f, pic.B() - bh * 0.26f);
                    ui.g.DrawImage(*image, { b.CX() - bw * art.CX(), cy - bh * art.CY(), bw, bh }, 1.f, Stretch::Fill);
                }
            });
        } else if (k == kMusicSection && !menuAlbums.empty()) {
            // Music: the albums' jackets in slanted panels
            SlantedCover(ui, pic, static_cast<int>(menuAlbums.size()), false, pic.h * 0.3f, s.tone, [&](int i, const Rect& b) {
                ui.g.FillRect(b, Lerp(s.tone, Colors::Black, 0.55f + 0.1f * (i % 2)));
                if (const ImagePtr image = Picture(SF::ListId::Albums, menuAlbums[static_cast<size_t>(i)]))
                    ui.g.DrawImage(*image, b.Inset(-grow), 1.f, Stretch::UniformToFill);
                shade(b);
            });
        } else if (k == kStatsSection && !menuStatDrivers.empty()) {
            // Statistics: the characters raced with most, in slanted panels
            SlantedCover(ui, pic, static_cast<int>(menuStatDrivers.size()), false, pic.h * 0.3f, s.tone, [&](int i, const Rect& b) {
                ui.g.FillRect(b, Lerp(s.tone, Colors::Black, 0.55f + 0.1f * (i % 2)));
                if (const ImagePtr image = Picture(SF::ListId::Drivers, menuStatDrivers[static_cast<size_t>(i)]))
                    ui.g.DrawImage(*image, b.Inset(-grow), 1.f, Stretch::UniformToFill);
                shade(b);
            });
        } else if (k == kMissionsSection && save && missions.second) {
            // Play Missions: the rewards (tickets, Duel Medals) still to get
            int64_t tickets = 0, medals = 0;
            for (const auto& m : save->Missions()) {
                if (m.claimed) continue;
                if (m.rewardType == SF::kRewardDonpaTickets) tickets += m.rewardCount;
                else if (m.rewardType == SF::kRewardDuelMedals) medals += m.rewardCount;
            }
            TextStyle amount = Style(15.f, DWRITE_FONT_WEIGHT_BOLD, false, HAlign::Center);
            amount.valign = VAlign::Center;
            const std::pair<const wchar_t*, int64_t> coins[] = { { L"tickets", tickets }, { L"medals", medals } };
            SlantedCover(ui, pic, 2, false, pic.h * 0.3f, s.tone, [&](int i, const Rect& b) {
                ui.g.FillRect(b, Lerp(s.tone, Colors::Black, 0.5f + 0.1f * (i % 2)));
                const float side = std::min(b.w * 0.5f, b.h * 0.5f) + grow;
                if (const ImagePtr image = GameImage(L"currency", coins[i].first, 128))
                    ui.g.DrawImage(*image, { b.CX() - side * 0.5f, b.y + b.h * 0.42f - side * 0.5f, side, side }, 1.f, Stretch::Uniform);
                ui.g.Text(L"\x00D7" + Grouped(coins[i].second), { b.x + 8.f, b.y + b.h * 0.72f, b.w - 16.f, 26.f }, Colors::White, amount);
                shade(b);
            });
        } else if (k == kProfileSection && save) {
            // The profile: the account's picture with its level, and the whole save's progress in a ring
            const float cy = pic.CY();
            const float ax = pic.x + pic.w * 0.3f;
            Avatar(ui, ax, cy, 58.f + grow * 0.5f, BackupKit::SteamAvatar(accountId), M::Icon::Account, s.tone);
            int64_t level = 0;
            for (const auto& f : save->Numbers())
                if (f.key == "level") level = f.value;
            TextStyle levelText = Style(13.f, DWRITE_FONT_WEIGHT_BOLD, false, HAlign::Center);
            levelText.valign = VAlign::Center;
            const std::wstring lv = Loc::Fmt("saveeditor_profile_level", Text::FromInt(level));
            const float lw = std::ceil(ui.g.Measure(lv, levelText).w) + 24.f;
            const Rect plate{ ax - lw * 0.5f, cy + 46.f, lw, 26.f };
            ui.g.FillRound(plate, 13.f, s.tone);
            ui.g.Text(lv, plate, Colors::White, levelText);
            const float rx = pic.x + pic.w * 0.72f;
            const float v = s.total ? static_cast<float>(s.done) / static_cast<float>(s.total) : 0.f;
            Ring(ui, rx, cy, 54.f + grow * 0.5f, 10.f, v, s.done == s.total ? kGreen : s.tone);
            TextStyle pct = Style(22.f, DWRITE_FONT_WEIGHT_BOLD, false, HAlign::Center);
            pct.valign = VAlign::Center;
            ui.g.Text(std::to_wstring(static_cast<int>(std::floor(v * 100.f))) + L"%", { rx - 50.f, cy - 16.f, 100.f, 32.f }, p.foreground, pct);
        } else {
            M::DrawIcon(ui.g, s.icon, pic.CX(), pic.CY(), 64.f + grow, TabKit::Readable(s.tone).Fade(0.7f));
        }
        ui.g.PopLayer();

        // Icon, name, what it holds
        const float ty = pic.B() + 16.f;
        ui.g.FillEllipse(lifted.x + 34.f, ty + 16.f, 18.f, 18.f, s.tone.Fade(0.2f));
        M::DrawIcon(ui.g, s.icon, lifted.x + 34.f, ty + 16.f, 20.f, TabKit::Readable(s.tone));
        ui.g.Text(Loc::Get(s.key), { lifted.x + 62.f, ty, lifted.w - 62.f - 110.f, 32.f }, p.foreground, title);
        const long long n = Counter(ui, UI::HashId(id, 0xC0), s.done, 0.7f);
        if (!s.value.empty()) {
            const float vw = std::min(lifted.w * 0.5f, std::ceil(ui.g.Measure(s.value, count).w) + 2.f);
            ui.g.Text(s.value, { lifted.R() - 18.f - vw, ty, vw, 32.f }, p.foreground.Fade(0.7f), count);
        } else {
            ui.g.Text(Text::FromInt(n) + L" / " + std::to_wstring(s.total), { lifted.R() - 130.f, ty, 112.f, 32.f },
                      s.done == s.total && s.total ? kGreen : p.foreground.Fade(0.7f), count);
        }
        ui.g.Text(Loc::Get(s.hint), { lifted.x + 20.f, ty + 38.f, lifted.w - 40.f, 40.f }, p.foreground.Fade(0.55f), hint);
        // Progress (the currencies have none)
        const float v = s.total ? static_cast<float>(n) / static_cast<float>(s.total) : 0.f;
        const Rect bar{ lifted.x + 20.f, lifted.B() - 22.f, lifted.w - 40.f, 6.f };
        if (s.value.empty()) ui.g.FillRound(bar, 3.f, p.foreground.Fade(0.08f));
        if (v > 0.f) ui.g.FillRound({ bar.x, bar.y, std::max(6.f, bar.w * v), bar.h }, 3.f, s.done == s.total ? kGreen : s.tone);

        if (it.hovered) ui.SetCursor(IDC_HAND);
        if (it.clicked) SwitchSection(ui, k);
    }
    const int lines = (kSections + cols - 1) / cols;
    scroll.SetContentSize(lines * (ch + kGap) - kGap + 8.f);
}

// ── Account picker (no save open) ───────────────────────────────────────────────────────────────

// The accounts with a save as cards in two columns (picture, name, id · files · last played, Open),
// and how it works on the right
void SaveEditorTab::Picker(UI::Ui& ui, const Rect& body) {
    const M::Palette& p = M::P();
    const Color accent = p.primaryMid.color;
    BackupState& b = main.Backups();
    constexpr float kHowW = 330.f;
    const Rect list{ body.x, body.y, body.w - kHowW - kGap, body.h };
    const Rect how{ list.R() + kGap, body.y, kHowW, body.h };
    const int count = static_cast<int>(b.Accounts().size());

    TextStyle heading = Style(12.f, DWRITE_FONT_WEIGHT_BOLD);
    heading.valign = VAlign::Center;
    // A Nintendo Switch save (a JKSV backup's "app"), on the right of the heading
    const std::wstring& switchText = Loc::Get("saveeditor_switch_button");
    const float switchW = TabKit::PillWidth(ui, switchText) + 10.f;
    const Rect switchR{ list.R() - switchW, list.y, switchW, 38.f };
    const UI::Id switchId = ui.MakeId("se-switch-open");
    if (TabKit::PillButton(ui, switchId, switchR, M::Icon::NintendoSwitch, switchText, Rgba(230, 0, 18), false, !busy)) OpenSwitchSave();
    ui.Tooltip(switchId, Loc::Get("saveeditor_switch_tip"));
    ui.g.Text(Loc::Get("saveeditor_pick_title"), { list.x + 4.f, list.y + 8.f, std::max(0.f, switchR.x - 12.f - list.x), 22.f }, p.foreground.Fade(0.7f), heading);
    const Rect cards{ list.x, list.y + 50.f, list.w, list.h - 50.f };
    if (!b.AccountsLoaded()) {
        BackupKit::EmptyState(ui, cards, M::Icon::Refresh, Loc::Get("status_loading_accounts"), L"");
    } else if (count == 0) {
        BackupKit::EmptyState(ui, cards, M::Icon::Account, Loc::Get("status_no_saves"), Loc::Get("saveeditor_no_accounts"));
    } else {
        UI::ScrollArea scroll(ui, ui.MakeId("se-accounts"), cards, false, 48.f, UI::ScrollArea::Bar::Thin);
        const float colW = (scroll.ContentWidth() - kGap) * 0.5f;
        const double reveal = BackupKit::ListReveal(ui, "se-accounts-reveal", b.Revision(), loadedAt + 0.1);
        TextStyle name = Style(16.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        name.valign = VAlign::Center;
        name.ellipsis = true;
        TextStyle meta = Style(11.5f);
        meta.valign = VAlign::Center;
        meta.ellipsis = true;
        for (int i = 0; i < count; ++i) {
            const auto& a = b.Accounts()[static_cast<size_t>(i)];
            const Rect card{ cards.x + (i % 2) * (colW + kGap), cards.y + (i / 2) * (kAccountH + kGap), colW, kAccountH };
            M::Motion::Enter enter(ui, M::Motion::Stagger(ui, reveal, i, 0.06f, 0.4f), 16.f);
            const UI::Id id = ui.MakeId("se-account", i);
            const auto it = ui.Interact(id, card);
            const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, it.held);
            const Rect lifted = card.Offset(0.f, -2.f * fb.hover);
            Card(ui, lifted, kRadius, accent, fb.hover);
            Avatar(ui, lifted.x + 46.f, lifted.CY(), 26.f + 2.f * fb.hover, BackupKit::SteamAvatar(a.accountId), M::Icon::Account, accent.Fade(0.35f + 0.5f * fb.hover));
            const float x = lifted.x + 88.f, w = lifted.R() - 70.f - x;
            ui.g.Text(AccountName(&a, a.accountId), { x, lifted.CY() - 22.f, w, 24.f }, p.foreground, name);
            ui.g.Text(L"ID " + a.accountId + L"   \x00B7   " + BackupKit::FilesText(a.fileCount), { x, lifted.CY() + 2.f, w, 18.f }, p.foreground.Fade(0.55f), meta);
            ui.g.Text(Time::Display(a.lastModified), { x, lifted.CY() + 19.f, w, 16.f }, p.foreground.Fade(0.4f), meta);
            // Open: a round button whose arrow nudges on hover
            const float bx = lifted.R() - 38.f;
            ui.g.FillEllipse(bx, lifted.CY(), 19.f, 19.f, Lerp(accent.Fade(0.14f), accent, fb.hover));
            M::DrawIcon(ui.g, M::Icon::ChevronRight, bx + 2.f * fb.hover, lifted.CY(), 22.f, Lerp(TabKit::Readable(accent), Colors::White, fb.hover));
            if (it.hovered) ui.SetCursor(IDC_HAND);
            if (it.clicked) Open(a.accountId, Files::Combine(a.path, L"app.sav"));
        }
        scroll.SetContentSize(((count + 1) / 2) * (kAccountH + kGap) - kGap);
    }

    M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.18, 0, 0.f, 0.45f), 0.f, 24.f);
    TabKit::StepsCard(ui, how, "se-how", Loc::Get("saveeditor_how_title"),
                      { Loc::Get("saveeditor_how_step1"), Loc::Get("saveeditor_how_step2"), Loc::Get("saveeditor_how_step3"),
                        Loc::Get("saveeditor_how_step4") },
                      loadedAt + 0.3);
}

} // namespace SonicRacingSaveManager::Main
