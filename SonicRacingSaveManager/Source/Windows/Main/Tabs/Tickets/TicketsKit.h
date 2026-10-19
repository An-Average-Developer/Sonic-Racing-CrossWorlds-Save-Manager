#pragma once
#include <functional>
#include <string>
#include <vector>
#include "Graphics/Graphics.h"
#include "UI/Material/PackIcon.h"
#include "UI/Ui.h"

// Building blocks of the Tickets Menu, as in the Inazuma editor's Tool Menu: every tool is a
// horizontal tile (icon badge, title + subtitle, its main control at the right end) with optional
// extra rows underneath. The tiles of a section sit in columns; each line of tiles stretches to its
// tallest one.
namespace SonicRacingSaveManager::Main::TicketsKit {

struct Row {
    float width = 0.f;          // 0 = full width
    float height = 0.f;
    Edges margin;
    std::function<void(const Rect&)> draw;   // gets the row's rect
};
using Rows = std::vector<Row>;

float RowsHeight(const Rows& rows);
// Draws the rows top to bottom from the top left of `area`
void DrawRows(const Rows& rows, const Rect& area);

// A switch bound to a command: the click flips it at once and runs `onClick`; it follows `on` again
// as soon as that value changes. A padlock sits before it while `locked`.
Row SwitchRow(UI::Ui& ui, UI::Id id, bool on, bool enabled, std::function<void()> onClick, bool locked = false, const std::wstring& tooltip = {});
// "> Open" (28 high, secondary background)
Row OpenButtonRow(UI::Ui& ui, UI::Id id, const std::wstring& label, std::function<void()> onClick, bool enabled = true);

struct ToolSpec {
    std::string key;                    // id seed (stable per tool)
    UI::Material::Icon icon = UI::Material::Icon::Star;
    std::wstring image;                 // picture in the badge instead of the icon (relative to Assets)
    std::wstring title, subtitle;
    bool visible = true;
    bool counted = true;                // counts as a tool (the guide tile doesn't)
    bool clickable = false;             // the whole tile opens something
    std::function<void()> onClick;
    bool active = false;                // the option is on: lit badge, accent bar, burst when it turns on
    bool wide = false;                  // spans two columns
    Row trailing;                       // main control at the right end of the title line
    std::function<Rows(float width)> details;   // extra rows under the title line
    std::wstring search;                // extra words for the search box (title + subtitle always count)
};

// Section title: a big faded index ("01"), the title in the section header colour, pills with the
// number of tools / options on, and a rule that draws itself in from `revealAt`. Returns the height.
float SectionTitle(UI::Ui& ui, float x, float y, float w, int number, const std::wstring& text, int count, int active, double revealAt);

// The tiles in columns of at least 330 px. Every tile slides in from the right (staggered from
// `revealAt`, `index` counts across calls), glides to its new place when the layout changes, lifts
// and sweeps a light over itself on hover, and lights up while active. Returns the height used.
float ToolGrid(UI::Ui& ui, float x, float y, float w, std::vector<ToolSpec>& tools, double revealAt, int& index);

// The search box's match (case-insensitive substring of title, subtitle and search words)
bool Matches(const ToolSpec& tool, const std::wstring& query);

} // namespace SonicRacingSaveManager::Main::TicketsKit
