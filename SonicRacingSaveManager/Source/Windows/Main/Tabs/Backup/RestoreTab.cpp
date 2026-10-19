#include <algorithm>
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

// The "Restore" tab of MainWindow.xaml, redesigned: the banner over two columns, the backups (the
// C#'s DataGrid with Backup Name / Date / Account ID / Files, now cards) on the left; the "Restore
// to" account, Restore (Original) / Restore (Selected) / Refresh, and the selected backup on the
// right.
namespace SonicRacingSaveManager::Main {

namespace {
namespace M = UI::Material;
using namespace BackupKit;
const Color kOrange = Argb(0xFFFF6B00);
} // namespace

void RestoreTab::Draw(UI::Ui& ui, const Rect& area) {
    BackupState& b = main.Backups();
    if (loadedAt < 0.0) loadedAt = ui.Time();
    const Columns cols = Layout(area);
    const int count = static_cast<int>(b.Backups().size());
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt, 0, 0.f, 0.5f), 14.f);
        TabBannerSpec spec;
        spec.icon = M::Icon::Restore;
        spec.motion = TabBannerSpec::Motion::SpinBack;
        spec.title = Loc::Get("restore_title");
        spec.subtitle = Loc::Get("restore_subtitle");
        spec.pill = Loc::Fmt(count == 1 ? "backups_count_one" : "backups_count_many", count);
        spec.pillIcon = M::Icon::ContentSaveAll;
        TabBanner(ui, cols.banner, spec, loadedAt);
    }

    // ── Left: the backups (Date = TimeAgo) ──
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.1, 0, 0.f, 0.5f), 18.f);
        M::CardLook look;
        look.radius = kRadius;
        M::Card card(ui, ui.MakeId("rs-card"), cols.left, Edges(24.f, 20.f), false, 1.f, look);
        const Rect c = card.content;
        float y = c.y + CardTitle(ui, c, Loc::Get("restore_backups"));
        // The orange warning (Foreground="#FF6B00")
        M::DrawIcon(ui.g, M::Icon::AlertOutline, c.x + 9.f, y + 9.f, 18.f, kOrange);
        y += Hint(ui, { c.x + 26.f, y, c.w - 26.f, 0.f }, Loc::Get("restore_warning"), TabKit::Readable(kOrange)) + 14.f;
        const Rect list{ c.x, y, c.w, c.B() - y };
        if (!b.BackupsLoaded()) {
            EmptyState(ui, list, M::Icon::Refresh, Loc::Get("status_loading_backups"), L"");
        } else if (count == 0) {
            EmptyState(ui, list, M::Icon::ContentSaveAll, Loc::Get("status_no_backups"), Loc::Get("restore_no_backups_message"));
        } else {
            List(ui, ui.MakeId("rs-backups"), list, count, kBackupRowH, ListReveal(ui, "rs-reveal", b.Revision(), loadedAt + 0.2),
                 [&](int i, const Rect& r) {
                     const auto& backup = b.Backups()[static_cast<size_t>(i)];
                     if (BackupRow(ui, ui.MakeId("rs-backup", i), r, backup, backup.TimeAgo(), backup.name == b.selectedBackupName))
                         b.selectedBackupName = backup.name;
                 });
        }
    }

    // ── Right: restore to, the actions, the selected backup ──
    constexpr float kPad = 20.f, kButtonH = 44.f;
    const int accounts = static_cast<int>(b.Accounts().size());
    const float listH = std::min(3, std::max(1, accounts)) * (kAccountRowH + kRowGap) - kRowGap;
    const float actionsH = kPad * 2.f + 26.f + 22.f + listH + 16.f + (kButtonH + 10.f) * 2.f + 40.f;
    const Rect actions{ cols.right.x, cols.right.y, cols.right.w, actionsH };
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.18, 0, 0.f, 0.5f), 0.f, 28.f);
        M::CardLook look;
        look.radius = kRadius;
        M::Card card(ui, ui.MakeId("rs-actions"), actions, Edges(kPad), false, 1.f, look);
        const Rect c = card.content;
        float y = c.y + CardTitle(ui, c, Loc::Get("restore_to"));
        y += Hint(ui, { c.x, y, c.w, 0.f }, Loc::Get("restore_to_hint")) + 8.f;
        const Rect list{ c.x, y, c.w, listH };
        if (accounts == 0) {
            TextStyle t = TabKit::Style(12.f, DWRITE_FONT_WEIGHT_NORMAL, false, HAlign::Center);
            t.valign = VAlign::Center;
            ui.g.Text(Loc::Get("status_no_saves"), list, M::P().foreground.Fade(0.5f), t);
        } else {
            List(ui, ui.MakeId("rs-accounts"), list, accounts, kAccountRowH, ListReveal(ui, "rs-acc-reveal", b.Revision(), loadedAt + 0.3),
                 [&](int i, const Rect& r) {
                     const auto& account = b.Accounts()[static_cast<size_t>(i)];
                     if (AccountRow(ui, ui.MakeId("rs-account", i), r, account, account.accountId == b.selectedRestoreAccountId))
                         b.selectedRestoreAccountId = account.accountId;
                 });
        }
        y = list.B() + 16.f;
        // Restore (Original) / Restore (Selected, Background #FF6B00): CanExecute = a backup is selected
        if (TabKit::PillButton(ui, ui.MakeId("rs-original"), { c.x, y, c.w, kButtonH }, M::Icon::RestoreAlert, Loc::Get("restore_original"),
                               M::P().primaryMid.color, true, b.CanRestore()))
            b.RestoreBackup();
        y += kButtonH + 10.f;
        if (TabKit::PillButton(ui, ui.MakeId("rs-selected"), { c.x, y, c.w, kButtonH }, M::Icon::AccountSwitch, Loc::Get("restore_selected"), kOrange,
                               true, b.CanRestore()))
            b.RestoreBackupToCustom();
        y += kButtonH + 10.f;
        const std::wstring& refresh = Loc::Get("refresh");
        const float angle = b.Busy() ? std::fmod(static_cast<float>(ui.Time()) * 360.f, 360.f) : 0.f;
        if (TabKit::PillButton(ui, ui.MakeId("rs-refresh"), { c.CX() - TabKit::PillWidth(ui, refresh) * 0.5f, y, TabKit::PillWidth(ui, refresh), 40.f },
                               M::Icon::Refresh, refresh, M::P().primaryMid.color, false, !b.Busy(), angle))
            b.RefreshBackups();
    }
    const float y = actions.B() + kGap;
    const float backupH = BackupCard(ui, "rs-backup-card", cols.right, nullptr, loadedAt, false);
    if (cols.right.B() - y >= backupH) {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.26, 0, 0.f, 0.5f), 0.f, 28.f);
        BackupCard(ui, "rs-backup-card", { cols.right.x, y, cols.right.w, cols.right.B() - y }, b.SelectedBackup(), loadedAt, true);
    }
}

} // namespace SonicRacingSaveManager::Main
