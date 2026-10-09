// PoseTool's grabbing of handles: turn handles, warp dots, IK tips.

#include <algorithm>
#include <cassert>
#include <cmath>
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

QString PoseTool::WarpKey(const WarpDot& dot) {
  assert(dot.point >= 0);
  assert(!dot.piece.isEmpty());
  return QStringLiteral("dot/%1/%2/%3")
      .arg(dot.layer.value())
      .arg(dot.piece)
      .arg(dot.point);
}

QString PoseTool::TurnKey(LayerId layer) {
  assert(layer.IsValid());
  assert(layer.value() > 0);
  return QStringLiteral("turn/%1").arg(layer.value());
}

QString PoseTool::IkKey(const QString& chain) {
  assert(!chain.isEmpty());
  assert(chain.size() < 100000);
  return QStringLiteral("ik/") + chain;
}

void PoseTool::Observe() {
  assert(managers_.selection != nullptr);
  assert(clock_.isValid());
  const bool is_shown = !managers_.selection->picks().empty();
  grace_.Update(is_shown, NowMs());
  if (is_shown) {
    shown_picks_ = managers_.selection->picks();
    shown_layer_ = managers_.selection->layer();
    shown_shot_ = managers_.selection->shot();
  }
}

std::set<Pick> PoseTool::GrabbablePicks(const StageFrame& frame) const {
  assert(frame.scale > 0.0);
  assert(managers_.selection != nullptr);
  const std::set<Pick>& live = managers_.selection->picks();
  const bool is_live =
      !live.empty() && managers_.selection->shot() == frame.shot;
  if (is_live) {
    return live;
  }
  const bool is_graced =
      shown_shot_ == frame.shot && grace_.IsGrabbable(false, NowMs());
  return is_graced ? shown_picks_ : std::set<Pick>();
}

std::vector<PoseTool::Handle> PoseTool::HandlesOf(
    const StageFrame& frame) const {
  assert(frame.scale > 0.0);
  assert(managers_.history != nullptr);
  std::vector<Handle> handles;
  const Project& project = managers_.history->current();
  const Shot* shot = FindShot(project, frame.shot);
  const std::set<Pick> picks = GrabbablePicks(frame);
  const bool has_handles = shot != nullptr && !picks.empty();
  if (!has_handles) {
    return handles;
  }
  for (const Pick& pick : picks) {
    const bool is_whole = pick.piece.isEmpty();
    const auto turn =
        is_whole ? TurnHandlesOf(project, *shot, pick.layer, frame.local,
                                 frame.scale, cache_)
                 : std::nullopt;
    if (turn) {
      handles.push_back({GrabKind::kLean, TurnKey(pick.layer),
                         turn->lean + frame.corner, {}, {}, {}, {}});
      handles.push_back({GrabKind::kSwivel, TurnKey(pick.layer),
                         turn->swivel + frame.corner, {}, {}, {}, {}});
    }
    const auto warp =
        is_whole ? std::nullopt
                 : PieceWarpOnScreen(project, *shot, pick.layer, pick.piece,
                                     frame.local, frame.scale,
                                     cache_->poses());
    for (size_t i = 0; warp && i < warp->points.size(); ++i) {
      const WarpDot dot{pick.layer, pick.piece, static_cast<int>(i)};
      handles.push_back({GrabKind::kWarpDot, WarpKey(dot),
                         warp->to_screen.map(warp->points[i]) + frame.corner,
                         dot, warp->to_screen, {}, {}});
    }
  }
  const bool is_live = !managers_.selection->picks().empty() &&
                       managers_.selection->shot() == frame.shot;
  const LayerId layer =
      is_live ? managers_.selection->layer() : shown_layer_;
  for (const IkHandle& tip :
       layer.IsValid() ? IkHandles(project, *shot, layer, frame.local,
                                   frame.scale, cache_->poses())
                       : std::vector<IkHandle>()) {
    handles.push_back({GrabKind::kIkTip, IkKey(tip.chain),
                       tip.point + frame.corner, {}, {}, layer, tip.chain});
  }
  return handles;
}

