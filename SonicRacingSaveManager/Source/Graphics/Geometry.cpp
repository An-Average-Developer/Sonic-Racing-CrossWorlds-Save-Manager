#include "Graphics/Geometry.h"
#include <cctype>
#include <cmath>
#include <cstdlib>
#include "Graphics/Device.h"

namespace SonicRacingSaveManager::Geometry {

ComPtr<ID2D1PathGeometry> Build(const std::function<void(ID2D1GeometrySink*)>& build, D2D1_FILL_MODE fill) {
    ComPtr<ID2D1PathGeometry> geometry;
    ComPtr<ID2D1GeometrySink> sink;
    if (FAILED(Device::Get().D2DFactory()->CreatePathGeometry(&geometry)) || FAILED(geometry->Open(&sink))) return nullptr;
    sink->SetFillMode(fill);
    build(sink.Get());
    sink->Close();
    return geometry;
}

namespace {

class PathParser {
public:
    explicit PathParser(std::string_view text) : s(text) {}

    void Run(ID2D1GeometrySink* sink) {
        char command = 0;
        while (true) {
            SkipSeparators();
            if (pos >= s.size()) break;
            const char c = s[pos];
            if (std::isalpha(static_cast<unsigned char>(c))) {
                command = c;
                ++pos;
                if (command == 'F' || command == 'f') {  // XAML fill rule, handled by the caller
                    Number();
                    continue;
                }
                if (command == 'Z' || command == 'z') {
                    Close(sink);
                    continue;
                }
            } else if (command == 0) {
                break;  // garbage
            }
            if (!Segment(sink, command)) break;
            // After a moveto, implicit repeats are linetos
            if (command == 'M') command = 'L';
            else if (command == 'm') command = 'l';
        }
        if (open) sink->EndFigure(D2D1_FIGURE_END_OPEN);
    }

private:
    void SkipSeparators() {
        while (pos < s.size() && (std::isspace(static_cast<unsigned char>(s[pos])) || s[pos] == ',')) ++pos;
    }

    bool HasNumber() {
        SkipSeparators();
        if (pos >= s.size()) return false;
        const char c = s[pos];
        return std::isdigit(static_cast<unsigned char>(c)) || c == '-' || c == '+' || c == '.';
    }

    float Number() {
        SkipSeparators();
        const char* begin = s.data() + pos;
        char* end = nullptr;
        // strtof would accept "1.5.5" as 1.5 followed by ".5" — exactly what SVG wants
        const float v = std::strtof(begin, &end);
        pos += static_cast<size_t>(end - begin);
        return v;
    }

    bool Flag() {
        SkipSeparators();
        if (pos < s.size() && (s[pos] == '0' || s[pos] == '1')) return s[pos++] == '1';
        return Number() != 0.f;
    }

    void Begin(ID2D1GeometrySink* sink) {
        if (!open) {
            sink->BeginFigure(D2D1::Point2F(cx, cy), D2D1_FIGURE_BEGIN_FILLED);
            open = true;
            startX = cx;
            startY = cy;
        }
    }

    void Close(ID2D1GeometrySink* sink) {
        if (open) sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        open = false;
        cx = startX;
        cy = startY;
    }

