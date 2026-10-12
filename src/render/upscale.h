#ifndef SNAPPER_RENDER_UPSCALE_H_
#define SNAPPER_RENDER_UPSCALE_H_

#include <QImage>
#include <QSize>

namespace snapper {

// The most a drawing is upscaled for the Smooth look.
constexpr int kMaxUpscale = 4;
// A drawing with a side past this many pixels upscales at most 2x, to
// keep memory in check.
constexpr int kLargeDrawingSide = 2048;

// source made times (2 or 4) as large with Scale2x (EPX, the simplest of
// the xBR family) once or twice: steps along a diagonal edge become
// smaller steps, colors are never mixed, so the result has only the
// drawing's own colors. source is premultiplied ARGB32 and so is the
// result. Pixel (x, y) of source covers the same area as the times by
// times block at (x * times, y * times), so placement never moves.
QImage UpscaleDrawing(const QImage& source, int times);

// How many times to upscale a drawing of size drawn at scale (screen
// pixels per drawing pixel): 2 up to 1:1, 4 when zoomed past it; never
// more than 2 for a large drawing.
int UpscaleTimes(QSize size, double scale);

}  // namespace snapper

#endif  // SNAPPER_RENDER_UPSCALE_H_
