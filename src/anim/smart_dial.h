#ifndef SNAPPER_ANIM_SMART_DIAL_H_
#define SNAPPER_ANIM_SMART_DIAL_H_

#include <QString>

#include <map>

#include "base/frame.h"
#include "model/doll.h"
#include "model/layer.h"
#include "model/pose.h"

namespace snapper {

// How near a stop (of the whole dial, 0 to 1) a turn snaps onto it.
constexpr double kDialSnap = 0.04;

// Where a value falls among a dial's stops: between from and to, t of
// the way (0 to 1). On a stop, or past the first or last, from and to
// are the same stop. With no stops both are -1.
struct DialSpot final {
  int from = -1;
  int to = -1;
  double t = 0.0;
};
DialSpot SpotOf(const SmartDial& dial, double value);

// The change dial makes to each piece it drives at value: stops
// blended, the drawing swapped where the dial says. A stop that does
// not name a piece leaves it at rest; a drawing name the piece no longer
// has is skipped (drawing -1).
std::map<QString, PiecePose> DialPoses(const Doll& doll,
                                       const SmartDial& dial, double value);

// The value layer keys for dial at frame, or its rest stop.
double DialValue(const SmartDial& dial, const DollLayer& layer, Frame frame);

// change put under hand: turn, move, skew and order add; scale and
// opacity multiply; hand's drawing wins when it picks one.
PiecePose DialUnder(const PiecePose& change, const PiecePose& hand);

// Puts every dial of doll at frame under poses: the dials pose the
// pieces first and the hand pose adds on top (turn, move, skew and
// order add; scale and opacity multiply; a hand-picked drawing wins).
void ApplyDials(const Doll& doll, const DollLayer& layer, Frame frame,
                std::map<QString, PiecePose>* poses);

// The stop value is near enough to snap to, else value itself.
double SnappedDialValue(const SmartDial& dial, double value);

// What the dial shows at value: a stop's name ("Front"), or the blend
// between two ("Front 60% Right": 60 % of the way to Right).
QString DialText(const SmartDial& dial, double value);

}  // namespace snapper

#endif  // SNAPPER_ANIM_SMART_DIAL_H_
