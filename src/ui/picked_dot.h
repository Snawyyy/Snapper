#ifndef SNAPPER_UI_PICKED_DOT_H_
#define SNAPPER_UI_PICKED_DOT_H_

#include <QString>

#include <optional>

#include "model/doll.h"
#include "ui/managers.h"

namespace snapper {

// The warp dot picked on the stage, as the dot panels edit it: the doll
// it belongs to, its piece and point, and the piece's rig as it is now
// (good until the next change).
struct PickedDot final {
  QString doll;
  QString piece;
  int point = -1;
  const RigPiece* rig = nullptr;
};

// The picked dot, or nothing when no dot of a doll piece is picked.
std::optional<PickedDot> PickedDotOf(const Managers& managers);

}  // namespace snapper

#endif  // SNAPPER_UI_PICKED_DOT_H_
