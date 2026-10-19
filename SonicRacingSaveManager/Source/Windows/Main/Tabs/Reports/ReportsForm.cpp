#include "Windows/Main/Tabs/Reports/ReportsTab.h"
#include <algorithm>
#include <cmath>
#include <cwctype>
#include "App/Paths.h"
#include "Core/Files.h"
#include "Core/System.h"
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "Services/App/Log.h"
#include "Services/Community/ReportServices.h"
#include "UI/Material/Buttons.h"
#include "UI/Material/Card.h"
#include "UI/Material/Elevation.h"
#include "UI/Material/Fields.h"
#include "UI/Material/Motion.h"
#include "UI/Material/Theme.h"
#include "UI/Material/Toggles.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Overlays/DiscordInfoOverlay.h"
#include "Windows/Main/Tabs/TabKit.h"
#include "Windows/Main/Tabs/TabBanner.h"

// The report form of the Reports tab: title, description, the screenshot drop zone, the logs row
// and the send row, and sending the report.
namespace SonicRacingSaveManager::Main {

namespace M = UI::Material;
namespace OAuth = Services::DiscordOAuth;
using namespace TabKit;

namespace {

const Color kGreen = Argb(0xFF4CAF50), kOrange = Argb(0xFFFF9800), kRed = Argb(0xFFF4511E);

std::wstring Trim(const std::wstring& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::iswspace(s[a])) ++a;
    while (b > a && std::iswspace(s[b - 1])) --b;
    return s.substr(a, b - a);
}

TextStyle Caps() {
    TextStyle t = Style(11.f, DWRITE_FONT_WEIGHT_BOLD);
    t.valign = VAlign::Center;
    return t;
}

// "Title" on the left, "12 / 100" on the right
void FieldLabel(UI::Ui& ui, const Rect& r, const std::wstring& text, size_t count, size_t limit) {
    const Color fg = M::P().foreground;
    TextStyle label = Style(12.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    label.valign = VAlign::Center;
    ui.g.Text(text, r, fg.Fade(0.8f), label);
    TextStyle counter = Style(11.f, DWRITE_FONT_WEIGHT_NORMAL, false, HAlign::Right);
    counter.valign = VAlign::Center;
    ui.g.Text(std::to_wstring(count) + L" / " + std::to_wstring(limit), r, count > limit ? kRed : fg.Fade(0.45f), counter);
}

} // namespace

// ── Form: title, description (fills the card), screenshot zone, send row ──
void ReportsTab::Form(UI::Ui& ui, const Rect& r) {
    const M::Palette& p = M::P();
    const Color fg = p.foreground;
    float y = r.y;
    int row = 0;
    const auto Reveal = [&](int index) { return M::Motion::Stagger(ui, loadedAt + 0.2, index, 0.06f, 0.4f); };

    {
        M::Motion::Enter enter(ui, Reveal(row++), 10.f);
        ui.g.Text(Loc::Get("reports_your_report"), { r.x, y, r.w, 18.f }, p.primaryMid.color, Caps());
        TextStyle hint = Style(12.f);
        hint.valign = VAlign::Center;
        ui.g.Text(Loc::Get("reports_your_report_hint"), { r.x, y + 18.f, r.w, 18.f }, fg.Fade(0.6f), hint);
    }
    y += 48.f;

    M::FieldStyle field;
    field.outlined = true;
    field.fontSize = 13.f;
    {
        M::Motion::Enter enter(ui, Reveal(row++), 10.f);
        FieldLabel(ui, { r.x, y, r.w, 18.f }, Loc::Get("reports_field_title"), title.size(), 100);
        field.hint = Loc::Get("reports_title_hint");
        const float h = M::FieldHeight(ui, field);
        M::TextField(ui, ui.MakeId("rep-title"), { r.x, y + 24.f, r.w, h }, title, field);
        y += 24.f + h + 18.f;
    }

    constexpr float kShotH = 104.f, kLogsH = 48.f, kSendH = 52.f;
    const float descH = std::max(120.f, r.B() - y - 24.f - 18.f - kShotH - 14.f - kLogsH - 18.f - kSendH);
    {
        M::Motion::Enter enter(ui, Reveal(row++), 10.f);
        FieldLabel(ui, { r.x, y, r.w, 18.f }, Loc::Get("reports_field_description"), description.size(), 2000);
        field.hint = Loc::Get("reports_description_hint");
        field.multiline = true;
        M::TextField(ui, ui.MakeId("rep-desc"), { r.x, y + 24.f, r.w, descH }, description, field);
        y += 24.f + descH + 18.f;
    }
    {
        M::Motion::Enter enter(ui, Reveal(row++), 10.f);
        Screenshot(ui, { r.x, y, r.w, kShotH });
        y += kShotH + 14.f;
    }
    {
        M::Motion::Enter enter(ui, Reveal(row++), 10.f);
        LogsRow(ui, { r.x, y, r.w, kLogsH });
        y += kLogsH + 18.f;
    }
    {
        M::Motion::Enter enter(ui, Reveal(row++), 10.f);
        SendRow(ui, { r.x, y, r.w, kSendH });
    }
}

// Drop-zone look: dashed-feeling outline that lights up on hover; once a picture is chosen, its
// preview, name and a remove button
void ReportsTab::Screenshot(UI::Ui& ui, const Rect& r) {
    const M::Palette& p = M::P();
    const Color fg = p.foreground;
    const UI::Id id = ui.MakeId("rep-image");
    const bool hasImage = !imagePath.empty();
    const Rect remove{ r.R() - 44.f, r.CY() - 16.f, 32.f, 32.f };
    const UI::Interaction it = ui.Interact(id, r);
    if (it.hovered) ui.SetCursor(IDC_HAND);
    const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, it.held);
    const float attached = ui.Tween(UI::HashId(id, 0xA77), hasImage ? 1.f : 0.f, 0.3f);

