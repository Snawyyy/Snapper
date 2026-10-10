#ifndef SNAPPER_UI_STAGE_POKE_H_
#define SNAPPER_UI_STAGE_POKE_H_

#include <QPointF>
#include <Qt>

#include "anim/pose_memo.h"
#include "edit/track_ref.h"
#include "ui/managers.h"
#include "ui/pose_tool.h"

namespace snapper {

// The stage's side of poking (see PokeManager): while P is held a press
// on a doll starts a poke, the mouse shoves it, letting go lets it
// settle. It only turns input into manager calls; a toast says why
// when the doll under the cursor cannot be poked. The cursor is an open
// hand over a doll while P is held and a closed hand while poking.
class StagePoke final {
 public:
  explicit StagePoke(const Managers& managers);

  // P went down or up. Down says why in a toast when under (the layer
  // under the cursor, invalid for none) cannot be poked.
  void Arm(const StageFrame& frame, LayerId under);
  void Disarm();
  bool IsArmed() const { return is_armed_; }
  bool IsPoking() const;

  // A press while armed starts the poke and returns true; the stage
  // then leaves the click alone. Not armed: ends any poke and returns
  // false so the click goes on to the pose tool.
  bool Press(QPointF point, LayerId under, const StageFrame& frame);
  void Move(QPointF point, const StageFrame& frame);
  void Release();
  // Escape: true when it ended a poke.
  bool Cancel();

  // The cursor to show with the mouse over under, or ArrowCursor when
  // poking is not armed.
  Qt::CursorShape CursorOver(LayerId under, const StageFrame& frame) const;
  // Hands the poked poses (or none) to the memo the stage draws from.
  void Sync(PoseMemo* memo);

 private:
  // Where point is in shot space, the unit a shove is worked in.
  static QPointF ShotPoint(QPointF point, const StageFrame& frame);

  Managers managers_;
  bool is_armed_ = false;
  // The layer the memo was last given poses for.
  LayerId synced_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_STAGE_POKE_H_
