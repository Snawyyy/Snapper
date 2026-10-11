#ifndef SNAPPER_ANIM_JELLY_FIT_H_
#define SNAPPER_ANIM_JELLY_FIT_H_

// The smooth shape of a jelly blob's swing: one shift plus one squash,
// stretch and turn, however many dots the blob has.

#include <QPointF>

#include <vector>

namespace snapper {

// A swing that is the same kind of smooth everywhere: at a spot q (the
// blob's own units, 1 at its edge) the swing is shift + (a b; c d) q.
struct Swing final {
  QPointF shift;
  double a = 0.0;
  double b = 0.0;
  double c = 0.0;
  double d = 0.0;
};

// The swing at spot q.
QPointF SwingAt(const Swing& swing, QPointF q);

Swing Plus(const Swing& x, const Swing& y);
Swing Times(const Swing& x, double k);

// How hard each part of a swing is pulled back (a share of the way to
// nothing; a quick spring pulls past it, up to 2): the shift and the
// squash, stretch and turn have each their own.
struct Pull final {
  double shift = 0.0;
  double squash = 0.0;
};

// swing with its shift pulled back by pull.shift and its squash by
// pull.squash, each a share of the way to nothing.
Swing PulledBack(const Swing& swing, const Pull& pull);

// The one swing of the form above that comes nearest the values (one
// per spot, a swing of that spot's dot), where dot i counts for
// weight[i] (0 to 1: a dot held near a pin swings less, so it counts
// for how much of the swing it takes). Few or lined up spots give
// little squash and stretch rather than a wild one.
Swing FitSwing(const std::vector<QPointF>& spots,
               const std::vector<double>& weights,
               const std::vector<QPointF>& values);

// swing with its squash and stretch kept within most (0 to 1 over the
// blob's whole size: 0.6 keeps every side above 0.4 of its length, so
// triangles never turn over; bending smoothly, not cutting off) and its
// area pulled back toward the same by keep (0 lets it squash flat, 1
// keeps the area).
Swing Bounded(const Swing& swing, double keep, double most);

}  // namespace snapper

#endif  // SNAPPER_ANIM_JELLY_FIT_H_
