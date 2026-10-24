#include "Windows/Main/Tabs/Backup/BackupKit.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <vector>
#include "Core/Async.h"
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "Services/Backup/SteamAvatars.h"
#include "UI/Material/Motion.h"
#include "UI/Material/Theme.h"
#include "UI/Widgets/ScrollArea.h"
#include "Windows/Main/Tabs/TabBanner.h"
#include "Windows/Main/Tabs/TabKit.h"

namespace SonicRacingSaveManager::Main::BackupKit {

namespace {

namespace M = UI::Material;
using TabKit::Style;

struct RowLook {
    UI::Interaction it;
    float hover = 0.f, selected = 0.f, pulse = 0.f;
    Rect r;   // the row moved by its selection shift
};

// The shared frame of a row: hover tint, selection tint / glow / accent bar / shift, the pulse when
// it gets selected
RowLook RowFrame(UI::Ui& ui, UI::Id id, const Rect& r, bool selected) {
    const M::Palette& p = M::P();
    RowLook look;
    look.it = ui.Interact(id, r);
    if (look.it.hovered && !selected) ui.SetCursor(IDC_HAND);
    const M::Motion::Feedback fb = M::Motion::Hover(ui, id, look.it.hovered, look.it.held);
    look.hover = fb.hover;
    look.selected = ui.Tween(UI::HashId(id, 0x5E1), selected ? 1.f : 0.f, 0.2f);
    look.pulse = M::Motion::Pulse(ui, id, selected, 0.6f);
    const float shift = 5.f * Anim::EaseOutCubic(look.selected);
    look.r = { r.x + shift, r.y, r.w - shift, r.h };
    const Color accent = p.primaryMid.color;
    if (look.selected > 0.f) ui.g.GlowRound(look.r, 12.f, accent, 12.f, 0.35f * look.selected + 0.4f * look.pulse);
    ui.g.FillRound(look.r, 12.f, Lerp(Lerp(p.foreground.Fade(0.04f), p.foreground.Fade(0.08f), fb.hover), accent.Fade(0.2f), look.selected));
    if (look.selected > 0.f || fb.hover > 0.f)
        ui.g.StrokeRound(look.r.Inset(0.75f), 11.25f, accent.Fade(0.25f * fb.hover + 0.55f * look.selected), 1.5f);
    if (look.selected > 0.f) ui.g.FillRound({ look.r.x, look.r.y + 12.f, 4.f, look.r.h - 24.f }, 2.f, accent.Fade(look.selected));
    if (look.selected > 0.f && look.selected < 1.f) ui.KeepAnimating();
    return look;
}

// The round badge on the left of a row (the C#'s PackIcon in PrimaryHueMidBrush), or the account's
// Steam picture in a ring once it's loaded (fading in over the icon)
void Badge(UI::Ui& ui, const RowLook& look, M::Icon icon, const ImagePtr& picture = nullptr, UI::Id pictureId = 0) {
    const M::Palette& p = M::P();
    const float cx = look.r.x + 34.f, cy = look.r.CY();
    const float s = 1.f + 0.08f * look.hover + 0.15f * look.pulse;
    const float shown = picture ? ui.Tween(pictureId, 1.f, 0.3f, 0.f) : 0.f;
    if (shown < 1.f) {
        ui.g.FillEllipse(cx, cy, 20.f * s, 20.f * s, p.primaryMid.color.Fade(0.16f + 0.14f * look.selected));
        M::DrawIcon(ui.g, icon, cx, cy, 24.f * s, TabKit::Readable(p.primaryMid.color).Fade(1.f - shown));
    }
    if (shown > 0.f) {
        const float radius = 20.f * s;
        ui.g.DrawImageRounded(*picture, { cx - radius, cy - radius, radius * 2.f, radius * 2.f }, radius, shown, Stretch::UniformToFill);
        ui.g.StrokeEllipse(cx, cy, radius + 1.f, radius + 1.f, p.primaryMid.color.Fade((0.35f + 0.5f * look.selected) * shown), 2.f);
    }
}

// The count chip and the check on the right of a row; returns where the texts must stop
float Trailing(UI::Ui& ui, const RowLook& look, const std::wstring& chip) {
    const M::Palette& p = M::P();
    TextStyle t = Style(11.f, DWRITE_FONT_WEIGHT_SEMI_BOLD, false, HAlign::Center);
    t.valign = VAlign::Center;
    const float w = std::ceil(ui.g.Measure(chip, t).w) + 20.f;
    float x = look.r.R() - 16.f;
    if (look.selected > 0.f) {
        const float k = Anim::EaseOutBack(look.selected, 1.2f);
        M::DrawIcon(ui.g, M::Icon::CheckCircle, x - 11.f, look.r.CY(), 22.f * k, p.primaryMid.color);
        x -= 32.f * look.selected;
    }
    const Rect pill{ x - w, look.r.CY() - 12.f, w, 24.f };
    ui.g.FillRound(pill, 12.f, p.foreground.Fade(0.07f));
    ui.g.Text(chip, pill, p.foreground.Fade(0.75f), t);
    return pill.x - 12.f;
}

void Texts(UI::Ui& ui, const RowLook& look, float right, const std::wstring& title, const std::wstring& subtitle) {
    const M::Palette& p = M::P();
    const float x = look.r.x + 66.f, w = std::max(0.f, right - x);
    TextStyle name = Style(14.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    name.valign = VAlign::Center;
    name.ellipsis = true;
    TextStyle sub = Style(11.5f);
    sub.valign = VAlign::Center;
    sub.ellipsis = true;
    ui.g.Text(title, { x, look.r.y + 11.f, w, 22.f }, p.foreground, name);
    ui.g.Text(subtitle, { x, look.r.y + 33.f, w, 20.f }, p.foreground.Fade(0.6f), sub);
}

} // namespace

Columns Layout(const Rect& area) {
    const Rect grid = area.Inset(20.f);
    Columns c;
    c.banner = { grid.x, grid.y, grid.w, kTabBannerHeight };
    const float top = c.banner.B() + kGap;
    c.left = { grid.x, top, grid.w - kRightW - kGap, grid.B() - top };
    c.right = { c.left.R() + kGap, top, kRightW, grid.B() - top };
    return c;
}

float CardTitle(UI::Ui& ui, const Rect& r, const std::wstring& title, const std::wstring& pill) {
    const M::Palette& p = M::P();
    TextStyle caps = Style(11.f, DWRITE_FONT_WEIGHT_BOLD);
    caps.valign = VAlign::Center;
    ui.g.Text(title, { r.x, r.y, r.w, 20.f }, p.primaryMid.color, caps);
    if (!pill.empty()) {
        TextStyle t = Style(11.f, DWRITE_FONT_WEIGHT_SEMI_BOLD, false, HAlign::Center);
        t.valign = VAlign::Center;
        const float w = std::ceil(ui.g.Measure(pill, t).w) + 20.f;
        const Rect pr{ r.R() - w, r.y - 1.f, w, 22.f };
        ui.g.FillRound(pr, 11.f, p.primaryMid.color.Fade(0.14f));
        ui.g.Text(pill, pr, p.foreground.Fade(0.8f), t);
    }
    return 26.f;
}

float Hint(UI::Ui& ui, const Rect& r, const std::wstring& text, Color color) {
    const TextStyle t = Style(12.5f, DWRITE_FONT_WEIGHT_NORMAL, true);
    const float h = TabKit::TextH(ui, text, t, r.w);
    ui.g.Text(text, { r.x, r.y, r.w, h }, color.Transparent() ? M::P().foreground.Fade(0.65f) : color, t);
    return h;
}

bool AccountRow(UI::Ui& ui, UI::Id id, const Rect& r, const Models::SaveAccount& account, bool selected) {
    const RowLook look = RowFrame(ui, id, r, selected);
    Badge(ui, look, M::Icon::Account, SteamAvatar(account.accountId), UI::HashId(id, 0xA7A));
    const float right = Trailing(ui, look, FilesText(account.fileCount));
    // The id is the title already when there's no Steam name
    const std::wstring date = Time::Display(account.lastModified);
    Texts(ui, look, right, account.ShortDisplay(),
          account.accountName.empty() ? Loc::Fmt("backup_account_row_no_name", date) : Loc::Fmt("backup_account_row", account.accountId, date));
    return look.it.clicked;
}

bool BackupRow(UI::Ui& ui, UI::Id id, const Rect& r, const Models::BackupInfo& backup, const std::wstring& dateText, bool selected) {
    const RowLook look = RowFrame(ui, id, r, selected);
    // The account's picture once it's loaded, the save icon until then
    Badge(ui, look, M::Icon::ContentSaveAll, SteamAvatar(backup.accountId), UI::HashId(id, 0xA7A));
    const float right = Trailing(ui, look, FilesText(backup.fileCount));
    Texts(ui, look, right, backup.DisplayTitle(), Loc::Fmt("backup_backup_row", dateText, backup.AccountDisplay()));
    return look.it.clicked;
}

void List(UI::Ui& ui, UI::Id id, const Rect& r, int count, float rowH, double revealAt, const std::function<void(int, const Rect&)>& row) {
    double& shownAt = ui.Data<double>(UI::HashId(id, 0x11));
    if (shownAt != revealAt) {
        shownAt = revealAt;
        UI::ScrollArea::ScrollTo(ui, id, 0.f);
    }
    UI::ScrollArea scroll(ui, id, r, false, 48.f, UI::ScrollArea::Bar::Material);
    const float w = scroll.ContentWidth() - (scroll.BarShown() ? 6.f : 0.f);
    float y = r.y;
    for (int i = 0; i < count; ++i, y += rowH + kRowGap) {
        // Rows out of view are skipped (long backup lists)
        if (y + rowH < r.y + scroll.Offset() - 20.f || y > r.B() + scroll.Offset() + 20.f) continue;
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, revealAt, i, 0.04f, 0.35f), 0.f, -20.f);
        row(i, { r.x, y, w - 6.f, rowH });
    }
    scroll.SetContentSize(count > 0 ? count * (rowH + kRowGap) - kRowGap : 0.f);
}

