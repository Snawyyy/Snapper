#ifndef SNAPPER_MODEL_SHOT_H_
#define SNAPPER_MODEL_SHOT_H_

#include <QColor>
#include <QString>

#include <vector>

#include "base/frame.h"
#include "base/id.h"
#include "model/comp.h"
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

// One end of a link: a layer.
struct LinkEnd final {
  LayerId layer;

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
// Drops links whose follower or leader layer is gone.
void DropDeadLinks(Shot* shot);

}  // namespace snapper

#endif  // SNAPPER_MODEL_SHOT_H_
