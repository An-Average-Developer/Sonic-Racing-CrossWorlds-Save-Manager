#pragma once
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include "Models/ModInfo.h"
#include "Services/Mods/ModManager.h"
#include "Windows/Main/Overlays/CardOverlay.h"

namespace SonicRacingSaveManager::Main {

class FileSelectionOverlay;

// Features\ModManager\Views\ModUpdateDialog.xaml (+ ModUpdateDialogViewModel) as an in-window card:
// the mod, current → latest version, the status, Cancel / Download & Update; while it downloads, the
// progress (ring and text) instead of the buttons. With several files on GameBanana the file
// picker opens over it first.
class ModUpdateOverlay : public CardOverlay {
public:
    ModUpdateOverlay(MainWindow& window, FileSelectionOverlay& files);
    void Show(const Models::ModInfo& mod, const Services::Mods::ModManager& service);
    // Runs on success (UpdateCompleted → DialogResult = true)
    std::function<void(const Models::ModInfo& mod)> onUpdated;

protected:
    float CardWidth() const override { return 560.f; }
    float CardHeight(UI::Ui& ui, float width) override;
    void DrawCard(UI::Ui& ui, const Rect& card) override;
    bool Closable() const override { return !isDownloading; }

private:
    void StartUpdate();
    void Download(std::vector<Models::GameBananaFile> files);

    FileSelectionOverlay& files;
    std::optional<Models::ModInfo> mod;
    std::optional<Services::Mods::ModManager> service;
    std::wstring statusMessage;
    bool statusError = false;   // the status is an "Error: ..." one
    bool isDownloading = false, fetching = false;
    double progressPercentage = 0.0;
    std::wstring progressText;
};

} // namespace SonicRacingSaveManager::Main
