#pragma once
#include <string>
#include <vector>
#include "UI/Style.h"
#include "UI/Ui.h"

namespace SonicRacingSaveManager::UI {

// Horizontal slider. Returns true while the value changes.
bool Slider(Ui& ui, Id id, const Rect& r, float& value, float minimum, float maximum, float step = 1.f);

// ComboBox styled like the Cores tab one. Returns true when the selection changed.
bool ComboBox(Ui& ui, Id id, const Rect& r, int& selected, const std::vector<std::wstring>& items);

// Determinate progress bar (value 0..1).
void ProgressBar(Ui& ui, const Rect& r, float value, Color fill = Palette::Teal, Color track = Hex(0x0D2535));
// Indeterminate (marquee) progress bar.
void ProgressBarIndeterminate(Ui& ui, const Rect& r, Color fill = Palette::Teal, Color track = Hex(0x0D2535));

// Horizontal divider line (<Border BorderThickness="0,0,0,1" BorderBrush="#0D2030"/>).
void Divider(Ui& ui, float x, float y, float w, Color c = Palette::Divider);

// Section caption (FontSize 9, bold, teal).
void SectionLabel(Ui& ui, const Rect& r, const std::wstring& text, Color c = Palette::Teal);

// Label on the left + control on the right, the "<Grid><TextBlock/><Button HorizontalAlignment=Right/></Grid>" pattern.
// Returns the rect for a control of `controlWidth` aligned to the right.
Rect RightAligned(const Rect& row, float controlWidth, float controlHeight);

// Wrapped paragraph; returns its height.
float Paragraph(Ui& ui, float x, float y, float w, const std::wstring& text, const TextStyle& style, Color c);

// Small chip that can be toggled (hand mod submission chapters / colors). Returns true when clicked.
bool Chip(Ui& ui, Id id, const Rect& r, const std::wstring& text, bool selected, Color accent = Palette::Teal);

} // namespace SonicRacingSaveManager::UI
