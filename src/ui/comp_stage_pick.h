#ifndef SNAPPER_UI_COMP_STAGE_PICK_H_
#define SNAPPER_UI_COMP_STAGE_PICK_H_

#include <QPainter>
#include <QPointF>
#include <QRectF>

#include <optional>

#include "anim/reel_timeline.h"
#include "edit/selection_manager.h"
#include "render/image_cache.h"
#include "ui/managers.h"

namespace snapper {

// Where the stage draws the frame: its top left in widget pixels, the
// scale it is drawn at, and the shot clip on top at the reel playhead.
struct CompStageSpot final {
  QPointF corner;
  double scale = 1.0;
  ReelShot at;
};

// Picking on the Comp stage, by the shared pick rule (ui/pick_keys.h):
// a click picks the piece or layer under it (Ctrl flips it, Shift adds
// it, a plain click on something picked keeps the pick until release),
// and a box dragged from empty space picks what it touches (Shift adds,
// Ctrl takes out). Pieces of several dolls can be picked together; the
// Pieces tile of the Comp panel takes them all. The stage forwards its
// mouse events here and paints Paint on top.
class CompStagePick final {
 public:
  explicit CompStagePick(const Managers& managers, ImageCache* cache);

  // A left press at point (widget pixels).
  void Press(QPointF point, Qt::KeyboardModifiers mods,
             const CompStageSpot& spot);
  void Move(QPointF point);
  // Finishes the click or the box; true when the pick changed.
  bool Release(QPointF point, const CompStageSpot& spot);
  // Drops a box in progress (Escape).
  void Cancel();
  bool IsBoxing() const { return is_boxing_; }
  // The box being dragged, in widget pixels.
  QRectF Box() const;
  // Outlines the picked pieces and layers (Teto red) and the box.
  void Paint(QPainter* painter, const CompStageSpot& spot);

 private:
  Managers managers_;
  ImageCache* cache_;
  bool is_boxing_ = false;
  QPointF start_;
  QPointF corner_;
  PickMode box_mode_ = PickMode::kReplace;
  // A plain press on something picked waits for the release.
  std::optional<Pick> later_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_COMP_STAGE_PICK_H_
