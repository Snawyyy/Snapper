#ifndef SNAPPER_UI_STYLE_BEVEL_H_
#define SNAPPER_UI_STYLE_BEVEL_H_

#include <QColor>
#include <QPainter>
#include <QRect>

namespace snapper {

// The style's edges, shared by everything it draws raised or sunk.

// A one pixel bevel over fill: light on the top and left, shadow on the
// bottom and right, or the other way round when sunk.
void DrawBevel(QPainter* painter, const QRect& rect, const QColor& fill,
               bool is_sunk);

// A square sunken frame with no fill: shadow on the top and left, light
// on the bottom and right.
void DrawSunkenFrame(QPainter* painter, const QRect& rect);

}  // namespace snapper

#endif  // SNAPPER_UI_STYLE_BEVEL_H_
