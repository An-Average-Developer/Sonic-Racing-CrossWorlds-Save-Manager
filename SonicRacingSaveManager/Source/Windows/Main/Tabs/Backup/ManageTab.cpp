#include <cmath>
#include "Localization/Localization.h"
#include "UI/Material/Card.h"
#include "UI/Material/Motion.h"
#include "UI/Material/Theme.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Tabs/Backup/BackupKit.h"
#include "Windows/Main/Tabs/Backup/BackupState.h"
#include "Windows/Main/Tabs/Backup/BackupTabs.h"
#include "Windows/Main/Tabs/TabBanner.h"
#include "Windows/Main/Tabs/TabKit.h"

// The "Manage" tab of MainWindow.xaml, redesigned: the banner over two columns, the backups (Date
// Created = the full date) on the left; Export / Import / Delete / Open Folder / Refresh, the
// selected backup and a sharing tip on the right.
namespace SonicRacingSaveManager::Main {

namespace {
namespace M = UI::Material;
using namespace BackupKit;
const Color kRed = Argb(0xFFD32F2F);
} // namespace

void ManageTab::Draw(UI::Ui& ui, const Rect& area) {
    BackupState& b = main.Backups();
    if (loadedAt < 0.0) loadedAt = ui.Time();
    const Columns cols = Layout(area);
    const int count = static_cast<int>(b.Backups().size());
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt, 0, 0.f, 0.5f), 14.f);
        TabBannerSpec spec;
        spec.icon = M::Icon::FolderCog;
        spec.motion = TabBannerSpec::Motion::Wobble;
        spec.title = Loc::Get("manage_title");
        spec.subtitle = Loc::Get("manage_subtitle");
        spec.pill = Loc::Fmt(count == 1 ? "backups_count_one" : "backups_count_many", count);
        spec.pillIcon = M::Icon::ContentSaveAll;
        TabBanner(ui, cols.banner, spec, loadedAt);
    }

    // ── Left: the backups (Date Created = DisplayDate) ──
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.1, 0, 0.f, 0.5f), 18.f);
        M::CardLook look;
        look.radius = kRadius;
        M::Card card(ui, ui.MakeId("mg-card"), cols.left, Edges(24.f, 20.f), false, 1.f, look);
        const Rect c = card.content;
        float y = c.y + CardTitle(ui, c, Loc::Get("manage_backups"));
        y += Hint(ui, { c.x, y, c.w, 0.f }, Loc::Get("manage_hint")) + 14.f;
        const Rect list{ c.x, y, c.w, c.B() - y };
        if (!b.BackupsLoaded()) {
            EmptyState(ui, list, M::Icon::Refresh, Loc::Get("status_loading_backups"), L"");
        } else if (count == 0) {
            EmptyState(ui, list, M::Icon::ContentSaveAll, Loc::Get("status_no_backups"), Loc::Get("manage_no_backups_message"));
        } else {
            List(ui, ui.MakeId("mg-backups"), list, count, kBackupRowH, ListReveal(ui, "mg-reveal", b.Revision(), loadedAt + 0.2),
                 [&](int i, const Rect& r) {
                     const auto& backup = b.Backups()[static_cast<size_t>(i)];
                     if (BackupRow(ui, ui.MakeId("mg-backup", i), r, backup, backup.DisplayDate(), backup.name == b.selectedManageBackupName))
                         b.selectedManageBackupName = backup.name;
                 });
        }
    }

    // ── Right: actions, the selected backup, the tip ──
    constexpr float kPad = 20.f, kButtonH = 44.f;
    const float actionsH = kPad * 2.f + 26.f + (kButtonH + 10.f) * 3.f + 40.f;
    const Rect actions{ cols.right.x, cols.right.y, cols.right.w, actionsH };
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.18, 0, 0.f, 0.5f), 0.f, 28.f);
        M::CardLook look;
        look.radius = kRadius;
        M::Card card(ui, ui.MakeId("mg-actions"), actions, Edges(kPad), false, 1.f, look);
        const Rect c = card.content;
        float y = c.y + CardTitle(ui, c, Loc::Get("manage_actions"));
        const Color primary = M::P().primaryMid.color;
        const float half = (c.w - 10.f) * 0.5f;
        // Export / Import (raised), Delete (Background #D32F2F), Open Folder / Refresh (outlined)
        if (TabKit::PillButton(ui, ui.MakeId("mg-export"), { c.x, y, half, kButtonH }, M::Icon::Export, Loc::Get("manage_export"), primary, true,
                               b.CanManage()))
            b.ExportBackup();
        if (TabKit::PillButton(ui, ui.MakeId("mg-import"), { c.x + half + 10.f, y, half, kButtonH }, M::Icon::Import, Loc::Get("manage_import"),
                               primary, true, !b.Busy()))
            b.ImportBackup();
        y += kButtonH + 10.f;
        if (TabKit::PillButton(ui, ui.MakeId("mg-delete"), { c.x, y, c.w, kButtonH }, M::Icon::Delete, Loc::Get("manage_delete"), kRed, true,
                               b.CanManage()))
            b.DeleteBackup();
        y += kButtonH + 10.f;
        if (TabKit::PillButton(ui, ui.MakeId("mg-folder"), { c.x, y, c.w, kButtonH }, M::Icon::FolderOpen, Loc::Get("manage_open_folder"), primary,
                               false))
            b.OpenBackupFolder();
        y += kButtonH + 10.f;
        const std::wstring& refresh = Loc::Get("refresh");
        const float w = TabKit::PillWidth(ui, refresh);
        const float angle = b.Busy() ? std::fmod(static_cast<float>(ui.Time()) * 360.f, 360.f) : 0.f;
        if (TabKit::PillButton(ui, ui.MakeId("mg-refresh"), { c.CX() - w * 0.5f, y, w, 40.f }, M::Icon::Refresh, refresh, primary, false, !b.Busy(),
                               angle))
            b.RefreshBackups();
    }
    float y = actions.B() + kGap;
    const float backupH = BackupCard(ui, "mg-backup-card", cols.right, nullptr, loadedAt, false);
    if (cols.right.B() - y >= backupH) {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.26, 0, 0.f, 0.5f), 0.f, 28.f);
        BackupCard(ui, "mg-backup-card", { cols.right.x, y, cols.right.w, backupH }, b.SelectedManageBackup(), loadedAt, true);
        y += backupH + kGap;
    }
    const std::vector<std::wstring> tips = { Loc::Get("manage_tip1"), Loc::Get("manage_tip2") };
    const float tipH = TabKit::StepsCard(ui, { cols.right.x, y, cols.right.w, 0.f }, "mg-tip", Loc::Get("manage_tip_title"), tips, 0.0, false);
    if (cols.right.B() - y >= tipH) {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.34, 0, 0.f, 0.5f), 0.f, 28.f);
        TabKit::StepsCard(ui, { cols.right.x, y, cols.right.w, cols.right.B() - y }, "mg-tip", Loc::Get("manage_tip_title"), tips, loadedAt + 0.45);
    }
}

} // namespace SonicRacingSaveManager::Main
