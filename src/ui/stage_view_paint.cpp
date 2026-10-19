// StageView's overlay: pick outlines, handles, warp grids and hover.

#include <QLineF>
#include <QPainter>
#include <QPolygonF>

#include <algorithm>
#include <cassert>
#include <vector>

#include "anim/warp_shape.h"
#include "edit/history_manager.h"
#include "edit/selection_manager.h"
#include "render/stage_geometry.h"
#include "render/stage_hit.h"
#include "ui/dot_shape.h"
#include "ui/motion.h"
#include "ui/stage_view.h"
#include "ui/bone_glyph.h"
#include "ui/theme.h"

namespace snapper {
namespace {

// Warp grids: faint lines, dots drawn like every handle.
constexpr QColor kWarpLine{255, 255, 255, 70};
constexpr double kWarpDot = kHandleDraw / 2;
// Lean and swivel handles are bars, long the way they drag.
constexpr double kBarLong = 2 * kHandleDraw;
// The thin outline of the thing under the cursor.
constexpr QColor kHoverLine{255, 255, 255, 200};

// A box of w by h around centre, grown by scale.
QRectF Box(QPointF centre, double w, double h, double scale) {
  assert(scale >= 1.0);
  assert(w > 0.0 && h > 0.0);
  const QSizeF size(w * scale, h * scale);
  return QRectF(centre - QPointF(size.width() / 2, size.height() / 2),
                size);
}

// Squares dots that move by themselves, in yellow, so they read apart
// from plain dots.
void PaintMarks(const WarpOnScreen& warp, QPainter* painter) {
  assert(painter != nullptr);
  assert(warp.points.size() == static_cast<size_t>(warp.grid.PointCount()));
  painter->setPen(QPen(theme::kHandle, 1.5));
  painter->setBrush(Qt::NoBrush);
  constexpr double kSquare = kWarpDot + 5.0;
  for (const int move : warp.moves) {
    painter->drawRect(QRectF(
        warp.to_screen.map(warp.points[static_cast<size_t>(move)]) -
            QPointF(kSquare, kSquare),
        QSizeF(kSquare * 2, kSquare * 2)));
  }
}

// A ring round the centre with a tail on the side the dot trails.
void PaintRing(double radius, QPointF tail, QPainter* painter) {
  assert(painter != nullptr);
  assert(radius > 0.0);
  const double ring = radius + 3.5;
  painter->setPen(QPen(theme::kHandle, 1.5));
  painter->setBrush(Qt::NoBrush);
  painter->drawEllipse(QPointF(), ring, ring);
  const double length = QLineF(QPointF(), tail).length();
  const bool has_tail = length > 0.0;
  if (has_tail) {
    painter->drawLine(QPointF(), tail * ((ring + length) / length));
  }
}

// A dot of one shape at centre, radius already grown by hover. The
// offset is how far the dot is shown from its keyed place, on screen.
void PaintDot(QPointF centre, double radius, DotShape shape,
              QPointF offset, bool is_animated, QPainter* painter) {
  assert(painter != nullptr);
  assert(radius > 0.0);
  const bool is_round = shape == DotShape::kPlain || shape == DotShape::kRing;
  const bool is_ring = shape == DotShape::kRing;
  const bool is_diamond = shape == DotShape::kDiamond;
  const bool is_blob = shape == DotShape::kBlob;
  painter->save();
  painter->translate(centre);
  painter->setPen(QPen(theme::kShadow, 1.0));
  painter->setBrush(theme::kHandle);
  if (is_round) {
    painter->drawEllipse(QPointF(), radius, radius);
  }
  if (is_ring) {
    PaintRing(radius, TailOf(offset), painter);
  }
  if (is_diamond) {
    const double reach = radius * 1.4;
    painter->drawPolygon(QPolygonF({QPointF(0, -reach), QPointF(reach, 0),
                                    QPointF(0, reach), QPointF(-reach, 0)}));
  }
  if (is_blob) {
    const Squish squish = SquishOf(offset, is_animated);
    painter->rotate(squish.angle);
    painter->drawEllipse(QPointF(), radius * squish.along,
                         radius * squish.across);
  }
  painter->restore();
}

// Where the keys alone put each dot, as a faint ring tied to the dot by
// a faint line, wherever drags, physics or a collider moved it away:
// dragging a dot moves its key, so the ring shows what the drag edits.
void PaintKeyed(const WarpOnScreen& warp, QPainter* painter) {
  assert(painter != nullptr);
  assert(warp.keyed.size() == warp.points.size());
  constexpr double kApart = 2.0;
  QColor faint = theme::kHandle;
  faint.setAlpha(120);
  painter->setPen(QPen(faint, 1.0, Qt::DotLine));
  painter->setBrush(Qt::NoBrush);
  for (size_t i = 0; i < warp.points.size(); ++i) {
    const QPointF shown = warp.to_screen.map(warp.points[i]);
    const QPointF keyed = warp.to_screen.map(warp.keyed[i]);
    const bool is_moved = QLineF(shown, keyed).length() > kApart;
    if (is_moved) {
      painter->drawLine(keyed, shown);
      painter->drawEllipse(keyed, kWarpDot - 1.0, kWarpDot - 1.0);
    }
  }
}

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
  QPolygonF outline;
  const bool is_whole = hover_pick_->piece.isEmpty();
  if (is_whole) {
    outline = LayerShape(project, *shot, hover_pick_->layer, frame.local,
                         frame.scale, renderer_.cache());
  } else {
    const auto piece = PieceOnScreen(project, *shot, hover_pick_->layer,
                                     hover_pick_->piece, frame.local,
                                     frame.scale, renderer_.cache()->poses());
    outline = piece ? piece->outline : QPolygonF();
  }
  painter->save();
  painter->translate(frame.corner);
  painter->setPen(QPen(kHoverLine, 1.0, Qt::DashLine));
  painter->setBrush(Qt::NoBrush);
  painter->drawPolygon(outline);
  painter->restore();
}

void StageView::PaintTurn(const TurnHandles& turn, LayerId layer,
                          QPainter* painter) {
  assert(painter != nullptr);
  assert(layer.IsValid());
  const double scale = hover_.ScaleOf(GrabKind::kLean,
                                      PoseTool::TurnKey(layer));
  const double swivel = hover_.ScaleOf(GrabKind::kSwivel,
                                       PoseTool::TurnKey(layer));
  painter->setPen(QPen(theme::kShadow, 1.0));
  painter->setBrush(theme::kHandle);
  // Long the way they drag: lean up and down, swivel sideways.
  painter->drawRect(Box(turn.lean, kHandleDraw, kBarLong, scale));
  painter->drawRect(Box(turn.swivel, kBarLong, kHandleDraw, swivel));
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
  for (const Pick& pick : selection.picks()) {
    painter->setPen(QPen(theme::kPick, 2.0));
    painter->setBrush(Qt::NoBrush);
    const bool is_whole = pick.piece.isEmpty();
    if (is_whole) {
      painter->drawPolygon(LayerShape(project, *shot, pick.layer, frame.local,
                                      frame.scale, renderer_.cache()));
      const auto turn = TurnHandlesOf(project, *shot, pick.layer,
                                      frame.local, frame.scale,
                                      renderer_.cache());
      if (turn) {
        PaintTurn(*turn, pick.layer, painter);
      }
      continue;
    }
    const auto outline = PieceOnScreen(project, *shot, pick.layer,
                                       pick.piece, frame.local, frame.scale,
                                       renderer_.cache()->poses());
    PaintWarp(frame, pick, painter);
    painter->setPen(QPen(theme::kPick, 2.0));
    painter->setBrush(Qt::NoBrush);
    if (outline) {
      painter->drawPolygon(outline->outline);
      painter->setPen(QPen(theme::kShadow, 1.0));
      painter->setBrush(theme::kHandle);
      painter->drawEllipse(outline->pivot, kWarpDot, kWarpDot);
    }
  }
  painter->setPen(QPen(theme::kShadow, 1.0));
  painter->setBrush(theme::kHandle);
  for (const IkHandle& handle : IkHandles(project, *shot, selection.layer(),
                                          frame.local, frame.scale,
                                          renderer_.cache()->poses())) {
    const double grow = hover_.ScaleOf(GrabKind::kIkTip,
                                       PoseTool::IkKey(handle.chain));
    painter->drawRect(Box(handle.point, kHandleDraw, kHandleDraw, grow));
  }
  for (const PosedBone& bone : CharacterBonesOnScreen(
           project, *shot, selection.layer(), frame.local, frame.scale,
           renderer_.cache()->poses())) {
    const double grow = hover_.ScaleOf(
        GrabKind::kBone, PoseTool::BoneKey(selection.layer(), bone.id));
    PaintBoneGlyph(painter, bone.base, bone.tip,
                   bone.is_smart ? theme::kCollider : theme::kFaceLight, grow);
  }
}

void StageView::PaintWarp(const StageFrame& frame, const Pick& pick,
                          QPainter* painter) {
  assert(painter != nullptr);
  assert(!pick.piece.isEmpty());
  const Project& project = managers_.history->current();
  const Shot* shot = FindShot(project, frame.shot);
  const auto warp =
      shot != nullptr ? PieceWarpOnScreen(project, *shot, pick.layer,
                                          pick.piece, frame.local,
                                          frame.scale,
                                          renderer_.cache()->poses())
                      : std::nullopt;
  if (!warp) {
    return;
  }
  const auto at = [&warp](int point) {
    return warp->to_screen.map(warp->points[static_cast<size_t>(point)]);
  };
  PaintKeyed(*warp, painter);
  // A faint grid (a shape mesh's triangles), so the dots read as one
  // sheet.
  painter->setPen(QPen(kWarpLine, 1.0));
  painter->setBrush(Qt::NoBrush);
  for (const MeshEdge& edge : WarpEdges(warp->grid)) {
    painter->drawLine(at(edge.first), at(edge.second));
  }
  const bool is_animated = managers_.motion->mode() != MotionMode::kOff;
  for (size_t i = 0; i < warp->points.size(); ++i) {
    const WarpDot dot{pick.layer, pick.piece, static_cast<int>(i)};
    const double grow =
        hover_.ScaleOf(GrabKind::kWarpDot, PoseTool::WarpKey(dot));
    const QPointF shown = warp->to_screen.map(warp->points[i]);
    const QPointF keyed = warp->to_screen.map(warp->keyed[i]);
    PaintDot(shown, kWarpDot * grow, ShapeOf(warp->kinds[i]), shown - keyed,
             is_animated, painter);
  }
  PaintMarks(*warp, painter);
  PaintPickedDots(frame, pick, *warp, painter);
  const auto dot = tool_.PickedDot(frame);
  const bool is_dot_here =
      dot.has_value() && dot->layer == pick.layer && dot->piece == pick.piece;
  if (is_dot_here) {
    PaintReach(*warp, dot->point, painter);
  }
}

void StageView::PaintPickedDots(const StageFrame& frame, const Pick& pick,
                                const WarpOnScreen& warp, QPainter* painter) {
  assert(painter != nullptr);
  assert(!pick.piece.isEmpty());
  painter->setPen(QPen(theme::kPick, 2.0));
  painter->setBrush(Qt::NoBrush);
  for (const WarpDot& picked : tool_.PickedDots(frame)) {
    const bool is_here =
        picked.layer == pick.layer && picked.piece == pick.piece;
    if (is_here) {
      painter->drawEllipse(
          warp.to_screen.map(warp.points[static_cast<size_t>(picked.point)]),
          kWarpDot + 2, kWarpDot + 2);
    }
  }
}

void StageView::PaintReach(const WarpOnScreen& warp, int point,
                           QPainter* painter) {
  assert(painter != nullptr);
  assert(point >= 0 && point < warp.grid.PointCount());
  const QPointF spot = warp.points[static_cast<size_t>(point)];
  const double reach = warp.reach[static_cast<size_t>(point)];
  const QPointF on_screen = warp.to_screen.map(spot);
  // Picked is orange; the faint ring is how far the rubber reaches, in
  // the drawing's own grid cells.
  painter->setPen(QPen(theme::kPick, 2.0));
  painter->setBrush(Qt::NoBrush);
  painter->drawEllipse(on_screen, kWarpDot + 2, kWarpDot + 2);
  const bool has_reach = reach > 0.0;
  if (has_reach) {
    painter->save();
    painter->setTransform(warp.to_screen, true);
    QColor faint = theme::kPick;
    faint.setAlpha(150);
    painter->setPen(QPen(faint, 0.0, Qt::DashLine));
    const QSizeF cell = CellSize(warp.grid, warp.size);
    painter->drawEllipse(spot, reach * cell.width(), reach * cell.height());
    painter->restore();
  }
  painter->setPen(theme::kText);
  painter->drawText(on_screen + QPointF(kWarpDot + 4, -kWarpDot - 4),
                    tr("reach %1").arg(reach, 0, 'f', 1));
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
