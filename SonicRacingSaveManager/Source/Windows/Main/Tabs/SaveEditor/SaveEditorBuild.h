#pragma once
#include <string>
#include <vector>
#include "Services/SaveFile/AppSave.h"
#include "Windows/Main/Overlays/CardOverlay.h"

namespace SonicRacingSaveManager::Main {

class SaveEditorTab;

// A machine of the garage as an in-window card: its name and type (Race with it, Empty the slot) over
// a row per thing it is made of (front, rear, tyres, colour, horn, aura), each with everything the
// game has as pictures, the one on it first and marked; a click puts another on (a colour preset brings its
// colours, as the game does; a horn plays). What the save doesn't have yet shows dimmed with a lock,
// and putting it on unlocks it too (DLC, which Steam gives, isn't offered). Escape or a click outside
// closes it.
class SaveEditorBuild : public CardOverlay {
public:
    SaveEditorBuild(MainWindow& window, SaveEditorTab& editor) : CardOverlay(window), tab(editor) {}
    // `slot`: a garage slot ("DSH_102")
    void Show(const std::wstring& slot);

protected:
    float CardWidth() const override { return 820.f; }
    float CardHeight(UI::Ui& ui, float width) override;
    void DrawCard(UI::Ui& ui, const Rect& card) override;

private:
    // What a row offers: (id, name) of each choice, with the picture's list
    struct Choice {
        std::wstring id, name;
        bool owned = true;   // false: putting it on unlocks it in the save too
        bool dlc = false;    // DLC (only offered when the save has it)
    };
    struct Row {
        Services::SaveFile::BuildPart part;
        Services::SaveFile::ListId list;
        const char* key;
        std::vector<Choice> choices;
    };
    const std::vector<Row>& Rows(const Services::SaveFile::MachineBuild& b);
    // One row; returns its height
    float DrawRow(UI::Ui& ui, float x, float y, float w, const Row& row, const std::wstring& current, int index);

    SaveEditorTab& tab;
    std::wstring slot;
    std::vector<Row> rows;
    int rowsFor = -1;
};

} // namespace SonicRacingSaveManager::Main