    ui.g.FillRound(r, 14.f, Lerp(fg.Fade(0.04f), p.primaryMid.color.Fade(0.1f), fb.hover));
    // Dashes along the outline, drifting while hovered
    {
        const Color dash = Lerp(fg.Fade(0.25f), p.primaryMid.color, fb.hover);
        const float offset = std::fmod(static_cast<float>(ui.Time()) * 30.f * fb.hover, 16.f);
        if (fb.hover > 0.f) ui.KeepAnimating();
        const Rect in = r.Inset(1.f);
        for (float x = in.x + 14.f + offset - 16.f; x < in.R() - 14.f; x += 16.f) {
            const float a = std::max(x, in.x + 14.f), b = std::min(x + 8.f, in.R() - 14.f);
            if (b > a) {
                ui.g.Line(a, in.y, b, in.y, dash, 1.5f);
                ui.g.Line(a, in.B(), b, in.B(), dash, 1.5f);
            }
        }
        for (float y = in.y + 14.f + offset - 16.f; y < in.B() - 14.f; y += 16.f) {
            const float a = std::max(y, in.y + 14.f), b = std::min(y + 8.f, in.B() - 14.f);
            if (b > a) {
                ui.g.Line(in.x, a, in.x, b, dash, 1.5f);
                ui.g.Line(in.R(), a, in.R(), b, dash, 1.5f);
            }
        }
        // Corners
        const auto Corner = [&](float cx, float cy, float start) { TabKit::Arc(ui.g, cx, cy, 13.f, start, 90.f, dash, 1.5f); };
        Corner(in.x + 14.f, in.y + 14.f, 270.f);
        Corner(in.R() - 14.f, in.y + 14.f, 0.f);
        Corner(in.R() - 14.f, in.B() - 14.f, 90.f);
        Corner(in.x + 14.f, in.B() - 14.f, 180.f);
    }

