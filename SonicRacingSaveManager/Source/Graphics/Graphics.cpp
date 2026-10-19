#include "Graphics/Graphics.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "Graphics/Geometry.h"

#pragma comment(lib, "dxguid.lib")

namespace SonicRacingSaveManager {

namespace {

std::string StopsKey(const std::vector<GradientStop>& stops, Graphics::ColorFilter filter) {
    std::string key;
    char buffer[32];
    for (const auto& s : stops) {
        sprintf_s(buffer, "%.3f:%08X;", s.offset, (filter ? filter(s.color) : s.color).Packed());
        key += buffer;
    }
    return key;
}

} // namespace

Graphics::~Graphics() { Shutdown(); }

bool Graphics::Init(HWND window, bool isTransparent) {
    hwnd = window;
    transparent = isTransparent;
    Device& device = Device::Get();
    if (!device.Init()) return false;
    dpi = static_cast<float>(GetDpiForWindow(hwnd));
    if (dpi <= 0.f) dpi = 96.f;
    if (FAILED(device.D2D()->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &dc))) return false;
    dc->SetTextAntialiasMode(transparent ? D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE : D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    device.D2DFactory()->CreateStrokeStyle(D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND,
                                                                       D2D1_CAP_STYLE_ROUND, D2D1_LINE_JOIN_ROUND),
                                           nullptr, 0, &roundStroke);
    dc->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0, 1), &solid);
    return CreateTarget();
}

void Graphics::Shutdown() {
    ReleaseTarget();
    layouts.clear();
    formats.clear();
    glowMasks.clear();
    shadowEffect.Reset();
    linearBrushes.clear();
    radialBrushes.clear();
    gradientStops.clear();
    visual.Reset();
    compositionTarget.Reset();
    swapChain.Reset();
    solid.Reset();
    roundStroke.Reset();
    dc.Reset();
}

bool Graphics::CreateTarget() {
    if (target) return true;
    RECT rc{};
    GetClientRect(hwnd, &rc);
    pixelW = static_cast<UINT>(std::max<LONG>(1, rc.right - rc.left));
    pixelH = static_cast<UINT>(std::max<LONG>(1, rc.bottom - rc.top));
    Device& device = Device::Get();

    if (!swapChain) {
        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.Width = pixelW;
        desc.Height = pixelH;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 2;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        if (transparent) {
            desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
            desc.Scaling = DXGI_SCALING_STRETCH;
            if (!device.Composition() ||
                FAILED(device.DxgiFactory()->CreateSwapChainForComposition(device.D3D(), &desc, nullptr, &swapChain)))
                return false;
            if (FAILED(device.Composition()->CreateTargetForHwnd(hwnd, TRUE, &compositionTarget)) ||
                FAILED(device.Composition()->CreateVisual(&visual)))
                return false;
            visual->SetContent(swapChain.Get());
            compositionTarget->SetRoot(visual.Get());
            device.Composition()->Commit();
        } else {
            desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
            desc.Scaling = DXGI_SCALING_NONE;
            if (FAILED(device.DxgiFactory()->CreateSwapChainForHwnd(device.D3D(), hwnd, &desc, nullptr, nullptr, &swapChain))) {
                desc.Scaling = DXGI_SCALING_STRETCH;  // older drivers
                if (FAILED(device.DxgiFactory()->CreateSwapChainForHwnd(device.D3D(), hwnd, &desc, nullptr, nullptr, &swapChain)))
                    return false;
            }
            device.DxgiFactory()->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
        }
    }

    ComPtr<IDXGISurface> surface;
    if (FAILED(swapChain->GetBuffer(0, IID_PPV_ARGS(&surface)))) return false;
    const auto props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, transparent ? D2D1_ALPHA_MODE_PREMULTIPLIED : D2D1_ALPHA_MODE_IGNORE), dpi, dpi);
    if (FAILED(dc->CreateBitmapFromDxgiSurface(surface.Get(), &props, &target))) return false;
    dc->SetTarget(target.Get());
    dc->SetDpi(dpi, dpi);
    width = pixelW * 96.f / dpi;
    height = pixelH * 96.f / dpi;
    return true;
}

void Graphics::ReleaseTarget() {
    if (dc) dc->SetTarget(nullptr);
    target.Reset();
}

