#include "Windows/Main/MainScreen/Bars.h"
#include <algorithm>
#include <cmath>
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "Seasonal/SeasonalDecorations.h"
#include "Seasonal/SeasonalTheme.h"
#include "Services/App/AppVersion.h"
#include "UI/Material/Buttons.h"
#include "UI/Material/Elevation.h"
#include "UI/Material/Feedback.h"
#include "UI/Material/Motion.h"
#include "UI/Material/PackIcon.h"
#include "UI/Material/Theme.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Tabs/Backup/BackupState.h"
#include "Windows/Main/Tabs/TabKit.h"
#include "Windows/Main/Overlays/HeaderPopups.h"
#include "Windows/Main/Overlays/ThemeTransition.h"

// The header, redesigned like the other C++ ports: a drifting gradient of the primary colour with a
// light sweeping over it, the app badge and title (version pill), the orange "Import Save Files"
// button and the floating buttons grouped in a glass dock (save folder, backup folder | language,
// settings, theme) next to the close button. Everything pops in once at startup; each button has
// its own hover animation.
namespace SonicRacingSaveManager::Main::Header {

namespace {

namespace M = UI::Material;

constexpr float kPi = 3.14159265f;
constexpr float kButton = 40.f, kButtonGap = 6.f, kDockPad = 6.f;

void BadgeArcs(UI::Ui& ui, const Rect& badge, float t, float hover);
void HoverRing(UI::Ui& ui, const Rect& r, float angleDeg, float alpha);

double& StartedAt() {
    static double t = -1.0;
    return t;
}

// Entrance of item `index` (staggered from the first frame)
float Pop(UI::Ui& ui, int index) { return M::Motion::Stagger(ui, StartedAt() + 0.15, index, 0.06f, 0.45f); }

// Background: gradient, two wandering glows, a light band every few seconds, the accent line
void DrawBackground(UI::Ui& ui, const Rect& header) {
    const M::Palette& p = M::P();
    const float t = static_cast<float>(ui.Time());
    M::Elevation(ui.g, header, 0.f, 4.f);
    const float drift = 0.2f * std::sin(t * 0.3f);
    ui.g.FillLinear(header, 0.f, { { 0.f, p.primaryDark.color }, { 0.5f, p.primaryMid.color }, { 1.f, Lerp(p.primaryMid.color, p.primaryDark.color, 0.6f) } },
                    { drift, 0.f }, { 1.f, 1.f });
    ui.g.PushClip(header);
    const float g1 = header.x + header.w * (0.3f + 0.15f * std::sin(t * 0.21f));
    ui.g.FillRadial({ g1 - 260.f, header.CY() - 120.f, 520.f, 240.f }, { { 0.f, p.primaryLight.color.Fade(0.35f) }, { 1.f, p.primaryLight.color.Fade(0.f) } });
    const float g2 = header.x + header.w * (0.75f + 0.12f * std::cos(t * 0.17f));
    ui.g.FillRadial({ g2 - 220.f, header.CY() - 110.f, 440.f, 220.f }, { { 0.f, p.secondaryMid.color.Fade(0.18f) }, { 1.f, p.secondaryMid.color.Fade(0.f) } });
    const float sweep = static_cast<float>(std::fmod(ui.Time(), 8.0) / 1.6);
    if (sweep < 1.f) {
        ui.KeepAnimating();
        const float sx = header.x - 200.f + (header.w + 400.f) * Anim::EaseInOutCubic(sweep);
        ui.g.PushTransform(D2D1::Matrix3x2F::Skew(-25.f, 0.f, D2D1::Point2F(sx, header.CY())));
        ui.g.FillLinear({ sx, header.y, 160.f, header.h }, 0.f,
                        { { 0.f, Colors::White.Fade(0.f) }, { 0.5f, Colors::White.Fade(0.12f) }, { 1.f, Colors::White.Fade(0.f) } }, { 0.f, 0.f }, { 1.f, 0.f });
        ui.g.PopTransform();
    }
    ui.g.PopClip();
    // Accent line along the bottom edge with a light running along it
    const Rect line{ header.x, header.B() - 2.f, header.w, 2.f };
    ui.g.FillRect(line, p.secondaryMid.color.Fade(0.55f));
    const float lx = header.x + std::fmod(t * 180.f, header.w + 260.f) - 260.f;
    ui.g.FillLinear({ lx, line.y, 260.f, line.h }, 0.f, { { 0.f, Colors::White.Fade(0.f) }, { 0.5f, Colors::White.Fade(0.85f) }, { 1.f, Colors::White.Fade(0.f) } },
                    { 0.f, 0.f }, { 1.f, 0.f });
    ui.KeepAnimatingThrottled();
}

// App badge (the save icon in a glass tile with a spinning ring) + title, tagline and version pill
void DrawTitle(UI::Ui& ui, const Rect& header) {
    const M::Palette& p = M::P();
    const Color fg = p.primaryMid.foreground;
    const float t = static_cast<float>(ui.Time());

    const Rect badge{ header.x + 18.f, header.CY() - 23.f, 46.f, 46.f };
    const UI::Id badgeId = ui.MakeId("header-badge");
    const float hover = M::Motion::Hover(ui, badgeId, ui.MouseOver(badge), false).hover;
    {
        const float k = Pop(ui, 0);
        M::Motion::Enter enter(ui, k, 0.f, -20.f);
        ui.g.FillRound(badge, 14.f, Colors::White.Fade(0.14f + 0.08f * hover));
        ui.g.StrokeRound(badge.Inset(0.5f), 13.5f, Colors::White.Fade(0.3f), 1.f);
        BadgeArcs(ui, badge, t, hover);
        ui.g.PushTransform(D2D1::Matrix3x2F::Rotation(-8.f * hover + 3.f * std::sin(t * 1.5f), D2D1::Point2F(badge.CX(), badge.CY())));
        M::DrawIcon(ui.g, M::Icon::ContentSave, badge.CX(), badge.CY(), 26.f * (1.f + 0.08f * hover), fg);
        ui.g.PopTransform();
    }

    const float x = badge.R() + 16.f;
    TextStyle title = M::Text(21.f, DWRITE_FONT_WEIGHT_BOLD);
    title.valign = VAlign::Top;
    TextStyle accent = title;
    const std::wstring first = L"SONIC RACING CROSSWORLDS", second = L"SAVE MANAGER";
    const float titleH = 28.f, captionH = 18.f;
    const float top = header.CY() - (titleH + captionH + 2.f) * 0.5f;
    {
        M::Motion::Enter enter(ui, Pop(ui, 1), 0.f, -24.f);
        const float firstW = std::ceil(ui.g.Measure(first, title).w);
        ui.g.Text(first, { x, top, firstW + 2.f, titleH }, fg, title);
        // "SAVE MANAGER" in the accent colour, glowing softly
        const Rect secondR{ x + firstW + 10.f, top, std::ceil(ui.g.Measure(second, accent).w) + 2.f, titleH };
        ui.g.GlowText(second, secondR, accent, p.secondaryMid.color, 10.f, 0.25f + 0.15f * std::sin(t * 2.f));
        ui.g.Text(second, secondR, Lerp(p.secondaryLight.color, Colors::White, 0.2f), accent);
    }
    {
        M::Motion::Enter enter(ui, Pop(ui, 2), 0.f, -24.f);
        TextStyle caption = M::Text(12.f);
        caption.valign = VAlign::Center;
        const std::wstring& tagline = Loc::Get("app_tagline");
        const float taglineW = std::ceil(ui.g.Measure(tagline, caption).w) + 2.f;
        const float cy = top + titleH + 2.f;
        ui.g.Text(tagline, { x, cy, taglineW, captionH }, fg.Fade(0.8f), caption);
        // Version pill
        TextStyle pill = M::Text(10.f, DWRITE_FONT_WEIGHT_BOLD);
        pill.align = HAlign::Center;
        pill.valign = VAlign::Center;
        const std::wstring version = AppVersion::DisplayVersion();
        const Rect pr{ x + taglineW + 8.f, cy + 1.f, std::ceil(ui.g.Measure(version, pill).w) + 16.f, captionH - 2.f };
        ui.g.FillRound(pr, pr.h * 0.5f, Colors::Black.Fade(0.22f));
        ui.g.StrokeRound(pr.Inset(0.5f), pr.h * 0.5f - 0.5f, Colors::White.Fade(0.25f), 1.f);
        ui.g.Text(version, pr, fg, pill);
    }
}

// A round glass button of the dock (the caller draws and animates its icon)
struct DockButton {
    Rect r;
    UI::Interaction it;
    float hover = 0.f, press = 0.f;
};

DockButton BeginButton(UI::Ui& ui, UI::Id id, const Rect& r, Color tint) {
    DockButton b;
    b.r = r;
    b.it = ui.Interact(id, r);
    if (b.it.hovered) ui.SetCursor(IDC_HAND);
    const M::Motion::Feedback fb = M::Motion::Hover(ui, id, b.it.hovered, b.it.held);
    b.hover = fb.hover;
    b.press = fb.press;
    if (b.hover > 0.f) ui.g.GlowRound(r, r.h * 0.5f, tint, 12.f, 0.55f * b.hover);
    ui.g.FillEllipse(r.CX(), r.CY(), r.w * 0.5f, r.h * 0.5f, Lerp(Colors::White.Fade(0.1f), tint, b.hover));
    ui.g.StrokeEllipse(r.CX(), r.CY(), r.w * 0.5f - 0.5f, r.h * 0.5f - 0.5f, Colors::White.Fade(0.25f + 0.35f * b.hover), 1.f);
    return b;
}

void DrawFabs(UI::Ui& ui, MainWindow& main, const Rect& header) {
    const M::Palette& p = M::P();
    const Color fg = p.primaryMid.foreground;
    const float t = static_cast<float>(ui.Time());
    const float cy = header.CY();
    const Color red = Rgba(0xE5, 0x39, 0x35);

    // Close, on its own at the right edge: the X turns a quarter on hover
    const Rect closeR{ header.R() - 18.f - 44.f, cy - 22.f, 44.f, 44.f };
    {
        M::Motion::Enter enter(ui, Pop(ui, 10), 0.f, 24.f);
        const UI::Id id = ui.MakeId("fab-close");
        const UI::Interaction it = ui.Interact(id, closeR);
        if (it.hovered) ui.SetCursor(IDC_HAND);
        const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, it.held);
        if (fb.hover > 0.f) ui.g.GlowRound(closeR, 22.f, red, 14.f, 0.7f * fb.hover);
        const float s = 1.f + 0.06f * fb.hover - 0.08f * fb.press;
        ui.g.FillEllipse(closeR.CX(), closeR.CY(), 22.f * s, 22.f * s, Lerp(red.Fade(0.85f), red, fb.hover));
        ui.g.PushTransform(D2D1::Matrix3x2F::Rotation(90.f * fb.hover, D2D1::Point2F(closeR.CX(), closeR.CY())));
        M::DrawIcon(ui.g, M::Icon::Close, closeR.CX(), closeR.CY(), 22.f, Colors::White);
        ui.g.PopTransform();
        if (it.clicked) main.Quit();
        ui.Tooltip(id, Loc::Get("header_close"));
    }