    TextStyle name = Style(14.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    name.valign = VAlign::Center;
    name.ellipsis = true;
    TextStyle hint = Style(11.f);
    hint.valign = VAlign::Center;
    if (!hasImage) {
        M::Motion::Enter enter(ui, 1.f - attached, 0.f);
        const float bob = -4.f * fb.hover + 1.5f * std::sin(static_cast<float>(ui.Time()) * 2.f);
        ui.KeepAnimatingThrottled();
        const float icx = r.x + 52.f, icy = r.CY() + bob;
        ui.g.FillEllipse(icx, icy, 26.f, 26.f, p.primaryMid.color.Fade(0.14f + 0.1f * fb.hover));
        M::DrawIcon(ui.g, M::Icon::ImagePlus, icx, icy, 26.f, p.primaryMid.color);
        ui.g.Text(Loc::Get("reports_attach_screenshot"), { r.x + 94.f, r.CY() - 20.f, r.w - 110.f, 22.f }, fg, name);
        ui.g.Text(Loc::Get("reports_screenshot_formats"), { r.x + 94.f, r.CY() + 2.f, r.w - 110.f, 18.f },
                  fg.Fade(0.6f), hint);
    } else {
        M::Motion::Enter enter(ui, attached, 0.f, 16.f);
        const Rect thumb{ r.x + 14.f, r.y + 12.f, (r.h - 24.f) * 1.6f, r.h - 24.f };
        if (auto picture = Images::Load(imagePath, 512)) ui.g.DrawImageRounded(*picture, thumb, 10.f, 1.f, Stretch::UniformToFill);
        else ui.g.FillRound(thumb, 10.f, fg.Fade(0.08f));
        ui.g.StrokeRound(thumb.Inset(0.5f), 9.5f, fg.Fade(0.15f), 1.f);
        const float tx = thumb.R() + 16.f, tw = remove.x - 12.f - tx;
        M::DrawIcon(ui.g, M::Icon::CheckCircle, tx + 9.f, r.CY() - 11.f, 18.f, kGreen);
        ui.g.Text(imageFileName, { tx + 26.f, r.CY() - 22.f, tw - 26.f, 22.f }, fg, name);
        ui.g.Text(Loc::Get("reports_screenshot_attached"), { tx, r.CY() + 2.f, tw, 18.f }, fg.Fade(0.6f), hint);
        const UI::Id removeId = ui.MakeId("rep-image-remove");
        M::ButtonLook look = M::IconButtonLook(fg.Fade(0.7f));
        look.padding = Edges(0.f);
        if (M::IconButton(ui, removeId, remove, M::Icon::Close, 18.f, look)) {
            imagePath.clear();
            imageFileName.clear();
            return;
        }
        ui.Tooltip(removeId, Loc::Get("reports_remove_screenshot"));
    }
    if (it.clicked) PickImage();
}

// Status message (slides in) on the left, the send button on the right: glows once the report is
// ready, a spinner while sending, a green burst once sent
void ReportsTab::SendRow(UI::Ui& ui, const Rect& r) {
    const M::Palette& p = M::P();
    const float t = static_cast<float>(ui.Time());
    const bool ready = discordUser && !Trim(title).empty() && Trim(description).size() >= 20;

    if (statusAt == -1.0) statusAt = ui.Time();
    if (sentAt == -1.0) sentAt = ui.Time();
    constexpr float kButtonW = 210.f;
    const Rect button{ r.R() - kButtonW, r.CY() - 23.f, kButtonW, 46.f };

    if (statusVisible) {
        const float k = Anim::EaseOutCubic(Anim::Clamp01(static_cast<float>((ui.Time() - statusAt) / 0.4)));
        if (k < 1.f) ui.KeepAnimating();
        const Color c = statusKind == StatusKind::Success ? kGreen : statusKind == StatusKind::Error ? kRed : kOrange;
        const M::Icon icon = statusKind == StatusKind::Success ? M::Icon::CheckCircle : statusKind == StatusKind::Error ? M::Icon::AlertCircle : M::Icon::AlertOutline;
        TextStyle text = Style(12.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        text.valign = VAlign::Center;
        text.ellipsis = true;
        const float maxW = button.x - 16.f - r.x;
        const std::wstring& status = Loc::Get(statusKey);
        const float pw = std::min(maxW, std::ceil(ui.g.Measure(status, text).w) + 52.f);
        M::Motion::Enter enter(ui, k, 0.f, -24.f);
        const Rect pill{ r.x, r.CY() - 19.f, pw, 38.f };
        ui.g.FillRound(pill, 19.f, c.Fade(0.14f));
        ui.g.StrokeRound(pill.Inset(0.5f), 18.5f, c.Fade(0.5f), 1.f);
        M::DrawIcon(ui.g, icon, pill.x + 22.f, pill.CY(), 20.f, c);
        ui.g.Text(status, { pill.x + 40.f, pill.y, pw - 52.f, pill.h }, p.baseDark ? Lerp(c, Colors::White, 0.3f) : Lerp(c, Colors::Black, 0.35f), text);
    }

    const UI::Id id = ui.MakeId("rep-send");
    const UI::Interaction it = ui.Interact(id, button, sendEnabled);
    if (it.hovered) ui.SetCursor(IDC_HAND);
    const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered && sendEnabled, it.held);
    const float glow = ready && sendEnabled ? 0.35f + 0.25f * std::sin(t * 3.f) : 0.f;
    if (glow > 0.f) {
        ui.g.GlowRound(button, 23.f, p.primaryMid.color, 16.f, glow);
        ui.KeepAnimatingThrottled();
    }
    // Burst once the report went out
    const float since = static_cast<float>(ui.Time() - sentAt);
    if (since >= 0.f && since < 1.f) {
        ui.KeepAnimating();
        const float k = Anim::EaseOutCubic(since);
        ui.g.StrokeRound(button.Inset(-14.f * k), 23.f + 14.f * k, kGreen.Fade(1.f - since), 3.f);
    }
    const float scale = 1.f + 0.03f * fb.hover - 0.04f * fb.press;
    ui.PushScale(scale, button.CX(), button.CY());
    const Color bg = since >= 0.f && since < 2.f ? kGreen : p.primaryMid.color;
    M::Elevation(ui.g, button, 23.f, 2.f + 4.f * fb.hover);
    ui.g.FillRound(button, 23.f, sendEnabled || sending ? bg : bg.Fade(0.5f));
    const Color fg = sendEnabled || sending ? p.primaryMid.foreground : p.primaryMid.foreground.Fade(0.6f);
    TextStyle label = Style(15.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    label.valign = VAlign::Center;
    const std::wstring text = since >= 0.f && since < 2.f ? Loc::Get("reports_sent") : Loc::Get(sending ? "reports_sending" : "reports_send");
    const float lw = std::ceil(ui.g.Measure(text, label).w);
    const float cx = button.CX() - (22.f + 10.f + lw) * 0.5f;
    if (sending) {
        TabKit::Arc(ui.g, cx + 11.f, button.CY(), 9.f, t * 360.f, 260.f, fg, 2.5f);
        ui.KeepAnimating();
    } else {
        // The paper plane takes off on hover
        const float fly = 5.f * fb.hover;
        ui.g.PushTransform(D2D1::Matrix3x2F::Rotation(-18.f * fb.hover, D2D1::Point2F(cx + 11.f, button.CY())) *
                           D2D1::Matrix3x2F::Translation(fly, -fly * 0.6f));
        M::DrawIcon(ui.g, since >= 0.f && since < 2.f ? M::Icon::Check : M::Icon::Send, cx + 11.f, button.CY(), 22.f, fg);
        ui.g.PopTransform();
    }
    ui.g.Text(text, { cx + 32.f, button.y, lw + 2.f, button.h }, fg, label);
    ui.PopTransform();
    if (it.clicked && sendEnabled) Send();
}

// "Attach the manager's logs" switch (logs.zip goes with the report) and "Copy logs" (the log text
// to the clipboard, a check and "Copied!" for a moment)
void ReportsTab::LogsRow(UI::Ui& ui, const Rect& r) {
    const M::Palette& p = M::P();
    const Color fg = p.foreground;
    ui.g.FillRound(r, 14.f, fg.Fade(0.04f));
    const Rect sw{ r.x + 14.f, r.CY() - M::kSwitchHeight * 0.5f, M::kSwitchWidth, M::kSwitchHeight };
    const UI::Id switchId = ui.MakeId("rep-logs-switch");
    if (M::Switch(ui, switchId, sw, includeLogs)) includeLogs = !includeLogs;
    ui.Tooltip(switchId, Loc::Get("reports_logs_tooltip"));
    TextStyle name = Style(13.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    name.valign = VAlign::Center;
    TextStyle hint = Style(11.f);
    hint.valign = VAlign::Center;
    hint.ellipsis = true;

    // Copy logs (outlined pill), feedback for 2 s
    const float since = static_cast<float>(ui.Time() - logsCopiedAt);
    const bool copied = since >= 0.f && since < 2.f;
    if (copied) ui.KeepAnimatingThrottled();
    const std::wstring& label = Loc::Get(copied ? "reports_copied" : "reports_copy_logs");
    TextStyle text = Style(13.f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    text.valign = VAlign::Center;
    const float bw = 150.f;
    const Rect button{ r.R() - 8.f - bw, r.CY() - 17.f, bw, 34.f };
    const UI::Id copyId = ui.MakeId("rep-logs-copy");
    const UI::Interaction it = ui.Interact(copyId, button);
    if (it.hovered) ui.SetCursor(IDC_HAND);
    const M::Motion::Feedback fb = M::Motion::Hover(ui, copyId, it.hovered, it.held);
    const Color accent = copied ? kGreen : p.primaryMid.color;
    ui.PushScale(1.f - 0.04f * fb.press, button.CX(), button.CY());
    ui.g.FillRound(button, 17.f, accent.Fade(0.08f + 0.12f * fb.hover));
    ui.g.StrokeRound(button.Inset(0.75f), 16.25f, accent.Fade(0.6f + 0.4f * fb.hover), 1.5f);
    const float pop = copied ? 1.f + 0.25f * std::max(0.f, 1.f - since / 0.3f) : 1.f;
    const float lw = std::ceil(ui.g.Measure(label, text).w);
    const float cx = button.CX() - (18.f + 8.f + lw) * 0.5f;
    M::DrawIcon(ui.g, copied ? M::Icon::CheckCircle : M::Icon::ClipboardTextClockOutline, cx + 9.f, button.CY(), 18.f * pop, accent);
    ui.g.Text(label, { cx + 26.f, button.y, lw + 2.f, button.h }, p.baseDark ? Lerp(accent, Colors::White, 0.3f) : Lerp(accent, Colors::Black, 0.3f), text);
    ui.PopTransform();
    ui.Tooltip(copyId, Loc::Get("reports_copy_logs_tooltip"));
    if (it.clicked) {
        System::SetClipboardText(Log::Text());
        logsCopiedAt = ui.Time();
    }

    const float tx = sw.R() + 14.f, tw = button.x - 12.f - tx;
    ui.g.Text(Loc::Get("reports_attach_logs"), { tx, r.CY() - 18.f, tw, 18.f }, fg.Fade(includeLogs ? 0.95f : 0.6f), name);
    ui.g.Text(Loc::Get(includeLogs ? "reports_logs_on" : "reports_logs_off"), { tx, r.CY(), tw, 18.f },
              fg.Fade(0.55f), hint);
}

void ReportsTab::PickImage() {
    const std::wstring picked = System::PickFile(main.Hwnd(), L"", { { Loc::Get("reports_images_filter"), L"*.jpg;*.jpeg;*.png;*.gif;*.webp;*.bmp" } });
    if (picked.empty()) return;
    imagePath = picked;
    imageFileName = Files::FileName(picked);
}

void ReportsTab::ShowStatus(const char* messageKey, StatusKind kind) {
    statusKey = messageKey;
    statusKind = kind;
    statusVisible = true;
    statusAt = -1.0;   // stamped on the next frame (this runs from async callbacks)
}

void ReportsTab::Send() {
    const std::wstring t = Trim(title), d = Trim(description);
    if (t.empty() || d.empty()) {
        ShowStatus("reports_fill_both", StatusKind::Warning);
        return;
    }
    if (!discordUser) {
        ShowStatus("reports_connect_first", StatusKind::Warning);
        return;
    }
    sendEnabled = false;
    sending = true;
    statusVisible = false;
    const OAuth::User user = *discordUser;
    const std::wstring image = imagePath;
    const bool logs = includeLogs;
    Log::Info(L"Sending a bug report" + std::wstring(logs ? L" with logs.zip" : L""));
    Async::Run([user, t, d, image, logs] { return Services::Reports::SendBugReport(user.id, user.username, t, d, image, logs ? Log::Zip() : std::string()); },
               [this](bool ok) {
        sending = false;
        if (ok) {
            ShowStatus("reports_sent_ok", StatusKind::Success);
            sentAt = -1.0;
            title.clear();
            description.clear();
            imagePath.clear();
            imageFileName.clear();
            Async::After(2000, [this] {
                statusVisible = false;
                sendEnabled = true;
            });
        } else {
            ShowStatus("reports_send_failed", StatusKind::Error);
            sendEnabled = true;
        }
    });
}

} // namespace SonicRacingSaveManager::Main

