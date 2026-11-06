#include "Windows/Main/Tabs/SaveEditor/SaveEditorSwitch.h"
#include <algorithm>
#include "Localization/Localization.h"
#include "UI/Material/Buttons.h"
#include "UI/Material/Motion.h"
#include "UI/Material/Theme.h"
#include "Windows/Main/Tabs/SaveEditor/SaveEditorTab.h"
#include "Windows/Main/Tabs/TabKit.h"

namespace SonicRacingSaveManager::Main {

namespace {
namespace M = UI::Material;
using namespace SaveEditorKit;
using TabKit::Style;

constexpr Color kSwitchRed = Rgba(230, 0, 18);
constexpr float kTop = 92.f, kFooterH = 76.f, kSystemH = 104.f, kStepGap = 14.f;
constexpr int kSteps = 6;

// The console's systems: its name, what it is, its colour (the one to use green, the one not to amber)
struct SystemInfo {
    const char* name;
    const char* about;
    Color tone;
    M::Icon icon;
};
const SystemInfo kSystems[3] = {
    { "saveeditor_switch_sys_stock", "saveeditor_switch_sys_stock_about", Rgba(100, 116, 139), M::Icon::NintendoSwitch },
    { "saveeditor_switch_sys_cfw", "saveeditor_switch_sys_cfw_about", kGreen, M::Icon::CheckCircle },
    { "saveeditor_switch_sys_emu", "saveeditor_switch_sys_emu_about", kAmber, M::Icon::AlertOutline },
};
} // namespace

Color SaveEditorSwitch::Accent() const { return kSwitchRed; }

float SaveEditorSwitch::CardHeight(UI::Ui& ui, float width) {
    const Rect card{ 0.f, 0.f, width, 10000.f };
    return Body(ui, card, false) + kFooterH;
}

float SaveEditorSwitch::Body(UI::Ui& ui, const Rect& card, bool draw) {
    const M::Palette& p = M::P();
    const float x = card.x + kPad, w = card.w - kPad * 2.f;
    float y = card.y + kTop;
    int row = 1;
    TextStyle text = Style(12.5f, DWRITE_FONT_WEIGHT_NORMAL, true);
    TextStyle caps = Style(11.f, DWRITE_FONT_WEIGHT_BOLD);
    caps.valign = VAlign::Center;

    // What it is, and that it needs a modded console
    {
        const std::wstring& intro = Loc::Get("saveeditor_switch_intro");
        const float h = TabKit::TextH(ui, intro, text, w);
        if (draw) {
            M::Motion::Enter enter(ui, Reveal(ui, row), 0.f, 10.f);
            ui.g.Text(intro, { x, y, w, h }, p.foreground.Fade(0.85f), text);
        }
        y += h + 18.f;
        ++row;
    }

    // The 3 systems, side by side
    {
        if (draw) ui.g.Text(Loc::Get("saveeditor_switch_systems"), { x, y, w, 20.f }, kSwitchRed, caps);
        y += 28.f;
        const float cw = (w - 2.f * kGap) / 3.f;
        TextStyle name = Style(13.f, DWRITE_FONT_WEIGHT_BOLD);
        name.valign = VAlign::Center;
        name.ellipsis = true;
        TextStyle about = Style(11.5f, DWRITE_FONT_WEIGHT_NORMAL, true);
        float tallest = kSystemH;
        for (int k = 0; k < 3; ++k) tallest = std::max(tallest, 48.f + TabKit::TextH(ui, Loc::Get(kSystems[k].about), about, cw - 28.f) + 14.f);
        for (int k = 0; k < 3 && draw; ++k) {
            const SystemInfo& s = kSystems[k];
            M::Motion::Enter enter(ui, Reveal(ui, row + k), 0.f, 12.f);
            const Rect c{ x + k * (cw + kGap), y, cw, tallest };
            ui.g.FillRound(c, 14.f, s.tone.Fade(k == 1 ? 0.16f : 0.09f));
            ui.g.StrokeRound(c, 14.f, s.tone.Fade(k == 1 ? 0.8f : 0.35f), k == 1 ? 2.f : 1.f);
            ui.g.FillEllipse(c.x + 28.f, c.y + 26.f, 14.f, 14.f, s.tone.Fade(0.2f));
            M::DrawIcon(ui.g, s.icon, c.x + 28.f, c.y + 26.f, 17.f, TabKit::Readable(s.tone));
            ui.g.Text(Loc::Get(s.name), { c.x + 50.f, c.y + 14.f, c.w - 62.f, 24.f }, p.foreground, name);
            ui.g.Text(Loc::Get(s.about), { c.x + 14.f, c.y + 48.f, c.w - 28.f, c.h - 56.f }, p.foreground.Fade(0.75f), about);
        }
        y += tallest + 22.f;
        row += 3;
    }

    // The steps, one by one
    {
        if (draw) ui.g.Text(Loc::Get("saveeditor_switch_steps"), { x, y, w, 20.f }, kSwitchRed, caps);
        y += 30.f;
        TextStyle num = Style(12.f, DWRITE_FONT_WEIGHT_BOLD, false, HAlign::Center);
        num.valign = VAlign::Center;
        const float tw = w - 42.f;
        for (int k = 0; k < kSteps; ++k) {
            const std::wstring& step = Loc::Get("saveeditor_switch_step" + std::to_string(k + 1));
            const float th = std::max(28.f, TabKit::TextH(ui, step, text, tw));
            if (draw) {
                M::Motion::Enter enter(ui, Reveal(ui, row), 0.f, 14.f);
                ui.g.FillEllipse(x + 14.f, y + 14.f, 14.f, 14.f, kSwitchRed.Fade(0.16f));
                ui.g.Text(std::to_wstring(k + 1), { x, y, 28.f, 28.f }, TabKit::Readable(kSwitchRed), num);
                ui.g.Text(step, { x + 42.f, y + (th > 28.f ? 0.f : 6.f), tw, th }, p.foreground.Fade(0.85f), text);
                if (k + 1 < kSteps) ui.g.FillRect({ x + 13.f, y + 31.f, 2.f, th - 6.f + kStepGap - 8.f }, kSwitchRed.Fade(0.2f));
            }
            y += th + kStepGap;
            ++row;
        }
    }
    return y - card.y;
}

void SaveEditorSwitch::DrawCard(UI::Ui& ui, const Rect& card) {
    Frame(ui, card);
    const M::Palette& p = M::P();

    // Header: the console's icon, the title, close
    {
        M::Motion::Enter enter(ui, Reveal(ui, 0), 0.f, -10.f);
        const Rect header{ card.x, card.y, card.w, kTop - 14.f };
        ui.g.PushRoundClip(card, kRadius);
        ui.g.FillLinear(header, 0.f, { { 0.f, kSwitchRed.Fade(0.24f) }, { 0.7f, Colors::Transparent } }, { header.x, header.y }, { header.R(), header.y });
        ui.g.PopLayer();
        const float cy = header.CY(), ax = header.x + kPad + 26.f;
        ui.g.FillEllipse(ax, cy, 26.f, 26.f, kSwitchRed);
        M::DrawIcon(ui.g, M::Icon::NintendoSwitch, ax, cy, 28.f, Colors::White);
        const float bh = 38.f;
        const Rect closeR{ header.R() - kPad - bh + 8.f, cy - bh * 0.5f, bh, bh };
        if (M::IconButton(ui, ui.MakeId("ses-close"), closeR, M::Icon::Close, 20.f, M::IconButtonLook(), true)) Close();
        TextStyle title = Style(20.f, DWRITE_FONT_WEIGHT_BOLD);
        title.valign = VAlign::Center;
        title.ellipsis = true;
        TextStyle sub = Style(12.f);
        sub.valign = VAlign::Center;
        sub.ellipsis = true;
        const float tx = ax + 42.f, tw = std::max(0.f, closeR.x - 12.f - tx);
        ui.g.Text(Loc::Get("saveeditor_switch_title"), { tx, cy - 24.f, tw, 28.f }, p.foreground, title);
        ui.g.Text(Loc::Get("saveeditor_switch_subtitle"), { tx, cy + 6.f, tw, 18.f }, p.foreground.Fade(0.6f), sub);
    }

    Body(ui, card, true);

    // Footer: Cancel, and pick the file
    M::Motion::Enter enter(ui, Reveal(ui, 12), 0.f, 10.f);
    const float by = card.B() - kPad - 40.f;
    const std::wstring& pick = Loc::Get("saveeditor_switch_pick_button");
    const float pw = TabKit::PillWidth(ui, pick) + 10.f;
    const Rect pickR{ card.R() - kPad - pw, by, pw, 40.f };
    if (TabKit::PillButton(ui, ui.MakeId("ses-pick"), pickR, M::Icon::FolderOpen, pick, kSwitchRed, true, !tab.busy)) {
        Close();
        tab.PickSwitchSave();
        return;
    }
    const std::wstring& cancel = Loc::Get("cancel");
    const float cw = TabKit::PillWidth(ui, cancel);
    if (TabKit::PillButton(ui, ui.MakeId("ses-cancel"), { pickR.x - 10.f - cw, by, cw, 40.f }, M::Icon::Close, cancel, p.foreground.Fade(0.6f), false))
        Close();
}

} // namespace SonicRacingSaveManager::Main
