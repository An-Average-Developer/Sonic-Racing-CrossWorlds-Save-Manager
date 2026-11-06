#include <algorithm>
#include <cmath>
#include "Core/Text.h"
#include "Data/MusicCatalog.h"
#include "Localization/Localization.h"
#include "UI/Material/Motion.h"
#include "UI/Material/PackIcon.h"
#include "UI/Material/Theme.h"
#include "UI/Widgets/ScrollArea.h"
#include "Windows/Main/Tabs/Backup/BackupKit.h"
#include "Windows/Main/Tabs/SaveEditor/SaveEditorTab.h"
#include "Windows/Main/Tabs/TabKit.h"

// The Save Editor's music section: a strip with the jukebox's count and Unlock all, then the albums as
// their jackets on the left (the picked one lit) and the picked album on the right: its jacket and name,
// Unlock / Lock the album (with its tracks), and its tracks, each a switch.
namespace SonicRacingSaveManager::Main {

namespace M = UI::Material;
namespace SF = Services::SaveFile;
namespace MC = Data::MusicCatalog;
using namespace SaveEditorKit;
using TabKit::Style;

namespace {
constexpr Color kMusicTone = Rgba(20, 184, 166);   // the music: teal
constexpr float kStripH = 64.f, kAlbumW = 150.f, kTrackH = 52.f, kDetailMinW = 420.f;

const SF::ItemRow* FindRow(const std::vector<SF::ItemRow>& rows, const std::wstring& id) {
    for (const auto& r : rows)
        if (r.id == id) return &r;
    return nullptr;
}
} // namespace

void SaveEditorTab::MusicRows() {
    if (musicFor == version || !save) return;
    musicFor = version;
    musicAlbumRows = save->Rows(SF::ListId::Albums);
    musicTrackRows = save->Rows(SF::ListId::Tracks);
    // In the jukebox's order
    const auto order = [](const SF::ItemRow& r) {
        const auto* a = MC::FindAlbum(static_cast<int>(Text::ToInt(r.id).value_or(-1)));
        return a ? a->order * 10000 + a->id : 1 << 30;
    };
    std::stable_sort(musicAlbumRows.begin(), musicAlbumRows.end(), [&](const auto& a, const auto& b) { return order(a) < order(b); });
}

void SaveEditorTab::MusicView(UI::Ui& ui, const Rect& r) {
    const M::Palette& p = M::P();
    MusicRows();
    const double reveal = std::max(loadedAt, screenAt);
    if (musicAlbumRows.empty()) {
        BackupKit::EmptyState(ui, r, M::Icon::MusicNote, Loc::Get("saveeditor_empty_title"), Loc::Get("saveeditor_empty_message"));
        return;
    }
    if (musicAlbum < 0 || musicAlbum >= static_cast<int>(musicAlbumRows.size())) musicAlbum = 0;

    // ── The strip: what the jukebox has, and Unlock all ──
    int albumsDone = 0, tracksDone = 0;
    bool anyLeft = false;
    for (const auto& row : musicAlbumRows) {
        albumsDone += row.state == SF::ItemState::Unlocked;
        anyLeft |= row.state != SF::ItemState::Unlocked && row.canUnlock;
    }
    for (const auto& row : musicTrackRows) {
        tracksDone += row.state == SF::ItemState::Unlocked;
        anyLeft |= row.state != SF::ItemState::Unlocked && row.canUnlock;
    }
    {
        M::Motion::Enter enter(ui, M::Motion::Stagger(ui, reveal, 0, 0.f, 0.4f), 10.f);
        const Rect strip{ r.x, r.y, r.w, kStripH };
        Card(ui, strip, kRadius, kMusicTone, 0.f);
        ui.g.FillEllipse(strip.x + 36.f, strip.CY(), 20.f, 20.f, kMusicTone.Fade(0.2f));
        M::DrawIcon(ui.g, M::Icon::MusicNote, strip.x + 36.f, strip.CY(), 22.f, TabKit::Readable(kMusicTone));
        const std::wstring& unlockAll = Loc::Get("saveeditor_unlock_all_short");
        const float uw = TabKit::PillWidth(ui, unlockAll) + 10.f;
        const Rect ub{ strip.R() - 14.f - uw, strip.CY() - 19.f, uw, 38.f };
        if (TabKit::PillButton(ui, ui.MakeId("se-music-all"), ub, M::Icon::LockOpenVariant, unlockAll, kMusicTone, true, !busy && anyLeft)) {
            int changed = 0;
            Edit([&] { changed = save->UnlockAll(SF::ListId::Albums) + save->UnlockAll(SF::ListId::Tracks); });
            Status(Loc::Fmt("saveeditor_unlocked_n", std::to_wstring(changed)));
        }
        TextStyle title = Style(16.f, DWRITE_FONT_WEIGHT_BOLD);
        title.ellipsis = true;
        TextStyle sub = Style(12.f, DWRITE_FONT_WEIGHT_NORMAL);
        sub.ellipsis = true;
        const float tx = strip.x + 68.f, tw = ub.x - 16.f - tx;
        ui.g.Text(Loc::Get("saveeditor_section_music"), { tx, strip.y + 11.f, tw, 22.f }, p.foreground, title);
        ui.g.Text(Loc::Fmt("saveeditor_music_summary", std::to_wstring(albumsDone), std::to_wstring(musicAlbumRows.size()), std::to_wstring(tracksDone),
                           std::to_wstring(musicTrackRows.size())),
                  { tx, strip.y + 34.f, tw, 20.f }, p.foreground.Fade(0.6f), sub);
    }

    const float top = r.y + kStripH + kGap;
    const float detailW = std::max(kDetailMinW, r.w * 0.42f);
    const Rect grid{ r.x, top, r.w - detailW - kGap, r.B() - top };
    const Rect detail{ grid.R() + kGap, top, detailW, r.B() - top };

    // ── The albums ──
    {
        UI::ScrollArea scroll(ui, ui.MakeId("se-music-albums"), grid, false, 60.f, UI::ScrollArea::Bar::Thin);
        const float w = scroll.ContentWidth() - (scroll.BarShown() ? 10.f : 0.f);
        const int cols = std::max(1, static_cast<int>((w + kGap) / (kAlbumW + kGap)));
        const float cw = (w - (cols - 1) * kGap) / cols, ch = cw + 58.f;
        TextStyle name = Style(12.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        name.valign = VAlign::Center;
        name.ellipsis = true;
        TextStyle count = Style(11.f);
        count.valign = VAlign::Center;
        for (size_t k = 0; k < musicAlbumRows.size(); ++k) {
            const SF::ItemRow& row = musicAlbumRows[k];
            const Rect cell{ grid.x + (k % cols) * (cw + kGap), grid.y + (k / cols) * (ch + kGap), cw, ch };
            M::Motion::Enter enter(ui, M::Motion::Stagger(ui, reveal + 0.08, static_cast<int>(k), 0.02f, 0.35f), 14.f);
            const UI::Id id = ui.MakeId("se-album", static_cast<int64_t>(k));
            const auto it = ui.Interact(id, cell);
            const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, it.held);
            const bool picked = static_cast<int>(k) == musicAlbum;
            const bool on = row.state == SF::ItemState::Unlocked;
            const Rect lifted = cell.Offset(0.f, -3.f * fb.hover);
            Card(ui, lifted, kRadius, kMusicTone, picked ? 1.f : fb.hover);
            if (picked) ui.g.StrokeRound(lifted.Inset(1.f), kRadius - 1.f, kMusicTone, 2.f);
            // The jacket (greyed out while locked)
            const Rect jacket{ lifted.x + 10.f, lifted.y + 10.f, lifted.w - 20.f, lifted.w - 20.f };
            ui.g.PushRoundClip(jacket, 10.f);
            ui.g.FillRect(jacket, kMusicTone.Fade(0.15f));
            if (const ImagePtr image = Picture(SF::ListId::Albums, row.id))
                ui.g.DrawImage(*image, jacket.Inset(-2.f * fb.hover), on ? 1.f : 0.45f, Stretch::UniformToFill);
            else M::DrawIcon(ui.g, M::Icon::Album, jacket.CX(), jacket.CY(), 40.f, TabKit::Readable(kMusicTone).Fade(0.6f));
            ui.g.PopLayer();
            // Its state on the jacket's corner
            const Color state = row.dlc && !on ? kAmber : StateColor(row.state);
            ui.g.FillEllipse(jacket.R() - 4.f, jacket.y + 4.f, 12.f, 12.f, on ? kGreen : state);
            M::DrawIcon(ui.g, on ? M::Icon::Check : StateIcon(row.state), jacket.R() - 4.f, jacket.y + 4.f, 14.f, Colors::White);
            ui.g.Text(row.name, { lifted.x + 10.f, jacket.B() + 6.f, lifted.w - 20.f, 20.f }, on ? p.foreground : p.foreground.Fade(0.65f), name);
            // Its tracks you have
            const int aid = static_cast<int>(Text::ToInt(row.id).value_or(-1));
            int have = 0, total = 0;
            for (int t : MC::TracksOf(aid)) {
                ++total;
                if (const auto* tr = FindRow(musicTrackRows, std::to_wstring(t))) have += tr->state == SF::ItemState::Unlocked;
            }
            const std::wstring countText = row.dlc ? Loc::Get("savefile_dlc") + L"  \x00B7  " + Loc::Fmt("saveeditor_music_tracks_n", std::to_wstring(have), std::to_wstring(total))
                                                   : Loc::Fmt("saveeditor_music_tracks_n", std::to_wstring(have), std::to_wstring(total));
            ui.g.Text(countText, { lifted.x + 10.f, jacket.B() + 26.f, lifted.w - 20.f, 18.f }, row.dlc ? kAmber : p.foreground.Fade(0.5f), count);
            if (it.hovered && !picked) ui.SetCursor(IDC_HAND);
            if (it.clicked && !picked) {
                musicAlbum = static_cast<int>(k);
                UI::ScrollArea::ScrollTo(ui, ui.MakeId("se-music-tracks"), 0.f);
            }
        }
        const size_t lines = (musicAlbumRows.size() + cols - 1) / cols;
        scroll.SetContentSize(lines * (ch + kGap) - kGap + 6.f);
    }

    // ── The picked album ──
    const SF::ItemRow album = musicAlbumRows[static_cast<size_t>(musicAlbum)];
    const int aid = static_cast<int>(Text::ToInt(album.id).value_or(-1));
    const std::vector<int> trackIds = MC::TracksOf(aid);
    M::Motion::Enter enter(ui, M::Motion::Stagger(ui, reveal + 0.12, 0, 0.f, 0.35f), 0.f, 20.f);
    Card(ui, detail, kRadius, kMusicTone, 0.f);
    const bool albumOn = album.state == SF::ItemState::Unlocked;
    {
        const Rect jacket{ detail.x + 18.f, detail.y + 18.f, 112.f, 112.f };
        ui.g.PushRoundClip(jacket, 12.f);
        ui.g.FillRect(jacket, kMusicTone.Fade(0.15f));
        if (const ImagePtr image = Picture(SF::ListId::Albums, album.id)) ui.g.DrawImage(*image, jacket, 1.f, Stretch::UniformToFill);
        else M::DrawIcon(ui.g, M::Icon::Album, jacket.CX(), jacket.CY(), 40.f, TabKit::Readable(kMusicTone).Fade(0.6f));
        ui.g.PopLayer();
        const float tx = jacket.R() + 16.f, tw = detail.R() - 18.f - tx;
        TextStyle title = Style(18.f, DWRITE_FONT_WEIGHT_BOLD, true);
        title.ellipsis = true;
        ui.g.Text(album.name, { tx, jacket.y, tw, 48.f }, p.foreground, title);
        float cx = tx;
        cx += Chip(ui, cx, jacket.y + 62.f, Loc::Get(StateKey(album.state)), album.dlc && !albumOn ? kAmber : StateColor(album.state), false, true) + 8.f;
        if (album.dlc) Chip(ui, cx, jacket.y + 62.f, Loc::Get("savefile_dlc"), kAmber, true, false);
        // Unlock the album with its tracks / lock it
        if (CanToggle(album)) {
            const std::wstring& text = Loc::Get(albumOn ? "saveeditor_music_lock_album" : "saveeditor_music_unlock_album");
            const float bw = std::min(tw, TabKit::PillWidth(ui, text) + 10.f);
            if (TabKit::PillButton(ui, ui.MakeId("se-album-toggle"), { tx, jacket.B() - 36.f, bw, 36.f }, albumOn ? M::Icon::Lock : M::Icon::LockOpenVariant, text,
                                   albumOn ? kAmber : kMusicTone, !albumOn, !busy)) {
                const std::wstring id = album.id;
                int changed = 0;
                Edit([&] {
                    if (albumOn) changed += save->Lock(SF::ListId::Albums, id);
                    else {
                        changed += save->Unlock(SF::ListId::Albums, id);
                        for (int t : trackIds) changed += save->Unlock(SF::ListId::Tracks, std::to_wstring(t));
                    }
                });
                if (!albumOn) Status(Loc::Fmt("saveeditor_music_album_unlocked", album.name, std::to_wstring(changed)));
            }
        } else if (album.dlc) {
            TextStyle note = Style(11.5f, DWRITE_FONT_WEIGHT_NORMAL, true);
            ui.g.Text(Loc::Get("savefile_dlc_purchase"), { tx, jacket.B() - 36.f, tw, 36.f }, p.foreground.Fade(0.55f), note);
        }
    }

    // Its tracks
    const Rect list{ detail.x + 12.f, detail.y + 148.f, detail.w - 24.f, detail.h - 160.f };
    ui.g.FillRect({ detail.x + 18.f, list.y - 8.f, detail.w - 36.f, 1.f }, p.foreground.Fade(0.08f));
    if (trackIds.empty()) {
        TextStyle note = Style(12.f, DWRITE_FONT_WEIGHT_NORMAL, true, HAlign::Center);
        ui.g.Text(Loc::Get("saveeditor_music_no_tracks"), list.Inset(20.f, 20.f), p.foreground.Fade(0.5f), note);
        return;
    }
    UI::ScrollArea scroll(ui, ui.MakeId("se-music-tracks"), list, false, 50.f, UI::ScrollArea::Bar::Thin);
    const float w = scroll.ContentWidth() - (scroll.BarShown() ? 8.f : 0.f);
    TextStyle name = Style(13.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    name.ellipsis = true;
    TextStyle artist = Style(11.f);
    artist.ellipsis = true;
    float y = list.y;
    for (size_t k = 0; k < trackIds.size(); ++k) {
        const std::wstring tid = std::to_wstring(trackIds[k]);
        const SF::ItemRow* row = FindRow(musicTrackRows, tid);
        if (!row) continue;
        const Rect cell{ list.x, y, w, kTrackH };
        y += kTrackH + 6.f;
        const UI::Id id = ui.MakeId("se-track", trackIds[k]);
        const auto it = ui.Interact(id, cell);
        const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, it.held);
        const bool on = row->state == SF::ItemState::Unlocked;
        ui.g.FillRound(cell, 12.f, Lerp(p.foreground.Fade(0.03f), kMusicTone.Fade(0.14f), fb.hover));
        ui.g.FillEllipse(cell.x + 24.f, cell.CY(), 13.f, 13.f, on ? kGreen : StateColor(row->state).Fade(0.2f));
        M::DrawIcon(ui.g, on ? M::Icon::Check : StateIcon(row->state), cell.x + 24.f, cell.CY(), 15.f, on ? Colors::White : TabKit::Readable(StateColor(row->state)));
        const auto* t = MC::FindTrack(trackIds[k]);
        const float tx = cell.x + 46.f, tw = cell.R() - 12.f - tx;
        ui.g.Text(row->name, { tx, cell.y + 7.f, tw, 20.f }, on ? p.foreground : p.foreground.Fade(0.7f), name);
        if (t && *t->artist) ui.g.Text(t->artist, { tx, cell.y + 27.f, tw, 18.f }, p.foreground.Fade(0.5f), artist);
        if (CanToggle(*row)) {
            ui.Tooltip(id, ActionText(*row));
            if (it.hovered) ui.SetCursor(IDC_HAND);
            if (it.clicked && !busy) {
                Edit([&] {
                    if (on) save->Lock(SF::ListId::Tracks, tid);
                    else {
                        save->Unlock(SF::ListId::Tracks, tid);
                        // A track plays from its album: that opens too
                        if (!albumOn && album.canUnlock) save->Unlock(SF::ListId::Albums, album.id);
                    }
                });
            }
        }
    }
    scroll.SetContentSize(y - list.y);
}

} // namespace SonicRacingSaveManager::Main
