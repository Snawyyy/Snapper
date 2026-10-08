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

  bool operator==(const Shot&) const = default;
};

const Layer* FindLayer(const Shot& shot, LayerId id);
Layer* FindLayer(Shot* shot, LayerId id);

}  // namespace snapper

#endif  // SNAPPER_MODEL_SHOT_H_