    // The dock: [save folder, backup folder] | [language, settings, theme]
    constexpr int count = 5;
    const float dockW = kDockPad * 2.f + kButton * count + kButtonGap * (count - 1) + 14.f;
    const Rect dock{ closeR.x - 14.f - dockW, cy - (kButton + kDockPad * 2.f) * 0.5f, dockW, kButton + kDockPad * 2.f };
    {
        M::Motion::Enter enter(ui, Pop(ui, 4), 0.f, 30.f);
        ui.g.FillRound(dock, dock.h * 0.5f, Colors::Black.Fade(0.18f));
        ui.g.StrokeRound(dock.Inset(0.5f), dock.h * 0.5f - 0.5f, Colors::White.Fade(0.22f), 1.f);
    }
    float x = dock.x + kDockPad;
    int index = 5;
    const auto Next = [&] {
        const Rect r{ x, dock.y + kDockPad, kButton, kButton };
        x += kButton + kButtonGap;
        return r;
    };

    // Import Save Files (Background #FF6B00): a pill left of the dock, the icon nudging up on hover
    {
        const std::wstring& label = Loc::Get("header_import_saves");
        const float w = Main::TabKit::PillWidth(ui, label);
        const Rect r{ dock.x - 14.f - w, cy - 22.f, w, 44.f };
        M::Motion::Enter enter(ui, Pop(ui, 3), 0.f, 30.f);
        const UI::Id id = ui.MakeId("fab-import");
        if (Main::TabKit::PillButton(ui, id, r, M::Icon::FileImport, label, Rgba(0xFF, 0x6B, 0x00), true)) main.Backups().ImportFilesDirectly();
        ui.Tooltip(id, Loc::Get("header_import_saves_tip"));
    }