void Graphics::Resize() {
    if (!swapChain) return;
    RECT rc{};
    GetClientRect(hwnd, &rc);
    const UINT w = static_cast<UINT>(std::max<LONG>(1, rc.right - rc.left));
    const UINT h = static_cast<UINT>(std::max<LONG>(1, rc.bottom - rc.top));
    if (w == pixelW && h == pixelH && target) return;
    ReleaseTarget();
    swapChain->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0);
    CreateTarget();
}

void Graphics::SetDpi(float newDpi) {
    dpi = newDpi;
    glowMasks.clear();
    ReleaseTarget();
    CreateTarget();
}

bool Graphics::Begin() {
    if (!CreateTarget()) return false;
    ++frame;
    dc->BeginDraw();
    dc->SetTransform(D2D1::Matrix3x2F::Identity());
    transforms.clear();
    return true;
}

void Graphics::End() {
    const HRESULT hr = dc->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        ReleaseTarget();
        return;
    }
    // Interval 0: the message loop paces all windows together with DwmFlush
    const HRESULT present = swapChain->Present(0, 0);
    if (present == DXGI_ERROR_DEVICE_REMOVED || present == DXGI_ERROR_DEVICE_RESET) ReleaseTarget();
    EndFrameHousekeeping();
}

void Graphics::EndFrameHousekeeping() {
    // Drop text layouts that weren't used for a while (text that changes every frame, like timers)
    if (frame % 120 != 0) return;
    for (auto it = layouts.begin(); it != layouts.end();) {
        if (frame - it->second.lastUsed > 240) it = layouts.erase(it);
        else ++it;
    }
    for (auto it = glowMasks.begin(); it != glowMasks.end();) {
        if (frame - it->second.lastUsed > 600) it = glowMasks.erase(it);
        else ++it;
    }
}

void Graphics::Clear(Color c) { dc->Clear(c.ToD2D()); }

void Graphics::PushTransform(const D2D1::Matrix3x2F& m) {
    D2D1::Matrix3x2F current;
    dc->GetTransform(&current);
    transforms.push_back(current);
    dc->SetTransform(m * current);
}

void Graphics::PopTransform() {
    if (transforms.empty()) return;
    dc->SetTransform(transforms.back());
    transforms.pop_back();
}

void Graphics::PushClip(const Rect& r) { dc->PushAxisAlignedClip(r.D2D(), D2D1_ANTIALIAS_MODE_ALIASED); }
void Graphics::PopClip() { dc->PopAxisAlignedClip(); }

void Graphics::PushLayer(float opacity, ID2D1Geometry* mask) {
    D2D1_LAYER_PARAMETERS1 params = D2D1::LayerParameters1(D2D1::InfiniteRect(), mask, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
                                                           D2D1::IdentityMatrix(), opacity, nullptr,
                                                           D2D1_LAYER_OPTIONS1_NONE);
    PushLayerKeepingText(params);
}

// ClearType can't blend onto a layer's transparent pixels (dark text vanishes): grayscale text
// while a layer is open, ClearType again once the last one is popped
void Graphics::PushLayerKeepingText(const D2D1_LAYER_PARAMETERS1& params) {
    layerTextModes.push_back(dc->GetTextAntialiasMode());
    dc->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    dc->PushLayer(params, nullptr);
}

void Graphics::PushOpacity(float opacity) { PushLayer(opacity, nullptr); }

void Graphics::PushMaskLayer(float opacity, ID2D1Brush* opacityMask) {
    D2D1_LAYER_PARAMETERS1 params = D2D1::LayerParameters1(D2D1::InfiniteRect(), nullptr, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
                                                           D2D1::IdentityMatrix(), opacity, opacityMask, D2D1_LAYER_OPTIONS1_NONE);
    PushLayerKeepingText(params);
}

void Graphics::PushRoundClip(const Rect& r, float radius) {
    auto geometry = Geometry::RoundedRect(r.D2D(), radius, radius);
    PushLayer(1.f, geometry.Get());
}

void Graphics::PopLayer() {
    dc->PopLayer();
    if (!layerTextModes.empty()) {
        dc->SetTextAntialiasMode(layerTextModes.back());
        layerTextModes.pop_back();
    }
}

ID2D1SolidColorBrush* Graphics::Brush(Color c) {
    solid->SetColor(Filter(c).ToD2D());
    solid->SetOpacity(1.f);
    return solid.Get();
}

void Graphics::FillRect(const Rect& r, Color c) {
    if (c.Transparent() || r.Empty()) return;
    dc->FillRectangle(r.D2D(), Brush(c));
}

