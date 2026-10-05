#include "ui/pose_tool.h"

#include <QLineF>

#include <cassert>
#include <cmath>

#include "anim/sampler.h"
#include "edit/history_manager.h"
#include "edit/pose_manager.h"
#include "edit/selection_manager.h"
#include "render/frame_renderer.h"
#include "render/stage_geometry.h"
#include "render/stage_hit.h"
#include "ui/playhead.h"

namespace snapper {
namespace {

// The point mapped back through to_screen, from widget space.
// A squashed-flat frame (scale 0) has no way back; the point then stays
// put rather than fly off.
QPointF Back(const QTransform& to_screen, QPointF point, QPointF corner) {
  assert(std::isfinite(point.x()) && std::isfinite(point.y()));
  assert(std::isfinite(corner.x()) && std::isfinite(corner.y()));
  bool is_invertible = false;
  const QTransform back = to_screen.inverted(&is_invertible);
  return is_invertible ? back.map(point - corner) : QPointF();
}

}  // namespace

PoseTool::PoseTool(const Managers& managers, ImageCache* cache)
    : managers_(managers), cache_(cache) {
  assert(managers_.IsComplete());
  assert(cache_ != nullptr);
}

void PoseTool::Press(QPointF point, bool is_ctrl, const StageFrame& frame) {
  assert(cache_ != nullptr);
  assert(frame.scale > 0.0);
  Cancel();
  problem_.clear();
  const bool is_ik = PressIk(point, frame);
  if (is_ik) {
    return;
  }
  const Project& project = managers_.history->current();
  const Shot* shot = FindShot(project, frame.shot);
  const auto hit =
      shot != nullptr ? HitTest(project, *shot, frame.local,
                                point - frame.corner, frame.scale, cache_)
                      : std::nullopt;
  SelectionManager* selection = managers_.selection;
  if (!hit) {
    selection->Clear();
    return;
  }
  selection->SelectShot(frame.shot);
  selection->SelectLayer(hit->layer);
  selection->SelectPieces(hit->piece.isEmpty() ? std::set<QString>()
                                               : std::set<QString>{hit->piece},
                          false);
  const TrackRef track =
      hit->piece.isEmpty()
          ? TrackRef{frame.shot, TrackKind::kLayer, hit->layer, {}}
          : TrackRef{frame.shot, TrackKind::kPiece, hit->layer, hit->piece};
  const QString what = hit->piece.isEmpty() ? QObject::tr("layer")
                                            : hit->piece;
  auto drag = std::make_unique<Drag>();
  drag->kind = is_ctrl ? Kind::kScale : Kind::kMove;
  drag->frame = frame;
  drag->track = track;
  drag->start = point;
  drag->start_pose = PoseOf(track, frame.local);
  drag->scope = std::make_unique<EditScope>(
      managers_.history, is_ctrl ? QObject::tr("Scale %1").arg(what)
                                 : QObject::tr("Move %1").arg(what));
  drag_ = std::move(drag);
  assert(IsDragging());
}

void PoseTool::Move(QPointF point, bool is_shift) {
  assert(std::isfinite(point.x()) && std::isfinite(point.y()));
  assert(managers_.pose != nullptr);
  if (!drag_) {
    return;
  }
  const Project& project = managers_.history->current();
  const Shot* shot = FindShot(project, drag_->frame.shot);
  const bool has_shot = shot != nullptr;
  if (!has_shot) {
    return;
  }
  const StageFrame& at = drag_->frame;
  QPointF delta = point - drag_->start;
  const bool is_locked = is_shift && drag_->kind == Kind::kMove;
  if (is_locked) {
    const bool is_sideways = std::abs(delta.x()) >= std::abs(delta.y());
    delta = is_sideways ? QPointF(delta.x(), 0) : QPointF(0, delta.y());
  }
  const TrackRef& track = drag_->track;
  switch (drag_->kind) {
    case Kind::kIk: {
      const auto to_screen =
          LayerToScreen(project, *shot, track.layer, at.local, at.scale);
      if (to_screen) {
        Note(managers_.pose->DragIk(at.shot, track.layer, drag_->chain,
                                    at.local,
                                    Back(*to_screen, point, at.corner)));
      }
      break;
    }
    case Kind::kMove: {
      const bool is_piece = track.kind == TrackKind::kPiece;
      const auto frame =
          is_piece ? PieceParentToScreen(project, *shot, track.layer,
                                         track.piece, at.local, at.scale)
                   : std::optional<QTransform>(FrameRenderer::ViewTransform(
                         project, *shot, at.local, at.scale));
      if (frame) {
        const QPointF moved = Back(*frame, drag_->start + delta, at.corner) -
                              Back(*frame, drag_->start, at.corner);
        Note(managers_.pose->Move(track, at.local,
                                  drag_->start_pose.offset + moved));
      }
      break;
    }
    case Kind::kScale: {
      // Right or up grows, left or down shrinks.
      const double factor =
          std::pow(2.0, (delta.x() - delta.y()) / kScalePixels);
      Note(managers_.pose->Scale(track, at.local,
                                 drag_->start_pose.scale_x * factor,
                                 drag_->start_pose.scale_y * factor));
      break;
    }
  }
}

void PoseTool::Release() {
  assert(cache_ != nullptr);
  assert(managers_.history != nullptr);
  drag_.reset();
}

void PoseTool::Cancel() {
  assert(cache_ != nullptr);
  assert(managers_.history != nullptr);
  if (drag_) {
    drag_->scope->Cancel();
  }
  drag_.reset();
}

void PoseTool::Wheel(int notches, bool is_fine, const StageFrame& frame) {
  assert(notches != 0);
  assert(frame.scale > 0.0);
  const auto track = PickedTrack(managers_, frame.shot);
  if (!track) {
    problem_ = QObject::tr("Click a piece first, then turn it.");
    return;
  }
  const double step = is_fine ? kFineWheelStep : kWheelStep;
  // Wheel up turns counterclockwise, like a knob.
  const double turned =
      PoseOf(*track, frame.local).rotation - notches * step;
  Note(managers_.pose->Rotate(*track, frame.local, turned));
}

PiecePose PoseTool::PoseOf(const TrackRef& track, Frame local) const {
  assert(local.index() >= 0);
  assert(managers_.history != nullptr);
  PiecePose pose;
  const Shot* shot = FindShot(managers_.history->current(), track.shot);
  const bool has_shot = shot != nullptr;
  if (has_shot) {
    ReadTrack(*shot, track, [&](const auto& channel) {
      using Held = typename std::remove_cvref_t<decltype(channel)>::value_type;
      constexpr bool is_pose = std::is_same_v<Held, PiecePose>;
      if constexpr (is_pose) {
        pose = Sample(channel, local, PiecePose());
      }
    });
  }
  return pose;
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
      drag->track = {frame.shot, TrackKind::kLayer, layer, {}};
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

void PoseTool::Note(const Result<void>& result) {
  assert(managers_.history != nullptr);
  const bool is_failed = !result.has_value();
  problem_ = is_failed ? result.error().message : QString();
  assert(problem_.isEmpty() != is_failed);
}

}  // namespace snapper
