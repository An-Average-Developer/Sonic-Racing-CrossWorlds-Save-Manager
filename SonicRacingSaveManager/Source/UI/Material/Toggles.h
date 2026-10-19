#pragma once
#include <string>
#include "UI/Material/Theme.h"
#include "UI/Ui.h"

// MaterialDesign toggles: MD3 switch (MaterialDesign3.ToggleButton.xaml), CheckBox and RadioButton.
namespace SonicRacingSaveManager::UI::Material {

// MaterialDesignSwitchToggleButton (MD3): 52x32 track, thumb 16 -> 24 sliding 20 px with a BackEase,
// check icon on the thumb while on. Draws centered in r; returns true when clicked (caller flips).
bool Switch(Ui& ui, Id id, const Rect& r, bool on, bool enabled = true);
inline constexpr float kSwitchWidth = 52.f, kSwitchHeight = 32.f;

// MaterialDesignCheckBox: 18 px box + content (Padding 4,2,0,0). r is the whole row; returns true when clicked.
bool CheckBox(Ui& ui, Id id, const Rect& r, bool checked, const std::wstring& text, const TextStyle& style, Color foreground,
              bool enabled = true);
float CheckBoxWidth(Ui& ui, const std::wstring& text, const TextStyle& style);

// MaterialDesignRadioButton: 18 px circle + content. Returns true when clicked.
bool RadioButton(Ui& ui, Id id, const Rect& r, bool checked, const std::wstring& text, const TextStyle& style, Color foreground,
                 bool enabled = true);

} // namespace SonicRacingSaveManager::UI::Material
