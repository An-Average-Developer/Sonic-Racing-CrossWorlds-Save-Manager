#pragma once
#include <string>
#include "Services/SaveFile/AppSave.h"
#include "Windows/Main/Overlays/CardOverlay.h"

namespace SonicRacingSaveManager::Main {

class SaveEditorTab;

// A course of the Save Editor as an in-window card, in two steps under its photo, emblem and name:
//   - its five Red Star Rings (the same at every speed) over its speed classes (Normal, High, Sonic,
//     Super Sonic, Mirror), each with its Time Trial time (Sonic and Super Sonic)
//   - a speed class: its Grand Prix (opened, the result at that speed) and its Time Trial record (only
//     at Sonic and Super Sonic: the laps, which can be typed, the total, medal, who, when, the reward)
// The rings, the results, the medals and the rewards change with a click. Escape or a click outside closes it.
class SaveEditorCourse : public CardOverlay {
public:
    SaveEditorCourse(MainWindow& window, SaveEditorTab& editor) : CardOverlay(window), tab(editor) {}
    // `id`: an EStageId value name ("STG1016")
    void Show(const std::wstring& id);

protected:
    float CardWidth() const override { return 680.f; }
    float CardHeight(UI::Ui& ui, float width) override;
    void DrawCard(UI::Ui& ui, const Rect& card) override;
    Color Accent() const override;

private:
    void Header(UI::Ui& ui, const Rect& r, const Services::SaveFile::Course& c);
    void Speeds(UI::Ui& ui, const Rect& r, const Services::SaveFile::Course& c);
    void Detail(UI::Ui& ui, const Rect& r, const Services::SaveFile::Course& c);
    // The five rings with their count; returns the height used
    float Rings(UI::Ui& ui, float x, float y, float w, const Services::SaveFile::Course& c);
    void Pick(UI::Ui& ui, int speedClass);

    SaveEditorTab& tab;
    std::wstring id;
    int speed = -1;   // -1: the speed classes
    double stepAt = 0.0;
};

} // namespace SonicRacingSaveManager::Main
