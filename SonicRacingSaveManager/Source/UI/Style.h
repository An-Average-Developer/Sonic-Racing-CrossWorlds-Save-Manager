#pragma once
#include "Graphics/Color.h"
#include "Graphics/Graphics.h"
#include "Graphics/Rect.h"

// The launcher's classic colors and the control styles repeated all over the old XAML.
namespace SonicRacingSaveManager::UI {

namespace Palette {
inline constexpr Color Teal = Hex(0x00CCAA);
inline constexpr Color TealDim = Rgba(0, 204, 170, 120);
inline constexpr Color Label = Hex(0x8AAABB);        // regular setting labels
inline constexpr Color Dim = Hex(0x3A6A8A);          // unselected tabs / button text
inline constexpr Color Muted = Hex(0x2A5A7A);
inline constexpr Color Faint = Hex(0x1A3A55);
inline constexpr Color Hint = Hex(0x5A8AA0);
inline constexpr Color Question = Hex(0xA0C8D8);
inline constexpr Color Bright = Hex(0xD2DCE6);
inline constexpr Color Warning = Hex(0xE8A000);
inline constexpr Color Error = Hex(0xCC4422);
inline constexpr Color Backdrop = Argb(0xBB000000);
inline constexpr Color BackdropDark = Argb(0xCC000000);
inline constexpr Color PanelBg = Hex(0x09141E);
inline constexpr Color PanelBorder = Hex(0x153048);
inline constexpr Color HeaderBg = Hex(0x060F18);
inline constexpr Color Divider = Hex(0x0D2030);
inline constexpr Color ButtonBg = Hex(0x0A1825);
inline constexpr Color ButtonBorder = Hex(0x0D2535);
inline constexpr Color ControlBorder = Hex(0x1A3A55);
inline constexpr Color OptionBg = Hex(0x060F18);
inline constexpr Color SelectedBg = Rgba(0, 40, 30);
inline constexpr Color DangerBg = Hex(0x1A0606);
inline constexpr Color DangerBorder = Hex(0x440000);
inline constexpr Color DangerText = Hex(0xCC3333);
inline constexpr Color ConfirmBg = Hex(0x00503C);
inline constexpr Color ConfirmBorder = Hex(0x00B482);
inline constexpr Color OffText = Rgba(42, 90, 122);
} // namespace Palette

struct ButtonStyle {
    Color background = Palette::ButtonBg;
    Color border = Palette::ButtonBorder;
    float thickness = 1.f;
    float radius = 3.f;
    Edges padding{ 0.f };
    float minWidth = 0.f;
    float height = 0.f;      // 0 = from content
};

namespace Styles {
// <Button Height="36" MinWidth="120" CornerRadius="3" Background="#0A1825" BorderBrush="#0D2535"> (footer buttons)
inline ButtonStyle Footer() { return { Palette::ButtonBg, Palette::ButtonBorder, 1.f, 3.f, { 0.f }, 120.f, 36.f }; }
// ON / OFF toggles: Height="26" MinWidth="60" Padding="10,0"
inline ButtonStyle Toggle() { return { Palette::ButtonBg, Palette::ControlBorder, 1.f, 3.f, { 10.f, 0.f }, 60.f, 26.f }; }
// Key binding buttons: Height="30" MinWidth="130" CornerRadius="4" Padding="10,0"
inline ButtonStyle KeyBind() { return { Palette::ButtonBg, Palette::ControlBorder, 1.f, 4.f, { 10.f, 0.f }, 130.f, 30.f }; }
// Full-width list buttons: Padding="12,10" CornerRadius="4" Background="#060F18"
inline ButtonStyle Option() { return { Palette::OptionBg, Palette::ButtonBorder, 1.f, 4.f, { 12.f, 10.f }, 0.f, 0.f }; }
inline ButtonStyle OptionSelected() { return { Palette::SelectedBg, Palette::Teal, 1.f, 4.f, { 12.f, 10.f }, 0.f, 0.f }; }
inline ButtonStyle Danger(float minWidth = 110.f) { return { Palette::DangerBg, Palette::DangerBorder, 1.f, 3.f, { 16.f, 0.f }, minWidth, 36.f }; }
inline ButtonStyle Confirm(float minWidth = 110.f) { return { Palette::ConfirmBg, Palette::ConfirmBorder, 1.f, 3.f, { 16.f, 0.f }, minWidth, 36.f }; }
inline ButtonStyle Flat() { return { Colors::Transparent, Colors::Transparent, 0.f, 0.f, { 0.f }, 0.f, 0.f }; }
} // namespace Styles

// Text styles ("Cascadia Code, Consolas, Courier New" everywhere)
inline TextStyle Mono(float size, bool bold = false) {
    TextStyle s;
    s.size = size;
    s.font = Font::Mono;
    if (bold) s.weight = DWRITE_FONT_WEIGHT_BOLD;
    return s;
}
inline TextStyle IconFont(float size) {
    TextStyle s;
    s.size = size;
    s.font = Font::Icons;
    s.align = HAlign::Center;
    return s;
}

// Segoe MDL2 Assets glyphs used by the XAML
namespace Glyphs {
inline constexpr wchar_t Minimize = 0xE921;
inline constexpr wchar_t ChevronDown = 0xE70D;
inline constexpr wchar_t Settings = 0xE713;
inline constexpr wchar_t Play = 0xE768;
inline constexpr wchar_t Power = 0xE7E8;
inline constexpr wchar_t Bug = 0xE730;
inline constexpr wchar_t Education = 0xE714;   // tutorials
inline constexpr wchar_t Changelog = 0xE8A1;
inline constexpr wchar_t Gamepad = 0xE7FC;
inline constexpr wchar_t Keyboard = 0xE92E;
inline constexpr wchar_t Sync = 0xE74A;        // updates
inline constexpr wchar_t Download = 0xE774;    // GameBanana / globe
inline constexpr wchar_t Install = 0xECC5;
inline constexpr wchar_t Delete = 0xE74D;
inline constexpr wchar_t Save = 0xE74E;
inline constexpr wchar_t People = 0xE8BD;      // Discord identity
inline constexpr wchar_t Image = 0xEB9F;
inline constexpr wchar_t CheckMark = 0xE73E;
inline constexpr wchar_t Cancel = 0xE711;
inline constexpr wchar_t Info = 0xE946;
inline constexpr wchar_t Warning = 0xE7BA;
inline constexpr wchar_t Folder = 0xE8B7;
inline constexpr wchar_t Edit = 0xE70F;
inline constexpr wchar_t Copy = 0xE8C8;
inline constexpr wchar_t Refresh = 0xE72C;
inline constexpr wchar_t Back = 0xE72B;
inline constexpr wchar_t Forward = 0xE72A;
inline constexpr wchar_t Video = 0xE714;
} // namespace Glyphs

} // namespace SonicRacingSaveManager::UI
