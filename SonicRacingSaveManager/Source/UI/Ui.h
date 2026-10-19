#pragma once
#include <windows.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "Graphics/Graphics.h"
#include "UI/Input.h"

// Immediate-mode UI: every frame the screens draw their widgets and ask, in the same call,
// whether they were hovered / clicked. Persistent bits (hover animations, scroll offsets,
// text box carets) live in per-id slots inside this context.
namespace SonicRacingSaveManager::UI {

using Id = uint64_t;

Id HashId(std::string_view text, Id seed = 0xcbf29ce484222325ull);
Id HashId(int64_t value, Id seed);

struct Interaction {
    bool hovered = false;   // mouse is over it (and nothing on top)
    bool held = false;      // left button went down on it and is still down
    bool clicked = false;   // left button released over it
    bool rightClicked = false;
    bool entered = false;   // hover started this frame
    bool middleClicked = false;
};

class Ui {
public:
    explicit Ui(Graphics& graphics) : g(graphics) {}
    Graphics& g;

    // ── Frame ────────────────────────────────────────────────────────────
    void BeginFrame(Input& input, float dt, double time);
    void EndFrame();
    Input& In() { return *input; }
    float Dt() const { return dt; }
    double Time() const { return time; }
    uint64_t FrameNumber() const { return frame; }
    // True when an animation is still running (the window keeps rendering).
    bool Animating() const { return animating; }
    void KeepAnimating() { animating = true; }
    // Ambient animation that doesn't need the full refresh rate (seasonal scenery): the window
    // keeps rendering at about 60 fps, like WPF's animation clock.
    bool AnimatingThrottled() const { return animatingThrottled; }
    void KeepAnimatingThrottled() { animatingThrottled = true; }

    // ── Ids ──────────────────────────────────────────────────────────────
    Id MakeId(std::string_view name) const;
    Id MakeId(std::string_view name, int64_t index) const;
    void PushId(std::string_view name);
    void PushId(int64_t index);
    void PopId();

    // ── Hit testing ──────────────────────────────────────────────────────
    Interaction Interact(Id id, const Rect& r, bool enabled = true, bool sounds = true);
    // Opaque area: anything drawn before it can't be hovered / clicked through it. With `wheel`
    // false the mouse wheel still reaches the scroll area underneath (content inside a list).
    void Block(const Rect& r, bool wheel = true);
    bool MouseOver(const Rect& r) const;   // geometric test (clip + transform aware)
    bool Hot(Id id) const { return hot == id; }
    bool Active(Id id) const { return active == id; }
    Id HotId() const { return hot; }
    bool AnyActive() const { return active != 0; }
    void SetCursor(LPCWSTR cursor) { cursorWanted = cursor; }
    LPCWSTR Cursor() const { return cursorShown; }
    // Local mouse position (after transforms)
    float MouseX() const;
    float MouseY() const;

    // Wheel routing for scroll areas
    void WheelTarget(Id id, const Rect& r);
    bool OwnsWheel(Id id) const { return wheelOwner == id; }

    // ── Clip / transform (drawing and hit testing) ───────────────────────
    void PushClip(const Rect& r);
    void PopClip();
    void PushScale(float scale, float cx, float cy);
    void PushTranslate(float dx, float dy);
    void PopTransform();
    Rect ScreenClip() const;
    // A local rect in window coordinates (e.g. to anchor a deferred dropdown inside a scroll area)
    Rect ToScreenRect(const Rect& r) const { return ToScreen(r); }

    // ── Animated / persistent values ─────────────────────────────────────
    // Exponential approach; `speed` ≈ 1 / time constant in seconds. First call starts at target
    // unless `initial` is given.
    float Animate(Id id, float target, float speed = 14.f, float initial = -1e30f);
    // Linear move at `1/seconds` per second (for 0..1 open/close tweens).
    float Tween(Id id, float target, float seconds, float initial = -1e30f);
    void SetValue(Id id, float value);
    float& Slot(Id id, float initial = 0.f);
    template <typename T>
    T& Data(Id id) {
        auto& p = objects[id];
        if (!p) p = std::make_shared<Holder<T>>();
        return static_cast<Holder<T>*>(p.get())->value;
    }

    // ── Keyboard ─────────────────────────────────────────────────────────
    Id Focus() const { return focus; }
    void SetFocus(Id id) { focus = id; caretStart = time; }
    void ClearFocus() { focus = 0; }
    double CaretStart() const { return caretStart; }
    void ResetCaret() { caretStart = time; }
    // Unhandled key-down this frame (marks it handled when consumed).
    bool KeyPressed(unsigned vk, bool consume = true);

    // ── Deferred drawing (dropdowns, tooltips) ───────────────────────────
    void Defer(std::function<void()> fn) { deferred.push_back(std::move(fn)); }
    // WPF ToolTip for the widget `id` (call after its Interact): shown under the cursor after the
    // mouse rests on it for 400 ms, hidden on click or after 5 s.
    void Tooltip(Id id, const std::wstring& text);

    // Button feedback hooks (hover / click sounds)
    std::function<void()> onHoverSound, onClickSound;
    // Draws a tooltip box (text, mouse position in window coordinates); the default is the stock
    // Windows look
    std::function<void(Graphics& g, const std::wstring& text, float mouseX, float mouseY)> tooltipRenderer;

private:
    struct Transform {
        float scale = 1.f, tx = 0.f, ty = 0.f;
    };
    struct AnimSlot {
        float value = 0.f;
    };
    struct HolderBase {
        virtual ~HolderBase() = default;
    };
    template <typename T>
    struct Holder : HolderBase {
        T value{};
    };

    bool InClip(float sx, float sy) const;
    void ToLocal(float sx, float sy, float& lx, float& ly) const;
    Rect ToScreen(const Rect& r) const;

    Input* input = nullptr;
    float dt = 0.f;
    double time = 0.0;
    uint64_t frame = 0;
    bool animating = false;
    bool animatingThrottled = false;

    std::vector<Id> idStack;
    Id hot = 0, nextHot = 0, active = 0, lastHovered = 0;
    Id wheelOwner = 0, nextWheel = 0;
    Id focus = 0;
    double caretStart = 0.0;
    LPCWSTR cursorWanted = nullptr, cursorShown = nullptr;
    std::vector<Rect> clips;         // screen space
    std::vector<Transform> transforms;
    Transform current;
    std::unordered_map<Id, AnimSlot> anims;
    std::unordered_map<Id, float> slots;
    std::unordered_map<Id, std::shared_ptr<HolderBase>> objects;
    std::vector<std::function<void()>> deferred;
    Id tooltipId = 0;
    double tooltipSince = 0.0;
    bool tooltipDismissed = false;
    float tooltipX = 0.f, tooltipY = 0.f;
};

} // namespace SonicRacingSaveManager::UI