std::optional<PoseTool::Handle> PoseTool::HandleAt(
    QPointF point, const StageFrame& frame) const {
  assert(frame.scale > 0.0);
  assert(std::isfinite(point.x()) && std::isfinite(point.y()));
  const std::vector<Handle> handles = HandlesOf(frame);
  std::vector<QPointF> spots;
  for (const Handle& handle : handles) {
    spots.push_back(handle.spot);
  }
  const int nearest = NearestGrab(spots, point);
  const bool is_hit = nearest >= 0;
  return is_hit ? std::optional<Handle>(handles[static_cast<size_t>(nearest)])
                : std::nullopt;
}

PoseTool::Hover PoseTool::HoverAt(QPointF point,
                                  const StageFrame& frame) const {
  assert(cache_ != nullptr);
  assert(frame.scale > 0.0);
  Hover hover;
  const auto handle = HandleAt(point, frame);
  if (handle) {
    hover.kind = handle->kind;
    hover.key = handle->key;
    return hover;
  }
  const Project& project = managers_.history->current();
  const Shot* shot = FindShot(project, frame.shot);
  const auto hit =
      shot != nullptr ? HitTest(project, *shot, frame.local,
                                point - frame.corner, frame.scale, cache_)
                      : std::nullopt;
  if (hit) {
    // A doll picked whole is hovered whole, like a press would take it.
    const auto& picks = managers_.selection->picks();
    const Pick whole{hit->layer, QString()};
    const Pick pick = picks.contains(whole) ? whole
                                            : Pick{hit->layer, hit->piece};
    const bool is_unpicked = !picks.contains(pick);
    hover.pick = is_unpicked ? std::optional<Pick>(pick) : std::nullopt;
  }
  return hover;
}

bool PoseTool::PressHandle(QPointF point, const StageFrame& frame) {
  assert(frame.scale > 0.0);
  assert(managers_.history != nullptr);
  const auto handle = HandleAt(point, frame);
  if (!handle) {
    return false;
  }
  switch (handle->kind) {
    case GrabKind::kLean:
    case GrabKind::kSwivel:
      GrabTurn(*handle, point, frame);
      break;
    case GrabKind::kWarpDot:
      GrabWarp(*handle, point, frame);
      break;
    default:
      GrabIk(*handle, point, frame);
      break;
  }
  return drag_ != nullptr;
}

void PoseTool::GrabTurn(const Handle& handle, QPointF point,
                        const StageFrame& frame) {
  assert(handle.kind == GrabKind::kLean || handle.kind == GrabKind::kSwivel);
  assert(frame.scale > 0.0);
  const Project& project = managers_.history->current();
  const Shot* shot = FindShot(project, frame.shot);
  std::vector<LayerId> dolls;
  for (const Pick& pick : GrabbablePicks(frame)) {
    const bool is_doll = shot != nullptr && pick.piece.isEmpty() &&
                         TurnHandlesOf(project, *shot, pick.layer,
                                       frame.local, frame.scale, cache_)
                             .has_value();
    if (is_doll) {
      dolls.push_back(pick.layer);
    }
  }
  const bool has_dolls = !dolls.empty();
  if (!has_dolls) {
    return;
  }
  const bool is_lean = handle.kind == GrabKind::kLean;
  const Layer* first = FindLayer(*shot, dolls.front());
  const QString what =
      dolls.size() > 1
          ? QObject::tr("%1 dolls").arg(dolls.size())
          : std::get<DollLayer>(first->content).doll;
  auto drag = std::make_unique<Drag>();
  drag->kind = is_lean ? Kind::kLean : Kind::kSwivel;
  drag->frame = frame;
  drag->dolls = std::move(dolls);
  drag->press = point;
  drag->start = point;
  drag->scope = std::make_unique<EditScope>(
      managers_.history, is_lean ? QObject::tr("Lean %1").arg(what)
                                 : QObject::tr("Swivel %1").arg(what));
  drag_ = std::move(drag);
}

