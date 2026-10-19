#include "Windows/Main/Tabs/Steam/SteamOverlay.h"
#include <windows.h>
#include <shellapi.h>
#include <algorithm>
#include <cmath>
#include "App/Paths.h"
#include "App/SteamShortcutHelper.h"
#include "Core/Files.h"
#include "Core/Process.h"
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "Services/Platforms/SteamShortcutService.h"
#include "UI/Material/Motion.h"
#include "UI/Material/PackIcon.h"
#include "UI/Material/Theme.h"
#include "Windows/Dialogs/MessageDialog.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Tabs/TabKit.h"

namespace SonicRacingSaveManager::Main {

namespace {

namespace M = UI::Material;
using TabKit::Style;
using TabKit::TextH;

constexpr float kHeaderH = 84.f, kTile = 104.f, kFooterH = 76.f;
const Color kSteamBlue = Rgba(0x66, 0xC0, 0xF4), kSteamMid = Rgba(0x2A, 0x47, 0x5E), kSteamDark = Rgba(0x17, 0x1A, 0x21), kAmber = Argb(0xFFFFB300);

std::wstring IconPath() { return Paths::Asset(L"Steam\\icons\\iconHD.png"); }

TextStyle NoteStyle() { return Style(12.f, DWRITE_FONT_WEIGHT_NORMAL, true); }

// Win32Exception.Message
std::wstring LastError() {
    wchar_t* text = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, GetLastError(), 0,
                   reinterpret_cast<wchar_t*>(&text), 0, nullptr);
    std::wstring message = text ? text : L"";
    if (text) LocalFree(text);
    while (!message.empty() && (message.back() == L'\n' || message.back() == L'\r')) message.pop_back();
    return message;
}

} // namespace

Color AddToSteamOverlay::Accent() const { return kSteamBlue; }

void AddToSteamOverlay::Show(bool running) {
    steamRunning = running;
    Open();
}

float AddToSteamOverlay::CardHeight(UI::Ui& ui, float width) {
    const std::wstring& note = Loc::Get(steamRunning ? "steam_helper_warning_running" : "steam_helper_warning_not_running");
    const float noteH = TextH(ui, note, NoteStyle(), width - kPad * 2.f - 48.f);
    return kHeaderH + kPad + 18.f + 10.f + kTile + 12.f + 22.f + 18.f + noteH + 26.f + kFooterH;
}