void Graphics::FillRound(const Rect& r, float radius, Color c) {
    if (c.Transparent() || r.Empty()) return;
    if (radius <= 0.f) dc->FillRectangle(r.D2D(), Brush(c));
    else dc->FillRoundedRectangle(D2D1::RoundedRect(r.D2D(), radius, radius), Brush(c));
}

void Graphics::FillRound(const Rect& r, float tl, float tr, float br, float bl, Color c) {
    if (c.Transparent() || r.Empty()) return;
    auto g = Geometry::RoundedRect(r.D2D(), tl, tr, br, bl);
    if (g) dc->FillGeometry(g.Get(), Brush(c));
}

void Graphics::StrokeRect(const Rect& r, Color c, float w) {
    if (c.Transparent()) return;
    dc->DrawRectangle(r.Inset(w * 0.5f).D2D(), Brush(c), w);
}

void Graphics::StrokeRound(const Rect& r, float radius, Color c, float w) {
    if (c.Transparent()) return;
    const Rect inner = r.Inset(w * 0.5f);
    const float rr = std::max(0.f, radius - w * 0.5f);
    if (rr <= 0.f) dc->DrawRectangle(inner.D2D(), Brush(c), w);
    else dc->DrawRoundedRectangle(D2D1::RoundedRect(inner.D2D(), rr, rr), Brush(c), w);
}

void Graphics::Border(const Rect& r, float radius, Color background, Color border, float thickness) {
    FillRound(r, radius, background);
    if (thickness > 0.f) StrokeRound(r, radius, border, thickness);
}

void Graphics::BorderSides(const Rect& r, const Edges& t, Color c) {
    if (c.Transparent()) return;
    if (t.t > 0) FillRect({ r.x, r.y, r.w, t.t }, c);
    if (t.b > 0) FillRect({ r.x, r.B() - t.b, r.w, t.b }, c);
    if (t.l > 0) FillRect({ r.x, r.y, t.l, r.h }, c);
    if (t.r > 0) FillRect({ r.R() - t.r, r.y, t.r, r.h }, c);
}

void Graphics::FillEllipse(float cx, float cy, float rx, float ry, Color c) {
    if (c.Transparent()) return;
    dc->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), rx, ry), Brush(c));
}

void Graphics::StrokeEllipse(float cx, float cy, float rx, float ry, Color c, float w) {
    if (c.Transparent()) return;
    dc->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), rx, ry), Brush(c), w);
}

void Graphics::Line(float x1, float y1, float x2, float y2, Color c, float w, bool roundCaps) {
    if (c.Transparent()) return;
    dc->DrawLine(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), Brush(c), w, roundCaps ? roundStroke.Get() : nullptr);
}

void Graphics::FillGeometry(ID2D1Geometry* g, Color c) {
    if (g && !c.Transparent()) dc->FillGeometry(g, Brush(c));
}

void Graphics::StrokeGeometry(ID2D1Geometry* g, Color c, float w, bool round) {
    if (g && !c.Transparent()) dc->DrawGeometry(g, Brush(c), w, round ? roundStroke.Get() : nullptr);
}

void Graphics::FillGeometry(ID2D1Geometry* g, ID2D1Brush* brush) {
    if (g && brush) dc->FillGeometry(g, brush);
}

ID2D1LinearGradientBrush* Graphics::LinearBrush(const std::vector<GradientStop>& stops, D2D1_POINT_2F start, D2D1_POINT_2F end,
                                                float opacity) {
    const std::string key = StopsKey(stops, colorFilter);
    auto it = linearBrushes.find(key);
    if (it == linearBrushes.end()) {
        std::vector<D2D1_GRADIENT_STOP> d2dStops;
        for (const auto& s : stops) d2dStops.push_back({ s.offset, Filter(s.color).ToD2D() });
        ComPtr<ID2D1GradientStopCollection> collection;
        ComPtr<ID2D1LinearGradientBrush> brush;
        if (FAILED(dc->CreateGradientStopCollection(d2dStops.data(), static_cast<UINT32>(d2dStops.size()), &collection)) ||
            FAILED(dc->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(start, end), collection.Get(), &brush)))
            return nullptr;
        it = linearBrushes.emplace(key, brush).first;
    }
    it->second->SetStartPoint(start);
    it->second->SetEndPoint(end);
    it->second->SetOpacity(opacity);
    return it->second.Get();
}

