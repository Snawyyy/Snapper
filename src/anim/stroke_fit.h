#ifndef SNAPPER_ANIM_STROKE_FIT_H_
#define SNAPPER_ANIM_STROKE_FIT_H_

#include <QPointF>
#include <QRectF>

#include <span>
#include <vector>

namespace snapper {

// The few points a freehand stroke is drawn with: the stroke's ends and
// the places it bends most, so no part of it strays more than tolerance
// from the line through the kept points (Ramer, Douglas and Peucker).
std::vector<QPointF> SimplifyStroke(std::span<const QPointF> stroke,
                                    double tolerance);

// The points of an oval filling box: eight round points, which the
// curve draws as a near-perfect ellipse at the smooth curvature.
std::vector<QPointF> OvalPlaces(const QRectF& box);
// The four corners of box, clockwise from the top left.
std::vector<QPointF> RectanglePlaces(const QRectF& box);

}  // namespace snapper

#endif  // SNAPPER_ANIM_STROKE_FIT_H_
