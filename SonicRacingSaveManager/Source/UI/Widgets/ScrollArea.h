#pragma once
#include "UI/Ui.h"

namespace SonicRacingSaveManager::UI {

struct ScrollState;

// Vertical (or horizontal) ScrollViewer. Construct it, draw the content in normal coordinates
// starting at `viewport.y` (the scope translates by the scroll offset), call SetContentSize with
// how tall/wide the content was, then let it go out of scope to pop the clip and draw the bar.
class ScrollArea {
public:
    enum class Bar {
        Thin,      // 6 px rounded thumb drawn over the content
        Classic,   // the stock WPF (Aero2) vertical scrollbar: 17 px, arrows, takes room from the content
        Material,  // MaterialDesignScrollBar: same layout, translucent (0.5, 0.8 on hover), square thumb
    };
    // Colors of the Material bar (MaterialDesign.Brush.ScrollBar.Foreground and the arrows' Foreground)
    static void SetMaterialColors(Color thumb, Color arrow);
    ScrollArea(Ui& ui, Id id, const Rect& viewport, bool horizontal = false, float wheelStep = 48.f, Bar bar = Bar::Thin);
    ~ScrollArea();
    ScrollArea(const ScrollArea&) = delete;
    ScrollArea& operator=(const ScrollArea&) = delete;

    void SetContentSize(float size) { contentSize = size; }
    float Offset() const { return offset; }
    // Width left for the content (a Classic bar takes 17 px while it's shown, like ScrollViewer Auto).
    float ContentWidth() const { return viewport.w - (barShown ? kClassicWidth : 0.f); }
    bool BarShown() const { return barShown; }
    static constexpr float kClassicWidth = 17.f;
    Rect viewport;

    static void ScrollTo(Ui& ui, Id id, float offset);   // e.g. 0 to reset when a page changes
    static void ScrollBy(Ui& ui, Id id, float delta);

private:
    void DrawClassic(ScrollState& s, float view);
    Ui& ui;
    Id id;
    bool horizontal;
    Bar bar;
    bool barShown = false;
    float offset = 0.f;
    float contentSize = 0.f;
};

} // namespace SonicRacingSaveManager::UI
