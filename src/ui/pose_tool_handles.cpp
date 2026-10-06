// PoseTool's grabbing of handles: turn handles, warp dots, IK tips.

#include <QLineF>

#include <cassert>
#include <memory>
#include <variant>
#include <vector>

#include "anim/warp.h"
#include "edit/history_manager.h"
#include "edit/rig_manager.h"
#include "edit/project_edits.h"
#include "edit/selection_manager.h"
#include "render/stage_geometry.h"
#include "render/stage_hit.h"
#include "ui/pose_tool.h"

namespace snapper {

bool PoseTool::PressLean(QPointF point, const StageFrame& frame) {
  assert(frame.scale > 0.0);
  assert(managers_.selection != nullptr);
  const Project& project = managers_.history->current();
  const Shot* shot = FindShot(project, frame.shot);
  const bool is_ready =
      shot != nullptr && managers_.selection->shot() == frame.shot;
  if (!is_ready) {
    return false;
  }
  std::vector<LayerId> dolls;
  bool is_lean = false;
  bool is_swivel = false;
  const auto is_near = [&](QPointF handle) {
    return QLineF(handle + frame.corner, point).length() <= kHandleReach;
  };
  for (const Pick& pick : managers_.selection->picks()) {
    const auto handles =
        pick.piece.isEmpty()
            ? TurnHandlesOf(project, *shot, pick.layer, frame.local,
                            frame.scale, cache_)
            : std::nullopt;
    const bool is_doll = handles.has_value();
    if (is_doll) {
      dolls.push_back(pick.layer);
      is_lean = is_lean || is_near(handles->lean);
      is_swivel = is_swivel || (!is_lean && is_near(handles->swivel));
    }
  }
  const bool is_grabbed = is_lean || is_swivel;
  if (!is_grabbed) {
    return false;
  }
  const Layer* first = FindLayer(*shot, dolls.front());
  const QString what =
      dolls.size() > 1
          ? QObject::tr("%1 dolls").arg(dolls.size())
          : std::get<DollLayer>(first->content).doll;
  auto drag = std::make_unique<Drag>();
  drag->kind = is_lean ? Kind::kLean : Kind::kSwivel;
  drag->frame = frame;
  drag->dolls = std::move(dolls);
  drag->start = point;
  drag->scope = std::make_unique<EditScope>(
      managers_.history, is_lean ? QObject::tr("Lean %1").arg(what)
                                 : QObject::tr("Swivel %1").arg(what));
  drag_ = std::move(drag);
  return true;
}

std::optional<std::pair<WarpDot, QTransform>> PoseTool::DotAt(
    QPointF point, const StageFrame& frame) const {
  assert(frame.scale > 0.0);
  assert(managers_.selection != nullptr);
  const Project& project = managers_.history->current();
  const Shot* shot = FindShot(project, frame.shot);
  const bool is_ready =
      shot != nullptr && managers_.selection->shot() == frame.shot;
  if (!is_ready) {
    return std::nullopt;
  }
  for (const Pick& pick : managers_.selection->picks()) {
    const auto warp =
        pick.piece.isEmpty()
            ? std::nullopt
            : PieceWarpOnScreen(project, *shot, pick.layer, pick.piece,
                                frame.local, frame.scale);
    const bool has_grid = warp.has_value();
    if (!has_grid) {
      continue;
    }
    std::vector<QPointF> on_screen;
    for (const QPointF& spot : warp->points) {
      on_screen.push_back(warp->to_screen.map(spot) + frame.corner);
    }
    const int nearest = NearestPoint(on_screen, point, kHandleReach);
    const bool is_hit = nearest >= 0;
    if (is_hit) {
      return std::pair(WarpDot{pick.layer, pick.piece, nearest},
                       warp->to_screen);
    }
  }
  return std::nullopt;
}

bool PoseTool::PressWarp(QPointF point, const StageFrame& frame) {
  assert(frame.scale > 0.0);
  assert(managers_.history != nullptr);
  const auto hit = DotAt(point, frame);
  if (!hit) {
    return false;
  }
  auto drag = std::make_unique<Drag>();
  drag->kind = Kind::kWarp;
  drag->frame = frame;
  drag->layer = hit->first.layer;
  drag->piece = hit->first.piece;
  drag->point = hit->first.point;
  drag->drawing_to_screen = hit->second;
  drag->start = point;
  drag->scope = std::make_unique<EditScope>(
      managers_.history, QObject::tr("Warp %1").arg(hit->first.piece));
  drag_ = std::move(drag);
  return true;
}

std::optional<WarpDot> PoseTool::PickedDot(const StageFrame& frame) const {
  assert(frame.scale > 0.0);
  assert(managers_.selection != nullptr);
  const bool is_here = managers_.selection->shot() == frame.shot;
  return is_here ? managers_.selection->dot() : std::nullopt;
}

bool PoseTool::DropDot() {
  assert(managers_.selection != nullptr);
  const bool had_dot = managers_.selection->dot().has_value();
  managers_.selection->PickDot(std::nullopt);
  assert(!managers_.selection->dot().has_value());
  return had_dot;
}

void PoseTool::WheelReach(const WarpDot& dot, int notches, bool is_fine,
                          const StageFrame& frame) {
  assert(notches != 0);
  assert(dot.point >= 0);
  const Project& project = managers_.history->current();
  const DollLayer* posed = PosedLayerOf(project, frame.shot, dot.layer);
  const Doll* doll = DollOfLayer(project, frame.shot, dot.layer);
  const RigPiece* rig =
      doll != nullptr ? FindRig(doll->rig, dot.piece) : nullptr;
  const bool has_rig = rig != nullptr && posed != nullptr;
  if (!has_rig) {
    return;
  }
  const bool has_reach =
      static_cast<int>(rig->warp_reach.size()) == rig->warp.PointCount();
  const double reach =
      has_reach ? rig->warp_reach[static_cast<size_t>(dot.point)] : 0.0;
  // Wheel up reaches further.
  Note(managers_.rig->SetWarpReach(
      posed->doll, dot.piece, dot.point,
      reach + notches * (is_fine ? kFineReachStep : kReachStep)));
}

void PoseTool::ToggleDrag(const StageFrame& frame) {
  assert(frame.scale > 0.0);
  assert(managers_.rig != nullptr);
  const auto dot = PickedDot(frame);
  const DollLayer* posed =
      dot ? PosedLayerOf(managers_.history->current(), frame.shot,
                         dot->layer)
          : nullptr;
  const bool has_dot = posed != nullptr;
  if (!has_dot) {
    problem_ = QObject::tr("Click a warp dot of a picked piece first.");
    return;
  }
  Note(managers_.rig->ToggleDragNode(posed->doll, dot->piece, dot->point));
}

bool PoseTool::PressIk(QPointF point, const StageFrame& frame) {
  assert(frame.scale > 0.0);
  assert(managers_.selection != nullptr);
  const LayerId layer = managers_.selection->layer();
  const Project& project = managers_.history->current();
  const Shot* shot = FindShot(project, frame.shot);
  const bool is_ready = shot != nullptr && layer.IsValid() &&
                        managers_.selection->shot() == frame.shot;
  if (!is_ready) {
    return false;
  }
  for (const IkHandle& handle :
       IkHandles(project, *shot, layer, frame.local, frame.scale)) {
    const bool is_near =
        QLineF(handle.point + frame.corner, point).length() <= kHandleReach;
    if (is_near) {
      auto drag = std::make_unique<Drag>();
      drag->kind = Kind::kIk;
      drag->frame = frame;
      drag->layer = layer;
      drag->chain = handle.chain;
      drag->start = point;
      drag->scope = std::make_unique<EditScope>(
          managers_.history, QObject::tr("Bend %1").arg(handle.chain));
      drag_ = std::move(drag);
      return true;
    }
  }
  return false;
}

}  // namespace snapper