ID2D1RadialGradientBrush* Graphics::RadialBrush(const std::vector<GradientStop>& stops, D2D1_POINT_2F center, float rx, float ry,
                                                float opacity, D2D1_POINT_2F originOffset) {
    const std::string key = StopsKey(stops, colorFilter);
    auto it = radialBrushes.find(key);
    if (it == radialBrushes.end()) {
        std::vector<D2D1_GRADIENT_STOP> d2dStops;
        for (const auto& s : stops) d2dStops.push_back({ s.offset, Filter(s.color).ToD2D() });
        ComPtr<ID2D1GradientStopCollection> collection;
        ComPtr<ID2D1RadialGradientBrush> brush;
        if (FAILED(dc->CreateGradientStopCollection(d2dStops.data(), static_cast<UINT32>(d2dStops.size()), &collection)) ||
            FAILED(dc->CreateRadialGradientBrush(D2D1::RadialGradientBrushProperties(center, originOffset, rx, ry),
                                                 collection.Get(), &brush)))
            return nullptr;
        it = radialBrushes.emplace(key, brush).first;
    }
    it->second->SetCenter(center);
    it->second->SetGradientOriginOffset(originOffset);
    it->second->SetRadiusX(rx);
    it->second->SetRadiusY(ry);
    it->second->SetOpacity(opacity);
    return it->second.Get();
}

void Graphics::FillLinear(const Rect& r, float radius, const std::vector<GradientStop>& stops, D2D1_POINT_2F start, D2D1_POINT_2F end) {
    // start / end are relative (0..1) like XAML StartPoint / EndPoint
    auto* b = LinearBrush(stops, { r.x + start.x * r.w, r.y + start.y * r.h }, { r.x + end.x * r.w, r.y + end.y * r.h });
    if (!b) return;
    if (radius <= 0.f) dc->FillRectangle(r.D2D(), b);
    else dc->FillRoundedRectangle(D2D1::RoundedRect(r.D2D(), radius, radius), b);
}

void Graphics::FillRadial(const Rect& r, const std::vector<GradientStop>& stops) {
    auto* b = RadialBrush(stops, { r.CX(), r.CY() }, r.w * 0.5f, r.h * 0.5f);
    if (b) dc->FillEllipse(D2D1::Ellipse(D2D1::Point2F(r.CX(), r.CY()), r.w * 0.5f, r.h * 0.5f), b);
}

// ── Text ────────────────────────────────────────────────────────────────────

IDWriteTextFormat* Graphics::Format(const TextStyle& style) {
    const uint64_t key = static_cast<uint64_t>(style.size * 100.f) << 32 | static_cast<uint64_t>(style.font) << 24 |
                         static_cast<uint64_t>(style.weight) << 12 | (style.italic ? 1ull : 0ull) << 11 |
                         static_cast<uint64_t>(style.lineHeight * 10.f) & 0x7FF;
    auto it = formats.find(key);
    if (it != formats.end()) return it->second.Get();
    const Fonts::Family& family = Fonts::Get(style.font);
    ComPtr<IDWriteTextFormat> format;
    if (FAILED(Device::Get().DWrite()->CreateTextFormat(family.name.c_str(), family.collection.Get(), style.weight,
                                                        style.italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
                                                        DWRITE_FONT_STRETCH_NORMAL, style.size, L"en-us", &format)))
        return nullptr;
    if (style.lineHeight > 0.f)
        format->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, style.lineHeight, style.lineHeight * 0.8f);
    return formats.emplace(key, format).first->second.Get();
}

IDWriteTextLayout* Graphics::Layout(const std::wstring& s, const TextStyle& style, float maxWidth, float maxHeight) {
    wchar_t header[160];
    swprintf_s(header, L"%.2f|%d|%d|%d|%d|%d|%d|%d|%d|%.1f|%.1f|%.1f|", style.size, static_cast<int>(style.font),
               static_cast<int>(style.weight), style.italic, static_cast<int>(style.align), static_cast<int>(style.valign),
               style.wrap, style.ellipsis, style.underline, style.lineHeight, maxWidth, maxHeight);
    std::wstring key = header;
    key += s;
    auto it = layouts.find(key);
    if (it != layouts.end()) {
        it->second.lastUsed = frame;
        return it->second.layout.Get();
    }
    IDWriteTextFormat* format = Format(style);
    if (!format) return nullptr;
    ComPtr<IDWriteTextLayout> layout;
    if (FAILED(Device::Get().DWrite()->CreateTextLayout(s.c_str(), static_cast<UINT32>(s.size()), format, std::max(0.f, maxWidth),
                                                        std::max(0.f, maxHeight), &layout)))
        return nullptr;
    switch (style.align) {
        case HAlign::Left: layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING); break;
        case HAlign::Center: layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER); break;
        case HAlign::Right: layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING); break;
        case HAlign::Justify: layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_JUSTIFIED); break;
    }
    switch (style.valign) {
        case VAlign::Top: layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR); break;
        case VAlign::Center: layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER); break;
        case VAlign::Bottom: layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_FAR); break;
    }
    layout->SetWordWrapping(style.wrap ? DWRITE_WORD_WRAPPING_WRAP : DWRITE_WORD_WRAPPING_NO_WRAP);
    if (style.ellipsis) {
        ComPtr<IDWriteInlineObject> sign;
        Device::Get().DWrite()->CreateEllipsisTrimmingSign(layout.Get(), &sign);
        const DWRITE_TRIMMING trimming{ DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
        layout->SetTrimming(&trimming, sign.Get());
    }
    if (style.underline) layout->SetUnderline(TRUE, DWRITE_TEXT_RANGE{ 0, static_cast<UINT32>(s.size()) });
    auto& entry = layouts[key];
    entry.layout = layout;
    entry.lastUsed = frame;
    return layout.Get();
}

