#pragma once
#include <functional>
#include <string>
#include "Models/BackupInfo.h"
#include "Models/SaveAccount.h"
#include "UI/Material/PackIcon.h"
#include "UI/Ui.h"

// Pieces shared by the Create Backup, Restore and Manage tabs: the banner + two columns layout, the
// account and backup rows (the C#'s ComboBox items and DataGrid rows, now selectable cards) and the
// scrolling list holding them.
namespace SonicRacingSaveManager::Main::BackupKit {

inline constexpr float kRightW = 360.f, kGap = 16.f, kRadius = 16.f;
inline constexpr float kAccountRowH = 64.f, kBackupRowH = 64.f, kRowGap = 8.f;

struct Columns {
    Rect banner, left, right;
};
// The animated banner on top, the main card on the left, the help / status cards on the right
Columns Layout(const Rect& area);

// Card heading: bold caps in the primary colour, an optional count pill on the right. Returns 26.
float CardTitle(UI::Ui& ui, const Rect& r, const std::wstring& title, const std::wstring& pill = L"");
// A hint line under a heading (wrapping). Returns its height.
float Hint(UI::Ui& ui, const Rect& r, const std::wstring& text, Color color = Colors::Transparent);

// Selectable rows; true when clicked. The selected row is tinted, glows, shows a check and sits 5 px
// to the right (the C#'s selected ListBoxItem).
bool AccountRow(UI::Ui& ui, UI::Id id, const Rect& r, const Models::SaveAccount& account, bool selected);
bool BackupRow(UI::Ui& ui, UI::Id id, const Rect& r, const Models::BackupInfo& backup, const std::wstring& dateText, bool selected);

// A scrolling list of `count` rows `rowH` tall that slide in one by one from `revealAt`; `row` draws
// row i in its rect. The list scrolls back to the top when `revealAt` changes.
void List(UI::Ui& ui, UI::Id id, const Rect& r, int count, float rowH, double revealAt, const std::function<void(int, const Rect&)>& row);

// When a list's rows slide in: `base` (the page's entrance), or the moment the lists were last
// reloaded (`revision` changed) when that's later
double ListReveal(UI::Ui& ui, const char* key, unsigned revision, double base);

// Nothing to list: a faded icon, a title and a message, centered in `r`
void EmptyState(UI::Ui& ui, const Rect& r, UI::Material::Icon icon, const std::wstring& title, const std::wstring& message);

// "Label  value" in a soft rounded box (the selected item's details)
void InfoChip(UI::Ui& ui, const Rect& r, UI::Material::Icon icon, const std::wstring& label, const std::wstring& value);

// BackupCards.cpp — the right column's "selected item" cards. With `draw` false they only measure.
// The account card's button opens the save folder.
float AccountCard(UI::Ui& ui, const Rect& r, const Models::SaveAccount* account, double revealAt, const std::function<void()>& openFolder, bool draw);
float BackupCard(UI::Ui& ui, const char* key, const Rect& r, const Models::BackupInfo* backup, double revealAt, bool draw);

// "{n} file" / "{n} files"
std::wstring FilesText(int count);

} // namespace SonicRacingSaveManager::Main::BackupKit
