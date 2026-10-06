#include "ui/pose_tool.h"

#include <QLineF>

#include <algorithm>
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

void PoseTool::Press(QPointF point, bool is_shift, bool is_ctrl,
                     const StageFrame& frame) {
  assert(cache_ != nullptr);
  assert(frame.scale > 0.0);
  Cancel();
  problem_.clear();
  const bool is_handle = PressLean(point, frame) || PressIk(point, frame);
  if (is_handle) {
    return;
  }
  const Project& project = managers_.history->current();
  const Shot* shot = FindShot(project, frame.shot);
  const auto hit =
      shot != nullptr ? HitTest(project, *shot, frame.local,
                                point - frame.corner, frame.scale, cache_)
                      : std::nullopt;
  managers_.selection->SelectShot(frame.shot);
  if (hit) {
    // A doll picked whole stays whole: grabbing any of its limbs moves
    // the group, not the limb.
    const Pick whole{hit->layer, QString()};
    const bool is_group = managers_.selection->picks().contains(whole);
    PressPick(is_group ? whole : Pick{hit->layer, hit->piece}, is_shift,
              is_ctrl, frame, point);
    return;
  }
  auto drag = std::make_unique<Drag>();
  drag->kind = Kind::kBox;
  drag->frame = frame;
  drag->start = point;
  drag->corner = point;
  drag->box_mode = is_shift  ? PickMode::kAdd
                   : is_ctrl ? PickMode::kRemove
                             : PickMode::kReplace;
  drag_ = std::move(drag);
  assert(IsDragging());
}

void PoseTool::PressPick(const Pick& pick, bool is_shift, bool is_ctrl,
                         const StageFrame& frame, QPointF point) {
  SelectionManager* selection = managers_.selection;
  assert(selection != nullptr);
  const bool is_picked = selection->picks().contains(pick);
  auto drag = std::make_unique<Drag>();
  const bool is_flip_later = is_ctrl && is_picked;
  if (is_flip_later) {
    drag->toggle = pick;
  } else {
    const bool is_adding = is_shift || is_ctrl;
    const bool is_kept = is_picked && !is_adding;
    if (!is_kept) {
      selection->PickThings({pick},
                            is_adding ? PickMode::kAdd : PickMode::kReplace,
                            pick.layer);
    }
  }
  const auto count = selection->picks().size();
  const QString what = count > 1 ? QObject::tr("%1 parts").arg(count)
                       : pick.piece.isEmpty() ? QObject::tr("layer")
                                              : pick.piece;
  drag->kind = is_ctrl ? Kind::kScale : Kind::kMove;
  drag->frame = frame;
  drag->start = point;
  drag->scope = std::make_unique<EditScope>(
      managers_.history, is_ctrl ? QObject::tr("Scale %1").arg(what)
                                 : QObject::tr("Move %1").arg(what));
  drag_ = std::move(drag);
  assert(IsDragging());
}

QRectF PoseTool::Box() const {
  const bool is_box = drag_ != nullptr && drag_->kind == Kind::kBox;
  assert(!is_box || drag_->frame.scale > 0.0);
  return is_box ? QRectF(drag_->start, drag_->corner).normalized()
                : QRectF();
}

void PoseTool::Move(QPointF point, bool is_shift) {
  assert(std::isfinite(point.x()) && std::isfinite(point.y()));
  assert(managers_.pose != nullptr);
  if (!drag_) {
    return;
  }
  QPointF total = point - drag_->start;
  const bool is_locked = is_shift && drag_->kind == Kind::kMove;
  if (is_locked) {
    const bool is_sideways = std::abs(total.x()) >= std::abs(total.y());
    total = is_sideways ? QPointF(total.x(), 0) : QPointF(0, total.y());
  }
  const bool is_moved = QLineF(QPointF(), total).length() > 2.0;
  if (is_moved) {
    // A Ctrl-click that turned into a drag doesn't flip the pick.
    drag_->toggle.reset();
  }
  switch (drag_->kind) {
    case Kind::kBox:
      drag_->corner = point;
      break;
    case Kind::kIk: {
      const Project& project = managers_.history->current();
      const Shot* shot = FindShot(project, drag_->frame.shot);
      const StageFrame& at = drag_->frame;
      const auto to_screen =
          shot != nullptr
              ? LayerToScreen(project, *shot, drag_->layer, at.local, at.scale)
              : std::nullopt;
      if (to_screen) {
        Note(managers_.pose->DragIk(at.shot, drag_->layer, drag_->chain,
                                    at.local,
                                    Back(*to_screen, point, at.corner)));
      }
      break;
    }
    case Kind::kMove:
      MoveAll(total);
      break;
    case Kind::kScale:
      // Right or up grows, left or down shrinks.
      ScaleAll(std::pow(2.0, (total.x() - total.y()) / kScalePixels));
      break;
    case Kind::kLean:
      Note(managers_.pose->Lean(drag_->frame.shot, drag_->dolls,
                                drag_->frame.local,
                                total.y() * kLeanPerPixel));
      break;
  }
}