    // Open Save Folder / Open Backup Folder: the folder pops open on hover
    const struct {
        const char* key;
        M::Icon icon, hoverIcon;
        const char* tip;
        bool saves;
    } folders[] = { { "fab-saves", M::Icon::Folder, M::Icon::FolderOpen, "header_save_folder", true },
                    { "fab-backups", M::Icon::FolderMultiple, M::Icon::FolderOpen, "header_backup_folder", false } };
    for (const auto& f : folders) {
        const Rect r = Next();
        M::Motion::Enter enter(ui, Pop(ui, index++), 8.f);
        const UI::Id id = ui.MakeId(f.key);
        const DockButton b = BeginButton(ui, id, r, p.secondaryMid.color);
        const float s = 1.f + 0.1f * b.hover - 0.08f * b.press;
        ui.g.PushTransform(D2D1::Matrix3x2F::Scale(s, s, { r.CX(), r.CY() }) * D2D1::Matrix3x2F::Translation(0.f, -2.f * b.hover));
        M::DrawIcon(ui.g, b.hover > 0.5f ? f.hoverIcon : f.icon, r.CX(), r.CY(), 22.f, fg);
        ui.g.PopTransform();
        if (b.it.clicked) {
            if (f.saves) main.Backups().OpenSaveFolder();
            else main.Backups().OpenBackupFolder();
        }
        ui.Tooltip(id, Loc::Get(f.tip));
    }
    // Separator
    ui.g.FillRect({ x + 1.f, dock.y + 12.f, 1.f, dock.h - 24.f }, Colors::White.Fade(0.25f));
    x += 14.f;

