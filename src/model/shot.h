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

  bool operator==(const Shot&) const = default;
};

const Layer* FindLayer(const Shot& shot, LayerId id);
Layer* FindLayer(Shot* shot, LayerId id);

// The overlap actually used: never more than either shot can give.
Frame UsableTransition(const Shot& shot, const Shot* next);

}  // namespace snapper

#endif  // SNAPPER_MODEL_SHOT_H_