void PoseTool::MoveAll(QPointF total) {
  assert(drag_ != nullptr);
  const Project& project = managers_.history->current();
  const StageFrame& at = drag_->frame;
  const Shot* shot = FindShot(project, at.shot);
  const bool has_shot = shot != nullptr;
  if (!has_shot) {
    return;
  }
  // Each pick moves in its own parent's frame, so all follow the cursor
  // on screen by the same amount.
  for (const TrackRef& track : managers_.selection->PickedTracks()) {
    const bool is_piece = track.kind == TrackKind::kPiece;
    const auto frame =
        is_piece ? PieceParentToScreen(project, *shot, track.layer,
                                       track.piece, at.local, at.scale)
                 : std::optional<QTransform>(FrameRenderer::ViewTransform(
                       project, *shot, at.local, at.scale));
    if (frame) {
      PoseDelta delta;
      delta.offset = Back(*frame, drag_->start + total, at.corner) -
                     Back(*frame, drag_->start + drag_->done, at.corner);
      Note(managers_.pose->Shift({track}, at.local, delta));
    }
  }
  drag_->done = total;
}

void PoseTool::ScaleAll(double factor) {
  assert(drag_ != nullptr);
  assert(std::isfinite(factor) && factor > 0.0);
  PoseDelta delta;
  delta.scale_x = factor - drag_->scaled;
  delta.scale_y = delta.scale_x;
  Note(managers_.pose->Shift(managers_.selection->PickedTracks(),
                             drag_->frame.local, delta));
  drag_->scaled = factor;
}

void PoseTool::Release() {
  assert(cache_ != nullptr);
  assert(managers_.history != nullptr);
  const bool is_box = drag_ != nullptr && drag_->kind == Kind::kBox;
  if (is_box) {
    const QRectF box = Box();
    const StageFrame& at = drag_->frame;
    const Project& project = managers_.history->current();
    const Shot* shot = FindShot(project, at.shot);
    std::set<Pick> caught;
    const bool is_box_drawn = shot != nullptr && box.width() > 2.0 &&
                              box.height() > 2.0;
    if (is_box_drawn) {
      for (const StageHit& hit :
           HitBox(project, *shot, at.local,
                  box.translated(-at.corner), at.scale, cache_)) {
        caught.insert({hit.layer, hit.piece});
      }
    }
    const LayerId focus = caught.empty() ? managers_.selection->layer()
                                         : caught.begin()->layer;
    managers_.selection->PickThings(caught, drag_->box_mode, focus);
  }
  const bool flips = drag_ != nullptr && drag_->toggle.has_value();
  if (flips) {
    const Pick pick = *drag_->toggle;
    drag_->scope->Cancel();
    managers_.selection->PickThings({pick}, PickMode::kToggle, pick.layer);
  }
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

void PoseTool::PickWhole(QPointF point, const StageFrame& frame) {
  assert(cache_ != nullptr);
  assert(frame.scale > 0.0);
  Cancel();
  const Project& project = managers_.history->current();
  const Shot* shot = FindShot(project, frame.shot);
  const auto hit =
      shot != nullptr ? HitTest(project, *shot, frame.local,
                                point - frame.corner, frame.scale, cache_)
                      : std::nullopt;
  if (hit) {
    managers_.selection->SelectShot(frame.shot);
    managers_.selection->PickThings({{hit->layer, QString()}},
                                    PickMode::kReplace, hit->layer);
  }
}

void PoseTool::Wheel(int notches, bool is_fine, const StageFrame& frame) {
  assert(notches != 0);
  assert(frame.scale > 0.0);
  const auto tracks = managers_.selection->PickedTracks();
  const bool is_picked =
      !tracks.empty() && managers_.selection->shot() == frame.shot;
  if (!is_picked) {
    problem_ = QObject::tr("Click a piece first, then turn it.");
    return;
  }
  PoseDelta delta;
  // Wheel up turns counterclockwise, like a knob.
  delta.rotation = -notches * (is_fine ? kFineWheelStep : kWheelStep);
  Note(managers_.pose->Shift(tracks, frame.local, delta));
}

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
  bool is_near = false;
  for (const Pick& pick : managers_.selection->picks()) {
    const auto handle =
        pick.piece.isEmpty()
            ? LeanHandle(project, *shot, pick.layer, frame.local,
                         frame.scale, cache_)
            : std::nullopt;
    const bool is_doll = handle.has_value();
    if (is_doll) {
      dolls.push_back(pick.layer);
      is_near = is_near || QLineF(*handle + frame.corner, point).length() <=
                               kHandleReach;
    }
  }
  if (!is_near) {
    return false;
  }
  const Layer* first = FindLayer(*shot, dolls.front());
  const QString what =
      dolls.size() > 1
          ? QObject::tr("%1 dolls").arg(dolls.size())
          : std::get<DollLayer>(first->content).doll;
  auto drag = std::make_unique<Drag>();
  drag->kind = Kind::kLean;
  drag->frame = frame;
  drag->dolls = std::move(dolls);
  drag->start = point;
  drag->scope = std::make_unique<EditScope>(
      managers_.history, QObject::tr("Lean %1").arg(what));
  drag_ = std::move(drag);
  return true;
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

void PoseTool::Note(const Result<void>& result) {
  assert(managers_.history != nullptr);
  const bool is_failed = !result.has_value();
  problem_ = is_failed ? result.error().message : QString();
  assert(problem_.isEmpty() != is_failed);
}

}  // namespace snapper