    // Language: the current flag on a little rolled banner that sways on hover
    {
        const Rect r = Next();
        M::Motion::Enter enter(ui, Pop(ui, index++), 8.f);
        const UI::Id id = ui.MakeId("fab-language");
        const DockButton b = BeginButton(ui, id, r, p.secondaryMid.color);
        const float sway = 8.f * b.hover * std::sin(t * 5.f);
        if (b.hover > 0.f) ui.KeepAnimating();
        const float s = 1.f + 0.08f * b.hover - 0.06f * b.press;
        ui.g.PushTransform(D2D1::Matrix3x2F::Rotation(sway, D2D1::Point2F(r.CX(), r.CY())));
        LanguagePopup::Banner(ui, { r.CX() - 15.f * s, r.CY() - 10.f * s, 30.f * s, 20.f * s }, LanguagePopup::Flag(Loc::CurrentLang()), b.hover, 0.f);
        ui.g.PopTransform();
        if (b.it.clicked) main.Languages().Toggle(r);
        ui.Tooltip(id, Loc::Get("header_language"));
    }
    // Settings: the cog keeps turning while hovered
    {
        const Rect r = Next();
        M::Motion::Enter enter(ui, Pop(ui, index++), 8.f);
        const UI::Id id = ui.MakeId("fab-settings");
        const DockButton b = BeginButton(ui, id, r, p.secondaryMid.color);
        float& angle = ui.Slot(ui.MakeId("fab-settings-angle"));
        angle = std::fmod(angle + ui.Dt() * 200.f * b.hover, 360.f);
        if (b.hover > 0.f) ui.KeepAnimating();
        ui.g.PushTransform(D2D1::Matrix3x2F::Rotation(angle, D2D1::Point2F(r.CX(), r.CY())));
        M::DrawIcon(ui.g, M::Icon::Cog, r.CX(), r.CY(), 22.f, fg);
        ui.g.PopTransform();
        if (b.it.clicked) main.SettingsMenu().Toggle(r);
        ui.Tooltip(id, Loc::Get("header_settings"));
    }
    // Toggle Theme: the icon spins and swaps while the ring transition runs, tilts on hover. The
    // seasonal themes are dark only (as in the Inazuma editor): there the button is grey with a
    // lock, and a click shakes it and says why in the status bar.
    {
        const Rect r = Next();
        M::Motion::Enter enter(ui, Pop(ui, index++), 8.f);
        const UI::Id id = ui.MakeId("fab-theme");
        const bool locked = Seasonal::IsSeasonal();
        const Color grey = Rgba(0x9E, 0x9E, 0x9E);
        const DockButton b = BeginButton(ui, id, r, locked ? grey : p.secondaryMid.color);
        double& shakeAt = ui.Data<double>(ui.MakeId("fab-theme-shake"));
        if (locked && b.it.clicked) {
            shakeAt = ui.Time();
            main.State().statusMessage = Loc::Get("header_theme_locked_message");
        }
        const float since = static_cast<float>(ui.Time() - shakeAt);
        const float shake = since >= 0.f && since < 0.5f ? 9.f * (1.f - since / 0.5f) * std::sin(since * 45.f) : 0.f;
        if (shake != 0.f) ui.KeepAnimating();
        const ThemeTransition& tt = *ThemeTransition::Of(main);
        const float scale = tt.IconScale(ui.Time()), angle = tt.IconAngle(ui.Time()) + (locked ? 0.f : 25.f * b.hover);
        if (scale > 0.001f) {
            ui.g.PushTransform(D2D1::Matrix3x2F::Scale(scale, scale, { r.CX(), r.CY() }) * D2D1::Matrix3x2F::Rotation(angle, { r.CX(), r.CY() }) *
                               D2D1::Matrix3x2F::Translation(shake, 0.f));
            M::DrawIcon(ui.g, tt.Icon(), r.CX(), r.CY(), 22.f, locked ? fg.Fade(0.45f) : fg);
            ui.g.PopTransform();
        }
        if (locked) {
            // Lock badge on the lower right
            const float lx = r.R() - 7.f + shake, ly = r.B() - 7.f;
            ui.g.FillEllipse(lx, ly, 8.f, 8.f, Rgba(0x61, 0x61, 0x61));
            ui.g.StrokeEllipse(lx, ly, 8.f, 8.f, Colors::White.Fade(0.5f), 1.f);
            M::DrawIcon(ui.g, M::Icon::Lock, lx, ly, 10.f, Colors::White);
        } else if (b.it.clicked && !main.ThemeAnimating()) {
            main.ToggleTheme(r);
        }
        ui.Tooltip(id, Loc::Get(locked ? "header_theme_locked" : "header_theme"));
    }
}

// Two arcs turning around the badge, faster on hover
void BadgeArcs(UI::Ui& ui, const Rect& badge, float t, float hover) {
    const float cx = badge.CX(), cy = badge.CY(), radius = badge.w * 0.5f + 5.f;
    const float speed = 40.f + 160.f * hover;
    for (int i = 0; i < 2; ++i) {
        const float start = (t * speed + 180.f * static_cast<float>(i)) * kPi / 180.f;
        const float sweep = 55.f * kPi / 180.f;
        const int steps = 10;
        for (int s = 0; s < steps; ++s) {
            const float a0 = start + sweep * s / steps, a1 = start + sweep * (s + 1) / steps;
            ui.g.Line(cx + radius * std::sin(a0), cy - radius * std::cos(a0), cx + radius * std::sin(a1), cy - radius * std::cos(a1),
                      M::P().secondaryMid.color.Fade(0.4f + 0.5f * hover), 2.f, true);
        }
    }
}

// A dashed ring turning around a dock button (hover)
void HoverRing(UI::Ui& ui, const Rect& r, float angleDeg, float alpha) {
    const float cx = r.CX(), cy = r.CY(), radius = r.w * 0.5f + 3.f;
    for (int i = 0; i < 6; ++i) {
        const float a0 = (angleDeg + 60.f * i) * kPi / 180.f, a1 = a0 + 0.45f;
        ui.g.Line(cx + radius * std::sin(a0), cy - radius * std::cos(a0), cx + radius * std::sin(a1), cy - radius * std::cos(a1),
                  M::P().secondaryMid.color.Fade(0.9f * alpha), 2.f, true);
    }
    ui.KeepAnimating();
}

} // namespace

void Draw(UI::Ui& ui, MainWindow& main, const Rect& header) {
    if (StartedAt() < 0.0) StartedAt() = ui.Time();
    DrawBackground(ui, header);

    // Dragging the window by the header (Header_MouseLeftButtonDown → DragMove)
    const UI::Id dragId = ui.MakeId("header-drag");
    const UI::Interaction drag = ui.Interact(dragId, header, true, false);
    if (drag.hovered) {
        ui.SetCursor(IDC_ARROW);
        if (ui.In().pressed[0]) main.BeginDrag();
    }

    DrawTitle(ui, header);
    // seasonal:SeasonalDecorations.Decoration="Header"
    Seasonal::Decorations::Header(ui.g, header, Seasonal::LastAnimationTime());
    DrawFabs(ui, main, header);
}

} // namespace SonicRacingSaveManager::Main::Header