double ListReveal(UI::Ui& ui, const char* key, unsigned revision, double base) {
    struct Seen {
        unsigned revision = ~0u;
        double at = -10.0;
    };
    Seen& seen = ui.Data<Seen>(ui.MakeId(key));
    if (seen.revision != revision) {
        // The first load plays with the page; later reloads replay the rows
        seen.at = seen.revision == ~0u ? -10.0 : ui.Time();
        seen.revision = revision;
    }
    return std::max(base, seen.at);
}

void EmptyState(UI::Ui& ui, const Rect& r, M::Icon icon, const std::wstring& title, const std::wstring& message) {
    const M::Palette& p = M::P();
    const float t = static_cast<float>(ui.Time());
    const float cy = r.CY() - 30.f;
    ui.g.FillEllipse(r.CX(), cy, 44.f, 44.f, p.primaryMid.color.Fade(0.08f));
    M::DrawIcon(ui.g, icon, r.CX(), cy + 3.f * std::sin(t * 2.f), 48.f, p.foreground.Fade(0.3f));
    ui.KeepAnimatingThrottled();
    TextStyle head = Style(18.f, DWRITE_FONT_WEIGHT_MEDIUM, false, HAlign::Center);
    ui.g.Text(title, { r.x, cy + 58.f, r.w, 26.f }, p.foreground.Fade(0.55f), head);
    TextStyle body = Style(13.f, DWRITE_FONT_WEIGHT_NORMAL, true, HAlign::Center);
    const float bh = TabKit::TextH(ui, message, body, r.w - 60.f);
    ui.g.Text(message, { r.x + 30.f, cy + 90.f, r.w - 60.f, bh }, p.foreground.Fade(0.45f), body);
}

