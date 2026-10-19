#pragma once
#include <deque>
#include <optional>
#include <string>
#include "UI/Material/Theme.h"
#include "UI/Ui.h"

// MaterialDesign Badged, Snackbar and Separator.
namespace SonicRacingSaveManager::UI::Material {

// materialDesign:Badged with BadgePlacementMode TopRight: an 18 px pill centered on the content's
// top-right corner (FontSize 11, Padding 2, CornerRadius 9). Pops (1.4 -> 1 over 0.3 s) when the
// count changes. Draws nothing for 0 (IntToNullIfZeroConverter).
void Badge(Ui& ui, Id id, const Rect& content, int count, Color background);

// MaterialDesignLinearProgressBar: square track in Primary.Light, value in Primary.Mid (0..100).
// Indeterminate: a 30 % segment sweeping left to right. `fill` = a Foreground set in the XAML.
void ProgressBar(Ui& ui, const Rect& r, float percent, bool indeterminate = false, Color fill = Colors::Transparent);

// <Separator/> in a popup: 1 px line in Separators.Background.
void Separator(Ui& ui, float x, float y, float w);

// materialDesign:Snackbar + SnackbarMessageQueue(TimeSpan.FromSeconds(4)): one message at a time,
// revealed bottom-up over 0.3 s, shown 4 s, then hidden. Horizontally centered on `area`, resting
// on its bottom edge; MinWidth 288, MaxWidth 568, 16 px message margin, 14 px text.
class Snackbar {
public:
    void Enqueue(const std::wstring& message);
    void Draw(Ui& ui, const Rect& area);
    bool Visible() const { return current.has_value() || !queue.empty(); }

private:
    std::deque<std::wstring> queue;
    std::optional<std::wstring> current;
    double shownAt = -1.0;
};

} // namespace SonicRacingSaveManager::UI::Material
