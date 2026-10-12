#ifndef SNAPPER_RENDER_PIXEL_PAINT_H_
#define SNAPPER_RENDER_PIXEL_PAINT_H_

#include <QColor>
#include <QImage>
#include <QPoint>

#include <vector>

namespace snapper {

// The hard pixel brush of draw mode, with no soft edges, so it matches
// Krita's pixel brush on flat-color dolls. Images are Format_ARGB32;
// pixels outside the image are cut off. A transparent color erases.

// The biggest brush, in pixels across.
inline constexpr int kMaxBrushSize = 100;

// The pixels a dab of size pixels across covers, as offsets from its
// middle pixel: a round dot, a single pixel at size 1.
std::vector<QPoint> DabPixels(int size);
// Sets every pixel of a dab at center to color.
void PaintDab(QImage* image, QPoint center, int size, QColor color);
// Dabs along the straight line from from to to, every pixel of it, so
// a fast stroke leaves no gaps.
void PaintLine(QImage* image, QPoint from, QPoint to, int size, QColor color);
// Fills the area of pixels of at's exact color that touch at (edges, not
// corners) with color; every fully clear pixel counts as one color.
// False when at is outside or already color.
bool FloodFill(QImage* image, QPoint at, QColor color);

}  // namespace snapper

#endif  // SNAPPER_RENDER_PIXEL_PAINT_H_
