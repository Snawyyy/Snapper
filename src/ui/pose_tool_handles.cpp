// PoseTool's grabbing of handles: turn handles, warp dots, IK tips.

#include <QLineF>

#include <cassert>
#include <memory>
#include <variant>
#include <vector>

#include "anim/warp.h"
#include "edit/history_manager.h"
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

bool PoseTool::PressWarp(QPointF point, const StageFrame& frame) {
  assert(frame.scale > 0.0);
  assert(managers_.selection != nullptr);
  const Project& project = managers_.history->current();
  const Shot* shot = FindShot(project, frame.shot);
  const bool is_ready =
      shot != nullptr && managers_.selection->shot() == frame.shot;
  if (!is_ready) {
    return false;
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
    const bool is_grabbed = nearest >= 0;
    if (is_grabbed) {
      auto drag = std::make_unique<Drag>();
      drag->kind = Kind::kWarp;
      drag->frame = frame;
      drag->layer = pick.layer;
      drag->piece = pick.piece;
      drag->point = nearest;
      drag->pushed = warp->offsets.empty()
                         ? QPointF()
                         : warp->offsets[static_cast<size_t>(nearest)];
      drag->drawing_to_screen = warp->to_screen;
      drag->start = point;
      drag->scope = std::make_unique<EditScope>(
          managers_.history, QObject::tr("Warp %1").arg(pick.piece));
      drag_ = std::move(drag);
      return true;
    }
  }
  return false;
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
