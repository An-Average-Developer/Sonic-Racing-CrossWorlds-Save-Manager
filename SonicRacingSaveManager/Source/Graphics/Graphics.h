#pragma once
#include <windows.h>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include "Graphics/Color.h"
#include "Graphics/Device.h"
#include "Graphics/Fonts.h"
#include "Graphics/Images.h"
#include "Graphics/Rect.h"

namespace SonicRacingSaveManager {

enum class HAlign { Left, Center, Right, Justify };
enum class VAlign { Top, Center, Bottom };
enum class Stretch { Fill, Uniform, UniformToFill, None };

// Mirrors the TextBlock properties the XAML used.
struct TextStyle {
    float size = 12.f;
    Font font = Font::Ui;
    DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL;
    bool italic = false;
    HAlign align = HAlign::Left;
    VAlign valign = VAlign::Center;
    bool wrap = false;
    bool ellipsis = false;     // TextTrimming="CharacterEllipsis"
    bool underline = false;    // TextDecorations="Underline"
    float lineHeight = 0.f;    // 0 = font default

    TextStyle& Bold() { weight = DWRITE_FONT_WEIGHT_BOLD; return *this; }
    TextStyle& SemiBold() { weight = DWRITE_FONT_WEIGHT_SEMI_BOLD; return *this; }
    TextStyle& Center() { align = HAlign::Center; return *this; }
    TextStyle& Right() { align = HAlign::Right; return *this; }
    TextStyle& Top() { valign = VAlign::Top; return *this; }
    TextStyle& Wrap() { wrap = true; return *this; }
    TextStyle& Ellipsis() { ellipsis = true; return *this; }
    TextStyle& Italic() { italic = true; return *this; }
    TextStyle& Underline() { underline = true; return *this; }
    TextStyle& WithFont(Font f) { font = f; return *this; }
};

struct GradientStop {
    float offset;
    Color color;
};

struct TextSize {
    float w = 0.f, h = 0.f;
};

// A styled range inside one TextBlock (<Run FontWeight=..., Foreground=...>, <Hyperlink>).
struct TextSpan {
    size_t start = 0, length = 0;
    DWRITE_FONT_WEIGHT weight = static_cast<DWRITE_FONT_WEIGHT>(0);   // 0 = the style's
    Color color = Colors::Transparent;                                 // transparent = the text color
    bool underline = false;
};

// Direct2D renderer for one window (flip-model swap chain; per-pixel alpha when `transparent`).
// All coordinates are DIPs, like WPF.
class Graphics {
public:
    ~Graphics();
    bool Init(HWND hwnd, bool transparent);
    void Shutdown();
    void Resize();               // after WM_SIZE
    void SetDpi(float dpi);
    float Dpi() const { return dpi; }
    float Width() const { return width; }    // DIPs
    float Height() const { return height; }
    bool Ready() const { return target != nullptr; }

    bool Begin();
    void End();                  // presents
    void Clear(Color c);
    ID2D1DeviceContext2* Context() const { return dc.Get(); }

    // State stacks (must be nested properly)
    void PushTransform(const D2D1::Matrix3x2F& m);   // applied before the current transform
    void PopTransform();
    void PushClip(const Rect& r);
    void PopClip();
    void PushOpacity(float opacity);
    void PushLayer(float opacity, ID2D1Geometry* mask);
    void PushRoundClip(const Rect& r, float radius);
    void PushMaskLayer(float opacity, ID2D1Brush* opacityMask);   // alpha of the brush masks what follows
    void PopLayer();

    // Shapes
    void FillRect(const Rect& r, Color c);
    void FillRound(const Rect& r, float radius, Color c);
    void FillRound(const Rect& r, float tl, float tr, float br, float bl, Color c);
    void StrokeRect(const Rect& r, Color c, float width);
    void StrokeRound(const Rect& r, float radius, Color c, float width);
    // WPF Border: background + border drawn inside the bounds.
    void Border(const Rect& r, float radius, Color background, Color border, float thickness);
    // Border with per-side thickness (no rounding), e.g. BorderThickness="0,0,0,1".
    void BorderSides(const Rect& r, const Edges& thickness, Color border);
    void FillEllipse(float cx, float cy, float rx, float ry, Color c);
    void StrokeEllipse(float cx, float cy, float rx, float ry, Color c, float width);
    void Line(float x1, float y1, float x2, float y2, Color c, float width, bool roundCaps = false);
    void FillGeometry(ID2D1Geometry* g, Color c);
    void StrokeGeometry(ID2D1Geometry* g, Color c, float width, bool round = false);
    void FillGeometry(ID2D1Geometry* g, ID2D1Brush* brush);

