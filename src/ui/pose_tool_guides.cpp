// PoseTool's guides: a move drag catching on what it lines up with.

#include <algorithm>
#include <cassert>
#include <cmath>

#include "edit/history_manager.h"
#include "edit/selection_manager.h"
#include "render/stage_hit.h"
#include "ui/pose_tool.h"

namespace snapper {

void PoseTool::ArmGuides(const StageFrame& frame) {
  assert(drag_ != nullptr);
  assert(frame.scale > 0.0);
  const Project& project = managers_.history->current();
  const Shot* shot = FindShot(project, frame.shot);
  const bool has_shot = shot != nullptr;
  if (!has_shot) {
    return;
  }
  const std::vector<LayerId> picked = managers_.selection->PickedLayers();
  QRectF moving;
  std::vector<QRectF> others;
  for (const Layer& layer : shot->layers) {
    const QRectF shape =
        LayerShape(project, *shot, layer.id, frame.local, frame.scale, cache_)
            .boundingRect()
            .translated(frame.corner);
    const bool is_shown = !shape.isNull();
    const bool is_moving =
        std::find(picked.begin(), picked.end(), layer.id) != picked.end();
    const bool is_ours = is_shown && is_moving;
    if (is_ours) {
      moving = moving.isNull() ? shape : moving.united(shape);
    } else if (is_shown) {
      others.push_back(shape);
    }
  }
  // The frame itself: its edges and middle lines.
  const CanvasSize& canvas = project.canvas;
  others.push_back(QRectF(frame.corner, QSizeF(canvas.width * frame.scale,
                                               canvas.height * frame.scale)));
  drag_->moving = moving;
  drag_->targets = std::move(others);
  drag_->skip = AlignedGuides(drag_->moving, drag_->targets);
}

QPointF PoseTool::Snapped(QPointF total) {
  assert(drag_ != nullptr);
  assert(std::isfinite(total.x()) && std::isfinite(total.y()));
  guides_.clear();
  const bool has_box = is_catching_ && !drag_->moving.isNull();
  if (!has_box) {
    return total;
  }
  const GuideSnap snap = SnapToGuides(drag_->moving.translated(total),
                                      drag_->targets, kGuideReach,
                                      drag_->skip);
  guides_ = snap.lines;
  return total + snap.shift;
}

}  // namespace snapper
