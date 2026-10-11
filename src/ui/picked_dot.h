#ifndef SNAPPER_UI_PICKED_DOT_H_
#define SNAPPER_UI_PICKED_DOT_H_

#include <QString>

#include <memory>
#include <optional>
#include <vector>

#include "model/doll.h"
#include "ui/managers.h"

namespace snapper {

// The warp dot picked on the stage, as the dot panels edit it: the doll
// it belongs to, the layer it is picked on, its piece and point, and the
// piece's rig as that layer shows it (its own drag nodes, motions and
// reach, else the doll's), good until the next change.
struct PickedDot final {
  QString doll;
  LayerId layer;
  QString piece;
  int point = -1;
  std::shared_ptr<const RigPiece> rig;
};

// The picked dot, or nothing when no dot of a doll piece is picked.
std::optional<PickedDot> PickedDotOf(const Managers& managers);

// Every picked dot of a doll piece, the focused one first.
std::vector<PickedDot> PickedDotsOf(const Managers& managers);

}  // namespace snapper

#endif  // SNAPPER_UI_PICKED_DOT_H_
