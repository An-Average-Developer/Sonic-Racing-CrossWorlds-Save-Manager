#pragma once
#include <array>
#include <memory>
#include <string>
#include <vector>
#include "UI/Material/PackIcon.h"
#include "Windows/Main/Tabs/Tab.h"
#include "Windows/Main/Tabs/Tickets/TicketsKit.h"
#include "Windows/Main/Tabs/Tickets/TicketsState.h"

namespace SonicRacingSaveManager::Main {

class ValueAppliedOverlay;

// The "Tickets Menu" tab of MainWindow.xaml, redesigned like the Inazuma editor's Tool Menu: an
// animated banner with the connection orb, the status and Attach / Detach on top of both views.
// Menu view (SelectedTool "menu"): a control deck (search, Auto-Attach / your tickets, the
// manual process picker), a category rail that follows the scroll and the tool tiles (the patches,
// the Ticket Editor). Editor view ("tickets"): Find Tickets and the game values on the left, Edit
// Value on the right. Split:
//   TicketsTab.cpp          layout, the two views
//   TicketsConnection.cpp   the banner and its connection orb
//   TicketsDeck.cpp         the control deck (search, quick tiles, process picker)
//   TicketsMenu.cpp         the category rail, the tool list and its tools
//   TicketsKit              the tool tiles, section titles and grid
//   TicketsEditor.cpp       Find Tickets, the game values, Edit Value
//   ValueAppliedOverlay     "Value Applied Successfully!" (was the ValueAppliedDialog window)
class TicketsTab : public Tab {
public:
    explicit TicketsTab(MainWindow& window);
    ~TicketsTab() override;
    void Draw(UI::Ui& ui, const Rect& area) override;
    void OnSelected() override { loadedAt = -1.0; }
    void OnWindowOpened() override;

private:
    static constexpr int kSections = 2;
    struct Section {
        std::wstring title;
        UI::Material::Icon icon;
        std::vector<TicketsKit::ToolSpec> tools;
        int count = 0, active = 0;
    };

    void Banner(UI::Ui& ui, const Rect& r);
    void StatusOrb(UI::Ui& ui, float cx, float cy, float radius);
    // Menu view
    void MenuView(UI::Ui& ui, const Rect& area);
    float ControlDeck(UI::Ui& ui, const Rect& area, bool draw);
    void SearchBox(UI::Ui& ui, const Rect& r);
    void QuickTiles(UI::Ui& ui, const Rect& area);
    float ProcessPicker(UI::Ui& ui, const Rect& area, bool draw);
    void Rail(UI::Ui& ui, const Rect& r, const Section* sections, int current);
    std::vector<TicketsKit::ToolSpec> PatchTools(UI::Ui& ui);
    std::vector<TicketsKit::ToolSpec> EditorTools(UI::Ui& ui);
    // Editor view
    void FindTicketsCard(UI::Ui& ui, const Rect& r);
    void EditValueCard(UI::Ui& ui, const Rect& r);

    std::unique_ptr<TicketsState> state;
    std::unique_ptr<ValueAppliedOverlay> applied;
    double loadedAt = -1.0;
    std::wstring shownTool;          // the view switch replays the entrance
    std::wstring searchText = L"0";  // the "Tickets you have" box (bound to TicketSearchValue)
    std::wstring newValueText;       // the New Value box (bound to SelectedValue.NewValue)
    int32_t syncedNewValue = 0;      // the NewValue the box shows
    // Menu search box
    std::wstring search;
    double searchChangedAt = -10.0;
    // Scrolling: last frame's offset and where each section starts (content coordinates), for the
    // rail's scroll-spy; a rail click scrolls on the next frame
    float lastScroll = 0.f, lastMaxScroll = 0.f;
    std::array<float, kSections> sectionTop{};
    int scrollToSection = -1;
    float pickerOpen = 0.f;   // manual process picker (Auto-Attach off), animated once per frame
};

inline constexpr float kTicketsBannerH = 136.f;

} // namespace SonicRacingSaveManager::Main
