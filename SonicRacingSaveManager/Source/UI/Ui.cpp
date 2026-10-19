#include "UI/Ui.h"
#include <algorithm>
#include <cmath>
#include "Graphics/Animation.h"

namespace SonicRacingSaveManager::UI {

Id HashId(std::string_view text, Id seed) {
    Id h = seed;
    for (unsigned char c : text) {
        h ^= c;
        h *= 0x100000001b3ull;
    }
    return h ? h : 1;
}

Id HashId(int64_t value, Id seed) {
    Id h = seed;
    for (int i = 0; i < 8; ++i) {
        h ^= static_cast<uint8_t>(value >> (i * 8));
        h *= 0x100000001b3ull;
    }
    return h ? h : 1;
}

void Ui::BeginFrame(Input& in, float frameDt, double frameTime) {
    input = &in;
    dt = frameDt;
    time = frameTime;
    ++frame;
    animating = false;
    animatingThrottled = false;
    nextHot = 0;
    nextWheel = 0;
    cursorWanted = nullptr;
    idStack.clear();
    clips.clear();
    transforms.clear();
    current = Transform{};
    deferred.clear();
    // A press on empty space takes keyboard focus away from text boxes
    if (in.pressed[0] && hot == 0) focus = 0;
}

void Ui::EndFrame() {
    // Deferred items may defer more items; run until settled
    for (size_t i = 0; i < deferred.size(); ++i) {
        auto fn = std::move(deferred[i]);
        fn();
    }
    deferred.clear();

    hot = input && input->mouseInside ? nextHot : 0;
    if (hot != tooltipId) tooltipId = 0;   // the next hover starts the tooltip delay again
    wheelOwner = input && input->mouseInside ? nextWheel : 0;
    if (input && !input->down[0]) active = 0;
    cursorShown = cursorWanted;
    if (hot != lastHovered) lastHovered = hot;
}

Id Ui::MakeId(std::string_view name) const {
    return HashId(name, idStack.empty() ? 0xcbf29ce484222325ull : idStack.back());
}

Id Ui::MakeId(std::string_view name, int64_t index) const { return HashId(index, MakeId(name)); }

void Ui::PushId(std::string_view name) { idStack.push_back(MakeId(name)); }
void Ui::PushId(int64_t index) { idStack.push_back(HashId(index, idStack.empty() ? 0xcbf29ce484222325ull : idStack.back())); }
void Ui::PopId() {
    if (!idStack.empty()) idStack.pop_back();
}

// ── Transforms / clips ──────────────────────────────────────────────────────

void Ui::ToLocal(float sx, float sy, float& lx, float& ly) const {
    lx = (sx - current.tx) / current.scale;
    ly = (sy - current.ty) / current.scale;
}

Rect Ui::ToScreen(const Rect& r) const {
    return { r.x * current.scale + current.tx, r.y * current.scale + current.ty, r.w * current.scale, r.h * current.scale };
}

float Ui::MouseX() const {
    float lx = 0, ly = 0;
    ToLocal(input->mouseX, input->mouseY, lx, ly);
    return lx;
}

float Ui::MouseY() const {
    float lx = 0, ly = 0;
    ToLocal(input->mouseX, input->mouseY, lx, ly);
    return ly;
}

bool Ui::InClip(float sx, float sy) const {
    for (const auto& c : clips)
        if (!c.Contains(sx, sy)) return false;
    return true;
}

Rect Ui::ScreenClip() const {
    Rect r{ -1e6f, -1e6f, 2e6f, 2e6f };
    for (const auto& c : clips) r = r.Intersect(c);
    return r;
}

void Ui::PushClip(const Rect& r) {
    clips.push_back(ToScreen(r));
    g.PushClip(r);
}

void Ui::PopClip() {
    if (clips.empty()) return;
    clips.pop_back();
    g.PopClip();
}

void Ui::PushScale(float scale, float cx, float cy) {
    transforms.push_back(current);
    // local point p maps to: (p - c) * scale + c, then the previous transform
    const float tx = cx - cx * scale, ty = cy - cy * scale;
    current.tx = current.tx + tx * current.scale;
    current.ty = current.ty + ty * current.scale;
    current.scale *= scale;
    g.PushTransform(D2D1::Matrix3x2F::Scale(scale, scale, D2D1::Point2F(cx, cy)));
}

void Ui::PushTranslate(float dx, float dy) {
    transforms.push_back(current);
    current.tx += dx * current.scale;
    current.ty += dy * current.scale;
    g.PushTransform(D2D1::Matrix3x2F::Translation(dx, dy));
}

void Ui::PopTransform() {
    if (transforms.empty()) return;
    current = transforms.back();
    transforms.pop_back();
    g.PopTransform();
}

// ── Hit testing ─────────────────────────────────────────────────────────────

bool Ui::MouseOver(const Rect& r) const {
    if (!input || !input->mouseInside) return false;
    if (!InClip(input->mouseX, input->mouseY)) return false;
    float lx = 0, ly = 0;
    ToLocal(input->mouseX, input->mouseY, lx, ly);
    return r.Contains(lx, ly);
}

Interaction Ui::Interact(Id id, const Rect& r, bool enabled, bool sounds) {
    Interaction result;
    const bool over = MouseOver(r);
    if (over) nextHot = enabled ? id : HashId(id, 0x51ED);  // disabled items still block what's below
    if (!enabled) return result;

    result.hovered = hot == id && over;
    if (result.hovered) {
        cursorWanted = IDC_HAND;
        uint64_t& lastFrame = Data<uint64_t>(HashId(id, 0x401E));
        if (lastFrame + 1 != frame) {
            result.entered = true;
            if (sounds && onHoverSound) onHoverSound();
        }
        lastFrame = frame;
        if (input->pressed[0]) active = id;
        if (input->released[1]) result.rightClicked = true;
        if (input->released[2]) result.middleClicked = true;
    }
    result.held = active == id && input->down[0];
    if (active == id && input->released[0] && over) {
        result.clicked = true;
        if (sounds && onClickSound) onClickSound();
    }
    return result;
}

void Ui::Block(const Rect& r, bool wheel) {
    if (MouseOver(r)) {
        nextHot = HashId(reinterpret_cast<int64_t>(this), 0xB10C);
        if (wheel) nextWheel = 0;
    }
}

void Ui::WheelTarget(Id id, const Rect& r) {
    if (MouseOver(r)) nextWheel = id;
}

// ── Values ──────────────────────────────────────────────────────────────────

float Ui::Animate(Id id, float target, float speed, float initial) {
    auto it = anims.find(id);
    if (it == anims.end()) it = anims.emplace(id, AnimSlot{ initial > -1e29f ? initial : target }).first;
    float& v = it->second.value;
    v = Anim::Approach(v, target, speed, dt);
    if (std::fabs(v - target) < 0.002f) v = target;
    else animating = true;
    return v;
}

float Ui::Tween(Id id, float target, float seconds, float initial) {
    auto it = anims.find(id);
    if (it == anims.end()) it = anims.emplace(id, AnimSlot{ initial > -1e29f ? initial : target }).first;
    float& v = it->second.value;
    v = Anim::MoveTowards(v, target, seconds > 0.f ? 1.f / seconds : 1e9f, dt);
    if (v != target) animating = true;
    return v;
}

void Ui::SetValue(Id id, float value) { anims[id].value = value; }

float& Ui::Slot(Id id, float initial) {
    auto it = slots.find(id);
    if (it == slots.end()) it = slots.emplace(id, initial).first;
    return it->second;
}

void Ui::Tooltip(Id id, const std::wstring& text) {
    if (!input || hot != id || text.empty()) return;
    constexpr double kInitialDelay = 0.4, kShowDuration = 5.0;   // WPF ToolTipService defaults
    if (tooltipId != id) {
        tooltipId = id;
        tooltipSince = time;
        tooltipDismissed = false;
    }
    // Moving the mouse before it appears restarts the delay; a click hides it until the next hover
    if (input->mouseMoved && time - tooltipSince < kInitialDelay) tooltipSince = time;
    if (input->pressed[0] || input->pressed[1]) tooltipDismissed = true;
    const double shown = time - tooltipSince;
    if (tooltipDismissed || shown >= kInitialDelay + kShowDuration) return;
    if (shown < kInitialDelay) {
        KeepAnimatingThrottled();   // wake up when the delay ends, even if the mouse stays still
        tooltipX = input->mouseX;
        tooltipY = input->mouseY;
        return;
    }
    KeepAnimatingThrottled();       // hide after the show duration
    const float mx = tooltipX, my = tooltipY;
    Defer([this, text, mx, my] {
        if (tooltipRenderer) {
            tooltipRenderer(g, text, mx, my);
            return;
        }
        // Default Windows tooltip look (the launcher never restyled it): Segoe UI 12, white, grey border
        TextStyle style;
        style.size = 12.f;
        style.font = Font::Ui;
        const TextSize size = g.Measure(text, style, 400.f);
        const float w = std::ceil(size.w) + 12.f, h = std::ceil(size.h) + 6.f;
        float x = mx, y = my + 20.f;   // below the cursor
        x = std::clamp(x, 0.f, std::max(0.f, g.Width() - w));
        if (y + h > g.Height()) y = my - h - 2.f;
        const Rect box{ std::round(x), std::round(y), w, h };
        g.Shadow(box, 0.f, 3.f, Rgba(0, 0, 0, 60));
        g.Border(box, 0.f, Hex(0xFFFFFF), Hex(0x767676), 1.f);
        TextStyle drawn = style;
        drawn.wrap = true;
        g.Text(text, box.Inset(6.f, 3.f), Hex(0x575757), drawn);
    });
}

bool Ui::KeyPressed(unsigned vk, bool consume) {
    if (!input) return false;
    for (auto& k : input->keys) {
        if (k.down && !k.handled && k.vk == vk) {
            if (consume) k.handled = true;
            return true;
        }
    }
    return false;
}

} // namespace SonicRacingSaveManager::UI