    bool Segment(ID2D1GeometrySink* sink, char command) {
        if (!HasNumber()) return false;
        const bool rel = std::islower(static_cast<unsigned char>(command)) != 0;
        const float ox = rel ? cx : 0.f, oy = rel ? cy : 0.f;
        switch (std::toupper(static_cast<unsigned char>(command))) {
            case 'M': {
                const float x = Number() + ox, y = Number() + oy;
                if (open) sink->EndFigure(D2D1_FIGURE_END_OPEN);
                open = false;
                cx = x;
                cy = y;
                Begin(sink);
                lastControl = false;
                return true;
            }
            case 'L': {
                const float x = Number() + ox, y = Number() + oy;
                Begin(sink);
                sink->AddLine(D2D1::Point2F(x, y));
                cx = x;
                cy = y;
                lastControl = false;
                return true;
            }
            case 'H': {
                const float x = Number() + ox;
                Begin(sink);
                sink->AddLine(D2D1::Point2F(x, cy));
                cx = x;
                lastControl = false;
                return true;
            }
            case 'V': {
                const float y = Number() + oy;
                Begin(sink);
                sink->AddLine(D2D1::Point2F(cx, y));
                cy = y;
                lastControl = false;
                return true;
            }
            case 'C': {
                const float x1 = Number() + ox, y1 = Number() + oy, x2 = Number() + ox, y2 = Number() + oy;
                const float x = Number() + ox, y = Number() + oy;
                Begin(sink);
                sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), D2D1::Point2F(x, y)));
                ctrlX = x2;
                ctrlY = y2;
                lastControl = true;
                lastQuad = false;
                cx = x;
                cy = y;
                return true;
            }
            case 'S': {
                const float x2 = Number() + ox, y2 = Number() + oy, x = Number() + ox, y = Number() + oy;
                const float x1 = lastControl && !lastQuad ? 2 * cx - ctrlX : cx;
                const float y1 = lastControl && !lastQuad ? 2 * cy - ctrlY : cy;
                Begin(sink);
                sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), D2D1::Point2F(x, y)));
                ctrlX = x2;
                ctrlY = y2;
                lastControl = true;
                lastQuad = false;
                cx = x;
                cy = y;
                return true;
            }
            case 'Q': {
                const float x1 = Number() + ox, y1 = Number() + oy, x = Number() + ox, y = Number() + oy;
                Begin(sink);
                sink->AddQuadraticBezier(D2D1::QuadraticBezierSegment(D2D1::Point2F(x1, y1), D2D1::Point2F(x, y)));
                ctrlX = x1;
                ctrlY = y1;
                lastControl = true;
                lastQuad = true;
                cx = x;
                cy = y;
                return true;
            }
            case 'T': {
                const float x = Number() + ox, y = Number() + oy;
                const float x1 = lastControl && lastQuad ? 2 * cx - ctrlX : cx;
                const float y1 = lastControl && lastQuad ? 2 * cy - ctrlY : cy;
                Begin(sink);
                sink->AddQuadraticBezier(D2D1::QuadraticBezierSegment(D2D1::Point2F(x1, y1), D2D1::Point2F(x, y)));
                ctrlX = x1;
                ctrlY = y1;
                lastControl = true;
                lastQuad = true;
                cx = x;
                cy = y;
                return true;
            }
            case 'A': {
                const float rx = Number(), ry = Number(), rotation = Number();
                const bool large = Flag(), sweep = Flag();
                const float x = Number() + ox, y = Number() + oy;
                Begin(sink);
                sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(x, y), D2D1::SizeF(std::fabs(rx), std::fabs(ry)), rotation,
                                              sweep ? D2D1_SWEEP_DIRECTION_CLOCKWISE : D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,
                                              large ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL));
                cx = x;
                cy = y;
                lastControl = false;
                return true;
            }
            default:
                return false;
        }
    }

    std::string_view s;
    size_t pos = 0;
    float cx = 0.f, cy = 0.f, startX = 0.f, startY = 0.f, ctrlX = 0.f, ctrlY = 0.f;
    bool open = false, lastControl = false, lastQuad = false;
};

} // namespace

ComPtr<ID2D1PathGeometry> FromPathData(std::string_view data) {
    // "F1" at the start selects the non-zero (winding) rule; default is even-odd like XAML
    D2D1_FILL_MODE fill = D2D1_FILL_MODE_ALTERNATE;
    size_t i = 0;
    while (i < data.size() && std::isspace(static_cast<unsigned char>(data[i]))) ++i;
    if (i + 1 < data.size() && (data[i] == 'F' || data[i] == 'f') && data[i + 1] == '1') fill = D2D1_FILL_MODE_WINDING;
    return Build([&](ID2D1GeometrySink* sink) { PathParser(data).Run(sink); }, fill);
}

ComPtr<ID2D1EllipseGeometry> Ellipse(float cx, float cy, float rx, float ry) {
    ComPtr<ID2D1EllipseGeometry> g;
    Device::Get().D2DFactory()->CreateEllipseGeometry(D2D1::Ellipse(D2D1::Point2F(cx, cy), rx, ry), &g);
    return g;
}

ComPtr<ID2D1RoundedRectangleGeometry> RoundedRect(const D2D1_RECT_F& r, float rx, float ry) {
    ComPtr<ID2D1RoundedRectangleGeometry> g;
    Device::Get().D2DFactory()->CreateRoundedRectangleGeometry(D2D1::RoundedRect(r, rx, ry), &g);
    return g;
}

ComPtr<ID2D1RectangleGeometry> Rectangle(const D2D1_RECT_F& r) {
    ComPtr<ID2D1RectangleGeometry> g;
    Device::Get().D2DFactory()->CreateRectangleGeometry(r, &g);
    return g;
}

ComPtr<ID2D1PathGeometry> RoundedRect(const D2D1_RECT_F& r, float tl, float tr, float br, float bl) {
    return Build([&](ID2D1GeometrySink* sink) {
        sink->BeginFigure(D2D1::Point2F(r.left + tl, r.top), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(r.right - tr, r.top));
        if (tr > 0) sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(r.right, r.top + tr), D2D1::SizeF(tr, tr), 0.f,
                                                  D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
        sink->AddLine(D2D1::Point2F(r.right, r.bottom - br));
        if (br > 0) sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(r.right - br, r.bottom), D2D1::SizeF(br, br), 0.f,
                                                  D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
        sink->AddLine(D2D1::Point2F(r.left + bl, r.bottom));
        if (bl > 0) sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(r.left, r.bottom - bl), D2D1::SizeF(bl, bl), 0.f,
                                                  D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
        sink->AddLine(D2D1::Point2F(r.left, r.top + tl));
        if (tl > 0) sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(r.left + tl, r.top), D2D1::SizeF(tl, tl), 0.f,
                                                  D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    });
}

} // namespace SonicRacingSaveManager::Geometry
