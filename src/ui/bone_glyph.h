#ifndef SNAPPER_UI_BONE_GLYPH_H_
#define SNAPPER_UI_BONE_GLYPH_H_

#include <QColor>
#include <QPainter>
#include <QPointF>

namespace snapper {

// A bone as Moho draws one, in widget pixels: a long diamond from base
// to tip, widest near the base, body colored, with a yellow grab dot at
// the tip (grow scales it for hover). Shared by the Character tab and
// the Pose stage so a bone looks the same everywhere.
void PaintBoneGlyph(QPainter* painter, QPointF base, QPointF tip,
                    const QColor& body, double grow = 1.0);

}  // namespace snapper

#endif  // SNAPPER_UI_BONE_GLYPH_H_
