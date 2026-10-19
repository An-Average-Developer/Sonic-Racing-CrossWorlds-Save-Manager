#include "UI/Material/Card.h"
#include <algorithm>
#include <unordered_map>
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "Seasonal/SeasonalDecorations.h"
#include "Seasonal/SeasonalTheme.h"
#include "UI/Material/Elevation.h"
#include "UI/Material/PackIcon.h"

namespace SonicRacingSaveManager::UI::Material {

namespace {

// SeasonalDecorations: the variant is assigned once per element, in the order they first appear
int VariantOf(Id id) {
    static std::unordered_map<Id, int> variants;
    static int next = 0;
    auto it = variants.find(id);
    if (it == variants.end()) it = variants.emplace(id, next++).first;
    return it->second;
}

} // namespace

Card::Card(Ui& u, Id cardId, const Rect& r, const Edges& padding, bool decorate, float alpha, const CardLook& cardLook)
    : ui(u), id(cardId), decorated(decorate), opacity(alpha), look(cardLook) {
    bounds = r;
    content = r.Inset(padding);
    if (opacity < 1.f) {
        ui.g.PushOpacity(opacity);
        ui.PushTranslate(0.f, 12.f * (1.f - opacity));
        moved = true;
    }
    Elevation(ui.g, r, look.radius, look.elevation);
    ui.g.FillRound(r, look.radius, P().cardBackground);
}

Card::~Card() {
    if (!look.border.Transparent())
        ui.g.StrokeRound(bounds.Inset(look.borderWidth * 0.5f), std::max(0.f, look.radius - look.borderWidth * 0.5f), look.border, look.borderWidth);
    // The decorations live in the adorner layer, above the card's content
    if (decorated) Seasonal::Decorations::Card(ui.g, bounds, VariantOf(id), Seasonal::LastAnimationTime());
    if (moved) ui.PopTransform();
    if (opacity < 1.f) ui.g.PopLayer();
}

float CardFade(Ui& ui, double loadedAt) {
    const float t = Anim::Clamp01(static_cast<float>((ui.Time() - loadedAt) / 0.3));
    if (t < 1.f) ui.KeepAnimating();
    return Anim::EaseOutCubic(t);
}

void MaintenanceOverlay(Ui& ui, const Rect& r, float radius, bool underWord, const wchar_t* caption, float lockSize) {
    ui.Block(r, false);   // the list it sits in still scrolls under it
    ui.g.FillRound(r, radius, Argb(0xCC808080));
    TextStyle word = Bold(11.f);
    word.align = HAlign::Center;
    word.valign = VAlign::Top;
    TextStyle under = Bold(12.f);
    under.align = HAlign::Center;
    under.valign = VAlign::Top;
    TextStyle note = Text(9.f);
    note.align = HAlign::Center;
    note.valign = VAlign::Top;
    // StackPanel centered: lock, (UNDER), MAINTENANCE, (caption)
    const float lockGap = lockSize >= 48.f ? 8.f : 6.f;
    float h = lockSize + lockGap + 15.f;
    if (underWord) h += 16.f + 2.f;
    if (caption) h += 4.f + 12.f;
    float y = r.CY() - h * 0.5f;
    DrawIcon(ui.g, Icon::Lock, { r.CX() - lockSize * 0.5f, y, lockSize, lockSize }, Colors::White.Fade(0.9f));
    y += lockSize + lockGap;
    if (underWord) {
        ui.g.Text(Loc::Get("maintenance_under"), { r.x, y, r.w, 16.f }, Colors::White, under);
        y += 16.f + 2.f;
    }
    ui.g.Text(Loc::Get("maintenance"), { r.x, y, r.w, 15.f }, Colors::White, word);
    y += 15.f;
    if (caption) ui.g.Text(caption, { r.x, y + 4.f, r.w, 12.f }, Colors::White.Fade(0.8f), note);
}

} // namespace SonicRacingSaveManager::UI::Material
