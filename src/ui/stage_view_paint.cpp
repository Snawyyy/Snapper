// StageView's overlay: pick outlines, bones and hover.

#include <QLineF>
#include <QPainter>
#include <QPolygonF>

#include <algorithm>
#include <cassert>
#include <vector>

#include "edit/history_manager.h"
#include "edit/selection_manager.h"
#include "render/stage_geometry.h"
#include "render/stage_hit.h"
#include "ui/stage_view.h"
#include "ui/bone_glyph.h"
#include "ui/theme.h"

namespace snapper {
namespace {

// The thin outline of the thing under the cursor.
constexpr QColor kHoverLine{255, 255, 255, 200};

}  // namespace

void StageView::PaintHover(const StageFrame& frame, QPainter* painter) {
  assert(painter != nullptr);
  assert(frame.scale > 0.0);
  const Project& project = managers_.history->current();
  const Shot* shot = FindShot(project, frame.shot);
  const bool has_hover = hover_pick_.has_value() && shot != nullptr;
  if (!has_hover) {
    return;
  }
  const QPolygonF outline =
      LayerShape(project, *shot, hover_pick_->layer, frame.local,
                 frame.scale, renderer_.cache());
  painter->save();
  painter->translate(frame.corner);
  painter->setPen(QPen(kHoverLine, 1.0, Qt::DashLine));
  painter->setBrush(Qt::NoBrush);
  painter->drawPolygon(outline);
  painter->restore();
}

void StageView::PaintHandles(const StageFrame& frame, QPainter* painter) {
  assert(painter != nullptr);
  assert(frame.scale > 0.0);
  const Project& project = managers_.history->current();
  const SelectionManager& selection = *managers_.selection;
  const Shot* shot = FindShot(project, frame.shot);
  const bool is_picked = shot != nullptr && selection.shot() == frame.shot &&
                         !selection.picks().empty();
  if (!is_picked) {
    return;
  }
  painter->translate(frame.corner);
  painter->setPen(QPen(theme::kPick, 2.0));
  painter->setBrush(Qt::NoBrush);
  for (const Pick& pick : selection.picks()) {
    painter->drawPolygon(LayerShape(project, *shot, pick.layer, frame.local,
                                    frame.scale, renderer_.cache()));
  }
  for (const PosedBone& bone : CharacterBonesOnScreen(
           project, *shot, selection.layer(), frame.local, frame.scale)) {
    const double grow = hover_.ScaleOf(
        GrabKind::kBone, PoseTool::BoneKey(selection.layer(), bone.id));
    PaintBoneGlyph(painter, bone.base, bone.tip,
                   bone.is_smart ? theme::kCollider : theme::kFaceLight, grow);
  }
}

void StageView::PaintGuides(QPainter* painter) {
  assert(painter != nullptr);
  assert(managers_.selection != nullptr);
  painter->setPen(QPen(theme::kPick, 1.0));
  for (const GuideLine& line : tool_.guides()) {
    const bool is_vertical = line.is_vertical;
    if (is_vertical) {
      painter->drawLine(QPointF(line.at, 0), QPointF(line.at, height()));
    } else {
      painter->drawLine(QPointF(0, line.at), QPointF(width(), line.at));
    }
  }
}

void StageView::PaintRopes(const StageFrame& frame, QPainter* painter) {
  assert(painter != nullptr);
  assert(frame.scale > 0.0);
  const Project& project = managers_.history->current();
  const Shot* shot = FindShot(project, frame.shot);
  const bool has_shot = shot != nullptr;
  if (!has_shot) {
    return;
  }
  const std::vector<LayerId> picked = managers_.selection->PickedLayers();
  const auto is_picked = [&picked](LayerId layer) {
    return std::find(picked.begin(), picked.end(), layer) != picked.end();
  };
  const auto middle = [&](LayerId layer) {
    return LayerShape(project, *shot, layer, frame.local, frame.scale,
                      renderer_.cache())
               .boundingRect()
               .center() +
           frame.corner;
  };
  painter->setPen(QPen(theme::kPick, 2.0, Qt::DashLine, Qt::RoundCap));
  for (const Link& link : shot->links) {
    const bool is_shown = is_picked(link.follower.layer) ||
                          is_picked(link.leader.layer);
    if (is_shown) {
      painter->drawLine(middle(link.follower.layer),
                        middle(link.leader.layer));
    }
  }
}

}  // namespace snapper
