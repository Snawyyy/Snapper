// CompStage: the outline of what is picked, so it is clear what a new
// effect goes on.

#include <QPainter>

#include <cassert>

#include "edit/history_manager.h"
#include "edit/selection_manager.h"
#include "model/project.h"
#include "render/stage_geometry.h"
#include "render/stage_hit.h"
#include "ui/comp_stage.h"
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
  painter->restore();
}

}  // namespace snapper