void AddToSteamOverlay::DrawCard(UI::Ui& ui, const Rect& card) {
    Frame(ui, card);
    const M::Palette& p = M::P();
    const Color fg = p.foreground;
    const float t = static_cast<float>(ui.Time());
    const float w = card.w - kPad * 2.f;
    // Header: Steam gradient, the Steam icon, "ADD TO STEAM"
    {
        M::Motion::Enter enter(ui, Reveal(ui, 0), 0.f, -10.f);
        const Rect header{ card.x, card.y, card.w, kHeaderH };
        ui.g.PushClip(header);
        ui.g.FillLinear({ header.x, header.y, header.w, header.h + kRadius }, kRadius, { { 0.f, kSteamDark }, { 1.f, kSteamMid } }, { 0.f, 0.f }, { 1.f, 1.f });
        const float gx = header.x + header.w * (0.65f + 0.2f * std::sin(t * 0.6f));
        ui.g.FillRadial({ gx - 160.f, header.y - 70.f, 320.f, 200.f }, { { 0.f, kSteamBlue.Fade(0.25f) }, { 1.f, kSteamBlue.Fade(0.f) } });
        ui.g.PopClip();
        ui.KeepAnimatingThrottled();
        M::DrawIcon(ui.g, M::Icon::Steam, header.x + kPad + 16.f, header.CY(), 32.f, Colors::White);
        TextStyle title = Style(20.f, DWRITE_FONT_WEIGHT_BOLD);
        title.valign = VAlign::Center;
        ui.g.Text(Loc::Get("steam_helper_title"), { header.x + kPad + 46.f, header.y, header.w - kPad * 2.f, header.h }, Colors::White, title);
    }
    float y = card.y + kHeaderH + kPad;
    {
        M::Motion::Enter enter(ui, Reveal(ui, 1), 10.f);
        TextStyle caption = Style(11.f, DWRITE_FONT_WEIGHT_BOLD, false, HAlign::Center);
        caption.valign = VAlign::Center;
        ui.g.Text(Loc::Get("steam_add_icon"), { card.x, y, card.w, 18.f }, TabKit::Readable(kSteamBlue), caption);
        y += 18.f + 10.f;
    }
    // The tile: bobbing, glowing, a shine passing over it every few seconds
    {
        const float k = Reveal(ui, 2);
        M::Motion::Enter enter(ui, k, 0.f);
        const float bob = 4.f * std::sin(t * 2.f);
        const Rect tile{ card.CX() - kTile * 0.5f, y + bob, kTile, kTile };
        ui.PushScale(0.7f + 0.3f * Anim::EaseOutBack(k, 1.6f), tile.CX(), tile.CY());
        ui.g.GlowRound(tile, 22.f, kSteamBlue, 24.f, 0.45f + 0.15f * std::sin(t * 2.f));
        ui.g.FillLinear(tile, 22.f, { { 0.f, kSteamMid }, { 1.f, kSteamDark } }, { 0.f, 0.f }, { 1.f, 1.f });
        if (auto icon = Images::Load(IconPath())) ui.g.DrawImage(*icon, tile.Inset(14.f), 1.f, Stretch::Uniform);
        const float sweep = std::fmod(t, 3.5f) / 0.9f;
        if (sweep < 1.f) {
            ui.g.FillLinear(tile, 22.f,
                            { { std::clamp(sweep - 0.25f, 0.f, 1.f), Colors::White.Fade(0.f) }, { std::clamp(sweep, 0.f, 1.f), Colors::White.Fade(0.3f) },
                              { std::clamp(sweep + 0.25f, 0.f, 1.f), Colors::White.Fade(0.f) } },
                            { 0.f, 0.f }, { 1.f, 1.f });
            ui.KeepAnimating();
        }
        ui.g.StrokeRound(tile.Inset(0.75f), 21.25f, kSteamBlue.Fade(0.7f), 1.5f);
        ui.PopTransform();
        y += kTile + 12.f;
        TextStyle name = Style(15.f, DWRITE_FONT_WEIGHT_SEMI_BOLD, false, HAlign::Center);
        name.valign = VAlign::Center;
        ui.g.Text(Services::SteamShortcuts::AppName, { card.x, y, card.w, 22.f }, fg, name);
        y += 22.f + 18.f;
    }
    // What happens to Steam: amber when it has to restart, blue otherwise
    {
        M::Motion::Enter enter(ui, Reveal(ui, 3), 10.f);
        const Color c = steamRunning ? kAmber : kSteamBlue;
        const std::wstring& note = Loc::Get(steamRunning ? "steam_helper_warning_running" : "steam_helper_warning_not_running");
        const float noteH = TextH(ui, note, NoteStyle(), w - 48.f);
        const Rect box{ card.x + kPad, y, w, noteH + 26.f };
        ui.g.FillRound(box, 12.f, c.Fade(0.12f));
        ui.g.StrokeRound(box.Inset(0.5f), 11.5f, c.Fade(0.45f), 1.f);
        const float spin = steamRunning ? std::fmod(t * 90.f, 360.f) : 0.f;
        ui.g.PushTransform(D2D1::Matrix3x2F::Rotation(spin, D2D1::Point2F(box.x + 24.f, box.CY())));
        M::DrawIcon(ui.g, steamRunning ? M::Icon::Refresh : M::Icon::Information, box.x + 24.f, box.CY(), 20.f, c);
        ui.g.PopTransform();
        ui.g.Text(note, { box.x + 44.f, box.CY() - noteH * 0.5f, w - 48.f, noteH }, TabKit::Readable(c), NoteStyle());
    }
    // Cancel / ADD TO STEAM
    {
        M::Motion::Enter enter(ui, Reveal(ui, 4), 10.f);
        const float by = card.B() - kFooterH + 14.f;
        const std::wstring& add = Loc::Get("steam_helper_title");
        const std::wstring& cancel = Loc::Get("cancel");
        const float aw = TabKit::PillWidth(ui, add), cw = TabKit::PillWidth(ui, cancel);
        if (TabKit::PillButton(ui, ui.MakeId("st-cancel"), { card.R() - kPad - aw - 10.f - cw, by, cw, 44.f }, M::Icon::Close, cancel, fg.Fade(0.8f), false))
            Close();
        if (TabKit::PillButton(ui, ui.MakeId("st-add"), { card.R() - kPad - aw, by, aw, 44.f }, M::Icon::Steam, add, kSteamBlue, true)) Confirm();
    }
}

// AddToSteamConfirmBtn_Click: the close-Steam / edit-vdf / restart-Steam sequence runs in a fresh,
// elevated copy of this exe (never the process Steam tracks), and this one exits right away
void AddToSteamOverlay::Confirm() {
    Close();
    const HWND owner = main.Hwnd();
    const std::wstring& title = Loc::Get("steam_add_title");
    const std::wstring exe = Files::ExecutablePath();
    if (exe.empty()) {
        Dialogs::Message(owner, Loc::Get("steam_add_exe_error"), title, Dialogs::MessageButtons::Ok, Dialogs::MessageIcon::Error);
        return;
    }
    const std::wstring icon = IconPath();
    if (!Files::Exists(icon)) {
        Dialogs::Message(owner, Loc::Fmt("steam_add_icon_error", icon), title, Dialogs::MessageButtons::Ok, Dialogs::MessageIcon::Error);
        return;
    }
    const std::wstring args = std::wstring(App::SteamShortcutHelperArg) + L" " + Process::QuoteArgument(exe) + L" " + Process::QuoteArgument(icon);
    SHELLEXECUTEINFOW info{ sizeof(info) };
    info.lpVerb = L"runas";
    info.lpFile = exe.c_str();
    info.lpParameters = args.c_str();
    info.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&info)) {
        Dialogs::Message(owner, Loc::Fmt("steam_add_error", LastError()), title, Dialogs::MessageButtons::Ok, Dialogs::MessageIcon::Error);
        return;
    }
    // This process must be gone before the helper shuts Steam down: hard exit (Environment.Exit(0))
    ExitProcess(0);
}

} // namespace SonicRacingSaveManager::Main
