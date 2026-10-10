#ifndef SNAPPER_ANIM_OUTLINE_TRACE_H_
#define SNAPPER_ANIM_OUTLINE_TRACE_H_

#include <QImage>
#include <QPointF>

#include <vector>

namespace snapper {

// A pixel counts as visible from this alpha (0 to 255) up.
constexpr int kOutlineAlpha = 128;
// One dot about every this many pixels of outline.
constexpr double kOutlineSpacing = 12.0;

// Traces the outer outline of the visible pixels of image into a closed
// ring of dots, in image pixels (a pixel's corners are whole numbers),
// spaced evenly along the outline about spacing apart and never more
// than max_dots. Of several separate shapes it follows the one met
// first from the top left. The ring runs with the shape on its right
// (clockwise on a screen with y down), so the outside is on the left of
// each step. Empty when nothing is visible.
std::vector<QPointF> TraceOutline(const QImage& image, int alpha_threshold,
                                  double spacing, int max_dots);

// A closed ring of corners with its staircases straightened: no corner
// it drops was further than tolerance from the ring that is left.
std::vector<QPointF> SimplifyRing(const std::vector<QPointF>& ring,
                                  double tolerance);

}  // namespace snapper

#endif  // SNAPPER_ANIM_OUTLINE_TRACE_H_
