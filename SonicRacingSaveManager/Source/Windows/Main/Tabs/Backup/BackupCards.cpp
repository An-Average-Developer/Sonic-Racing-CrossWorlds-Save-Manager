#include <cmath>
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "UI/Material/Card.h"
#include "UI/Material/Motion.h"
#include "UI/Material/Theme.h"
#include "Windows/Main/Tabs/Backup/BackupKit.h"
#include "Windows/Main/Tabs/TabKit.h"

// The right column's cards of the backup tabs: the selected account (Create Backup) and the
// selected backup (Restore, Manage). The content pops in again whenever the selection changes.
namespace SonicRacingSaveManager::Main::BackupKit {

namespace {

namespace M = UI::Material;
using TabKit::Style;

constexpr float kPad = 20.f, kChipH = 52.f, kChipGap = 10.f;

// Entrance of the card's content: replays when `key` (the selection) changes
float SelectionPop(UI::Ui& ui, const char* id, const std::wstring& key) {
    struct Shown {
        std::wstring key;
        double at = -10.0;
    };
    Shown& shown = ui.Data<Shown>(ui.MakeId(id));
    if (shown.key != key) {
        shown.key = key;
        shown.at = ui.Time();
    }
    const float k = Anim::Clamp01(static_cast<float>((ui.Time() - shown.at) / 0.4));
    if (k < 1.f) ui.KeepAnimating();
    return Anim::EaseOutCubic(k);
}

// Big round avatar with the icon, the title and a caption under it, centered
float Hero(UI::Ui& ui, const Rect& c, M::Icon icon, const std::wstring& title, const std::wstring& caption, float pop) {
    const M::Palette& p = M::P();
    const float t = static_cast<float>(ui.Time());
    const float cx = c.CX(), cy = c.y + 44.f;
    const float ring = static_cast<float>(std::fmod(ui.Time(), 2.4) / 2.4);
    ui.g.StrokeEllipse(cx, cy, 34.f + 14.f * ring, 34.f + 14.f * ring, p.primaryMid.color.Fade(0.35f * (1.f - ring)), 1.5f);
    ui.KeepAnimatingThrottled();
    const float s = Anim::EaseOutBack(pop, 1.4f);
    ui.g.GlowRound({ cx - 34.f, cy - 34.f, 68.f, 68.f }, 34.f, p.primaryMid.color, 16.f, 0.35f);
    ui.g.FillRadial({ cx - 34.f, cy - 34.f, 68.f, 68.f }, { { 0.f, Lerp(p.primaryMid.color, Colors::White, 0.3f) }, { 1.f, p.primaryDark.color } });
    M::DrawIcon(ui.g, icon, cx, cy + 2.f * std::sin(t * 2.f), 34.f * s, p.primaryMid.foreground);
    M::Motion::Enter enter(ui, pop, 8.f);
    TextStyle name = Style(16.f, DWRITE_FONT_WEIGHT_BOLD, false, HAlign::Center);
    name.valign = VAlign::Center;
    name.ellipsis = true;
    ui.g.Text(title, { c.x, cy + 44.f, c.w, 24.f }, p.foreground, name);
    TextStyle sub = Style(12.f, DWRITE_FONT_WEIGHT_NORMAL, false, HAlign::Center);
    sub.valign = VAlign::Center;
    sub.ellipsis = true;
    ui.g.Text(caption, { c.x, cy + 68.f, c.w, 18.f }, p.foreground.Fade(0.6f), sub);
    return 44.f + 68.f + 18.f + 16.f;
}

} // namespace

float AccountCard(UI::Ui& ui, const Rect& r, const Models::SaveAccount* account, double revealAt, const std::function<void()>& openFolder, bool draw) {
    const float h = kPad * 2.f + 26.f + 130.f + (kChipH + kChipGap) * 2.f + 42.f;
    if (!draw) return h;
    (void)revealAt;
    M::CardLook look;
    look.radius = kRadius;
    M::Card card(ui, ui.MakeId("bk-account-card"), r, Edges(kPad), false, 1.f, look);
    const Rect c = card.content;
    CardTitle(ui, c, Loc::Get("backup_selected_account"));
    const Rect body{ c.x, c.y + 26.f, c.w, c.h - 26.f };
    if (!account) {
        EmptyState(ui, body, M::Icon::Account, Loc::Get("backup_no_account_title"), Loc::Get("backup_no_account_message"));
        return h;
    }
    const float pop = SelectionPop(ui, "bk-account-pop", account->accountId);
    float y = body.y + Hero(ui, body, M::Icon::AccountCircle, account->accountName.empty() ? account->accountId : account->accountName,
                            Loc::Fmt("backup_account_id", account->accountId), pop);
    {
        M::Motion::Enter enter(ui, pop, 10.f);
        InfoChip(ui, { c.x, y, c.w, kChipH }, M::Icon::FileMultiple, Loc::Get("backup_save_files"), FilesText(account->fileCount));
        y += kChipH + kChipGap;
        InfoChip(ui, { c.x, y, c.w, kChipH }, M::Icon::ClockOutline, Loc::Get("backup_last_played"), Time::Display(account->lastModified));
        y += kChipH + kChipGap;
    }
    if (TabKit::PillButton(ui, ui.MakeId("bk-open-saves"), { c.x, c.B() - 40.f, c.w, 40.f }, M::Icon::FolderOpen, Loc::Get("header_save_folder"),
                           M::P().primaryMid.color, false))
        openFolder();
    return h;
}

float BackupCard(UI::Ui& ui, const char* key, const Rect& r, const Models::BackupInfo* backup, double revealAt, bool draw) {
    const float h = kPad * 2.f + 26.f + 130.f + (kChipH + kChipGap) * 2.f - kChipGap;
    if (!draw) return h;
    (void)revealAt;
    M::CardLook look;
    look.radius = kRadius;
    M::Card card(ui, ui.MakeId(key), r, Edges(kPad), false, 1.f, look);
    const Rect c = card.content;
    CardTitle(ui, c, Loc::Get("backup_selected_backup"));
    const Rect body{ c.x, c.y + 26.f, c.w, c.h - 26.f };
    if (!backup) {
        EmptyState(ui, body, M::Icon::ContentSaveAll, Loc::Get("backup_no_selection_title"), Loc::Get("backup_no_selection_message"));
        return h;
    }
    const float pop = SelectionPop(ui, (std::string(key) + "-pop").c_str(), backup->name);
    float y = body.y + Hero(ui, body, M::Icon::ContentSaveAll, backup->name, backup->DisplayDate(), pop);
    M::Motion::Enter enter(ui, pop, 10.f);
    const float half = (c.w - kChipGap) * 0.5f;
    InfoChip(ui, { c.x, y, half, kChipH }, M::Icon::Account, Loc::Get("backup_account"), backup->accountId);
    InfoChip(ui, { c.x + half + kChipGap, y, half, kChipH }, M::Icon::FileMultiple, Loc::Get("backup_files"), FilesText(backup->fileCount));
    y += kChipH + kChipGap;
    InfoChip(ui, { c.x, y, c.w, kChipH }, M::Icon::ClockOutline, Loc::Get("backup_created"), backup->TimeAgo());
    return h;
}

} // namespace SonicRacingSaveManager::Main::BackupKit