void InfoChip(UI::Ui& ui, const Rect& r, M::Icon icon, const std::wstring& label, const std::wstring& value) {
    const M::Palette& p = M::P();
    ui.g.FillRound(r, 12.f, p.foreground.Fade(0.05f));
    M::DrawIcon(ui.g, icon, r.x + 20.f, r.CY(), 18.f, p.primaryMid.color);
    TextStyle tiny = Style(10.f);
    tiny.valign = VAlign::Center;
    TextStyle big = Style(13.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    big.valign = VAlign::Center;
    big.ellipsis = true;
    ui.g.Text(label, { r.x + 38.f, r.y + 6.f, r.w - 44.f, 16.f }, p.foreground.Fade(0.55f), tiny);
    ui.g.Text(value, { r.x + 38.f, r.y + 20.f, r.w - 44.f, r.h - 26.f }, p.foreground, big);
}

std::wstring FilesText(int count) { return Loc::Fmt(count == 1 ? "files_count_one" : "files_count_many", count); }

ImagePtr SteamAvatar(const std::wstring& accountId) {
    struct Slot {
        ImagePtr image;
        bool requested = false;
    };
    static std::map<std::wstring, Slot> avatars;   // UI thread only
    Slot& slot = avatars[accountId];
    if (!slot.requested) {
        slot.requested = true;
        struct Decoded {
            unsigned width = 0, height = 0;
            std::vector<uint32_t> pixels;
            bool ok = false;
        };
        Async::Run(
            [accountId] {
                Decoded d;
                const std::string bytes = Services::SteamAvatars::Fetch(accountId);
                d.ok = !bytes.empty() && Images::DecodeMemory(bytes, 184, d.width, d.height, d.pixels);
                return d;
            },
            [accountId](Decoded d) {
                if (d.ok) avatars[accountId].image = Images::FromPixels(d.width, d.height, d.pixels.data());
            });
    }
    return slot.image;
}

} // namespace SonicRacingSaveManager::Main::BackupKit
