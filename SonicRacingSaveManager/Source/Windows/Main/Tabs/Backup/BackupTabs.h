#pragma once
#include "Windows/Main/Tabs/Tab.h"

namespace SonicRacingSaveManager::Main {

// "Create Backup" tab (MainWindow.xaml)
class CreateBackupTab : public Tab {
public:
    using Tab::Tab;
    void Draw(UI::Ui& ui, const Rect& area) override;
    void OnSelected() override { loadedAt = -1.0; }

private:
    double loadedAt = -1.0;
};

// "Restore" tab
class RestoreTab : public Tab {
public:
    using Tab::Tab;
    void Draw(UI::Ui& ui, const Rect& area) override;
    void OnSelected() override { loadedAt = -1.0; }

private:
    double loadedAt = -1.0;
};

// "Manage" tab
class ManageTab : public Tab {
public:
    using Tab::Tab;
    void Draw(UI::Ui& ui, const Rect& area) override;
    void OnSelected() override { loadedAt = -1.0; }

private:
    double loadedAt = -1.0;
};

} // namespace SonicRacingSaveManager::Main