void Graphics::Text(const std::wstring& s, const Rect& r, Color c, const TextStyle& style) {
    if (s.empty() || c.Transparent() || r.w <= 0.f) return;
    IDWriteTextLayout* layout = Layout(s, style, r.w, std::max(r.h, 0.f));
    if (!layout) return;
    dc->DrawTextLayout(D2D1::Point2F(r.x, r.y), layout, Brush(c), D2D1_DRAW_TEXT_OPTIONS_NONE);   // WPF never draws color emoji
}

ComPtr<IDWriteTextLayout> Graphics::RichLayout(const std::wstring& s, const TextStyle& style, const std::vector<TextSpan>& spans,
                                               float maxWidth, float maxHeight, bool withColors) {
    // Same paragraph settings as the cached layouts, then the ranges on a private copy
    IDWriteTextLayout* base = Layout(s, style, maxWidth, maxHeight);
    IDWriteTextFormat* format = Format(style);
    if (!base || !format) return nullptr;
    ComPtr<IDWriteTextLayout> layout;
    if (FAILED(Device::Get().DWrite()->CreateTextLayout(s.c_str(), static_cast<UINT32>(s.size()), format, std::max(0.f, maxWidth),
                                                        std::max(0.f, maxHeight), &layout)))
        return nullptr;
    layout->SetTextAlignment(base->GetTextAlignment());
    layout->SetParagraphAlignment(base->GetParagraphAlignment());
    layout->SetWordWrapping(base->GetWordWrapping());
    DWRITE_LINE_SPACING_METHOD method;
    FLOAT spacing = 0.f, baseline = 0.f;
    if (SUCCEEDED(base->GetLineSpacing(&method, &spacing, &baseline))) layout->SetLineSpacing(method, spacing, baseline);
    if (style.underline) layout->SetUnderline(TRUE, { 0, static_cast<UINT32>(s.size()) });
    for (const auto& span : spans) {
        const DWRITE_TEXT_RANGE range{ static_cast<UINT32>(span.start), static_cast<UINT32>(span.length) };
        if (span.weight != 0) layout->SetFontWeight(span.weight, range);
        if (span.underline) layout->SetUnderline(TRUE, range);
        if (withColors && !span.color.Transparent()) {
            ComPtr<ID2D1SolidColorBrush> brush;
            if (SUCCEEDED(dc->CreateSolidColorBrush(Filter(span.color).ToD2D(), &brush))) layout->SetDrawingEffect(brush.Get(), range);
        }
    }
    return layout;
}

void Graphics::RichText(const std::wstring& s, const Rect& r, Color c, const TextStyle& style, const std::vector<TextSpan>& spans) {
    if (s.empty() || r.w <= 0.f) return;
    auto layout = RichLayout(s, style, spans, r.w, std::max(r.h, 0.f), true);
    if (layout) dc->DrawTextLayout(D2D1::Point2F(r.x, r.y), layout.Get(), Brush(c), D2D1_DRAW_TEXT_OPTIONS_NONE);
}

TextSize Graphics::MeasureRich(const std::wstring& s, const TextStyle& style, const std::vector<TextSpan>& spans, float maxWidth) {
    TextStyle measure = style;
    measure.align = HAlign::Left;
    measure.valign = VAlign::Top;
    measure.ellipsis = false;
    auto layout = RichLayout(s.empty() ? L" " : s, measure, spans, maxWidth, 100000.f, false);
    if (!layout) return {};
    DWRITE_TEXT_METRICS m{};
    layout->GetMetrics(&m);
    return { s.empty() ? 0.f : m.widthIncludingTrailingWhitespace, m.height };
}

