#include "UI/Widgets/Controls.h"
#include <algorithm>
#include <cmath>
#include "UI/Widgets/Button.h"

namespace SonicRacingSaveManager::UI {

bool Slider(Ui& ui, Id id, const Rect& r, float& value, float minimum, float maximum, float step) {
    const Interaction it = ui.Interact(id, r, true, false);
    bool changed = false;
    const float trackX = r.x + 6.f, trackW = r.w - 12.f;
    if (it.held) {
        float t = std::clamp((ui.MouseX() - trackX) / trackW, 0.f, 1.f);
        float v = minimum + t * (maximum - minimum);
        if (step > 0.f) v = std::round(v / step) * step;
        if (v != value) {
            value = v;
            changed = true;
        }
    }
    if (it.hovered && ui.In().wheel != 0.f) {
        value = std::clamp(value + (ui.In().wheel > 0 ? step : -step), minimum, maximum);
        ui.In().wheel = 0.f;
        changed = true;
    }
    const float t = maximum > minimum ? (value - minimum) / (maximum - minimum) : 0.f;
    // The stock WPF (Aero2) slider the old window used: light 4 px track, 11×18 grey thumb
    const float cy = r.CY();
    ui.g.Border({ trackX, cy - 2.f, trackW, 4.f }, 0.f, Hex(0xE7EAEA), Hex(0xD6D6D6), 1.f);
    const float tx = std::round(trackX + trackW * t);
    const Rect thumb{ tx - 5.5f, cy - 9.f, 11.f, 18.f };
    if (it.held) ui.g.Border(thumb, 2.f, Hex(0xCCE4F7), Hex(0x569DE5), 1.f);
    else if (it.hovered) ui.g.Border(thumb, 2.f, Hex(0xDCECFC), Hex(0x7EB4EA), 1.f);
    else ui.g.Border(thumb, 2.f, Hex(0xF0F0F0), Hex(0xACACAC), 1.f);
    return changed;
}

namespace {

struct ComboState {
    bool open = false;
    int picked = -1;   // chosen in the dropdown, applied on the next call
};

} // namespace

bool ComboBox(Ui& ui, Id id, const Rect& r, int& selected, const std::vector<std::wstring>& items) {
    ComboState& state = ui.Data<ComboState>(HashId(id, 0x0CE));   // stable address (heap slot)
    bool changed = false;
    if (state.picked >= 0) {
        changed = state.picked != selected;
        selected = state.picked;
        state.picked = -1;
    }
    ButtonStyle style{ Palette::ButtonBg, Palette::ControlBorder, 1.f, 3.f, { 10.f, 0.f }, 0.f, r.h };
    {
        ButtonScope b(ui, id, r, style);
        if (b.Clicked()) state.open = !state.open;
        const std::wstring current = selected >= 0 && selected < static_cast<int>(items.size()) ? items[static_cast<size_t>(selected)] : L"";
        ui.g.Text(current, b.content.CutRight(18.f), Palette::Label, Mono(10.f, true));
        ui.g.Text(L"\x25BC", r.Right(18.f), Palette::Label, Mono(7.f).Center());
    }
    if (state.open) {
        // The list draws on top of everything else
        const Rect anchor = r;
        const int current = selected;
        ComboState* s = &state;
        ui.Defer([&ui, id, anchor, current, s, items] {
            const float itemH = ui.g.Measure(L"Ag", Mono(10.f)).h + 10.f;
            const Rect list{ anchor.x, anchor.B(), anchor.w, itemH * static_cast<float>(items.size()) + 2.f };
            ui.Block(list);
            ui.g.Border(list, 0.f, Palette::ButtonBg, Palette::ControlBorder, 1.f);
            for (size_t i = 0; i < items.size(); ++i) {
                const Rect row{ list.x + 1.f, list.y + 1.f + itemH * i, list.w - 2.f, itemH };
                const Interaction it = ui.Interact(HashId(static_cast<int64_t>(i), HashId(id, 0x1157)), row);
                const bool isSelected = static_cast<int>(i) == current;
                if (isSelected) ui.g.FillRect(row, Hex(0x0F2438));
                if (it.hovered) ui.g.FillRect(row, Hex(0x123049));
                ui.g.Text(items[i], row.Inset(Edges{ 10.f, 0.f }), isSelected ? Palette::Teal : Palette::Label, Mono(10.f));
                if (it.clicked) {
                    s->picked = static_cast<int>(i);
                    s->open = false;
                }
            }
            // Click anywhere else closes it
            if (ui.In().pressed[0] && !ui.MouseOver(list) && !ui.MouseOver(anchor)) s->open = false;
        });
    }
    return changed;
}

void ProgressBar(Ui& ui, const Rect& r, float value, Color fill, Color track) {
    const float radius = std::min(3.f, r.h * 0.5f);
    ui.g.FillRound(r, radius, track);
    const float w = r.w * std::clamp(value, 0.f, 1.f);
    if (w > 0.5f) ui.g.FillRound({ r.x, r.y, w, r.h }, radius, fill);
}

void ProgressBarIndeterminate(Ui& ui, const Rect& r, Color fill, Color track) {
    const float radius = std::min(3.f, r.h * 0.5f);
    ui.g.FillRound(r, radius, track);
    const float t = static_cast<float>(std::fmod(ui.Time(), 1.6) / 1.6);
    const float segment = r.w * 0.3f;
    const float x = r.x - segment + (r.w + segment) * t;
    ui.PushClip(r);
    ui.g.FillRound({ x, r.y, segment, r.h }, radius, fill);
    ui.PopClip();
    ui.KeepAnimatingThrottled();
}

void Divider(Ui& ui, float x, float y, float w, Color c) { ui.g.FillRect({ x, y, w, 1.f }, c); }

void SectionLabel(Ui& ui, const Rect& r, const std::wstring& text, Color c) { ui.g.Text(text, r, c, Mono(9.f, true)); }

Rect RightAligned(const Rect& row, float controlWidth, float controlHeight) {
    return { row.R() - controlWidth, row.y + (row.h - controlHeight) * 0.5f, controlWidth, controlHeight };
}

float Paragraph(Ui& ui, float x, float y, float w, const std::wstring& text, const TextStyle& style, Color c) {
    TextStyle s = style;
    s.wrap = true;
    s.valign = VAlign::Top;
    const float h = ui.g.Measure(text, s, w).h;
    ui.g.Text(text, { x, y, w, h + 2.f }, c, s);
    return h;
}

bool Chip(Ui& ui, Id id, const Rect& r, const std::wstring& text, bool selected, Color accent) {
    ButtonStyle s{ selected ? Rgba(0, 40, 30) : Palette::OptionBg, selected ? accent : Palette::ButtonBorder, 1.f, 3.f,
                   { 10.f, 4.f }, 0.f, r.h };
    return TextButton(ui, id, r, text, Mono(10.f, true).Center(), selected ? accent : Palette::Dim, s);
}

} // namespace SonicRacingSaveManager::UI
