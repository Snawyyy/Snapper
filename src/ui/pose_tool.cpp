#include "ui/pose_tool.h"

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
#include "ui/drag_text.h"
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
  clock_.start();
  Observe();
}

void PoseTool::Press(QPointF point, bool is_shift, bool is_ctrl,
                     const StageFrame& frame) {
  assert(cache_ != nullptr);
  assert(frame.scale > 0.0);
  Cancel();
  problem_.clear();
  Observe();
  const bool is_handle = PressHandle(point, frame);
  const bool is_on_dot = drag_ != nullptr && drag_->kind == Kind::kWarp;
  // Shift and Ctrl add to, or take from, the picked dots; a plain press
  // anywhere else lets them go.
  const bool keeps_dots = is_shift || is_ctrl;
  const bool lets_go = !is_on_dot && !keeps_dots;
  if (lets_go) {
    managers_.selection->PickDot(std::nullopt);
  }
  if (is_on_dot) {
    drag_->box_mode = is_shift  ? PickMode::kAdd
                      : is_ctrl ? PickMode::kToggle
                                : PickMode::kReplace;
  }
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
  drag->press = point;
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
  drag->press = point;
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
  assert(!is_box || drag_->kind == Kind::kBox);
  return is_box ? QRectF(drag_->start, drag_->corner).normalized()
                : QRectF();
}

void PoseTool::MoveIk(QPointF point) {
  assert(drag_ != nullptr);
  assert(managers_.pose != nullptr);
  const Project& project = managers_.history->current();
  const Shot* shot = FindShot(project, drag_->frame.shot);
  const StageFrame& at = drag_->frame;
  const auto to_screen =
      shot != nullptr
          ? LayerToScreen(project, *shot, drag_->layer, at.local, at.scale,
                        cache_->poses())
          : std::nullopt;
  if (to_screen) {
    Note(managers_.pose->DragIk(at.shot, drag_->layer, drag_->chain,
                                at.local,
                                Back(*to_screen, point, at.corner)));
  }
  Say(OffsetText(point - drag_->start), false, point);
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
  const bool is_waiting = !LeftDeadZone(point);
  if (is_waiting) {
    return;
  }
  switch (drag_->kind) {
    case Kind::kBox:
      drag_->corner = point;
      break;
    case Kind::kIk:
      MoveIk(point);
      break;
    case Kind::kMove:
      MoveAll(total);
      Say(OffsetText(total), false, point);
      break;
    case Kind::kScale:
      // Right or up grows, left or down shrinks.
      ScaleAll(std::pow(2.0, (total.x() - total.y()) / kScalePixels));
      Say(ScaleText(drag_->scaled), false, point);
      break;
    case Kind::kLean:
      MoveTurn(total, point, false, is_shift);
      break;
    case Kind::kWarp: {
      const StageFrame& at = drag_->frame;
      const QPointF moved =
          Back(drag_->drawing_to_screen, point, at.corner) -
          Back(drag_->drawing_to_screen, drag_->start, at.corner);
      Note(managers_.pose->Pull(
          {at.shot, TrackKind::kPiece, drag_->layer, drag_->piece},
          at.local, drag_->point, moved));
      Say(OffsetText(point - drag_->start), false, point);
      break;
    }
    case Kind::kSwivel:
      MoveTurn(total, point, true, is_shift);
      break;
  }
}

bool PoseTool::LeftDeadZone(QPointF point) {
  assert(drag_ != nullptr);
  assert(std::isfinite(point.x()) && std::isfinite(point.y()));
  // The pick box follows the mouse from the first pixel.
  const bool is_box = drag_->kind == Kind::kBox;
  const bool is_new = !drag_->has_moved &&
                      IsPastDeadZone(drag_->press, point);
  if (is_new) {
    // A Ctrl-click that turned into a drag doesn't flip the pick, and a
    // click on a warp dot that turned into a drag doesn't pick the dot.
    drag_->toggle.reset();
    drag_->has_moved = true;
  }
  return is_box || drag_->has_moved;
}

void PoseTool::MoveAll(QPointF total) {
  assert(drag_ != nullptr);
  assert(std::isfinite(total.x()) && std::isfinite(total.y()));
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
                                       track.piece, at.local, at.scale,
                                       cache_->poses())
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
    // A box over warp dots of a picked piece picks those dots; over
    // none it picks pieces and layers as it always did.
    const std::set<WarpDot> dots =
        is_box_drawn ? DotsIn(box, at) : std::set<WarpDot>();
    const bool has_dots = !dots.empty();
    if (has_dots) {
      managers_.selection->PickDots(dots, drag_->box_mode);
    }
    const bool picks_pieces = is_box_drawn && !has_dots;
    if (picks_pieces) {
      for (const StageHit& hit :
           HitBox(project, *shot, at.local,
                  box.translated(-at.corner), at.scale, cache_)) {
        caught.insert({hit.layer, hit.piece});
      }
    }
    const LayerId focus = caught.empty() ? managers_.selection->layer()
                                         : caught.begin()->layer;
    if (!has_dots) {
      managers_.selection->PickThings(caught, drag_->box_mode, focus);
    }
  }
  const bool flips = drag_ != nullptr && drag_->toggle.has_value();
  if (flips) {
    const Pick pick = *drag_->toggle;
    drag_->scope->Cancel();
    managers_.selection->PickThings({pick}, PickMode::kToggle, pick.layer);
  }
  const bool is_dot_click = drag_ != nullptr &&
                            drag_->kind == Kind::kWarp && !drag_->has_moved;
  if (is_dot_click) {
    // Clicking a dot picks it for the wheel; clicking it again drops it.
    drag_->scope->Cancel();
    const WarpDot dot{drag_->layer, drag_->piece, drag_->point};
    // A plain click on the only picked dot drops it.
    const bool is_only = managers_.selection->dots() ==
                         std::vector<WarpDot>{dot};
    const bool is_again = is_only && drag_->box_mode == PickMode::kReplace;
    managers_.selection->PickDots(
        is_again ? std::set<WarpDot>() : std::set<WarpDot>{dot},
        is_again ? PickMode::kReplace : drag_->box_mode);
  }
  drag_.reset();
}

void PoseTool::Cancel() {
  assert(cache_ != nullptr);
  assert(managers_.history != nullptr);
  // A pick box has no edit to take back.
  const bool has_scope = drag_ != nullptr && drag_->scope != nullptr;
  if (has_scope) {
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
  const auto dot = PickedDot(frame);
  if (dot) {
    WheelReach(notches, is_fine, frame);
    return;
  }
  const auto tracks = managers_.selection->PickedTracks();
  const bool is_picked =
      !tracks.empty() && managers_.selection->shot() == frame.shot;
  if (!is_picked) {
    problem_ = QObject::tr("Click a piece first, then turn it.");
    return;
  }
  PoseDelta delta;
  // Wheel up turns counterclockwise, like a knob.
  delta.rotation = WheelDegrees(notches, is_fine);
  Note(managers_.pose->Shift(tracks, frame.local, delta));
}

double PoseTool::WheelDegrees(int notches, bool is_fine) {
  assert(notches > -1000000 && notches < 1000000);
  assert(kWheelStep > kFineWheelStep);
  return -notches * (is_fine ? kFineWheelStep : kWheelStep);
}

void PoseTool::Note(const Result<void>& result) {
  assert(managers_.history != nullptr);
  const bool is_failed = !result.has_value();
  problem_ = is_failed ? result.error().message : QString();
  assert(problem_.isEmpty() != is_failed);
}

}  // namespace snapper
