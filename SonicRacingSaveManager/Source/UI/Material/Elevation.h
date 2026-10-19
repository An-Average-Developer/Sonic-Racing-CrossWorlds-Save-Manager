#pragma once
#include "UI/Ui.h"

// ElevationAssist.Elevation + RippleAssist: the two effects every MaterialDesign control shares.
namespace SonicRacingSaveManager::UI::Material {

// DropShadowEffect of MaterialDesign's DpN shadows (fractional dp while an elevation animates).
// `darken` = ShadowAssist.Darken (raised buttons on hover): the shadow goes fully opaque.
void Elevation(Graphics& g, const Rect& r, float radius, float dp, bool darken = false);
void EllipseElevation(Graphics& g, float cx, float cy, float rx, float dp, bool darken = false);
// Shadow of an ellipse with a transparent fill: only its outline casts it (the header logo FABs).
void RingElevation(Graphics& g, float cx, float cy, float radius, float thickness, float dp, bool darken = false);

// wpf:Ripple: a circle grows from the press point over the control and fades out after release.
// Call after the control's Interact, inside whatever clip the ripple must respect.
void Ripple(Ui& ui, Id id, const Interaction& state, const Rect& r, float radius, Color feedback);

} // namespace SonicRacingSaveManager::UI::Material