int Graphics::HitTestRich(const std::wstring& s, const Rect& r, const TextStyle& style, const std::vector<TextSpan>& spans, float x, float y) {
    auto layout = RichLayout(s, style, spans, r.w, std::max(r.h, 0.f), false);
    if (!layout) return -1;
    BOOL trailing = FALSE, inside = FALSE;
    DWRITE_HIT_TEST_METRICS m{};
    layout->HitTestPoint(x - r.x, y - r.y, &trailing, &inside, &m);
    return inside ? static_cast<int>(m.textPosition) : -1;
}

TextSize Graphics::Measure(const std::wstring& s, const TextStyle& style, float maxWidth) {
    if (s.empty()) {
        // Height of an empty line still matters for layout (empty TextBlocks keep their line height)
        IDWriteTextLayout* probe = Layout(L" ", style, maxWidth, 100000.f);
        if (!probe) return {};
        DWRITE_TEXT_METRICS m{};
        probe->GetMetrics(&m);
        return { 0.f, m.height };
    }
    TextStyle measure = style;
    measure.align = HAlign::Left;
    measure.valign = VAlign::Top;
    measure.ellipsis = false;
    IDWriteTextLayout* layout = Layout(s, measure, maxWidth, 100000.f);
    if (!layout) return {};
    DWRITE_TEXT_METRICS m{};
    layout->GetMetrics(&m);
    return { m.widthIncludingTrailingWhitespace, m.height };
}

int Graphics::HitTestText(const std::wstring& s, const TextStyle& style, float maxWidth, float x, float y) {
    TextStyle measure = style;
    measure.valign = VAlign::Top;
    measure.ellipsis = false;
    IDWriteTextLayout* layout = Layout(s.empty() ? L" " : s, measure, maxWidth, 100000.f);
    if (!layout) return 0;
    BOOL trailing = FALSE, inside = FALSE;
    DWRITE_HIT_TEST_METRICS m{};
    layout->HitTestPoint(x, y, &trailing, &inside, &m);
    int index = static_cast<int>(m.textPosition) + (trailing ? static_cast<int>(m.length) : 0);
    return std::clamp(index, 0, static_cast<int>(s.size()));
}

void Graphics::CaretPosition(const std::wstring& s, const TextStyle& style, float maxWidth, int index, float& x, float& y, float& h) {
    TextStyle measure = style;
    measure.valign = VAlign::Top;
    measure.ellipsis = false;
    const std::wstring text = s.empty() ? L" " : s;
    IDWriteTextLayout* layout = Layout(text, measure, maxWidth, 100000.f);
    x = y = 0.f;
    h = style.size * 1.3f;
    if (!layout) return;
    DWRITE_HIT_TEST_METRICS m{};
    FLOAT px = 0, py = 0;
    const bool atEnd = index >= static_cast<int>(s.size()) && !s.empty();
    layout->HitTestTextPosition(static_cast<UINT32>(atEnd ? s.size() - 1 : (s.empty() ? 0 : index)), atEnd ? TRUE : FALSE, &px,
                                &py, &m);
    x = s.empty() ? (style.align == HAlign::Center ? m.left : 0.f) : px;
    y = py;
    h = m.height;
    // A caret after a trailing newline sits at the start of the next line
    if (atEnd && s.back() == L'\n') {
        x = 0.f;
        y = py + m.height;
    }
}

// ── Images ──────────────────────────────────────────────────────────────────

namespace {

D2D1_RECT_F Fit(const Image& image, const Rect& r, Stretch stretch) {
    if (stretch == Stretch::Fill || image.w <= 0 || image.h <= 0) return r.D2D();
    float s = 1.f;
    if (stretch == Stretch::Uniform) s = std::min(r.w / image.w, r.h / image.h);
    else if (stretch == Stretch::UniformToFill) s = std::max(r.w / image.w, r.h / image.h);
    const float w = image.w * s, h = image.h * s;
    const float x = r.x + (r.w - w) * 0.5f, y = r.y + (r.h - h) * 0.5f;
    return D2D1::RectF(x, y, x + w, y + h);
}

} // namespace

