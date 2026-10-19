#include "Windows/Main/Tabs/Mods/ModUpdateOverlay.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "Core/Async.h"
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "Services/Mods/GameBananaApi.h"
#include "UI/Material/Motion.h"
#include "UI/Material/PackIcon.h"
#include "UI/Material/Theme.h"
#include "Windows/Main/Tabs/Mods/FileSelectionOverlay.h"
#include "Windows/Main/Tabs/TabKit.h"

namespace SonicRacingSaveManager::Main {

namespace {

namespace M = UI::Material;
namespace Mods = Services::Mods;
using TabKit::Style;

constexpr float kHeaderH = 84.f, kVersionsH = 72.f, kBodyH = 150.f, kFooterH = 76.f;
const Color kGreen = Argb(0xFF4CAF50), kOrange = Argb(0xFFFF6B00);

} // namespace

ModUpdateOverlay::ModUpdateOverlay(MainWindow& window, FileSelectionOverlay& picker) : CardOverlay(window), files(picker) { SetCover(&files); }

void ModUpdateOverlay::Show(const Models::ModInfo& info, const Mods::ModManager& manager) {
    mod = info;
    service = manager;
    statusMessage = Loc::Get("mod_update_click_to_begin");
    statusError = false;
    isDownloading = fetching = false;
    progressPercentage = 0.0;
    progressText.clear();
    Open();
}

void ModUpdateOverlay::StartUpdate() {
    if (!mod || fetching || isDownloading) return;
    fetching = true;
    statusMessage = Loc::Get("mod_update_fetching");
    statusError = false;
    struct Outcome {
        std::optional<std::vector<Models::GameBananaFile>> files;
        std::wstring error;
    };
    const std::wstring url = mod->modPageUrl;
    Async::Run(
        [url] {
            Outcome o;
            try {
                o.files = Mods::GameBanana::GetAvailableFiles(url);
            } catch (const Mods::Error& e) {
                o.error = e.Message();
            }
            return o;
        },
        [this](Outcome o) {
            fetching = false;
            if (!o.error.empty()) {
                statusMessage = Loc::Fmt("status_error", o.error);
                statusError = true;
                return;
            }
            if (!o.files || o.files->empty()) {
                statusMessage = Loc::Get("mod_update_no_files");
                statusError = true;
                return;
            }
            if (o.files->size() == 1) {
                Download(*o.files);
                return;
            }
            // Several files: pick which ones (the first one checked)
            std::vector<FileSelectionOverlay::Item> items;
            for (const auto& f : *o.files) items.push_back({ f.fileName.value_or(L"Unknown"), f.fileSize.value_or(0), items.empty() });
            const std::vector<Models::GameBananaFile> available = *o.files;
            files.Show(mod->name, std::move(items), [this, available](const std::vector<FileSelectionOverlay::Item>* picked) {
                if (!picked) return;   // cancelled: back to the update card
                std::vector<Models::GameBananaFile> selected;
                for (const auto& item : *picked) {
                    if (!item.isSelected) continue;
                    // availableFiles.First(f => f.FileName == item.FileName)
                    const auto it = std::find_if(available.begin(), available.end(),
                                                 [&](const auto& f) { return f.fileName.value_or(L"Unknown") == item.fileName; });
                    if (it != available.end()) selected.push_back(*it);
                }
                if (selected.empty()) {
                    statusMessage = Loc::Get("mod_update_none_selected");
                    statusError = true;
                    return;
                }
                Download(std::move(selected));
            });
        });
}

void ModUpdateOverlay::Download(std::vector<Models::GameBananaFile> selected) {
    isDownloading = true;
    progressPercentage = 0.0;
    progressText = Loc::Get("mod_update_preparing");
    struct Outcome {
        Models::ModInfo mod;
        std::wstring error;
        bool ok = false;
    };
    Async::Run(
        [this, info = *mod, manager = *service, selected = std::move(selected)]() mutable {
            Outcome o;
            o.mod = info;
            try {
                manager.DownloadAndUpdateMod(o.mod, selected, info.latestVersion, [this](double percentage, const std::wstring& status) {
                    Async::Post([this, percentage, status] {
                        progressPercentage = percentage;
                        progressText = status;
                    });
                });
                o.ok = true;
            } catch (const Mods::Error& e) {
                o.error = e.Message();
            }
            return o;
        },
        [this](Outcome o) {
            if (!o.ok) {
                statusMessage = Loc::Fmt("status_error", o.error);
                statusError = true;
                isDownloading = false;
                return;
            }
            progressPercentage = 100.0;
            progressText = Loc::Get("mod_update_done");
            // await Task.Delay(500), then UpdateCompleted
            const Models::ModInfo updated = *mod;
            Async::After(500, [this, updated] {
                isDownloading = false;
                Close();
                if (onUpdated) onUpdated(updated);
            });
        });
}

float ModUpdateOverlay::CardHeight(UI::Ui&, float) { return kHeaderH + kPad + kVersionsH + 18.f + kBodyH + kFooterH; }

void ModUpdateOverlay::DrawCard(UI::Ui& ui, const Rect& card) {
    if (!mod) return;
    Frame(ui, card);
    const M::Palette& p = M::P();
    const Color fg = p.foreground;
    const float t = static_cast<float>(ui.Time());
    // Header: "Mod Update Available!" and the mod name, the cloud bobbing
    {
        M::Motion::Enter enter(ui, Reveal(ui, 0), 0.f, -10.f);
        const Rect header{ card.x, card.y, card.w, kHeaderH };
        ui.g.PushClip(header);
        ui.g.FillLinear({ header.x, header.y, header.w, header.h + kRadius }, kRadius, { { 0.f, p.primaryDark.color }, { 1.f, p.primaryMid.color } },
                        { 0.f, 0.f }, { 1.f, 1.f });
        ui.g.PopClip();
        ui.KeepAnimatingThrottled();
        M::DrawIcon(ui.g, M::Icon::CloudDownload, header.x + kPad + 16.f, header.CY() + 3.f * std::sin(t * 2.5f), 32.f, p.primaryMid.foreground);
        TextStyle title = Style(20.f, DWRITE_FONT_WEIGHT_BOLD);
        title.valign = VAlign::Center;
        TextStyle sub = Style(13.f);
        sub.valign = VAlign::Center;
        sub.ellipsis = true;
        const float tx = header.x + kPad + 46.f, tw = header.w - kPad * 2.f - 46.f;
        ui.g.Text(Loc::Get("mod_update_title"), { tx, header.CY() - 24.f, tw, 28.f }, p.primaryMid.foreground, title);
        ui.g.Text(mod->name, { tx, header.CY() + 4.f, tw, 20.f }, p.primaryMid.foreground.Fade(0.85f), sub);
    }
    const Rect in{ card.x + kPad, card.y + kHeaderH + kPad, card.w - kPad * 2.f, 0.f };
    float y = in.y;
    // Current version → latest version
    {
        M::Motion::Enter enter(ui, Reveal(ui, 1), 10.f);
        const float boxW = (in.w - 50.f) * 0.5f;
        const Rect from{ in.x, y, boxW, kVersionsH }, to{ in.R() - boxW, y, boxW, kVersionsH };
        ui.g.FillRound(from, 12.f, fg.Fade(0.05f));
        ui.g.FillRound(to, 12.f, kGreen.Fade(0.14f));
        ui.g.StrokeRound(to.Inset(0.75f), 11.25f, kGreen.Fade(0.6f), 1.5f);
        TextStyle cap = Style(11.f, DWRITE_FONT_WEIGHT_NORMAL, false, HAlign::Center);
        TextStyle v = Style(20.f, DWRITE_FONT_WEIGHT_BOLD, false, HAlign::Center);
        v.valign = VAlign::Center;
        v.ellipsis = true;
        ui.g.Text(Loc::Get("mod_update_current"), { from.x, from.y + 10.f, from.w, 16.f }, fg.Fade(0.6f), cap);
        ui.g.Text(Loc::Get("mod_update_latest"), { to.x, to.y + 10.f, to.w, 16.f }, TabKit::Readable(kGreen), cap);
        ui.g.Text(mod->version, { from.x + 6.f, from.y + 28.f, from.w - 12.f, 34.f }, fg.Fade(0.85f), v);
        ui.g.Text(mod->latestVersion, { to.x + 6.f, to.y + 28.f, to.w - 12.f, 34.f }, fg, v);
        M::DrawIcon(ui.g, M::Icon::ArrowRight, in.CX() + 3.f * std::sin(t * 3.f), from.CY(), 24.f, kGreen);
        y += kVersionsH + 18.f;
    }
    // The progress while downloading, else the status message
    const Rect body{ in.x, y, in.w, kBodyH };
    {
        M::Motion::Enter enter(ui, Reveal(ui, 2), 10.f);
        if (isDownloading) {
            const float shown = ui.Animate(ui.MakeId("mu-progress"), static_cast<float>(progressPercentage / 100.0), 8.f, 0.f);
            const float rcx = body.CX(), rcy = body.y + 52.f, radius = 44.f;
            ui.g.StrokeEllipse(rcx, rcy, radius, radius, fg.Fade(0.08f), 8.f);
            TabKit::Arc(ui.g, rcx, rcy, radius, 0.f, std::max(2.f, 360.f * shown), kGreen, 8.f);
            TextStyle pct = Style(18.f, DWRITE_FONT_WEIGHT_BOLD, false, HAlign::Center);
            pct.valign = VAlign::Center;
            wchar_t text[16];
            swprintf_s(text, L"%.1f%%", progressPercentage);   // {0:F1}%
            ui.g.Text(text, { rcx - radius, rcy - 14.f, radius * 2.f, 28.f }, fg, pct);
            TextStyle caption = Style(12.f, DWRITE_FONT_WEIGHT_NORMAL, false, HAlign::Center);
            caption.ellipsis = true;
            ui.g.Text(progressText, { body.x, rcy + radius + 14.f, body.w, 18.f }, fg.Fade(0.75f), caption);
            ui.KeepAnimatingThrottled();
        } else {
            const bool error = statusError;
            const Color c = error ? kOrange : p.primaryMid.color;
            ui.g.FillRound(body, 14.f, c.Fade(0.08f));
            M::DrawIcon(ui.g, fetching ? M::Icon::CloudSearch : error ? M::Icon::AlertCircle : M::Icon::InformationOutline, body.CX(),
                        body.y + 42.f + (fetching ? 3.f * std::sin(t * 6.f) : 0.f), 34.f, TabKit::Readable(c));
            if (fetching) ui.KeepAnimating();
            const TextStyle s = Style(13.f, DWRITE_FONT_WEIGHT_NORMAL, true, HAlign::Center);
            const float sh = TabKit::TextH(ui, statusMessage, s, body.w - 40.f);
            ui.g.Text(statusMessage, { body.x + 20.f, body.y + 74.f, body.w - 40.f, sh }, fg.Fade(0.85f), s);
        }
    }
    // Cancel / Download & Update (hidden while downloading)
    if (!isDownloading) {
        M::Motion::Enter enter(ui, Reveal(ui, 3), 10.f);
        const float by = card.B() - kFooterH + 14.f;
        const std::wstring& go = Loc::Get("mod_update_download");
        const std::wstring& cancel = Loc::Get("cancel");
        const float gw = TabKit::PillWidth(ui, go), cw = TabKit::PillWidth(ui, cancel);
        if (TabKit::PillButton(ui, ui.MakeId("mu-cancel"), { card.R() - kPad - gw - 10.f - cw, by, cw, 44.f }, M::Icon::Close, cancel, fg.Fade(0.8f), false))
            Close();
        if (TabKit::PillButton(ui, ui.MakeId("mu-go"), { card.R() - kPad - gw, by, gw, 44.f }, M::Icon::Download, go, kGreen, true, !fetching)) StartUpdate();
    }
}

} // namespace SonicRacingSaveManager::Main
