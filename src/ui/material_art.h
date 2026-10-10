#ifndef SNAPPER_UI_MATERIAL_ART_H_
#define SNAPPER_UI_MATERIAL_ART_H_

#include <QColor>
#include <QRectF>

#include "ui/dot_kinds.h"

class QPainter;

namespace snapper {

// The frame a tile shows while nothing moves it.
inline constexpr double kRestPhase = 0.2;

// How far a loop has swung at phase 0 to 1 (a full loop): -1 to 1,
// back where it began at phase 1.
double Swing(double phase);

// Draws a tiny looping cartoon of material inside area at phase 0 to 1
// of its loop: Still a dot sitting, Drag a dot with a swinging tail,
// Cloth a waving sheet, Jelly a squashing blob.
void PaintMaterial(QPainter* painter, const QRectF& area, Material material,
                   double phase, const QColor& ink);

}  // namespace snapper

#endif  // SNAPPER_UI_MATERIAL_ART_H_