void Graphics::DrawImage(const Image& image, const Rect& r, float opacity, Stretch stretch, bool highQuality) {
    if (!image.bitmap || r.Empty()) return;
    const D2D1_RECT_F dest = Fit(image, r, stretch);
    const auto mode = highQuality ? D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC : D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR;
    if (stretch == Stretch::UniformToFill) {
        PushClip(r);
        dc->DrawBitmap(image.bitmap.Get(), dest, opacity, mode, nullptr, nullptr);
        PopClip();
    } else {
        dc->DrawBitmap(image.bitmap.Get(), dest, opacity, mode, nullptr, nullptr);
    }
}

void Graphics::DrawImageRounded(const Image& image, const Rect& r, float radius, float opacity, Stretch stretch) {
    if (!image.bitmap || r.Empty()) return;
    // A full radius gives a circle on a square and a capsule on anything wider (not an ellipse)
    radius = std::min(radius, std::min(r.w, r.h) * 0.5f);
    ComPtr<ID2D1Geometry> mask = Geometry::RoundedRect(r.D2D(), radius, radius);
    PushLayer(opacity, mask.Get());
    dc->DrawBitmap(image.bitmap.Get(), Fit(image, r, stretch), 1.f, D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC, nullptr, nullptr);
    PopLayer();
}

void Graphics::DrawBitmap(ID2D1Bitmap* bitmap, const Rect& dest, float opacity, const D2D1_RECT_F* source) {
    if (!bitmap) return;
    dc->DrawBitmap(bitmap, dest.D2D(), opacity, D2D1_INTERPOLATION_MODE_LINEAR, source, nullptr);
}

ComPtr<ID2D1Bitmap1> Graphics::CreateWindowBitmap() {
    ComPtr<ID2D1Bitmap1> bitmap;
    if (!dc) return bitmap;
    const auto props = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET,
                                               D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), dpi, dpi);
    dc->CreateBitmap(D2D1::SizeU(pixelW, pixelH), nullptr, 0, props, &bitmap);
    return bitmap;
}

bool Graphics::MatchesWindow(ID2D1Bitmap1* bitmap) const {
    if (!bitmap) return false;
    const auto size = bitmap->GetPixelSize();
    return size.width == pixelW && size.height == pixelH;
}

void Graphics::BeginOffscreen(ID2D1Bitmap1* bitmap) {
    if (!bitmap || savedTarget) return;
    dc->GetTarget(&savedTarget);
    dc->SetTarget(bitmap);
}

void Graphics::EndOffscreen() {
    if (!savedTarget) return;
    dc->SetTarget(savedTarget.Get());
    savedTarget.Reset();
}

Graphics::GlowMask* Graphics::Mask(const std::wstring& key, float w, float h, float pad,
                                   const std::function<void(ID2D1DeviceContext*, ID2D1Brush*)>& draw) {
    auto it = glowMasks.find(key);
    if (it != glowMasks.end()) {
        it->second.lastUsed = frame;
        return it->second.bitmap ? &it->second : nullptr;
    }
    GlowMask mask;
    mask.pad = pad;
    mask.lastUsed = frame;
    ID2D1DeviceContext2* rc = Device::Get().ResourceContext();
    const UINT pw = static_cast<UINT>(std::ceil((w + pad * 2.f) * dpi / 96.f));
    const UINT ph = static_cast<UINT>(std::ceil((h + pad * 2.f) * dpi / 96.f));
    if (rc && pw > 0 && ph > 0 && pw < 8192 && ph < 8192) {
        const auto props = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET,
                                                   D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), dpi, dpi);
        if (SUCCEEDED(rc->CreateBitmap(D2D1::SizeU(pw, ph), nullptr, 0, props, &mask.bitmap))) {
            ComPtr<ID2D1SolidColorBrush> white;
            rc->SetTarget(mask.bitmap.Get());
            rc->SetDpi(dpi, dpi);
            rc->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 1), &white);
            rc->BeginDraw();
            rc->Clear(D2D1::ColorF(0, 0, 0, 0));
            rc->SetTransform(D2D1::Matrix3x2F::Translation(pad, pad));
            draw(rc, white.Get());
            rc->SetTransform(D2D1::Matrix3x2F::Identity());
            rc->EndDraw();
            rc->SetTarget(nullptr);
        }
    }
    auto& stored = glowMasks[key];
    stored = mask;
    return stored.bitmap ? &stored : nullptr;
}

