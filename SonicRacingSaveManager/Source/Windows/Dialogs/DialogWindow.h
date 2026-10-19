#pragma once
#include <windows.h>
#include <functional>
#include <string>
#include "Graphics/Color.h"
#include "UI/Ui.h"

// Dark modal dialog (the old WpfDialog): title bar, custom content, Close / Secondary / Primary
// buttons. Opened asynchronously — the owner is disabled until it closes — so it can be started
// from inside a frame without a nested message loop. Worker threads can use ShowBlocking.
namespace SonicRacingSaveManager::Dialogs {

enum class Result { Primary, Secondary, Close };

struct Options {
    std::wstring title;
    std::wstring primary;     // empty = no button
    std::wstring secondary;
    std::wstring close;
    float contentWidth = 360.f;
    float primaryMinWidth = 100.f;
    std::function<bool()> primaryEnabled;   // e.g. gated by a checkbox in the content
    bool live = false;                      // redraw continuously (progress text / bars)
};

// Draws the content inside `area` (top-left anchored) and returns the height it used.
using Content = std::function<float(UI::Ui& ui, const Rect& area)>;

// Returns an id usable with Close (progress dialogs the code closes itself).
int Show(HWND owner, const Options& options, Content content, std::function<void(Result)> done = {});
void Close(int id);   // closes without calling `done`
// Wrapped text content (TextBlock with TextWrapping=Wrap, MaxWidth = options.contentWidth).
Content Text(const std::wstring& text, Color color, float fontSize = 12.f);
void ShowText(HWND owner, const Options& options, const std::wstring& text, Color color, std::function<void(Result)> done = {});

// From a worker thread: shows the dialog on the UI thread and waits for the answer.
Result ShowBlocking(HWND owner, const Options& options, Content content);

bool AnyOpen();

} // namespace SonicRacingSaveManager::Dialogs
