#pragma once
#include <functional>
#include <string>
#include <vector>
#include "UI/Material/Theme.h"
#include "UI/Ui.h"

// MaterialDesignTextBox / MaterialDesignComboBox (underlined, the MD3 Defaults) and their
// MaterialDesignOutlined* variants.
namespace SonicRacingSaveManager::UI::Material {

struct FieldStyle {
    float fontSize = 12.f;   // the window's default FontSize unless the XAML set one
    HAlign align = HAlign::Left;
    std::wstring tooltip;
    bool outlined = false;   // MaterialDesignOutlinedTextBox / ComboBox: 1 px border, CornerRadius 4
    Edges padding{ -1.f };   // -1 = the style's (underlined 0,4,0,4 / outlined 16,16,12,16)
    Color background = Colors::Transparent;
    // HintAssist.Hint: floats onto the outline (FloatingScale .75) on the outlined styles once the
    // field is focused or has a value; elsewhere a placeholder shown while it's empty
    std::wstring hint;
    bool multiline = false;  // AcceptsReturn + TextWrapping=Wrap (TextField only; give it the XAML Height)
    Font font = Font::Ui;                       // FontFamily (TextField only)
    Color foreground = Colors::Transparent;     // Foreground (TextField only; transparent = MaterialDesignBody)
};

// Field height: padding + one text line + the underline / outline
float FieldHeight(Ui& ui, float fontSize);
float FieldHeight(Ui& ui, const FieldStyle& style);

// Text box. Edits `text` in place; returns true on every change (UpdateSourceTrigger=PropertyChanged).
bool TextField(Ui& ui, Id id, const Rect& r, std::wstring& text, const FieldStyle& style = {}, bool enabled = true);

// <TextBox Text="{Binding X}"/> with the default UpdateSourceTrigger (LostFocus): the box edits its
// own copy and hands it to `commit` when the focus leaves; while unfocused it shows `current`.
void BoundTextField(Ui& ui, Id id, const Rect& r, const std::wstring& current, const std::function<void(const std::wstring&)>& commit,
                    const FieldStyle& style = {}, bool enabled = true);

// Items of a ComboBox: their text (shown / searched) and, optionally, an ItemTemplate.
struct ComboItems {
    int count = 0;
    std::function<std::wstring(int)> text;
    std::function<void(int index, const Rect& r, bool inDropDown)> draw;   // null = the text
    float itemHeight = 0.f;                                                 // 0 = one text line
};

// ComboBox with a dropdown list (drawn on top, scrollable, closes on any click outside).
// `editable` = IsEditable + IsTextSearchEnabled: typing selects (and completes) the first item
// starting with the text, or clears the selection. Returns true when the selection changed.
bool ComboBox(Ui& ui, Id id, const Rect& r, int& selected, const ComboItems& items, const FieldStyle& style = {}, bool enabled = true,
              float maxDropHeight = 300.f, bool editable = false);
bool ComboBox(Ui& ui, Id id, const Rect& r, int& selected, const std::vector<std::wstring>& items, const FieldStyle& style = {},
              bool enabled = true, float maxDropHeight = 300.f);

} // namespace SonicRacingSaveManager::UI::Material
