// CompStage: the outline of what is picked, so it is clear what a new
// effect goes on, and library effects dropped on the stage.

#include <QDragEnterEvent>
#include <QPainter>

#include <cassert>

#include "edit/comp_manager.h"
#include "edit/history_manager.h"
#include "edit/selection_manager.h"
#include "model/project.h"
#include "render/stage_geometry.h"
#include "render/stage_hit.h"
#include "ui/comp_drag.h"
#include "ui/comp_stage.h"
#include "ui/problem.h"
#include "ui/theme.h"

namespace snapper {

void CompStage::PaintPicks(QPainter* painter) {
  assert(painter != nullptr);
  assert(managers_.selection != nullptr);
  const Project& project = managers_.history->current();
  const Shot* shot = FindShot(project, managers_.selection->shot());
  const bool has_shot = shot != nullptr;
  if (!has_shot) {
    return;
  }
  const Frame local = Local();
  const double scale = Scale();
  painter->save();
  painter->translate(FrameRect().topLeft());
  painter->setPen(QPen(theme::kPick, 2.0));
  painter->setBrush(Qt::NoBrush);
  for (const Pick& pick : managers_.selection->picks()) {
    const bool is_whole = pick.piece.isEmpty();
    if (is_whole) {
      painter->drawPolygon(LayerShape(project, *shot, pick.layer, local, scale,
                                      renderer_.cache()));
      continue;
    }
    const auto piece = PieceOnScreen(project, *shot, pick.layer, pick.piece,
                                     local, scale, renderer_.cache()->poses());
    const bool has_piece = piece.has_value();
    if (has_piece) {
      painter->drawPolygon(piece->outline);
    }
  }
  const bool has_drop = drop_layer_.IsValid();
  if (has_drop) {
    painter->setPen(QPen(theme::kPick, 2.0, Qt::DashLine));
    painter->drawPolygon(LayerShape(project, *shot, drop_layer_, local, scale,
                                    renderer_.cache()));
  }
  painter->restore();
}

LayerId CompStage::LayerUnder(QPointF point) {
  assert(managers_.history != nullptr);
  assert(managers_.selection != nullptr);
  const Project& project = managers_.history->current();
  const Shot* shot = FindShot(project, managers_.selection->shot());
  const bool has_shot = shot != nullptr;
  if (!has_shot) {
    return LayerId();
  }
  const auto hit = HitTest(project, *shot, Local(),
                           point - FrameRect().topLeft(), Scale(),
                           renderer_.cache());
  return hit ? hit->layer : LayerId();
}

void CompStage::dragEnterEvent(QDragEnterEvent* event) {
  assert(event != nullptr);
  assert(managers_.comp != nullptr);
  const bool is_entry = EntryOf(event->mimeData()).has_value();
  if (is_entry) {
    event->acceptProposedAction();
    drop_layer_ = LayerUnder(event->position());
    update();
  }
}

void CompStage::dragMoveEvent(QDragMoveEvent* event) {
  assert(event != nullptr);
  assert(event->mimeData() != nullptr);
  event->acceptProposedAction();
  const LayerId under = LayerUnder(event->position());
  const bool is_new = under != drop_layer_;
  if (is_new) {
    drop_layer_ = under;
    ShowChip(under.IsValid() ? tr("Drop on this doll")
                             : tr("Drop on the screen"));
  }
}

void CompStage::dragLeaveEvent(QDragLeaveEvent* event) {
  assert(event != nullptr);
  assert(managers_.comp != nullptr);
  drop_layer_ = LayerId();
  update();
}

void CompStage::dropEvent(QDropEvent* event) {
  assert(event != nullptr);
  assert(managers_.comp != nullptr);
  const LayerId layer = LayerUnder(event->position());
  drop_layer_ = LayerId();
  update();
  const auto entry = EntryOf(event->mimeData());
  const bool is_entry = entry.has_value();
  if (!is_entry) {
    return;
  }
  event->acceptProposedAction();
  const ShotId shot = managers_.selection->shot();
  const auto target = managers_.comp->TargetFor(shot, layer, {});
  if (!target) {
    Report(target.error().message);
    return;
  }
  // The pick follows, so the effect list shows where it went.
  const bool is_screen = !layer.IsValid();
  if (is_screen) {
    managers_.selection->Clear();
  } else {
    managers_.selection->SelectLayer(layer);
  }
  Report(ProblemOf(managers_.comp->AddEntry(shot, *entry, *target, kOnTop)));
}

}  // namespace snapper
