#ifndef SNAPPER_MODEL_SHOT_H_
#define SNAPPER_MODEL_SHOT_H_

#include <QColor>
#include <QString>

#include <vector>

#include "base/frame.h"
#include "base/id.h"
#include "model/key.h"
#include "model/layer.h"
#include "model/pose.h"

namespace snapper {

constexpr int kMaxLayersPerShot = 256;
constexpr int kMaxLinksPerShot = 256;
// 1 copies the leader's movement, 0 ignores it, above 1 exaggerates it.
constexpr double kMaxLinkStrength = 4.0;

struct ShotTag;
using ShotId = Id<ShotTag>;

enum class TransitionKind {
  kCut,
  kSwipeLeft,
  kSwipeRight,
  kSwipeUp,
  kSwipeDown,
  kFlash,
  kCrossfade,
};
constexpr int kTransitionKindCount = 7;

// How a shot hands over to the next one. The next shot starts length
// frames before this one ends, and the two are mixed over that overlap.
struct Transition final {
  TransitionKind kind = TransitionKind::kCut;
  Frame length;

  bool operator==(const Transition&) const = default;
};

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
  Transition transition;
  // At most one per follower.
  std::vector<Link> links;

  bool operator==(const Shot&) const = default;
};

const Layer* FindLayer(const Shot& shot, LayerId id);
Layer* FindLayer(Shot* shot, LayerId id);

// The link follower has, or nullptr.
const Link* FindLink(const Shot& shot, const LinkEnd& follower);
// Drops links whose follower or leader layer is gone.
void DropDeadLinks(Shot* shot);

// The overlap actually used: never more than either shot can give.
Frame UsableTransition(const Shot& shot, const Shot* next);

}  // namespace snapper

#endif  // SNAPPER_MODEL_SHOT_H_
