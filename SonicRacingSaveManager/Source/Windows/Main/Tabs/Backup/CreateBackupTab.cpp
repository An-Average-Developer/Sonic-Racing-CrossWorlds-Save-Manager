#include <cmath>
#include "Localization/Localization.h"
#include "UI/Material/Card.h"
#include "UI/Material/Fields.h"
#include "UI/Material/Motion.h"
#include "UI/Material/Theme.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Tabs/Backup/BackupKit.h"
#include "Windows/Main/Tabs/Backup/BackupState.h"
#include "Windows/Main/Tabs/Backup/BackupTabs.h"
#include "Windows/Main/Tabs/TabBanner.h"
#include "Windows/Main/Tabs/TabKit.h"

// The "Create Backup" tab of MainWindow.xaml, redesigned: the banner over two columns, the accounts
// (the C#'s ComboBox items, now cards) with Refresh Accounts, the optional backup name and Create
// Backup on the left; the selected account and how backups work on the right.
namespace SonicRacingSaveManager::Main {

namespace {
namespace M = UI::Material;
using namespace BackupKit;
} // namespace

void CreateBackupTab::Draw(UI::Ui& ui, const Rect& area) {
    BackupState& b = main.Backups();
    if (loadedAt < 0.0) loadedAt = ui.Time();
    const Columns cols = Layout(area);
    const int count = static_cast<int>(b.Accounts().size());
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt, 0, 0.f, 0.5f), 14.f);
        TabBannerSpec spec;
        spec.icon = M::Icon::BackupRestore;
        spec.motion = TabBannerSpec::Motion::SpinBack;
        spec.title = Loc::Get("create_title");
        spec.subtitle = Loc::Get("create_subtitle");
        spec.pill = Loc::Fmt(count == 1 ? "accounts_count_one" : "accounts_count_many", count);
        spec.pillIcon = M::Icon::Account;
        TabBanner(ui, cols.banner, spec, loadedAt);
    }

    // ── Left: accounts, name, Create Backup ──
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.1, 0, 0.f, 0.5f), 18.f);
        M::CardLook look;
        look.radius = kRadius;
        M::Card card(ui, ui.MakeId("cb-card"), cols.left, Edges(24.f, 20.f), false, 1.f, look);
        const Rect c = card.content;
        float y = c.y + CardTitle(ui, c, Loc::Get("create_select_account"));
        y += Hint(ui, { c.x, y, c.w, 0.f }, Loc::Get("create_select_hint")) + 14.f;

        M::FieldStyle field;
        field.outlined = true;
        field.fontSize = 14.f;
        field.hint = Loc::Get("create_name_hint");
        const float fieldH = M::FieldHeight(ui, field);
        constexpr float kButtonH = 50.f, kRefreshH = 40.f;
        const float bottom = c.B() - kButtonH - 14.f - fieldH - 18.f - kRefreshH - 14.f;
        const Rect list{ c.x, y, c.w, bottom - y };

        if (!b.AccountsLoaded()) {
            EmptyState(ui, list, M::Icon::Refresh, Loc::Get("status_loading_accounts"), L"");
        } else if (count == 0) {
            EmptyState(ui, list, M::Icon::Account, Loc::Get("status_no_saves"), Loc::Get("create_no_accounts_message"));
        } else {
            List(ui, ui.MakeId("cb-accounts"), list, count, kAccountRowH, ListReveal(ui, "cb-accounts-reveal", b.Revision(), loadedAt + 0.2), [&](int i, const Rect& r) {
                const auto& account = b.Accounts()[static_cast<size_t>(i)];
                if (AccountRow(ui, ui.MakeId("cb-account", i), r, account, account.accountId == b.selectedAccountId))
                    b.selectedAccountId = account.accountId;
            });
        }

        float by = list.B() + 14.f;
        // Refresh Accounts (outlined), the icon turning while loading
        {
            const std::wstring& label = Loc::Get("create_refresh_accounts");
            const float angle = b.Busy() ? std::fmod(static_cast<float>(ui.Time()) * 360.f, 360.f) : 0.f;
            if (b.Busy()) ui.KeepAnimating();
            if (TabKit::PillButton(ui, ui.MakeId("cb-refresh"), { c.x, by, TabKit::PillWidth(ui, label), kRefreshH }, M::Icon::Refresh, label,
                                   M::P().primaryMid.color, false, !b.Busy(), angle))
                b.RefreshAccounts();
        }
        by += kRefreshH + 18.f;
        M::TextField(ui, ui.MakeId("cb-name"), { c.x, by, c.w, fieldH }, b.backupName, field);
        by += fieldH + 14.f;
        if (TabKit::PillButton(ui, ui.MakeId("cb-create"), { c.x, by, c.w, kButtonH }, M::Icon::ContentSaveAll, Loc::Get("create_button"),
                               M::P().primaryMid.color, true, b.CanCreateBackup()))
            b.CreateBackup();
    }

    // ── Right: the selected account, how it works ──
    const float accountH = AccountCard(ui, cols.right, nullptr, loadedAt, {}, false);
    const Rect accountR{ cols.right.x, cols.right.y, cols.right.w, accountH };
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.18, 0, 0.f, 0.5f), 0.f, 28.f);
        AccountCard(ui, accountR, b.SelectedAccount(), loadedAt, [&b] { b.OpenSaveFolder(); }, true);
    }
    const float y = accountR.B() + kGap;
    if (cols.right.B() - y > 120.f) {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, loadedAt + 0.26, 0, 0.f, 0.5f), 0.f, 28.f);
        TabKit::StepsCard(ui, { cols.right.x, y, cols.right.w, cols.right.B() - y }, "cb-how", Loc::Get("create_how_title"),
                          { Loc::Get("create_how_step1"), Loc::Get("create_how_step2"), Loc::Get("create_how_step3") }, loadedAt + 0.4);
    }
}

} // namespace SonicRacingSaveManager::Main