void Graphics::DrawGlow(const GlowMask& mask, float x, float y, Color color, float blurRadius, float opacity) {
    if (!shadowEffect && FAILED(dc->CreateEffect(CLSID_D2D1Shadow, &shadowEffect))) return;
    const Color c = Filter(color);
    shadowEffect->SetInput(0, mask.bitmap.Get());
    shadowEffect->SetValue(D2D1_SHADOW_PROP_COLOR, D2D1::Vector4F(c.r, c.g, c.b, c.a * opacity));
    // WPF's BlurRadius spans about three standard deviations
    shadowEffect->SetValue(D2D1_SHADOW_PROP_BLUR_STANDARD_DEVIATION, std::max(0.1f, blurRadius / 3.f));
    dc->DrawImage(shadowEffect.Get(), D2D1::Point2F(x - mask.pad, y - mask.pad));
}

void Graphics::GlowRound(const Rect& r, float radius, Color color, float blurRadius, float opacity) {
    if (r.Empty() || opacity <= 0.f) return;
    wchar_t key[96];
    swprintf_s(key, L"round|%.1f|%.1f|%.1f|%.1f", r.w, r.h, radius, blurRadius);
    const float pad = blurRadius * 1.5f + 2.f;
    GlowMask* mask = Mask(key, r.w, r.h, pad, [&](ID2D1DeviceContext* rc, ID2D1Brush* white) {
        rc->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(0, 0, r.w, r.h), radius, radius), white);
    });
    if (mask) DrawGlow(*mask, r.x, r.y, color, blurRadius, opacity);
}

void Graphics::GlowText(const std::wstring& s, const Rect& r, const TextStyle& style, Color color, float blurRadius, float opacity) {
    if (s.empty() || r.Empty() || opacity <= 0.f) return;
    IDWriteTextLayout* layout = Layout(s, style, r.w, r.h);
    if (!layout) return;
    wchar_t header[96];
    swprintf_s(header, L"text|%.1f|%.1f|%.1f|%.1f|%d|", r.w, r.h, style.size, blurRadius, static_cast<int>(style.weight));
    const float pad = blurRadius * 1.5f + 2.f;
    ComPtr<IDWriteTextLayout> keep = layout;
    GlowMask* mask = Mask(std::wstring(header) + s, r.w, r.h, pad, [&](ID2D1DeviceContext* rc, ID2D1Brush* white) {
        rc->DrawTextLayout(D2D1::Point2F(0, 0), keep.Get(), white);
    });
    if (mask) DrawGlow(*mask, r.x, r.y, color, blurRadius, opacity);
}

void Graphics::GlowImage(const Image& image, const Rect& r, Color color, float blurRadius, float opacity) {
    if (!image.bitmap || r.Empty() || opacity <= 0.f) return;
    wchar_t key[96];
    swprintf_s(key, L"image|%p|%.1f|%.1f|%.1f", static_cast<void*>(image.bitmap.Get()), r.w, r.h, blurRadius);
    const float pad = blurRadius * 1.5f + 2.f;
    GlowMask* mask = Mask(key, r.w, r.h, pad, [&](ID2D1DeviceContext* rc, ID2D1Brush*) {
        // The shadow only uses the alpha channel, so the image itself is the mask
        rc->DrawBitmap(image.bitmap.Get(), D2D1::RectF(0, 0, r.w, r.h), 1.f, D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC);
    });
    if (mask) DrawGlow(*mask, r.x, r.y, color, blurRadius, opacity);
}

void Graphics::GlowRing(float cx, float cy, float radius, float thickness, Color color, float blurRadius, float opacity) {
    if (radius <= 0.f || opacity <= 0.f) return;
    wchar_t key[96];
    swprintf_s(key, L"ring|%.1f|%.1f|%.1f", radius, thickness, blurRadius);
    const float pad = blurRadius * 1.5f + 2.f;
    const float d = radius * 2.f;
    GlowMask* mask = Mask(key, d, d, pad, [&](ID2D1DeviceContext* rc, ID2D1Brush* white) {
        const float r = radius - thickness * 0.5f;
        rc->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(radius, radius), r, r), white, thickness);
    });
    if (mask) DrawGlow(*mask, cx - radius, cy - radius, color, blurRadius, opacity);
}

void Graphics::Shadow(const Rect& r, float radius, float blur, Color c) {
    // Stacked translucent rounded rects approximate a gaussian falloff
    const int steps = 6;
    for (int i = steps; i >= 1; --i) {
        const float grow = blur * i / steps;
        FillRound(r.Inset(-grow), radius + grow, c.Fade(1.f / (steps + 2)));
    }
}

} // namespace SonicRacingSaveManager
