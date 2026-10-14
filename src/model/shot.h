#ifndef SNAPPER_MODEL_SHOT_H_
#define SNAPPER_MODEL_SHOT_H_

#include <QColor>
#include <QString>

#include <vector>

#include "base/frame.h"
#include "base/id.h"
#include "model/comp.h"
#include "model/dot_group.h"
#include "model/key.h"
#include "model/layer.h"
#include "model/pose.h"
#include "model/reference.h"

namespace snapper {

constexpr int kMaxLayersPerShot = 256;
constexpr int kMaxLinksPerShot = 256;
// 1 copies the leader's movement, 0 ignores it, above 1 exaggerates it.
constexpr double kMaxLinkStrength = 4.0;

struct ShotTag;
using ShotId = Id<ShotTag>;

// One end of a link: a whole layer (empty piece) or one piece of a doll.
struct LinkEnd final {
  LayerId layer;
  QString piece;

  auto operator<=>(const LinkEnd&) const = default;
  bool operator==(const LinkEnd&) const = default;
};

// follower copies how far leader has moved (in x and y) since frame
// from, times strength, on top of its own keys. It never jumps onto the
// leader: at frame from it is where its keys put it.
struct Link final {
  LinkEnd follower;
  LinkEnd leader;
  Frame from;
  double strength = 1.0;

  bool operator==(const Link&) const = default;
};

constexpr int kMaxHoldsPerShot = 256;

// From frame at on, the object layer is held by holder (a doll piece):
// it follows the piece fully, place, turn and scale, on top of its own
// keys. An empty holder (no layer) drops it there: it stays where it was
// at that frame. Changing holder never jumps; the object goes on from
// where it is. The holds of one object, by frame, are its hold track.
struct Hold final {
  LayerId object;
  LinkEnd holder;
  Frame at;

  bool IsDrop() const { return !holder.layer.IsValid(); }
  bool operator==(const Hold&) const = default;
};

// One shot: its own stage, camera and length, placed on the master
// track by order.
struct Shot final {
  ShotId id;
  QString name;
  // Two seconds by default.
  Frame length = Frame(48);
  QColor background = Qt::white;
  // Bottom to top.
  std::vector<Layer> layers;
  Channel<CameraPose> camera;
  // At most one per follower.
  std::vector<Link> links;
  // Named sets of this shot's warp dots, picked with one click.
  std::vector<DotGroup> dot_groups;
  // Objects held by doll pieces, at most one per object and frame.
  std::vector<Hold> holds{};
  // See-through: nothing is drawn where no layer draws. background
  // stays remembered, for turning this off and for formats with no
  // alpha (MP4), which put the shot over it.
  bool is_transparent = false;
  // Videos and pictures to animate over, shown on the Pose tab stage
  // only (never exported), first on top of the timeline.
  std::vector<Reference> references{};
  // The Comp tab's finishing effects, bottom to top, run over the
  // finished frame after every layer.
  std::vector<CompEffect> comp{};
  // The shot's Light: the direction (degrees, 0 up, clockwise) the
  // light comes from, shared by every effect that picks "Shot light".
  Channel<double> light{};

  bool operator==(const Shot&) const = default;
};

const Layer* FindLayer(const Shot& shot, LayerId id);
Layer* FindLayer(Shot* shot, LayerId id);

// The link follower has, or nullptr.
const Link* FindLink(const Shot& shot, const LinkEnd& follower);
// Drops links whose follower or leader layer is gone, and holds whose
// object is gone; a hold whose holder layer is gone becomes a drop at
// its frame, so the object stays where it was.
void DropDeadLinks(Shot* shot);

// The holds of object in shot, earliest first.
std::vector<const Hold*> HoldsOf(const Shot& shot, LayerId object);
// The hold of object in force at frame (the last at or before it), or
// nullptr when it has none yet.
const Hold* HoldAt(const Shot& shot, LayerId object, Frame frame);

}  // namespace snapper

#endif  // SNAPPER_MODEL_SHOT_H_
