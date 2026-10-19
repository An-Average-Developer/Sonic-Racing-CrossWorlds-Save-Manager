#pragma once
#include <string>
#include <vector>
#include "Windows/Main/Tabs/Tab.h"
#include "Windows/Main/Tabs/Updates/ChangelogCards.h"

namespace SonicRacingSaveManager::Main {

// Features\Updates\Views\ChangelogsTabControl.xaml, redesigned like the Reports tab: the animated
// banner over two columns, the whole history as folding version cards on the left and, on the
// right, the history at a glance (versions, changes, installed / latest) and the changes by type
// as growing bars.
class ChangelogsTab : public Tab {
public:
    using Tab::Tab;
    void Draw(UI::Ui& ui, const Rect& area) override;
    void OnSelected() override { loadedAt = -1.0; }

private:
    struct TypeCount {
        std::wstring title;
        int count = 0;
    };
    void Summarize();
    float Glance(UI::Ui& ui, const Rect& r, bool draw);
    void ByType(UI::Ui& ui, const Rect& r);

    double loadedAt = -1.0;   // entrance of the whole page
    std::wstring text;        // the embedded changelog, read once
    ChangelogCards cards{ "chl-history" };
    // History summary (from the embedded changelog)
    bool summarized = false;
    int versions = 0, changes = 0;
    std::wstring firstVersion, latestVersion, latestLabel;
    std::vector<TypeCount> types;   // most changes first
};

} // namespace SonicRacingSaveManager::Main