void PoseTool::GrabWarp(const Handle& handle, QPointF point,
                        const StageFrame& frame) {
  assert(handle.kind == GrabKind::kWarpDot);
  assert(frame.scale > 0.0);
  auto drag = std::make_unique<Drag>();
  drag->kind = Kind::kWarp;
  drag->frame = frame;
  drag->layer = handle.dot.layer;
  drag->piece = handle.dot.piece;
  drag->point = handle.dot.point;
  drag->drawing_to_screen = handle.to_screen;
  // The dot jumps under the cursor: the drag measures from its centre.
  drag->press = point;
  drag->start = handle.spot;
  drag->scope = std::make_unique<EditScope>(
      managers_.history, QObject::tr("Warp %1").arg(handle.dot.piece));
  drag_ = std::move(drag);
}

void PoseTool::GrabIk(const Handle& handle, QPointF point,
                      const StageFrame& frame) {
  assert(handle.kind == GrabKind::kIkTip);
  assert(frame.scale > 0.0);
  auto drag = std::make_unique<Drag>();
  drag->kind = Kind::kIk;
  drag->frame = frame;
  drag->layer = handle.layer;
  drag->chain = handle.chain;
  drag->press = point;
  drag->start = point;
  drag->scope = std::make_unique<EditScope>(
      managers_.history, QObject::tr("Bend %1").arg(handle.chain));
  drag_ = std::move(drag);
}

std::optional<WarpDot> PoseTool::PickedDot(const StageFrame& frame) const {
  assert(frame.scale > 0.0);
  assert(managers_.selection != nullptr);
  const bool is_here = managers_.selection->shot() == frame.shot;
  return is_here ? managers_.selection->dot() : std::nullopt;
}

std::vector<WarpDot> PoseTool::PickedDots(const StageFrame& frame) const {
  assert(frame.scale > 0.0);
  assert(managers_.selection != nullptr);
  const bool is_here = managers_.selection->shot() == frame.shot;
  return is_here ? managers_.selection->dots() : std::vector<WarpDot>();
}

std::set<WarpDot> PoseTool::DotsIn(const QRectF& box,
                                   const StageFrame& frame) const {
  assert(frame.scale > 0.0);
  assert(box.width() >= 0.0 && box.height() >= 0.0);
  std::set<WarpDot> caught;
  for (const Handle& handle : HandlesOf(frame)) {
    const bool is_in = handle.kind == GrabKind::kWarpDot &&
                       box.contains(handle.spot);
    if (is_in) {
      caught.insert(handle.dot);
    }
  }
  return caught;
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

std::vector<DotRef> PoseTool::PickedRefs(const StageFrame& frame) const {
  assert(frame.scale > 0.0);
  assert(managers_.history != nullptr);
  std::vector<DotRef> refs;
  for (const WarpDot& dot : PickedDots(frame)) {
    const DollLayer* posed =
        PosedLayerOf(managers_.history->current(), frame.shot, dot.layer);
    const bool has_doll = posed != nullptr;
    if (has_doll) {
      refs.push_back({posed->doll, dot.piece, dot.point});
    }
  }
  return refs;
}

void PoseTool::ToggleDrag(const StageFrame& frame) {
  assert(frame.scale > 0.0);
  assert(managers_.rig != nullptr);
  const std::vector<DotRef> refs = PickedRefs(frame);
  const bool has_dots = !refs.empty();
  if (!has_dots) {
    problem_ = QObject::tr("Click a warp dot of a picked piece first.");
    return;
  }
  const Project& project = managers_.history->current();
  const bool is_all_drag = std::all_of(
      refs.begin(), refs.end(), [&project](const DotRef& ref) {
        const Doll* doll = FindDoll(project, ref.doll);
        const RigPiece* rig =
            doll != nullptr ? FindRig(doll->rig, ref.piece) : nullptr;
        return rig != nullptr &&
               std::any_of(rig->drag_nodes.begin(), rig->drag_nodes.end(),
                           [&ref](const DragNode& n) {
                             return n.point == ref.point;
                           });
      });
  Note(managers_.rig->SetDragNodes(refs, !is_all_drag));
}

bool PoseTool::ClearDots(const StageFrame& frame) {
  assert(frame.scale > 0.0);
  assert(managers_.rig != nullptr);
  const std::vector<DotRef> refs = PickedRefs(frame);
  const bool has_dots = !refs.empty();
  if (!has_dots) {
    return false;
  }
  Note(managers_.rig->ClearDots(refs));
  return true;
}

}  // namespace snapper
