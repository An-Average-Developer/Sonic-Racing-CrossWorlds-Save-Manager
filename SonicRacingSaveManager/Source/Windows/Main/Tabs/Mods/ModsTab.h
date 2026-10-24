#pragma once
#include <memory>
#include "Windows/Main/Tabs/Mods/ModsState.h"
#include "Windows/Main/Tabs/Tab.h"

namespace SonicRacingSaveManager::Main {

class ModUpdateOverlay;
class FileSelectionOverlay;

// The "Mod Manager" tab of MainWindow.xaml, redesigned: the banner over two columns. Left: the
// stats (Total / Enabled / Disabled), the game-running lock and the mods (the C#'s DataGrid, now
// cards: priority, status, name, version, the Update button, files and size), which can be dragged
// in priority mode. Right: the actions (Toggle Selected, Enable All, Disable All, Open Folder, Check
// Updates, Refresh) and the priority card (Select Mods Folder, Enable / Disable Priority, Move Up /
// Down). Split:
//   ModsTab.cpp        layout, banner, stats
//   ModsList.cpp       the mod rows and drag-and-drop
//   ModsSidebar.cpp    the actions and priority cards
//   ModUpdateOverlay / FileSelectionOverlay   the update cards (were the ModUpdateDialog and
//                      FileSelectionDialog windows)
class ModsTab : public Tab {
public:
    explicit ModsTab(MainWindow& window);
    ~ModsTab() override;
    void Draw(UI::Ui& ui, const Rect& area) override;
    void OnSelected() override;
    void OnWindowOpened() override;
    // The Mod Store installed a mod: scan the folder again (when the list was already loaded)
    void Rescan();
    // Selects the mod `name` (folder name without the priority) once it's in the list
    void Select(const std::wstring& name);

private:
    float Stats(UI::Ui& ui, const Rect& r);
    void List(UI::Ui& ui, const Rect& r);
    bool ModRow(UI::Ui& ui, const Rect& r, int index, float dragLift);
    void ReorderVeil(UI::Ui& ui, const Rect& r);
    float ActionsCard(UI::Ui& ui, const Rect& r, bool draw);
    float PriorityCard(UI::Ui& ui, const Rect& r, bool draw);

    std::unique_ptr<ModsState> state;
    std::unique_ptr<FileSelectionOverlay> files;
    std::unique_ptr<ModUpdateOverlay> update;
    double loadedAt = -1.0;

    // Drag-and-drop in priority mode
    int pressedIndex = -1;          // row the mouse went down on
    float pressX = 0.f, pressY = 0.f;
    bool dragging = false;
    float dragOffsetY = 0.f;        // mouse y minus the row's top when the drag began
    int dropIndex = -1;             // where the row would land

    // The "Reordering" veil stays a moment even when the renaming is instant
    unsigned veilReorders = 0;
    double veilUntil = -10.0;
};

} // namespace SonicRacingSaveManager::Main