    // Gradients (cached by stops)
    ID2D1LinearGradientBrush* LinearBrush(const std::vector<GradientStop>& stops, D2D1_POINT_2F start, D2D1_POINT_2F end,
                                          float opacity = 1.f);
    ID2D1RadialGradientBrush* RadialBrush(const std::vector<GradientStop>& stops, D2D1_POINT_2F center, float rx, float ry,
                                          float opacity = 1.f, D2D1_POINT_2F originOffset = { 0.f, 0.f });
    void FillLinear(const Rect& r, float radius, const std::vector<GradientStop>& stops, D2D1_POINT_2F start, D2D1_POINT_2F end);
    void FillRadial(const Rect& r, const std::vector<GradientStop>& stops);   // ellipse filling r
    ID2D1SolidColorBrush* Brush(Color c);

    // Text
    void Text(const std::wstring& s, const Rect& r, Color c, const TextStyle& style);
    TextSize Measure(const std::wstring& s, const TextStyle& style, float maxWidth = 100000.f);
    // Text with styled ranges (not cached: meant for the few mixed-style paragraphs)
    void RichText(const std::wstring& s, const Rect& r, Color c, const TextStyle& style, const std::vector<TextSpan>& spans);
    TextSize MeasureRich(const std::wstring& s, const TextStyle& style, const std::vector<TextSpan>& spans, float maxWidth = 100000.f);
    // Character index under a point of a RichText laid out in r (-1 when outside the text)
    int HitTestRich(const std::wstring& s, const Rect& r, const TextStyle& style, const std::vector<TextSpan>& spans, float x, float y);
    // Character index under a point (text boxes); `trailing` tells which half was hit.
    int HitTestText(const std::wstring& s, const TextStyle& style, float maxWidth, float x, float y);
    // Caret position for an index: x, y, line height (relative to the layout origin, top aligned).
    void CaretPosition(const std::wstring& s, const TextStyle& style, float maxWidth, int index, float& x, float& y, float& h);

    // Images
    void DrawImage(const Image& image, const Rect& r, float opacity = 1.f, Stretch stretch = Stretch::Uniform,
                   bool highQuality = true);
    void DrawImage(const ImagePtr& image, const Rect& r, float opacity = 1.f, Stretch stretch = Stretch::Uniform) {
        if (image) DrawImage(*image, r, opacity, stretch);
    }
    // Image clipped to a rounded rectangle (or ellipse when radius >= half the size).
    void DrawImageRounded(const Image& image, const Rect& r, float radius, float opacity = 1.f, Stretch stretch = Stretch::UniformToFill);
    void DrawBitmap(ID2D1Bitmap* bitmap, const Rect& dest, float opacity = 1.f, const D2D1_RECT_F* source = nullptr);

    // Off-screen drawing (e.g. a snapshot of a screen that isn't changing): a window-sized bitmap,
    // and Begin/EndOffscreen to redirect the drawing of the current frame into it.
    ComPtr<ID2D1Bitmap1> CreateWindowBitmap();
    bool MatchesWindow(ID2D1Bitmap1* bitmap) const;   // same pixel size as the window right now
    void BeginOffscreen(ID2D1Bitmap1* bitmap);
    void EndOffscreen();

    // Soft drop shadow behind a rounded rectangle (cheap stacked approximation).
    void Shadow(const Rect& r, float radius, float blur, Color c);
    // WPF DropShadowEffect with ShadowDepth=0 (a coloured glow) around a rounded rectangle / text.
    // `blurRadius` is WPF's BlurRadius. The shape mask is rendered once and cached.
    void GlowRound(const Rect& r, float radius, Color color, float blurRadius, float opacity);
    void GlowText(const std::wstring& s, const Rect& r, const TextStyle& style, Color color, float blurRadius, float opacity);
    // DropShadowEffect on an Image (follows the image's alpha) / on an ellipse outline (Stroke only).
    void GlowImage(const Image& image, const Rect& r, Color color, float blurRadius, float opacity);
    void GlowRing(float cx, float cy, float radius, float thickness, Color color, float blurRadius, float opacity);

