#pragma once
#include <string>
#include "UI/Style.h"
#include "UI/Ui.h"

namespace SonicRacingSaveManager::UI {

struct TextBoxStyle {
    TextStyle text = Mono(11.f);
    Color foreground = Hex(0xCCEEFF);
    Color background = Hex(0x080C18);
    Color border = Hex(0x2A1050);
    Color focusBorder = Colors::Transparent;   // transparent = use the caret color
    Color caret = Hex(0x9944CC);
    Color selection = Rgba(0x33, 0x99, 0xFF, 0x66);
    Color placeholderColor = Hex(0x3A4A5A);
    Edges padding{ 8.f, 6.f };
    float radius = 0.f;
    bool multiline = false;
    HAlign align = HAlign::Left;   // single line: Center = HorizontalContentAlignment="Center" (while the text fits)
    bool password = false;
    size_t maxLength = 0;
    std::wstring placeholder;
};

// Editable text field. Returns true when the text changed this frame.
// `enterPressed` (optional) is set when Enter was pressed in a single-line box.
bool TextBox(Ui& ui, Id id, const Rect& r, std::wstring& value, const TextBoxStyle& style, bool* enterPressed = nullptr);
float TextBoxHeight(Ui& ui, const TextBoxStyle& style);  // single line
// Selects [anchor, caret) of the box `id` (editable ComboBox autocompletion)
void SetTextBoxSelection(Ui& ui, Id id, int anchor, int caret);

} // namespace SonicRacingSaveManager::UI
