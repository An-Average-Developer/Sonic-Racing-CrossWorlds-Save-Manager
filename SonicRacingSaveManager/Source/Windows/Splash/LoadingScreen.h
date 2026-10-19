#pragma once
#include <string>
#include "Windows/Main/Overlays/Overlay.h"

namespace SonicRacingSaveManager::Main {

// The loading screen (new in the Sonic port, the same as the Inazuma editor's LoadingScreenOverlay):
// the theme's colours with wandering glows and rising sparks, a glass card in the middle (the
// manager's pictures cycling in a ring of turning arcs, the title, a shimmering progress bar driven
// by fixed steps over ~8 s, the step dots and the status) and the "this application is free"
// warning under it. Fades out at the end and
// hands over to MainWindow::LoadingFinished. Skipped when "Show Loading Screen" is off.
class LoadingOverlay : public Overlay {
public:
    using Overlay::Overlay;
    void Start(double now);
    void Hide() { visible = false; }
    bool Visible() const override { return visible; }
    void Draw(UI::Ui& ui, const Rect& screen) override;

private:
    void Advance(double now);
    float CardHeight() const;
    float DrawWarning(UI::Ui& ui, float centerX, float y, bool measureOnly);

    bool visible = true;
    double startedAt = -1.0;
    bool started = false;
    double progressClock = 0.0;   // the 16 ms DispatcherTimer
    double currentProgress = 0.0, targetProgress = 0.0;
    int step = 0;                 // steps reached; the status shows the last one ("Initializing..." before)
    double stepAt = -10.0;        // when the last step was reached (its dot pops)
};

} // namespace SonicRacingSaveManager::Main