    // Every solid / gradient color goes through this filter (seasonal re-tint of the classic palette).
    using ColorFilter = Color (*)(Color);
    void SetColorFilter(ColorFilter filter) { colorFilter = filter; }
    ColorFilter GetColorFilter() const { return colorFilter; }
    Color Filter(Color c) const { return colorFilter ? colorFilter(c) : c; }

    void EndFrameHousekeeping();

private:
    bool CreateTarget();
    void ReleaseTarget();
    IDWriteTextFormat* Format(const TextStyle& style);
    IDWriteTextLayout* Layout(const std::wstring& s, const TextStyle& style, float maxWidth, float maxHeight);
    ComPtr<IDWriteTextLayout> RichLayout(const std::wstring& s, const TextStyle& style, const std::vector<TextSpan>& spans, float maxWidth,
                                         float maxHeight, bool withColors);

    HWND hwnd = nullptr;
    bool transparent = false;
    float dpi = 96.f;
    float width = 0.f, height = 0.f;
    UINT pixelW = 0, pixelH = 0;
    ComPtr<IDXGISwapChain1> swapChain;
    ComPtr<IDCompositionTarget> compositionTarget;
    ComPtr<IDCompositionVisual> visual;
    ComPtr<ID2D1DeviceContext2> dc;
    ComPtr<ID2D1Bitmap1> target;
    ComPtr<ID2D1Image> savedTarget;   // while drawing off-screen
    ComPtr<ID2D1SolidColorBrush> solid;
    ComPtr<ID2D1StrokeStyle> roundStroke;
    std::vector<D2D1::Matrix3x2F> transforms;
    std::vector<D2D1_TEXT_ANTIALIAS_MODE> layerTextModes;   // text mode to restore per open layer
    void PushLayerKeepingText(const D2D1_LAYER_PARAMETERS1& params);

    struct CachedLayout {
        ComPtr<IDWriteTextLayout> layout;
        uint64_t lastUsed = 0;
    };
    std::unordered_map<uint64_t, ComPtr<IDWriteTextFormat>> formats;
    std::unordered_map<std::wstring, CachedLayout> layouts;
    std::unordered_map<std::string, ComPtr<ID2D1GradientStopCollection>> gradientStops;
    std::unordered_map<std::string, ComPtr<ID2D1LinearGradientBrush>> linearBrushes;
    std::unordered_map<std::string, ComPtr<ID2D1RadialGradientBrush>> radialBrushes;
    uint64_t frame = 0;
    ColorFilter colorFilter = nullptr;

    // Glow masks (white shape on transparent, padded) and the shadow effect that tints / blurs them
    struct GlowMask {
        ComPtr<ID2D1Bitmap1> bitmap;
        float pad = 0.f;
        uint64_t lastUsed = 0;
    };
    GlowMask* Mask(const std::wstring& key, float w, float h, float pad, const std::function<void(ID2D1DeviceContext*, ID2D1Brush*)>& draw);
    void DrawGlow(const GlowMask& mask, float x, float y, Color color, float blurRadius, float opacity);
    std::unordered_map<std::wstring, GlowMask> glowMasks;
    ComPtr<ID2D1Effect> shadowEffect;
};

// Temporarily disables the color filter (seasonal art keeps its own colors).
class NoColorFilter {
public:
    explicit NoColorFilter(Graphics& graphics) : g(graphics), saved(graphics.GetColorFilter()) { g.SetColorFilter(nullptr); }
    ~NoColorFilter() { g.SetColorFilter(saved); }
    NoColorFilter(const NoColorFilter&) = delete;
    NoColorFilter& operator=(const NoColorFilter&) = delete;

private:
    Graphics& g;
    Graphics::ColorFilter saved;
};

} // namespace SonicRacingSaveManager
