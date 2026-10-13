#ifndef SNAPPER_RENDER_COMP_EFFECTS_H_
#define SNAPPER_RENDER_COMP_EFFECTS_H_

#include <QColor>
#include <QImage>
#include <QPointF>
#include <QTransform>

#include <cstdint>
#include <vector>

#include "model/comp.h"

namespace snapper {

// The pixel work of the Comp tab's effects (render/comp.cpp runs them).
// Pictures are premultiplied ARGB32 at the output size; a Mask is one
// coverage byte per pixel, row by row, the same size. Sizes are in
// output pixels (already scaled). Each returns what the effect draws,
// on a clear picture, for BlendOnto to lay over the frame.
using Mask = std::vector<std::uint8_t>;

// A repeatable random number from 0 to 1 for a seed and an index.
double Noise(int seed, int index);

// The alpha of picture as a mask.
Mask AlphaOf(const QImage& picture);
// Pixels whose color is within tolerance (0 to 441, RGB distance) of
// one of colors, fading out over feather times tolerance past it.
Mask ColorMaskOf(const QImage& picture, const std::vector<QColor>& colors,
                 double tolerance, double feather);
// a times b, pixel by pixel.
Mask Both(const Mask& a, const Mask& b);

// Distance (pixels, up to limit) from each pixel to the nearest pixel
// across shape's edge: for pixels inside shape (inside true) to the
// outside, else from outside pixels to the shape. Pixels on the other
// side read 0.
std::vector<float> DistanceToEdge(const Mask& shape, int width, int height,
                                  bool inside, float limit);

// Corner rounding over stair steps between two flat colors, without
// blur. With line_colors, only steps touching one of them change.
QImage SmoothLines(const QImage& frame, const std::vector<QColor>& line_colors);
// The swapped pixels only: each swap's from color (within tolerance,
// feathered) becomes its to color, keeping alpha.
QImage SwapColors(const QImage& frame, const std::vector<ColorSwap>& swaps,
                  double tolerance, double feather);
// Glow by distance from shape's edge, outside it or inside it.
QImage EdgeGlow(const Mask& shape, int width, int height, QColor color,
                double size, double falloff, bool inside);
// A band on the edges of shape that face a light coming from angle
// (degrees, 0 up, clockwise), width wide, fading inward by falloff.
QImage RimBand(const Mask& shape, int width, int height, QColor color,
               double angle, double band, double falloff);

// How a gradient sits on the output: space maps the effect's point
// space (canvas pixels from the middle, or a layer's own) to output
// pixels.
struct GradientPlace final {
  GradientShape shape = GradientShape::kLinear;
  QPointF a;
  QPointF b;
  QTransform space;
  int steps = 0;
  // For kFromEdge: the target's shape and the ramp's length.
  const Mask* edge = nullptr;
  double edge_size = 1.0;
};
QImage GradientOver(const std::vector<GradientStop>& stops,
                    const GradientPlace& place, int width, int height);

// source smeared along angle (degrees, 0 up), length long, with streak
// noise (0 none, 1 strong).
QImage SmearAlong(const QImage& source, double angle, double length,
                  double streaks, int seed);
// Star streaks and glow from source's pixels brighter than threshold.
QImage Sparkles(const QImage& source, QColor color, double threshold,
                int streaks, double length, double rotation, double scale);
// Burst lines around center (style 0) or parallel lines along angle
// (style 1), new for each seed.
QImage SpeedLines(QSize size, int style, QPointF center, double angle,
                  int density, double thickness, double clear, QColor color,
                  int seed);
// Grain (style 0), scanlines (1) or posterize (2) of frame.
QImage Finish(const QImage& frame, int style, double size, int levels,
              int seed);

}  // namespace snapper

#endif  // SNAPPER_RENDER_COMP_EFFECTS_H_
